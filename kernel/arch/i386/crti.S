.section .init
.global _init
.type _init, @function
_init:
    pushl %ebp      # Используем pushl для 32-бит
    movl %esp, %ebp
    # gcc will nicely put the contents of crtbegin.o's .init section here

.section .fini
.global _fini
.type _fini, @function
_fini:
    pushl %ebp      # Используем pushl для 32-бит
    movl %esp, %ebp
    # gcc will nicely put the contents of crtbegin.o's .fini section here
