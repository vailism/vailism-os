#include "../include/heap.h"
#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/serial.h"

#define HEAP_MAGIC 0xCAFEBABE

struct heap_block {
    uint32_t magic;
    uint32_t is_free;
    size_t   size;
    struct heap_block *next;
    struct heap_block *prev;
};

static struct heap_block *g_heap_head = NULL;
static uint64_t g_heap_top = KERNEL_HEAP_START;

static void heap_expand(size_t pages) {
    pagetable_t *pml4 = vmm_get_kernel_pml4();

    for (size_t i = 0; i < pages; i++) {
        void *phys = pmm_alloc_page();
        if (!phys) {
            serial_puts("[HEAP ERROR] Out of physical memory to expand heap!\n");
            return;
        }
        vmm_map_page(pml4, g_heap_top, (uint64_t)phys, PTE_PRESENT | PTE_WRITABLE);

        struct heap_block *new_block = (struct heap_block *)g_heap_top;
        new_block->magic = HEAP_MAGIC;
        new_block->is_free = 1;
        new_block->size = PAGE_SIZE - sizeof(struct heap_block);
        new_block->next = NULL;
        new_block->prev = NULL;

        if (!g_heap_head) {
            g_heap_head = new_block;
        } else {
            // Find tail
            struct heap_block *curr = g_heap_head;
            while (curr->next) {
                curr = curr->next;
            }
            curr->next = new_block;
            new_block->prev = curr;

            // Merge if previous was free and adjacent
            if (curr->is_free && ((uint64_t)curr + sizeof(struct heap_block) + curr->size == (uint64_t)new_block)) {
                curr->size += sizeof(struct heap_block) + new_block->size;
                curr->next = NULL;
            }
        }

        g_heap_top += PAGE_SIZE;
    }
}

void heap_init(void) {
    heap_expand(KERNEL_HEAP_INITIAL_PAGES);
    serial_puts("[HEAP] Kernel heap initialized with 64 KiB dynamic memory pool.\n");
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 16-byte boundary
    size = ALIGN_UP(size, 16);

    struct heap_block *curr = g_heap_head;
    while (curr) {
        if (curr->magic != HEAP_MAGIC) {
            serial_puts("[HEAP CORRUPTION] Invalid block magic detected in kmalloc!\n");
            return NULL;
        }

        if (curr->is_free && curr->size >= size) {
            // Check if we can split this block
            if (curr->size >= size + sizeof(struct heap_block) + 16) {
                struct heap_block *split = (struct heap_block *)((uint64_t)curr + sizeof(struct heap_block) + size);
                split->magic = HEAP_MAGIC;
                split->is_free = 1;
                split->size = curr->size - size - sizeof(struct heap_block);
                split->next = curr->next;
                split->prev = curr;

                if (curr->next) {
                    curr->next->prev = split;
                }
                curr->next = split;
                curr->size = size;
            }

            curr->is_free = 0;
            return (void *)(curr + 1);
        }
        curr = curr->next;
    }

    // Need more memory: calculate required pages and expand
    size_t needed_pages = ALIGN_UP(size + sizeof(struct heap_block), PAGE_SIZE) / PAGE_SIZE;
    heap_expand(needed_pages > 4 ? needed_pages : 4);

    // Try again after expanding
    return kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) return;

    struct heap_block *block = ((struct heap_block *)ptr) - 1;
    if (block->magic != HEAP_MAGIC) {
        serial_puts("[HEAP CORRUPTION] Invalid magic on kfree!\n");
        return;
    }

    block->is_free = 1;

    // Coalesce with next block if free
    if (block->next && block->next->is_free &&
        ((uint64_t)block + sizeof(struct heap_block) + block->size == (uint64_t)block->next)) {
        block->size += sizeof(struct heap_block) + block->next->size;
        block->next = block->next->next;
        if (block->next) {
            block->next->prev = block;
        }
    }

    // Coalesce with previous block if free
    if (block->prev && block->prev->is_free &&
        ((uint64_t)block->prev + sizeof(struct heap_block) + block->prev->size == (uint64_t)block)) {
        block->prev->size += sizeof(struct heap_block) + block->size;
        block->prev->next = block->next;
        if (block->next) {
            block->next->prev = block->prev;
        }
    }
}

void *kcalloc(size_t num, size_t size) {
    size_t total = num * size;
    void *ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *krealloc(void *ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }

    struct heap_block *block = ((struct heap_block *)ptr) - 1;
    if (block->magic != HEAP_MAGIC) return NULL;

    if (block->size >= size) return ptr;

    void *new_ptr = kmalloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        kfree(ptr);
    }
    return new_ptr;
}
