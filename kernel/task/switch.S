.section .text
.global task_switch
.type task_switch, @function
task_switch:
    # Save current task state
    pushl %ebp
    movl %esp, %ebp
    
    pushl %ebx
    pushl %esi
    pushl %edi
    
    # Save current stack pointer
    movl 8(%ebp), %eax    # old_esp
    movl %esp, (%eax)
    
    # Load new task state
    movl 12(%ebp), %esp   # new_esp
    
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    
    ret
.size task_switch, . - task_switch
