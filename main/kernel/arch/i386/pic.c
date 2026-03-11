#include <stdint.h>
#include <stdio.h>
#include <kernel/io.h>  // Это уже содержит outb/inb

// УДАЛИТЕ эти строки (примерно 14-28):
// static inline void outb(uint16_t port, uint8_t val) {
//     asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
// }
// 
// static inline uint8_t inb(uint16_t port) {
//     uint8_t ret;
//     asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
//     return ret;
// }

// PIC ports
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

// ICW commands
#define ICW1_INIT     0x10
#define ICW1_ICW4     0x01
#define ICW4_8086     0x01

void pic_remap(int offset1, int offset2) {
    printf("Remapping PIC...\n");
    
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);
    
    // Start initialization
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    
    // Send offsets
    outb(PIC1_DATA, offset1);
    outb(PIC2_DATA, offset2);
    
    // Send cascade info
    outb(PIC1_DATA, 4);  // Tell master there's a slave at IRQ2
    outb(PIC2_DATA, 2);  // Tell slave its cascade identity
    
    // Request 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
    
    // Restore masks
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
    
    printf("PIC remapped: Master offset %d, Slave offset %d\n", offset1, offset2);
}

void pic_disable(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}
