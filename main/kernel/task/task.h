#ifndef TASK_H
#define TASK_H

#define MAX_TASKS 16
#define STACK_SIZE 4096

typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip, eflags;
    uint32_t cr3;  // Page directory (для защиты памяти)
    int state;     // 0 = свободно, 1 = готов, 2 = работает
    uint32_t pid;
    char name[32];
    uint8_t stack[STACK_SIZE];
} task_t;

#define TASK_READY  1
#define TASK_RUNNING 2
#define TASK_WAITING 3
#define TASK_FREE   0

void task_init(void);
int task_create(void (*entry)(void), const char* name);
void task_yield(void);
void task_exit(void);

#endif
