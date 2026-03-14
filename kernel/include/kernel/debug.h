#ifndef KERNEL_DEBUG_H
#define KERNEL_DEBUG_H 1

#include <stdint.h>

void debug_puthex(uint32_t n);
void debug_puthex32(uint32_t n);
void debug_puthex16(uint16_t n);
void debug_puthex8(uint8_t n);
void debug_dump_regs(void);

#define KERNEL_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("\nKERNEL PANIC: %s\n", msg); \
            printf("File: %s, Line: %d\n", __FILE__, __LINE__); \
            debug_dump_regs(); \
            __asm__ volatile("cli; hlt"); \
            for(;;); \
        } \
    } while(0)

#endif
