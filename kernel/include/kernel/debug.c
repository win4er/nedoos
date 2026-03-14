#include <kernel/debug.h>
#include <stdio.h>

static const char hex_chars[] = "0123456789ABCDEF";

void debug_puthex32(uint32_t n) {
    putchar('0');
    putchar('x');
    for (int i = 7; i >= 0; i--) {
        putchar(hex_chars[(n >> (i * 4)) & 0xF]);
    }
}

void debug_puthex16(uint16_t n) {
    putchar('0');
    putchar('x');
    for (int i = 3; i >= 0; i--) {
        putchar(hex_chars[(n >> (i * 4)) & 0xF]);
    }
}

void debug_puthex8(uint8_t n) {
    putchar('0');
    putchar('x');
    putchar(hex_chars[(n >> 4) & 0xF]);
    putchar(hex_chars[n & 0xF]);
}

void debug_puthex(uint32_t n) {
    debug_puthex32(n);
}

void debug_dump_regs(void) {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags;
    
    asm volatile(
        "movl %%eax, %0\n"
        "movl %%ebx, %1\n"
        "movl %%ecx, %2\n"
        "movl %%edx, %3\n"
        "movl %%esi, %4\n"
        "movl %%edi, %5\n"
        "movl %%ebp, %6\n"
        "movl %%esp, %7\n"
        "pushfl\n"
        "popl %8\n"
        "movl 4(%%ebp), %9"
        : "=m"(eax), "=m"(ebx), "=m"(ecx), "=m"(edx),
          "=m"(esi), "=m"(edi), "=m"(ebp), "=m"(esp),
          "=m"(eflags), "=m"(eip)
        :
        : "memory"
    );
    
    printf("EAX="); debug_puthex(eax);
    printf(" EBX="); debug_puthex(ebx);
    printf(" ECX="); debug_puthex(ecx);
    printf(" EDX="); debug_puthex(edx);
    printf("\nESI="); debug_puthex(esi);
    printf(" EDI="); debug_puthex(edi);
    printf(" EBP="); debug_puthex(ebp);
    printf(" ESP="); debug_puthex(esp);
    printf("\nEIP="); debug_puthex(eip);
    printf(" EFLAGS="); debug_puthex(eflags);
    printf("\n");
}
