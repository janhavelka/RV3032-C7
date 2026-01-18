# Managed Synchronous Driver Pattern

A reusable architecture for embedded I2C device drivers with health tracking and offline detection.

---

## Overview

**Pattern Type**: Managed Synchronous Driver  
**Blocking**: Yes (all public operations are blocking)  
**Health Tracking**: Centralized via `_updateHealth()`  
**Recovery**: Manual only (no automatic recovery in tick)  
**Async Support**: Limited to EEPROM state machine only

---

## 1. Driver State Model (4 States)

```cpp
enum class DriverState : uint8_t {
  UNINIT,    // begin() not called or end() called
  READY,     // Operational, consecutiveFailures == 0
  DEGRADED,  // 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    // consecutiveFailures >= offlineThreshold
};
```

### State Transition Rules

| From | Event | To |
|------|-------|-----|
| Any | `begin()` success | READY |
| Any | `end()` | UNINIT |
| READY | I2C failure | DEGRADED |
| DEGRADED | I2C success | READY |
| DEGRADED | failures >= threshold | OFFLINE |
| OFFLINE | I2C success | READY |
| OFFLINE | `recover()` success | READY |

### Dual Tracking: `_initialized` vs `DriverState`

| Field | Purpose | Values |
|-------|---------|--------|
| `_initialized` (bool) | Guards API calls | true/false |
| `_driverState` (enum) | Reflects device health | UNINIT/READY/DEGRADED/OFFLINE |

**Consistency Rules**:
- `_initialized == false` → `_driverState == UNINIT` (always)
- `_initialized == true` → `_driverState ∈ {READY, DEGRADED, OFFLINE}`

---

## 2. Configuration

### Config Struct Addition

```cpp
struct Config {
  // ... existing fields ...
  
  /// Consecutive failure threshold before OFFLINE state
  /// Default: 3. Clamped to minimum 1 in begin().
  uint8_t offlineThreshold = 3;
};
```

### Validation in begin()

```cpp
if (_config.offlineThreshold < 1) {
  _config.offlineThreshold = 1;
}
```

---

## 3. Health Tracking Fields (Private)

```cpp
// Driver state
DriverState _driverState = DriverState::UNINIT;

// Health counters
uint32_t _lastOkMs = 0;              // Timestamp of last success (millis)
uint32_t _lastErrorMs = 0;           // Timestamp of last error (millis)
Status   _lastError = Status::Ok();  // Most recent error
uint8_t  _consecutiveFailures = 0;   // Failures since last success
uint32_t _totalFailures = 0;         // Lifetime failure count
uint32_t _totalSuccess = 0;          // Lifetime success count
```

---

## 4. Core Private Helpers

### `_updateHealth(const Status& st)` — Central Health Tracker

**Purpose**: Single point for all health state updates.

```cpp
Status _updateHealth(const Status& st) {
  bool isSuccess = st.ok() || st.code == Err::IN_PROGRESS;
  
  if (isSuccess) {
    _lastOkMs = millis();
    _consecutiveFailures = 0;
    _totalSuccess++;
    if (_driverState == DriverState::DEGRADED || 
        _driverState == DriverState::OFFLINE) {
      _driverState = DriverState::READY;
    }
  } else {
    _lastError = st;
    _lastErrorMs = millis();
    _consecutiveFailures++;
    _totalFailures++;
    if (_consecutiveFailures == 1 && _driverState == DriverState::READY) {
      _driverState = DriverState::DEGRADED;
    }
    if (_consecutiveFailures >= _config.offlineThreshold) {
      _driverState = DriverState::OFFLINE;
    }
  }
  return st;
}
```

**When to call**:
| Scenario | Call `_updateHealth()`? |
|----------|------------------------|
| Real I2C transaction result | ✅ YES |
| `Err::IN_PROGRESS` (queued) | ✅ YES (treated as success) |
| `INVALID_CONFIG` | ❌ NO |
| `INVALID_PARAM` | ❌ NO |
| `NOT_INITIALIZED` | ❌ NO |
| `probe()` result | ❌ NO (diagnostic only) |

### `_readRegisterRaw(uint8_t reg, uint8_t& value)` — Raw I2C Read

**Purpose**: Bypass health tracking for diagnostics.

```cpp
Status _readRegisterRaw(uint8_t reg, uint8_t& value) {
  if (!_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback null");
  }
  uint8_t tx = reg;
  return _config.i2cWriteRead(_config.i2cAddress, &tx, 1, &value, 1,
                              _config.i2cTimeoutMs, _config.i2cUser);
}
```

**Used by**: `probe()`, `_applyConfig()`

### `_applyConfig()` — Shared Configuration Application

**Purpose**: Apply stored config to device (shared by `begin()` and `recover()`).

```cpp
Status _applyConfig() {
  // Read-modify-write device registers as needed
  // Does NOT call _updateHealth() - caller handles that
  return Status::Ok();
}
```

---

## 5. Public API — Lifecycle

### `Status begin(const Config& config)`

**Purpose**: Initialize driver and device.

```cpp
Status begin(const Config& config) {
  _config = config;
  _driverState = DriverState::UNINIT;
  _initialized = false;
  
  // Reset health tracking
  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
  
  // Clamp threshold
  if (_config.offlineThreshold < 1) _config.offlineThreshold = 1;
  
  // Validate config (no _updateHealth for config errors)
  if (!_config.i2cWrite || !_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks null");
  }
  
  // Probe device (no health update)
  Status st = probe();
  if (!st.ok()) {
    _updateHealth(st);  // Track the failure
    return st;
  }
  
  // Apply config
  st = _applyConfig();
  if (!st.ok() && st.code != Err::IN_PROGRESS) {
    _updateHealth(st);
    return st;
  }
  
  _initialized = true;
  _updateHealth(Status::Ok());  // Sets READY
  return Status::Ok();
}
```

### `void end()`

**Purpose**: Deinitialize driver.

```cpp
void end() {
  _initialized = false;
  _driverState = DriverState::UNINIT;
  
  // Reset health tracking
  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
}
```

### `void tick(uint32_t nowMs)`

**Purpose**: Advance async state machines (EEPROM only).

- Does **NOT** perform automatic recovery
- Does **NOT** update health tracking
- Only drives EEPROM persistence state machine

---

## 6. Public API — Diagnostics

### `Status probe()`

**Purpose**: Check device presence without affecting state.

```cpp
Status probe() {
  uint8_t dummy = 0;
  Status st = _readRegisterRaw(REG_STATUS, dummy);
  
  if (!st.ok() && (st.code == Err::I2C_ERROR || st.code == Err::TIMEOUT)) {
    return Status::Error(Err::DEVICE_NOT_FOUND, "Device not responding", st.detail);
  }
  
  // Do NOT call _updateHealth() - diagnostic only
  return st;
}
```

**Key Properties**:
- Can be called in ANY state (even UNINIT)
- Does NOT modify `_driverState`
- Does NOT update health counters
- Uses `_readRegisterRaw()` to bypass tracking

### `Status recover()`

**Purpose**: Manual recovery from OFFLINE state.

```cpp
Status recover() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "begin() not called");
  }
  
  Status st = probe();
  if (!st.ok()) {
    _updateHealth(st);  // Track failed recovery attempt
    return st;
  }
  
  st = _applyConfig();
  _updateHealth(st);  // Success → READY, failure → track it
  return st;
}
```

**Key Properties**:
- Requires `_initialized == true`
- On success: resets counters, sets READY
- On failure: increments counters, may transition to OFFLINE

---

## 7. Public API — Health Getters

| Function | Return Type | Description |
|----------|-------------|-------------|
| `state()` | `DriverState` | Current state (UNINIT/READY/DEGRADED/OFFLINE) |
| `isOnline()` | `bool` | `true` if READY or DEGRADED |
| `lastOkMs()` | `uint32_t` | Timestamp of last successful I2C op |
| `lastErrorMs()` | `uint32_t` | Timestamp of last failed I2C op |
| `lastError()` | `Status` | Most recent error status |
| `consecutiveFailures()` | `uint8_t` | Failures since last success |
| `totalFailures()` | `uint32_t` | Lifetime failure count |
| `totalSuccess()` | `uint32_t` | Lifetime success count |

```cpp
DriverState state() const { return _driverState; }

bool isOnline() const {
  return _driverState == DriverState::READY || 
         _driverState == DriverState::DEGRADED;
}

uint32_t lastOkMs() const { return _lastOkMs; }
uint32_t lastErrorMs() const { return _lastErrorMs; }
Status lastError() const { return _lastError; }
uint8_t consecutiveFailures() const { return _consecutiveFailures; }
uint32_t totalFailures() const { return _totalFailures; }
uint32_t totalSuccess() const { return _totalSuccess; }
```

---

## 8. Public API — Device Operations Pattern

All public methods that perform I2C follow this pattern:

```cpp
Status someOperation(params...) {
  // 1. Precondition check (no health update)
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  
  // 2. Parameter validation (no health update)
  if (invalidParams) {
    return Status::Error(Err::INVALID_PARAM, "Invalid parameters");
  }
  
  // 3. I2C operations (track health on result)
  Status st = readRegister(REG_X, value);
  if (!st.ok()) return _updateHealth(st);
  
  st = writeRegister(REG_Y, newValue);
  return _updateHealth(st);  // Final result
}
```

### Multi-Step Operations

For operations with multiple I2C calls, count as **ONE logical operation**:
- Call `_updateHealth()` on early exit (failure)
- Call `_updateHealth()` on final result (success or failure)
- Do NOT call multiple times for intermediate steps

---

## 9. I2C Transport Abstraction

The driver does NOT own I2C. Transport is injected via Config:

```cpp
// Function pointer types
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                               uint32_t timeoutMs, void* user);

using I2cWriteReadFn = Status (*)(uint8_t addr, 
                                   const uint8_t* txData, size_t txLen,
                                   uint8_t* rxData, size_t rxLen,
                                   uint32_t timeoutMs, void* user);

struct Config {
  I2cWriteFn i2cWrite = nullptr;         // Write-only operations
  I2cWriteReadFn i2cWriteRead = nullptr; // Write-then-read (register reads)
  void* i2cUser = nullptr;               // User context (e.g., &Wire)
  uint8_t i2cAddress = 0x51;
  uint32_t i2cTimeoutMs = 100;
};
```

| Callback | Purpose | I2C Sequence |
|----------|---------|--------------|
| `i2cWrite` | Write data to device | START → ADDR+W → DATA → STOP |
| `i2cWriteRead` | Read registers | START → ADDR+W → REG → RESTART → ADDR+R → DATA → STOP |

---

## 10. Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Private members | `_camelCase` | `_driverState`, `_lastOkMs` |
| Public methods | `camelCase` | `probe()`, `recover()`, `isOnline()` |
| Private helpers | `_camelCase` | `_updateHealth()`, `_applyConfig()` |
| Enum values | `CAPS_CASE` | `UNINIT`, `READY`, `OFFLINE` |
| Config fields | `camelCase` | `offlineThreshold`, `i2cTimeoutMs` |

---

## 11. Summary Table

### Private Members Added

| Name | Type | Purpose |
|------|------|---------|
| `_driverState` | `DriverState` | Current operational state |
| `_lastOkMs` | `uint32_t` | Last success timestamp |
| `_lastErrorMs` | `uint32_t` | Last error timestamp |
| `_lastError` | `Status` | Most recent error |
| `_consecutiveFailures` | `uint8_t` | Sequential failure count |
| `_totalFailures` | `uint32_t` | Lifetime failures |
| `_totalSuccess` | `uint32_t` | Lifetime successes |

### Private Methods Added

| Name | Purpose |
|------|---------|
| `_updateHealth(const Status&)` | Central health state manager |
| `_readRegisterRaw(uint8_t, uint8_t&)` | Raw I2C read (no health tracking) |
| `_applyConfig()` | Apply stored config to device |

### Public Methods Added

| Name | Returns | Purpose |
|------|---------|---------|
| `probe()` | `Status` | Check device presence (no state change) |
| `recover()` | `Status` | Manual recovery attempt |
| `state()` | `DriverState` | Get current state |
| `isOnline()` | `bool` | Check if operational |
| `lastOkMs()` | `uint32_t` | Last success timestamp |
| `lastErrorMs()` | `uint32_t` | Last error timestamp |
| `lastError()` | `Status` | Most recent error |
| `consecutiveFailures()` | `uint8_t` | Sequential failures |
| `totalFailures()` | `uint32_t` | Lifetime failures |
| `totalSuccess()` | `uint32_t` | Lifetime successes |

### Config Fields Added

| Name | Type | Default | Purpose |
|------|------|---------|---------|
| `offlineThreshold` | `uint8_t` | `3` | Failures before OFFLINE |

---

## 12. Example Usage

```cpp
#include "Driver.h"

Driver device;

void setup() {
  Config cfg;
  cfg.i2cWrite = myI2cWrite;
  cfg.i2cWriteRead = myI2cWriteRead;
  cfg.i2cUser = &Wire;
  cfg.offlineThreshold = 3;
  
  // Optional pre-check
  if (!device.probe().ok()) {
    Serial.println("Device not found!");
    return;
  }
  
  if (!device.begin(cfg).ok()) {
    Serial.println("Init failed!");
    return;
  }
}

void loop() {
  device.tick(millis());
  
  if (!device.isOnline()) {
    // Manual recovery every 5s
    static uint32_t lastTry = 0;
    if (millis() - lastTry > 5000) {
      if (device.recover().ok()) {
        Serial.println("Recovered!");
      }
      lastTry = millis();
    }
    return;
  }
  
  // Normal operations...
  Status st = device.doSomething();
  if (!st.ok()) {
    Serial.printf("Error: %s (failures: %d)\n", 
                  st.msg, device.consecutiveFailures());
  }
}
```

---

## 13. Key Design Principles

1. **Centralized health tracking** — All state changes flow through `_updateHealth()`
2. **No implicit recovery** — Application controls retry strategy
3. **Diagnostic isolation** — `probe()` never affects health counters
4. **Config vs I2C errors** — Only real bus transactions affect health
5. **IN_PROGRESS is success** — Queued async ops don't count as failures
6. **Single logical operation** — Multi-step ops count once, not per-transaction
7. **Transport agnostic** — Driver never touches Wire/I2C directly
