#ifndef VMM_H
#define VMM_H

#include "types.h"

// Page Table Entry (PTE) Attribute Flags
#define PTE_PRESENT       (1ULL << 0)
#define PTE_WRITABLE      (1ULL << 1)
#define PTE_USER          (1ULL << 2)
#define PTE_WRITE_THROUGH (1ULL << 3)
#define PTE_CACHE_DISABLE (1ULL << 4)
#define PTE_ACCESSED      (1ULL << 5)
#define PTE_DIRTY         (1ULL << 6)
#define PTE_HUGE_PAGE     (1ULL << 7)
#define PTE_GLOBAL        (1ULL << 8)
#define PTE_NX            (1ULL << 63)

#define PTE_FRAME_MASK    0x000FFFFFFFFFF000ULL

typedef uint64_t pagetable_t;

/**
 * Initialize Virtual Memory Manager and construct kernel page table.
 */
void vmm_init(void);

/**
 * Map a 4 KiB virtual page to a physical frame in the given PML4.
 */
void vmm_map_page(pagetable_t *pml4, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

/**
 * Unmap a 4 KiB virtual page from the given PML4.
 */
void vmm_unmap_page(pagetable_t *pml4, uint64_t virt_addr);

/**
 * Switch active PML4 by loading the physical address into the CR3 register.
 */
void vmm_switch_pml4(pagetable_t *pml4);

/**
 * Invalidate a single virtual address in the CPU TLB (Translation Lookaside Buffer).
 */
static inline void vmm_invlpg(uint64_t virt_addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

/**
 * Get the kernel's master PML4 table.
 */
pagetable_t *vmm_get_kernel_pml4(void);

#endif // VMM_H
