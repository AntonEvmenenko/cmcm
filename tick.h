#ifndef CMCM_TICK_H
#define CMCM_TICK_H

#include <stdint.h>

uint32_t cmcm_tick_get(void);
uint32_t cmcm_tick_since(uint32_t since);

#endif
