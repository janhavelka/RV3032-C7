#pragma once

#include <stddef.h>
#include <stdint.h>

class TwoWire {
 public:
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission(bool stop = true) {
    (void)stop;
    return 0;
  }
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t*, size_t len) { return len; }
  size_t requestFrom(uint8_t, uint8_t len) {
    return len;
  }
  int available() { return 0; }
  int read() { return 0; }
};
