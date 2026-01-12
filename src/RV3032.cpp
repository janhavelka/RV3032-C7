/**
 * @file RV3032.cpp
 * @brief Implementation of RV-3032-C7 RTC driver
 */

#include <Arduino.h>
#include <Wire.h>
#include "RV3032/RV3032.h"

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

constexpr uint16_t kTimerMaxTicks = 4095;

constexpr uint8_t kStatusThfBit = 7;
constexpr uint8_t kStatusTlfBit = 6;
constexpr uint8_t kStatusUfBit = 5;
constexpr uint8_t kStatusTfBit = 4;
constexpr uint8_t kStatusAfBit = 3;
constexpr uint8_t kStatusEvfBit = 2;
constexpr uint8_t kStatusPorfBit = 1;
constexpr uint8_t kStatusVlfBit = 0;

constexpr uint8_t kTempBsfBit = 0;

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
constexpr uint8_t kEepromUpdateCmd = 0x21;
constexpr uint32_t kEepromReadyTimeoutMs = 50;
}  // namespace

// ===== Lifecycle Functions =====

Status RV3032::begin(const Config& config) {
  _config = config;
  _initialized = false;
  _eeprom = EepromOp{};

  // Validate configuration
  if (!_config.wire) {
    return Status::Error(Err::INVALID_CONFIG, "I2C wire pointer is null");
  }
  if (_config.i2cAddress < 0x08 || _config.i2cAddress > 0x77) {
    return Status::Error(Err::INVALID_CONFIG, "I2C address out of range");
  }
  if (static_cast<uint8_t>(_config.backupMode) > static_cast<uint8_t>(BackupSwitchMode::Direct)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid backup mode");
  }
  if (_config.enableEepromWrites && _config.eepromTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "EEPROM timeout must be > 0");
  }

  // Test I2C communication
  _config.wire->beginTransmission(_config.i2cAddress);
  uint8_t result = _config.wire->endTransmission();
  if (result != 0) {
    return Status::Error(Err::DEVICE_NOT_FOUND, "RTC not responding on I2C", result);
  }

  // Apply backup switching mode
  uint8_t coe = 0;
  Status st = readRegister(kRegCoe, coe);
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
    if (!st.ok()) {
      return st;
    }
  }

  _initialized = true;
  return Status::Ok();
}

void RV3032::tick(uint32_t now_ms) {
  processEeprom(now_ms);
}

void RV3032::end() {
  _initialized = false;
  _eeprom = EepromOp{};
  // No resources to release (I2C managed by application)
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

  out.second = bcdToBin(buf[0] & 0x7F);
  out.minute = bcdToBin(buf[1] & 0x7F);
  out.hour = bcdToBin(buf[2] & 0x3F);
  
  // Decode weekday bitmask
  uint8_t wdayMask = static_cast<uint8_t>(buf[3] & 0x7F);
  out.weekday = 0;
  if (wdayMask != 0) {
    for (uint8_t i = 0; i < 7; ++i) {
      if (wdayMask & (1u << i)) {
        out.weekday = i;
        break;
      }
    }
  }

  out.day = bcdToBin(buf[4] & 0x3F);
  out.month = bcdToBin(buf[5] & 0x1F);
  out.year = static_cast<uint16_t>(2000 + bcdToBin(buf[6]));

  return Status::Ok();
}

Status RV3032::setTime(const DateTime& time) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  DateTime normalized = time;
  normalized.weekday = computeWeekday(time.year, time.month, time.day);

  if (!isValidDateTime(normalized)) {
    return Status::Error(Err::INVALID_DATETIME, "Invalid date/time values");
  }

  uint8_t buf[7] = {
    binToBcd(normalized.second),
    binToBcd(normalized.minute),
    binToBcd(normalized.hour),
    static_cast<uint8_t>(1u << (normalized.weekday % 7)),
    binToBcd(normalized.day),
    binToBcd(normalized.month),
    binToBcd(static_cast<uint8_t>(normalized.year % 100))
  };

  return writeRegs(kTimeReg, buf, sizeof(buf));
}

Status RV3032::readUnix(uint32_t& out) {
  DateTime dt;
  Status st = readTime(dt);
  if (!st.ok()) {
    return st;
  }

  if (!dateTimeToUnix(dt, out)) {
    return Status::Error(Err::INVALID_DATETIME, "Invalid date/time values");
  }

  return Status::Ok();
}

Status RV3032::setUnix(uint32_t ts) {
  DateTime dt;
  if (!unixToDateTime(ts, dt)) {
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

  triggered = ((status & (1u << kStatusAfBit)) != 0);
  return Status::Ok();
}

Status RV3032::clearAlarmFlag() {
  return clearStatus(static_cast<uint8_t>(1u << kStatusAfBit));
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
  if (ticks > kTimerMaxTicks) {
    return Status::Error(Err::INVALID_PARAM, "Timer ticks out of range");
  }
  if (static_cast<uint8_t>(freq) > static_cast<uint8_t>(TimerFrequency::Hz1_60)) {
    return Status::Error(Err::INVALID_PARAM, "Timer frequency out of range");
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
  uint8_t high = static_cast<uint8_t>((ticks >> 8) & 0xFF);

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

  ticks = static_cast<uint16_t>((static_cast<uint16_t>(high) << 8) | low);
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

  uint8_t raw = static_cast<uint8_t>(value & 0x3F);
  return writeEepromRegister(kRegOffset, static_cast<uint8_t>(raw & 0x3F));
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

  float frac = static_cast<float>(lsb >> 6) * 0.25f;
  if (msb < 0) {
    celsius = static_cast<float>(msb) - frac;
  } else {
    celsius = static_cast<float>(msb) + frac;
  }

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

Status RV3032::readStatusFlags(StatusFlags& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(kRegStatus, status);
  if (!st.ok()) {
    return st;
  }

  out.tempHigh = ((status & (1u << kStatusThfBit)) != 0);
  out.tempLow = ((status & (1u << kStatusTlfBit)) != 0);
  out.update = ((status & (1u << kStatusUfBit)) != 0);
  out.timer = ((status & (1u << kStatusTfBit)) != 0);
  out.alarm = ((status & (1u << kStatusAfBit)) != 0);
  out.event = ((status & (1u << kStatusEvfBit)) != 0);
  out.powerOnReset = ((status & (1u << kStatusPorfBit)) != 0);
  out.voltageLow = ((status & (1u << kStatusVlfBit)) != 0);

  return Status::Ok();
}

Status RV3032::readValidity(ValidityFlags& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  StatusFlags statusFlags;
  Status st = readStatusFlags(statusFlags);
  if (!st.ok()) {
    return st;
  }

  uint8_t tempLsb = 0;
  st = readRegister(kRegTempLsb, tempLsb);
  if (!st.ok()) {
    return st;
  }

  out.powerOnReset = statusFlags.powerOnReset;
  out.voltageLow = statusFlags.voltageLow;
  out.backupSwitched = ((tempLsb & (1u << kTempBsfBit)) != 0);
  out.timeInvalid = (out.powerOnReset || out.voltageLow);

  return Status::Ok();
}

Status RV3032::clearBackupSwitchFlag() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t tempLsb = 0;
  Status st = readRegister(kRegTempLsb, tempLsb);
  if (!st.ok()) {
    return st;
  }

  tempLsb = static_cast<uint8_t>(tempLsb & ~(1u << kTempBsfBit));
  return writeRegister(kRegTempLsb, tempLsb);
}

bool RV3032::isEepromBusy() const {
  return _eeprom.step != EepromStep::Idle;
}

Status RV3032::getEepromLastStatus() const {
  return _eeprom.lastStatus;
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
  DateTime normalized = time;
  normalized.weekday = computeWeekday(time.year, time.month, time.day);
  if (!isValidDateTime(normalized)) {
    return false;
  }

  uint32_t days = dateToDays(normalized.year, normalized.month, normalized.day);
  out = days * 86400UL
      + static_cast<uint32_t>(normalized.hour) * 3600UL
      + static_cast<uint32_t>(normalized.minute) * 60UL
      + static_cast<uint32_t>(normalized.second);

  return true;
}

// ===== Private Helper Functions =====

Status RV3032::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
  if (!_config.wire || !buf || len == 0) {
    return Status::Error(Err::I2C_ERROR, "Invalid I2C parameters");
  }

  _config.wire->beginTransmission(_config.i2cAddress);
  _config.wire->write(reg);
  uint8_t result = _config.wire->endTransmission(false);
  if (result != 0) {
    return Status::Error(Err::I2C_ERROR, "I2C write failed", result);
  }

  size_t read = _config.wire->requestFrom(_config.i2cAddress, static_cast<uint8_t>(len));
  if (read != len) {
    return Status::Error(Err::I2C_ERROR, "I2C read length mismatch", read);
  }

  for (size_t i = 0; i < len; ++i) {
    if (_config.wire->available()) {
      buf[i] = static_cast<uint8_t>(_config.wire->read());
    } else {
      return Status::Error(Err::I2C_ERROR, "I2C data not available");
    }
  }

  return Status::Ok();
}

Status RV3032::writeRegs(uint8_t reg, const uint8_t* buf, size_t len) {
  if (!_config.wire || !buf || len == 0) {
    return Status::Error(Err::I2C_ERROR, "Invalid I2C parameters");
  }

  _config.wire->beginTransmission(_config.i2cAddress);
  _config.wire->write(reg);
  for (size_t i = 0; i < len; ++i) {
    _config.wire->write(buf[i]);
  }

  uint8_t result = _config.wire->endTransmission();
  if (result != 0) {
    return Status::Error(Err::I2C_ERROR, "I2C write failed", result);
  }

  return Status::Ok();
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

  // Proceed with EEPROM update
  if (_config.eepromNonBlocking) {
    return scheduleEepromUpdate(reg, value);
  }
  return updateEepromByteBlocking(reg, value);
}

Status RV3032::scheduleEepromUpdate(uint8_t reg, uint8_t value) {
  if (_eeprom.step != EepromStep::Idle) {
    return Status::Error(Err::BUSY, "EEPROM busy");
  }

  _eeprom.step = EepromStep::ReadControl1;
  _eeprom.reg = reg;
  _eeprom.value = value;
  _eeprom.waitStartMs = 0;
  _eeprom.waitTimeoutMs = 0;
  _eeprom.waitActive = false;
  _eeprom.restoreNeeded = false;
  _eeprom.result = Status::Ok();
  return Status::Ok();
}

void RV3032::processEeprom(uint32_t now_ms) {
  if (!_initialized) {
    return;
  }
  if (!_config.enableEepromWrites || !_config.eepromNonBlocking) {
    return;
  }
  if (_eeprom.step == EepromStep::Idle) {
    return;
  }

  Status st;
  uint8_t busy = 0;
  switch (_eeprom.step) {
    case EepromStep::ReadControl1:
      st = readRegister(kRegControl1, _eeprom.control1);
      if (!st.ok()) {
        _eeprom.result = st;
        _eeprom.step = EepromStep::RestoreControl;
        _eeprom.restoreNeeded = false;
        break;
      }
      _eeprom.step = EepromStep::WriteControl1;
      break;
    case EepromStep::WriteControl1: {
      uint8_t control1 = static_cast<uint8_t>(_eeprom.control1 | (1u << kControl1EerdBit));
      st = writeRegister(kRegControl1, control1);
      if (!st.ok()) {
        _eeprom.result = st;
        _eeprom.step = EepromStep::RestoreControl;
        _eeprom.restoreNeeded = false;
        break;
      }
      _eeprom.restoreNeeded = true;
      _eeprom.step = EepromStep::WriteAddr;
      break;
    }
    case EepromStep::WriteAddr:
      st = writeRegister(kRegEeAddr, _eeprom.reg);
      if (!st.ok()) {
        _eeprom.result = st;
        _eeprom.step = EepromStep::RestoreControl;
        break;
      }
      _eeprom.step = EepromStep::WriteData;
      break;
    case EepromStep::WriteData:
      st = writeRegister(kRegEeData, _eeprom.value);
      if (!st.ok()) {
        _eeprom.result = st;
        _eeprom.step = EepromStep::RestoreControl;
        break;
      }
      _eeprom.waitActive = false;
      _eeprom.step = EepromStep::WaitReadyBefore;
      break;
    case EepromStep::WaitReadyBefore:
      if (!_eeprom.waitActive) {
        _eeprom.waitStartMs = now_ms;
        _eeprom.waitTimeoutMs = kEepromReadyTimeoutMs;
        _eeprom.waitActive = true;
      }
      st = readRegister(kRegTempLsb, busy);
      if (!st.ok()) {
        _eeprom.result = st;
        _eeprom.waitActive = false;
        _eeprom.step = EepromStep::RestoreControl;
        break;
      }
      if ((busy & kEepromBusyBit) == 0) {
        _eeprom.waitActive = false;
        _eeprom.step = EepromStep::WriteCmd;
        break;
      }
      if (static_cast<uint32_t>(now_ms - _eeprom.waitStartMs) >= _eeprom.waitTimeoutMs) {
        _eeprom.result = Status::Error(Err::TIMEOUT, "EEPROM write timeout");
        _eeprom.waitActive = false;
        _eeprom.step = EepromStep::RestoreControl;
      }
      break;
    case EepromStep::WriteCmd:
      st = writeRegister(kRegEeCmd, kEepromUpdateCmd);
      if (!st.ok()) {
        _eeprom.result = st;
        _eeprom.step = EepromStep::RestoreControl;
        break;
      }
      _eeprom.waitActive = false;
      _eeprom.step = EepromStep::WaitReadyAfter;
      break;
    case EepromStep::WaitReadyAfter:
      if (!_eeprom.waitActive) {
        _eeprom.waitStartMs = now_ms;
        _eeprom.waitTimeoutMs = _config.eepromTimeoutMs;
        _eeprom.waitActive = true;
      }
      st = readRegister(kRegTempLsb, busy);
      if (!st.ok()) {
        _eeprom.result = st;
        _eeprom.waitActive = false;
        _eeprom.step = EepromStep::RestoreControl;
        break;
      }
      if ((busy & kEepromBusyBit) == 0) {
        _eeprom.waitActive = false;
        _eeprom.step = EepromStep::RestoreControl;
        break;
      }
      if (static_cast<uint32_t>(now_ms - _eeprom.waitStartMs) >= _eeprom.waitTimeoutMs) {
        _eeprom.result = Status::Error(Err::TIMEOUT, "EEPROM write timeout");
        _eeprom.waitActive = false;
        _eeprom.step = EepromStep::RestoreControl;
      }
      break;
    case EepromStep::RestoreControl:
      if (_eeprom.restoreNeeded) {
        Status restoreSt = writeRegister(kRegControl1, _eeprom.control1);
        if (_eeprom.result.ok() && !restoreSt.ok()) {
          _eeprom.result = restoreSt;
        }
      }
      _eeprom.lastStatus = _eeprom.result;
      _eeprom.step = EepromStep::Idle;
      _eeprom.restoreNeeded = false;
      _eeprom.waitActive = false;
      _eeprom.waitStartMs = 0;
      _eeprom.waitTimeoutMs = 0;
      break;
    case EepromStep::Idle:
    default:
      break;
  }
}

Status RV3032::updateEepromByteBlocking(uint8_t reg, uint8_t value) {
  Status st = Status::Ok();

  // Enable EEPROM auto refresh
  uint8_t control1 = 0;
  st = readRegister(kRegControl1, control1);
  if (!st.ok()) {
    return st;
  }

  st = writeRegister(kRegControl1, static_cast<uint8_t>(control1 | (1u << kControl1EerdBit)));
  if (!st.ok()) {
    return st;
  }

  // Write EEPROM address and data
  st = writeRegister(kRegEeAddr, reg);
  if (!st.ok()) {
    goto restore_control;
  }

  st = writeRegister(kRegEeData, value);
  if (!st.ok()) {
    goto restore_control;
  }

  // Wait for EEPROM ready
  st = waitEepromReady(kEepromReadyTimeoutMs);
  if (!st.ok()) {
    goto restore_control;
  }

  // Execute EEPROM update command
  st = writeRegister(kRegEeCmd, kEepromUpdateCmd);
  if (!st.ok()) {
    goto restore_control;
  }

  // Wait for completion
  st = waitEepromReady(_config.eepromTimeoutMs);

restore_control:
  // Restore control1 register (disable EERD bit)
  Status restoreSt = writeRegister(kRegControl1, static_cast<uint8_t>(control1 & ~(1u << kControl1EerdBit)));
  
  // Return original error if any, otherwise restore status
  return st.ok() ? restoreSt : st;
}

Status RV3032::waitEepromReady(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (true) {
    uint8_t busy = 0;
    Status st = readRegister(kRegTempLsb, busy);
    if (!st.ok()) {
      return st;
    }

    if ((busy & kEepromBusyBit) == 0) {
      return Status::Ok();
    }

    if (static_cast<uint32_t>(millis() - start) >= timeoutMs) {
      return Status::Error(Err::TIMEOUT, "EEPROM write timeout");
    }

    yield();
  }
}

// ===== Conversion Helper Functions =====

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

}  // namespace RV3032
