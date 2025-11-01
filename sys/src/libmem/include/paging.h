#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "mmu.h"

#define PAGING_FLAG_PRESENT     PAGE_PRESENT
#define PAGING_FLAG_WRITE       PAGE_WRITE
#define PAGING_FLAG_USER        PAGE_USER
#define PAGING_FLAG_WRITETHROUGH PAGE_WRITETHROUGH
#define PAGING_FLAG_NOCACHE     PAGE_CACHE_DISABLE
#define PAGING_FLAG_GLOBAL      PAGE_GLOBAL
#define PAGING_FLAG_NX          PAGE_NX

typedef struct {
    uint64_t base;
    uint64_t length;
    uint64_t flags;
} memory_region_t;

typedef enum {
    REGION_TYPE_KERNEL_CODE,
    REGION_TYPE_KERNEL_DATA,
    REGION_TYPE_KERNEL_STACK,
    REGION_TYPE_USER_CODE,
    REGION_TYPE_USER_DATA,
    REGION_TYPE_USER_STACK,
    REGION_TYPE_MMIO,
    REGION_TYPE_FRAMEBUFFER
} region_type_t;

void paging_init(void);
bool paging_map_range(page_directory_t *pd, uint64_t virt_base, uint64_t phys_base, 
                      size_t num_pages, uint64_t flags);
bool paging_unmap_range(page_directory_t *pd, uint64_t virt_base, size_t num_pages);
bool paging_map_region(page_directory_t *pd, memory_region_t *region);
bool paging_identity_map(page_directory_t *pd, uint64_t phys_base, size_t num_pages, uint64_t flags);
bool paging_is_range_mapped(page_directory_t *pd, uint64_t virt_base, size_t num_pages);
page_directory_t *paging_create_kernel_address_space(void);
page_directory_t *paging_create_user_address_space(void);
page_directory_t *paging_clone_address_space(page_directory_t *src);
void paging_switch_directory(page_directory_t *pd);
page_directory_t *paging_get_kernel_directory(void);
page_directory_t *paging_get_current_directory(void);
uint64_t paging_get_flags_for_region_type(region_type_t type);
bool paging_protect_range(page_directory_t *pd, uint64_t virt_base, size_t num_pages, uint64_t flags);
void paging_dump_address_space(page_directory_t *pd);

#endif