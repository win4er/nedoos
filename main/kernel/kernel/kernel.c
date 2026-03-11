#include <stdio.h>
#include <kernel/tty.h>
#include <kernel/debug.h>
#include <kernel/io.h>
#include <kernel/regs.h>
#include <drivers/keyboard.h>    // Добавить для keyboard_getchar()
#include <drivers/timer.h>        // Добавить для timer_get_ticks()
#include <mm/pmm.h>               // Добавить для pmm_*()
#include <mm/vmm.h>               // Добавить для vmm_*()

// External declarations
extern void gdt_install(void);
extern void idt_install(void);
extern void pic_remap(int offset1, int offset2);
extern void keyboard_install(void);
extern void timer_install(uint32_t frequency);
extern void pmm_init(uint32_t memory_size);
extern void vmm_init(void);
extern void vmm_enable(void);
extern void scheduler_init(void);
extern int scheduler_create_task(void (*entry)(void));

// Test task functions
void test_task1(void) {
    int counter = 0;
    while (1) {
        printf("Task 1: %d\n", counter++);
        for (volatile int i = 0; i < 1000000; i++); // Simple delay
    }
}

void test_task2(void) {
    int counter = 100;
    while (1) {
        printf("Task 2: %d\n", counter++);
        for (volatile int i = 0; i < 1000000; i++); // Simple delay
    }
}

void kernel_main(void) {
    // Initialize terminal first so we can see output
    terminal_initialize();
    printf("\033[1;36m");  // Cyan color
    printf("╔══════════════════════════════════════╗\n");
    printf("║     NedoOS Kernel v0.0.2             ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("\033[0m");  // Reset color
    printf("\n");
    
    // Install GDT
    gdt_install();
    printf("\n");
    
    // Remap PIC before installing IDT
    pic_remap(32, 40);
    printf("\n");
    
    // Install IDT
    idt_install();
    printf("\n");
    
    // Initialize memory managers
    pmm_init(128 * 1024 * 1024);  // Assume 128MB for testing
    printf("\n");
    
    vmm_init();
    printf("\n");
    
    // Install timer (100Hz)
    timer_install(100);
    printf("\n");
    
    // Install keyboard
    keyboard_install();
    printf("\n");
    
    // Enable interrupts
    __asm__ volatile("sti");
    
    printf("\n\033[1;32mSystem ready!\033[0m\n");
    printf("Commands:\n");
    printf("  t - Create test tasks\n");
    printf("  m - Test memory allocation\n");
    printf("  p - Enable paging\n");
    printf("  q - Quit\n\n");
    
    // Main command loop
    while (1) {
        printf("\n> ");
        char c = keyboard_getchar();
        putchar(c);
        printf("\n");
        
        switch (c) {
            case 't': {
                printf("Creating test tasks...\n");
                // scheduler_create_task(test_task1);
                // scheduler_create_task(test_task2);
                break;
            }
            
            case 'm': {
                printf("Testing PMM: ");
                void* page = pmm_alloc_page();
                if (page) {
                    printf("Allocated page at ");
                    debug_puthex((uint32_t)page);
                    printf("\n");
                    pmm_free_page(page);
                    printf("Page freed\n");
                } else {
                    printf("Allocation failed!\n");
                }
                break;
            }
            
            case 'p': {
                printf("Enabling paging...\n");
                vmm_enable();
                printf("Paging enabled - VGA text mode should still work\n");
                break;
            }
            
            case 'q': {
                printf("\nShutting down...\n");
                __asm__ volatile("cli; hlt");
                break;
            }
            
            default:
                printf("Unknown command: %c\n", c);
                break;
        }
    }
}
