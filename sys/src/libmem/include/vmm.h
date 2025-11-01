#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "paging.h"

#define VMM_KERNEL_HEAP_START   0xFFFFFFFF90000000ULL
#define VMM_KERNEL_HEAP_SIZE    (64 * 1024 * 1024)
#define VMM_USER_HEAP_START     0x0000000001000000ULL
#define VMM_USER_STACK_TOP      0x00007FFFFFFFF000ULL
#define VMM_USER_STACK_SIZE     (8 * 1024 * 1024)

typedef enum {
    VMM_REGION_KERNEL_CODE,
    VMM_REGION_KERNEL_DATA,
    VMM_REGION_KERNEL_HEAP,
    VMM_REGION_KERNEL_STACK,
    VMM_REGION_USER_CODE,
    VMM_REGION_USER_DATA,
    VMM_REGION_USER_HEAP,
    VMM_REGION_USER_STACK,
    VMM_REGION_SHARED,
    VMM_REGION_MMIO
} vmm_region_type_t;

typedef struct vmm_region {
    uint64_t virt_start;
    uint64_t virt_end;
    uint64_t flags;
    vmm_region_type_t type;
    bool cow;
    struct vmm_region *next;
} vmm_region_t;

typedef struct {
    page_directory_t *page_dir;
    vmm_region_t *regions;
    uint64_t heap_start;
    uint64_t heap_end;
    uint64_t brk;
} vmm_address_space_t;

typedef struct {
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t cached_pages;
    uint64_t page_faults;
    uint64_t tlb_flushes;
} vmm_stats_t;

void vmm_init(void);
vmm_address_space_t *vmm_create_address_space(bool is_kernel);
void vmm_destroy_address_space(vmm_address_space_t *space);
vmm_address_space_t *vmm_fork_address_space(vmm_address_space_t *parent);
void vmm_switch_address_space(vmm_address_space_t *space);
vmm_address_space_t *vmm_get_kernel_space(void);
vmm_address_space_t *vmm_get_current_space(void);
void *vmm_alloc(vmm_address_space_t *space, size_t size);
void *vmm_alloc_at(vmm_address_space_t *space, uint64_t virt_addr, size_t size, uint64_t flags);
void vmm_free(vmm_address_space_t *space, void *ptr, size_t size);
bool vmm_map_region(vmm_address_space_t *space, uint64_t virt_start, size_t size, 
                    vmm_region_type_t type);
bool vmm_unmap_region(vmm_address_space_t *space, uint64_t virt_start, size_t size);
bool vmm_protect_region(vmm_address_space_t *space, uint64_t virt_start, size_t size, 
                        uint64_t flags);
void *vmm_sbrk(vmm_address_space_t *space, intptr_t increment);
void vmm_handle_page_fault(uint64_t fault_addr, uint64_t error_code);
void vmm_flush_tlb(uint64_t virt_addr);
void vmm_flush_tlb_all(void);
vmm_stats_t vmm_get_stats(void);
void vmm_dump_regions(vmm_address_space_t *space);

#endif