#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <stdint.h>

void scheduler_init(void);
int scheduler_create_task(void (*entry)(void));
void scheduler_switch(void);

#endif
