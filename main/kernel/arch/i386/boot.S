.section .multiboot
.align 4
.long 0x1BADB002
.long 0x00
.long -(0x1BADB002)

.section .text
.global _start
_start:
    mov $kernel_main, %eax
    call *%eax
1:  hlt
    jmp 1b
