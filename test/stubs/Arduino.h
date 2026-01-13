#pragma once

#include <stddef.h>
#include <stdint.h>

inline uint32_t millis() {
  return 0;
}

inline void delay(uint32_t) {}

inline void yield() {}
