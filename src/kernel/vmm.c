#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/memory.h"
#include "../include/string.h"
#include "../include/serial.h"

static pagetable_t *g_kernel_pml4 = NULL;

static pagetable_t *get_next_level(pagetable_t *current_table, uint64_t index, bool allocate, uint64_t flags) {
    if (current_table[index] & PTE_PRESENT) {
        uint64_t phys = current_table[index] & PTE_FRAME_MASK;
        return (pagetable_t *)PHYS_TO_VIRT(phys);
    }

    if (!allocate) {
        return NULL;
    }

    // Allocate a new physical page for the next level table
    void *new_page_phys = pmm_alloc_page();
    if (!new_page_phys) {
        serial_puts("[VMM ERROR] Failed to allocate page table frame!\n");
        return NULL;
    }

    pagetable_t *new_table_virt = (pagetable_t *)PHYS_TO_VIRT(new_page_phys);
    memset(new_table_virt, 0, PAGE_SIZE);

    current_table[index] = (uint64_t)new_page_phys | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    return new_table_virt;
}

void vmm_map_page(pagetable_t *pml4, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    uint64_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    pagetable_t *pdpt = get_next_level(pml4, pml4_idx, true, flags);
    if (!pdpt) return;

    pagetable_t *pd   = get_next_level(pdpt, pdpt_idx, true, flags);
    if (!pd) return;

    pagetable_t *pt   = get_next_level(pd, pd_idx, true, flags);
    if (!pt) return;

    pt[pt_idx] = (phys_addr & PTE_FRAME_MASK) | flags | PTE_PRESENT;
    vmm_invlpg(virt_addr);
}

void vmm_unmap_page(pagetable_t *pml4, uint64_t virt_addr) {
    uint64_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    pagetable_t *pdpt = get_next_level(pml4, pml4_idx, false, 0);
    if (!pdpt) return;

    pagetable_t *pd   = get_next_level(pdpt, pdpt_idx, false, 0);
    if (!pd) return;

    pagetable_t *pt   = get_next_level(pd, pd_idx, false, 0);
    if (!pt) return;

    pt[pt_idx] = 0;
    vmm_invlpg(virt_addr);
}

void vmm_switch_pml4(pagetable_t *pml4) {
    uint64_t phys = (uint64_t)VIRT_TO_PHYS(pml4);
    __asm__ volatile ("mov %0, %%cr3" : : "r"(phys) : "memory");
}

pagetable_t *vmm_get_kernel_pml4(void) {
    return g_kernel_pml4;
}

void vmm_init(void) {
    // Read active CR3 loaded by Limine
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));

    g_kernel_pml4 = (pagetable_t *)PHYS_TO_VIRT(cr3 & PTE_FRAME_MASK);
    serial_puts("[VMM] Virtual Memory Manager initialized with 4-level paging active.\n");
}
