#include <stdint.h>
#include <stdio.h>
#include <kernel/debug.h>
#include <kernel/io.h>
#include <kernel/regs.h>

// Array of function pointers for IRQ handlers
static void (*irq_handlers[16])(struct regs *) = {0};

// Called from isr_common_stub in isr.S
void isr_handler(struct regs *r) {
    if (r->int_no < 32) {
        printf("\n*** EXCEPTION: ");
        switch(r->int_no) {
            case 0:  printf("Division by Zero"); break;
            case 1:  printf("Debug"); break;
            case 2:  printf("Non-Maskable Interrupt"); break;
            case 3:  printf("Breakpoint"); break;
            case 4:  printf("Overflow"); break;
            case 5:  printf("Bound Range Exceeded"); break;
            case 6:  printf("Invalid Opcode"); break;
            case 7:  printf("Device Not Available"); break;
            case 8:  printf("Double Fault"); break;
            case 9:  printf("Coprocessor Segment Overrun"); break;
            case 10: printf("Invalid TSS"); break;
            case 11: printf("Segment Not Present"); break;
            case 12: printf("Stack-Segment Fault"); break;
            case 13: printf("General Protection Fault"); break;
            case 14: printf("Page Fault"); break;
            case 15: printf("Reserved"); break;
            case 16: printf("x87 Floating-Point Exception"); break;
            case 17: printf("Alignment Check"); break;
            case 18: printf("Machine Check"); break;
            case 19: printf("SIMD Floating-Point Exception"); break;
            case 20: printf("Virtualization Exception"); break;
            default: printf("Unknown Exception"); break;
        }
        printf(" (Interrupt: %d", r->int_no);
        if (r->err_code) {
            printf(", Error Code: "); debug_puthex(r->err_code);
        }
        printf(")\n");
        
        debug_dump_regs();
        printf("\nKERNEL PANIC: Unhandled exception\n");
        __asm__ volatile("cli; hlt");
        for(;;);
    }
}

// Called from irq_common_stub in isr.S
void irq_handler(struct regs *r) {
    // Send EOI to PIC if this is an IRQ (0-15)
    if (r->int_no >= 32 && r->int_no <= 47) {
        int irq = r->int_no - 32;
        
        // Call the handler if it exists
        if (irq_handlers[irq]) {
            irq_handlers[irq](r);
        }
        
        // Send EOI to PICs
        if (irq >= 8) {
            outb(0xA0, 0x20);  // Send EOI to slave
        }
        outb(0x20, 0x20);      // Send EOI to master
    }
}

// Register an IRQ handler
static void (*irq_handlers[16])(struct regs*);

void irq_register_handler(int irq, void (*handler)(struct regs*)) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = handler;
    }
}
// Unregister an IRQ handler
void irq_unregister_handler(int irq) {
    if (irq >= 0 && irq < 16) {
        irq_handlers[irq] = 0;
    }
}
