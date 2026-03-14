#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <kernel/debug.h>
#include "scheduler.h"

#define MAX_TASKS 64
#define STACK_SIZE 4096

typedef struct task {
    uint32_t id;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eip;
    uint32_t page_directory;
    uint8_t stack[STACK_SIZE];
    int state;  // 0 = dead, 1 = ready, 2 = running
} task_t;

static task_t tasks[MAX_TASKS];
static int current_task = -1;
static int next_task_id = 1;

// Called from timer interrupt
void scheduler_switch(void) {
    // Simple round-robin scheduling
    int old_task = current_task;
    
    // Find next ready task
    for (int i = 0; i < MAX_TASKS; i++) {
        current_task = (current_task + 1) % MAX_TASKS;
        if (tasks[current_task].state == 1) {
            break;
        }
    }
    
    if (current_task == old_task || tasks[current_task].state != 1) {
        // No other task to switch to
        current_task = old_task;
        return;
    }
    
    // Switch tasks
    tasks[old_task].state = 1;
    tasks[current_task].state = 2;
    
    // Context switch (implemented in assembly)
    extern void task_switch(uint32_t old_esp, uint32_t new_esp);
    task_switch(tasks[old_task].esp, tasks[current_task].esp);
}

int scheduler_create_task(void (*entry)(void)) {
    // Find free task slot
    int task_id = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == 0) {
            task_id = i;
            break;
        }
    }
    
    if (task_id == -1) {
        printf("Scheduler: No free task slots\n");
        return -1;
    }
    
    // Initialize task stack
    uint32_t* stack_top = (uint32_t*)((uint32_t)tasks[task_id].stack + STACK_SIZE);
    
    // Setup initial stack frame for task_switch return
    *--stack_top = (uint32_t)entry;  // EIP
    *--stack_top = 0x200;             // EFLAGS (IF set)
    *--stack_top = 0;                  // EAX
    *--stack_top = 0;                  // ECX
    *--stack_top = 0;                  // EDX
    *--stack_top = 0;                  // EBX
    *--stack_top = 0;                  // ESP (unused)
    *--stack_top = 0;                  // EBP
    *--stack_top = 0;                  // ESI
    *--stack_top = 0;                  // EDI
    
    tasks[task_id].esp = (uint32_t)stack_top;
    tasks[task_id].ebp = (uint32_t)stack_top;
    tasks[task_id].id = next_task_id++;
    tasks[task_id].state = 1;
    
    return tasks[task_id].id;
}

void scheduler_init(void) {
    printf("Initializing scheduler...\n");
    
    memset(tasks, 0, sizeof(tasks));
    current_task = -1;
    
    printf("Scheduler initialized\n");
}
