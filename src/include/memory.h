#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

#define PAGE_SIZE 4096ULL

#define ALIGN_UP(addr, align)   (((uint64_t)(addr) + ((uint64_t)(align) - 1)) & ~((uint64_t)(align) - 1))
#define ALIGN_DOWN(addr, align) ((uint64_t)(addr) & ~((uint64_t)(align) - 1))

extern uint64_t g_hhdm_offset;

#define PHYS_TO_VIRT(paddr) ((void *)((uint64_t)(paddr) + g_hhdm_offset))
#define VIRT_TO_PHYS(vaddr) ((void *)((uint64_t)(vaddr) - g_hhdm_offset))

#endif // MEMORY_H
