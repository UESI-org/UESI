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

static uint64_t hhdm_offset = 0;

void init_hhdm_offset(void) {
    struct limine_hhdm_response *hhdm_resp = boot_get_hhdm();
    if (hhdm_resp && hhdm_resp->offset) {
        hhdm_offset = hhdm_resp->offset;
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
    
    return true;
}

bool elf_load(struct process *ps, const void *elf_data, size_t size, 
              uint64_t *entry_point) {
    if (!ps || !elf_data || !entry_point) {
        return false;
    }

    init_hhdm_offset();
    
    extern uint64_t mmu_get_physical_address(page_directory_t *pd, uint64_t virt);
    extern void serial_printf(uint16_t port, const char *fmt, ...);
    #define DEBUG_PORT 0x3F8
    
    if (!elf_validate(elf_data, size)) {
        return false;
    }
    
    const Elf64_Ehdr *ehdr = elf_get_header(elf_data);
    
    printf_("ELF: Loading executable...\n");
    printf_("  Entry point: 0x%lx\n", ehdr->e_entry);
    printf_("  Program headers: %d\n", ehdr->e_phnum);
    
    /* Load all PT_LOAD segments */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = elf_get_program_header(elf_data, i);
        
        if (phdr->p_type != PT_LOAD) {
            continue;
        }
        
        printf_("  Segment %d: vaddr=0x%lx filesz=0x%lx memsz=0x%lx flags=0x%x\n",
                i, phdr->p_vaddr, phdr->p_filesz, phdr->p_memsz, phdr->p_flags);
        
        if (phdr->p_vaddr >= 0x800000000000ULL) {
            printf_("ELF: Segment virtual address in kernel space\n");
            return false;
        }
        
        /* Calculate number of pages needed */
        uint64_t virt_start = phdr->p_vaddr & ~0xFFFULL;
        uint64_t virt_end = (phdr->p_vaddr + phdr->p_memsz + 0xFFF) & ~0xFFFULL;
        size_t num_pages = (virt_end - virt_start) / 4096;
        
        /* Determine page flags based on segment flags */
        uint64_t page_flags = PAGING_FLAG_PRESENT | PAGING_FLAG_USER;
        if (phdr->p_flags & PF_W) {
            page_flags |= PAGING_FLAG_WRITE;
        }
        if (!(phdr->p_flags & PF_X)) {
            page_flags |= PAGING_FLAG_NX;
        }
        
        /* CRITICAL FIX: pmm_alloc returns HHDM virtual address, not physical! */
        for (size_t j = 0; j < num_pages; j++) {
            uint64_t virt_page = virt_start + (j * 4096);
            
            /* Get HHDM virtual address from PMM */
            void *page_virt = pmm_alloc();
            if (page_virt == NULL) {
                printf_("ELF: Failed to allocate physical page\n");
                return false;
            }
            
            /* Convert to physical address by subtracting HHDM offset */
            uint64_t phys_page = (uint64_t)page_virt - hhdm_offset;
            
            if (!paging_map_range(ps->ps_vmspace, virt_page, phys_page, 1, page_flags)) {
                printf_("ELF: Failed to map page at 0x%lx\n", virt_page);
                pmm_free(page_virt);  /* Free using virtual address */
                return false;
            }
        }
        
        /* Copy segment data from file */
        if (phdr->p_filesz > 0) {
            const uint8_t *src = (const uint8_t *)elf_data + phdr->p_offset;
            
            serial_printf(DEBUG_PORT, "  Copying %lu bytes starting at vaddr=0x%lx\n",
                          (unsigned long)phdr->p_filesz, phdr->p_vaddr);
            
            /* Copy data page-by-page */
            for (size_t off = 0; off < phdr->p_filesz; off += 4096) {
                uint64_t virt_page = (phdr->p_vaddr + off) & ~0xFFFULL;
                
                /* Get physical address of this page */
                uint64_t phys_page = mmu_get_physical_address(ps->ps_vmspace, virt_page);
                if (phys_page == 0) {
                    serial_printf(DEBUG_PORT, "ERROR: Failed to get phys for virt=0x%lx\n", virt_page);
                    return false;
                }
                
                /* Calculate copy parameters for this page */
                size_t page_offset = (phdr->p_vaddr + off) & 0xFFF;
                size_t bytes_in_page = 4096 - page_offset;
                if (off + bytes_in_page > phdr->p_filesz) {
                    bytes_in_page = phdr->p_filesz - off;
                }
                
                /* Map to virtual address via HHDM and copy */
                uint64_t phys_addr = (phys_page & ~0xFFFULL) + page_offset;
                uint8_t *dest = (uint8_t *)(phys_addr + hhdm_offset);
                
                /* Copy the data */
                for (size_t i = 0; i < bytes_in_page; i++) {
                    dest[i] = src[off + i];
                }
            }
            
            serial_printf(DEBUG_PORT, "  Copy complete\n");
        }
        
        /* Zero out BSS */
        if (phdr->p_memsz > phdr->p_filesz) {
            size_t zero_size = phdr->p_memsz - phdr->p_filesz;
            uint64_t zero_start = phdr->p_vaddr + phdr->p_filesz;
            
            serial_printf(DEBUG_PORT, "  Zeroing %lu bytes starting at vaddr=0x%lx\n",
                          (unsigned long)zero_size, zero_start);
            
            for (size_t off = 0; off < zero_size; off += 4096) {
                uint64_t virt_page = (zero_start + off) & ~0xFFFULL;
                
                uint64_t phys_page = mmu_get_physical_address(ps->ps_vmspace, virt_page);
                if (phys_page == 0) {
                    serial_printf(DEBUG_PORT, "ERROR: Failed to get phys for virt=0x%lx\n", virt_page);
                    return false;
                }
                
                size_t page_offset = (zero_start + off) & 0xFFF;
                size_t bytes_in_page = 4096 - page_offset;
                if (off + bytes_in_page > zero_size) {
                    bytes_in_page = zero_size - off;
                }
                
                uint64_t phys_addr = (phys_page & ~0xFFFULL) + page_offset;
                uint8_t *dest = (uint8_t *)(phys_addr + hhdm_offset);
                
                for (size_t i = 0; i < bytes_in_page; i++) {
                    dest[i] = 0;
                }
            }
            
            serial_printf(DEBUG_PORT, "  BSS zeroing complete\n");
        }
    }
    
    *entry_point = ehdr->e_entry;
    
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
    
    ps->ps_brk = (max_addr + 0xFFF) & ~0xFFFULL;
    
    printf_("ELF: Loaded successfully, entry=0x%lx brk=0x%lx\n", 
            *entry_point, ps->ps_brk);
    
    return true;
}