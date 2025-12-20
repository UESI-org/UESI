#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

typedef struct {
	uint64_t total_physical;         /* Total physical memory in system */
	uint64_t usable_memory;          /* Total usable RAM */
	uint64_t reserved_memory;        /* Reserved/MMIO/ACPI memory */
	uint64_t bootloader_reclaimable; /* Bootloader reclaimable memory */
	uint64_t total_pages;            /* Total pages being tracked */
	uint64_t free_pages;             /* Currently free pages */
	uint64_t used_pages;             /* Currently allocated pages */
	uint64_t peak_used_pages;        /* Peak memory usage */
	uint64_t alloc_count;            /* Total allocations */
	uint64_t free_count;             /* Total frees */
	uint64_t bitmap_size;            /* Size of bitmap in bytes */
} pmm_stats_t;

typedef struct {
	uint64_t base;
	uint64_t length;
	uint64_t type;
	const char *type_name;
} pmm_region_info_t;

void pmm_init(struct limine_memmap_response *memmap,
              struct limine_hhdm_response *hhdm);

void *pmm_alloc(void);
void pmm_free(void *page);

void *pmm_alloc_contiguous(size_t num_pages);
void pmm_free_contiguous(void *base, size_t num_pages);
void pmm_reclaim_bootloader_memory(void);
bool pmm_is_page_allocated(void *page);
uint64_t pmm_get_free_memory(void);
uint64_t pmm_get_total_memory(void);
void pmm_get_stats(pmm_stats_t *stats);
void pmm_print_stats(void);
void pmm_print_memory_map(void);
bool pmm_validate(void);

#endif