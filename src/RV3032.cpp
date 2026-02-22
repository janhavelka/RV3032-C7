/**
 * @file RV3032.cpp
 * @brief Implementation of RV-3032-C7 RTC driver
 */

#include "RV3032/RV3032.h"
#include "RV3032/CommandTable.h"
#include <Arduino.h>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace RV3032 {

// Implementation-only constants (not part of public API)
namespace {
constexpr uint32_t kEepromPreCmdTimeoutMs = 50;
constexpr uint8_t kMaxTimerFrequency = static_cast<uint8_t>(TimerFrequency::Hz1_60);
constexpr uint8_t kMaxClkoutFrequency = static_cast<uint8_t>(ClkoutFrequency::Hz1);
constexpr uint8_t kMaxEviDebounce = static_cast<uint8_t>(EviDebounce::Hz8);
constexpr uint32_t kEpoch2000 = 946684800UL;  // 2000-01-01 00:00:00 UTC

/// @brief Check if deadline has passed, with wraparound-safe comparison.
/// Uses signed arithmetic to handle millis() wraparound (~49 days).
bool hasDeadlinePassed(uint32_t now_ms, uint32_t deadline_ms) {
  return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

bool isI2cFailure(const Status& st) {
  return st.code == Err::I2C_ERROR || st.code == Err::TIMEOUT;
}

Status mapPresenceError(const Status& st) {
  if (!st.ok() && isI2cFailure(st)) {
    return Status::Error(Err::DEVICE_NOT_FOUND, "RTC not responding", st.detail);
  }
  return st;
}
}  // namespace

// ===== Lifecycle Functions =====

Status RV3032::begin(const Config& config) {
  // Clean up any previous instance to ensure consistent state.
  // If caller needs to detect accidental re-initialization, check isInitialized() first.
  if (_initialized) {
    end();
  }

  // Validate configuration FIRST - don't modify any state until validation passes
  if (!config.i2cWrite || !config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C transport callbacks are null");
  }
  if (config.i2cAddress != cmd::I2C_ADDR_7BIT) {
    return Status::Error(Err::INVALID_CONFIG, "RV3032 I2C address must be 0x51");
  }
  if (config.i2cTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "I2C timeout must be > 0");
  }
  if (config.enableEepromWrites && config.eepromTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "EEPROM timeout must be > 0");
  }
  if (config.enableEepromWrites && config.i2cTimeoutMs < kEepromPreCmdTimeoutMs) {
    return Status::Error(Err::INVALID_CONFIG, "I2C timeout must be >= 50ms for EEPROM writes");
  }

  // Validation passed - now initialize all state
  _initialized = false;
  _beginInProgress = false;
  _driverState = DriverState::UNINIT;
  _eeprom = EepromOp{};
  _eepromLastStatus = Status::Ok();
  _eepromWriteCount = 0;
  _eepromWriteFailures = 0;
  _lastOkMs = 0;
  _lastError = Status::Ok();
  _lastErrorMs = 0;
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;

  // Copy validated config
  _config = config;
  _beginInProgress = true;

  // Clamp offlineThreshold (values < 1 make no sense)
  if (_config.offlineThreshold < 1) {
    _config.offlineThreshold = 1;
  }

  // Test device presence (probe uses raw I2C - no health tracking for diagnostic)
  Status st = probe();
  if (!st.ok()) {
    // Device not found - remain in UNINIT with clean health state
    // (no health tracking since begin() failed)
    _beginInProgress = false;
    return st;
  }

  // Apply stored configuration (uses tracked I2C internally)
  // Health is updated automatically via tracked wrappers
  st = _applyConfig();
  if (!st.ok() && st.code != Err::IN_PROGRESS) {
    _beginInProgress = false;
    return st;
  }

  // Success - public lifecycle methods do not call _updateHealth().
  _initialized = true;
  _driverState = DriverState::READY;
  _beginInProgress = false;
  return Status::Ok();
}

void RV3032::tick(uint32_t now_ms) {
  if (!_initialized) {
    return;
  }
  processEeprom(now_ms);
}

void RV3032::end() {
  _initialized = false;
  _beginInProgress = false;
  _driverState = DriverState::UNINIT;
  _eeprom = EepromOp{};
  _eepromLastStatus = Status::Ok();
  _eepromWriteCount = 0;
  _eepromWriteFailures = 0;

  // Reset health tracking
  _lastOkMs = 0;
  _lastError = Status::Ok();
  _lastErrorMs = 0;
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
  // No resources to release (I2C managed by application)
}

bool RV3032::isEepromBusy() const {
  return _eeprom.state != EepromState::Idle || _eeprom.queueCount > 0;
}

Status RV3032::getEepromStatus() const {
  if (isEepromBusy()) {
    return Status::Error(Err::IN_PROGRESS, "EEPROM update in progress");
  }
  return _eepromLastStatus;
}

// ===== Driver State and Health =====

Status RV3032::probe() {
  if (!_initialized && !_beginInProgress) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks not configured");
  }
  // Read a known register to verify device presence.
  // Uses raw I2C (no health tracking) - this is a diagnostic-only function.
  // The status register value is not used; we only check if I2C succeeds.
  uint8_t statusReg = 0;
  Status st = _readRegisterRaw(cmd::REG_STATUS, statusReg);
  (void)statusReg;  // Value unused - we only verify I2C communication works

  // Do NOT call _updateHealth() - probe is diagnostic only
  return mapPresenceError(st);
}

Status RV3032::recover() {
  // Precondition: must be initialized to recover
  if (!_initialized) {
    // No health tracking for precondition errors
    return Status::Error(Err::NOT_INITIALIZED, "begin() not called");
  }

  // Verify device presence with tracked I2C so recovery failures affect health.
  uint8_t statusReg = 0;
  Status st = readRegister(cmd::REG_STATUS, statusReg);
  (void)statusReg;
  st = mapPresenceError(st);
  if (!st.ok()) {
    return st;
  }

  // Re-apply stored configuration
  st = _applyConfig();
  return st;
}

// ===== I2C Transport Wrappers =====

Status RV3032::_i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen, uint8_t* rxBuf, size_t rxLen) {
  if (!_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback null");
  }
  return _config.i2cWriteRead(_config.i2cAddress, txBuf, txLen, rxBuf, rxLen,
                              _config.i2cTimeoutMs, _config.i2cUser);
}

Status RV3032::_i2cWriteRaw(const uint8_t* buf, size_t len) {
  if (!_config.i2cWrite) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback null");
  }
  return _config.i2cWrite(_config.i2cAddress, buf, len,
                          _config.i2cTimeoutMs, _config.i2cUser);
}

Status RV3032::_i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen, uint8_t* rxBuf, size_t rxLen) {
  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen);
  return _updateHealth(st);
}

Status RV3032::_i2cWriteTracked(const uint8_t* buf, size_t len) {
  Status st = _i2cWriteRaw(buf, len);
  return _updateHealth(st);
}

Status RV3032::_readRegisterRaw(uint8_t reg, uint8_t& value) {
  uint8_t tx = reg;
  return _i2cWriteReadRaw(&tx, 1, &value, 1);
}

Status RV3032::_updateHealth(const Status& st) {
  // Treat IN_PROGRESS as success (EEPROM queueing is not a failure)
  bool isSuccess = st.ok() || st.code == Err::IN_PROGRESS;

  if (isSuccess) {
    // Success path
    _lastOkMs = millis();
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      ++_totalSuccess;
    }

    // State transitions only when initialized
    // (during begin(), we track counters but don't transition states)
    if (_initialized) {
      // Transition to READY on first success after init or recovery.
      if (_driverState == DriverState::UNINIT ||
          _driverState == DriverState::DEGRADED ||
          _driverState == DriverState::OFFLINE) {
        _driverState = DriverState::READY;
      }
    }
  } else {
    // Failure path
    _lastError = st;
    _lastErrorMs = millis();
    if (_consecutiveFailures < 0xFFu) {
      ++_consecutiveFailures;
    }
    if (_totalFailures < UINT32_MAX) {
      ++_totalFailures;
    }

    // State transitions only when initialized
    // (during begin(), we track counters but don't transition states)
    if (_initialized) {
      // Transition READY → DEGRADED on first failure
      if (_consecutiveFailures == 1 && _driverState == DriverState::READY) {
        _driverState = DriverState::DEGRADED;
      }

      // Transition DEGRADED → OFFLINE when threshold reached
      if (_consecutiveFailures >= _config.offlineThreshold) {
        _driverState = DriverState::OFFLINE;
      }
    }
  }

  return st;
}

Status RV3032::_applyConfig() {
  // Read current PMU register
  uint8_t coe = 0;
  Status st = readRegister(cmd::REG_EEPROM_PMU, coe);
  if (!st.ok()) return st;

  // Modify backup mode bits
  uint8_t newCoe = static_cast<uint8_t>(coe & ~cmd::PMU_BSM_MASK);
  switch (_config.backupMode) {
    case BackupSwitchMode::Off:
      break;
    case BackupSwitchMode::Level:
      newCoe = static_cast<uint8_t>(newCoe | cmd::PMU_BSM_LEVEL);
      break;
    case BackupSwitchMode::Direct:
      newCoe = static_cast<uint8_t>(newCoe | cmd::PMU_BSM_DIRECT);
      break;
  }

  // Write if changed
  if (newCoe != coe) {
    st = writeEepromRegister(cmd::REG_EEPROM_PMU, newCoe);
    // Treat IN_PROGRESS as success (EEPROM queued)
    if (!st.ok() && st.code != Err::IN_PROGRESS) {
      return st;
    }
  }

  return Status::Ok();
}

// ===== Time/Date Operations =====

Status RV3032::readTime(DateTime& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t buf[7] = {0};
  Status st = readRegs(cmd::REG_SECONDS, buf, sizeof(buf));
  if (!st.ok()) {
    return st;  // readRegs() uses tracked I2C, so health state is already updated
  }

  const uint8_t secReg = static_cast<uint8_t>(buf[0] & 0x7F);
  const uint8_t minReg = static_cast<uint8_t>(buf[1] & 0x7F);
  const uint8_t hourReg = static_cast<uint8_t>(buf[2] & 0x3F);
  const uint8_t wdayReg = static_cast<uint8_t>(buf[3] & 0x07);
  const uint8_t dayReg = static_cast<uint8_t>(buf[4] & 0x3F);
  const uint8_t monthReg = static_cast<uint8_t>(buf[5] & 0x1F);
  const uint8_t yearReg = buf[6];

  if (!isValidBcd(secReg) || !isValidBcd(minReg) || !isValidBcd(hourReg) ||
      !isValidBcd(dayReg) || !isValidBcd(monthReg) || !isValidBcd(yearReg)) {
    // I2C succeeded but data is corrupt - not an I2C failure, so no health update
    return Status::Error(Err::INVALID_DATETIME, "RTC returned invalid BCD");
  }

  out.second = bcdToBin(secReg);
  out.minute = bcdToBin(minReg);
  out.hour = bcdToBin(hourReg);
  out.weekday = wdayReg;
  out.day = bcdToBin(dayReg);
  out.month = bcdToBin(monthReg);
  out.year = static_cast<uint16_t>(2000 + bcdToBin(yearReg));

  if (!isValidDateTime(out)) {
    // I2C succeeded but data is invalid - not an I2C failure, so no health update
    return Status::Error(Err::INVALID_DATETIME, "RTC returned invalid date/time");
  }

  return Status::Ok();
}

Status RV3032::setTime(const DateTime& time) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  // Validate input fields BEFORE computing weekday to avoid calling
  // dateToDays() with out-of-range month/day values.
  if (time.year < 2000 || time.year > 2099 ||
      time.month < 1 || time.month > 12 ||
      time.day < 1 || time.day > daysInMonth(time.year, time.month) ||
      time.hour > 23 || time.minute > 59 || time.second > 59) {
    return Status::Error(Err::INVALID_DATETIME, "Invalid date/time values");
  }

  uint8_t weekday = computeWeekday(time.year, time.month, time.day);

  uint8_t buf[7] = {
    binToBcd(time.second),
    binToBcd(time.minute),
    binToBcd(time.hour),
    static_cast<uint8_t>(weekday & 0x07),
    binToBcd(time.day),
    binToBcd(time.month),
    binToBcd(static_cast<uint8_t>(time.year % 100))
  };

  // Health updated automatically by tracked wrapper
  return writeRegs(cmd::REG_SECONDS, buf, sizeof(buf));
}

Status RV3032::readUnix(uint32_t& out) {
  DateTime dt;
  Status st = readTime(dt);
  if (!st.ok()) {
    return st;
  }

  uint32_t days = dateToDays(dt.year, dt.month, dt.day);
  out = days * 86400UL
      + static_cast<uint32_t>(dt.hour) * 3600UL
      + static_cast<uint32_t>(dt.minute) * 60UL
      + static_cast<uint32_t>(dt.second);

  return Status::Ok();
}

Status RV3032::setUnix(uint32_t ts) {
  DateTime dt;
  if (!unixToDate(ts, dt)) {
    return Status::Error(Err::INVALID_DATETIME, "Unix timestamp out of range");
  }
  return setTime(dt);
}

// ===== Alarm Operations =====

Status RV3032::setAlarmTime(uint8_t minute, uint8_t hour, uint8_t date) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  if (minute > 59 || hour > 23 || date > 31 || date == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid alarm time values");
  }

  // Burst-read all 3 alarm registers (0x08-0x0A) to preserve enable bits
  uint8_t buf[3] = {0};
  Status st = readRegs(cmd::REG_ALARM_MINUTE, buf, sizeof(buf));
  if (!st.ok()) return st;

  buf[0] = static_cast<uint8_t>((buf[0] & 0x80) | binToBcd(minute));
  buf[1] = static_cast<uint8_t>((buf[1] & 0x80) | binToBcd(hour));
  buf[2] = static_cast<uint8_t>((buf[2] & 0x80) | binToBcd(date));

  return writeRegs(cmd::REG_ALARM_MINUTE, buf, sizeof(buf));
}

Status RV3032::setAlarmMatch(bool matchMinute, bool matchHour, bool matchDate) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  // Burst-read all 3 alarm registers (0x08-0x0A) to preserve time values
  uint8_t buf[3] = {0};
  Status st = readRegs(cmd::REG_ALARM_MINUTE, buf, sizeof(buf));
  if (!st.ok()) return st;

  buf[0] = static_cast<uint8_t>((buf[0] & 0x7F) | (matchMinute ? 0 : 0x80));
  buf[1] = static_cast<uint8_t>((buf[1] & 0x7F) | (matchHour ? 0 : 0x80));
  buf[2] = static_cast<uint8_t>((buf[2] & 0x7F) | (matchDate ? 0 : 0x80));

  return writeRegs(cmd::REG_ALARM_MINUTE, buf, sizeof(buf));
}

Status RV3032::getAlarmConfig(AlarmConfig& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  // Burst-read all 3 alarm registers (0x08-0x0A)
  uint8_t buf[3] = {0};
  Status st = readRegs(cmd::REG_ALARM_MINUTE, buf, sizeof(buf));
  if (!st.ok()) return st;

  const uint8_t minReg = buf[0];
  const uint8_t hourReg = buf[1];
  const uint8_t dateReg = buf[2];

  const uint8_t minRaw = static_cast<uint8_t>(minReg & 0x7F);
  const uint8_t hourRaw = static_cast<uint8_t>(hourReg & 0x7F);
  const uint8_t dateRaw = static_cast<uint8_t>(dateReg & 0x7F);
  if (!isValidBcd(minRaw) || !isValidBcd(hourRaw) || !isValidBcd(dateRaw)) {
    return Status::Error(Err::INVALID_PARAM, "Alarm registers contain invalid BCD");
  }

  const uint8_t minute = bcdToBin(minRaw);
  const uint8_t hour = bcdToBin(hourRaw);
  const uint8_t date = bcdToBin(dateRaw);
  if (minute > 59 || hour > 23 || date == 0 || date > 31) {
    return Status::Error(Err::INVALID_PARAM, "Alarm registers out of range");
  }

  out.matchMinute = ((minReg & 0x80) == 0);
  out.matchHour = ((hourReg & 0x80) == 0);
  out.matchDate = ((dateReg & 0x80) == 0);
  out.minute = minute;
  out.hour = hour;
  out.date = date;

  return Status::Ok();
}

Status RV3032::getAlarmFlag(bool& triggered) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(cmd::REG_STATUS, status);
  if (!st.ok()) {
    return st;
  }

  triggered = ((status & (1u << cmd::STATUS_AF_BIT)) != 0);
  return Status::Ok();
}

Status RV3032::clearAlarmFlag() {
  return clearStatus(static_cast<uint8_t>(1u << cmd::STATUS_AF_BIT));
}

Status RV3032::enableAlarmInterrupt(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control2 = 0;
  Status st = readRegister(cmd::REG_CONTROL2, control2);
  if (!st.ok()) {
    return st;
  }

  uint8_t newControl2 = enable ? static_cast<uint8_t>(control2 | (1u << cmd::CTRL2_AIE_BIT))
                               : static_cast<uint8_t>(control2 & ~(1u << cmd::CTRL2_AIE_BIT));

  return writeRegister(cmd::REG_CONTROL2, newControl2);
}

Status RV3032::getAlarmInterruptEnabled(bool& enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control2 = 0;
  Status st = readRegister(cmd::REG_CONTROL2, control2);
  if (!st.ok()) {
    return st;
  }

  enabled = ((control2 & (1u << cmd::CTRL2_AIE_BIT)) != 0);
  return Status::Ok();
}

// ===== Timer Operations =====

Status RV3032::setTimer(uint16_t ticks, TimerFrequency freq, bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (ticks > 0x0FFF) {
    return Status::Error(Err::INVALID_PARAM, "Timer ticks out of range");
  }
  const uint8_t freqRaw = static_cast<uint8_t>(freq);
  if (freqRaw > kMaxTimerFrequency) {
    return Status::Error(Err::INVALID_PARAM, "Timer frequency out of range");
  }

  uint8_t control1 = 0;
  Status st = readRegister(cmd::REG_CONTROL1, control1);
  if (!st.ok()) {
    return st;
  }

  control1 = static_cast<uint8_t>(control1 & ~(cmd::CTRL1_TD_MASK | (1u << cmd::CTRL1_TE_BIT)));
  control1 = static_cast<uint8_t>(control1 | (freqRaw & cmd::CTRL1_TD_MASK));
  if (enable) {
    control1 = static_cast<uint8_t>(control1 | (1u << cmd::CTRL1_TE_BIT));
  }

  uint8_t low = static_cast<uint8_t>(ticks & 0xFF);
  uint8_t currentHigh = 0;
  st = readRegister(cmd::REG_TIMER_HIGH, currentHigh);
  if (!st.ok()) {
    return st;
  }
  uint8_t high = static_cast<uint8_t>((currentHigh & 0xF0u) | ((ticks >> 8) & 0x0Fu));

  st = writeRegister(cmd::REG_CONTROL1, control1);
  if (!st.ok()) {
    return st;
  }

  st = writeRegister(cmd::REG_TIMER_LOW, low);
  if (!st.ok()) return st;
  return writeRegister(cmd::REG_TIMER_HIGH, high);
}

Status RV3032::getTimer(uint16_t& ticks, TimerFrequency& freq, bool& enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control1 = 0, low = 0, high = 0;
  Status st = readRegister(cmd::REG_CONTROL1, control1);
  if (!st.ok()) return st;
  st = readRegister(cmd::REG_TIMER_LOW, low);
  if (!st.ok()) return st;
  st = readRegister(cmd::REG_TIMER_HIGH, high);
  if (!st.ok()) return st;

  ticks = static_cast<uint16_t>((static_cast<uint16_t>(high & 0x0F) << 8) | low);
  freq = static_cast<TimerFrequency>(control1 & cmd::CTRL1_TD_MASK);
  enabled = ((control1 & (1u << cmd::CTRL1_TE_BIT)) != 0);

  return Status::Ok();
}

// ===== Clock Output Operations =====

Status RV3032::setClkoutEnabled(bool enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t coe = 0;
  Status st = readRegister(cmd::REG_EEPROM_PMU, coe);
  if (!st.ok()) {
    return st;
  }

  uint8_t newCoe = enabled ? static_cast<uint8_t>(coe & ~cmd::PMU_CLKOUT_DISABLE)
                           : static_cast<uint8_t>(coe | cmd::PMU_CLKOUT_DISABLE);

  return writeEepromRegister(cmd::REG_EEPROM_PMU, newCoe);
}

Status RV3032::getClkoutEnabled(bool& enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t coe = 0;
  Status st = readRegister(cmd::REG_EEPROM_PMU, coe);
  if (!st.ok()) {
    return st;
  }

  enabled = ((coe & cmd::PMU_CLKOUT_DISABLE) == 0);
  return Status::Ok();
}

Status RV3032::setClkoutFrequency(ClkoutFrequency freq) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  const uint8_t freqRaw = static_cast<uint8_t>(freq);
  if (freqRaw > kMaxClkoutFrequency) {
    return Status::Error(Err::INVALID_PARAM, "CLKOUT frequency out of range");
  }

  uint8_t clkout = 0;
  Status st = readRegister(cmd::REG_EEPROM_CLKOUT2, clkout);
  if (!st.ok()) {
    return st;
  }

  uint8_t newClkout = static_cast<uint8_t>(clkout & ~cmd::CLKOUT_FREQ_MASK);
  newClkout = static_cast<uint8_t>(newClkout | ((freqRaw << cmd::CLKOUT_FREQ_SHIFT) & cmd::CLKOUT_FREQ_MASK));

  return writeEepromRegister(cmd::REG_EEPROM_CLKOUT2, newClkout);
}

Status RV3032::getClkoutFrequency(ClkoutFrequency& freq) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t clkout = 0;
  Status st = readRegister(cmd::REG_EEPROM_CLKOUT2, clkout);
  if (!st.ok()) {
    return st;
  }

  uint8_t value = static_cast<uint8_t>((clkout & cmd::CLKOUT_FREQ_MASK) >> cmd::CLKOUT_FREQ_SHIFT);
  if (value > 3) {
    value = 0;
  }
  freq = static_cast<ClkoutFrequency>(value);

  return Status::Ok();
}

// ===== Calibration Operations =====

Status RV3032::setOffsetPpm(float ppm) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!std::isfinite(ppm)) {
    return Status::Error(Err::INVALID_PARAM, "Offset must be finite");
  }

  float steps = ppm / 0.2384f;
  int value = (steps >= 0.0f) ? static_cast<int>(steps + 0.5f)
                              : static_cast<int>(steps - 0.5f);

  if (value < -32) {
    value = -32;
  } else if (value > 31) {
    value = 31;
  }

  uint8_t current = 0;
  Status st = readRegister(cmd::REG_EEPROM_OFFSET, current);
  if (!st.ok()) {
    return st;
  }

  uint8_t raw = static_cast<uint8_t>(value & 0x3F);
  uint8_t newValue = static_cast<uint8_t>((current & 0xC0) | raw);
  return writeEepromRegister(cmd::REG_EEPROM_OFFSET, newValue);
}

Status RV3032::getOffsetPpm(float& ppm) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t raw = 0;
  Status st = readRegister(cmd::REG_EEPROM_OFFSET, raw);
  if (!st.ok()) {
    return st;
  }

  raw = static_cast<uint8_t>(raw & 0x3F);
  int8_t signedRaw = (raw & 0x20) ? static_cast<int8_t>(raw | 0xC0) : static_cast<int8_t>(raw);
  ppm = static_cast<float>(signedRaw) * 0.2384f;

  return Status::Ok();
}

// ===== Temperature Sensor =====

Status RV3032::readTemperatureC(float& celsius) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t buf[2] = {0};
  Status st = readRegs(cmd::REG_TEMP_LSB, buf, sizeof(buf));
  if (!st.ok()) {
    return st;
  }

  // Combine MSB (integer, two's complement) and LSB upper nibble (fractional)
  // into a 12-bit value using unsigned arithmetic to avoid UB from shifting negatives.
  uint16_t rawU = static_cast<uint16_t>(
      (static_cast<uint16_t>(buf[1]) << 4) | (buf[0] >> 4));
  // Sign-extend 12-bit two's complement to 16-bit
  int16_t raw = (rawU & 0x0800u)
      ? static_cast<int16_t>(rawU | 0xF000u)
      : static_cast<int16_t>(rawU);
  celsius = static_cast<float>(raw) / 16.0f;

  return Status::Ok();
}

// ===== External Event Input =====

Status RV3032::setEviEdge(bool rising) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control = 0;
  Status st = readRegister(cmd::REG_EVI_CONTROL, control);
  if (!st.ok()) {
    return st;
  }

  uint8_t newControl = rising ? static_cast<uint8_t>(control | (1u << cmd::EVI_EB_BIT))
                              : static_cast<uint8_t>(control & ~(1u << cmd::EVI_EB_BIT));

  return writeRegister(cmd::REG_EVI_CONTROL, newControl);
}

Status RV3032::setEviDebounce(EviDebounce debounce) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  const uint8_t debounceRaw = static_cast<uint8_t>(debounce);
  if (debounceRaw > kMaxEviDebounce) {
    return Status::Error(Err::INVALID_PARAM, "EVI debounce out of range");
  }

  uint8_t control = 0;
  Status st = readRegister(cmd::REG_EVI_CONTROL, control);
  if (!st.ok()) {
    return st;
  }

  uint8_t newControl = static_cast<uint8_t>(control & ~cmd::EVI_DB_MASK);
  newControl = static_cast<uint8_t>(newControl | ((debounceRaw << cmd::EVI_DB_SHIFT) & cmd::EVI_DB_MASK));

  return writeRegister(cmd::REG_EVI_CONTROL, newControl);
}

Status RV3032::setEviOverwrite(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control = 0;
  Status st = readRegister(cmd::REG_TS_CONTROL, control);
  if (!st.ok()) {
    return st;
  }

  uint8_t newControl = enable ? static_cast<uint8_t>(control | (1u << cmd::TS_OVERWRITE_BIT))
                              : static_cast<uint8_t>(control & ~(1u << cmd::TS_OVERWRITE_BIT));

  return writeRegister(cmd::REG_TS_CONTROL, newControl);
}

Status RV3032::getEviConfig(EviConfig& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t evi = 0, ts = 0;
  Status st = readRegister(cmd::REG_EVI_CONTROL, evi);
  if (!st.ok()) return st;
  st = readRegister(cmd::REG_TS_CONTROL, ts);
  if (!st.ok()) return st;

  out.rising = ((evi & (1u << cmd::EVI_EB_BIT)) != 0);
  out.debounce = static_cast<EviDebounce>((evi & cmd::EVI_DB_MASK) >> cmd::EVI_DB_SHIFT);
  out.overwrite = ((ts & (1u << cmd::TS_OVERWRITE_BIT)) != 0);

  return Status::Ok();
}

// ===== Status Operations =====

Status RV3032::readStatus(uint8_t& status) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  return readRegister(cmd::REG_STATUS, status);
}

Status RV3032::readStatusFlags(StatusFlags& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(cmd::REG_STATUS, status);
  if (!st.ok()) {
    return st;
  }

  out.tempHigh = (status & (1u << cmd::STATUS_THF_BIT)) != 0;
  out.tempLow = (status & (1u << cmd::STATUS_TLF_BIT)) != 0;
  out.update = (status & (1u << cmd::STATUS_UF_BIT)) != 0;
  out.timer = (status & (1u << cmd::STATUS_TF_BIT)) != 0;
  out.alarm = (status & (1u << cmd::STATUS_AF_BIT)) != 0;
  out.event = (status & (1u << cmd::STATUS_EVF_BIT)) != 0;
  out.powerOnReset = (status & (1u << cmd::STATUS_PORF_BIT)) != 0;
  out.voltageLow = (status & (1u << cmd::STATUS_VLF_BIT)) != 0;

  return Status::Ok();
}

Status RV3032::clearStatus(uint8_t mask) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(cmd::REG_STATUS, status);
  if (!st.ok()) {
    return st;
  }

  status = static_cast<uint8_t>(status & ~mask);
  return writeRegister(cmd::REG_STATUS, status);
}

// ===== Low-Level Operations =====

Status RV3032::readRegister(uint8_t reg, uint8_t& value) {
  uint8_t buf = 0;
  Status st = readRegs(reg, &buf, 1);
  if (st.ok()) {
    value = buf;
  }
  return st;
}

Status RV3032::writeRegister(uint8_t reg, uint8_t value) {
  return writeRegs(reg, &value, 1);
}

// ===== Static Utility Functions =====

bool RV3032::isValidDateTime(const DateTime& time) {
  if (time.year < 2000 || time.year > 2099) {
    return false;
  }
  if (time.month < 1 || time.month > 12) {
    return false;
  }
  if (time.day < 1 || time.day > daysInMonth(time.year, time.month)) {
    return false;
  }
  if (time.hour > 23 || time.minute > 59 || time.second > 59) {
    return false;
  }
  if (time.weekday > 6) {
    return false;
  }
  return true;
}

uint8_t RV3032::computeWeekday(uint16_t year, uint8_t month, uint8_t day) {
  uint32_t days = dateToDays(year, month, day);
  return static_cast<uint8_t>((days + 4) % 7);
}

bool RV3032::parseBuildTime(DateTime& out) {
  const char* dateStr = __DATE__;
  const char* timeStr = __TIME__;
  if (!dateStr || !timeStr) {
    return false;
  }

  char monthStr[4] = {0};
  int day = 0;
  int year = 0;
  if (sscanf(dateStr, "%3s %d %d", monthStr, &day, &year) != 3) {
    return false;
  }

  int hour = 0;
  int minute = 0;
  int second = 0;
  if (sscanf(timeStr, "%d:%d:%d", &hour, &minute, &second) != 3) {
    return false;
  }

  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* pos = strstr(months, monthStr);
  if (!pos) {
    return false;
  }

  uint8_t month = static_cast<uint8_t>((pos - months) / 3 + 1);

  out.year = static_cast<uint16_t>(year);
  out.month = month;
  out.day = static_cast<uint8_t>(day);
  out.hour = static_cast<uint8_t>(hour);
  out.minute = static_cast<uint8_t>(minute);
  out.second = static_cast<uint8_t>(second);
  out.weekday = computeWeekday(out.year, out.month, out.day);

  return isValidDateTime(out);
}

// ===== Private Helper Functions =====

Status RV3032::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
  if (!_initialized && !_beginInProgress) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!buf || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C read parameters");
  }
  // Limit reads to avoid oversized transfers.
  if (len > 255) {
    return Status::Error(Err::INVALID_PARAM, "I2C read length too large");
  }

  uint8_t tx = reg;
  // Use tracked wrapper - health is updated automatically
  return _i2cWriteReadTracked(&tx, 1, buf, len);
}

Status RV3032::writeRegs(uint8_t reg, const uint8_t* buf, size_t len) {
  if (!_initialized && !_beginInProgress) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!buf || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C write parameters");
  }

  // Max write buffer: 1 byte for register address + 15 bytes data = 16 bytes total.
  // This covers all RV3032 multi-byte writes (time registers = 7 bytes, etc.)
  static constexpr size_t kMaxWriteLen = 16;
  if (len > (kMaxWriteLen - 1)) {  // len is data only, we add 1 for register address
    return Status::Error(Err::INVALID_PARAM, "I2C write length exceeds 15 bytes");
  }

  uint8_t tx[kMaxWriteLen] = {0};
  tx[0] = reg;
  std::memcpy(&tx[1], buf, len);
  // Use tracked wrapper - health is updated automatically
  return _i2cWriteTracked(tx, len + 1);
}

Status RV3032::writeEepromRegister(uint8_t reg, uint8_t value) {
  // Read current value
  uint8_t current = 0;
  Status st = readRegister(reg, current);
  if (!st.ok()) {
    return st;
  }

  // Skip if no change needed
  if (current == value) {
    return Status::Ok();
  }

  // Write to RAM register first
  st = writeRegister(reg, value);
  if (!st.ok()) {
    return st;
  }

  // If EEPROM writes disabled, stop here (RAM-only mode)
  if (!_config.enableEepromWrites) {
    return Status::Ok();
  }

  return queueEepromUpdate(reg, value, millis());
}

void RV3032::processEeprom(uint32_t now_ms) {
  if (!_config.enableEepromWrites) {
    _eeprom.state = EepromState::Idle;
    _eeprom.queueHead = 0;
    _eeprom.queueTail = 0;
    _eeprom.queueCount = 0;
    return;
  }

  if (_eeprom.state == EepromState::Idle) {
    uint8_t nextReg = 0;
    uint8_t nextValue = 0;
    if (eepromQueuePop(nextReg, nextValue)) {
      startEepromUpdate(nextReg, nextValue, now_ms);
    }
    return;
  }

  auto finishEepromOperation = [&](const Status& finalStatus) {
    _eepromLastStatus = finalStatus;
    if (_eeprom.countPending) {
      if (finalStatus.ok()) {
        _eepromWriteCount++;
      } else {
        _eepromWriteFailures++;
      }
      _eeprom.countPending = false;
    }
    _eeprom.control1Valid = false;
    _eeprom.state = EepromState::Idle;

    uint8_t nextReg = 0;
    uint8_t nextValue = 0;
    if (eepromQueuePop(nextReg, nextValue)) {
      startEepromUpdate(nextReg, nextValue, now_ms);
    }
  };

  Status st;
  bool busy = false;
  bool failed = false;

  switch (_eeprom.state) {
    case EepromState::ReadControl1:
      st = readRegister(cmd::REG_CONTROL1, _eeprom.control1);
      if (!st.ok()) {
        finishEepromOperation(st);
        break;
      }
      _eeprom.control1Valid = true;
      _eeprom.state = EepromState::EnableEerd;
      break;
    case EepromState::EnableEerd:
      st = writeRegister(cmd::REG_CONTROL1, static_cast<uint8_t>(_eeprom.control1 | (1u << cmd::CTRL1_EERD_BIT)));
      if (!st.ok()) {
        _eepromLastStatus = st;
        _eeprom.state = EepromState::RestoreControl1;
        break;
      }
      _eeprom.state = EepromState::WriteAddr;
      break;
    case EepromState::WriteAddr:
      st = writeRegister(cmd::REG_EE_ADDRESS, _eeprom.reg);
      if (!st.ok()) {
        _eepromLastStatus = st;
        _eeprom.state = EepromState::RestoreControl1;
        break;
      }
      _eeprom.state = EepromState::WriteData;
      break;
    case EepromState::WriteData:
      st = writeRegister(cmd::REG_EE_DATA, _eeprom.value);
      if (!st.ok()) {
        _eepromLastStatus = st;
        _eeprom.state = EepromState::RestoreControl1;
        break;
      }
      _eeprom.deadlineMs = now_ms + kEepromPreCmdTimeoutMs;
      _eeprom.state = EepromState::WaitReadyPreCmd;
      break;
    case EepromState::WaitReadyPreCmd:
      st = readEepromFlags(busy, failed);
      if (!st.ok()) {
        _eepromLastStatus = st;
        _eeprom.state = EepromState::RestoreControl1;
        break;
      }
      if (!busy) {
        _eeprom.state = EepromState::WriteCmd;
        break;
      }
      if (hasDeadlinePassed(now_ms, _eeprom.deadlineMs)) {
        _eepromLastStatus = Status::Error(Err::TIMEOUT, "EEPROM busy timeout");
        _eeprom.state = EepromState::RestoreControl1;
      }
      break;
    case EepromState::WriteCmd:
      st = writeRegister(cmd::REG_EE_COMMAND, cmd::EEPROM_CMD_UPDATE);
      if (!st.ok()) {
        _eepromLastStatus = st;
        _eeprom.state = EepromState::RestoreControl1;
        break;
      }
      _eeprom.deadlineMs = now_ms + _config.eepromTimeoutMs;
      _eeprom.state = EepromState::WaitReadyPostCmd;
      break;
    case EepromState::WaitReadyPostCmd:
      st = readEepromFlags(busy, failed);
      if (!st.ok()) {
        _eepromLastStatus = st;
        _eeprom.state = EepromState::RestoreControl1;
        break;
      }
      if (!busy) {
        _eeprom.state = EepromState::RestoreControl1;
        if (failed) {
          _eepromLastStatus = Status::Error(Err::EEPROM_WRITE_FAILED, "EEPROM write failed");
        } else {
          _eepromLastStatus = Status::Ok();
        }
        break;
      }
      if (hasDeadlinePassed(now_ms, _eeprom.deadlineMs)) {
        _eepromLastStatus = Status::Error(Err::TIMEOUT, "EEPROM write timeout");
        _eeprom.state = EepromState::RestoreControl1;
      }
      break;
    case EepromState::RestoreControl1: {
      Status finalStatus = _eepromLastStatus;
      if (_eeprom.control1Valid) {
        uint8_t restore = static_cast<uint8_t>(_eeprom.control1 & ~(1u << cmd::CTRL1_EERD_BIT));
        st = writeRegister(cmd::REG_CONTROL1, restore);
        if (!st.ok() && finalStatus.ok()) {
          finalStatus = st;
        }
      }
      finishEepromOperation(finalStatus);
      break;
    }
    case EepromState::Idle:
    default:
      _eeprom.state = EepromState::Idle;
      break;
  }
}

Status RV3032::queueEepromUpdate(uint8_t reg, uint8_t value, uint32_t now_ms) {
  if (_eeprom.state != EepromState::Idle || _eeprom.queueCount > 0) {
    // State machine busy - try to queue
    if (!eepromQueuePush(reg, value)) {
      return Status::Error(Err::QUEUE_FULL, "EEPROM queue full");
    }
    return Status::Error(Err::IN_PROGRESS, "EEPROM update queued");
  }

  // Idle - start immediately
  return startEepromUpdate(reg, value, now_ms);
}

Status RV3032::startEepromUpdate(uint8_t reg, uint8_t value, uint32_t now_ms) {
  _eeprom.reg = reg;
  _eeprom.value = value;
  _eeprom.control1Valid = false;
  _eeprom.countPending = true;
  _eeprom.deadlineMs = now_ms + kEepromPreCmdTimeoutMs;
  _eeprom.state = EepromState::ReadControl1;
  _eepromLastStatus = Status::Ok();
  return Status::Error(Err::IN_PROGRESS, "EEPROM update in progress");
}

bool RV3032::eepromQueuePush(uint8_t reg, uint8_t value) {
  if (_eeprom.queueCount >= kEepromQueueSize) {
    return false;  // Queue full
  }
  
  _eeprom.queue[_eeprom.queueHead].reg = reg;
  _eeprom.queue[_eeprom.queueHead].value = value;
  _eeprom.queueHead = (_eeprom.queueHead + 1) % kEepromQueueSize;
  _eeprom.queueCount++;
  return true;
}

bool RV3032::eepromQueuePop(uint8_t& reg, uint8_t& value) {
  if (_eeprom.queueCount == 0) {
    return false;  // Queue empty
  }
  
  reg = _eeprom.queue[_eeprom.queueTail].reg;
  value = _eeprom.queue[_eeprom.queueTail].value;
  _eeprom.queueTail = (_eeprom.queueTail + 1) % kEepromQueueSize;
  _eeprom.queueCount--;
  return true;
}

Status RV3032::readEepromFlags(bool& busy, bool& failed) {
  // EEPROM busy and error flags are in Temperature LSBs register (0x0E).
  uint8_t tempLsb = 0;
  Status st = readRegister(cmd::REG_TEMP_LSB, tempLsb);
  if (!st.ok()) {
    return st;
  }
  busy = ((tempLsb & (1u << cmd::EEPROM_BUSY_BIT)) != 0);
  failed = ((tempLsb & (1u << cmd::EEPROM_ERROR_BIT)) != 0);
  return Status::Ok();
}

// ===== Conversion Helper Functions =====

bool RV3032::isValidBcd(uint8_t v) {
  uint8_t low = static_cast<uint8_t>(v & 0x0F);
  uint8_t high = static_cast<uint8_t>((v >> 4) & 0x0F);
  return (low <= 9) && (high <= 9);
}

uint8_t RV3032::bcdToBin(uint8_t v) {
  return static_cast<uint8_t>((v >> 4) * 10 + (v & 0x0F));
}

uint8_t RV3032::binToBcd(uint8_t v) {
  // Precondition: v must be <= 99. All call sites validate before calling.
  // Return 0x99 (max valid BCD) as a safe, detectable sentinel if violated.
  if (v > 99) { return 0x99; }
  return static_cast<uint8_t>(((v / 10) << 4) | (v % 10));
}

bool RV3032::isLeapYear(uint16_t year) {
  return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

uint8_t RV3032::daysInMonth(uint16_t year, uint8_t month) {
  static constexpr uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year)) {
    return 29;
  }
  if (month == 0 || month > 12) {
    return 0;
  }
  return kDays[month - 1];
}

uint32_t RV3032::dateToDays(uint16_t year, uint8_t month, uint8_t day) {
  uint32_t days = 0;

  // Count days from 1970 to current year
  for (uint16_t y = 1970; y < year; ++y) {
    days += isLeapYear(y) ? 366 : 365;
  }

  // Days before current month
  static constexpr uint16_t kDaysBeforeMonth[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };
  if (month >= 1 && month <= 12) {
    days += kDaysBeforeMonth[month - 1];
  }

  // Add leap day if after February in leap year
  if (month > 2 && isLeapYear(year)) {
    days += 1;
  }

  // Add day of month
  if (day > 0) {
    days += static_cast<uint32_t>(day - 1);
  }

  return days;
}

bool RV3032::unixToDate(uint32_t ts, DateTime& out) {
  // Reject timestamps before 2000-01-01 (RTC valid range is 2000-2099)
  if (ts < kEpoch2000) {
    return false;
  }

  uint32_t days = ts / 86400UL;
  uint32_t rem = ts % 86400UL;

  // Find year
  uint16_t year = 1970;
  for (; year <= 2099; ++year) {
    uint16_t daysInYear = isLeapYear(year) ? 366 : 365;
    if (days < daysInYear) {
      break;
    }
    days -= daysInYear;
  }
  if (year > 2099 || year < 2000) {
    return false;  // Out of RTC range
  }

  // Find month
  uint8_t month = 1;
  for (; month <= 12; ++month) {
    uint8_t dim = daysInMonth(year, month);
    if (days < dim) {
      break;
    }
    days -= dim;
  }
  if (month > 12) {
    return false;
  }

  out.year = year;
  out.month = month;
  out.day = static_cast<uint8_t>(days + 1);
  out.hour = static_cast<uint8_t>(rem / 3600UL);
  rem %= 3600UL;
  out.minute = static_cast<uint8_t>(rem / 60UL);
  out.second = static_cast<uint8_t>(rem % 60UL);
  out.weekday = computeWeekday(out.year, out.month, out.day);

  return true;
}

Status RV3032::readValidity(ValidityFlags& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(cmd::REG_STATUS, status);
  if (!st.ok()) {
    return st;
  }

  uint8_t tempLsb = 0;
  st = readRegister(cmd::REG_TEMP_LSB, tempLsb);
  if (!st.ok()) {
    return st;
  }

  out.voltageLow = (status & (1u << cmd::STATUS_VLF_BIT)) != 0;
  out.powerOnReset = (status & (1u << cmd::STATUS_PORF_BIT)) != 0;
  out.backupSwitched = (tempLsb & (1u << cmd::TEMP_BSF_BIT)) != 0;
  out.timeInvalid = out.powerOnReset || out.voltageLow;

  return Status::Ok();
}

Status RV3032::clearPowerOnResetFlag() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(cmd::REG_STATUS, status);
  if (!st.ok()) {
    return st;
  }

  status = static_cast<uint8_t>(status & ~(1u << cmd::STATUS_PORF_BIT));

  return writeRegister(cmd::REG_STATUS, status);
}

Status RV3032::clearVoltageLowFlag() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(cmd::REG_STATUS, status);
  if (!st.ok()) {
    return st;
  }

  status = static_cast<uint8_t>(status & ~(1u << cmd::STATUS_VLF_BIT));

  return writeRegister(cmd::REG_STATUS, status);
}

Status RV3032::clearBackupSwitchFlag() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t tempLsb = 0;
  Status st = readRegister(cmd::REG_TEMP_LSB, tempLsb);
  if (!st.ok()) {
    return st;
  }

  // Clear BSF (bit 0 in REG_TEMP_LSB)
  tempLsb = static_cast<uint8_t>(tempLsb & ~(1u << cmd::TEMP_BSF_BIT));

  return writeRegister(cmd::REG_TEMP_LSB, tempLsb);
}

// ===== Public Static Conversion Functions =====

uint8_t RV3032::bcdToBinary(uint8_t bcd) {
  return bcdToBin(bcd);
}

uint8_t RV3032::binaryToBcd(uint8_t bin) {
  return binToBcd(bin);
}

bool RV3032::unixToDateTime(uint32_t ts, DateTime& out) {
  return unixToDate(ts, out);
}

bool RV3032::dateTimeToUnix(const DateTime& time, uint32_t& out) {
  if (!isValidDateTime(time)) {
    return false;
  }
  uint32_t days = dateToDays(time.year, time.month, time.day);
  out = days * 86400UL
      + static_cast<uint32_t>(time.hour) * 3600UL
      + static_cast<uint32_t>(time.minute) * 60UL
      + static_cast<uint32_t>(time.second);
  return true;
}

}  // namespace RV3032
