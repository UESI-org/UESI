#include <pmm.h>
#include <limine.h>
#include <string.h>

extern void serial_printf(uint16_t port, const char *fmt, ...);
#define DEBUG_PORT 0x3F8

static uint8_t *bitmap = NULL;
static uint64_t total_pages = 0;
static uint64_t free_pages = 0;
static uint64_t usable_pages = 0;
static uint64_t bitmap_size = 0;
static uint64_t hhdm_offset = 0;

static uint64_t last_alloc_hint = 0;
static pmm_stats_t stats = { 0 };
static struct limine_memmap_response *saved_memmap = NULL;

#define BITMAP_INDEX(page) ((page) / 8)
#define BITMAP_BIT(page) ((page) % 8)
#define BITMAP_QWORD_INDEX(page) ((page) / 64)
#define BITMAP_QWORD_BIT(page) ((page) % 64)

static inline uint64_t
bytes_to_mb(uint64_t bytes)
{
	return bytes / (1024 * 1024);
}

static inline uint64_t
bytes_to_kb(uint64_t bytes)
{
	return bytes / 1024;
}

static inline uint64_t
bytes_to_mb_frac(uint64_t bytes)
{
	return ((bytes % (1024 * 1024)) * 100) / (1024 * 1024);
}

static inline uint64_t
bytes_to_kb_frac(uint64_t bytes)
{
	return ((bytes % 1024) * 100) / 1024;
}

static inline void
bitmap_set(uint64_t index)
{
	bitmap[BITMAP_INDEX(index)] |= (1 << BITMAP_BIT(index));
}

static inline void
bitmap_clear(uint64_t index)
{
	bitmap[BITMAP_INDEX(index)] &= ~(1 << BITMAP_BIT(index));
}

static inline bool
bitmap_test(uint64_t index)
{
	return bitmap[BITMAP_INDEX(index)] & (1 << BITMAP_BIT(index));
}

static inline uint64_t *
bitmap_as_qwords(void)
{
	return (uint64_t *)bitmap;
}

static uint64_t
find_free_page_fast(void)
{
	size_t bitmap_qwords = (total_pages + 63) / 64;
	uint64_t *qwords = bitmap_as_qwords();

	size_t start_qword = BITMAP_QWORD_INDEX(last_alloc_hint);

	for (size_t i = start_qword; i < bitmap_qwords; i++) {
		if (qwords[i] != 0xFFFFFFFFFFFFFFFFULL) {
			/* Found a qword with free bits (0s in bitmap = free) */
			uint64_t bits =
			    ~qwords[i]; /* Invert: free bits become 1s */
			int bit =
			    __builtin_ctzll(bits); /* Count trailing zeros */
			uint64_t page = (i * 64) + bit;

			if (page < total_pages) {
				last_alloc_hint = page;
				return page;
			}
		}
	}

	for (size_t i = 0; i < start_qword; i++) {
		if (qwords[i] != 0xFFFFFFFFFFFFFFFFULL) {
			uint64_t bits = ~qwords[i];
			int bit = __builtin_ctzll(bits);
			uint64_t page = (i * 64) + bit;

			if (page < total_pages) {
				last_alloc_hint = page;
				return page;
			}
		}
	}

	return (uint64_t)-1; /* No free pages */
}

static uint64_t
find_free_contiguous(size_t num_pages)
{
	if (num_pages == 0 || num_pages > total_pages) {
		return (uint64_t)-1;
	}

	uint64_t start = 0;
	uint64_t count = 0;

	for (uint64_t i = 0; i < total_pages; i++) {
		if (!bitmap_test(i)) {
			if (count == 0) {
				start = i;
			}
			count++;

			if (count == num_pages) {
				return start;
			}
		} else {
			count = 0;
		}
	}

	return (uint64_t)-1;
}

static const char *
get_memmap_type_name(uint64_t type)
{
	switch (type) {
	case LIMINE_MEMMAP_USABLE:
		return "Usable";
	case LIMINE_MEMMAP_RESERVED:
		return "Reserved";
	case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
		return "ACPI Reclaimable";
	case LIMINE_MEMMAP_ACPI_NVS:
		return "ACPI NVS";
	case LIMINE_MEMMAP_BAD_MEMORY:
		return "Bad Memory";
	case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
		return "Bootloader Reclaimable";
	case LIMINE_MEMMAP_KERNEL_AND_MODULES:
		return "Kernel/Modules";
	case LIMINE_MEMMAP_FRAMEBUFFER:
		return "Framebuffer";
	default:
		return "Unknown";
	}
}

void
pmm_init(struct limine_memmap_response *memmap,
         struct limine_hhdm_response *hhdm)
{
	if (memmap == NULL || hhdm == NULL) {
		serial_printf(DEBUG_PORT, "[PMM] FATAL: NULL memmap or hhdm\n");
		return;
	}

	saved_memmap = memmap;
	hhdm_offset = hhdm->offset;

	struct limine_memmap_entry **entries = memmap->entries;
	uint64_t entry_count = memmap->entry_count;

	serial_printf(DEBUG_PORT,
	              "[PMM] Initializing with %llu memory regions\n",
	              entry_count);

	uint64_t highest_usable_addr = 0;
	stats.total_physical = 0;
	stats.usable_memory = 0;
	stats.reserved_memory = 0;
	stats.bootloader_reclaimable = 0;

	for (uint64_t i = 0; i < entry_count; i++) {
		struct limine_memmap_entry *entry = entries[i];

		if (entry->base > UINT64_MAX - entry->length) {
			serial_printf(
			    DEBUG_PORT,
			    "[PMM] WARNING: Skipping region with overflow\n");
			continue;
		}

		uint64_t top = entry->base + entry->length;

		switch (entry->type) {
		case LIMINE_MEMMAP_USABLE:
			stats.usable_memory += entry->length;
			stats.total_physical += entry->length;
			if (top > highest_usable_addr) {
				highest_usable_addr = top;
			}
			break;
		case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
			stats.bootloader_reclaimable += entry->length;
			stats.total_physical += entry->length;
			if (top > highest_usable_addr) {
				highest_usable_addr = top;
			}
			break;
		case LIMINE_MEMMAP_RESERVED:
		case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
		case LIMINE_MEMMAP_ACPI_NVS:
			if (entry->base < 0x100000000ULL) { /* Below 4GB */
				stats.reserved_memory += entry->length;
			}
			break;
		case LIMINE_MEMMAP_FRAMEBUFFER:
			break;
		}
	}

	total_pages = highest_usable_addr / PAGE_SIZE;
	bitmap_size = (total_pages + 7) / 8;

	uint64_t track_mb = bytes_to_mb(total_pages * PAGE_SIZE);
	uint64_t track_mb_frac = bytes_to_mb_frac(total_pages * PAGE_SIZE);
	serial_printf(DEBUG_PORT,
	              "[PMM] Tracking %llu pages (%llu.%02llu MB)\n",
	              total_pages,
	              track_mb,
	              track_mb_frac);

	uint64_t bitmap_kb = bytes_to_kb(bitmap_size);
	uint64_t bitmap_kb_frac = bytes_to_kb_frac(bitmap_size);
	serial_printf(DEBUG_PORT,
	              "[PMM] Bitmap size: %llu bytes (%llu.%02llu KB)\n",
	              bitmap_size,
	              bitmap_kb,
	              bitmap_kb_frac);

	bitmap = NULL;
	for (uint64_t i = 0; i < entry_count; i++) {
		struct limine_memmap_entry *entry = entries[i];

		if (entry->type == LIMINE_MEMMAP_USABLE &&
		    entry->length >= bitmap_size) {
			bitmap = (uint8_t *)(entry->base + hhdm_offset);

			entry->base += bitmap_size;
			entry->length -= bitmap_size;

			serial_printf(DEBUG_PORT,
			              "[PMM] Bitmap placed at virtual 0x%p\n",
			              (void *)bitmap);
			break;
		}
	}

	if (bitmap == NULL) {
		serial_printf(DEBUG_PORT,
		              "[PMM] FATAL: Could not allocate bitmap\n");
		return;
	}

	memset(bitmap, 0xFF, bitmap_size);

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
			/* Align to page boundaries */
			uint64_t aligned_base =
			    (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
			uint64_t aligned_end =
			    (entry->base + entry->length) & ~(PAGE_SIZE - 1);

			if (aligned_end > aligned_base) {
				uint64_t base_page = aligned_base / PAGE_SIZE;
				uint64_t page_count =
				    (aligned_end - aligned_base) / PAGE_SIZE;

				for (uint64_t j = 0; j < page_count; j++) {
					uint64_t page = base_page + j;
					if (page < total_pages) {
						bitmap_clear(page);
						free_pages++;
						usable_pages++;
					}
				}
			}
		}
	}

	stats.total_pages = total_pages;
	stats.free_pages = free_pages;
	stats.used_pages = 0;
	stats.peak_used_pages = 0;
	stats.alloc_count = 0;
	stats.free_count = 0;
	stats.bitmap_size = bitmap_size;

	uint64_t free_mb = bytes_to_mb(free_pages * PAGE_SIZE);
	uint64_t free_mb_frac = bytes_to_mb_frac(free_pages * PAGE_SIZE);
	serial_printf(DEBUG_PORT,
	              "[PMM] Initialized: %llu free pages (%llu.%02llu MB)\n",
	              free_pages,
	              free_mb,
	              free_mb_frac);

	uint64_t usable_mb = bytes_to_mb(stats.usable_memory);
	uint64_t usable_mb_frac = bytes_to_mb_frac(stats.usable_memory);
	uint64_t reserved_mb = bytes_to_mb(stats.reserved_memory);
	uint64_t reserved_mb_frac = bytes_to_mb_frac(stats.reserved_memory);
	serial_printf(
	    DEBUG_PORT,
	    "[PMM] Usable: %llu.%02llu MB, Reserved: %llu.%02llu MB\n",
	    usable_mb,
	    usable_mb_frac,
	    reserved_mb,
	    reserved_mb_frac);
}

void *
pmm_alloc(void)
{
	uint64_t page_index = find_free_page_fast();

	if (page_index == (uint64_t)-1) {
		serial_printf(DEBUG_PORT, "[PMM] ERROR: Out of memory\n");
		return NULL;
	}

	bitmap_set(page_index);
	free_pages--;

	stats.free_pages = free_pages;
	stats.used_pages = usable_pages - free_pages;
	stats.alloc_count++;

	if (stats.used_pages > stats.peak_used_pages) {
		stats.peak_used_pages = stats.used_pages;
	}

	uint64_t phys_addr = page_index * PAGE_SIZE;
	return (void *)(phys_addr + hhdm_offset);
}

void
pmm_free(void *page)
{
	if (page == NULL) {
		return;
	}

	uint64_t virt_addr = (uint64_t)page;
	uint64_t phys_addr = virt_addr - hhdm_offset;
	uint64_t page_index = phys_addr / PAGE_SIZE;

	if (page_index >= total_pages) {
		serial_printf(DEBUG_PORT,
		              "[PMM] ERROR: Attempted to free invalid page "
		              "0x%llx (index %llu >= %llu)\n",
		              phys_addr,
		              page_index,
		              total_pages);
		return;
	}

	if (!bitmap_test(page_index)) {
		serial_printf(DEBUG_PORT,
		              "[PMM] WARNING: Double-free detected at physical "
		              "0x%llx (page %llu)\n",
		              phys_addr,
		              page_index);
		return;
	}

	bitmap_clear(page_index);
	free_pages++;

	stats.free_pages = free_pages;
	stats.used_pages = usable_pages - free_pages;
	stats.free_count++;

	if (page_index < last_alloc_hint) {
		last_alloc_hint = page_index;
	}
}

void *
pmm_alloc_contiguous(size_t num_pages)
{
	if (num_pages == 0) {
		return NULL;
	}

	if (num_pages == 1) {
		return pmm_alloc();
	}

	uint64_t start_page = find_free_contiguous(num_pages);

	if (start_page == (uint64_t)-1) {
		serial_printf(
		    DEBUG_PORT,
		    "[PMM] ERROR: Could not find %zu contiguous pages\n",
		    num_pages);
		return NULL;
	}

	for (size_t i = 0; i < num_pages; i++) {
		bitmap_set(start_page + i);
		free_pages--;
	}

	stats.free_pages = free_pages;
	stats.used_pages = usable_pages - free_pages;
	stats.alloc_count += num_pages;

	if (stats.used_pages > stats.peak_used_pages) {
		stats.peak_used_pages = stats.used_pages;
	}

	uint64_t phys_addr = start_page * PAGE_SIZE;
	return (void *)(phys_addr + hhdm_offset);
}

void
pmm_free_contiguous(void *base, size_t num_pages)
{
	if (base == NULL || num_pages == 0) {
		return;
	}

	uint64_t virt_addr = (uint64_t)base;
	uint64_t phys_addr = virt_addr - hhdm_offset;
	uint64_t start_page = phys_addr / PAGE_SIZE;

	for (size_t i = 0; i < num_pages; i++) {
		uint64_t page_index = start_page + i;

		if (page_index >= total_pages) {
			serial_printf(
			    DEBUG_PORT,
			    "[PMM] WARNING: Page %llu out of range in "
			    "contiguous free\n",
			    page_index);
			continue;
		}

		if (!bitmap_test(page_index)) {
			serial_printf(
			    DEBUG_PORT,
			    "[PMM] WARNING: Page %llu already free in "
			    "contiguous free\n",
			    page_index);
			continue;
		}

		bitmap_clear(page_index);
		free_pages++;
	}

	stats.free_pages = free_pages;
	stats.used_pages = usable_pages - free_pages;
	stats.free_count += num_pages;
}

void
pmm_reclaim_bootloader_memory(void)
{
	if (saved_memmap == NULL) {
		serial_printf(
		    DEBUG_PORT,
		    "[PMM] WARNING: Cannot reclaim, no saved memmap\n");
		return;
	}

	uint64_t reclaimed_pages = 0;

	for (uint64_t i = 0; i < saved_memmap->entry_count; i++) {
		struct limine_memmap_entry *entry = saved_memmap->entries[i];

		if (entry->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {
			uint64_t aligned_base =
			    (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
			uint64_t aligned_end =
			    (entry->base + entry->length) & ~(PAGE_SIZE - 1);

			if (aligned_end > aligned_base) {
				uint64_t base_page = aligned_base / PAGE_SIZE;
				uint64_t page_count =
				    (aligned_end - aligned_base) / PAGE_SIZE;

				for (uint64_t j = 0; j < page_count; j++) {
					uint64_t page = base_page + j;
					if (page < total_pages &&
					    bitmap_test(page)) {
						bitmap_clear(page);
						free_pages++;
						usable_pages++;
						reclaimed_pages++;
					}
				}
			}
		}
	}

	stats.free_pages = free_pages;
	stats.used_pages = usable_pages - free_pages;

	uint64_t reclaim_mb = bytes_to_mb(reclaimed_pages * PAGE_SIZE);
	uint64_t reclaim_mb_frac =
	    bytes_to_mb_frac(reclaimed_pages * PAGE_SIZE);
	serial_printf(
	    DEBUG_PORT,
	    "[PMM] Reclaimed %llu pages (%llu.%02llu MB) from bootloader\n",
	    reclaimed_pages,
	    reclaim_mb,
	    reclaim_mb_frac);
}

bool
pmm_is_page_allocated(void *page)
{
	if (page == NULL) {
		return false;
	}

	uint64_t virt_addr = (uint64_t)page;
	uint64_t phys_addr = virt_addr - hhdm_offset;
	uint64_t page_index = phys_addr / PAGE_SIZE;

	if (page_index >= total_pages) {
		return false;
	}

	return bitmap_test(page_index);
}

uint64_t
pmm_get_free_memory(void)
{
	return free_pages * PAGE_SIZE;
}

uint64_t
pmm_get_total_memory(void)
{
	return usable_pages * PAGE_SIZE;
}

void
pmm_get_stats(pmm_stats_t *out_stats)
{
	if (out_stats != NULL) {
		stats.free_pages = free_pages;
		stats.used_pages = usable_pages - free_pages;
		*out_stats = stats;
	}
}

void
pmm_print_stats(void)
{
	uint64_t total_mb, total_frac, usable_mb, usable_frac;
	uint64_t reserved_mb, reserved_frac, bootloader_mb, bootloader_frac;
	uint64_t free_mb, free_frac, used_mb, used_frac, peak_mb, peak_frac;
	uint64_t bitmap_kb, bitmap_frac;

	total_mb = bytes_to_mb(stats.total_physical);
	total_frac = bytes_to_mb_frac(stats.total_physical);
	usable_mb = bytes_to_mb(stats.usable_memory);
	usable_frac = bytes_to_mb_frac(stats.usable_memory);
	reserved_mb = bytes_to_mb(stats.reserved_memory);
	reserved_frac = bytes_to_mb_frac(stats.reserved_memory);
	bootloader_mb = bytes_to_mb(stats.bootloader_reclaimable);
	bootloader_frac = bytes_to_mb_frac(stats.bootloader_reclaimable);

	free_mb = bytes_to_mb(free_pages * PAGE_SIZE);
	free_frac = bytes_to_mb_frac(free_pages * PAGE_SIZE);
	used_mb = bytes_to_mb((usable_pages - free_pages) * PAGE_SIZE);
	used_frac = bytes_to_mb_frac((usable_pages - free_pages) * PAGE_SIZE);
	peak_mb = bytes_to_mb(stats.peak_used_pages * PAGE_SIZE);
	peak_frac = bytes_to_mb_frac(stats.peak_used_pages * PAGE_SIZE);

	bitmap_kb = bytes_to_kb(stats.bitmap_size);
	bitmap_frac = bytes_to_kb_frac(stats.bitmap_size);

	serial_printf(DEBUG_PORT,
	              "\n=== Physical Memory Manager Statistics ===\n");
	serial_printf(DEBUG_PORT,
	              "Total Physical Memory: %llu.%02llu MB\n",
	              total_mb,
	              total_frac);
	serial_printf(DEBUG_PORT,
	              "Usable Memory:         %llu.%02llu MB\n",
	              usable_mb,
	              usable_frac);
	serial_printf(DEBUG_PORT,
	              "Reserved Memory:       %llu.%02llu MB\n",
	              reserved_mb,
	              reserved_frac);
	serial_printf(DEBUG_PORT,
	              "Bootloader Reclaimable: %llu.%02llu MB\n",
	              bootloader_mb,
	              bootloader_frac);
	serial_printf(DEBUG_PORT, "\n");
	serial_printf(
	    DEBUG_PORT, "Total Pages Tracked:   %llu\n", stats.total_pages);
	serial_printf(DEBUG_PORT,
	              "Free Pages:            %llu (%llu.%02llu MB)\n",
	              free_pages,
	              free_mb,
	              free_frac);
	serial_printf(DEBUG_PORT,
	              "Used Pages:            %llu (%llu.%02llu MB)\n",
	              usable_pages - free_pages,
	              used_mb,
	              used_frac);
	serial_printf(DEBUG_PORT,
	              "Peak Used Pages:       %llu (%llu.%02llu MB)\n",
	              stats.peak_used_pages,
	              peak_mb,
	              peak_frac);
	serial_printf(DEBUG_PORT, "\n");
	serial_printf(
	    DEBUG_PORT, "Total Allocations:     %llu\n", stats.alloc_count);
	serial_printf(
	    DEBUG_PORT, "Total Frees:           %llu\n", stats.free_count);
	serial_printf(DEBUG_PORT,
	              "Bitmap Size:           %llu bytes (%llu.%02llu KB)\n",
	              stats.bitmap_size,
	              bitmap_kb,
	              bitmap_frac);
	serial_printf(DEBUG_PORT,
	              "==========================================\n\n");
}

void
pmm_print_memory_map(void)
{
	if (saved_memmap == NULL) {
		serial_printf(DEBUG_PORT, "[PMM] No saved memory map\n");
		return;
	}

	serial_printf(DEBUG_PORT, "\n=== Physical Memory Map ===\n");
	serial_printf(DEBUG_PORT,
	              "%-18s %-18s %-10s %s\n",
	              "Base",
	              "Length",
	              "Size",
	              "Type");
	serial_printf(DEBUG_PORT,
	              "%-18s %-18s %-10s %s\n",
	              "--------",
	              "--------",
	              "----",
	              "----");

	for (uint64_t i = 0; i < saved_memmap->entry_count; i++) {
		struct limine_memmap_entry *entry = saved_memmap->entries[i];

		uint64_t size_mb = bytes_to_mb(entry->length);
		uint64_t size_mb_frac = bytes_to_mb_frac(entry->length);

		serial_printf(DEBUG_PORT,
		              "0x%016llx 0x%016llx %5llu.%02llu MB %s\n",
		              entry->base,
		              entry->length,
		              size_mb,
		              size_mb_frac,
		              get_memmap_type_name(entry->type));
	}

	serial_printf(DEBUG_PORT, "===========================\n\n");
}

bool
pmm_validate(void)
{
	if (bitmap == NULL) {
		serial_printf(DEBUG_PORT,
		              "[PMM] VALIDATION FAILED: NULL bitmap\n");
		return false;
	}

	uint64_t counted_free = 0;
	for (uint64_t i = 0; i < total_pages; i++) {
		if (!bitmap_test(i)) {
			counted_free++;
		}
	}

	if (counted_free != free_pages) {
		serial_printf(DEBUG_PORT,
		              "[PMM] VALIDATION FAILED: free_pages=%llu but "
		              "counted %llu\n",
		              free_pages,
		              counted_free);
		return false;
	}

	serial_printf(DEBUG_PORT, "[PMM] Validation passed\n");
	return true;
}