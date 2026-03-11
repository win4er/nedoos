#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <kernel/debug.h>
#include "pmm.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

// x86 paging structures
typedef struct page_directory_entry {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t write_through : 1;
    uint32_t cache_disable : 1;
    uint32_t accessed   : 1;
    uint32_t reserved   : 1;
    uint32_t page_size  : 1;
    uint32_t global     : 1;
    uint32_t available  : 3;
    uint32_t frame      : 20;
} __attribute__((packed)) page_directory_entry_t;

typedef struct page_table_entry {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t write_through : 1;
    uint32_t cache_disable : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t reserved   : 1;
    uint32_t global     : 1;
    uint32_t available  : 3;
    uint32_t frame      : 20;
} __attribute__((packed)) page_table_entry_t;

#define PAGE_DIRECTORY_INDEX(vaddr) ((uint32_t)vaddr >> 22)
#define PAGE_TABLE_INDEX(vaddr) (((uint32_t)vaddr >> 12) & 0x3FF)

static page_directory_entry_t* current_directory = NULL;

void vmm_init(void) {
    printf("Initializing Virtual Memory Manager...\n");
    
    // Allocate page directory
    current_directory = (page_directory_entry_t*)pmm_alloc_page();
    memset(current_directory, 0, PAGE_SIZE);
    
    // Identity map first 4MB for kernel
    page_directory_entry_t* dir = current_directory;
    page_table_entry_t* table = (page_table_entry_t*)pmm_alloc_page();
    memset(table, 0, PAGE_SIZE);
    
    // Map first 4MB (1024 pages)
    for (uint32_t i = 0; i < 1024; i++) {
        table[i].frame = i;
        table[i].present = 1;
        table[i].rw = 1;
        table[i].user = 0;
    }
    
    dir[0].frame = (uint32_t)table >> 12;
    dir[0].present = 1;
    dir[0].rw = 1;
    dir[0].user = 0;
    dir[0].page_size = 0;
    
    printf("VMM initialized\n");
}

void vmm_enable(void) {
    printf("Enabling paging...\n");
    
    // Load page directory
    __asm__ volatile("mov %0, %%cr3" : : "r" (current_directory));
    
    // Enable paging
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r" (cr0));
    cr0 |= 0x80000000; // Set PG bit
    __asm__ volatile("mov %0, %%cr0" : : "r" (cr0));
    
    printf("Paging enabled\n");
}

void* vmm_map_page(void* phys_addr, void* virt_addr, uint32_t flags) {
    uint32_t pd_idx = PAGE_DIRECTORY_INDEX(virt_addr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(virt_addr);
    
    // Check if page table exists
    if (!current_directory[pd_idx].present) {
        // Allocate new page table
        page_table_entry_t* table = (page_table_entry_t*)pmm_alloc_page();
        memset(table, 0, PAGE_SIZE);
        
        current_directory[pd_idx].frame = (uint32_t)table >> 12;
        current_directory[pd_idx].present = 1;
        current_directory[pd_idx].rw = 1;
        current_directory[pd_idx].user = (flags & 2) ? 1 : 0;
    }
    
    // Get page table
    page_table_entry_t* table = (page_table_entry_t*)(current_directory[pd_idx].frame << 12);
    
    // Map page
    table[pt_idx].frame = (uint32_t)phys_addr >> 12;
    table[pt_idx].present = 1;
    table[pt_idx].rw = flags & 1;
    table[pt_idx].user = (flags & 2) ? 1 : 0;
    
    // Flush TLB
    __asm__ volatile("invlpg (%0)" : : "r" (virt_addr) : "memory");
    
    return virt_addr;
}
