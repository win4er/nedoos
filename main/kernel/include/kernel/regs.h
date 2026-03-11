#ifndef KERNEL_REGS_H
#define KERNEL_REGS_H

#include <stdint.h>

struct regs {
    uint32_t ds;                  // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
    uint32_t int_no, err_code;    // Interrupt number and error code
    uint32_t eip, cs, eflags, useresp, ss; // Pushed by processor
};

#endif
