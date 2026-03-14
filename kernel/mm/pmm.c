#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <kernel/debug.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
#define PAGE_COUNT 1048576  // 4GB / 4KB = 1M pages
#define BITMAP_SIZE (PAGE_COUNT / 8)

static uint8_t memory_bitmap[BITMAP_SIZE];
static uint32_t last_allocated = 0;

static inline void bitmap_set(size_t bit) {
    memory_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(size_t bit) {
    memory_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bitmap_test(size_t bit) {
    return (memory_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

void pmm_init(uint32_t memory_size) {
    printf("Initializing Physical Memory Manager...\n");
    
    // Mark all memory as used initially
    memset(memory_bitmap, 0xFF, BITMAP_SIZE);
    
    // Mark first 1MB as used (kernel + BIOS)
    for (uint32_t i = 0; i < (1024 * 1024) / PAGE_SIZE; i++) {
        bitmap_set(i);
    }
    
    // Mark available memory from multiboot info
    uint32_t total_pages = memory_size / PAGE_SIZE;
    for (uint32_t i = (1024 * 1024) / PAGE_SIZE; i < total_pages && i < PAGE_COUNT; i++) {
        bitmap_clear(i);
    }
    
    printf("PMM initialized: %d MB total\n", memory_size / (1024 * 1024));
}

void* pmm_alloc_page(void) {
    // Simple first-fit allocation
    for (uint32_t i = last_allocated; i < PAGE_COUNT; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            last_allocated = i + 1;
            return (void*)(i * PAGE_SIZE);
        }
    }
    
    // Wrap around
    for (uint32_t i = 0; i < last_allocated; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            last_allocated = i + 1;
            return (void*)(i * PAGE_SIZE);
        }
    }
    
    printf("PMM: Out of memory!\n");
    return NULL;
}

void pmm_free_page(void* addr) {
    uint32_t page = (uint32_t)addr / PAGE_SIZE;
    if (page < PAGE_COUNT) {
        bitmap_clear(page);
    }
}
