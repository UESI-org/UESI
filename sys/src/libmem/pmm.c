#include <pmm.h>
#include <limine.h>

static uint8_t *bitmap;
static uint64_t total_pages;
static uint64_t free_pages;
static uint64_t usable_pages;
static uint64_t bitmap_size;
static uint64_t hhdm_offset;

static inline void bitmap_set(uint64_t index) {
    bitmap[index / 8] |= (1 << (index % 8));
}

static inline void bitmap_clear(uint64_t index) {
    bitmap[index / 8] &= ~(1 << (index % 8));
}

static inline bool bitmap_test(uint64_t index) {
    return bitmap[index / 8] & (1 << (index % 8));
}

static uint64_t find_free_page(void) {
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            return i;
        }
    }
    return (uint64_t)-1;
}

static uint64_t find_free_contiguous(size_t num_pages) {
    uint64_t start = 0;
    uint64_t count = 0;
    
    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (count == 0) start = i;
            count++;
            if (count == num_pages) return start;
        } else {
            count = 0;
        }
    }
    return (uint64_t)-1;
}

void pmm_init(struct limine_memmap_response *memmap, struct limine_hhdm_response *hhdm) {
    if (memmap == NULL || hhdm == NULL) {
        return;
    }

    hhdm_offset = hhdm->offset;
    struct limine_memmap_entry **entries = memmap->entries;
    uint64_t entry_count = memmap->entry_count;

    uint64_t highest_addr = 0;
    
    for (uint64_t i = 0; i < entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];
        
        if (entry->base > UINT64_MAX - entry->length) {
            continue;
        }
        
        uint64_t top = entry->base + entry->length;
        if (top > highest_addr) {
            highest_addr = top;
        }
    }

    total_pages = highest_addr / PAGE_SIZE;
    bitmap_size = (total_pages + 7) / 8;

    bitmap = NULL;
    for (uint64_t i = 0; i < entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            bitmap = (uint8_t *)(entry->base + hhdm_offset);
            entry->base += bitmap_size;
            entry->length -= bitmap_size;
            break;
        }
    }

    if (bitmap == NULL) {
        return;
    }

    for (uint64_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFF;
    }
    
    uint64_t excess_bits = total_pages % 8;
    if (excess_bits != 0) {
        uint8_t mask = (1 << excess_bits) - 1;
        bitmap[bitmap_size - 1] = 0xFF & ~mask;
    }

    free_pages = 0;
    usable_pages = 0;
    for (uint64_t i = 0; i < entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t aligned_base = (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            uint64_t aligned_end = (entry->base + entry->length) & ~(PAGE_SIZE - 1);
            
            if (aligned_end > aligned_base) {
                uint64_t base_page = aligned_base / PAGE_SIZE;
                uint64_t page_count = (aligned_end - aligned_base) / PAGE_SIZE;
                
                for (uint64_t j = 0; j < page_count; j++) {
                    bitmap_clear(base_page + j);
                    free_pages++;
                    usable_pages++;
                }
            }
        }
    }
}

void *pmm_alloc(void) {
    uint64_t page_index = find_free_page();
    if (page_index == (uint64_t)-1) {
        return NULL;
    }

    bitmap_set(page_index);
    free_pages--;
    
    uint64_t phys_addr = page_index * PAGE_SIZE;
    return (void *)(phys_addr + hhdm_offset);
}

void pmm_free(void *page) {
    if (page == NULL) return;
    
    uint64_t virt_addr = (uint64_t)page;
    uint64_t phys_addr = virt_addr - hhdm_offset;
    uint64_t page_index = phys_addr / PAGE_SIZE;

    if (page_index >= total_pages) return;
    if (!bitmap_test(page_index)) return;

    bitmap_clear(page_index);
    free_pages++;
}

void *pmm_alloc_contiguous(size_t num_pages) {
    if (num_pages == 0) return NULL;

    uint64_t start_page = find_free_contiguous(num_pages);
    if (start_page == (uint64_t)-1) {
        return NULL;
    }

    for (size_t i = 0; i < num_pages; i++) {
        bitmap_set(start_page + i);
        free_pages--;
    }

    uint64_t phys_addr = start_page * PAGE_SIZE;
    return (void *)(phys_addr + hhdm_offset);
}

void pmm_free_contiguous(void *base, size_t num_pages) {
    if (base == NULL || num_pages == 0) return;

    uint64_t virt_addr = (uint64_t)base;
    uint64_t phys_addr = virt_addr - hhdm_offset;
    uint64_t start_page = phys_addr / PAGE_SIZE;

    for (size_t i = 0; i < num_pages; i++) {
        uint64_t page_index = start_page + i;
        if (page_index >= total_pages) continue;
        if (!bitmap_test(page_index)) continue;

        bitmap_clear(page_index);
        free_pages++;
    }
}

uint64_t pmm_get_free_memory(void) {
    return free_pages * PAGE_SIZE;
}

uint64_t pmm_get_total_memory(void) {
    return usable_pages * PAGE_SIZE;
}