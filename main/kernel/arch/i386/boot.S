.section .multiboot
.align 4
    .long 0x1BADB002      // magic
    .long 0x00            // flags
    .long -0x1BADB002     // checksum

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .text
.global _start
_start:
    movl $stack_top, %esp
    call kernel_main
    cli
    hlt
.Lhang:
    jmp .Lhang
