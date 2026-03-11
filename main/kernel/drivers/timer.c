#include <stdint.h>
#include <stdio.h>
#include <kernel/io.h>
#include <kernel/regs.h>  // Добавить этот заголовок
#include "timer.h"

#define PIT_COMMAND 0x43
#define PIT_DATA    0x40

static volatile uint32_t timer_ticks = 0;

static void timer_handler(struct regs *r) {
    (void)r;
    timer_ticks++;
}

// Объявление внешней функции регистрации обработчиков
extern void irq_register_handler(int irq, void (*handler)(struct regs *));

void timer_install(uint32_t frequency) {
    printf("Installing PIT timer with frequency %d Hz...\n", frequency);
    
    // Register IRQ handler (IRQ0 = PIT)
    irq_register_handler(0, timer_handler);
    
    // Calculate divisor
    uint32_t divisor = 1193180 / frequency;
    
    // Send command byte
    outb(PIT_COMMAND, 0x36);
    
    // Send divisor (low byte, then high byte)
    outb(PIT_DATA, divisor & 0xFF);
    outb(PIT_DATA, (divisor >> 8) & 0xFF);
    
    printf("Timer installed\n");
}

void timer_wait(uint32_t ticks) {
    uint32_t target = timer_ticks + ticks;
    while (timer_ticks < target) {
        asm volatile("hlt");
    }
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}
