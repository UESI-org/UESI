#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#define PAGE_SIZE 4096

void pmm_init(struct limine_memmap_response *memmap, struct limine_hhdm_response *hhdm);
void *pmm_alloc(void);
void pmm_free(void *page);
void *pmm_alloc_contiguous(size_t num_pages);
void pmm_free_contiguous(void *base, size_t num_pages);
uint64_t pmm_get_free_memory(void);
uint64_t pmm_get_total_memory(void);

#endif