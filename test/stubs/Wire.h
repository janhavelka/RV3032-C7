#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Arduino.h"

// Arduino-ESP32 3.3.11 default from Wire.h. The example adapter deliberately
// consumes this public core capacity when it is available.
#ifndef I2C_BUFFER_LENGTH
#define I2C_BUFFER_LENGTH 128U
#endif

class TwoWire {
 public:
  enum class Call : uint8_t {
    BEGIN_TRANSMISSION,
    WRITE,
    END_WITH_STOP,
    END_WITHOUT_STOP,
    REQUEST_WITH_STOP
  };

  static constexpr size_t MAX_LOG = 32U;

  void reset() {
    physicalWriteHook = nullptr;
    physicalWriteReadHook = nullptr;
    beginResult = true;
    setClockResult = true;
    beginCalls = 0;
    beginFrequency = 0;
    setClockCalls = 0;
    beginTransmissionCalls = 0;
    writeCalls = 0;
    endTransmissionCalls = 0;
    requestCalls = 0;
    flushCalls = 0;
    physicalAttemptCalls = 0;
    transactionActive = false;
    configuredTimeoutMs = 50;
    stagedLengthLimit = SIZE_MAX;
    requestLength = SIZE_MAX;
    availableLength = SIZE_MAX;
    readFailureIndex = SIZE_MAX;
    beginDurationMs = 0;
    writeDurationMs = 0;
    endDurationMs = 0;
    requestDurationMs = 0;
    endResultCount = 0;
    endResultIndex = 0;
    callCount = 0;
    rxLength = 0;
    rxIndex = 0;
    stagedLength = 0;
    lastPhysicalTxLength = 0;
  }

  bool begin(int, int) {
    ++beginCalls;
    return beginResult;
  }

  bool begin(int, int, uint32_t frequency) {
    ++beginCalls;
    beginFrequency = frequency;
    return beginResult;
  }
  bool setClock(uint32_t) {
    ++setClockCalls;
    return setClockResult;
  }
  void setTimeOut(uint16_t timeoutMs) {
    configuredTimeoutMs = timeoutMs;
  }
  uint16_t getTimeOut() const { return configuredTimeoutMs; }

  void beginTransmission(uint8_t address) {
    transmissionAddress = address;
    ++beginTransmissionCalls;
    transactionActive = true;
    stagedLength = 0;
    record(Call::BEGIN_TRANSMISSION);
    arduinoStubMillis += beginDurationMs;
  }
  size_t write(const uint8_t* data, size_t len) {
    ++writeCalls;
    record(Call::WRITE);
    arduinoStubMillis += writeDurationMs;
    size_t accepted = len < stagedLengthLimit ? len : stagedLengthLimit;
    const size_t remaining = sizeof(stagedData) - stagedLength;
    if (accepted > remaining) accepted = remaining;
    if (data != nullptr && accepted > 0U) {
      for (size_t i = 0; i < accepted; ++i) {
        stagedData[stagedLength + i] = data[i];
      }
      stagedLength += accepted;
    }
    return accepted;
  }
  uint8_t endTransmission(bool stop = true) {
    ++endTransmissionCalls;
    record(stop ? Call::END_WITH_STOP : Call::END_WITHOUT_STOP);
    arduinoStubMillis += endDurationMs;
    if (stop) {
      recordPhysicalAttempt();
      transactionActive = false;
      if (physicalWriteHook != nullptr) {
        return physicalWriteHook(transmissionAddress, lastPhysicalTxData, lastPhysicalTxLength, configuredTimeoutMs);
      }
    }
    if (endResultIndex < endResultCount) {
      return endResults[endResultIndex++];
    }
    return 0;
  }
  size_t requestFrom(uint8_t address, size_t len, bool stop = true) {
    ++requestCalls;
    recordPhysicalAttempt();
    if (stop) {
      record(Call::REQUEST_WITH_STOP);
      transactionActive = false;
    }
    arduinoStubMillis += requestDurationMs;
    const size_t returned = physicalWriteReadHook != nullptr
                                ? physicalWriteReadHook(address, lastPhysicalTxData, lastPhysicalTxLength, rxData, len, configuredTimeoutMs)
                                : (requestLength == SIZE_MAX ? len : requestLength);
    const size_t available = availableLength == SIZE_MAX
                                 ? returned
                                 : availableLength;
    rxLength = available > sizeof(rxData) ? sizeof(rxData) : available;
    rxIndex = 0;
    return returned;
  }
  void flush() {
    ++flushCalls;
    stagedLength = 0;
    rxLength = 0;
    rxIndex = 0;
  }
  int available() const {
    return static_cast<int>(rxLength - rxIndex);
  }
  int read() {
    if (rxIndex == readFailureIndex) {
      return -1;
    }
    if (rxIndex >= rxLength) {
      return -1;
    }
    return rxData[rxIndex++];
  }

  void queueEndResult(uint8_t result) {
    if (endResultCount < MAX_LOG) {
      endResults[endResultCount++] = result;
    }
  }

  bool beginResult = true;
  // Optional physical backend lets native tests execute the maintained HIL
  // harness against the existing chip model, without duplicating its logic.
  uint8_t (*physicalWriteHook)(uint8_t, const uint8_t*, size_t, uint16_t) = nullptr;
  size_t (*physicalWriteReadHook)(uint8_t, const uint8_t*, size_t, uint8_t*, size_t, uint16_t) = nullptr;
  uint8_t transmissionAddress = 0;
  bool setClockResult = true;
  uint32_t beginCalls = 0;
  uint32_t beginFrequency = 0;
  uint32_t setClockCalls = 0;
  uint32_t beginTransmissionCalls = 0;
  uint32_t writeCalls = 0;
  uint32_t endTransmissionCalls = 0;
  uint32_t requestCalls = 0;
  uint32_t flushCalls = 0;
  uint32_t physicalAttemptCalls = 0;
  bool transactionActive = false;
  uint16_t configuredTimeoutMs = 50;
  size_t stagedLengthLimit = SIZE_MAX;
  size_t requestLength = SIZE_MAX;
  size_t availableLength = SIZE_MAX;
  size_t readFailureIndex = SIZE_MAX;
  uint32_t beginDurationMs = 0;
  uint32_t writeDurationMs = 0;
  uint32_t endDurationMs = 0;
  uint32_t requestDurationMs = 0;
  uint8_t endResults[MAX_LOG] = {};
  size_t endResultCount = 0;
  size_t endResultIndex = 0;
  Call calls[MAX_LOG] = {};
  uint16_t effectiveTimeouts[MAX_LOG] = {};
  size_t callCount = 0;
  uint8_t rxData[128] = {};
  size_t rxLength = 0;
  size_t rxIndex = 0;
  uint8_t stagedData[128] = {};
  size_t stagedLength = 0;
  uint8_t lastPhysicalTxData[128] = {};
  size_t lastPhysicalTxLength = 0;

 private:
  void record(Call call) {
    if (callCount < MAX_LOG) {
      calls[callCount] = call;
      effectiveTimeouts[callCount] = configuredTimeoutMs;
      ++callCount;
    }
  }

  void recordPhysicalAttempt() {
    ++physicalAttemptCalls;
    lastPhysicalTxLength = stagedLength;
    for (size_t i = 0; i < stagedLength; ++i) {
      lastPhysicalTxData[i] = stagedData[i];
    }
    stagedLength = 0;
  }
};

inline TwoWire Wire;
