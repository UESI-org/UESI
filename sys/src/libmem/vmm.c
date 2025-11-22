#include <vmm.h>
#include <paging.h>
#include <pmm.h>
#include <mmu.h>
#include <string.h>

extern void tty_printf(const char *fmt, ...);
extern void serial_printf(uint16_t port, const char *fmt, ...);

#define DEBUG_PORT 0x3F8

static vmm_address_space_t *kernel_space = NULL;
static vmm_address_space_t *current_space = NULL;
static vmm_stats_t stats = {0};

static vmm_region_t *vmm_create_region(uint64_t virt_start, uint64_t virt_end, 
                                       vmm_region_type_t type, uint64_t flags) {
    vmm_region_t *region = (vmm_region_t *)pmm_alloc();
    if (region == NULL) {
        return NULL;
    }
    
    region->virt_start = virt_start;
    region->virt_end = virt_end;
    region->type = type;
    region->flags = flags;
    region->cow = false;
    region->next = NULL;
    
    return region;
}

static void vmm_free_region(vmm_region_t *region) {
    if (region != NULL) {
        pmm_free(region);
    }
}

static vmm_region_t *vmm_find_region(vmm_address_space_t *space, uint64_t virt_addr) {
    if (space == NULL) {
        return NULL;
    }
    
    vmm_region_t *current = space->regions;
    while (current != NULL) {
        if (virt_addr >= current->virt_start && virt_addr < current->virt_end) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

static bool vmm_add_region(vmm_address_space_t *space, vmm_region_t *region) {
    if (space == NULL || region == NULL) {
        return false;
    }
    
    if (space->regions == NULL) {
        space->regions = region;
    } else {
        vmm_region_t *current = space->regions;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = region;
    }
    
    return true;
}

static bool vmm_remove_region(vmm_address_space_t *space, uint64_t virt_start) {
    if (space == NULL || space->regions == NULL) {
        return false;
    }
    
    vmm_region_t *current = space->regions;
    vmm_region_t *prev = NULL;
    
    while (current != NULL) {
        if (current->virt_start == virt_start) {
            if (prev == NULL) {
                space->regions = current->next;
            } else {
                prev->next = current->next;
            }
            vmm_free_region(current);
            return true;
        }
        prev = current;
        current = current->next;
    }
    
    return false;
}

void vmm_init(void) {
    paging_init();
    
    kernel_space = (vmm_address_space_t *)pmm_alloc();
    if (kernel_space == NULL) {
        return;
    }
    
    memset(kernel_space, 0, sizeof(vmm_address_space_t));
    
    kernel_space->page_dir = paging_get_kernel_directory();
    kernel_space->regions = NULL;
    kernel_space->heap_start = VMM_KERNEL_HEAP_START;
    kernel_space->heap_end = VMM_KERNEL_HEAP_START + VMM_KERNEL_HEAP_SIZE;
    kernel_space->brk = VMM_KERNEL_HEAP_START;
    
    current_space = kernel_space;
    
    stats.total_pages = pmm_get_total_memory() / PAGE_SIZE;
    stats.used_pages = 0;
    stats.cached_pages = 0;
    stats.page_faults = 0;
    stats.tlb_flushes = 0;
    
    // mmu_set_page_fault_handler(vmm_handle_page_fault);
}

vmm_address_space_t *vmm_create_address_space(bool is_kernel) {
    vmm_address_space_t *space = (vmm_address_space_t *)pmm_alloc();
    if (space == NULL) {
        return NULL;
    }
    
    memset(space, 0, sizeof(vmm_address_space_t));
    
    if (is_kernel) {
        space->page_dir = paging_create_kernel_address_space();
        space->heap_start = VMM_KERNEL_HEAP_START;
        space->heap_end = VMM_KERNEL_HEAP_START + VMM_KERNEL_HEAP_SIZE;
    } else {
        space->page_dir = paging_create_user_address_space();
        space->heap_start = VMM_USER_HEAP_START;
        space->heap_end = VMM_USER_STACK_TOP - VMM_USER_STACK_SIZE;
    }
    
    space->brk = space->heap_start;
    space->regions = NULL;
    
    if (space->page_dir == NULL) {
        pmm_free(space);
        return NULL;
    }
    
    return space;
}

void vmm_destroy_address_space(vmm_address_space_t *space) {
    if (space == NULL) {
        return;
    }
    
    vmm_region_t *current = space->regions;
    while (current != NULL) {
        vmm_region_t *next = current->next;
        
        size_t num_pages = (current->virt_end - current->virt_start) / PAGE_SIZE;
        paging_unmap_range(space->page_dir, current->virt_start, num_pages);
        
        vmm_free_region(current);
        current = next;
    }
    
    mmu_destroy_address_space(space->page_dir);
    pmm_free(space);
}

vmm_address_space_t *vmm_fork_address_space(vmm_address_space_t *parent) {
    if (parent == NULL) {
        return NULL;
    }
    
    vmm_address_space_t *child = vmm_create_address_space(false);
    if (child == NULL) {
        return NULL;
    }
    
    vmm_region_t *current = parent->regions;
    while (current != NULL) {
        vmm_region_t *new_region = vmm_create_region(current->virt_start, 
                                                     current->virt_end,
                                                     current->type, 
                                                     current->flags);
        if (new_region == NULL) {
            vmm_destroy_address_space(child);
            return NULL;
        }
        
        new_region->cow = true;
        vmm_add_region(child, new_region);
        
        size_t num_pages = (current->virt_end - current->virt_start) / PAGE_SIZE;
        for (size_t i = 0; i < num_pages; i++) {
            uint64_t virt = current->virt_start + (i * PAGE_SIZE);
            uint64_t phys = mmu_get_physical_address(parent->page_dir, virt);
            
            if (phys != 0) {
                uint64_t cow_flags = current->flags & ~PAGE_WRITE;
                
                mmu_map_page(child->page_dir, virt, phys, cow_flags);
                
                paging_protect_range(parent->page_dir, virt, 1, cow_flags);
            }
        }
        
        current->cow = true;
        
        current = current->next;
    }
    
    child->brk = parent->brk;
    
    return child;
}

void vmm_switch_address_space(vmm_address_space_t *space) {
    if (space == NULL || space->page_dir == NULL) {
        return;
    }
    
    paging_switch_directory(space->page_dir);
    current_space = space;
    vmm_flush_tlb_all();
}

vmm_address_space_t *vmm_get_kernel_space(void) {
    return kernel_space;
}

vmm_address_space_t *vmm_get_current_space(void) {
    return current_space;
}

void *vmm_alloc(vmm_address_space_t *space, size_t size) {
    if (space == NULL || size == 0) {
        return NULL;
    }
    
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    uint64_t virt_addr = space->brk;
    if (virt_addr + size > space->heap_end) {
        return NULL;
    }
    
    vmm_region_t *check = space->regions;
    while (check != NULL) {
        if (!(virt_addr + size <= check->virt_start || virt_addr >= check->virt_end)) {
            return NULL;
        }
        check = check->next;
    }
    
    size_t num_pages = size / PAGE_SIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        void *phys_page = pmm_alloc();
        if (phys_page == NULL) {
            for (size_t j = 0; j < i; j++) {
                uint64_t virt = virt_addr + (j * PAGE_SIZE);
                uint64_t phys = mmu_get_physical_address(space->page_dir, virt);
                if (phys != 0) {
                    mmu_unmap_page(space->page_dir, virt);
                    void *virt = mmu_phys_to_virt(phys);
                    pmm_free(virt);
                }
            }
            return NULL;
        }
        
        memset(phys_page, 0, PAGE_SIZE);
        
        uint64_t phys = mmu_virt_to_phys(phys_page);
        uint64_t flags = PAGE_PRESENT | PAGE_WRITE;
        
        if (space != kernel_space) {
            flags |= PAGE_USER;
        }
        
        if (!mmu_map_page(space->page_dir, virt_addr + (i * PAGE_SIZE), phys, flags)) {
            pmm_free(phys_page);
            for (size_t j = 0; j < i; j++) {
                uint64_t virt = virt_addr + (j * PAGE_SIZE);
                uint64_t phys_addr = mmu_get_physical_address(space->page_dir, virt);
                if (phys_addr != 0) {
                    mmu_unmap_page(space->page_dir, virt);
                    pmm_free((void *)phys_addr);
                }
            }
            return NULL;
        }
    }
    
    if (virt_addr == space->brk) {
        space->brk += size;
    }
    
    if (stats.used_pages + num_pages >= stats.used_pages) {  // Overflow check
        stats.used_pages += num_pages;
    }
    
    vmm_region_t *region = vmm_create_region(virt_addr, virt_addr + size,
                                             space == kernel_space ? VMM_REGION_KERNEL_HEAP : VMM_REGION_USER_HEAP,
                                             PAGE_PRESENT | PAGE_WRITE);
    if (region != NULL) {
        vmm_add_region(space, region);
    }
    
    return (void *)virt_addr;
}

void *vmm_alloc_at(vmm_address_space_t *space, uint64_t virt_addr, size_t size, uint64_t flags) {
    if (space == NULL || size == 0) {
        return NULL;
    }
    
    virt_addr &= ~(PAGE_SIZE - 1);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t num_pages = size / PAGE_SIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        void *phys_page = pmm_alloc();
        if (phys_page == NULL) {
            for (size_t j = 0; j < i; j++) {
                uint64_t virt = virt_addr + (j * PAGE_SIZE);
                uint64_t phys = mmu_get_physical_address(space->page_dir, virt);
                if (phys != 0) {
                    mmu_unmap_page(space->page_dir, virt);
                    pmm_free(mmu_phys_to_virt(phys));
                }
            }
            return NULL;
        }
        
        memset(phys_page, 0, PAGE_SIZE);
        
        uint64_t phys = mmu_virt_to_phys(phys_page);
        if (!mmu_map_page(space->page_dir, virt_addr + (i * PAGE_SIZE), phys, flags)) {
            pmm_free(phys_page);
            for (size_t j = 0; j < i; j++) {
                uint64_t virt = virt_addr + (j * PAGE_SIZE);
                uint64_t phys_addr = mmu_get_physical_address(space->page_dir, virt);
                if (phys_addr != 0) {
                    mmu_unmap_page(space->page_dir, virt);
                    pmm_free((void *)phys_addr);
                }
            }
            return NULL;
        }
    }
    
    if (stats.used_pages + num_pages >= stats.used_pages) {  // Overflow check
        stats.used_pages += num_pages;
    }
    
    return (void *)virt_addr;
}

void vmm_free(vmm_address_space_t *space, void *ptr, size_t size) {
    if (space == NULL || ptr == NULL || size == 0) {
        return;
    }
    
    uint64_t virt_addr = (uint64_t)ptr;
    virt_addr &= ~(PAGE_SIZE - 1);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t num_pages = size / PAGE_SIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        uint64_t virt = virt_addr + (i * PAGE_SIZE);
        uint64_t phys = mmu_get_physical_address(space->page_dir, virt);
        
        if (phys != 0) {
            mmu_unmap_page(space->page_dir, virt);
            pmm_free((void *)phys);
        }
    }
    
    if (stats.used_pages >= num_pages) {
        stats.used_pages -= num_pages;
    } else {
        stats.used_pages = 0;
    }
    
    vmm_remove_region(space, virt_addr);
}

bool vmm_map_region(vmm_address_space_t *space, uint64_t virt_start, size_t size,
                    vmm_region_type_t type) {
    if (space == NULL || size == 0) {
        return false;
    }
    
    virt_start &= ~(PAGE_SIZE - 1);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    uint64_t flags = paging_get_flags_for_region_type((region_type_t)type);
    size_t num_pages = size / PAGE_SIZE;
    
    for (size_t i = 0; i < num_pages; i++) {
        void *phys_page = pmm_alloc();
        if (phys_page == NULL) {
            return false;
        }
        
        uint64_t phys = mmu_virt_to_phys(phys_page);
        if (!mmu_map_page(space->page_dir, virt_start + (i * PAGE_SIZE), phys, flags)) {
            pmm_free(phys_page);
            return false;
        }
    }
    
    vmm_region_t *region = vmm_create_region(virt_start, virt_start + size, type, flags);
    if (region == NULL) {
        return false;
    }
    
    vmm_add_region(space, region);
    stats.used_pages += num_pages;
    
    return true;
}

bool vmm_unmap_region(vmm_address_space_t *space, uint64_t virt_start, size_t size) {
    if (space == NULL || size == 0) {
        return false;
    }
    
    virt_start &= ~(PAGE_SIZE - 1);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t num_pages = size / PAGE_SIZE;
    
    paging_unmap_range(space->page_dir, virt_start, num_pages);
    vmm_remove_region(space, virt_start);
    
    if (stats.used_pages >= num_pages) {
        stats.used_pages -= num_pages;
    } else {
        stats.used_pages = 0;
    }
    
    return true;
}

bool vmm_protect_region(vmm_address_space_t *space, uint64_t virt_start, size_t size,
                        uint64_t flags) {
    if (space == NULL || size == 0) {
        return false;
    }
    
    virt_start &= ~(PAGE_SIZE - 1);
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t num_pages = size / PAGE_SIZE;
    
    return paging_protect_range(space->page_dir, virt_start, num_pages, flags);
}

void *vmm_sbrk(vmm_address_space_t *space, intptr_t increment) {
    if (space == NULL) {
        return (void *)-1;
    }
    
    uint64_t old_brk = space->brk;
    
    if (increment == 0) {
        return (void *)old_brk;
    }
    
    uint64_t new_brk = old_brk + increment;
    
    if (new_brk > space->heap_end || new_brk < space->heap_start) {
        return (void *)-1;
    }
    
    if (increment > 0) {
        size_t alloc_size = increment;
        void *result = vmm_alloc(space, alloc_size);
        if (result == NULL) {
            return (void *)-1;
        }
    } else {
        uint64_t free_start = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t free_end = old_brk & ~(PAGE_SIZE - 1);
        
        if (free_end > free_start) {
            size_t free_size = free_end - free_start;
            vmm_free(space, (void *)free_start, free_size);
        }
        
        space->brk = new_brk;
    }
    
    return (void *)old_brk;
}

void vmm_handle_page_fault(uint64_t fault_addr, uint64_t error_code) {
    stats.page_faults++;
    
    vmm_address_space_t *space = current_space;
    if (space == NULL) {
        serial_printf(DEBUG_PORT, "Page fault in NULL address space at 0x%p\n", 
                     (void *)fault_addr);
        return;
    }
    
    vmm_region_t *region = vmm_find_region(space, fault_addr);
    if (region == NULL) {
        serial_printf(DEBUG_PORT, "Page fault at unmapped address 0x%p (error: 0x%p)\n",
                     (void *)fault_addr, (void *)error_code);
        return;
    }
    
    if (region->cow && (error_code & 0x2)) {
        uint64_t page_addr = fault_addr & ~(PAGE_SIZE - 1);
        
        uint64_t old_phys = mmu_get_physical_address(space->page_dir, page_addr);
        if (old_phys == 0) {
            serial_printf(DEBUG_PORT, "COW fault but page not mapped at 0x%p\n",
                         (void *)fault_addr);
            return;
        }
        
        void *new_page = pmm_alloc();
        if (new_page == NULL) {
            serial_printf(DEBUG_PORT, "Failed to allocate page for COW at 0x%p\n",
                         (void *)fault_addr);
            return;
        }
        
        void *old_virt = mmu_phys_to_virt(old_phys);
        memcpy(new_page, old_virt, PAGE_SIZE);
        
        mmu_unmap_page(space->page_dir, page_addr);
        
        uint64_t new_flags = region->flags | PAGE_WRITE;
        uint64_t new_phys = mmu_virt_to_phys(new_page);
        if (!mmu_map_page(space->page_dir, page_addr, new_phys, new_flags)) {
            serial_printf(DEBUG_PORT, "Failed to remap COW page at 0x%p\n",
                         (void *)fault_addr);
            pmm_free(new_page);
            return;
        }
        size_t region_pages = (region->virt_end - region->virt_start) / PAGE_SIZE;
        
        vmm_flush_tlb(page_addr);
        
        serial_printf(DEBUG_PORT, "COW page fault resolved at 0x%p\n", (void *)fault_addr);
    } else {
        serial_printf(DEBUG_PORT, "Unhandled page fault at 0x%p (error: 0x%p)\n",
                     (void *)fault_addr, (void *)error_code);
    }
}


void vmm_flush_tlb(uint64_t virt_addr) {
    mmu_flush_tlb_single(virt_addr);
    stats.tlb_flushes++;
}

void vmm_flush_tlb_all(void) {
    mmu_flush_tlb_all();
    stats.tlb_flushes++;
}

vmm_stats_t vmm_get_stats(void) {
    return stats;
}

void vmm_dump_regions(vmm_address_space_t *space) {
    if (space == NULL) {
        serial_printf(DEBUG_PORT, "NULL address space\n");
        return;
    }
    
    serial_printf(DEBUG_PORT, "\n=== VMM Address Space Dump ===\n");
    serial_printf(DEBUG_PORT, "Heap: 0x%p - 0x%p\n", 
                 (void *)space->heap_start, (void *)space->heap_end);
    serial_printf(DEBUG_PORT, "BRK: 0x%p\n", (void *)space->brk);
    serial_printf(DEBUG_PORT, "\nRegions:\n");
    
    vmm_region_t *current = space->regions;
    int count = 0;
    
    while (current != NULL) {
        serial_printf(DEBUG_PORT, "  [%d] 0x%p - 0x%p (type=%d, cow=%d)\n",
                     count++, (void *)current->virt_start, (void *)current->virt_end,
                     current->type, current->cow);
        current = current->next;
    }
    
    serial_printf(DEBUG_PORT, "\nStats:\n");
    serial_printf(DEBUG_PORT, "  Total pages: %llu\n", stats.total_pages);
    serial_printf(DEBUG_PORT, "  Used pages: %llu\n", stats.used_pages);
    serial_printf(DEBUG_PORT, "  Page faults: %llu\n", stats.page_faults);
    serial_printf(DEBUG_PORT, "  TLB flushes: %llu\n", stats.tlb_flushes);
    serial_printf(DEBUG_PORT, "==============================\n\n");
}