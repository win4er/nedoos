#ifndef DRIVERS_TIMER_H
#define DRIVERS_TIMER_H

#include <stdint.h>

void timer_install(uint32_t frequency);
void timer_wait(uint32_t ticks);
uint32_t timer_get_ticks(void);

#endif
