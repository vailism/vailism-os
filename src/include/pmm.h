#ifndef PMM_H
#define PMM_H

#include "types.h"
#include "limine.h"

/**
 * Initialize the Physical Memory Manager (PMM) using the memory map and HHDM offset from Limine.
 */
void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset);

/**
 * Allocate a single 4 KiB physical page frame.
 * Returns the physical address of the allocated page, or NULL if out of memory.
 */
void *pmm_alloc_page(void);

/**
 * Free a single 4 KiB physical page frame.
 */
void pmm_free_page(void *paddr);

/**
 * Allocate 'count' contiguous 4 KiB physical page frames.
 */
void *pmm_alloc_pages(size_t count);

/**
 * Free 'count' contiguous 4 KiB physical page frames.
 */
void pmm_free_pages(void *paddr, size_t count);

uint64_t pmm_get_total_memory(void);
uint64_t pmm_get_used_memory(void);
uint64_t pmm_get_free_memory(void);

#endif // PMM_H
