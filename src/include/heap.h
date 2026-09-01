#ifndef HEAP_H
#define HEAP_H

#include "types.h"

#define KERNEL_HEAP_START 0xffffffffc0000000ULL
#define KERNEL_HEAP_INITIAL_PAGES 16 // 64 KiB initial heap

/**
 * Initialize kernel heap allocator and map initial memory pages.
 */
void heap_init(void);

/**
 * Allocate dynamic kernel memory.
 */
void *kmalloc(size_t size);

/**
 * Free previously allocated kernel memory.
 */
void kfree(void *ptr);

/**
 * Allocate and zero-initialize memory for an array of elements.
 */
void *kcalloc(size_t num, size_t size);

/**
 * Reallocate memory block with a new size.
 */
void *krealloc(void *ptr, size_t size);

#endif // HEAP_H
