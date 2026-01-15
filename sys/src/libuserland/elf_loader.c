/* Complex program, keep heavily commented! */

#include <sys/elf_loader.h>
#include <sys/elf.h>
#include <sys/param.h>
#include <proc.h>
#include <string.h>
#include <stdbool.h>
#include <pmm.h>
#include <paging.h>
#include <mmu.h>
#include <printf.h>
#include <boot.h>
#include <limine.h>

#define USER_SPACE_MAX 0x800000000000ULL
#define DEBUG_PORT 0x3F8
#define PAGE_MASK (~(NBPG - 1))
#define MAX_FILE_SIZE (256 * 1024 * 1024) /* 256MB max ELF file */
#define MIN_STACK_PAGES 512                /* 2MB minimum stack */

#ifdef DEBUG_ELF
extern void serial_printf(uint16_t port, const char *fmt, ...);
#else
#define serial_printf(...)
#endif

static uint64_t hhdm_offset = 0;
static bool hhdm_initialized = false;

struct segment_alloc {
	uint64_t virt_start;
	size_t num_pages;
	bool allocated;
};

#define MAX_SEGMENTS 16
struct elf_load_state {
	struct segment_alloc segments[MAX_SEGMENTS];
	size_t segment_count;
	bool stack_allocated;
	uint64_t stack_base;
	size_t stack_pages;
};

void
init_hhdm_offset(void)
{
	if (hhdm_initialized) {
		return;
	}

	struct limine_hhdm_response *hhdm_resp = boot_get_hhdm();
	if (hhdm_resp && hhdm_resp->offset) {
		hhdm_offset = hhdm_resp->offset;
		hhdm_initialized = true;
	}
}

const Elf64_Ehdr *
elf_get_header(const void *elf_data)
{
	return (const Elf64_Ehdr *)elf_data;
}

const Elf64_Phdr *
elf_get_program_header(const void *elf_data, int index)
{
	const Elf64_Ehdr *ehdr = elf_get_header(elf_data);

	if (index >= ehdr->e_phnum) {
		return NULL;
	}

	const uint8_t *phdr_base = (const uint8_t *)elf_data + ehdr->e_phoff;
	return (const Elf64_Phdr *)(phdr_base + (index * ehdr->e_phentsize));
}

bool
elf_check_magic(const Elf64_Ehdr *ehdr)
{
	return (ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
	        ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
	        ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
	        ehdr->e_ident[EI_MAG3] == ELFMAG3);
}

bool
elf_check_supported(const Elf64_Ehdr *ehdr)
{
	if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
		printf_("ELF: Not a 64-bit executable\n");
		return false;
	}

	if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
		printf_("ELF: Not little-endian\n");
		return false;
	}

	if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
		printf_("ELF: Invalid ELF version\n");
		return false;
	}

	if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
		printf_("ELF: Not an executable or shared object (type=%d)\n",
		        ehdr->e_type);
		return false;
	}

	if (ehdr->e_machine != EM_X86_64) {
		printf_("ELF: Not an x86-64 executable\n");
		return false;
	}

	return true;
}

/* Check for overlapping segments */
static bool
elf_check_segment_overlap(const void *elf_data)
{
	const Elf64_Ehdr *ehdr = elf_get_header(elf_data);

	for (int i = 0; i < ehdr->e_phnum; i++) {
		const Elf64_Phdr *phdr1 = elf_get_program_header(elf_data, i);
		if (phdr1->p_type != PT_LOAD || phdr1->p_memsz == 0) {
			continue;
		}

		uint64_t start1 = phdr1->p_vaddr;
		uint64_t end1 = phdr1->p_vaddr + phdr1->p_memsz;

		for (int j = i + 1; j < ehdr->e_phnum; j++) {
			const Elf64_Phdr *phdr2 =
			    elf_get_program_header(elf_data, j);
			if (phdr2->p_type != PT_LOAD || phdr2->p_memsz == 0) {
				continue;
			}

			uint64_t start2 = phdr2->p_vaddr;
			uint64_t end2 = phdr2->p_vaddr + phdr2->p_memsz;

			/* Check for overlap */
			if (start1 < end2 && start2 < end1) {
				printf_(
				    "ELF: Segments %d and %d overlap (0x%lx-0x%lx vs 0x%lx-0x%lx)\n",
				    i,
				    j,
				    start1,
				    end1,
				    start2,
				    end2);
				return false;
			}
		}
	}

	return true;
}

/* Validate ELF file */
bool
elf_validate(const void *elf_data, size_t size)
{
	if (!elf_data || size < sizeof(Elf64_Ehdr)) {
		printf_("ELF: File too small (%lu bytes)\n", size);
		return false;
	}

	if (size > MAX_FILE_SIZE) {
		printf_("ELF: File too large (%lu bytes, max %lu)\n",
		        size,
		        MAX_FILE_SIZE);
		return false;
	}

	const Elf64_Ehdr *ehdr = elf_get_header(elf_data);

	if (!elf_check_magic(ehdr)) {
		printf_("ELF: Invalid magic number\n");
		return false;
	}

	if (!elf_check_supported(ehdr)) {
		return false;
	}

	if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0) {
		printf_("ELF: No program headers\n");
		return false;
	}

	if (ehdr->e_phoff + (ehdr->e_phnum * ehdr->e_phentsize) > size) {
		printf_("ELF: Program headers outside file bounds\n");
		return false;
	}

	/* Validate each PT_LOAD segment */
	for (int i = 0; i < ehdr->e_phnum; i++) {
		const Elf64_Phdr *phdr = elf_get_program_header(elf_data, i);

		if (phdr->p_type == PT_LOAD) {
			/* Check p_memsz >= p_filesz */
			if (phdr->p_memsz < phdr->p_filesz) {
				printf_("ELF: Segment %d: memsz (0x%lx) < filesz (0x%lx)\n",
				        i,
				        phdr->p_memsz,
				        phdr->p_filesz);
				return false;
			}

			/* Check segment data is within file bounds */
			if (phdr->p_offset + phdr->p_filesz > size) {
				printf_("ELF: Segment %d data outside file bounds\n",
				        i);
				return false;
			}

			/* Check for integer overflow */
			if (phdr->p_vaddr > UINT64_MAX - phdr->p_memsz) {
				printf_("ELF: Segment %d virtual address overflow\n",
				        i);
				return false;
			}

			/* Validate alignment */
			if (phdr->p_align > 0 &&
			    (phdr->p_align & (phdr->p_align - 1)) != 0) {
				printf_("ELF: Segment %d has invalid alignment 0x%lx\n",
				        i,
				        phdr->p_align);
				return false;
			}

			/* Check segment doesn't overlap kernel space */
			if (phdr->p_vaddr >= USER_SPACE_MAX) {
				printf_("ELF: Segment %d in kernel space (vaddr=0x%lx)\n",
				        i,
				        phdr->p_vaddr);
				return false;
			}

			if (phdr->p_vaddr + phdr->p_memsz > USER_SPACE_MAX) {
				printf_("ELF: Segment %d extends into kernel space\n",
				        i);
				return false;
			}

			/* Prevent mapping at NULL */
			if (phdr->p_vaddr < NBPG) {
				printf_("ELF: Segment %d maps NULL page (vaddr=0x%lx)\n",
				        i,
				        phdr->p_vaddr);
				return false;
			}
		}
	}

	/* Check for overlapping segments */
	if (!elf_check_segment_overlap(elf_data)) {
		return false;
	}

	return true;
}

/* Write data to virtual memory through physical mapping */
static bool
write_to_virtual_memory(struct process *ps,
                        uint64_t virt_addr,
                        const uint8_t *data,
                        size_t size,
                        bool zero_fill)
{
	for (size_t off = 0; off < size; off += NBPG) {
		uint64_t virt_page = (virt_addr + off) & PAGE_MASK;

		/* Get physical address of this page */
		uint64_t phys_page =
		    mmu_get_physical_address(ps->ps_vmspace, virt_page);
		if (phys_page == 0) {
			serial_printf(DEBUG_PORT,
			              "ERROR: Failed to get phys for virt=0x%lx\n",
			              virt_page);
			return false;
		}

		/* Calculate parameters for this page */
		size_t page_offset = (virt_addr + off) & PGOFSET;
		size_t bytes_in_page = NBPG - page_offset;
		if (off + bytes_in_page > size) {
			bytes_in_page = size - off;
		}

		/* Map to virtual address via HHDM */
		uint64_t phys_addr = (phys_page & PAGE_MASK) + page_offset;
		uint8_t *dest = (uint8_t *)(phys_addr + hhdm_offset);

		/* Copy or zero the data */
		if (zero_fill) {
			memset(dest, 0, bytes_in_page);
		} else {
			memcpy(dest, data + off, bytes_in_page);
		}
	}

	return true;
}

/* Clean up allocated resources on error */
static void
cleanup_elf_load(struct process *ps, struct elf_load_state *state)
{
	/* Clean up user stack */
	if (state->stack_allocated) {
		serial_printf(DEBUG_PORT,
		              "Cleanup: Unmapping stack at 0x%lx (%lu pages)\n",
		              state->stack_base,
		              state->stack_pages);

		paging_unmap_range(ps->ps_vmspace,
		                   state->stack_base,
		                   state->stack_pages);

		/* Free physical pages */
		for (size_t i = 0; i < state->stack_pages; i++) {
			uint64_t virt_page = state->stack_base + (i * NBPG);
			uint64_t phys =
			    mmu_get_physical_address(ps->ps_vmspace, virt_page);
			if (phys != 0) {
				void *phys_virt = (void *)(phys + hhdm_offset);
				pmm_free(phys_virt);
			}
		}
	}

	/* Clean up segment allocations */
	for (size_t i = 0; i < state->segment_count; i++) {
		if (!state->segments[i].allocated) {
			continue;
		}

		serial_printf(DEBUG_PORT,
		              "Cleanup: Unmapping segment at 0x%lx (%lu pages)\n",
		              state->segments[i].virt_start,
		              state->segments[i].num_pages);

		paging_unmap_range(ps->ps_vmspace,
		                   state->segments[i].virt_start,
		                   state->segments[i].num_pages);

		/* Free physical pages */
		for (size_t j = 0; j < state->segments[i].num_pages; j++) {
			uint64_t virt_page =
			    state->segments[i].virt_start + (j * NBPG);
			uint64_t phys =
			    mmu_get_physical_address(ps->ps_vmspace, virt_page);
			if (phys != 0) {
				void *phys_virt = (void *)(phys + hhdm_offset);
				pmm_free(phys_virt);
			}
		}
	}
}

/* Allocate user stack */
static bool
allocate_user_stack(struct process *ps,
                    struct elf_load_state *state,
                    size_t stack_pages)
{
	if (stack_pages < MIN_STACK_PAGES) {
		stack_pages = MIN_STACK_PAGES;
	}

	uint64_t stack_base = USER_STACK_TOP - (stack_pages * NBPG);

	printf_("ELF: Allocating user stack: 0x%lx - 0x%lx (%lu pages, %lu KB)\n",
	        stack_base,
	        USER_STACK_TOP,
	        stack_pages,
	        (stack_pages * NBPG) / 1024);

	/* Allocate and map stack pages */
	uint64_t page_flags =
	    PAGING_FLAG_PRESENT | PAGING_FLAG_WRITE | PAGING_FLAG_USER;

	for (size_t i = 0; i < stack_pages; i++) {
		uint64_t virt_page = stack_base + (i * NBPG);

		/* Allocate physical page */
		void *page_virt = pmm_alloc();
		if (page_virt == NULL) {
			printf_("ELF: Failed to allocate stack page %lu\n", i);
			return false;
		}

		/* Convert to physical address */
		uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;

		/* Map the page */
		if (!paging_map_range(ps->ps_vmspace,
		                      virt_page,
		                      phys_page,
		                      1,
		                      page_flags)) {
			printf_("ELF: Failed to map stack page at 0x%lx\n",
			        virt_page);
			pmm_free(page_virt);
			return false;
		}

		/* Zero the stack page */
		uint8_t *page_data = (uint8_t *)page_virt;
		memset(page_data, 0, NBPG);
	}

	state->stack_allocated = true;
	state->stack_base = stack_base;
	state->stack_pages = stack_pages;

	return true;
}

/* Load ELF executable into process address space */
bool
elf_load(struct process *ps,
         const void *elf_data,
         size_t size,
         uint64_t *entry_point)
{
	if (!ps || !elf_data || !entry_point) {
		printf_("ELF: NULL parameter - ps=%p elf_data=%p entry_point=%p\n",
		        ps,
		        elf_data,
		        entry_point);
		return false;
	}

	if (!hhdm_initialized) {
		printf_("ELF: HHDM offset not initialized, initializing now\n");
		init_hhdm_offset();
		if (!hhdm_initialized) {
			printf_("ELF: Failed to initialize HHDM offset\n");
			return false;
		}
	}

	printf_("ELF: Starting load (size=%lu bytes, hhdm=0x%lx)\n",
	        size,
	        hhdm_offset);

	/* Validate ELF file */
	if (!elf_validate(elf_data, size)) {
		return false;
	}

	const Elf64_Ehdr *ehdr = elf_get_header(elf_data);

	/* Initialize load state for cleanup tracking */
	struct elf_load_state state = { 0 };

	printf_("ELF: Loading executable...\n");
	printf_("  Entry point: 0x%lx\n", ehdr->e_entry);
	printf_("  Program headers: %d\n", ehdr->e_phnum);

	/* Validate entry point is in an executable segment */
	bool entry_valid = false;
	for (int i = 0; i < ehdr->e_phnum; i++) {
		const Elf64_Phdr *phdr = elf_get_program_header(elf_data, i);
		if (phdr->p_type == PT_LOAD && (phdr->p_flags & PF_X)) {
			if (ehdr->e_entry >= phdr->p_vaddr &&
			    ehdr->e_entry < phdr->p_vaddr + phdr->p_memsz) {
				entry_valid = true;
				break;
			}
		}
	}

	if (!entry_valid) {
		printf_("ELF: Entry point 0x%lx not in executable segment\n",
		        ehdr->e_entry);
		return false;
	}

	/* Load all PT_LOAD segments */
	for (int i = 0; i < ehdr->e_phnum; i++) {
		const Elf64_Phdr *phdr = elf_get_program_header(elf_data, i);

		if (phdr->p_type != PT_LOAD) {
			continue;
		}

		printf_("  Segment %d: vaddr=0x%lx filesz=0x%lx memsz=0x%lx flags=0x%x\n",
		        i,
		        phdr->p_vaddr,
		        phdr->p_filesz,
		        phdr->p_memsz,
		        phdr->p_flags);

		/* Calculate page-aligned range */
		uint64_t virt_start = phdr->p_vaddr & PAGE_MASK;
		uint64_t virt_end =
		    (phdr->p_vaddr + phdr->p_memsz + PGOFSET) & PAGE_MASK;
		size_t num_pages = (virt_end - virt_start) / NBPG;

		/* Track this segment for cleanup */
		if (state.segment_count >= MAX_SEGMENTS) {
			printf_("ELF: Too many segments (max %d)\n", MAX_SEGMENTS);
			cleanup_elf_load(ps, &state);
			return false;
		}

		state.segments[state.segment_count].virt_start = virt_start;
		state.segments[state.segment_count].num_pages = num_pages;
		state.segments[state.segment_count].allocated = false;
		size_t seg_idx = state.segment_count++;

		/* Determine page flags */
		uint64_t page_flags = PAGING_FLAG_PRESENT | PAGING_FLAG_USER;

		if (phdr->p_flags & PF_W) {
			page_flags |= PAGING_FLAG_WRITE;
		}
		if (!(phdr->p_flags & PF_X)) {
			page_flags |= PAGING_FLAG_NX;
		}

		/* Allocate and map pages */
		for (size_t j = 0; j < num_pages; j++) {
			uint64_t virt_page = virt_start + (j * NBPG);

			/* Allocate physical page */
			void *page_virt = pmm_alloc();
			if (page_virt == NULL) {
				printf_("ELF: Failed to allocate page %lu of segment %d\n",
				        j,
				        i);
				cleanup_elf_load(ps, &state);
				return false;
			}

			/* Convert to physical address */
			uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;

			/* Map the page */
			if (!paging_map_range(ps->ps_vmspace,
			                      virt_page,
			                      phys_page,
			                      1,
			                      page_flags)) {
				printf_("ELF: Failed to map page at 0x%lx\n",
				        virt_page);
				pmm_free(page_virt);
				cleanup_elf_load(ps, &state);
				return false;
			}
		}

		/* Mark segment as allocated */
		state.segments[seg_idx].allocated = true;

		/* Copy segment data from file */
		if (phdr->p_filesz > 0) {
			const uint8_t *src =
			    (const uint8_t *)elf_data + phdr->p_offset;

			serial_printf(DEBUG_PORT,
			              "  Copying %lu bytes to vaddr=0x%lx\n",
			              (unsigned long)phdr->p_filesz,
			              phdr->p_vaddr);

			if (!write_to_virtual_memory(ps,
			                             phdr->p_vaddr,
			                             src,
			                             phdr->p_filesz,
			                             false)) {
				printf_("ELF: Failed to copy segment %d data\n", i);
				cleanup_elf_load(ps, &state);
				return false;
			}
		}

		/* Zero out BSS (uninitialized data) */
		if (phdr->p_memsz > phdr->p_filesz) {
			size_t zero_size = phdr->p_memsz - phdr->p_filesz;
			uint64_t zero_start = phdr->p_vaddr + phdr->p_filesz;

			serial_printf(DEBUG_PORT,
			              "  Zeroing %lu bytes at vaddr=0x%lx (BSS)\n",
			              (unsigned long)zero_size,
			              zero_start);

			if (!write_to_virtual_memory(ps,
			                             zero_start,
			                             NULL,
			                             zero_size,
			                             true)) {
				printf_("ELF: Failed to zero BSS in segment %d\n", i);
				cleanup_elf_load(ps, &state);
				return false;
			}
		}
	}

	/* Set program break for heap */
	uint64_t max_addr = 0;
	for (int i = 0; i < ehdr->e_phnum; i++) {
		const Elf64_Phdr *phdr = elf_get_program_header(elf_data, i);
		if (phdr->p_type == PT_LOAD) {
			uint64_t seg_end = phdr->p_vaddr + phdr->p_memsz;
			if (seg_end > max_addr) {
				max_addr = seg_end;
			}
		}
	}
	ps->ps_brk = (max_addr + PGOFSET) & PAGE_MASK;

	/* Allocate user stack */
	if (!allocate_user_stack(ps, &state, PROCESS_USER_STACK_SIZE / NBPG)) {
		printf_("ELF: Failed to allocate user stack\n");
		cleanup_elf_load(ps, &state);
		return false;
	}

	*entry_point = ehdr->e_entry;

	printf_("ELF: Loaded successfully\n");
	printf_("  Entry: 0x%lx\n", *entry_point);
	printf_("  Heap start (brk): 0x%lx\n", ps->ps_brk);
	printf_("  Stack: 0x%lx - 0x%lx\n", state.stack_base, USER_STACK_TOP);

	return true;
}