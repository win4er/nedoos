#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <kernel/debug.h>

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t always0;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;

// External assembly functions
extern void idt_load(uint32_t);
extern void isr0(void);  // Division by zero
extern void isr1(void);  // Debug
extern void isr2(void);  // NMI
extern void isr3(void);  // Breakpoint
extern void isr4(void);  // Overflow
extern void isr5(void);  // Bound range
extern void isr6(void);  // Invalid opcode
extern void isr7(void);  // Device not available
extern void isr8(void);  // Double fault
extern void isr9(void);  // Coprocessor segment overrun
extern void isr10(void); // Invalid TSS
extern void isr11(void); // Segment not present
extern void isr12(void); // Stack segment fault
extern void isr13(void); // General protection fault
extern void isr14(void); // Page fault
extern void isr15(void); // Reserved
extern void isr16(void); // Floating point
extern void isr17(void); // Alignment check
extern void isr18(void); // Machine check
extern void isr19(void); // SIMD floating point
extern void isr20(void); // Virtualization
extern void isr21(void); // Reserved
extern void isr22(void); // Reserved
extern void isr23(void); // Reserved
extern void isr24(void); // Reserved
extern void isr25(void); // Reserved
extern void isr26(void); // Reserved
extern void isr27(void); // Reserved
extern void isr28(void); // Reserved
extern void isr29(void); // Reserved
extern void isr30(void); // Reserved
extern void isr31(void); // Reserved

// IRQs
extern void irq0(void);  // PIT
extern void irq1(void);  // Keyboard
extern void irq2(void);  // Cascade
extern void irq3(void);  // COM2
extern void irq4(void);  // COM1
extern void irq5(void);  // LPT2
extern void irq6(void);  // Floppy
extern void irq7(void);  // LPT1
extern void irq8(void);  // RTC
extern void irq9(void);  // General
extern void irq10(void); // General
extern void irq11(void); // General
extern void irq12(void); // Mouse
extern void irq13(void); // FPU
extern void irq14(void); // Primary ATA
extern void irq15(void); // Secondary ATA

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
}

void idt_install(void) {
    printf("Installing IDT...\n");
    
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (uint32_t)&idt;
    
    memset(&idt, 0, sizeof(struct idt_entry) * 256);
    
    // ISRs 0-31
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
    
    // IRQs 32-47
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
    
    idt_load((uint32_t)&idtp);
    printf("IDT installed successfully\n");
}
