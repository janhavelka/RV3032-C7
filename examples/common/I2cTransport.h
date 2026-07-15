/**
 * @file I2cTransport.h
 * @brief Wire-based I2C transport adapter for RV3032 examples.
 *
 * This example-only adapter keeps Wire ownership in the application. The
 * caller must serialize the shared bus and keep the Wire mutex uncontended for
 * the complete synchronous callback. Buffers are borrowed only until return.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <limits.h>

#include "RV3032/Status.h"

namespace transport {

static constexpr int32_t I2C_DETAIL_INVALID_ARGUMENT = -1;
static constexpr int32_t I2C_DETAIL_INVALID_TIMEOUT = -2;
static constexpr int32_t I2C_DETAIL_SHORT_STAGING = -3;

class ScopedWireTimeout {
 public:
  inline ScopedWireTimeout(TwoWire& wire, uint16_t timeoutMs)
      : _wire(wire), _previousTimeoutMs(wire.getTimeOut()) {
    _wire.setTimeOut(timeoutMs);
  }

  inline ~ScopedWireTimeout() {
    _wire.setTimeOut(_previousTimeoutMs);
  }

  ScopedWireTimeout(const ScopedWireTimeout&) = delete;
  ScopedWireTimeout& operator=(const ScopedWireTimeout&) = delete;

 private:
  TwoWire& _wire;
  uint16_t _previousTimeoutMs;
};

inline bool remainingBefore(uint32_t nowMs, uint32_t deadlineMs,
                            uint32_t& remainingMs) {
  remainingMs = deadlineMs - nowMs;
  return remainingMs != 0U && remainingMs < 0x80000000UL;
}

inline uint16_t boundedWireTimeout(uint32_t remainingMs) {
  return static_cast<uint16_t>(
      remainingMs > UINT16_MAX ? UINT16_MAX : remainingMs);
}

inline RV3032::Status callbackTimeoutStatus() {
  return RV3032::Status::Error(RV3032::Err::I2C_TIMEOUT,
                               "I2C callback deadline exceeded");
}

inline RV3032::Status mapWireResult(uint8_t result) {
  switch (result) {
    case 0:
      return RV3032::Status::Ok();
    case 1:
      return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                   "I2C data too long", result);
    case 2:
      return RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR,
                                   "I2C address NACK", result);
    case 3:
      return RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA,
                                   "I2C data NACK", result);
    case 4:
      return RV3032::Status::Error(RV3032::Err::I2C_BUS,
                                   "I2C bus error", result);
    case 5:
      return RV3032::Status::Error(RV3032::Err::I2C_TIMEOUT,
                                   "I2C timeout", result);
    default:
      return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                   "I2C unknown error", result);
  }
}

inline bool deadlineCrossed(uint32_t deadlineMs) {
  uint32_t remainingMs = 0;
  return !remainingBefore(millis(), deadlineMs, remainingMs);
}

/** Close a transaction already opened by beginTransmission(). */
inline bool releaseStartedTransaction(TwoWire& wire, uint32_t deadlineMs) {
  uint32_t remainingMs = 0;
  const bool timeRemains = remainingBefore(millis(), deadlineMs, remainingMs);
  const uint16_t releaseTimeoutMs =
      timeRemains ? boundedWireTimeout(remainingMs) : uint16_t{1};
  {
    ScopedWireTimeout timeout(wire, releaseTimeoutMs);
    (void)wire.endTransmission(true);
  }
  return deadlineCrossed(deadlineMs);
}

inline RV3032::Status wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                                uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr || data == nullptr || len == 0U || len > 128U) {
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                 "Invalid I2C callback argument",
                                 I2C_DETAIL_INVALID_ARGUMENT);
  }
  if (timeoutMs == 0U || timeoutMs > UINT16_MAX) {
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                 "Invalid I2C callback timeout",
                                 I2C_DETAIL_INVALID_TIMEOUT);
  }

  const uint32_t deadlineMs = millis() + timeoutMs;
  uint32_t remainingMs = 0;
  if (!remainingBefore(millis(), deadlineMs, remainingMs) ||
      remainingMs <= 1U) {
    return callbackTimeoutStatus();
  }

  {
    ScopedWireTimeout timeout(*wire, boundedWireTimeout(remainingMs - 1U));
    wire->beginTransmission(addr);
  }
  if (deadlineCrossed(deadlineMs)) {
    (void)releaseStartedTransaction(*wire, deadlineMs);
    return callbackTimeoutStatus();
  }

  const size_t written = wire->write(data, len);
  if (written != len) {
    if (releaseStartedTransaction(*wire, deadlineMs)) {
      return callbackTimeoutStatus();
    }
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                 "I2C write staging incomplete",
                                 I2C_DETAIL_SHORT_STAGING);
  }

  if (!remainingBefore(millis(), deadlineMs, remainingMs) ||
      remainingMs <= 1U) {
    (void)releaseStartedTransaction(*wire, deadlineMs);
    return callbackTimeoutStatus();
  }

  uint8_t result = 0;
  {
    ScopedWireTimeout timeout(*wire, boundedWireTimeout(remainingMs - 1U));
    result = wire->endTransmission(true);
  }
  if (deadlineCrossed(deadlineMs)) {
    return callbackTimeoutStatus();
  }
  return mapWireResult(result);
}

inline RV3032::Status wireWriteRead(uint8_t addr, const uint8_t* tx,
                                    size_t txLen, uint8_t* rx, size_t rxLen,
                                    uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr || tx == nullptr || rx == nullptr || txLen == 0U ||
      rxLen == 0U || txLen > 128U || rxLen > 128U) {
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                 "Invalid I2C callback argument",
                                 I2C_DETAIL_INVALID_ARGUMENT);
  }
  if (timeoutMs == 0U || timeoutMs > UINT16_MAX) {
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                 "Invalid I2C callback timeout",
                                 I2C_DETAIL_INVALID_TIMEOUT);
  }

  const uint32_t deadlineMs = millis() + timeoutMs;
  uint32_t remainingMs = 0;
  if (!remainingBefore(millis(), deadlineMs, remainingMs) ||
      remainingMs <= 1U) {
    return callbackTimeoutStatus();
  }

  {
    ScopedWireTimeout timeout(*wire, boundedWireTimeout(remainingMs - 1U));
    wire->beginTransmission(addr);
  }
  if (deadlineCrossed(deadlineMs)) {
    (void)releaseStartedTransaction(*wire, deadlineMs);
    return callbackTimeoutStatus();
  }

  const size_t written = wire->write(tx, txLen);
  if (written != txLen) {
    if (releaseStartedTransaction(*wire, deadlineMs)) {
      return callbackTimeoutStatus();
    }
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                 "I2C write staging incomplete",
                                 I2C_DETAIL_SHORT_STAGING);
  }

  if (!remainingBefore(millis(), deadlineMs, remainingMs) ||
      remainingMs <= 1U) {
    (void)releaseStartedTransaction(*wire, deadlineMs);
    return callbackTimeoutStatus();
  }

  uint8_t result = 0;
  {
    ScopedWireTimeout timeout(*wire, boundedWireTimeout(remainingMs - 1U));
    result = wire->endTransmission(false);
  }
  if (deadlineCrossed(deadlineMs)) {
    (void)releaseStartedTransaction(*wire, deadlineMs);
    return callbackTimeoutStatus();
  }
  if (result != 0U) {
    if (releaseStartedTransaction(*wire, deadlineMs)) {
      return callbackTimeoutStatus();
    }
    return mapWireResult(result);
  }

  if (!remainingBefore(millis(), deadlineMs, remainingMs) ||
      remainingMs <= 1U) {
    (void)releaseStartedTransaction(*wire, deadlineMs);
    return callbackTimeoutStatus();
  }

  size_t read = 0;
  {
    ScopedWireTimeout timeout(*wire, boundedWireTimeout(remainingMs - 1U));
    read = wire->requestFrom(addr, rxLen, true);
  }
  if (deadlineCrossed(deadlineMs)) {
    return callbackTimeoutStatus();
  }
  if (read != rxLen) {
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                 "I2C read length mismatch",
                                 static_cast<int32_t>(read));
  }

  for (size_t i = 0; i < rxLen; ++i) {
    if (wire->available() <= 0) {
      return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                   "I2C data not available");
    }
    const int value = wire->read();
    if (value < 0) {
      return RV3032::Status::Error(RV3032::Err::I2C_ERROR,
                                   "I2C read failed");
    }
    rx[i] = static_cast<uint8_t>(value);
  }

  return deadlineCrossed(deadlineMs) ? callbackTimeoutStatus()
                                      : RV3032::Status::Ok();
}

inline bool initWire(int sda, int scl, uint32_t freq = 400000,
                     uint16_t timeoutMs = 50) {
#if defined(ARDUINO_ARCH_ESP32)
  // Optional application-owned recovery before the bus is initialized.
  pinMode(scl, OUTPUT);
  pinMode(sda, INPUT_PULLUP);
  for (int i = 0; i < 9; i++) {
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
    digitalWrite(scl, HIGH);
    delayMicroseconds(5);
  }
  pinMode(sda, OUTPUT);
  digitalWrite(sda, LOW);
  delayMicroseconds(5);
  digitalWrite(scl, HIGH);
  delayMicroseconds(5);
  digitalWrite(sda, HIGH);
  delayMicroseconds(5);
#endif

  if (!Wire.begin(sda, scl)) {
    return false;
  }
  if (!Wire.setClock(freq)) {
    return false;
  }
  Wire.setTimeOut(timeoutMs);
  return true;
}

}  // namespace transport
