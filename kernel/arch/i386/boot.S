/* Multiboot2 заголовок */
.section .multiboot
.align 8
    .long 0xE85250D6              /* магия Multiboot2 */
    .long 0                       /* архитектура (0 = i386) */
    .long 24                       /* длина заголовка */
    .long -(0xE85250D6 + 0 + 24)  /* контрольная сумма */
    
    /* Конечный тег */
    .short 0
    .short 0
    .long 8

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .text
.global _start
_start:
    mov $stack_top, %esp
    push %ebx              /* multiboot info */
    push %eax              /* magic */
    call kernel_main
    cli
    hlt
