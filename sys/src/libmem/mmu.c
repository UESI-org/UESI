#include <mmu.h>
#include <pmm.h>
#include <limine.h>
#include <string.h>

static uint64_t hhdm_offset;
static page_directory_t *kernel_pd;
static page_directory_t *current_pd = NULL;
static page_fault_handler_t page_fault_handler;

static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline void write_cr3(uint64_t cr3) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

static inline uint64_t read_cr2(void) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

static inline uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - hhdm_offset;
}

static uint64_t *get_next_level(uint64_t *table, size_t index, bool create, uint64_t flags) {
    if (!(table[index] & PAGE_PRESENT)) {
        if (!create) {
            return NULL;
        }
        
        void *new_table = pmm_alloc();
        if (new_table == NULL) {
            return NULL;
        }
        
        memset(new_table, 0, PAGE_SIZE);
        table[index] = virt_to_phys(new_table) | flags | PAGE_PRESENT;
    }
    
    uint64_t phys = table[index] & PAGE_ADDR_MASK;
    return (uint64_t *)phys_to_virt(phys);
}

void mmu_init(struct limine_hhdm_response *hhdm) {
    if (hhdm == NULL) {
        return;
    }
    
    hhdm_offset = hhdm->offset;
    uint64_t current_cr3 = read_cr3();
    
    kernel_pd = (page_directory_t *)pmm_alloc();
    if (kernel_pd == NULL) {
        return;
    }
    
    kernel_pd->pml4 = (pml4e_t *)phys_to_virt(current_cr3);
    kernel_pd->phys_addr = current_cr3;
    
    current_pd = kernel_pd;
    
    page_fault_handler = NULL;
}

page_directory_t *mmu_create_address_space(void) {
    page_directory_t *pd = pmm_alloc();
    if (pd == NULL) {
        return NULL;
    }
    
    void *pml4 = pmm_alloc();
    if (pml4 == NULL) {
        pmm_free(pd);
        return NULL;
    }
    
    memset(pml4, 0, PAGE_SIZE);
    
    if (kernel_pd != NULL && kernel_pd->pml4 != NULL) {
        pml4e_t *kernel_pml4 = kernel_pd->pml4;
        pml4e_t *new_pml4 = pml4;
        
        for (int i = 256; i < 512; i++) {
            new_pml4[i] = kernel_pml4[i];
        }
    }
    
    pd->pml4 = pml4;
    pd->phys_addr = virt_to_phys(pml4);
    
    return pd;
}

void mmu_destroy_address_space(page_directory_t *pd) {
    if (pd == NULL || pd->pml4 == NULL) {
        return;
    }
    
    pml4e_t *pml4 = pd->pml4;
    
    for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        if (!(pml4[pml4_idx] & PAGE_PRESENT)) continue;
        
        pdpte_t *pdpt = phys_to_virt(pml4[pml4_idx] & PAGE_ADDR_MASK);
        
        for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
            if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) continue;
            if (pdpt[pdpt_idx] & PAGE_HUGE) continue;
            
            pde_t *pd_table = phys_to_virt(pdpt[pdpt_idx] & PAGE_ADDR_MASK);
            
            for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
                if (!(pd_table[pd_idx] & PAGE_PRESENT)) continue;
                if (pd_table[pd_idx] & PAGE_HUGE) continue;
                
                pte_t *pt = phys_to_virt(pd_table[pd_idx] & PAGE_ADDR_MASK);
                pmm_free(pt);
            }
            
            pmm_free(pd_table);
        }
        
        pmm_free(pdpt);
    }
    
    pmm_free(pd->pml4);
    pmm_free(pd);
}

void mmu_switch_address_space(page_directory_t *pd) {
    if (pd == NULL) {
        return;
    }
    
    write_cr3(pd->phys_addr);
    current_pd = pd;
}

page_directory_t *mmu_get_current_address_space(void) {
    return current_pd;
}

bool mmu_map_page(page_directory_t *pd, uint64_t virt, uint64_t phys, uint64_t flags) {
    if (pd == NULL || pd->pml4 == NULL) {
        return false;
    }
    
    virt &= ~0xFFF;
    phys &= ~0xFFF;
    
    pml4e_t *pml4 = pd->pml4;
    
    uint64_t table_flags = PAGE_WRITE | (flags & PAGE_USER);
    
    pdpte_t *pdpt = get_next_level((uint64_t *)pml4, PML4_INDEX(virt), true, table_flags);
    if (pdpt == NULL) {
        return false;
    }
    
    pde_t *pd_table = get_next_level((uint64_t *)pdpt, PDPT_INDEX(virt), true, table_flags);
    if (pd_table == NULL) {
        return false;
    }
    
    pte_t *pt = get_next_level((uint64_t *)pd_table, PD_INDEX(virt), true, table_flags);
    if (pt == NULL) {
        return false;
    }
    
    pt[PT_INDEX(virt)] = phys | flags | PAGE_PRESENT;
    
    mmu_flush_tlb_single(virt);
    
    return true;
}

bool mmu_unmap_page(page_directory_t *pd, uint64_t virt) {
    if (pd == NULL || pd->pml4 == NULL) {
        return false;
    }
    
    virt &= ~0xFFF;
    
    pml4e_t *pml4 = pd->pml4;
    
    if (!(pml4[PML4_INDEX(virt)] & PAGE_PRESENT)) {
        return false;
    }
    
    pdpte_t *pdpt = phys_to_virt(pml4[PML4_INDEX(virt)] & PAGE_ADDR_MASK);
    if (!(pdpt[PDPT_INDEX(virt)] & PAGE_PRESENT)) {
        return false;
    }
    
    pde_t *pd_table = phys_to_virt(pdpt[PDPT_INDEX(virt)] & PAGE_ADDR_MASK);
    if (!(pd_table[PD_INDEX(virt)] & PAGE_PRESENT)) {
        return false;
    }
    
    pte_t *pt = phys_to_virt(pd_table[PD_INDEX(virt)] & PAGE_ADDR_MASK);
    
    pt[PT_INDEX(virt)] = 0;
    
    mmu_flush_tlb_single(virt);
    
    return true;
}

uint64_t mmu_get_physical_address(page_directory_t *pd, uint64_t virt) {
    if (pd == NULL || pd->pml4 == NULL) {
        return 0;
    }
    
    uint64_t page_offset = virt & 0xFFF;
    virt &= ~0xFFF;
    
    pml4e_t *pml4 = pd->pml4;
    
    if (!(pml4[PML4_INDEX(virt)] & PAGE_PRESENT)) {
        return 0;
    }
    
    pdpte_t *pdpt = phys_to_virt(pml4[PML4_INDEX(virt)] & PAGE_ADDR_MASK);
    if (!(pdpt[PDPT_INDEX(virt)] & PAGE_PRESENT)) {
        return 0;
    }
    
    pde_t *pd_table = phys_to_virt(pdpt[PDPT_INDEX(virt)] & PAGE_ADDR_MASK);
    if (!(pd_table[PD_INDEX(virt)] & PAGE_PRESENT)) {
        return 0;
    }
    
    pte_t *pt = phys_to_virt(pd_table[PD_INDEX(virt)] & PAGE_ADDR_MASK);
    if (!(pt[PT_INDEX(virt)] & PAGE_PRESENT)) {
        return 0;
    }
    
    return (pt[PT_INDEX(virt)] & PAGE_ADDR_MASK) | page_offset;
}

bool mmu_is_mapped(page_directory_t *pd, uint64_t virt) {
    return mmu_get_physical_address(pd, virt) != 0;
}

void mmu_set_page_fault_handler(page_fault_handler_t handler) {
    page_fault_handler = handler;
}

void mmu_handle_page_fault(uint64_t fault_addr, uint64_t error_code) {
    if (page_fault_handler != NULL) {
        page_fault_handler(fault_addr, error_code);
    }
}

void mmu_flush_tlb_single(uint64_t virt) {
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

void mmu_flush_tlb_all(void) {
    write_cr3(read_cr3());
}

void *mmu_phys_to_virt(uint64_t phys) {
    return phys_to_virt(phys);
}

uint64_t mmu_virt_to_phys(void *virt) {
    return virt_to_phys(virt);
}