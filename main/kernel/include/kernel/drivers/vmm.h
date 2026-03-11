#ifndef MM_VMM_H
#define MM_VMM_H

#include <stdint.h>

void vmm_init(void);
void vmm_enable(void);
void* vmm_map_page(void* phys_addr, void* virt_addr, uint32_t flags);

#endif
