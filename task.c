#include "task.h"
#include <kernel/kernel.h>

static task_t tasks[MAX_TASKS];
static int current_task = -1;
static int next_pid = 1;

extern void task_switch(uint32_t* old_esp, uint32_t new_esp);

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_FREE;
    }
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
        print("No free task slots\n");
        return -1;
    }
    
    uint32_t* stack_top = (uint32_t*)((uint32_t)tasks[slot].stack + STACK_SIZE);
    
    *--stack_top = (uint32_t)task_exit;
    *--stack_top = 0x200;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    *--stack_top = 0;
    
    tasks[slot].esp = (uint32_t)stack_top;
    tasks[slot].eip = (uint32_t)entry;
    tasks[slot].pid = next_pid++;
    tasks[slot].state = TASK_READY;
    
    int i = 0;
    while (name[i] && i < 31) {
        tasks[slot].name[i] = name[i];
        i++;
    }
    tasks[slot].name[i] = 0;
    
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

void task_exit(void) {
    if (current_task != -1) {
        tasks[current_task].state = TASK_FREE;
    }
    while (1) {
        task_yield();
    }
}
