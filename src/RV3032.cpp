/**
 * @file RV3032.cpp
 * @brief Implementation of RV-3032-C7 RTC driver
 */

#include "RV3032/RV3032.h"
#include <Arduino.h>
#include <cstring>

namespace RV3032 {

// Register addresses
namespace {
constexpr uint8_t kTimeReg = 0x01;
constexpr uint8_t kRegAlarmMinute = 0x08;
constexpr uint8_t kRegAlarmHour = 0x09;
constexpr uint8_t kRegAlarmDate = 0x0A;
constexpr uint8_t kRegTimerLow = 0x0B;
constexpr uint8_t kRegTimerHigh = 0x0C;
constexpr uint8_t kRegStatus = 0x0D;
constexpr uint8_t kRegTempLsb = 0x0E;
constexpr uint8_t kRegTempMsb = 0x0F;
constexpr uint8_t kRegControl1 = 0x10;
constexpr uint8_t kRegControl2 = 0x11;
constexpr uint8_t kRegControl3 = 0x12;
constexpr uint8_t kRegTsControl = 0x13;
constexpr uint8_t kRegEviControl = 0x15;
constexpr uint8_t kRegCoe = 0xC0;
constexpr uint8_t kRegOffset = 0xC1;
constexpr uint8_t kRegClkout = 0xC3;
constexpr uint8_t kRegEeAddr = 0x3D;
constexpr uint8_t kRegEeData = 0x3E;
constexpr uint8_t kRegEeCmd = 0x3F;

constexpr uint8_t kStatusAlarmBit = 3;

constexpr uint8_t kControl1EerdBit = 2;
constexpr uint8_t kControl1TeBit = 3;
constexpr uint8_t kControl1TdMask = 0x03;

constexpr uint8_t kControl2AieBit = 3;

constexpr uint8_t kCoeClkoutDisable = 0x40;
constexpr uint8_t kCoeBsmMask = 0x30;
constexpr uint8_t kCoeBsmLevel = 0x20;
constexpr uint8_t kCoeBsmDirect = 0x10;

constexpr uint8_t kClkoutFreqMask = 0x60;
constexpr uint8_t kClkoutFreqShift = 5;

constexpr uint8_t kEviEdgeBit = 6;
constexpr uint8_t kEviDebounceMask = 0x30;
constexpr uint8_t kEviDebounceShift = 4;
constexpr uint8_t kTsOverwriteBit = 2;

constexpr uint8_t kEepromBusyBit = 0x04;
constexpr uint8_t kEepromErrorBit = 0x08;
constexpr uint8_t kEepromUpdateCmd = 0x21;
constexpr uint32_t kEepromPreCmdTimeoutMs = 50;

/// @brief Check if deadline has passed, with wraparound-safe comparison.
/// Uses signed arithmetic to handle millis() wraparound (~49 days).
bool hasDeadlinePassed(uint32_t now_ms, uint32_t deadline_ms) {
  return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}
}  // namespace

// ===== Lifecycle Functions =====

Status RV3032::begin(const Config& config) {
  _config = config;
  _initialized = false;
  _eeprom = EepromOp{};
  _eepromLastStatus = Status::Ok();

  // Validate configuration
  if (!_config.i2cWrite || !_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C transport callbacks are null");
  }
  if (_config.i2cTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "I2C timeout must be > 0");
  }
  if (_config.enableEepromWrites && _config.eepromTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "EEPROM timeout must be > 0");
  }
  if (_config.enableEepromWrites && _config.i2cTimeoutMs < kEepromPreCmdTimeoutMs) {
    return Status::Error(Err::INVALID_CONFIG, "I2C timeout must be >= 50ms for EEPROM writes");
  }

  // Test I2C communication (read status register)
  uint8_t status = 0;
  Status st = readRegister(kRegStatus, status);
  (void)status;
  if (!st.ok()) {
    if (st.code == Err::I2C_ERROR || st.code == Err::TIMEOUT) {
      return Status::Error(Err::DEVICE_NOT_FOUND, "RTC not responding on I2C", st.detail);
    }
    return st;
  }

  // Apply backup switching mode
  uint8_t coe = 0;
  st = readRegister(kRegCoe, coe);
  if (!st.ok()) {
    return st;
  }

  uint8_t newCoe = static_cast<uint8_t>(coe & ~kCoeBsmMask);
  switch (_config.backupMode) {
    case BackupSwitchMode::Off:
      break;
    case BackupSwitchMode::Level:
      newCoe = static_cast<uint8_t>(newCoe | kCoeBsmLevel);
      break;
    case BackupSwitchMode::Direct:
      newCoe = static_cast<uint8_t>(newCoe | kCoeBsmDirect);
      break;
  }

  if (newCoe != coe) {
    st = writeEepromRegister(kRegCoe, newCoe);
    if (!st.ok() && st.code != Err::IN_PROGRESS) {
      return st;
    }
    // If IN_PROGRESS, EEPROM work is queued but initialization succeeds
  }

  _initialized = true;
  return Status::Ok();  // Always return OK (caller can check getEepromStatus() for pending work)
}

void RV3032::tick(uint32_t now_ms) {
  if (!_initialized) {
    return;
  }
  processEeprom(now_ms);
}

void RV3032::end() {
  _initialized = false;
  _eeprom = EepromOp{};
  _eepromLastStatus = Status::Ok();
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

// ===== Time/Date Operations =====

Status RV3032::readTime(DateTime& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t buf[7] = {0};
  Status st = readRegs(kTimeReg, buf, sizeof(buf));
  if (!st.ok()) {
    return st;
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
    return Status::Error(Err::INVALID_DATETIME, "RTC returned invalid date/time");
  }

  return Status::Ok();
}

Status RV3032::setTime(const DateTime& time) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t weekday = computeWeekday(time.year, time.month, time.day);
  DateTime validated = time;
  validated.weekday = weekday;
  if (!isValidDateTime(validated)) {
    return Status::Error(Err::INVALID_DATETIME, "Invalid date/time values");
  }

  uint8_t buf[7] = {
    binToBcd(time.second),
    binToBcd(time.minute),
    binToBcd(time.hour),
    static_cast<uint8_t>(weekday & 0x07),
    binToBcd(time.day),
    binToBcd(time.month),
    binToBcd(static_cast<uint8_t>(time.year % 100))
  };

  return writeRegs(kTimeReg, buf, sizeof(buf));
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

  uint8_t minReg = 0, hourReg = 0, dateReg = 0;
  Status st = readRegister(kRegAlarmMinute, minReg);
  if (!st.ok()) return st;
  st = readRegister(kRegAlarmHour, hourReg);
  if (!st.ok()) return st;
  st = readRegister(kRegAlarmDate, dateReg);
  if (!st.ok()) return st;

  minReg = static_cast<uint8_t>((minReg & 0x80) | binToBcd(minute));
  hourReg = static_cast<uint8_t>((hourReg & 0x80) | binToBcd(hour));
  dateReg = static_cast<uint8_t>((dateReg & 0x80) | binToBcd(date));

  st = writeRegister(kRegAlarmMinute, minReg);
  if (!st.ok()) return st;
  st = writeRegister(kRegAlarmHour, hourReg);
  if (!st.ok()) return st;
  return writeRegister(kRegAlarmDate, dateReg);
}

Status RV3032::setAlarmMatch(bool matchMinute, bool matchHour, bool matchDate) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t minReg = 0, hourReg = 0, dateReg = 0;
  Status st = readRegister(kRegAlarmMinute, minReg);
  if (!st.ok()) return st;
  st = readRegister(kRegAlarmHour, hourReg);
  if (!st.ok()) return st;
  st = readRegister(kRegAlarmDate, dateReg);
  if (!st.ok()) return st;

  minReg = static_cast<uint8_t>((minReg & 0x7F) | (matchMinute ? 0 : 0x80));
  hourReg = static_cast<uint8_t>((hourReg & 0x7F) | (matchHour ? 0 : 0x80));
  dateReg = static_cast<uint8_t>((dateReg & 0x7F) | (matchDate ? 0 : 0x80));

  st = writeRegister(kRegAlarmMinute, minReg);
  if (!st.ok()) return st;
  st = writeRegister(kRegAlarmHour, hourReg);
  if (!st.ok()) return st;
  return writeRegister(kRegAlarmDate, dateReg);
}

Status RV3032::getAlarmConfig(AlarmConfig& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t minReg = 0, hourReg = 0, dateReg = 0;
  Status st = readRegister(kRegAlarmMinute, minReg);
  if (!st.ok()) return st;
  st = readRegister(kRegAlarmHour, hourReg);
  if (!st.ok()) return st;
  st = readRegister(kRegAlarmDate, dateReg);
  if (!st.ok()) return st;

  out.matchMinute = ((minReg & 0x80) == 0);
  out.matchHour = ((hourReg & 0x80) == 0);
  out.matchDate = ((dateReg & 0x80) == 0);
  out.minute = bcdToBin(static_cast<uint8_t>(minReg & 0x7F));
  out.hour = bcdToBin(static_cast<uint8_t>(hourReg & 0x7F));
  out.date = bcdToBin(static_cast<uint8_t>(dateReg & 0x7F));

  return Status::Ok();
}

Status RV3032::getAlarmFlag(bool& triggered) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(kRegStatus, status);
  if (!st.ok()) {
    return st;
  }

  triggered = ((status & (1u << kStatusAlarmBit)) != 0);
  return Status::Ok();
}

Status RV3032::clearAlarmFlag() {
  return clearStatus(static_cast<uint8_t>(1u << kStatusAlarmBit));
}

Status RV3032::enableAlarmInterrupt(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control2 = 0;
  Status st = readRegister(kRegControl2, control2);
  if (!st.ok()) {
    return st;
  }

  uint8_t newControl2 = enable ? static_cast<uint8_t>(control2 | (1u << kControl2AieBit))
                               : static_cast<uint8_t>(control2 & ~(1u << kControl2AieBit));

  return writeRegister(kRegControl2, newControl2);
}

Status RV3032::getAlarmInterruptEnabled(bool& enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control2 = 0;
  Status st = readRegister(kRegControl2, control2);
  if (!st.ok()) {
    return st;
  }

  enabled = ((control2 & (1u << kControl2AieBit)) != 0);
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

  uint8_t control1 = 0;
  Status st = readRegister(kRegControl1, control1);
  if (!st.ok()) {
    return st;
  }

  control1 = static_cast<uint8_t>(control1 & ~(kControl1TdMask | (1u << kControl1TeBit)));
  control1 = static_cast<uint8_t>(control1 | (static_cast<uint8_t>(freq) & kControl1TdMask));
  if (enable) {
    control1 = static_cast<uint8_t>(control1 | (1u << kControl1TeBit));
  }

  st = writeRegister(kRegControl1, control1);
  if (!st.ok()) {
    return st;
  }

  uint8_t low = static_cast<uint8_t>(ticks & 0xFF);
  uint8_t high = static_cast<uint8_t>((ticks >> 8) & 0x0F);

  st = writeRegister(kRegTimerLow, low);
  if (!st.ok()) return st;
  return writeRegister(kRegTimerHigh, high);
}

Status RV3032::getTimer(uint16_t& ticks, TimerFrequency& freq, bool& enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control1 = 0, low = 0, high = 0;
  Status st = readRegister(kRegControl1, control1);
  if (!st.ok()) return st;
  st = readRegister(kRegTimerLow, low);
  if (!st.ok()) return st;
  st = readRegister(kRegTimerHigh, high);
  if (!st.ok()) return st;

  ticks = static_cast<uint16_t>((static_cast<uint16_t>(high & 0x0F) << 8) | low);
  freq = static_cast<TimerFrequency>(control1 & kControl1TdMask);
  enabled = ((control1 & (1u << kControl1TeBit)) != 0);

  return Status::Ok();
}

// ===== Clock Output Operations =====

Status RV3032::setClkoutEnabled(bool enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t coe = 0;
  Status st = readRegister(kRegCoe, coe);
  if (!st.ok()) {
    return st;
  }

  uint8_t newCoe = enabled ? static_cast<uint8_t>(coe & ~kCoeClkoutDisable)
                           : static_cast<uint8_t>(coe | kCoeClkoutDisable);

  return writeEepromRegister(kRegCoe, newCoe);
}

Status RV3032::getClkoutEnabled(bool& enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t coe = 0;
  Status st = readRegister(kRegCoe, coe);
  if (!st.ok()) {
    return st;
  }

  enabled = ((coe & kCoeClkoutDisable) == 0);
  return Status::Ok();
}

Status RV3032::setClkoutFrequency(ClkoutFrequency freq) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t clkout = 0;
  Status st = readRegister(kRegClkout, clkout);
  if (!st.ok()) {
    return st;
  }

  uint8_t newClkout = static_cast<uint8_t>(clkout & ~kClkoutFreqMask);
  newClkout = static_cast<uint8_t>(newClkout | ((static_cast<uint8_t>(freq) << kClkoutFreqShift) & kClkoutFreqMask));

  return writeEepromRegister(kRegClkout, newClkout);
}

Status RV3032::getClkoutFrequency(ClkoutFrequency& freq) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t clkout = 0;
  Status st = readRegister(kRegClkout, clkout);
  if (!st.ok()) {
    return st;
  }

  uint8_t value = static_cast<uint8_t>((clkout & kClkoutFreqMask) >> kClkoutFreqShift);
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

  float steps = ppm / 0.2384f;
  int value = (steps >= 0.0f) ? static_cast<int>(steps + 0.5f)
                              : static_cast<int>(steps - 0.5f);

  if (value < -32) {
    value = -32;
  } else if (value > 31) {
    value = 31;
  }

  uint8_t current = 0;
  Status st = readRegister(kRegOffset, current);
  if (!st.ok()) {
    return st;
  }

  uint8_t raw = static_cast<uint8_t>(value & 0x3F);
  uint8_t newValue = static_cast<uint8_t>((current & 0xC0) | raw);
  return writeEepromRegister(kRegOffset, newValue);
}

Status RV3032::getOffsetPpm(float& ppm) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t raw = 0;
  Status st = readRegister(kRegOffset, raw);
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
  Status st = readRegs(kRegTempLsb, buf, sizeof(buf));
  if (!st.ok()) {
    return st;
  }

  uint8_t lsb = buf[0];
  int8_t msb = static_cast<int8_t>(buf[1]);
  int16_t raw = static_cast<int16_t>(static_cast<int16_t>(msb) << 4)
                | static_cast<int16_t>(lsb >> 4);
  celsius = static_cast<float>(raw) / 16.0f;

  return Status::Ok();
}

// ===== External Event Input =====

Status RV3032::setEviEdge(bool rising) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control = 0;
  Status st = readRegister(kRegEviControl, control);
  if (!st.ok()) {
    return st;
  }

  uint8_t newControl = rising ? static_cast<uint8_t>(control | (1u << kEviEdgeBit))
                              : static_cast<uint8_t>(control & ~(1u << kEviEdgeBit));

  return writeRegister(kRegEviControl, newControl);
}

Status RV3032::setEviDebounce(EviDebounce debounce) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control = 0;
  Status st = readRegister(kRegEviControl, control);
  if (!st.ok()) {
    return st;
  }

  uint8_t newControl = static_cast<uint8_t>(control & ~kEviDebounceMask);
  newControl = static_cast<uint8_t>(newControl | ((static_cast<uint8_t>(debounce) << kEviDebounceShift) & kEviDebounceMask));

  return writeRegister(kRegEviControl, newControl);
}

Status RV3032::setEviOverwrite(bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control = 0;
  Status st = readRegister(kRegTsControl, control);
  if (!st.ok()) {
    return st;
  }

  uint8_t newControl = enable ? static_cast<uint8_t>(control | (1u << kTsOverwriteBit))
                              : static_cast<uint8_t>(control & ~(1u << kTsOverwriteBit));

  return writeRegister(kRegTsControl, newControl);
}

Status RV3032::getEviConfig(EviConfig& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t evi = 0, ts = 0;
  Status st = readRegister(kRegEviControl, evi);
  if (!st.ok()) return st;
  st = readRegister(kRegTsControl, ts);
  if (!st.ok()) return st;

  out.rising = ((evi & (1u << kEviEdgeBit)) != 0);
  out.debounce = static_cast<EviDebounce>((evi & kEviDebounceMask) >> kEviDebounceShift);
  out.overwrite = ((ts & (1u << kTsOverwriteBit)) != 0);

  return Status::Ok();
}

// ===== Status Operations =====

Status RV3032::readStatus(uint8_t& status) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  return readRegister(kRegStatus, status);
}

Status RV3032::clearStatus(uint8_t mask) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(kRegStatus, status);
  if (!st.ok()) {
    return st;
  }

  status = static_cast<uint8_t>(status & ~mask);
  return writeRegister(kRegStatus, status);
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
  if (!_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback is null");
  }
  if (!buf || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C read parameters");
  }
  // Limit reads to avoid oversized transfers.
  if (len > 255) {
    return Status::Error(Err::INVALID_PARAM, "I2C read length too large");
  }

  uint8_t tx = reg;
  return _config.i2cWriteRead(_config.i2cAddress, &tx, 1, buf, len,
                              _config.i2cTimeoutMs, _config.i2cUser);
}

Status RV3032::writeRegs(uint8_t reg, const uint8_t* buf, size_t len) {
  if (!_config.i2cWrite) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback is null");
  }
  if (!buf || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C write parameters");
  }

  static constexpr size_t kMaxWriteLen = 16;
  if (len > (kMaxWriteLen - 1)) {
    return Status::Error(Err::INVALID_PARAM, "I2C write length too large");
  }

  uint8_t tx[kMaxWriteLen] = {0};
  tx[0] = reg;
  std::memcpy(&tx[1], buf, len);
  return _config.i2cWrite(_config.i2cAddress, tx, len + 1,
                          _config.i2cTimeoutMs, _config.i2cUser);
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

  Status st;
  bool busy = false;
  bool failed = false;

  switch (_eeprom.state) {
    case EepromState::ReadControl1:
      st = readRegister(kRegControl1, _eeprom.control1);
      if (!st.ok()) {
        _eepromLastStatus = st;
        _eeprom.state = EepromState::Idle;
        break;
      }
      _eeprom.control1Valid = true;
      _eeprom.state = EepromState::EnableEerd;
      break;
    case EepromState::EnableEerd:
      st = writeRegister(kRegControl1, static_cast<uint8_t>(_eeprom.control1 | (1u << kControl1EerdBit)));
      if (!st.ok()) {
        _eepromLastStatus = st;
        _eeprom.state = EepromState::RestoreControl1;
        break;
      }
      _eeprom.state = EepromState::WriteAddr;
      break;
    case EepromState::WriteAddr:
      st = writeRegister(kRegEeAddr, _eeprom.reg);
      if (!st.ok()) {
        _eepromLastStatus = st;
        _eeprom.state = EepromState::RestoreControl1;
        break;
      }
      _eeprom.state = EepromState::WriteData;
      break;
    case EepromState::WriteData:
      st = writeRegister(kRegEeData, _eeprom.value);
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
      st = writeRegister(kRegEeCmd, kEepromUpdateCmd);
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
      if (_eeprom.control1Valid) {
        uint8_t restore = static_cast<uint8_t>(_eeprom.control1 & ~(1u << kControl1EerdBit));
        st = writeRegister(kRegControl1, restore);
        if (!st.ok() && _eepromLastStatus.ok()) {
          _eepromLastStatus = st;
        }
      }
      _eeprom.control1Valid = false;
      _eeprom.state = EepromState::Idle;
      
      // Check if there's a queued operation
      uint8_t nextReg, nextValue;
      if (eepromQueuePop(nextReg, nextValue)) {
        startEepromUpdate(nextReg, nextValue, now_ms);
      }
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
  Status st = readRegister(kRegTempLsb, tempLsb);
  if (!st.ok()) {
    return st;
  }
  busy = ((tempLsb & kEepromBusyBit) != 0);
  failed = ((tempLsb & kEepromErrorBit) != 0);
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
  uint32_t days = ts / 86400UL;
  uint32_t rem = ts % 86400UL;

  // Find year
  uint16_t year = 1970;
  while (true) {
    uint16_t daysInYear = isLeapYear(year) ? 366 : 365;
    if (days < daysInYear) {
      break;
    }
    days -= daysInYear;
    ++year;
    if (year > 2099) {
      return false;  // Out of RTC range
    }
  }

  // Find month
  uint8_t month = 1;
  while (true) {
    uint8_t dim = daysInMonth(year, month);
    if (days < dim) {
      break;
    }
    days -= dim;
    ++month;
    if (month > 12) {
      return false;
    }
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
  uint8_t status = 0;
  Status st = readRegister(kRegStatus, status);
  if (!st.ok()) {
    return st;
  }

  out.voltageLow = (status & 0x01) != 0;      // VLF bit
  out.powerOnReset = (status & 0x02) != 0;    // PORF bit
  out.backupSwitched = (status & 0x04) != 0;  // BSF bit
  out.timeInvalid = out.powerOnReset || out.voltageLow;

  return Status::Ok();
}

Status RV3032::clearPowerOnResetFlag() {
  uint8_t status = 0;
  Status st = readRegister(kRegStatus, status);
  if (!st.ok()) {
    return st;
  }

  // Clear PORF (bit 1)
  status &= ~0x02;

  return writeRegister(kRegStatus, status);
}

Status RV3032::clearVoltageLowFlag() {
  uint8_t status = 0;
  Status st = readRegister(kRegStatus, status);
  if (!st.ok()) {
    return st;
  }

  // Clear VLF (bit 0)
  status &= ~0x01;

  return writeRegister(kRegStatus, status);
}

Status RV3032::clearBackupSwitchFlag() {
  uint8_t status = 0;
  Status st = readRegister(kRegStatus, status);
  if (!st.ok()) {
    return st;
  }

  // Clear BSF (bit 2)
  status &= ~0x04;

  return writeRegister(kRegStatus, status);
}

}  // namespace RV3032
