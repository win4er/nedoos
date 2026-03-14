#include "task.h"

static task_t tasks[MAX_TASKS];
static int current_task = -1;
static int next_pid = 1;

// Внешняя функция переключения контекста (в assembly)
extern void task_switch(uint32_t* old_esp, uint32_t new_esp);

void task_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_FREE;
    }
    print_color("Scheduler initialized\n", COLOR_GREEN);
}

int task_create(void (*entry)(void), const char* name) {
    // Ищем свободный слот
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FREE) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        print_color("No free task slots\n", COLOR_RED);
        return -1;
    }
    
    // Создаём стек для задачи
    uint32_t* stack_top = (uint32_t*)((uint32_t)tasks[slot].stack + STACK_SIZE);
    
    // Подготавливаем начальный контекст
    *--stack_top = (uint32_t)task_exit;  // Адрес возврата
    *--stack_top = 0x200;                 // EFLAGS (IF=1)
    *--stack_top = 0;                      // EAX
    *--stack_top = 0;                      // ECX
    *--stack_top = 0;                      // EDX
    *--stack_top = 0;                      // EBX
    *--stack_top = 0;                      // ESP (будет перезаписан)
    *--stack_top = 0;                      // EBP
    *--stack_top = 0;                      // ESI
    *--stack_top = 0;                      // EDI
    
    // Сохраняем указатель стека и eip
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
    
    // Сохраняем текущую задачу
    tasks[current_task].state = TASK_READY;
    
    // Ищем следующую готовую задачу
    int next = current_task;
    for (int i = 0; i < MAX_TASKS; i++) {
        next = (next + 1) % MAX_TASKS;
        if (tasks[next].state == TASK_READY) {
            break;
        }
    }
    
    if (next == current_task || tasks[next].state != TASK_READY) {
        // Нет других задач, возвращаемся
        tasks[current_task].state = TASK_RUNNING;
        return;
    }
    
    // Переключаемся на следующую задачу
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

// Обработчик прерывания таймера
void timer_handler(void) {
    task_yield();
}
