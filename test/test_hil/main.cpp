#include <Arduino.h>
#include <Wire.h>

#include <cmath>
#include <cstring>

#include "RV3032/CommandTable.h"
#include "RV3032/RV3032.h"
#include "examples/common/I2cTransport.h"

namespace {

struct TransportOwner {
  TwoWire* wire = nullptr;
  uint32_t reads = 0;
  uint32_t writes = 0;
  uint32_t writeOneCommands = 0;
  uint8_t failReads = 0;
  uint8_t failWrites = 0;
};

struct TestStats {
  uint32_t pass = 0;
  uint32_t fail = 0;
  uint32_t skip = 0;
};

RV3032::RV3032 gRtc;
TransportOwner gOwner{&Wire};
TestStats gStats;

RV3032::Status ownerWrite(uint8_t address, const uint8_t* data, size_t length,
                          uint32_t timeoutMs, void* user) {
  auto* owner = static_cast<TransportOwner*>(user);
  ++owner->writes;
  if (data != nullptr && length >= 2U &&
      data[0] == RV3032::cmd::REG_EE_COMMAND &&
      data[1] == RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
    ++owner->writeOneCommands;
  }
  if (owner->failWrites > 0U) {
    --owner->failWrites;
    return RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA,
                                 "injected write NACK");
  }
  return transport::wireWrite(address, data, length, timeoutMs, owner->wire);
}

RV3032::Status ownerWriteRead(uint8_t address, const uint8_t* tx,
                              size_t txLength, uint8_t* rx, size_t rxLength,
                              uint32_t timeoutMs, void* user) {
  auto* owner = static_cast<TransportOwner*>(user);
  ++owner->reads;
  if (owner->failReads > 0U) {
    --owner->failReads;
    return RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR,
                                 "injected address NACK");
  }
  return transport::wireWriteRead(address, tx, txLength, rx, rxLength,
                                  timeoutMs, owner->wire);
}

uint32_t nowMs(void*) { return millis(); }

void waitMs(uint32_t durationMs, void*) { delay(durationMs); }

void report(const char* name, bool passed, const RV3032::Status* status = nullptr) {
  if (passed) {
    ++gStats.pass;
    Serial.printf("[PASS] %s\n", name);
    return;
  }
  ++gStats.fail;
  if (status == nullptr) {
    Serial.printf("[FAIL] %s\n", name);
  } else {
    Serial.printf("[FAIL] %s code=%u detail=%ld msg=%s\n", name,
                  static_cast<unsigned>(status->code),
                  static_cast<long>(status->detail),
                  status->msg == nullptr ? "" : status->msg);
  }
}

void reportStatus(const char* name, const RV3032::Status& status) {
  report(name, status.ok(), &status);
}

void skip(const char* name, const char* reason) {
  ++gStats.skip;
  Serial.printf("[SKIP] %s: %s\n", name, reason);
}

bool before(uint32_t deadlineMs) {
  return static_cast<int32_t>(deadlineMs - millis()) > 0;
}

RV3032::Status completeJob(RV3032::Status status,
                           uint32_t timeoutMs = 5000U,
                           uint8_t budget = 1U) {
  if (!status.inProgress()) {
    return status;
  }
  const uint32_t deadlineMs = millis() + timeoutMs;
  while (status.inProgress() && before(deadlineMs)) {
    uint8_t used = 0;
    status = gRtc.pollJob(millis(), budget, used);
    if (status.inProgress()) {
      delay(1);
    }
  }
  if (status.inProgress()) {
    uint8_t used = 0;
    status = gRtc.pollJob(deadlineMs, budget, used);
  }
  return status;
}

RV3032::Status drainEeprom(uint32_t timeoutMs = 10000U) {
  RV3032::Status status = gRtc.getEepromStatus();
  const uint32_t deadlineMs = millis() + timeoutMs;
  while ((status.inProgress() || gRtc.isEepromBusy()) && before(deadlineMs)) {
    uint8_t used = 0;
    status = gRtc.pollEeprom(millis(), 1U, used);
    if (status.inProgress() || gRtc.isEepromBusy()) {
      delay(1);
    }
  }
  if ((status.inProgress() || gRtc.isEepromBusy())) {
    uint8_t used = 0;
    status = gRtc.pollEeprom(deadlineMs, 1U, used);
  }
  return status;
}

RV3032::Status readPersistent(uint8_t offset, uint8_t length, uint8_t* out) {
  RV3032::Status status = gRtc.startReadUserEepromJob(
      offset, length, millis(), 3000U);
  status = completeJob(status, 4000U);
  if (!status.ok()) {
    return status;
  }
  RV3032::PersistentReadResult result{};
  status = gRtc.getPersistentReadJobResult(result);
  if (!status.ok()) {
    return status;
  }
  if (!result.persistentVerified || !result.cleanupVerified ||
      result.length != length) {
    return RV3032::Status::Error(RV3032::Err::INCOHERENT_DATA,
                                 "persistent proof incomplete");
  }
  memcpy(out, result.data, length);
  return RV3032::Status::Ok();
}

bool sameAlarm(const RV3032::AlarmConfig& left,
               const RV3032::AlarmConfig& right) {
  return left.minute == right.minute && left.hour == right.hour &&
         left.date == right.date && left.matchMinute == right.matchMinute &&
         left.matchHour == right.matchHour && left.matchDate == right.matchDate;
}

bool sameEvi(const RV3032::EviConfig& left, const RV3032::EviConfig& right) {
  return left.rising == right.rising && left.debounce == right.debounce &&
         left.overwrite == right.overwrite &&
         left.synchronized == right.synchronized &&
         left.clkoutStopDelay == right.clkoutStopDelay;
}

bool sameClockMask(const RV3032::ClockInterruptMaskConfig& left,
                   const RV3032::ClockInterruptMaskConfig& right) {
  return left.longStopDelay == right.longStopDelay &&
         left.interruptDelayEnabled == right.interruptDelayEnabled &&
         left.eventSource == right.eventSource &&
         left.alarmSource == right.alarmSource &&
         left.timerSource == right.timerSource &&
         left.updateSource == right.updateSource &&
         left.tempHighSource == right.tempHighSource &&
         left.tempLowSource == right.tempLowSource;
}

bool sameTemperatureConfig(const RV3032::TemperatureEventConfig& left,
                           const RV3032::TemperatureEventConfig& right) {
  return left.lowThresholdC == right.lowThresholdC &&
         left.highThresholdC == right.highThresholdC &&
         left.lowEventEnabled == right.lowEventEnabled &&
         left.highEventEnabled == right.highEventEnabled &&
         left.lowInterruptEnabled == right.lowInterruptEnabled &&
         left.highInterruptEnabled == right.highInterruptEnabled &&
         left.lowTimestampOverwrite == right.lowTimestampOverwrite &&
         left.highTimestampOverwrite == right.highTimestampOverwrite;
}

RV3032::Config makeConfig(bool enableEepromWrites) {
  RV3032::Config config{};
  config.i2cWrite = ownerWrite;
  config.i2cWriteRead = ownerWriteRead;
  config.i2cUser = &gOwner;
  config.nowMs = nowMs;
  config.waitMs = waitMs;
  config.enableEepromWrites = enableEepromWrites;
  config.i2cTimeoutMs = 50U;
  config.eepromTimeoutMs = 100U;
  config.offlineThreshold = 3U;
  return config;
}

void runLifecycleAndReadTests() {
  const uint32_t callbacksBefore = gOwner.reads + gOwner.writes;
  RV3032::Config config = makeConfig(false);
  RV3032::Status status = gRtc.begin(config);
  report("begin succeeds", status.ok(), &status);
  report("begin is passive", gOwner.reads + gOwner.writes == callbacksBefore);

  const uint32_t callbacksAfterBegin = gOwner.reads + gOwner.writes;
  status = gRtc.begin(config);
  report("second begin rejected with zero I/O",
         status.is(RV3032::Err::BUSY) &&
             gOwner.reads + gOwner.writes == callbacksAfterBegin,
         &status);

  RV3032::SettingsSnapshot settings{};
  status = gRtc.getSettings(settings);
  report("settings snapshot after passive begin",
         status.ok() && settings.initialized &&
             settings.state == RV3032::DriverState::READY &&
             settings.totalSuccess == 0U && settings.totalFailures == 0U,
         &status);

  status = gRtc.probe();
  reportStatus("explicit probe", status);
  status = gRtc.getSettings(settings);
  report("probe does not alter health counters",
         status.ok() && settings.totalSuccess == 0U &&
             settings.totalFailures == 0U,
         &status);

  const uint32_t validationCallbacks = gOwner.reads + gOwner.writes;
  RV3032::DateTime invalid{};
  invalid.year = 2025;
  invalid.month = 2;
  invalid.day = 29;
  invalid.weekday = 6;
  status = gRtc.setTime(invalid);
  report("invalid non-leap date is zero-I/O",
         status.is(RV3032::Err::INVALID_DATETIME) &&
             gOwner.reads + gOwner.writes == validationCallbacks,
         &status);
  uint8_t byte = 0;
  status = gRtc.readRegister(RV3032::cmd::REG_EE_COMMAND, byte);
  report("unsafe raw EEPROM command read rejected zero-I/O",
         status.is(RV3032::Err::INVALID_PARAM) &&
             gOwner.reads + gOwner.writes == validationCallbacks,
         &status);
  status = gRtc.readUserRam(16U, &byte, 1U);
  report("invalid RAM range rejected zero-I/O",
         status.is(RV3032::Err::INVALID_PARAM) &&
             gOwner.reads + gOwner.writes == validationCallbacks,
         &status);

  uint8_t rawStatus = 0;
  reportStatus("read status", gRtc.readStatus(rawStatus));
  RV3032::StatusFlags statusFlags{};
  reportStatus("read decoded status flags", gRtc.readStatusFlags(statusFlags));
  RV3032::ValidityFlags validity{};
  reportStatus("read validity flags", gRtc.readValidity(validity));
  Serial.printf("[INFO] initial status=0x%02X PORF=%u VLF=%u BSF=%u\n",
                rawStatus, validity.powerOnReset ? 1U : 0U,
                validity.voltageLow ? 1U : 0U,
                validity.backupSwitched ? 1U : 0U);

  RV3032::DateTime time{};
  status = gRtc.readTime(time);
  reportStatus("strict calendar burst", status);
  uint8_t hundredths = 0;
  status = gRtc.readHundredths(hundredths);
  report("hundredths range", status.ok() && hundredths <= 99U, &status);
  uint32_t unixTime = 0;
  reportStatus("read Unix time", gRtc.readUnix(unixTime));
  float temperature = 0.0F;
  status = gRtc.readTemperatureC(temperature);
  report("temperature finite and plausible",
         status.ok() && std::isfinite(temperature) && temperature > -100.0F &&
             temperature < 150.0F,
         &status);
  Serial.printf("[INFO] initial calendar=%04u-%02u-%02u %02u:%02u:%02u unix=%lu temp=%.3fC\n",
                static_cast<unsigned>(time.year), time.month, time.day,
                time.hour, time.minute, time.second,
                static_cast<unsigned long>(unixTime), temperature);

  RV3032::EepromHardwareFlags eepromFlags{};
  status = gRtc.getEepromHardwareFlags(eepromFlags);
  reportStatus("read hardware EEPROM flags", status);
  Serial.printf("[INFO] EEbusy=%u EEF=%u\n", eepromFlags.busy ? 1U : 0U,
                eepromFlags.writeFailed ? 1U : 0U);
}

void runCalendarTests() {
  RV3032::Status status = gRtc.startReadTimeSnapshotJob(millis(), 500U);
  const uint32_t callbacks = gOwner.reads + gOwner.writes;
  uint8_t used = 99U;
  if (status.inProgress()) {
    status = gRtc.pollJob(millis(), 0U, used);
  }
  report("snapshot zero-budget poll performs zero I/O",
         status.inProgress() && used == 0U &&
             gOwner.reads + gOwner.writes == callbacks,
         &status);
  status = completeJob(status, 1000U);
  reportStatus("status-first snapshot completes", status);
  RV3032::TimeSnapshot snapshot{};
  status = gRtc.getReadTimeSnapshotJobResult(snapshot);
  report("snapshot result has status evidence",
         status.ok() && snapshot.statusValid, &status);

  RV3032::DateTime buildTime{};
  status = RV3032::RV3032::parseBuildTime(buildTime);
  reportStatus("parse build timestamp", status);
  if (!status.ok()) {
    return;
  }
  status = gRtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      buildTime, millis(), 500U);
  status = completeJob(status, 1500U);
  reportStatus("verified calendar set and PORF/VLF clear", status);
  RV3032::VerifiedTimeSetReport setReport{};
  status = gRtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(setReport);
  report("verified calendar report is complete",
         status.ok() && setReport.calendarWriteAttempted &&
             setReport.statusWriteAttempted && setReport.verifiedValid &&
             setReport.statusBeforeValid && setReport.statusBeforeClearValid &&
             setReport.statusAfterValid,
         &status);

  RV3032::ValidityFlags validity{};
  status = gRtc.readValidity(validity);
  report("calendar is valid after named operation",
         status.ok() && !validity.timeInvalid, &status);

  RV3032::DateTime leap{};
  leap.year = 2024;
  leap.month = 2;
  leap.day = 29;
  leap.hour = 23;
  leap.minute = 59;
  leap.second = 58;
  status = RV3032::RV3032::computeWeekday(
      leap.year, leap.month, leap.day, leap.weekday);
  reportStatus("compute leap-day weekday", status);
  uint32_t leapUnix = 0;
  status = RV3032::RV3032::dateTimeToUnix(leap, leapUnix);
  reportStatus("leap-day to Unix conversion", status);
  RV3032::DateTime roundTrip{};
  status = RV3032::RV3032::unixToDateTime(leapUnix, roundTrip);
  report("Unix leap-day round trip",
         status.ok() && roundTrip.year == leap.year &&
             roundTrip.month == leap.month && roundTrip.day == leap.day &&
             roundTrip.hour == leap.hour && roundTrip.minute == leap.minute &&
             roundTrip.second == leap.second,
         &status);

  status = gRtc.setUnix(leapUnix);
  reportStatus("set Unix leap rollover point", status);
  delay(2300U);
  RV3032::DateTime afterLeap{};
  status = gRtc.readTime(afterLeap);
  report("hardware leap-day rollover",
         status.ok() && afterLeap.year == 2024U && afterLeap.month == 3U &&
             afterLeap.day == 1U && afterLeap.hour == 0U &&
             afterLeap.minute == 0U && afterLeap.second <= 4U,
         &status);

  RV3032::DateTime minTime{};
  RV3032::DateTime maxTime{};
  const RV3032::Status minStatus =
      RV3032::RV3032::unixToDateTime(946684800UL, minTime);
  const RV3032::Status maxStatus =
      RV3032::RV3032::unixToDateTime(4102444799UL, maxTime);
  report("Unix domain endpoints",
         minStatus.ok() && maxStatus.ok() && minTime.year == 2000U &&
             maxTime.year == 2099U && maxTime.month == 12U &&
             maxTime.day == 31U && maxTime.second == 59U);

  status = gRtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      buildTime, millis(), 500U);
  reportStatus("restore build calendar", completeJob(status, 1500U));
}

void runRamAndRawTests() {
  uint8_t original[16] = {};
  RV3032::Status status = gRtc.readUserRam(0U, original, sizeof(original));
  reportStatus("save all user RAM", status);
  if (!status.ok()) {
    return;
  }
  uint8_t pattern[16] = {};
  for (uint8_t i = 0; i < sizeof(pattern); ++i) {
    pattern[i] = static_cast<uint8_t>((i * 37U) ^ 0xA5U);
  }
  status = gRtc.writeUserRam(0U, pattern, sizeof(pattern));
  reportStatus("16-byte cooperative RAM write",
               completeJob(status, 1000U));
  uint8_t observed[16] = {};
  status = gRtc.readUserRam(0U, observed, sizeof(observed));
  report("RAM full-pattern readback",
         status.ok() && memcmp(pattern, observed, sizeof(pattern)) == 0,
         &status);

  status = gRtc.startRegisterUpdateJob(RV3032::cmd::REG_USER_RAM_START,
                                       0x0FU, 0x05U);
  status = completeJob(status, 1000U);
  uint8_t updated = 0;
  RV3032::Status readStatus =
      gRtc.readRegister(RV3032::cmd::REG_USER_RAM_START, updated);
  const uint8_t expected = static_cast<uint8_t>((pattern[0] & 0xF0U) | 0x05U);
  report("RAM read-modify-write job",
         status.ok() && readStatus.ok() && updated == expected, &status);

  status = gRtc.writeUserRam(0U, original, sizeof(original));
  reportStatus("restore all user RAM", completeJob(status, 1000U));

  uint8_t thresholds[2] = {};
  status = gRtc.readRegisters(RV3032::cmd::REG_TLOW_THRESHOLD,
                              thresholds, sizeof(thresholds));
  reportStatus("reviewed raw block read", status);
  if (status.ok()) {
    reportStatus("reviewed raw block write",
                 gRtc.writeRegisters(RV3032::cmd::REG_TLOW_THRESHOLD,
                                     thresholds, sizeof(thresholds)));
  }
}

void runAlarmTimerUpdateTests() {
  RV3032::AlarmConfig originalAlarm{};
  RV3032::Status status = gRtc.getAlarmConfig(originalAlarm);
  reportStatus("save alarm configuration", status);
  bool alarmInterrupt = false;
  reportStatus("save alarm interrupt state",
               gRtc.getAlarmInterruptEnabled(alarmInterrupt));
  reportStatus("disable alarm interrupt",
               completeJob(gRtc.enableAlarmInterrupt(false)));
  reportStatus("set alarm boundary time",
               completeJob(gRtc.setAlarmTime(59U, 23U, 31U)));
  reportStatus("set all alarm match fields",
               completeJob(gRtc.setAlarmMatch(true, true, true)));
  RV3032::AlarmConfig alarm{};
  status = gRtc.getAlarmConfig(alarm);
  report("alarm configuration round trip",
         status.ok() && alarm.minute == 59U && alarm.hour == 23U &&
             alarm.date == 31U && alarm.matchMinute && alarm.matchHour &&
             alarm.matchDate,
         &status);
  reportStatus("restore alarm time",
               completeJob(gRtc.setAlarmTime(originalAlarm.minute,
                                             originalAlarm.hour,
                                             originalAlarm.date)));
  reportStatus("restore alarm matches",
               completeJob(gRtc.setAlarmMatch(originalAlarm.matchMinute,
                                              originalAlarm.matchHour,
                                              originalAlarm.matchDate)));
  reportStatus("restore alarm interrupt",
               completeJob(gRtc.enableAlarmInterrupt(alarmInterrupt)));
  status = gRtc.getAlarmConfig(alarm);
  report("alarm restored", status.ok() && sameAlarm(alarm, originalAlarm),
         &status);

  uint16_t originalTicks = 1U;
  RV3032::TimerFrequency originalFrequency = RV3032::TimerFrequency::Hz1;
  bool originalTimerEnabled = false;
  status = gRtc.getTimer(originalTicks, originalFrequency,
                         originalTimerEnabled);
  reportStatus("save timer configuration", status);
  bool timerInterrupt = false;
  reportStatus("save timer interrupt state",
               gRtc.getTimerInterruptEnabled(timerInterrupt));
  reportStatus("disable timer interrupt",
               completeJob(gRtc.setTimerInterruptEnabled(false)));
  for (uint8_t raw = 0; raw < 4U; ++raw) {
    const uint16_t ticks = raw == 0U ? 1U : 4095U;
    status = completeJob(gRtc.setTimer(
        ticks, static_cast<RV3032::TimerFrequency>(raw), false));
    uint16_t observedTicks = 0;
    RV3032::TimerFrequency observedFrequency = RV3032::TimerFrequency::Hz4096;
    bool enabled = true;
    const RV3032::Status readStatus =
        gRtc.getTimer(observedTicks, observedFrequency, enabled);
    char name[64] = {};
    snprintf(name, sizeof(name), "timer frequency %u boundary round trip", raw);
    report(name, status.ok() && readStatus.ok() && observedTicks == ticks &&
                     observedFrequency ==
                         static_cast<RV3032::TimerFrequency>(raw) &&
                     !enabled,
           &status);
  }
  reportStatus("restore timer configuration",
               completeJob(gRtc.setTimer(originalTicks, originalFrequency,
                                         originalTimerEnabled)));
  reportStatus("restore timer interrupt",
               completeJob(gRtc.setTimerInterruptEnabled(timerInterrupt)));

  RV3032::PeriodicUpdateFrequency originalUpdateFrequency =
      RV3032::PeriodicUpdateFrequency::SECOND;
  bool originalUpdateEnabled = false;
  status = gRtc.getPeriodicUpdate(originalUpdateFrequency,
                                  originalUpdateEnabled);
  reportStatus("save periodic-update configuration", status);
  reportStatus("enable one-second periodic update",
               completeJob(gRtc.setPeriodicUpdate(
                   RV3032::PeriodicUpdateFrequency::SECOND, true)));
  RV3032::PeriodicUpdateFrequency updateFrequency =
      RV3032::PeriodicUpdateFrequency::MINUTE;
  bool updateEnabled = false;
  status = gRtc.getPeriodicUpdate(updateFrequency, updateEnabled);
  report("periodic-update round trip",
         status.ok() &&
             updateFrequency == RV3032::PeriodicUpdateFrequency::SECOND &&
             updateEnabled,
         &status);
  delay(1200U);
  bool updateFlag = false;
  status = gRtc.getPeriodicUpdateFlag(updateFlag);
  report("periodic update flag observable", status.ok() && updateFlag,
         &status);
  reportStatus("clear periodic update flag",
               completeJob(gRtc.clearPeriodicUpdateFlag()));
  reportStatus("restore periodic-update configuration",
               completeJob(gRtc.setPeriodicUpdate(originalUpdateFrequency,
                                                  originalUpdateEnabled)));
}

void runClockCalibrationTemperatureTests() {
  RV3032::ClkoutConfig originalClock{};
  RV3032::Status status = gRtc.getClkoutConfig(originalClock);
  reportStatus("save CLKOUT configuration", status);
  bool clockInterrupt = false;
  reportStatus("save CLKOUT interrupt state",
               gRtc.getClockInterruptEnabled(clockInterrupt));
  RV3032::ClockInterruptMaskConfig originalMask{};
  reportStatus("save CLKOUT interrupt mask",
               gRtc.getClockInterruptMaskConfig(originalMask));
  reportStatus("disable CLKOUT interrupt",
               completeJob(gRtc.setClockInterruptEnabled(false)));
  RV3032::ClockInterruptMaskConfig mask{};
  mask.longStopDelay = true;
  mask.interruptDelayEnabled = true;
  mask.eventSource = true;
  mask.alarmSource = true;
  mask.timerSource = true;
  mask.updateSource = true;
  mask.tempHighSource = true;
  mask.tempLowSource = true;
  reportStatus("set all CLKOUT interrupt sources",
               completeJob(gRtc.setClockInterruptMaskConfig(mask)));
  RV3032::ClockInterruptMaskConfig observedMask{};
  status = gRtc.getClockInterruptMaskConfig(observedMask);
  report("CLKOUT interrupt-mask round trip",
         status.ok() && sameClockMask(mask, observedMask), &status);
  reportStatus("restore CLKOUT interrupt mask",
               completeJob(gRtc.setClockInterruptMaskConfig(originalMask)));

  for (uint8_t raw = 0; raw < 4U; ++raw) {
    status = completeJob(gRtc.setClkoutFrequency(
        static_cast<RV3032::ClkoutFrequency>(raw)));
    RV3032::ClkoutFrequency observed = RV3032::ClkoutFrequency::Hz32768;
    const RV3032::Status readStatus = gRtc.getClkoutFrequency(observed);
    char name[64] = {};
    snprintf(name, sizeof(name), "CLKOUT crystal frequency %u round trip", raw);
    report(name, status.ok() && readStatus.ok() &&
                     observed == static_cast<RV3032::ClkoutFrequency>(raw),
           &status);
  }
  reportStatus("restore complete CLKOUT configuration",
               completeJob(gRtc.setClkoutConfig(originalClock)));
  reportStatus("restore CLKOUT interrupt",
               completeJob(gRtc.setClockInterruptEnabled(clockInterrupt)));

  float originalOffset = 0.0F;
  reportStatus("save frequency offset", gRtc.getOffsetPpm(originalOffset));
  reportStatus("set minimum frequency offset",
               completeJob(gRtc.setOffsetPpm(-7.6293945F)));
  float observedOffset = 0.0F;
  status = gRtc.getOffsetPpm(observedOffset);
  report("minimum offset round trip",
         status.ok() && fabsf(observedOffset - (-7.6293945F)) < 0.13F,
         &status);
  reportStatus("set maximum frequency offset",
               completeJob(gRtc.setOffsetPpm(7.3909760F)));
  status = gRtc.getOffsetPpm(observedOffset);
  report("maximum offset round trip",
         status.ok() && fabsf(observedOffset - 7.3909760F) < 0.13F,
         &status);
  reportStatus("restore frequency offset",
               completeJob(gRtc.setOffsetPpm(originalOffset)));

  bool porInterrupt = false;
  bool vlInterrupt = false;
  reportStatus("save POR interrupt state",
               gRtc.getPowerOnResetInterruptEnabled(porInterrupt));
  reportStatus("save voltage-low interrupt state",
               gRtc.getVoltageLowInterruptEnabled(vlInterrupt));
  reportStatus("enable POR interrupt",
               completeJob(gRtc.setPowerOnResetInterruptEnabled(true)));
  reportStatus("enable voltage-low interrupt",
               completeJob(gRtc.setVoltageLowInterruptEnabled(true)));
  bool observedPor = false;
  bool observedVl = false;
  const RV3032::Status porStatus =
      gRtc.getPowerOnResetInterruptEnabled(observedPor);
  const RV3032::Status vlStatus =
      gRtc.getVoltageLowInterruptEnabled(observedVl);
  report("POR/VL interrupt round trip",
         porStatus.ok() && vlStatus.ok() && observedPor && observedVl);
  reportStatus("restore POR interrupt",
               completeJob(gRtc.setPowerOnResetInterruptEnabled(porInterrupt)));
  reportStatus("restore voltage-low interrupt",
               completeJob(gRtc.setVoltageLowInterruptEnabled(vlInterrupt)));

  RV3032::TemperatureEventConfig originalTemperature{};
  status = gRtc.getTemperatureEventConfig(originalTemperature);
  reportStatus("save temperature-event configuration", status);
  RV3032::TemperatureEventConfig safeTemperature{};
  safeTemperature.lowThresholdC = -100;
  safeTemperature.highThresholdC = 100;
  safeTemperature.lowTimestampOverwrite = true;
  safeTemperature.highTimestampOverwrite = true;
  reportStatus("set disabled temperature-event boundaries",
               completeJob(gRtc.setTemperatureEventConfig(safeTemperature)));
  RV3032::TemperatureEventConfig observedTemperature{};
  status = gRtc.getTemperatureEventConfig(observedTemperature);
  report("temperature-event round trip",
         status.ok() &&
             sameTemperatureConfig(safeTemperature, observedTemperature),
         &status);
  reportStatus("restore temperature-event configuration",
               completeJob(gRtc.setTemperatureEventConfig(originalTemperature)));

  int16_t originalReference = 0;
  status = gRtc.getTemperatureReference(originalReference);
  reportStatus("save temperature reference", status);
  const int16_t testReference =
      originalReference == 123 ? static_cast<int16_t>(-123)
                               : static_cast<int16_t>(123);
  reportStatus("set temperature reference",
               completeJob(gRtc.setTemperatureReference(testReference)));
  int16_t observedReference = 0;
  status = gRtc.getTemperatureReference(observedReference);
  report("temperature-reference round trip",
         status.ok() && observedReference == testReference, &status);
  reportStatus("restore temperature reference",
               completeJob(gRtc.setTemperatureReference(originalReference)));

  status = gRtc.startReadCoherentTemperatureJob(millis(), 500U);
  status = completeJob(status, 1000U);
  RV3032::CoherentTemperatureResult coherent{};
  const RV3032::Status resultStatus =
      gRtc.getReadCoherentTemperatureJobResult(coherent);
  report("coherent temperature job",
         status.ok() && resultStatus.ok() && std::isfinite(coherent.celsius) &&
             coherent.raw >= -2048 && coherent.raw <= 2047,
         &status);
}

void runEviControlAndStatusTests() {
  RV3032::EviConfig originalEvi{};
  RV3032::Status status = gRtc.getEviConfig(originalEvi);
  reportStatus("save EVI configuration", status);
  bool eventInterrupt = false;
  reportStatus("save EVI interrupt state",
               gRtc.getEventInterruptEnabled(eventInterrupt));
  reportStatus("disable EVI interrupt",
               completeJob(gRtc.setEventInterruptEnabled(false)));
  reportStatus("set EVI rising edge", completeJob(gRtc.setEviEdge(true)));
  reportStatus("set EVI 8 Hz debounce",
               completeJob(gRtc.setEviDebounce(RV3032::EviDebounce::Hz8)));
  reportStatus("enable EVI overwrite",
               completeJob(gRtc.setEviOverwrite(true)));
  reportStatus("enable EVI synchronization",
               completeJob(gRtc.setEventSynchronizationEnabled(true)));
  reportStatus("enable CLKOUT stop delay",
               completeJob(gRtc.setClkoutStopDelayEnabled(true)));
  RV3032::EviConfig evi{};
  status = gRtc.getEviConfig(evi);
  report("EVI complete round trip",
         status.ok() && evi.rising && evi.debounce == RV3032::EviDebounce::Hz8 &&
             evi.overwrite && evi.synchronized && evi.clkoutStopDelay,
         &status);
  reportStatus("restore EVI edge",
               completeJob(gRtc.setEviEdge(originalEvi.rising)));
  reportStatus("restore EVI debounce",
               completeJob(gRtc.setEviDebounce(originalEvi.debounce)));
  reportStatus("restore EVI overwrite",
               completeJob(gRtc.setEviOverwrite(originalEvi.overwrite)));
  reportStatus("restore EVI synchronization",
               completeJob(gRtc.setEventSynchronizationEnabled(
                   originalEvi.synchronized)));
  reportStatus("restore CLKOUT stop delay",
               completeJob(gRtc.setClkoutStopDelayEnabled(
                   originalEvi.clkoutStopDelay)));
  reportStatus("restore EVI interrupt",
               completeJob(gRtc.setEventInterruptEnabled(eventInterrupt)));
  status = gRtc.getEviConfig(evi);
  report("EVI restored", status.ok() && sameEvi(evi, originalEvi), &status);

  bool originalGp0 = false;
  bool originalGp1 = false;
  reportStatus("save general-purpose bits",
               gRtc.getGeneralPurposeBits(originalGp0, originalGp1));
  reportStatus("set general-purpose bits",
               completeJob(gRtc.setGeneralPurposeBits(!originalGp0,
                                                      !originalGp1)));
  bool gp0 = originalGp0;
  bool gp1 = originalGp1;
  status = gRtc.getGeneralPurposeBits(gp0, gp1);
  report("general-purpose bits round trip",
         status.ok() && gp0 == !originalGp0 && gp1 == !originalGp1, &status);
  reportStatus("restore general-purpose bits",
               completeJob(gRtc.setGeneralPurposeBits(originalGp0,
                                                      originalGp1)));

  bool stopped = true;
  reportStatus("read STOP state", gRtc.getStopEnabled(stopped));
  if (stopped) {
    reportStatus("clear pre-existing STOP",
                 completeJob(gRtc.setStopEnabled(false)));
  }
  reportStatus("set STOP", completeJob(gRtc.setStopEnabled(true)));
  status = gRtc.getStopEnabled(stopped);
  report("STOP set readback", status.ok() && stopped, &status);
  reportStatus("clear STOP", completeJob(gRtc.setStopEnabled(false)));
  status = gRtc.getStopEnabled(stopped);
  report("STOP clear readback", status.ok() && !stopped, &status);

  RV3032::Timestamp timestamp{};
  reportStatus("read temperature-low timestamp",
               gRtc.readTimestamp(RV3032::TimestampSource::TLow, timestamp));
  reportStatus("read temperature-high timestamp",
               gRtc.readTimestamp(RV3032::TimestampSource::THigh, timestamp));
  reportStatus("read EVI timestamp",
               gRtc.readTimestamp(RV3032::TimestampSource::Evi, timestamp));
  reportStatus("reset EVI timestamp",
               completeJob(gRtc.resetTimestamp(RV3032::TimestampSource::Evi)));

  reportStatus("clear both temperature flags",
               completeJob(gRtc.clearTemperatureFlags()));
  reportStatus("guarded clear event/alarm/timer/update flags",
               completeJob(gRtc.clearStatus(0x3CU)));
  reportStatus("clear backup-switch flag",
               completeJob(gRtc.clearBackupSwitchFlag()));
  reportStatus("clear EEPROM error flag",
               completeJob(gRtc.clearEepromErrorFlag()));
  reportStatus("clear CLKOUT flag",
               completeJob(gRtc.clearClockOutputFlag()));
}

void runPersistentReadAndHealthTests() {
  for (uint8_t raw = 0; raw < 6U; ++raw) {
    RV3032::Status status = gRtc.startReadConfigurationEepromJob(
        static_cast<RV3032::ConfigurationEepromRegister>(0xC0U + raw),
        millis(), 3000U);
    status = completeJob(status, 4000U);
    RV3032::PersistentReadResult result{};
    const RV3032::Status resultStatus =
        gRtc.getPersistentReadJobResult(result);
    char name[64] = {};
    snprintf(name, sizeof(name), "configuration EEPROM C%u direct proof", raw);
    report(name, status.ok() && resultStatus.ok() &&
                     result.persistentVerified && result.cleanupVerified &&
                     result.length == 1U,
           &status);
  }
  uint8_t userEeprom[32] = {};
  RV3032::Status first = readPersistent(0U, 16U, userEeprom);
  RV3032::Status second = readPersistent(16U, 16U, userEeprom + 16U);
  report("all 32 user EEPROM bytes directly proven",
         first.ok() && second.ok(), first.ok() ? &second : &first);
  Serial.print("[INFO] user EEPROM:");
  for (uint8_t value : userEeprom) {
    Serial.printf(" %02X", value);
  }
  Serial.println();

  RV3032::SettingsSnapshot beforeProbeFailure{};
  gRtc.getSettings(beforeProbeFailure);
  gOwner.failReads = 1U;
  RV3032::Status status = gRtc.probe();
  RV3032::SettingsSnapshot afterProbeFailure{};
  gRtc.getSettings(afterProbeFailure);
  report("probe failure remains untracked",
         status.is(RV3032::Err::DEVICE_NOT_FOUND) &&
             beforeProbeFailure.totalSuccess == afterProbeFailure.totalSuccess &&
             beforeProbeFailure.totalFailures == afterProbeFailure.totalFailures &&
             afterProbeFailure.state == RV3032::DriverState::READY,
         &status);

  gOwner.failReads = 3U;
  uint8_t rawStatus = 0;
  const RV3032::Status failure1 = gRtc.readStatus(rawStatus);
  const RV3032::Status failure2 = gRtc.readStatus(rawStatus);
  const RV3032::Status failure3 = gRtc.readStatus(rawStatus);
  report("tracked failures reach OFFLINE observational state",
         failure1.is(RV3032::Err::I2C_NACK_ADDR) &&
             failure2.is(RV3032::Err::I2C_NACK_ADDR) &&
             failure3.is(RV3032::Err::I2C_NACK_ADDR) &&
             gRtc.state() == RV3032::DriverState::OFFLINE,
         &failure3);
  status = gRtc.readStatus(rawStatus);
  report("OFFLINE allows next operation and success returns READY",
         status.ok() && gRtc.state() == RV3032::DriverState::READY, &status);
}

void runWearLimitedPersistenceTests() {
  const uint32_t callbacksBeforeEnd = gOwner.reads + gOwner.writes;
  gRtc.end();
  report("end is zero-I/O and enters UNINIT",
         gOwner.reads + gOwner.writes == callbacksBeforeEnd &&
             gRtc.state() == RV3032::DriverState::UNINIT);
  uint8_t rawStatus = 0;
  RV3032::Status status = gRtc.readStatus(rawStatus);
  report("operation after end is NOT_INITIALIZED",
         status.is(RV3032::Err::NOT_INITIALIZED), &status);

  RV3032::Config config = makeConfig(true);
  status = gRtc.begin(config);
  reportStatus("begin wear-limited persistence lifecycle", status);
  reportStatus("probe persistence lifecycle", gRtc.probe());

  uint8_t original = 0;
  status = readPersistent(31U, 1U, &original);
  reportStatus("read original user EEPROM byte 31", status);
  if (status.ok()) {
    const uint8_t testValue = static_cast<uint8_t>(original ^ 0xA5U);
    const uint32_t commandsBefore = gOwner.writeOneCommands;
    status = gRtc.startWriteUserEepromJob(31U, &testValue, 1U, millis(),
                                          5000U);
    status = completeJob(status, 6000U);
    uint8_t observed = 0;
    const RV3032::Status verifyStatus = readPersistent(31U, 1U, &observed);
    report("user EEPROM changed once and durably verified",
           status.ok() && verifyStatus.ok() && observed == testValue &&
               gOwner.writeOneCommands == commandsBefore + 1U,
           &status);

    const uint32_t equalCommands = gOwner.writeOneCommands;
    status = gRtc.startWriteUserEepromJob(31U, &testValue, 1U, millis(),
                                          5000U);
    status = completeJob(status, 6000U);
    report("equal user EEPROM value performs zero WRITE_ONE",
           status.ok() && gOwner.writeOneCommands == equalCommands, &status);

    const uint32_t restoreCommands = gOwner.writeOneCommands;
    status = gRtc.startWriteUserEepromJob(31U, &original, 1U, millis(),
                                          5000U);
    status = completeJob(status, 6000U);
    observed = 0;
    const RV3032::Status restoreVerify = readPersistent(31U, 1U, &observed);
    report("user EEPROM restored with one WRITE_ONE",
           status.ok() && restoreVerify.ok() && observed == original &&
               gOwner.writeOneCommands == restoreCommands + 1U,
           &status);
  }

  float originalOffset = 0.0F;
  status = gRtc.getOffsetPpm(originalOffset);
  reportStatus("save persistent offset", status);
  if (status.ok()) {
    const uint32_t equalCommands = gOwner.writeOneCommands;
    status = completeJob(gRtc.setOffsetPpm(originalOffset));
    const RV3032::Status equalDrain = drainEeprom();
    report("equal configuration EEPROM value performs zero WRITE_ONE",
           status.ok() && equalDrain.ok() &&
               gOwner.writeOneCommands == equalCommands,
           &equalDrain);

    const float changedOffset = originalOffset > 0.0F ? 0.0F : 0.238418579F;
    const uint32_t changedCommands = gOwner.writeOneCommands;
    status = completeJob(gRtc.setOffsetPpm(changedOffset));
    const RV3032::Status changedDrain = drainEeprom();
    report("configuration EEPROM changed once",
           status.ok() && changedDrain.ok() &&
               gOwner.writeOneCommands == changedCommands + 1U,
           &changedDrain);

    const uint32_t restoreCommands = gOwner.writeOneCommands;
    status = completeJob(gRtc.setOffsetPpm(originalOffset));
    const RV3032::Status restoreDrain = drainEeprom();
    float observedOffset = 0.0F;
    const RV3032::Status readStatus = gRtc.getOffsetPpm(observedOffset);
    report("configuration EEPROM offset restored",
           status.ok() && restoreDrain.ok() && readStatus.ok() &&
               fabsf(observedOffset - originalOffset) < 0.13F &&
               gOwner.writeOneCommands == restoreCommands + 1U,
           &restoreDrain);
  }

  skip("primary-cell ensure",
       "primary-cell chemistry and board backfeed conditions cannot be proven in software");
}

void restoreFinalCalendar() {
  RV3032::DateTime buildTime{};
  RV3032::Status status = RV3032::RV3032::parseBuildTime(buildTime);
  if (status.ok()) {
    status = gRtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
        buildTime, millis(), 500U);
    status = completeJob(status, 1500U);
  }
  reportStatus("final valid build calendar", status);
  RV3032::ValidityFlags validity{};
  status = gRtc.readValidity(validity);
  report("final validity flags clear",
         status.ok() && !validity.powerOnReset && !validity.voltageLow,
         &status);
}

void runHilSetup() {
  delay(1000U);
  Serial.begin(115200);
  const uint32_t serialDeadline = millis() + 30000U;
  while (!Serial && before(serialDeadline)) {
    delay(10U);
  }
  Serial.println("HIL_BEGIN RV3032-C7 exhaustive COM20 harness v1");
  Serial.printf("[INFO] library=%s build=%s commit=%s\n",
                RV3032::VERSION, RV3032::BUILD_TIMESTAMP,
                RV3032::GIT_COMMIT);

  if (!transport::initWire(8, 9, 400000U, 50U)) {
    report("application-owned I2C initialization", false);
  } else {
    report("application-owned I2C initialization", true);
    runLifecycleAndReadTests();
    runCalendarTests();
    runRamAndRawTests();
    runAlarmTimerUpdateTests();
    runClockCalibrationTemperatureTests();
    runEviControlAndStatusTests();
    runPersistentReadAndHealthTests();
    runWearLimitedPersistenceTests();
    restoreFinalCalendar();
  }

  RV3032::SettingsSnapshot settings{};
  const RV3032::Status settingsStatus = gRtc.getSettings(settings);
  Serial.printf("[INFO] transport reads=%lu writes=%lu WRITE_ONE=%lu\n",
                static_cast<unsigned long>(gOwner.reads),
                static_cast<unsigned long>(gOwner.writes),
                static_cast<unsigned long>(gOwner.writeOneCommands));
  if (settingsStatus.ok()) {
    Serial.printf("[INFO] health success=%lu failures=%lu consecutive=%u state=%u\n",
                  static_cast<unsigned long>(settings.totalSuccess),
                  static_cast<unsigned long>(settings.totalFailures),
                  settings.consecutiveFailures,
                  static_cast<unsigned>(settings.state));
  }
  Serial.printf("HIL_RESULT pass=%lu fail=%lu skip=%lu\n",
                static_cast<unsigned long>(gStats.pass),
                static_cast<unsigned long>(gStats.fail),
                static_cast<unsigned long>(gStats.skip));
  Serial.println("HIL_DONE");
}

}  // namespace

void setup() { runHilSetup(); }

void loop() { delay(1000U); }
