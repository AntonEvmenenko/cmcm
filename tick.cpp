#include "tick.h"

#include <Arduino.h>

#define CMCM_TICK_MAX 0xFFFFFFFF // 32-bit

namespace cmcm {

uint32_t tick_get(void) {
  return millis();
}

uint32_t tick_since(uint32_t since) {
  uint32_t now = tick_get();

  if (now >= since) {
    return now - since;
  } else {
    // counter overflow
    return now + (CMCM_TICK_MAX - since);
  }
}

}