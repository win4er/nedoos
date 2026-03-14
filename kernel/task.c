// task.c - Планировщик задач
#include <stdint.h>

#define MAX_TASKS 16
#define STACK_SIZE 4096

typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip, eflags;
    int state;
    uint32_t pid;
    uint8_t stack[STACK_SIZE];
} task_t;

#define TASK_READY  1
#define TASK_RUNNING 2
#define TASK_FREE   0

static task_t tasks[MAX_TASKS];
static int current_task = -1;
static int next_pid = 1;

extern void task_switch(uint32_t* old_esp, uint32_t new_esp);

void print(const char* s);
void print_dec(uint32_t num);
void set_color(uint8_t fg);

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_FREE;
    }
    set_color(2); // GREEN
    print("Scheduler initialized\n");
}

int task_create(void (*entry)(void), const char* name) {
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FREE) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        return -1;
    }
    
    uint32_t* stack_top = (uint32_t*)((uint32_t)tasks[slot].stack + STACK_SIZE);
    
    *--stack_top = (uint32_t)entry;  // eip
    *--stack_top = 0x200;             // eflags
    *--stack_top = 0;                  // eax
    *--stack_top = 0;                  // ecx
    *--stack_top = 0;                  // edx
    *--stack_top = 0;                  // ebx
    *--stack_top = 0;                  // esp
    *--stack_top = 0;                  // ebp
    *--stack_top = 0;                  // esi
    *--stack_top = 0;                  // edi
    
    tasks[slot].esp = (uint32_t)stack_top;
    tasks[slot].state = TASK_READY;
    tasks[slot].pid = next_pid++;
    
    return tasks[slot].pid;
}

void task_yield(void) {
    if (current_task == -1) return;
    
    tasks[current_task].state = TASK_READY;
    
    int next = current_task;
    for (int i = 0; i < MAX_TASKS; i++) {
        next = (next + 1) % MAX_TASKS;
        if (tasks[next].state == TASK_READY) {
            break;
        }
    }
    
    if (next == current_task || tasks[next].state != TASK_READY) {
        tasks[current_task].state = TASK_RUNNING;
        return;
    }
    
    tasks[next].state = TASK_RUNNING;
    task_switch(&tasks[current_task].esp, tasks[next].esp);
    current_task = next;
}
