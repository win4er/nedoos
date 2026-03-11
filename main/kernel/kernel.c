#include <stdio.h>
#include <kernel/tty.h>
#include <kernel/debug.h>
#include <kernel/io.h>
#include <kernel/regs.h>
#include "drivers/keyboard.h"
#include "drivers/timer.h"
#include "mm/pmm.h"
#include "mm/vmm.h"

// Простая функция задержки
static void delay(void) {
    for (volatile int i = 0; i < 1000000; i++);
}

extern void gdt_install(void);
extern void idt_install(void);
extern void pic_remap(int offset1, int offset2);
extern void keyboard_install(void);
extern void timer_install(uint32_t frequency);

void kernel_main(void) {
    // Небольшая задержка перед инициализацией
    delay();
    
    // Очищаем экран
    terminal_initialize();
    
    // Теперь выводим логотип
    printf("\033[1;36m");  // Cyan color
    printf("╔══════════════════════════════════════╗\n");
    printf("║     NedoOS Kernel v0.0.2             ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("\033[0m");  // Reset color
    printf("\n");
    
    // Install GDT
    printf("Installing GDT...\n");
    gdt_install();
    printf("GDT installed successfully\n\n");
    
    // Remap PIC
    printf("Remapping PIC...\n");
    pic_remap(32, 40);
    printf("PIC remapped successfully\n\n");
    
    // Install IDT
    printf("Installing IDT...\n");
    idt_install();
    printf("IDT installed successfully\n\n");
    
    // Initialize memory managers
    printf("Initializing Physical Memory Manager...\n");
    pmm_init(128 * 1024 * 1024);  // 128MB for testing
    printf("PMM initialized: 128 MB total\n\n");
    
    printf("Initializing Virtual Memory Manager...\n");
    vmm_init();
    printf("VMM initialized successfully\n\n");
    
    // Install timer
    printf("Installing PIT timer...\n");
    timer_install(100);
    printf("Timer installed (100Hz)\n\n");
    
    // Install keyboard
    printf("Installing keyboard driver...\n");
    keyboard_install();
    printf("Keyboard driver installed\n\n");
    
    // Enable interrupts
    printf("Enabling interrupts...\n");
    __asm__ volatile("sti");
    printf("Interrupts enabled\n");
    
    printf("\n\033[1;32mSystem ready!\033[0m\n");
    printf("Type 'help' for commands\n\n");
    
    // Main loop
    while (1) {
        printf("> ");
        char c = keyboard_getchar();
        printf("%c\n", c);
    }
}
