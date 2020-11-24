#ifndef CMCM_TICK_H
#define CMCM_TICK_H

#include <stdint.h>

namespace cmcm {

uint32_t tick_get(void);
uint32_t tick_since(uint32_t since);

}

#endif
