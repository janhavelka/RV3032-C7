#pragma once

#include <Arduino.h>

template <typename DriverT>
inline void printHealthView(const DriverT& driver) {
  Serial.printf("state=%d failures=%u totalFail=%lu totalOk=%lu\n",
                static_cast<int>(driver.state()),
                static_cast<unsigned>(driver.consecutiveFailures()),
                static_cast<unsigned long>(driver.totalFailures()),
                static_cast<unsigned long>(driver.totalSuccess()));
}
