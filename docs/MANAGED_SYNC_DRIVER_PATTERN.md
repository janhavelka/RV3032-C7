# Managed Synchronous Driver Pattern

A reusable architecture for embedded I2C device drivers with health tracking and offline detection.

---

## Overview

**Pattern Type**: Managed Synchronous Driver  
**Blocking**: Yes (all public operations are blocking)  
**Health Tracking**: Centralized via tracked I2C transport wrappers  
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

## 4. I2C Transport Wrapper Architecture

### Layered Transport Design

All I2C communication flows through a layered wrapper architecture:

```
┌─────────────────────────────────────────────────────────┐
│  Public API (readTime, setTime, etc.)                   │
│  - NO direct _updateHealth() calls                      │
│  - Uses readRegister/writeRegister                      │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│  Register Helpers (readRegs, writeRegs)                 │
│  - Uses TRACKED wrappers                                │
│  - Health updated automatically per I2C transaction     │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│  TRACKED Wrappers (_i2cWriteReadTracked, _i2cWriteTracked)│
│  - Calls RAW wrapper                                    │
│  - Returns _updateHealth(status)                        │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│  RAW Wrappers (_i2cWriteReadRaw, _i2cWriteRaw)         │
│  - Direct transport callback                            │
│  - NO health tracking                                   │
└───────────────────────┬─────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────┐
│  Transport Callbacks (Config::i2cWrite, i2cWriteRead)  │
│  - Application-provided                                 │
└─────────────────────────────────────────────────────────┘
```

### Raw Transport Wrappers (No Health Tracking)

```cpp
Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen, 
                        uint8_t* rxBuf, size_t rxLen) {
  if (!_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback null");
  }
  return _config.i2cWriteRead(_config.i2cAddress, txBuf, txLen, rxBuf, rxLen,
                              _config.i2cTimeoutMs, _config.i2cUser);
}

Status _i2cWriteRaw(const uint8_t* buf, size_t len) {
  if (!_config.i2cWrite) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback null");
  }
  return _config.i2cWrite(_config.i2cAddress, buf, len,
                          _config.i2cTimeoutMs, _config.i2cUser);
}
```

### Tracked Transport Wrappers (With Health Tracking)

```cpp
Status _i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen, 
                            uint8_t* rxBuf, size_t rxLen) {
  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen);
  return _updateHealth(st);
}

Status _i2cWriteTracked(const uint8_t* buf, size_t len) {
  Status st = _i2cWriteRaw(buf, len);
  return _updateHealth(st);
}
```

### Raw Register Access (For Diagnostics)

```cpp
Status _readRegisterRaw(uint8_t reg, uint8_t& value) {
  uint8_t tx = reg;
  return _i2cWriteReadRaw(&tx, 1, &value, 1);
}
```

**Used by**: `probe()`, `_applyConfig()` for diagnostic reads

---

## 5. Health Tracking — `_updateHealth()`

Called **only** by tracked transport wrappers:

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

### What Triggers Health Updates

| Scenario | Health Updated? |
|----------|-----------------|
| Real I2C transaction (via tracked wrapper) | ✅ YES |
| `Err::IN_PROGRESS` (queued EEPROM) | ✅ YES (treated as success) |
| `INVALID_CONFIG` (precondition) | ❌ NO |
| `INVALID_PARAM` (precondition) | ❌ NO |
| `NOT_INITIALIZED` (precondition) | ❌ NO |
| `probe()` (uses raw wrapper) | ❌ NO |

---

## 6. Register Helpers — Use Tracked Wrappers

```cpp
Status readRegs(uint8_t reg, uint8_t* buf, size_t len) {
  if (!buf || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid parameters");
  }
  uint8_t tx = reg;
  // Health updated automatically by tracked wrapper
  return _i2cWriteReadTracked(&tx, 1, buf, len);
}

Status writeRegs(uint8_t reg, const uint8_t* buf, size_t len) {
  if (!buf || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid parameters");
  }
  uint8_t tx[kMaxWriteLen] = {0};
  tx[0] = reg;
  std::memcpy(&tx[1], buf, len);
  // Health updated automatically by tracked wrapper
  return _i2cWriteTracked(tx, len + 1);
}

Status readRegister(uint8_t reg, uint8_t& value) {
  uint8_t buf = 0;
  Status st = readRegs(reg, &buf, 1);
  if (st.ok()) value = buf;
  return st;
}

Status writeRegister(uint8_t reg, uint8_t value) {
  return writeRegs(reg, &value, 1);
}
```

---

## 7. Public API — No Direct `_updateHealth()` Calls

After this refactor, public API methods **never** call `_updateHealth()` directly:

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
  
  // 3. I2C operations (health updated automatically via tracked wrappers)
  Status st = readRegister(REG_X, value);
  if (!st.ok()) return st;  // Just return - health already updated
  
  return writeRegister(REG_Y, newValue);  // Health updated automatically
}
```

### Multi-Step Operations

Each I2C transaction updates health independently. This is intentional:

```cpp
Status setAlarmTime(uint8_t minute, uint8_t hour, uint8_t date) {
  if (!_initialized) return Status::Error(Err::NOT_INITIALIZED, ...);
  if (invalid params) return Status::Error(Err::INVALID_PARAM, ...);
  
  // Each call updates health automatically
  Status st = readRegister(REG_ALARM_MINUTE, minReg);
  if (!st.ok()) return st;
  
  st = readRegister(REG_ALARM_HOUR, hourReg);
  if (!st.ok()) return st;
  
  // ... modify ...
  
  st = writeRegister(REG_ALARM_MINUTE, minReg);
  if (!st.ok()) return st;
  
  return writeRegister(REG_ALARM_DATE, dateReg);
}
```

---

## 8. Diagnostics — Use Raw Path

### `probe()` — No Health Tracking

```cpp
Status probe() {
  uint8_t dummy = 0;
  Status st = _readRegisterRaw(REG_STATUS, dummy);
  
  if (!st.ok() && (st.code == Err::I2C_ERROR || st.code == Err::TIMEOUT)) {
    return Status::Error(Err::DEVICE_NOT_FOUND, "Device not responding");
  }
  
  // Do NOT call _updateHealth() - diagnostic only
  return st;
}
```

**Key Properties**:
- Can be called in ANY state (even UNINIT)
- Does NOT modify `_driverState`
- Does NOT update health counters
- Uses `_readRegisterRaw()` → `_i2cWriteReadRaw()`

---

## 9. Lifecycle Functions

### `begin()`

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
  
  // Validate config (no health tracking for config errors)
  if (!_config.i2cWrite || !_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks null");
  }
  
  // Probe device (uses raw path - no health tracking)
  Status st = probe();
  if (!st.ok()) return st;
  
  // Apply config (uses tracked path - health updated automatically)
  st = _applyConfig();
  if (!st.ok() && st.code != Err::IN_PROGRESS) return st;
  
  _initialized = true;
  _driverState = DriverState::READY;
  return Status::Ok();
}
```

### `recover()`

```cpp
Status recover() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "begin() not called");
  }
  
  // Probe uses raw path (no health tracking)
  Status st = probe();
  if (!st.ok()) return st;
  
  // Reapply config uses tracked path (health updated automatically)
  st = _applyConfig();
  if (st.ok()) return Status::Ok();
  
  return st;
}
```

### `end()`

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

---

## 10. Public Health Getters

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

---

## 11. Summary: Transport Wrapper Functions

### Added Private Members

| Name | Purpose |
|------|---------|
| `_i2cWriteReadRaw(...)` | Raw write-then-read, NO health tracking |
| `_i2cWriteRaw(...)` | Raw write-only, NO health tracking |
| `_i2cWriteReadTracked(...)` | Tracked write-then-read, calls `_updateHealth()` |
| `_i2cWriteTracked(...)` | Tracked write-only, calls `_updateHealth()` |
| `_readRegisterRaw(...)` | Raw single register read (for diagnostics) |
| `_updateHealth(...)` | Health state manager (called only by tracked wrappers) |

### Functions Using Tracked Wrappers

- `readRegs()` → `_i2cWriteReadTracked()`
- `writeRegs()` → `_i2cWriteTracked()`
- All public API via `readRegister()`/`writeRegister()`

### Functions Using Raw Wrappers

- `probe()` → `_readRegisterRaw()` → `_i2cWriteReadRaw()`
- `_applyConfig()` reads → `_readRegisterRaw()` → `_i2cWriteReadRaw()`

---

## 12. Key Design Principles

1. **Centralized health tracking** — All `_updateHealth()` calls inside tracked transport wrappers
2. **Public API never calls `_updateHealth()`** — Tracking is automatic via wrappers
3. **Diagnostic isolation** — `probe()` and raw register reads bypass health tracking
4. **Precondition errors don't affect health** — Config/param validation errors return early
5. **IN_PROGRESS is success** — Queued async ops don't count as failures
6. **Per-transaction tracking** — Multi-step ops update health per I2C call (intentional)
7. **Transport agnostic** — Driver never touches Wire/I2C directly
