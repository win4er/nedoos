#include <kernel/io.h>
#include <kernel/regs.h>
#include <stdio.h>

#define PIT_COMMAND 0x43
#define PIT_DATA 0x40

volatile uint32_t timer_ticks = 0;

static void timer_handler(struct regs *r) {
    (void)r;
    timer_ticks++;
}

void timer_install(int frequency) {
    extern void irq_register_handler(int, void(*)(struct regs*));
    irq_register_handler(0, timer_handler);
    
    uint32_t divisor = 1193180 / frequency;
    outb(PIT_COMMAND, 0x36);
    outb(PIT_DATA, divisor & 0xFF);
    outb(PIT_DATA, (divisor >> 8) & 0xFF);
}
