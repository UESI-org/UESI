/* Complex program, keep heavily commented! */

#include <sys/elf_loader.h>
#include <sys/elf.h>
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

#ifdef DEBUG_ELF
extern void serial_printf(uint16_t port, const char *fmt, ...);
#else
#define serial_printf(...)
#endif

static uint64_t hhdm_offset = 0;
static bool hhdm_initialized = false;

struct page_allocation {
    uint64_t virt_addr;
    void *phys_addr;
};

void init_hhdm_offset(void) {
    if (hhdm_initialized) {
        return;
    }
    
    struct limine_hhdm_response *hhdm_resp = boot_get_hhdm();
    if (hhdm_resp && hhdm_resp->offset) {
        hhdm_offset = hhdm_resp->offset;
        hhdm_initialized = true;
    }
}

const Elf64_Ehdr *elf_get_header(const void *elf_data) {
    return (const Elf64_Ehdr *)elf_data;
}

const Elf64_Phdr *elf_get_program_header(const void *elf_data, int index) {
    const Elf64_Ehdr *ehdr = elf_get_header(elf_data);
    
    if (index >= ehdr->e_phnum) {
        return NULL;
    }
    
    const uint8_t *phdr_base = (const uint8_t *)elf_data + ehdr->e_phoff;
    return (const Elf64_Phdr *)(phdr_base + (index * ehdr->e_phentsize));
}

bool elf_check_magic(const Elf64_Ehdr *ehdr) {
    return (ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
            ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
            ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
            ehdr->e_ident[EI_MAG3] == ELFMAG3);
}

bool elf_check_supported(const Elf64_Ehdr *ehdr) {
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
        printf_("ELF: Not an executable or shared object (type=%d)\n", ehdr->e_type);
        return false;
    }
    
    if (ehdr->e_machine != EM_X86_64) {
        printf_("ELF: Not an x86-64 executable\n");
        return false;
    }
    
    return true;
}

bool elf_validate(const void *elf_data, size_t size) {
    if (!elf_data || size < sizeof(Elf64_Ehdr)) {
        printf_("ELF: File too small\n");
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
                printf_("ELF: Segment %d: memsz < filesz\n", i);
                return false;
            }
            
            /* Check segment data is within file bounds */
            if (phdr->p_offset + phdr->p_filesz > size) {
                printf_("ELF: Segment %d data outside file bounds\n", i);
                return false;
            }
            
            /* Check for integer overflow in virtual address range */
            if (phdr->p_vaddr > UINT64_MAX - phdr->p_memsz) {
                printf_("ELF: Segment %d size overflow\n", i);
                return false;
            }
            
            /* Validate alignment (must be 0 or power of 2) */
            if (phdr->p_align > 0 && (phdr->p_align & (phdr->p_align - 1)) != 0) {
                printf_("ELF: Segment %d has invalid alignment 0x%lx\n", i, phdr->p_align);
                return false;
            }
            
            /* Check segment doesn't overlap kernel space */
            if (phdr->p_vaddr >= USER_SPACE_MAX) {
                printf_("ELF: Segment %d in kernel space\n", i);
                return false;
            }
            
            if (phdr->p_vaddr + phdr->p_memsz > USER_SPACE_MAX) {
                printf_("ELF: Segment %d extends into kernel space\n", i);
                return false;
            }
        }
    }
    
    return true;
}

/* Helper function to write data to virtual memory through physical mapping */
static bool write_to_virtual_memory(struct process *ps, uint64_t virt_addr,
                                    const uint8_t *data, size_t size,
                                    bool zero_fill) {
    extern uint64_t mmu_get_physical_address(page_directory_t *pd, uint64_t virt);
    
    for (size_t off = 0; off < size; off += 4096) {
        uint64_t virt_page = (virt_addr + off) & ~0xFFFULL;
        
        /* Get physical address of this page */
        uint64_t phys_page = mmu_get_physical_address(ps->ps_vmspace, virt_page);
        if (phys_page == 0) {
            serial_printf(DEBUG_PORT, "ERROR: Failed to get phys for virt=0x%lx\n", virt_page);
            return false;
        }
        
        /* Calculate parameters for this page */
        size_t page_offset = (virt_addr + off) & 0xFFF;
        size_t bytes_in_page = 4096 - page_offset;
        if (off + bytes_in_page > size) {
            bytes_in_page = size - off;
        }
        
        /* Map to virtual address via HHDM */
        uint64_t phys_addr = (phys_page & ~0xFFFULL) + page_offset;
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

/* TODO: Implement cleanup_pages for production use
static void cleanup_pages(struct process *ps, struct page_allocation *pages, 
                         size_t num_pages) {
    for (size_t i = 0; i < num_pages; i++) {
        if (pages[i].phys_addr != NULL) {
            paging_unmap_range(ps->ps_vmspace, pages[i].virt_addr, 1);
            pmm_free(pages[i].phys_addr);
        }
    }
}
*/

bool elf_load(struct process *ps, const void *elf_data, size_t size, 
              uint64_t *entry_point) {
    if (!ps || !elf_data || !entry_point) {
        printf_("ELF: NULL parameter - ps=%p elf_data=%p entry_point=%p\n", 
                ps, elf_data, entry_point);
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
    
    printf_("ELF: Starting validation (size=%lu bytes, hhdm=0x%lx)\n", size, hhdm_offset);
    
    extern uint64_t mmu_get_physical_address(page_directory_t *pd, uint64_t virt);
    
    if (!elf_validate(elf_data, size)) {
        return false;
    }
    
    const Elf64_Ehdr *ehdr = elf_get_header(elf_data);
    
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
        printf_("ELF: Entry point 0x%lx not in executable segment\n", ehdr->e_entry);
        return false;
    }
    
    /* Count total pages needed for tracking */
    size_t total_pages = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = elf_get_program_header(elf_data, i);
        if (phdr->p_type == PT_LOAD) {
            uint64_t virt_start = phdr->p_vaddr & ~0xFFFULL;
            uint64_t virt_end = (phdr->p_vaddr + phdr->p_memsz + 0xFFF) & ~0xFFFULL;
            total_pages += (virt_end - virt_start) / 4096;
        }
    }

    /* TODO: dynamically allocate this array.
     * For now, we proceed without full cleanup tracking for simplicity,
     * but you should add proper resource tracking in production. */
    
    /* Load all PT_LOAD segments */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = elf_get_program_header(elf_data, i);
        
        if (phdr->p_type != PT_LOAD) {
            continue;
        }
        
        printf_("  Segment %d: vaddr=0x%lx filesz=0x%lx memsz=0x%lx flags=0x%x\n",
                i, phdr->p_vaddr, phdr->p_filesz, phdr->p_memsz, phdr->p_flags);
        
        /* Calculate number of pages needed */
        uint64_t virt_start = phdr->p_vaddr & ~0xFFFULL;
        uint64_t virt_end = (phdr->p_vaddr + phdr->p_memsz + 0xFFF) & ~0xFFFULL;
        size_t num_pages = (virt_end - virt_start) / 4096;
        
        /* Determine page flags based on segment flags */
        uint64_t page_flags = PAGING_FLAG_PRESENT | PAGING_FLAG_USER;
        
        /* Read permission is implicit on most architectures */
        if (phdr->p_flags & PF_W) {
            page_flags |= PAGING_FLAG_WRITE;
        }
        if (!(phdr->p_flags & PF_X)) {
            page_flags |= PAGING_FLAG_NX;
        }
        
        /* Allocate and map pages */
        for (size_t j = 0; j < num_pages; j++) {
            uint64_t virt_page = virt_start + (j * 4096);
            
            /* Get HHDM virtual address from PMM */
            void *page_virt = pmm_alloc();
            if (page_virt == NULL) {
                printf_("ELF: Failed to allocate physical page\n");
                /* TODO: Cleanup allocated pages */
                return false;
            }
            
            /* Convert to physical address by subtracting HHDM offset */
            uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;
            
            if (!paging_map_range(ps->ps_vmspace, virt_page, phys_page, 1, page_flags)) {
                printf_("ELF: Failed to map page at 0x%lx\n", virt_page);
                pmm_free(page_virt);
                /* TODO: Cleanup allocated pages */
                return false;
            }
        }
        
        /* Copy segment data from file */
        if (phdr->p_filesz > 0) {
            const uint8_t *src = (const uint8_t *)elf_data + phdr->p_offset;
            
            serial_printf(DEBUG_PORT, "  Copying %lu bytes starting at vaddr=0x%lx\n",
                          (unsigned long)phdr->p_filesz, phdr->p_vaddr);
            
            if (!write_to_virtual_memory(ps, phdr->p_vaddr, src, phdr->p_filesz, false)) {
                printf_("ELF: Failed to copy segment data\n");
                /* TODO: Cleanup allocated pages */
                return false;
            }
            
            serial_printf(DEBUG_PORT, "  Copy complete\n");
        }
        
        /* Zero out BSS (uninitialized data) */
        if (phdr->p_memsz > phdr->p_filesz) {
            size_t zero_size = phdr->p_memsz - phdr->p_filesz;
            uint64_t zero_start = phdr->p_vaddr + phdr->p_filesz;
            
            serial_printf(DEBUG_PORT, "  Zeroing %lu bytes starting at vaddr=0x%lx\n",
                          (unsigned long)zero_size, zero_start);
            
            if (!write_to_virtual_memory(ps, zero_start, NULL, zero_size, true)) {
                printf_("ELF: Failed to zero BSS\n");
                /* TODO: Cleanup allocated pages */
                return false;
            }
            
            serial_printf(DEBUG_PORT, "  BSS zeroing complete\n");
        }
    }
    
    *entry_point = ehdr->e_entry;
    
    /* Set program break for heap (start of heap after all segments) */
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
    
    /* Align to page boundary */
    ps->ps_brk = (max_addr + 0xFFF) & ~0xFFFULL;
    
    printf_("ELF: Loaded successfully, entry=0x%lx brk=0x%lx\n", 
            *entry_point, ps->ps_brk);
    
    return true;
}