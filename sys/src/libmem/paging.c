#include "paging.h"
#include "mmu.h"
#include "pmm.h"
#include <string.h>

extern void tty_printf(const char *fmt, ...);
extern void serial_printf(uint16_t port, const char *fmt, ...);

#define DEBUG_PORT 0x3F8
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000ULL
#define USER_VIRTUAL_BASE   0x0000000000400000ULL

static page_directory_t *kernel_directory = NULL;

void paging_init(void) {
    kernel_directory = mmu_get_current_address_space();
}

bool paging_map_range(page_directory_t *pd, uint64_t virt_base, uint64_t phys_base,
                      size_t num_pages, uint64_t flags) {
    if (pd == NULL) {
        return false;
    }
    
    virt_base &= ~0xFFFULL;
    phys_base &= ~0xFFFULL;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint64_t virt = virt_base + (i * PAGE_SIZE);
        uint64_t phys = phys_base + (i * PAGE_SIZE);
        
        if (!mmu_map_page(pd, virt, phys, flags)) {
            for (size_t j = 0; j < i; j++) {
                mmu_unmap_page(pd, virt_base + (j * PAGE_SIZE));
            }
            return false;
        }
    }
    
    return true;
}

bool paging_unmap_range(page_directory_t *pd, uint64_t virt_base, size_t num_pages) {
    if (pd == NULL) {
        return false;
    }
    
    virt_base &= ~0xFFFULL;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint64_t virt = virt_base + (i * PAGE_SIZE);
        mmu_unmap_page(pd, virt);
    }
    
    return true;
}

bool paging_map_region(page_directory_t *pd, memory_region_t *region) {
    if (pd == NULL || region == NULL) {
        return false;
    }
    
    size_t num_pages = (region->length + PAGE_SIZE - 1) / PAGE_SIZE;
    void *phys_base = pmm_alloc_contiguous(num_pages);
    
    if (phys_base == NULL) {
        return false;
    }
    
    uint64_t phys_addr = (uint64_t)phys_base - mmu_get_current_address_space()->phys_addr;
    
    if (!paging_map_range(pd, region->base, phys_addr, num_pages, region->flags)) {
        pmm_free_contiguous(phys_base, num_pages);
        return false;
    }
    
    return true;
}

bool paging_identity_map(page_directory_t *pd, uint64_t phys_base, size_t num_pages, uint64_t flags) {
    return paging_map_range(pd, phys_base, phys_base, num_pages, flags);
}

bool paging_is_range_mapped(page_directory_t *pd, uint64_t virt_base, size_t num_pages) {
    if (pd == NULL) {
        return false;
    }
    
    virt_base &= ~0xFFFULL;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint64_t virt = virt_base + (i * PAGE_SIZE);
        if (!mmu_is_mapped(pd, virt)) {
            return false;
        }
    }
    
    return true;
}

page_directory_t *paging_create_kernel_address_space(void) {
    return mmu_create_address_space();
}

page_directory_t *paging_create_user_address_space(void) {
    page_directory_t *pd = mmu_create_address_space();
    if (pd == NULL) {
        return NULL;
    }
    
    return pd;
}

page_directory_t *paging_clone_address_space(page_directory_t *src) {
    if (src == NULL) {
        return NULL;
    }
    
    page_directory_t *new_pd = mmu_create_address_space();
    if (new_pd == NULL) {
        return NULL;
    }
    
    for (uint64_t pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        uint64_t virt_base = pml4_idx << PML4_SHIFT;
        
        for (uint64_t offset = 0; offset < (1ULL << PML4_SHIFT); offset += PAGE_SIZE) {
            uint64_t virt = virt_base + offset;
            
            if (mmu_is_mapped(src, virt)) {
                uint64_t phys = mmu_get_physical_address(src, virt);
                
                void *new_page = pmm_alloc();
                if (new_page == NULL) {
                    mmu_destroy_address_space(new_pd);
                    return NULL;
                }
                
                uint64_t src_virt = phys + kernel_directory->phys_addr;
                memcpy(new_page, (void *)src_virt, PAGE_SIZE);
                
                uint64_t new_phys = (uint64_t)new_page - kernel_directory->phys_addr;
                
                if (!mmu_map_page(new_pd, virt, new_phys, PAGE_PRESENT | PAGE_WRITE)) {
                    pmm_free(new_page);
                    mmu_destroy_address_space(new_pd);
                    return NULL;
                }
            }
        }
    }
    
    return new_pd;
}

void paging_switch_directory(page_directory_t *pd) {
    if (pd == NULL) {
        return;
    }
    
    mmu_switch_address_space(pd);
}

page_directory_t *paging_get_kernel_directory(void) {
    return kernel_directory;
}

page_directory_t *paging_get_current_directory(void) {
    return mmu_get_current_address_space();
}

uint64_t paging_get_flags_for_region_type(region_type_t type) {
    switch (type) {
        case REGION_TYPE_KERNEL_CODE:
            return PAGE_PRESENT | PAGE_GLOBAL;
        
        case REGION_TYPE_KERNEL_DATA:
            return PAGE_PRESENT | PAGE_WRITE | PAGE_GLOBAL;
        
        case REGION_TYPE_KERNEL_STACK:
            return PAGE_PRESENT | PAGE_WRITE | PAGE_GLOBAL | PAGE_NX;
        
        case REGION_TYPE_USER_CODE:
            return PAGE_PRESENT | PAGE_USER;
        
        case REGION_TYPE_USER_DATA:
            return PAGE_PRESENT | PAGE_WRITE | PAGE_USER | PAGE_NX;
        
        case REGION_TYPE_USER_STACK:
            return PAGE_PRESENT | PAGE_WRITE | PAGE_USER | PAGE_NX;
        
        case REGION_TYPE_MMIO:
            return PAGE_PRESENT | PAGE_WRITE | PAGE_CACHE_DISABLE | PAGE_WRITETHROUGH;
        
        case REGION_TYPE_FRAMEBUFFER:
            return PAGE_PRESENT | PAGE_WRITE | PAGE_CACHE_DISABLE;
        
        default:
            return PAGE_PRESENT;
    }
}

bool paging_protect_range(page_directory_t *pd, uint64_t virt_base, size_t num_pages, uint64_t flags) {
    if (pd == NULL) {
        return false;
    }
    
    virt_base &= ~0xFFFULL;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint64_t virt = virt_base + (i * PAGE_SIZE);
        
        if (!mmu_is_mapped(pd, virt)) {
            continue;
        }
        
        uint64_t phys = mmu_get_physical_address(pd, virt);
        
        if (!mmu_unmap_page(pd, virt)) {
            return false;
        }
        
        if (!mmu_map_page(pd, virt, phys, flags)) {
            return false;
        }
    }
    
    return true;
}

void paging_free_user_pages(page_directory_t *pd) {
    if (pd == NULL || pd->pml4 == NULL) {
        return;
    }
    
    extern void serial_printf(uint16_t port, const char *fmt, ...);
    #define DEBUG_PORT 0x3F8
    
    serial_printf(DEBUG_PORT, "Freeing user pages for page directory %p\n", pd);
    
    uint64_t pages_freed = 0;
    pml4e_t *pml4 = pd->pml4;
    
    for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        if (!(pml4[pml4_idx] & PAGE_PRESENT)) continue;
        
        uint64_t pdpt_phys = pml4[pml4_idx] & PAGE_ADDR_MASK;
        pdpte_t *pdpt = (pdpte_t *)mmu_phys_to_virt(pdpt_phys);
        
        for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
            if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) continue;
            if (pdpt[pdpt_idx] & PAGE_HUGE) continue;  /* Skip huge pages */
            
            uint64_t pd_phys = pdpt[pdpt_idx] & PAGE_ADDR_MASK;
            pde_t *pd_table = (pde_t *)mmu_phys_to_virt(pd_phys);
            
            for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
                if (!(pd_table[pd_idx] & PAGE_PRESENT)) continue;
                if (pd_table[pd_idx] & PAGE_HUGE) continue;  /* Skip huge pages */
                
                uint64_t pt_phys = pd_table[pd_idx] & PAGE_ADDR_MASK;
                pte_t *pt = (pte_t *)mmu_phys_to_virt(pt_phys);
                
                for (int pt_idx = 0; pt_idx < 512; pt_idx++) {
                    if (!(pt[pt_idx] & PAGE_PRESENT)) continue;
                    
                    uint64_t page_phys = pt[pt_idx] & PAGE_ADDR_MASK;
                    
                    void *page_virt = mmu_phys_to_virt(page_phys);
                    
                    pmm_free(page_virt);
                    pages_freed++;
                }
            }
        }
    }
    
    serial_printf(DEBUG_PORT, "Freed %llu user pages (%llu KB)\n", 
                  pages_freed, pages_freed * 4);
}

void paging_dump_address_space(page_directory_t *pd) {
    if (pd == NULL) {
        serial_printf(DEBUG_PORT, "Invalid page directory\n");
        return;
    }
    
    serial_printf(DEBUG_PORT, "\n=== Address Space Dump ===\n");
    serial_printf(DEBUG_PORT, "PML4 Physical: 0x%p\n", (void *)pd->phys_addr);
    serial_printf(DEBUG_PORT, "PML4 Virtual: 0x%p\n", (void *)pd->pml4);
    
    uint64_t mapped_pages = 0;
    
    for (uint64_t pml4_idx = 0; pml4_idx < 512; pml4_idx++) {
        bool has_mappings = false;
        
        for (uint64_t pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
            for (uint64_t pd_idx = 0; pd_idx < 512; pd_idx++) {
                for (uint64_t pt_idx = 0; pt_idx < 512; pt_idx++) {
                    uint64_t virt = (pml4_idx << PML4_SHIFT) |
                                   (pdpt_idx << PDPT_SHIFT) |
                                   (pd_idx << PD_SHIFT) |
                                   (pt_idx << PT_SHIFT);
                    
                    if (mmu_is_mapped(pd, virt)) {
                        if (!has_mappings) {
                            serial_printf(DEBUG_PORT, "\nPML4[%u] mappings:\n", (unsigned int)pml4_idx);
                            has_mappings = true;
                        }
                        
                        uint64_t phys = mmu_get_physical_address(pd, virt);
                        serial_printf(DEBUG_PORT, "  0x%p -> 0x%p\n", 
                                    (void *)virt, (void *)phys);
                        mapped_pages++;
                    }
                }
            }
        }
    }
    
    serial_printf(DEBUG_PORT, "\nTotal mapped pages: %llu\n", mapped_pages);
    serial_printf(DEBUG_PORT, "Total mapped memory: %llu KB\n", (mapped_pages * 4));
    serial_printf(DEBUG_PORT, "=========================\n\n");
}