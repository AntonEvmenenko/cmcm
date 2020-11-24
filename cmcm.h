#ifndef CMCM_H
#define CMCM_H

#include <stdint.h>

// stacks for all tasks are allocated statically so tune
// these values so as to not needlessly waste RAM
#ifndef CMCM_MAX_NUM_TASKS
#define CMCM_MAX_NUM_TASKS 8
#endif

#ifndef CMCM_STACK_SIZE
#define CMCM_STACK_SIZE 2048
#endif

namespace cmcm {

void create_task(void (*handler)(void));
int get_current_task(void);
void context_switch(void);
void yield(void);
void sleep(uint32_t ticks);
void pause(void);
void resume(int task_id);

void disable_interrupts(void);
void enable_interrupts(void);

}

#include "tick.h"

#endif
