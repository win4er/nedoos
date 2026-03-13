/* Только мультизагрузочный заголовок, ничего лишнего */
.section .multiboot, "a"
.align 4
    .long 0x1BADB002
    .long 0x00
    .long -0x1BADB002

/* Стек */
.section .bss, "aw", @nobits
.align 16
stack_bottom:
    .skip 16384
stack_top:

/* Точка входа */
.section .text, "ax"
.global _start
_start:
    mov $stack_top, %esp
    call kernel_main
    cli
    hlt
