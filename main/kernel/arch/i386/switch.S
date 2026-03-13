.section .text
.global task_switch
.type task_switch, @function

task_switch:
    pushl %ebp
    movl %esp, %ebp
    
    pushl %ebx
    pushl %esi
    pushl %edi
    
    movl 8(%ebp), %eax
    movl %esp, (%eax)
    
    movl 12(%ebp), %esp
    
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    
    ret
