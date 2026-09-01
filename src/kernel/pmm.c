#include "../include/pmm.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/serial.h"

uint64_t g_hhdm_offset = 0;

static uint8_t *g_bitmap = NULL;
static uint64_t g_bitmap_size = 0;
static uint64_t g_total_pages = 0;
static uint64_t g_free_pages = 0;
static uint64_t g_used_pages = 0;
static uint64_t g_highest_address = 0;
static uint64_t g_last_index = 0;

static inline void bitmap_set(uint64_t bit) {
    g_bitmap[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

static inline void bitmap_clear(uint64_t bit) {
    g_bitmap[bit / 8] &= (uint8_t)~(1 << (bit % 8));
}

static inline bool bitmap_test(uint64_t bit) {
    return (g_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset) {
    g_hhdm_offset = hhdm_offset;

    if (!memmap) {
        serial_puts("[PMM ERROR] No memory map response provided by bootloader!\n");
        return;
    }

    // 1. Calculate highest usable physical address
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t top = entry->base + entry->length;
            if (top > g_highest_address) {
                g_highest_address = top;
            }
        }
    }

    g_total_pages = g_highest_address / PAGE_SIZE;
    g_bitmap_size = ALIGN_UP(g_total_pages / 8, PAGE_SIZE);

    // 2. Find a usable memory block big enough to store the bitmap itself
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= g_bitmap_size) {
            g_bitmap = (uint8_t *)PHYS_TO_VIRT(entry->base);
            memset(g_bitmap, 0xFF, g_bitmap_size); // Default all to 1 (used)
            break;
        }
    }

    if (!g_bitmap) {
        serial_puts("[PMM ERROR] Could not find sufficient physical memory for bitmap!\n");
        return;
    }

    // 3. Mark usable regions as 0 (free) in the bitmap
    g_free_pages = 0;
    g_used_pages = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_page = entry->base / PAGE_SIZE;
            uint64_t count = entry->length / PAGE_SIZE;
            for (uint64_t p = 0; p < count; p++) {
                if (start_page + p < g_total_pages) {
                    bitmap_clear(start_page + p);
                    g_free_pages++;
                }
            }
        }
    }

    // 4. Mark the bitmap's own physical memory as 1 (used)
    uint64_t bitmap_phys = (uint64_t)VIRT_TO_PHYS(g_bitmap);
    uint64_t bitmap_pages = g_bitmap_size / PAGE_SIZE;
    for (uint64_t p = 0; p < bitmap_pages; p++) {
        bitmap_set((bitmap_phys / PAGE_SIZE) + p);
        if (g_free_pages > 0) {
            g_free_pages--;
        }
        g_used_pages++;
    }

    // 5. Reserve first 1MB (0x0 - 0x100000) for legacy BIOS/real-mode/SMP safety
    for (uint64_t p = 0; p < 256; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            if (g_free_pages > 0) g_free_pages--;
            g_used_pages++;
        }
    }

    g_used_pages = g_total_pages - g_free_pages;
    serial_puts("[PMM] Physical Memory Manager initialized successfully.\n");
}

void *pmm_alloc_page(void) {
    for (uint64_t i = 0; i < g_total_pages; i++) {
        uint64_t index = (g_last_index + i) % g_total_pages;
        if (!bitmap_test(index)) {
            bitmap_set(index);
            g_free_pages--;
            g_used_pages++;
            g_last_index = index + 1;
            return (void *)(index * PAGE_SIZE);
        }
    }
    serial_puts("[PMM ERROR] Out of physical memory!\n");
    return NULL;
}

void pmm_free_page(void *paddr) {
    if (!paddr) return;
    uint64_t page = (uint64_t)paddr / PAGE_SIZE;
    if (page >= g_total_pages) return;

    if (bitmap_test(page)) {
        bitmap_clear(page);
        g_free_pages++;
        g_used_pages--;
    }
}

void *pmm_alloc_pages(size_t count) {
    if (count == 0) return NULL;
    if (count == 1) return pmm_alloc_page();

    uint64_t consecutive = 0;
    uint64_t start_index = 0;

    for (uint64_t i = 0; i < g_total_pages; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) start_index = i;
            consecutive++;
            if (consecutive == count) {
                for (uint64_t j = 0; j < count; j++) {
                    bitmap_set(start_index + j);
                }
                g_free_pages -= count;
                g_used_pages += count;
                return (void *)(start_index * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    return NULL;
}

void pmm_free_pages(void *paddr, size_t count) {
    if (!paddr) return;
    uint64_t start_page = (uint64_t)paddr / PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        pmm_free_page((void *)((start_page + i) * PAGE_SIZE));
    }
}

uint64_t pmm_get_total_memory(void) {
    return g_total_pages * PAGE_SIZE;
}

uint64_t pmm_get_used_memory(void) {
    return g_used_pages * PAGE_SIZE;
}

uint64_t pmm_get_free_memory(void) {
    return g_free_pages * PAGE_SIZE;
}
