#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Arduino.h"

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
    beginResult = true;
    setClockResult = true;
    beginCalls = 0;
    setClockCalls = 0;
    beginTransmissionCalls = 0;
    writeCalls = 0;
    endTransmissionCalls = 0;
    requestCalls = 0;
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
  }

  bool begin(int, int) {
    ++beginCalls;
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

  void beginTransmission(uint8_t) {
    ++beginTransmissionCalls;
    transactionActive = true;
    record(Call::BEGIN_TRANSMISSION);
    arduinoStubMillis += beginDurationMs;
  }
  size_t write(const uint8_t*, size_t len) {
    ++writeCalls;
    record(Call::WRITE);
    arduinoStubMillis += writeDurationMs;
    return len < stagedLengthLimit ? len : stagedLengthLimit;
  }
  uint8_t endTransmission(bool stop = true) {
    ++endTransmissionCalls;
    record(stop ? Call::END_WITH_STOP : Call::END_WITHOUT_STOP);
    arduinoStubMillis += endDurationMs;
    if (stop) {
      transactionActive = false;
    }
    if (endResultIndex < endResultCount) {
      return endResults[endResultIndex++];
    }
    return 0;
  }
  size_t requestFrom(uint8_t, size_t len, bool stop = true) {
    ++requestCalls;
    if (stop) {
      record(Call::REQUEST_WITH_STOP);
      transactionActive = false;
    }
    arduinoStubMillis += requestDurationMs;
    const size_t returned = requestLength == SIZE_MAX ? len : requestLength;
    const size_t available = availableLength == SIZE_MAX
                                 ? returned
                                 : availableLength;
    rxLength = available > sizeof(rxData) ? sizeof(rxData) : available;
    rxIndex = 0;
    return returned;
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
  bool setClockResult = true;
  uint32_t beginCalls = 0;
  uint32_t setClockCalls = 0;
  uint32_t beginTransmissionCalls = 0;
  uint32_t writeCalls = 0;
  uint32_t endTransmissionCalls = 0;
  uint32_t requestCalls = 0;
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

 private:
  void record(Call call) {
    if (callCount < MAX_LOG) {
      calls[callCount] = call;
      effectiveTimeouts[callCount] = configuredTimeoutMs;
      ++callCount;
    }
  }
};

inline TwoWire Wire;
