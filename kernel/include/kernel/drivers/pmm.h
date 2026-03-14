#ifndef MM_PMM_H
#define MM_PMM_H

#include <stdint.h>

void pmm_init(uint32_t memory_size);
void* pmm_alloc_page(void);
void pmm_free_page(void* addr);

#endif
