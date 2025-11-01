#ifndef MMU_H
#define MMU_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT 21
#define PT_SHIFT 12

#define PML4_INDEX(addr) (((addr) >> PML4_SHIFT) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> PDPT_SHIFT) & 0x1FF)
#define PD_INDEX(addr) (((addr) >> PD_SHIFT) & 0x1FF)
#define PT_INDEX(addr) (((addr) >> PT_SHIFT) & 0x1FF)

#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITE      (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_WRITETHROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define PAGE_ACCESSED   (1ULL << 5)
#define PAGE_DIRTY      (1ULL << 6)
#define PAGE_HUGE       (1ULL << 7)
#define PAGE_GLOBAL     (1ULL << 8)
#define PAGE_NX         (1ULL << 63)

#define PAGE_ADDR_MASK  0x000FFFFFFFFFF000ULL

typedef uint64_t pml4e_t;
typedef uint64_t pdpte_t;
typedef uint64_t pde_t;
typedef uint64_t pte_t;

typedef struct {
    pml4e_t *pml4;
    uint64_t phys_addr;
} page_directory_t;

typedef void (*page_fault_handler_t)(uint64_t fault_addr, uint64_t error_code);

void mmu_init(struct limine_hhdm_response *hhdm);
page_directory_t *mmu_create_address_space(void);
void mmu_destroy_address_space(page_directory_t *pd);
void mmu_switch_address_space(page_directory_t *pd);
page_directory_t *mmu_get_current_address_space(void);
bool mmu_map_page(page_directory_t *pd, uint64_t virt, uint64_t phys, uint64_t flags);
bool mmu_unmap_page(page_directory_t *pd, uint64_t virt);
uint64_t mmu_get_physical_address(page_directory_t *pd, uint64_t virt);
bool mmu_is_mapped(page_directory_t *pd, uint64_t virt);
void mmu_set_page_fault_handler(page_fault_handler_t handler);
void mmu_handle_page_fault(uint64_t fault_addr, uint64_t error_code);
void mmu_flush_tlb_single(uint64_t virt);
void mmu_flush_tlb_all(void);

#endif