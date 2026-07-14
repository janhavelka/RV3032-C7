#include <stdint.h>

#include <unity.h>

#include "FakeRv3032.h"
#include "RV3032/RV3032.h"

using test_rv3032::FakeRv3032;

namespace {

RV3032::Status pollJobToCompletion(RV3032::RV3032& rtc, FakeRv3032& fake,
                                   uint8_t budget = 1,
                                   uint16_t callCap = 2000) {
  RV3032::Status st = RV3032::Status::Error(RV3032::Err::IN_PROGRESS,
                                             "not started");
  for (uint16_t i = 0; i < callCap; ++i) {
    uint8_t used = 0;
    const uint32_t callbacksBeforePoll = fake.callbackCount;
    st = rtc.pollJob(fake.nowMs, budget, used);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(budget, used);
    TEST_ASSERT_EQUAL_UINT32(
        used, fake.callbackCount - callbacksBeforePoll);
    if (!st.inProgress()) return st;
    ++fake.nowMs;
  }
  return RV3032::Status::Error(RV3032::Err::TIMEOUT, "test poll cap");
}

RV3032::Status pollEepromToCompletion(RV3032::RV3032& rtc,
                                      FakeRv3032& fake,
                                      uint8_t budget = 4,
                                      uint16_t callCap = 3000) {
  RV3032::Status st = RV3032::Status::Error(RV3032::Err::IN_PROGRESS,
                                             "not started");
  for (uint16_t i = 0; i < callCap; ++i) {
    uint8_t used = 0;
    const uint32_t callbacksBeforePoll = fake.callbackCount;
    st = rtc.pollEeprom(fake.nowMs, budget, used);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(budget, used);
    TEST_ASSERT_EQUAL_UINT32(
        used, fake.callbackCount - callbacksBeforePoll);
    if (!st.inProgress()) return st;
    ++fake.nowMs;
  }
  return RV3032::Status::Error(RV3032::Err::TIMEOUT, "test EEPROM poll cap");
}

void assertDateTimeEquals(const RV3032::DateTime& expected,
                          const RV3032::DateTime& actual) {
  TEST_ASSERT_EQUAL_UINT16(expected.year, actual.year);
  TEST_ASSERT_EQUAL_UINT8(expected.month, actual.month);
  TEST_ASSERT_EQUAL_UINT8(expected.day, actual.day);
  TEST_ASSERT_EQUAL_UINT8(expected.hour, actual.hour);
  TEST_ASSERT_EQUAL_UINT8(expected.minute, actual.minute);
  TEST_ASSERT_EQUAL_UINT8(expected.second, actual.second);
  TEST_ASSERT_EQUAL_UINT8(expected.weekday, actual.weekday);
}

size_t countWritesTo(const FakeRv3032& fake, uint8_t reg) {
  size_t count = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write && fake.log[i].reg == reg) ++count;
  }
  return count;
}

size_t countEepromCommands(const FakeRv3032& fake, uint8_t command) {
  size_t count = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write &&
        fake.log[i].reg == RV3032::cmd::REG_EE_COMMAND &&
        fake.log[i].length == 1 && fake.log[i].data[0] == command) {
      ++count;
    }
  }
  return count;
}

uint32_t findEepromCommandOrdinal(const FakeRv3032& fake, uint8_t command) {
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write &&
        fake.log[i].reg == RV3032::cmd::REG_EE_COMMAND &&
        fake.log[i].length == 1 && fake.log[i].data[0] == command) {
      return static_cast<uint32_t>(i + 1U);
    }
  }
  return 0;
}

uint32_t findWriteDataStagingOrdinal(const FakeRv3032& fake) {
  for (size_t commandIndex = 0; commandIndex < fake.logCount;
       ++commandIndex) {
    if (!fake.log[commandIndex].write ||
        fake.log[commandIndex].reg != RV3032::cmd::REG_EE_COMMAND ||
        fake.log[commandIndex].length != 1 ||
        fake.log[commandIndex].data[0] != RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
      continue;
    }
    for (size_t i = commandIndex; i > 0; --i) {
      if (fake.log[i - 1U].write &&
          fake.log[i - 1U].reg == RV3032::cmd::REG_EE_DATA) {
        return static_cast<uint32_t>(i);
      }
    }
  }
  return 0;
}

uint32_t findWriteOrdinal(const FakeRv3032& fake, uint8_t reg,
                          size_t occurrence = 0) {
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write && fake.log[i].reg == reg) {
      if (occurrence == 0) return static_cast<uint32_t>(i + 1U);
      --occurrence;
    }
  }
  return 0;
}

uint32_t findPersistentProofOrdinal(const FakeRv3032& fake,
                                    uint8_t readOneOccurrence) {
  uint8_t readOneCount = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write &&
        fake.log[i].reg == RV3032::cmd::REG_EE_COMMAND &&
        fake.log[i].length == 1 &&
        fake.log[i].data[0] == RV3032::cmd::EEPROM_CMD_READ_ONE) {
      ++readOneCount;
    } else if (readOneCount == readOneOccurrence &&
               !fake.log[i].write &&
               fake.log[i].reg == RV3032::cmd::REG_EE_DATA) {
      return static_cast<uint32_t>(i + 1U);
    }
  }
  return 0;
}

uint32_t findActiveTargetProofOrdinal(const FakeRv3032& fake) {
  uint32_t ordinal = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (!fake.log[i].write &&
        fake.log[i].reg == RV3032::cmd::REG_ACTIVE_PMU) {
      ordinal = static_cast<uint32_t>(i + 1U);
    }
  }
  return ordinal;
}

void assertPrimaryTargetEvidence(
    const RV3032::PrimaryCellConfigurationReport& report,
    bool expectedPersistent, bool expectedActive) {
  TEST_ASSERT_EQUAL(expectedPersistent, report.persistentTargetVerified);
  TEST_ASSERT_EQUAL(expectedActive, report.activeTargetVerified);
  if (report.activeTargetVerified) {
    TEST_ASSERT_TRUE(report.persistentTargetVerified);
  }
}

void assertPrimaryFaultCleanup(const FakeRv3032& fake,
                               const RV3032::RV3032& rtc,
                               const RV3032::Status& status,
                               const RV3032::PrimaryCellConfigurationReport& report,
                               uint32_t ordinal, uint8_t initialActive,
                               uint8_t initialControl1,
                               bool expectedPersistent,
                               bool expectedActive) {
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(ordinal, fake.callbackCount);
  TEST_ASSERT_FALSE(status.ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PrimaryCellConfigurationOutcome::FAILED),
      static_cast<uint8_t>(report.outcome));
  TEST_ASSERT_LESS_OR_EQUAL_UINT16(1, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(0, fake.updateAllAttempts);
  TEST_ASSERT_EQUAL_UINT16(0, fake.refreshAllAttempts);
  TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
  TEST_ASSERT_FALSE(fake.protocolViolation);
  TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
  TEST_ASSERT_FALSE(fake.logOverflow);

  assertPrimaryTargetEvidence(report, expectedPersistent, expectedActive);

  if (report.cleanupVerified) {
    TEST_ASSERT_TRUE(report.cleanupStatus.ok());
    TEST_ASSERT_EQUAL_HEX8(
        fake.direct[RV3032::cmd::REG_CONTROL1] &
            RV3032::cmd::CONTROL1_IMPLEMENTED_MASK,
        report.control1After);
    if (report.autoRefreshHeldDisabledForSafety) {
      TEST_ASSERT_EQUAL_HEX8(
          fake.activeConfig[0] & RV3032::cmd::PMU_IMPLEMENTED_MASK,
          report.activeAfter);
      TEST_ASSERT_EQUAL_HEX8(
          0, fake.activeConfig[0] &
                 static_cast<uint8_t>(RV3032::cmd::PMU_BSM_MASK |
                                      RV3032::cmd::PMU_TCM_MASK));
      TEST_ASSERT_BITS_HIGH(
          RV3032::cmd::CONTROL1_EERD_MASK,
          fake.direct[RV3032::cmd::REG_CONTROL1]);
    } else {
      TEST_ASSERT_EQUAL_HEX8(
          initialActive & RV3032::cmd::PMU_IMPLEMENTED_MASK,
          fake.activeConfig[0] & RV3032::cmd::PMU_IMPLEMENTED_MASK);
      TEST_ASSERT_EQUAL_HEX8(
          initialControl1 & RV3032::cmd::CONTROL1_IMPLEMENTED_MASK,
          fake.direct[RV3032::cmd::REG_CONTROL1] &
              RV3032::cmd::CONTROL1_IMPLEMENTED_MASK);
    }
  } else {
    if (report.cleanupStatus.ok()) {
      // A failure of the very first Control 1 read has no verified cleanup
      // basis and returns before access state has changed.
      TEST_ASSERT_EQUAL_UINT32(1, ordinal);
      TEST_ASSERT_EQUAL_HEX8(
          initialActive & RV3032::cmd::PMU_IMPLEMENTED_MASK,
          fake.activeConfig[0] & RV3032::cmd::PMU_IMPLEMENTED_MASK);
      TEST_ASSERT_EQUAL_HEX8(
          initialControl1 & RV3032::cmd::CONTROL1_IMPLEMENTED_MASK,
          fake.direct[RV3032::cmd::REG_CONTROL1] &
              RV3032::cmd::CONTROL1_IMPLEMENTED_MASK);
    }
  }
}

void test_passive_lifecycle_and_explicit_presence() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(fake.config());
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(0, fake.waitCount);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));

  const RV3032::SettingsSnapshot before = rtc.getSettings();
  RV3032::Config invalid = fake.config();
  invalid.i2cTimeoutMs = 101;
  st = rtc.begin(invalid);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(before.i2cTimeoutMs, rtc.getSettings().i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);

  st = rtc.probe();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(0, rtc.totalSuccess());
  TEST_ASSERT_FALSE(fake.log[0].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, fake.log[0].reg);
  st = rtc.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(1, rtc.totalSuccess());
  TEST_ASSERT_FALSE(fake.log[1].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, fake.log[1].reg);

  rtc.end();
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
  rtc.end();
  TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);

  FakeRv3032 missing;
  missing.failError = RV3032::Err::I2C_NACK_ADDR;
  missing.failOrdinal = 1;
  RV3032::RV3032 missingRtc;
  TEST_ASSERT_TRUE(missingRtc.begin(missing.config()).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
      static_cast<uint8_t>(missingRtc.probe().code));
  TEST_ASSERT_EQUAL_UINT32(0, missingRtc.totalFailures());
  missing.failOrdinal = 2;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
      static_cast<uint8_t>(missingRtc.recover().code));
  TEST_ASSERT_EQUAL_UINT32(1, missingRtc.totalFailures());
}

void test_config_validation_is_zero_io_and_non_destructive() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  RV3032::Config cfg = fake.config();

  cfg.i2cWrite = nullptr;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  cfg = fake.config();
  cfg.i2cWriteRead = nullptr;
  st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  cfg = fake.config();
  cfg.i2cAddress = 0x52;
  st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  cfg = fake.config();
  cfg.i2cTimeoutMs = 0;
  st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  cfg = fake.config();
  cfg.i2cTimeoutMs = 101;
  st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  cfg = fake.config();
  cfg.offlineThreshold = 0;
  st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  cfg = fake.config(true);
  cfg.eepromTimeoutMs = 9;
  st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  cfg = fake.config(true);
  cfg.eepromTimeoutMs = 251;
  st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));

  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(0, fake.waitCount);

  cfg = fake.config(false, false);
  cfg.nowMs = nullptr;
  cfg.i2cTimeoutMs = 1;
  cfg.eepromTimeoutMs = 0;
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(rtc.begin(cfg).ok());
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
  rtc.end();

  cfg = fake.config(true);
  cfg.i2cTimeoutMs = 100;
  cfg.eepromTimeoutMs = 250;
  TEST_ASSERT_TRUE(rtc.begin(cfg).ok());
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
  rtc.end();
  TEST_ASSERT_EQUAL_UINT32(0, fake.waitCount);
}

void test_end_preserves_active_work_with_zero_io() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  TEST_ASSERT_TRUE(rtc.setClockInterruptEnabled(true).inProgress());
  const uint32_t beforeEnd = fake.callbackCount;
  rtc.end();
  TEST_ASSERT_TRUE(rtc.isInitialized());
  TEST_ASSERT_TRUE(rtc.isJobBusy());
  TEST_ASSERT_EQUAL_UINT32(beforeEnd, fake.callbackCount);
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  rtc.end();
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(beforeEnd + 2, fake.callbackCount);
}

void test_tick_zero_budget_and_eeprom_end_guards() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  rtc.tick(fake.nowMs);
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);

  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  TEST_ASSERT_TRUE(rtc.setClockInterruptEnabled(true).inProgress());
  uint8_t used = 99;
  const uint32_t ordinaryCallbacks = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(rtc.pollEeprom(fake.nowMs, 1, used).code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  rtc.tick(fake.nowMs);
  TEST_ASSERT_EQUAL_UINT32(ordinaryCallbacks, fake.callbackCount);
  TEST_ASSERT_TRUE(rtc.isJobBusy());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());

  TEST_ASSERT_TRUE(rtc.setOffsetPpm(0.2384f).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());
  const uint32_t queuedCallbacks = fake.callbackCount;
  rtc.end();
  TEST_ASSERT_TRUE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT32(queuedCallbacks, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(rtc.startReadTimeSnapshotJob(fake.nowMs).code));
  TEST_ASSERT_EQUAL_UINT32(queuedCallbacks, fake.callbackCount);

  used = 99;
  TEST_ASSERT_TRUE(rtc.pollEeprom(fake.nowMs, 0, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(queuedCallbacks, fake.callbackCount);
  rtc.tick(fake.nowMs);
  TEST_ASSERT_EQUAL_UINT32(queuedCallbacks + 1U, fake.callbackCount);
  TEST_ASSERT_TRUE(rtc.isJobBusy());
  TEST_ASSERT_TRUE(rtc.isEepromBusy());

  const uint32_t activeCallbacks = fake.callbackCount;
  used = 99;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(rtc.pollJob(fake.nowMs, 1, used).code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  rtc.end();
  TEST_ASSERT_TRUE(rtc.isInitialized());
  TEST_ASSERT_TRUE(rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_UINT32(activeCallbacks, fake.callbackCount);

  TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake).ok());
  const uint32_t completedCallbacks = fake.callbackCount;
  rtc.tick(fake.nowMs);
  TEST_ASSERT_EQUAL_UINT32(completedCallbacks, fake.callbackCount);
  rtc.end();
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(completedCallbacks, fake.callbackCount);
}

void test_offline_is_observational() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  RV3032::Config cfg = fake.config();
  cfg.offlineThreshold = 2;
  TEST_ASSERT_TRUE(rtc.begin(cfg).ok());
  fake.failOrdinal = 1;
  uint8_t status = 0;
  TEST_ASSERT_FALSE(rtc.readStatus(status).ok());
  fake.failOrdinal = 2;
  TEST_ASSERT_FALSE(rtc.readStatus(status).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::OFFLINE),
                          static_cast<uint8_t>(rtc.state()));
  fake.failOrdinal = 0;
  TEST_ASSERT_TRUE(rtc.recover().ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
}

void test_read_time_is_strict_single_transfer() {
  FakeRv3032 fake;
  fake.setCalendar(2026, 7, 13, 12, 34, 56, 1);
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  RV3032::DateTime value{};
  TEST_ASSERT_TRUE(rtc.readTime(value).ok());
  TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT16(2026, value.year);
  fake.direct[RV3032::cmd::REG_HOURS] |= 0x80;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                          static_cast<uint8_t>(rtc.readTime(value).code));
  fake.direct[RV3032::cmd::REG_HOURS] &= 0x3F;
  fake.direct[RV3032::cmd::REG_WEEKDAY] = 2;
  TEST_ASSERT_TRUE(rtc.readTime(value).ok());
  TEST_ASSERT_EQUAL_UINT8(2, value.weekday);
}

void test_calendar_weekday_is_user_assigned_and_range_only() {
  for (uint8_t weekday = 0; weekday <= 6; ++weekday) {
    FakeRv3032 fake;
    fake.setCalendar(2026, 7, 13, 12, 34, 56, weekday);
    fake.direct[RV3032::cmd::REG_STATUS] = 0;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());

    RV3032::DateTime read{};
    TEST_ASSERT_TRUE(rtc.readTime(read).ok());
    TEST_ASSERT_EQUAL_UINT8(weekday, read.weekday);

    TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    RV3032::TimeSnapshot snapshot{};
    TEST_ASSERT_TRUE(rtc.getReadTimeSnapshotJobResult(snapshot).ok());
    TEST_ASSERT_TRUE(snapshot.timeValid);
    TEST_ASSERT_EQUAL_UINT8(weekday, snapshot.time.weekday);

    const RV3032::DateTime requested{2026, 7, 13, 12, 34, 56, weekday};
    const uint32_t beforeSet = fake.callbackCount;
    TEST_ASSERT_TRUE(rtc.setTime(requested).ok());
    TEST_ASSERT_EQUAL_UINT32(beforeSet + 1U, fake.callbackCount);
    TEST_ASSERT_TRUE(fake.log[fake.logCount - 1U].write);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_SECONDS,
                           fake.log[fake.logCount - 1U].reg);
    TEST_ASSERT_EQUAL_HEX8(FakeRv3032::bcd(weekday),
                           fake.log[fake.logCount - 1U].data[3]);

    TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
        requested, fake.nowMs, 250).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    RV3032::VerifiedTimeSetReport report{};
    TEST_ASSERT_TRUE(
        rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).ok());
    TEST_ASSERT_EQUAL_UINT8(weekday, report.requested.weekday);
    TEST_ASSERT_EQUAL_UINT8(weekday, report.verified.weekday);
    const size_t calendarWriteIndex = fake.logCount - 6U;
    TEST_ASSERT_TRUE(fake.log[calendarWriteIndex].write);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_SECONDS,
                           fake.log[calendarWriteIndex].reg);
    TEST_ASSERT_EQUAL_HEX8(FakeRv3032::bcd(weekday),
                           fake.log[calendarWriteIndex].data[3]);
  }

  FakeRv3032 fake;
  fake.setCalendar(2026, 7, 13, 12, 34, 56, 1);
  fake.direct[RV3032::cmd::REG_STATUS] = 0;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  const RV3032::DateTime invalidWeekday{2026, 7, 13, 12, 34, 56, 7};
  RV3032::DateTime unchanged{};
  const uint32_t beforeInvalidSet = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.setTime(invalidWeekday).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
          invalidWeekday, fake.nowMs, 250).code));
  TEST_ASSERT_EQUAL_UINT32(beforeInvalidSet, fake.callbackCount);

  fake.direct[RV3032::cmd::REG_WEEKDAY] = 0x07;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.readTime(unchanged).code));
  fake.direct[RV3032::cmd::REG_WEEKDAY] = 0x08;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.readTime(unchanged).code));
  fake.direct[RV3032::cmd::REG_WEEKDAY] = 0x0A;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.readTime(unchanged).code));
}

void test_status_first_snapshot_job_and_result_contract() {
  FakeRv3032 fake;
  fake.setCalendar(2026, 7, 13, 1, 2, 3, 1);
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  RV3032::TimeSnapshot unchanged{};
  unchanged.statusRaw = 0xA5;
  unchanged.statusFlags.powerOnReset = true;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
      static_cast<uint8_t>(rtc.getReadTimeSnapshotJobResult(unchanged).code));
  TEST_ASSERT_EQUAL_HEX8(0xA5, unchanged.statusRaw);
  TEST_ASSERT_TRUE(unchanged.statusFlags.powerOnReset);

  TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs).inProgress());
  const uint32_t admittedCallbacks = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(rtc.startReadTimeSnapshotJob(fake.nowMs).code));
  TEST_ASSERT_EQUAL_UINT32(admittedCallbacks, fake.callbackCount);
  RV3032::TimeSnapshot inProgress{};
  inProgress.statusRaw = 0x5A;
  inProgress.statusFlags.voltageLow = true;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
      static_cast<uint8_t>(rtc.getReadTimeSnapshotJobResult(inProgress).code));
  TEST_ASSERT_EQUAL_HEX8(0x5A, inProgress.statusRaw);
  TEST_ASSERT_TRUE(inProgress.statusFlags.voltageLow);
  uint8_t used = 99;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 0, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::TimeSnapshot result{};
  TEST_ASSERT_TRUE(rtc.getReadTimeSnapshotJobResult(result).ok());
  TEST_ASSERT_TRUE(result.statusValid);
  TEST_ASSERT_FALSE(result.timeValid);
  TEST_ASSERT_TRUE(result.statusFlags.powerOnReset);
  TEST_ASSERT_TRUE(result.statusFlags.voltageLow);
  TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
  const uint32_t beforeRejectedStart = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.startReadTimeSnapshotJob(fake.nowMs, 0).code));
  TEST_ASSERT_EQUAL_UINT32(beforeRejectedStart, fake.callbackCount);
  RV3032::TimeSnapshot retained{};
  TEST_ASSERT_TRUE(rtc.getReadTimeSnapshotJobResult(retained).ok());
  TEST_ASSERT_EQUAL_HEX8(result.statusRaw, retained.statusRaw);

  fake.direct[RV3032::cmd::REG_STATUS] = 0;
  TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs).inProgress());
  const uint32_t beforeFullSnapshot = fake.callbackCount;
  used = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 2, used).ok());
  TEST_ASSERT_EQUAL_UINT8(2, used);
  TEST_ASSERT_EQUAL_UINT32(beforeFullSnapshot + 2U, fake.callbackCount);
  TEST_ASSERT_TRUE(rtc.getReadTimeSnapshotJobResult(result).ok());
  TEST_ASSERT_TRUE(result.timeValid);
  TEST_ASSERT_FALSE(result.statusFlags.powerOnReset);
  TEST_ASSERT_FALSE(result.statusFlags.voltageLow);

  fake.failOrdinal = fake.callbackCount + 2U;
  TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs).inProgress());
  const RV3032::Status failed = rtc.pollJob(fake.nowMs, 2, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(failed.code));
  RV3032::TimeSnapshot partial{};
  const RV3032::Status partialStatus = rtc.getReadTimeSnapshotJobResult(partial);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(partialStatus.code));
  TEST_ASSERT_TRUE(partial.statusValid);
  TEST_ASSERT_FALSE(partial.timeValid);
  TEST_ASSERT_FALSE(partial.statusFlags.powerOnReset);
  TEST_ASSERT_FALSE(partial.statusFlags.voltageLow);

  fake.failOrdinal = 0;
  TEST_ASSERT_TRUE(rtc.startSetTimerJob(
      1, RV3032::TimerFrequency::Hz1, false).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::TimeSnapshot wrongKind{};
  wrongKind.statusRaw = 0x96;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
      static_cast<uint8_t>(rtc.getReadTimeSnapshotJobResult(wrongKind).code));
  TEST_ASSERT_EQUAL_HEX8(0x96, wrongKind.statusRaw);
}

void test_status_first_snapshot_rejects_every_invalid_calendar_encoding() {
  struct InvalidEncoding {
    uint8_t reg;
    uint8_t value;
  };
  const InvalidEncoding cases[] = {
      {RV3032::cmd::REG_SECONDS, 0x80},
      {RV3032::cmd::REG_MINUTES, 0x80},
      {RV3032::cmd::REG_HOURS, 0x40},
      {RV3032::cmd::REG_WEEKDAY, 0x08},
      {RV3032::cmd::REG_DATE, 0x40},
      {RV3032::cmd::REG_MONTH, 0x20},
      {RV3032::cmd::REG_SECONDS, 0x6A},
      {RV3032::cmd::REG_DATE, 0x32},
      {RV3032::cmd::REG_MONTH, 0x13},
      {RV3032::cmd::REG_YEAR, 0xFA},
      {RV3032::cmd::REG_WEEKDAY, 0x07},
      {RV3032::cmd::REG_WEEKDAY, 0x0A},
  };

  for (const InvalidEncoding& invalid : cases) {
    FakeRv3032 fake;
    fake.setCalendar(2026, 7, 13, 1, 2, 3, 1);
    fake.direct[RV3032::cmd::REG_STATUS] = 0;
    fake.direct[invalid.reg] = invalid.value;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs).inProgress());
    uint8_t used = 0;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 2, used);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(2, used);
    RV3032::TimeSnapshot result{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
        static_cast<uint8_t>(rtc.getReadTimeSnapshotJobResult(result).code));
    TEST_ASSERT_TRUE(result.statusValid);
    TEST_ASSERT_FALSE(result.timeValid);
  }
}

void test_status_first_snapshot_exposes_typed_invalid_flags() {
  for (uint8_t raw = 0; raw <= 3; ++raw) {
    FakeRv3032 fake;
    fake.setCalendar(2026, 7, 13, 1, 2, 3, 6);
    fake.direct[RV3032::cmd::REG_STATUS] = raw;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs).inProgress());

    uint8_t used = 0;
    const RV3032::Status first = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(1, used);
    const bool porf = (raw & 0x02u) != 0;
    const bool vlf = (raw & 0x01u) != 0;
    if (porf || vlf) {
      TEST_ASSERT_TRUE(first.ok());
    } else {
      TEST_ASSERT_TRUE(first.inProgress());
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).ok());
      TEST_ASSERT_EQUAL_UINT8(1, used);
    }

    RV3032::TimeSnapshot result{};
    TEST_ASSERT_TRUE(rtc.getReadTimeSnapshotJobResult(result).ok());
    TEST_ASSERT_EQUAL_HEX8(raw, result.statusRaw);
    TEST_ASSERT_TRUE(result.statusValid);
    TEST_ASSERT_EQUAL(porf, result.statusFlags.powerOnReset);
    TEST_ASSERT_EQUAL(vlf, result.statusFlags.voltageLow);
    TEST_ASSERT_EQUAL(!(porf || vlf), result.timeValid);
    TEST_ASSERT_EQUAL_UINT32(porf || vlf ? 1U : 2U, fake.callbackCount);
  }

  FakeRv3032 failedFake;
  failedFake.failOrdinal = 1;
  RV3032::RV3032 failedRtc;
  TEST_ASSERT_TRUE(failedRtc.begin(failedFake.config()).ok());
  TEST_ASSERT_TRUE(
      failedRtc.startReadTimeSnapshotJob(failedFake.nowMs).inProgress());
  uint8_t used = 0;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::I2C_BUS),
      static_cast<uint8_t>(
          failedRtc.pollJob(failedFake.nowMs, 1, used).code));
  TEST_ASSERT_EQUAL_UINT8(1, used);
  RV3032::TimeSnapshot failed{};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::I2C_BUS),
      static_cast<uint8_t>(
          failedRtc.getReadTimeSnapshotJobResult(failed).code));
  TEST_ASSERT_FALSE(failed.statusValid);
  TEST_ASSERT_FALSE(failed.timeValid);
  TEST_ASSERT_FALSE(failed.statusFlags.powerOnReset);
  TEST_ASSERT_FALSE(failed.statusFlags.voltageLow);
}

void test_verified_calendar_accepts_user_assigned_readback_weekday() {
  FakeRv3032 fake;
  fake.direct[RV3032::cmd::REG_STATUS] = 0;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  const RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 6};
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      requested, fake.nowMs, 250).inProgress());

  uint8_t used = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  fake.setCalendar(2026, 7, 13, 12, 0, 0, 2);
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());

  RV3032::VerifiedTimeSetReport report{};
  TEST_ASSERT_TRUE(
      rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).ok());
  TEST_ASSERT_EQUAL_UINT8(6, report.requested.weekday);
  TEST_ASSERT_EQUAL_UINT8(2, report.verified.weekday);
  TEST_ASSERT_EQUAL_HEX8(FakeRv3032::bcd(6), fake.log[1].data[3]);
}

void test_verified_calendar_status_payload_is_fixed_for_every_status() {
  for (uint16_t raw = 0; raw <= 0xFFu; ++raw) {
    FakeRv3032 fake;
    fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
    fake.direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(raw);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    const RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 4};
    TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
        requested, fake.nowMs, 250).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());

    TEST_ASSERT_EQUAL_UINT32(7, fake.logCount);
    TEST_ASSERT_TRUE(fake.log[4].write);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, fake.log[4].reg);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::STATUS_CLEAR_INVALID_TIME_VALUE,
                           fake.log[4].data[0]);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(raw) & 0x3Cu,
                           fake.direct[RV3032::cmd::REG_STATUS]);
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_SECONDS));
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_STATUS));
  }
}

void test_verified_calendar_preserves_flags_set_between_read_and_write() {
  const uint8_t eventBits[] = {
      static_cast<uint8_t>(1u << RV3032::cmd::STATUS_UF_BIT),
      static_cast<uint8_t>(1u << RV3032::cmd::STATUS_TF_BIT),
      static_cast<uint8_t>(1u << RV3032::cmd::STATUS_AF_BIT),
      static_cast<uint8_t>(1u << RV3032::cmd::STATUS_EVF_BIT),
  };
  for (uint8_t eventBit : eventBits) {
    FakeRv3032 fake;
    fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
    fake.direct[RV3032::cmd::REG_STATUS] = 0x03;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    const RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 5};
    TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
        requested, fake.nowMs, 250).inProgress());

    uint8_t used = 0;
    for (uint8_t callback = 0; callback < 4; ++callback) {
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(1, used);
    }
    fake.direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
        fake.direct[RV3032::cmd::REG_STATUS] | eventBit);
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_BITS_HIGH(eventBit,
                          fake.direct[RV3032::cmd::REG_STATUS]);
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    TEST_ASSERT_BITS_HIGH(eventBit,
                          fake.direct[RV3032::cmd::REG_STATUS]);
    TEST_ASSERT_BITS_LOW(0x03, fake.direct[RV3032::cmd::REG_STATUS]);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::STATUS_CLEAR_INVALID_TIME_VALUE,
                           fake.log[4].data[0]);
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_SECONDS));
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_STATUS));
  }
}

void test_verified_calendar_set_reports_status_side_effects() {
  FakeRv3032 fake;
  fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
  fake.direct[RV3032::cmd::REG_STATUS] = 0xFF;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());

  RV3032::VerifiedTimeSetReport unavailable{};
  unavailable.statusBefore = 0xA5;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
      static_cast<uint8_t>(
          rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(unavailable).code));
  TEST_ASSERT_EQUAL_HEX8(0xA5, unavailable.statusBefore);

  RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 6};
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      requested, fake.nowMs, 250).inProgress());

  RV3032::VerifiedTimeSetReport inProgress{};
  inProgress.statusAfter = 0x5A;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
      static_cast<uint8_t>(
          rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(inProgress).code));
  TEST_ASSERT_EQUAL_HEX8(0x5A, inProgress.statusAfter);

  uint8_t used = 99;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 0, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);

  RV3032::Status st = RV3032::Status::Error(
      RV3032::Err::IN_PROGRESS, "not polled");
  for (uint8_t phase = 0; phase < 7; ++phase) {
    st = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(phase + 1U),
                             fake.callbackCount);
    if (phase < 6) {
      TEST_ASSERT_TRUE(st.inProgress());
    }
  }
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(7, fake.logCount);
  TEST_ASSERT_FALSE(fake.log[0].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, fake.log[0].reg);
  TEST_ASSERT_TRUE(fake.log[1].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_SECONDS, fake.log[1].reg);
  TEST_ASSERT_EQUAL_UINT8(7, fake.log[1].length);
  TEST_ASSERT_FALSE(fake.log[2].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_SECONDS, fake.log[2].reg);
  TEST_ASSERT_FALSE(fake.log[3].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, fake.log[3].reg);
  TEST_ASSERT_TRUE(fake.log[4].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, fake.log[4].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::STATUS_CLEAR_INVALID_TIME_VALUE,
                         fake.log[4].data[0]);
  TEST_ASSERT_FALSE(fake.log[5].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, fake.log[5].reg);
  TEST_ASSERT_FALSE(fake.log[6].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_SECONDS, fake.log[6].reg);
  TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_SECONDS));
  TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_STATUS));

  RV3032::VerifiedTimeSetReport report{};
  TEST_ASSERT_TRUE(rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).ok());
  TEST_ASSERT_TRUE(report.calendarWriteAttempted);
  TEST_ASSERT_TRUE(report.statusWriteAttempted);
  TEST_ASSERT_TRUE(report.calendarWriteStatus.ok());
  TEST_ASSERT_TRUE(report.statusWriteStatus.ok());
  TEST_ASSERT_FALSE(report.calendarWriteAmbiguous);
  TEST_ASSERT_FALSE(report.statusWriteAmbiguous);
  TEST_ASSERT_FALSE(report.verifiedAfterAmbiguousWrite);
  TEST_ASSERT_TRUE(report.temperatureHighWasSetBeforeClear);
  TEST_ASSERT_TRUE(report.temperatureLowWasSetBeforeClear);
  TEST_ASSERT_TRUE(report.verifiedValid);
  TEST_ASSERT_TRUE(report.statusBeforeValid);
  TEST_ASSERT_TRUE(report.statusBeforeClearValid);
  TEST_ASSERT_TRUE(report.statusAfterValid);
  TEST_ASSERT_EQUAL_HEX8(0xFF, report.statusBefore);
  TEST_ASSERT_EQUAL_HEX8(0xFF, report.statusBeforeClear);
  TEST_ASSERT_EQUAL_HEX8(0x3C, report.statusAfter);
  TEST_ASSERT_EQUAL_HEX8(0x3C, fake.direct[RV3032::cmd::REG_STATUS]);
  assertDateTimeEquals(requested, report.requested);
  assertDateTimeEquals(requested, report.verified);

  const RV3032::DateTime rejected{2026, 2, 30, 12, 0, 0, 0};
  const uint32_t beforeRejectedStart = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
          rejected, fake.nowMs, 250).code));
  TEST_ASSERT_EQUAL_UINT32(beforeRejectedStart, fake.callbackCount);
  RV3032::VerifiedTimeSetReport retained{};
  TEST_ASSERT_TRUE(
      rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(retained).ok());
  assertDateTimeEquals(requested, retained.verified);

  TEST_ASSERT_TRUE(rtc.startSetTimerJob(
      1, RV3032::TimerFrequency::Hz1, false).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::VerifiedTimeSetReport wrongKind{};
  wrongKind.statusBefore = 0x96;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
      static_cast<uint8_t>(
          rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(wrongKind).code));
  TEST_ASSERT_EQUAL_HEX8(0x96, wrongKind.statusBefore);
}

void test_verified_calendar_set_validation_and_wrapped_deadline_are_zero_io() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());

  const RV3032::DateTime invalid{2026, 2, 30, 12, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
          invalid, fake.nowMs, 250).code));
  const RV3032::DateTime valid{2026, 7, 13, 12, 0, 0, 0};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
          valid, fake.nowMs, 125).code));
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);

  fake.nowMs = UINT32_MAX - 249U;
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      valid, fake.nowMs, 250).inProgress());
  const uint32_t wrappedDeadline = fake.nowMs + 250U;
  fake.nowMs = wrappedDeadline;
  uint8_t used = 99;
  const RV3032::Status deadline = rtc.pollJob(fake.nowMs, 7, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(deadline.code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);

  RV3032::VerifiedTimeSetReport report{};
  const RV3032::Status result =
      rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(result.code));
  TEST_ASSERT_FALSE(report.calendarWriteAttempted);
  TEST_ASSERT_FALSE(report.statusWriteAttempted);
  TEST_ASSERT_FALSE(report.verifiedValid);
  TEST_ASSERT_FALSE(report.statusBeforeValid);
  TEST_ASSERT_FALSE(report.statusBeforeClearValid);
  TEST_ASSERT_FALSE(report.statusAfterValid);

  ++fake.nowMs;
  used = 99;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::TIMEOUT),
      static_cast<uint8_t>(rtc.pollJob(fake.nowMs, 7, used).code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
}

void test_calendar_multi_instruction_budget_refreshes_deadlines_and_cutoffs() {
  {
    FakeRv3032 fake;
    fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
    fake.direct[RV3032::cmd::REG_STATUS] = 0x03;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    const RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 6};
    TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
        requested, fake.nowMs, 250).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 7, used).ok());
    TEST_ASSERT_EQUAL_UINT8(7, used);
    TEST_ASSERT_EQUAL_UINT32(7, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_SECONDS));
    TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_STATUS));
  }

  {
    FakeRv3032 fake;
    fake.setCalendar(2026, 7, 13, 12, 0, 0, 1);
    fake.direct[RV3032::cmd::REG_STATUS] = 0;
    fake.callbackDurationMs = 5;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs, 5).inProgress());
    uint8_t used = 0;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 2, used);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
    RV3032::TimeSnapshot partial{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TIMEOUT),
        static_cast<uint8_t>(rtc.getReadTimeSnapshotJobResult(partial).code));
    TEST_ASSERT_TRUE(partial.statusValid);
    TEST_ASSERT_FALSE(partial.timeValid);
  }

  {
    FakeRv3032 fake;
    fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
    fake.callbackDurationMs = 5;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    const RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 0};
    TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
        requested, fake.nowMs, 126).inProgress());
    uint8_t used = 0;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 7, used);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(0, countWritesTo(fake, RV3032::cmd::REG_SECONDS));
    RV3032::VerifiedTimeSetReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TIMEOUT),
        static_cast<uint8_t>(
            rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).code));
    TEST_ASSERT_TRUE(report.statusBeforeValid);
    TEST_ASSERT_FALSE(report.calendarWriteAttempted);
    TEST_ASSERT_FALSE(report.statusWriteAttempted);
  }
}

void test_verified_calendar_accepts_one_second_rollover() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  const RV3032::DateTime requested{
      2026, 12, 31, 23, 59, 59,
      RV3032::RV3032::computeWeekday(2026, 12, 31)};
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      requested, fake.nowMs, 250).inProgress());

  uint8_t used = 0;
  for (uint8_t phase = 0; phase < 2; ++phase) {
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(1, used);
  }
  const RV3032::DateTime rolled{
      2027, 1, 1, 0, 0, 0,
      RV3032::RV3032::computeWeekday(2027, 1, 1)};
  fake.setCalendar(rolled.year, rolled.month, rolled.day, rolled.hour,
                   rolled.minute, rolled.second, rolled.weekday);
  RV3032::Status st = RV3032::Status::Error(
      RV3032::Err::IN_PROGRESS, "not polled");
  for (uint8_t phase = 2; phase < 7; ++phase) {
    st = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(1, used);
  }
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(7, fake.callbackCount);
  RV3032::VerifiedTimeSetReport report{};
  TEST_ASSERT_TRUE(
      rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).ok());
  TEST_ASSERT_TRUE(report.verifiedValid);
  assertDateTimeEquals(requested, report.requested);
  assertDateTimeEquals(rolled, report.verified);
}

void test_verified_calendar_rejects_backward_final_read() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  const RV3032::DateTime requested{
      2026, 7, 13, 12, 0, 0,
      RV3032::RV3032::computeWeekday(2026, 7, 13)};
  const RV3032::DateTime first{
      2026, 7, 13, 12, 0, 1,
      RV3032::RV3032::computeWeekday(2026, 7, 13)};
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      requested, fake.nowMs, 250).inProgress());

  uint8_t used = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  fake.setCalendar(first.year, first.month, first.day, first.hour, first.minute,
                   first.second, first.weekday);
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  for (uint8_t phase = 3; phase < 6; ++phase) {
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  }
  fake.setCalendar(requested.year, requested.month, requested.day,
                   requested.hour, requested.minute, requested.second,
                   requested.weekday);
  const RV3032::Status st = rtc.pollJob(fake.nowMs, 1, used);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_EQUAL_UINT32(7, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_SECONDS));
  TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_STATUS));

  RV3032::VerifiedTimeSetReport report{};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(
          rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).code));
  TEST_ASSERT_TRUE(report.verifiedValid);
  assertDateTimeEquals(first, report.verified);
}

void test_verified_calendar_rejects_two_second_final_read() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  const RV3032::DateTime requested{
      2026, 7, 13, 12, 0, 0,
      RV3032::RV3032::computeWeekday(2026, 7, 13)};
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      requested, fake.nowMs, 250).inProgress());
  uint8_t used = 0;
  for (uint8_t phase = 0; phase < 6; ++phase) {
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(1, used);
  }
  fake.setCalendar(2026, 7, 13, 12, 0, 2, requested.weekday);
  const RV3032::Status st = rtc.pollJob(fake.nowMs, 1, used);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_EQUAL_UINT32(7, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_SECONDS));
  TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_STATUS));
}

void test_verified_calendar_rejects_2099_hardware_wrap() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  const RV3032::DateTime requested{
      2099, 12, 31, 23, 59, 59,
      RV3032::RV3032::computeWeekday(2099, 12, 31)};
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      requested, fake.nowMs, 250).inProgress());
  uint8_t used = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  fake.setCalendar(2000, 1, 1, 0, 0, 0,
                   RV3032::RV3032::computeWeekday(2000, 1, 1));
  const RV3032::Status st = rtc.pollJob(fake.nowMs, 1, used);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_SECONDS));
  TEST_ASSERT_EQUAL_UINT32(0, countWritesTo(fake, RV3032::cmd::REG_STATUS));

  RV3032::VerifiedTimeSetReport report{};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(
          rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).code));
  TEST_ASSERT_FALSE(report.verifiedValid);
  TEST_ASSERT_FALSE(report.statusWriteAttempted);
}

void test_verified_calendar_reconciles_ambiguous_write_errors() {
  const RV3032::Err errors[] = {
      RV3032::Err::I2C_NACK_DATA,
      RV3032::Err::I2C_TIMEOUT,
      RV3032::Err::I2C_BUS,
  };
  const RV3032::DateTime requested{
      2026, 7, 13, 12, 0, 0,
      RV3032::RV3032::computeWeekday(2026, 7, 13)};

  for (RV3032::Err error : errors) {
    {
      FakeRv3032 fake;
      fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
      fake.direct[RV3032::cmd::REG_STATUS] = 0xFF;
      fake.failOrdinal = 2;
      fake.failError = error;
      fake.failWriteAfterCommit = true;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
      TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
          requested, fake.nowMs, 250).inProgress());
      TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
      RV3032::VerifiedTimeSetReport report{};
      TEST_ASSERT_TRUE(
          rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).ok());
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(error),
                              static_cast<uint8_t>(report.calendarWriteStatus.code));
      TEST_ASSERT_TRUE(report.calendarWriteAttempted);
      TEST_ASSERT_TRUE(report.calendarWriteAmbiguous);
      TEST_ASSERT_TRUE(report.verifiedAfterAmbiguousWrite);
      TEST_ASSERT_TRUE(report.verifiedValid);
      TEST_ASSERT_TRUE(report.statusWriteStatus.ok());
      TEST_ASSERT_EQUAL_UINT32(1,
          countWritesTo(fake, RV3032::cmd::REG_SECONDS));
      TEST_ASSERT_EQUAL_UINT32(1,
          countWritesTo(fake, RV3032::cmd::REG_STATUS));
    }

    {
      FakeRv3032 fake;
      fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
      fake.failOrdinal = 2;
      fake.failError = error;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
      TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
          requested, fake.nowMs, 250).inProgress());
      const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
          static_cast<uint8_t>(terminal.code));
      RV3032::VerifiedTimeSetReport report{};
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
          static_cast<uint8_t>(
              rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).code));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(error),
                              static_cast<uint8_t>(report.calendarWriteStatus.code));
      TEST_ASSERT_TRUE(report.calendarWriteAttempted);
      TEST_ASSERT_TRUE(report.calendarWriteAmbiguous);
      TEST_ASSERT_FALSE(report.verifiedValid);
      TEST_ASSERT_FALSE(report.statusWriteAttempted);
      TEST_ASSERT_FALSE(report.verifiedAfterAmbiguousWrite);
      TEST_ASSERT_EQUAL_UINT32(1,
          countWritesTo(fake, RV3032::cmd::REG_SECONDS));
      TEST_ASSERT_EQUAL_UINT32(0,
          countWritesTo(fake, RV3032::cmd::REG_STATUS));
    }

    {
      FakeRv3032 fake;
      fake.direct[RV3032::cmd::REG_STATUS] = 0xFF;
      fake.failOrdinal = 5;
      fake.failError = error;
      fake.failWriteAfterCommit = true;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
      TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
          requested, fake.nowMs, 250).inProgress());
      TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
      RV3032::VerifiedTimeSetReport report{};
      TEST_ASSERT_TRUE(
          rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).ok());
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(error),
                              static_cast<uint8_t>(report.statusWriteStatus.code));
      TEST_ASSERT_TRUE(report.statusWriteAttempted);
      TEST_ASSERT_TRUE(report.statusWriteAmbiguous);
      TEST_ASSERT_TRUE(report.verifiedAfterAmbiguousWrite);
      TEST_ASSERT_TRUE(report.statusAfterValid);
      TEST_ASSERT_EQUAL_HEX8(0x3C, report.statusAfter);
      TEST_ASSERT_EQUAL_UINT32(1,
          countWritesTo(fake, RV3032::cmd::REG_SECONDS));
      TEST_ASSERT_EQUAL_UINT32(1,
          countWritesTo(fake, RV3032::cmd::REG_STATUS));
    }

    {
      FakeRv3032 fake;
      fake.direct[RV3032::cmd::REG_STATUS] = 0xFF;
      fake.failOrdinal = 5;
      fake.failError = error;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
      TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
          requested, fake.nowMs, 250).inProgress());
      const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
          static_cast<uint8_t>(terminal.code));
      RV3032::VerifiedTimeSetReport report{};
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
          static_cast<uint8_t>(
              rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).code));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(error),
                              static_cast<uint8_t>(report.statusWriteStatus.code));
      TEST_ASSERT_TRUE(report.statusWriteAttempted);
      TEST_ASSERT_TRUE(report.statusWriteAmbiguous);
      TEST_ASSERT_FALSE(report.verifiedAfterAmbiguousWrite);
      TEST_ASSERT_TRUE(report.verifiedValid);
      TEST_ASSERT_TRUE(report.statusAfterValid);
      TEST_ASSERT_EQUAL_HEX8(0xFF, report.statusAfter);
      TEST_ASSERT_EQUAL_UINT32(1,
          countWritesTo(fake, RV3032::cmd::REG_SECONDS));
      TEST_ASSERT_EQUAL_UINT32(1,
          countWritesTo(fake, RV3032::cmd::REG_STATUS));
    }
  }
}

void test_verified_calendar_retains_proven_status_write_on_later_failure() {
  FakeRv3032 fake;
  fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
  fake.direct[RV3032::cmd::REG_STATUS] = 0xFF;
  fake.failOrdinal = 5;
  fake.failError = RV3032::Err::I2C_TIMEOUT;
  fake.failWriteAfterCommit = true;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  const RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 0};
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      requested, fake.nowMs, 250).inProgress());

  uint8_t used = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 6, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(6, used);
  fake.failOrdinal = 7;
  fake.failError = RV3032::Err::I2C_BUS;
  const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 1, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(terminal.code));
  TEST_ASSERT_EQUAL_UINT8(1, used);

  RV3032::VerifiedTimeSetReport report{};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::I2C_BUS),
      static_cast<uint8_t>(
          rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).code));
  TEST_ASSERT_TRUE(report.statusWriteAttempted);
  TEST_ASSERT_TRUE(report.statusWriteAmbiguous);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(report.statusWriteStatus.code));
  TEST_ASSERT_TRUE(report.statusAfterValid);
  TEST_ASSERT_EQUAL_HEX8(0x3C, report.statusAfter);
  TEST_ASSERT_TRUE(report.verifiedAfterAmbiguousWrite);
  TEST_ASSERT_TRUE(report.verifiedValid);
}

void test_job_budget_and_timer_reserved_bits() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  TEST_ASSERT_TRUE(rtc.startSetTimerJob(0xABC, RV3032::TimerFrequency::Hz1,
                                       true).inProgress());
  uint8_t used = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 2, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(2, used);
  RV3032::Status st = pollJobToCompletion(rtc, fake, 1);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0x0A, fake.direct[RV3032::cmd::REG_TIMER_HIGH]);
  TEST_ASSERT_EQUAL_HEX8(0xBC, fake.direct[RV3032::cmd::REG_TIMER_LOW]);
}

void test_raw_access_allowlists_block_side_effect_routes() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  uint8_t value = 0;
  TEST_ASSERT_TRUE(rtc.readRegister(RV3032::cmd::REG_CONTROL1, value).ok());
  const uint32_t before = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.writeRegister(
                              RV3032::cmd::REG_EE_COMMAND, 0x21).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.readRegister(
                              RV3032::cmd::USER_EEPROM_START, value).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.readRegister(0x2E, value).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.readRegister(0x38, value).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.readRegister(
          RV3032::cmd::REG_EEPROM_PASSWORD0, value).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.writeRegister(
                              RV3032::cmd::REG_TIMER_HIGH, 0xF0).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.writeRegister(
                              RV3032::cmd::REG_SECONDS, 0x80).code));
  TEST_ASSERT_EQUAL_UINT32(before, fake.callbackCount);

  uint8_t block[4] = {0x11, 0x22, 0x33, 0x44};
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.readRegisters(
                              RV3032::cmd::REG_PASSWORD0, block, 4).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.readRegisters(
                              RV3032::cmd::REG_EE_DATA, block, 2).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.readRegisters(
                              RV3032::cmd::USER_EEPROM_START, block, 1).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.writeRegisters(
                              RV3032::cmd::REG_STATUS, block, 2).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.writeRegisters(
                              RV3032::cmd::REG_EE_DATA, block, 2).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.writeRegisters(
                              RV3032::cmd::REG_PASSWORD0, block, 4).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(rtc.writeRegisters(
                              RV3032::cmd::REG_ACTIVE_PMU, block, 1).code));
  TEST_ASSERT_EQUAL_UINT32(before, fake.callbackCount);

  const uint8_t thresholds[2] = {0xF0, 0x20};
  TEST_ASSERT_TRUE(rtc.writeRegisters(
      RV3032::cmd::REG_TLOW_THRESHOLD, thresholds, sizeof(thresholds)).ok());
  uint8_t observedThresholds[2] = {};
  TEST_ASSERT_TRUE(rtc.readRegisters(
      RV3032::cmd::REG_TLOW_THRESHOLD, observedThresholds,
      sizeof(observedThresholds)).ok());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(thresholds, observedThresholds,
                                sizeof(thresholds));

  fake.direct[RV3032::cmd::REG_USER_RAM_START] = 0xF0;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.startRegisterUpdateJob(
          RV3032::cmd::REG_CONTROL1, 0x0F, 0x05).code));
  const uint32_t beforeRamRmw = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.startRegisterUpdateJob(
      RV3032::cmd::REG_USER_RAM_START, 0xF0, 0x05).inProgress());
  uint8_t used = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 2, used).ok());
  TEST_ASSERT_EQUAL_UINT8(2, used);
  TEST_ASSERT_EQUAL_UINT32(beforeRamRmw + 2U, fake.callbackCount);
  TEST_ASSERT_EQUAL_HEX8(0x05,
      fake.direct[RV3032::cmd::REG_USER_RAM_START]);
}

void test_persistent_inspection_uses_direct_two_read_proof() {
  FakeRv3032 fake;
  fake.persistent[1] = 0x5A;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
  TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
      RV3032::ConfigurationEepromRegister::OFFSET,
      fake.nowMs, 250).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::PersistentReadResult result{};
  TEST_ASSERT_TRUE(rtc.getPersistentReadJobResult(result).ok());
  TEST_ASSERT_EQUAL_HEX8(0x5A, result.data[0]);
  TEST_ASSERT_TRUE(result.persistentVerified);
  TEST_ASSERT_TRUE(result.cleanupVerified);
  TEST_ASSERT_EQUAL_UINT16(2, fake.readOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  TEST_ASSERT_FALSE(fake.protocolViolation);
}

void test_persistent_read_result_contract_and_partial_evidence() {
  const uint8_t persistentBase = static_cast<uint8_t>(
      RV3032::cmd::USER_EEPROM_START - RV3032::cmd::CONFIG_EEPROM_START);
  FakeRv3032 probe;
  probe.persistent[persistentBase] = 0x80;
  probe.persistent[persistentBase + 1U] = 0x7F;
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config(false)).ok());

  RV3032::PersistentReadResult unchanged{};
  unchanged.eepromAddress = 0xAA;
  unchanged.length = 9;
  unchanged.data[0] = 0x55;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
      static_cast<uint8_t>(
          probeRtc.getPersistentReadJobResult(unchanged).code));
  TEST_ASSERT_EQUAL_HEX8(0xAA, unchanged.eepromAddress);
  TEST_ASSERT_EQUAL_UINT8(9, unchanged.length);
  TEST_ASSERT_EQUAL_HEX8(0x55, unchanged.data[0]);

  const uint32_t invalidBefore = probe.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(probeRtc.startReadConfigurationEepromJob(
          static_cast<RV3032::ConfigurationEepromRegister>(
              RV3032::cmd::REG_EEPROM_PASSWORD0),
          probe.nowMs, 250).code));
  TEST_ASSERT_EQUAL_UINT32(invalidBefore, probe.callbackCount);

  TEST_ASSERT_TRUE(probeRtc.startReadUserEepromJob(
      0, 2, probe.nowMs, 1000).inProgress());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
      static_cast<uint8_t>(
          probeRtc.getPersistentReadJobResult(unchanged).code));
  TEST_ASSERT_EQUAL_UINT8(9, unchanged.length);
  TEST_ASSERT_TRUE(pollJobToCompletion(probeRtc, probe, 1).ok());
  RV3032::PersistentReadResult complete{};
  TEST_ASSERT_TRUE(probeRtc.getPersistentReadJobResult(complete).ok());
  TEST_ASSERT_EQUAL_UINT8(2, complete.length);
  TEST_ASSERT_EQUAL_HEX8(0x80, complete.data[0]);
  TEST_ASSERT_EQUAL_HEX8(0x7F, complete.data[1]);
  TEST_ASSERT_TRUE(complete.persistentVerified);
  TEST_ASSERT_TRUE(complete.cleanupVerified);
  TEST_ASSERT_EQUAL_UINT16(4, probe.readOneAttempts);
  RV3032::PersistentReadResult repeated{};
  TEST_ASSERT_TRUE(probeRtc.getPersistentReadJobResult(repeated).ok());
  TEST_ASSERT_EQUAL_UINT8(2, repeated.length);

  uint32_t secondAddressOrdinal = 0;
  for (size_t i = 0; i < probe.logCount; ++i) {
    if (probe.log[i].write &&
        probe.log[i].reg == RV3032::cmd::REG_EE_ADDRESS &&
        probe.log[i].length == 1 &&
        probe.log[i].data[0] ==
            static_cast<uint8_t>(RV3032::cmd::USER_EEPROM_START + 1U)) {
      secondAddressOrdinal = static_cast<uint32_t>(i + 1U);
      break;
    }
  }
  TEST_ASSERT_NOT_EQUAL(0, secondAddressOrdinal);

  FakeRv3032 failed;
  failed.persistent[persistentBase] = 0x80;
  failed.persistent[persistentBase + 1U] = 0x7F;
  failed.failOrdinal = secondAddressOrdinal;
  failed.failError = RV3032::Err::I2C_BUS;
  RV3032::RV3032 failedRtc;
  TEST_ASSERT_TRUE(failedRtc.begin(failed.config(false)).ok());
  TEST_ASSERT_TRUE(failedRtc.startReadUserEepromJob(
      0, 2, failed.nowMs, 1000).inProgress());
  const RV3032::Status failure =
      pollJobToCompletion(failedRtc, failed, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(failure.code));
  RV3032::PersistentReadResult partial{};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::I2C_BUS),
      static_cast<uint8_t>(
          failedRtc.getPersistentReadJobResult(partial).code));
  TEST_ASSERT_EQUAL_UINT8(1, partial.length);
  TEST_ASSERT_EQUAL_HEX8(0x80, partial.data[0]);
  TEST_ASSERT_FALSE(partial.persistentVerified);
  TEST_ASSERT_TRUE(partial.cleanupVerified);

  TEST_ASSERT_TRUE(probeRtc.startReadTimeSnapshotJob(
      probe.nowMs, 100).inProgress());
  (void)pollJobToCompletion(probeRtc, probe, 1);
  unchanged.length = 7;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
      static_cast<uint8_t>(
          probeRtc.getPersistentReadJobResult(unchanged).code));
  TEST_ASSERT_EQUAL_UINT8(7, unchanged.length);

  FakeRv3032 wrapped;
  wrapped.persistent[1] = 0x5C;
  wrapped.nowMs = UINT32_MAX - 10U;
  RV3032::RV3032 wrappedRtc;
  TEST_ASSERT_TRUE(wrappedRtc.begin(wrapped.config(false)).ok());
  TEST_ASSERT_TRUE(wrappedRtc.startReadConfigurationEepromJob(
      RV3032::ConfigurationEepromRegister::OFFSET,
      wrapped.nowMs, 250).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(wrappedRtc, wrapped, 1).ok());
  TEST_ASSERT_LESS_THAN_UINT32(100, wrapped.nowMs);
  RV3032::PersistentReadResult wrappedResult{};
  TEST_ASSERT_TRUE(
      wrappedRtc.getPersistentReadJobResult(wrappedResult).ok());
  TEST_ASSERT_EQUAL_HEX8(0x5C, wrappedResult.data[0]);
  TEST_ASSERT_TRUE(wrappedResult.cleanupVerified);
}

void test_user_eeprom_write_is_compare_once_and_durably_verified() {
  FakeRv3032 fake;
  fake.persistent[RV3032::cmd::USER_EEPROM_START -
                  RV3032::cmd::CONFIG_EEPROM_START] = 0x11;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  const uint8_t values[2] = {0x11, 0x22};
  TEST_ASSERT_TRUE(rtc.startWriteUserEepromJob(0, values, sizeof(values),
                                               fake.nowMs, 1000).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::UserEepromWriteReport report{};
  TEST_ASSERT_TRUE(rtc.getUserEepromWriteJobResult(report).ok());
  TEST_ASSERT_EQUAL_UINT8(2, report.completedBytes);
  TEST_ASSERT_EQUAL_UINT8(2, report.durablyVerifiedBytes);
  TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_HEX8(0x22,
      fake.persistent[RV3032::cmd::USER_EEPROM_START + 1U -
                      RV3032::cmd::CONFIG_EEPROM_START]);
  TEST_ASSERT_EQUAL_UINT16(0, fake.updateAllAttempts);
  TEST_ASSERT_EQUAL_UINT16(0, fake.refreshAllAttempts);
  TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
}

void test_user_eeprom_read_boundaries_and_maximum_chunk() {
  FakeRv3032 fake;
  const uint8_t persistentBase = static_cast<uint8_t>(
      RV3032::cmd::USER_EEPROM_START - RV3032::cmd::CONFIG_EEPROM_START);
  for (uint8_t i = 0; i < RV3032::USER_EEPROM_SIZE; ++i) {
    fake.persistent[persistentBase + i] = static_cast<uint8_t>(0x40u + i);
  }

  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
  struct ReadCase {
    uint8_t offset;
    uint8_t length;
  };
  const ReadCase cases[] = {{0, 1}, {15, 1}, {16, 16}, {31, 1}};
  for (const ReadCase& testCase : cases) {
    const uint32_t beforeAdmission = fake.callbackCount;
    TEST_ASSERT_TRUE(rtc.startReadUserEepromJob(
        testCase.offset, testCase.length, fake.nowMs, 1000).inProgress());
    TEST_ASSERT_EQUAL_UINT32(beforeAdmission, fake.callbackCount);
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());

    RV3032::PersistentReadResult result{};
    TEST_ASSERT_TRUE(rtc.getPersistentReadJobResult(result).ok());
    TEST_ASSERT_EQUAL_HEX8(
        static_cast<uint8_t>(RV3032::cmd::USER_EEPROM_START +
                             testCase.offset),
        result.eepromAddress);
    TEST_ASSERT_EQUAL_UINT8(testCase.length, result.length);
    TEST_ASSERT_TRUE(result.persistentVerified);
    TEST_ASSERT_TRUE(result.cleanupVerified);
    for (uint8_t i = 0; i < testCase.length; ++i) {
      TEST_ASSERT_EQUAL_HEX8(
          static_cast<uint8_t>(0x40u + testCase.offset + i),
          result.data[i]);
    }
  }
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);

  const uint32_t beforeInvalid = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.startReadUserEepromJob(
          0, 0, fake.nowMs, 1000).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.startReadUserEepromJob(
          0, 17, fake.nowMs, 1000).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.startReadUserEepromJob(
          31, 2, fake.nowMs, 1000).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.startReadUserEepromJob(
          32, 1, fake.nowMs, 1000).code));
  TEST_ASSERT_EQUAL_UINT32(beforeInvalid, fake.callbackCount);
  TEST_ASSERT_FALSE(rtc.isJobBusy());
}

void test_user_eeprom_write_admission_copy_and_partial_failure_report() {
  const uint8_t persistentBase = static_cast<uint8_t>(
      RV3032::cmd::USER_EEPROM_START - RV3032::cmd::CONFIG_EEPROM_START);

  FakeRv3032 disabled;
  RV3032::RV3032 disabledRtc;
  TEST_ASSERT_TRUE(disabledRtc.begin(disabled.config(false)).ok());
  uint8_t disabledValue = 0xA5;
  const uint32_t disabledCallbacks = disabled.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
      static_cast<uint8_t>(disabledRtc.startWriteUserEepromJob(
          0, &disabledValue, 1, disabled.nowMs, 1000).code));
  TEST_ASSERT_EQUAL_UINT32(disabledCallbacks, disabled.callbackCount);
  TEST_ASSERT_FALSE(disabledRtc.isJobBusy());
  TEST_ASSERT_EQUAL_HEX8(0, disabled.persistent[persistentBase]);

  FakeRv3032 copied;
  RV3032::RV3032 copiedRtc;
  TEST_ASSERT_TRUE(copiedRtc.begin(copied.config(true)).ok());
  uint8_t source[2] = {0x31, 0x42};
  TEST_ASSERT_TRUE(copiedRtc.startWriteUserEepromJob(
      0, source, sizeof(source), copied.nowMs, 1000).inProgress());
  source[0] = 0xDE;
  source[1] = 0xAD;
  TEST_ASSERT_TRUE(pollJobToCompletion(copiedRtc, copied, 1).ok());
  TEST_ASSERT_EQUAL_HEX8(0x31, copied.persistent[persistentBase]);
  TEST_ASSERT_EQUAL_HEX8(0x42, copied.persistent[persistentBase + 1U]);
  RV3032::UserEepromWriteReport copiedReport{};
  TEST_ASSERT_TRUE(copiedRtc.getUserEepromWriteJobResult(copiedReport).ok());
  TEST_ASSERT_EQUAL_UINT8(2, copiedReport.requestedLength);
  TEST_ASSERT_EQUAL_UINT8(2, copiedReport.completedBytes);
  TEST_ASSERT_EQUAL_UINT8(2, copiedReport.durablyVerifiedBytes);
  TEST_ASSERT_TRUE(copiedReport.cleanupVerified);

  const uint8_t requested[3] = {0x51, 0x62, 0x73};
  FakeRv3032 probe;
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config(true)).ok());
  TEST_ASSERT_TRUE(probeRtc.startWriteUserEepromJob(
      0, requested, sizeof(requested), probe.nowMs, 2000).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(probeRtc, probe, 1).ok());
  uint32_t secondByteOrdinal = 0;
  for (size_t i = 0; i < probe.logCount; ++i) {
    if (probe.log[i].write &&
        probe.log[i].reg == RV3032::cmd::REG_EE_ADDRESS &&
        probe.log[i].length == 1 &&
        probe.log[i].data[0] ==
            static_cast<uint8_t>(RV3032::cmd::USER_EEPROM_START + 1U)) {
      secondByteOrdinal = static_cast<uint32_t>(i + 1U);
      break;
    }
  }
  TEST_ASSERT_NOT_EQUAL(0, secondByteOrdinal);

  FakeRv3032 failed;
  failed.failOrdinal = secondByteOrdinal;
  failed.failError = RV3032::Err::I2C_BUS;
  RV3032::RV3032 failedRtc;
  TEST_ASSERT_TRUE(failedRtc.begin(failed.config(true)).ok());
  TEST_ASSERT_TRUE(failedRtc.startWriteUserEepromJob(
      0, requested, sizeof(requested), failed.nowMs, 2000).inProgress());
  const RV3032::Status failure =
      pollJobToCompletion(failedRtc, failed, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(failure.code));
  RV3032::UserEepromWriteReport failedReport{};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::I2C_BUS),
      static_cast<uint8_t>(
          failedRtc.getUserEepromWriteJobResult(failedReport).code));
  TEST_ASSERT_EQUAL_UINT8(3, failedReport.requestedLength);
  TEST_ASSERT_EQUAL_UINT8(1, failedReport.completedBytes);
  TEST_ASSERT_EQUAL_UINT8(1, failedReport.durablyVerifiedBytes);
  TEST_ASSERT_TRUE(failedReport.cleanupVerified);
  TEST_ASSERT_EQUAL_UINT16(1, failed.writeOneAttempts);
  TEST_ASSERT_EQUAL_HEX8(requested[0], failed.persistent[persistentBase]);
  TEST_ASSERT_EQUAL_HEX8(0, failed.persistent[persistentBase + 1U]);
  TEST_ASSERT_EQUAL_HEX8(0, failed.persistent[persistentBase + 2U]);
  for (size_t i = 0; i < failed.logCount; ++i) {
    TEST_ASSERT_FALSE(
        failed.log[i].write &&
        failed.log[i].reg == RV3032::cmd::REG_EE_ADDRESS &&
        failed.log[i].length == 1 &&
        failed.log[i].data[0] ==
            static_cast<uint8_t>(RV3032::cmd::USER_EEPROM_START + 2U));
  }
}

void test_persistence_queue_capacity_duplicates_fifo_and_admission_guards() {
  FakeRv3032 fake;
  fake.persistent[1] = 0;
  fake.resetFromPersistent();
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());

  uint8_t expectedOrder[8] = {};
  for (uint8_t i = 0; i < sizeof(expectedOrder); ++i) {
    expectedOrder[i] = static_cast<uint8_t>(i + 1U);
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(
        static_cast<float>(expectedOrder[i]) * 0.2384f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i + 1U),
                            rtc.eepromQueueDepth());
  }
  TEST_ASSERT_EQUAL_UINT8(8, rtc.eepromQueueDepth());

  TEST_ASSERT_TRUE(rtc.setOffsetPpm(8.0f * 0.2384f).inProgress());
  const uint32_t busyCallbacks = fake.callbackCount;
  const uint8_t busyControl2 = fake.direct[RV3032::cmd::REG_CONTROL2];
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(rtc.setClockInterruptEnabled(true).code));
  TEST_ASSERT_EQUAL_UINT32(busyCallbacks, fake.callbackCount);
  TEST_ASSERT_EQUAL_HEX8(busyControl2,
                         fake.direct[RV3032::cmd::REG_CONTROL2]);
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_EQUAL_UINT8(8, rtc.eepromQueueDepth());

  const uint8_t activeBeforeFull = fake.activeConfig[1];
  const size_t activeWritesBeforeFull =
      countWritesTo(fake, RV3032::cmd::REG_ACTIVE_OFFSET);
  const uint32_t callbacksBeforeFull = fake.callbackCount;
  // The oldest queued value is not a duplicate of the newest pending value.
  // Reverting to it needs a new entry and must not be silently discarded.
  TEST_ASSERT_TRUE(rtc.setOffsetPpm(1.0f * 0.2384f).inProgress());
  const RV3032::Status full = pollJobToCompletion(rtc, fake, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::QUEUE_FULL),
                          static_cast<uint8_t>(full.code));
  TEST_ASSERT_EQUAL_UINT32(callbacksBeforeFull + 1U, fake.callbackCount);
  TEST_ASSERT_EQUAL_HEX8(activeBeforeFull, fake.activeConfig[1]);
  TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(activeWritesBeforeFull),
      static_cast<uint32_t>(
          countWritesTo(fake, RV3032::cmd::REG_ACTIVE_OFFSET)));
  TEST_ASSERT_EQUAL_UINT8(8, rtc.eepromQueueDepth());

  const uint8_t referenceIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_ACTIVE_TREFERENCE0 -
      RV3032::cmd::CONFIG_EEPROM_START);
  const uint8_t reference0Before = fake.activeConfig[referenceIndex];
  const uint8_t reference1Before = fake.activeConfig[referenceIndex + 1U];
  const size_t referenceWritesBefore =
      countWritesTo(fake, RV3032::cmd::REG_ACTIVE_TREFERENCE0);
  const uint32_t callbacksBeforeBlockFull = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.setTemperatureReference(0x1234).inProgress());
  const RV3032::Status blockFull = pollJobToCompletion(rtc, fake, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::QUEUE_FULL),
                          static_cast<uint8_t>(blockFull.code));
  TEST_ASSERT_EQUAL_UINT32(callbacksBeforeBlockFull + 1U,
                           fake.callbackCount);
  TEST_ASSERT_EQUAL_HEX8(reference0Before,
                         fake.activeConfig[referenceIndex]);
  TEST_ASSERT_EQUAL_HEX8(reference1Before,
                         fake.activeConfig[referenceIndex + 1U]);
  TEST_ASSERT_EQUAL_UINT32(
      static_cast<uint32_t>(referenceWritesBefore),
      static_cast<uint32_t>(
          countWritesTo(fake, RV3032::cmd::REG_ACTIVE_TREFERENCE0)));
  TEST_ASSERT_EQUAL_UINT8(8, rtc.eepromQueueDepth());

  TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 4).ok());
  TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT16(8, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_HEX8(8, fake.persistent[1]);

  uint8_t committed[8] = {};
  uint8_t committedCount = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (!fake.log[i].write ||
        fake.log[i].reg != RV3032::cmd::REG_EE_COMMAND ||
        fake.log[i].length != 1 ||
        fake.log[i].data[0] != RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
      continue;
    }
    TEST_ASSERT_LESS_THAN_UINT8(sizeof(committed), committedCount);
    for (size_t j = i; j > 0; --j) {
      const test_rv3032::Transfer& prior = fake.log[j - 1U];
      if (prior.write && prior.reg == RV3032::cmd::REG_EE_DATA &&
          prior.length == 1) {
        committed[committedCount++] = prior.data[0];
        break;
      }
    }
  }
  TEST_ASSERT_EQUAL_UINT8(sizeof(expectedOrder), committedCount);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedOrder, committed,
                                sizeof(expectedOrder));
}

void test_settings_snapshot_tracks_job_queue_health_and_deadlines() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  RV3032::Config config = fake.config(true);
  config.i2cTimeoutMs = 7;
  config.eepromTimeoutMs = 120;
  config.offlineThreshold = 2;
  TEST_ASSERT_TRUE(rtc.begin(config).ok());

  RV3032::SettingsSnapshot snapshot = rtc.getSettings();
  TEST_ASSERT_TRUE(snapshot.initialized);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(snapshot.state));
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::I2C_ADDR_7BIT, snapshot.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(7, snapshot.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(120, snapshot.eepromTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(2, snapshot.offlineThreshold);
  TEST_ASSERT_TRUE(snapshot.hasNowMsHook);
  TEST_ASSERT_TRUE(snapshot.hasWaitMsHook);
  TEST_ASSERT_TRUE(snapshot.enableEepromWrites);
  TEST_ASSERT_FALSE(snapshot.jobBusy);
  TEST_ASSERT_FALSE(snapshot.eepromBusy);

  fake.nowMs = 11;
  uint8_t status = 0;
  TEST_ASSERT_TRUE(rtc.readStatus(status).ok());
  snapshot = rtc.getSettings();
  TEST_ASSERT_EQUAL_UINT32(11, snapshot.lastOkMs);
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(0, snapshot.totalFailures);

  fake.nowMs = 22;
  fake.failOrdinal = fake.callbackCount + 1U;
  fake.failError = RV3032::Err::I2C_BUS;
  TEST_ASSERT_FALSE(rtc.readStatus(status).ok());
  snapshot = rtc.getSettings();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::DriverState::DEGRADED),
      static_cast<uint8_t>(snapshot.state));
  TEST_ASSERT_EQUAL_UINT32(22, snapshot.lastErrorMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(snapshot.lastError.code));
  TEST_ASSERT_EQUAL_UINT8(1, snapshot.consecutiveFailures);
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.totalFailures);

  fake.failOrdinal = 0;
  fake.nowMs = 33;
  TEST_ASSERT_TRUE(rtc.readStatus(status).ok());
  snapshot = rtc.getSettings();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(snapshot.state));
  TEST_ASSERT_EQUAL_UINT32(33, snapshot.lastOkMs);
  TEST_ASSERT_EQUAL_UINT8(0, snapshot.consecutiveFailures);
  TEST_ASSERT_EQUAL_UINT32(2, snapshot.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.totalFailures);

  const uint32_t beforeJobDeadline = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(
      fake.nowMs, 1).inProgress());
  snapshot = rtc.getSettings();
  TEST_ASSERT_TRUE(snapshot.jobBusy);
  TEST_ASSERT_FALSE(snapshot.eepromBusy);
  fake.nowMs += 1U;
  uint8_t used = 99;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::TIMEOUT),
      static_cast<uint8_t>(rtc.pollJob(fake.nowMs, 1, used).code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(beforeJobDeadline, fake.callbackCount);
  snapshot = rtc.getSettings();
  TEST_ASSERT_FALSE(snapshot.jobBusy);
  TEST_ASSERT_EQUAL_UINT32(2, snapshot.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.totalFailures);

  TEST_ASSERT_TRUE(rtc.setOffsetPpm(0.2384f).inProgress());
  snapshot = rtc.getSettings();
  TEST_ASSERT_TRUE(snapshot.jobBusy);
  TEST_ASSERT_FALSE(snapshot.eepromBusy);
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  snapshot = rtc.getSettings();
  TEST_ASSERT_FALSE(snapshot.jobBusy);
  TEST_ASSERT_TRUE(snapshot.eepromBusy);
  TEST_ASSERT_EQUAL_UINT8(1, snapshot.eepromQueueDepth);

  TEST_ASSERT_TRUE(rtc.pollEeprom(fake.nowMs, 1, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, used);
  snapshot = rtc.getSettings();
  TEST_ASSERT_TRUE(snapshot.jobBusy);
  TEST_ASSERT_TRUE(snapshot.eepromBusy);
  TEST_ASSERT_EQUAL_UINT8(0, snapshot.eepromQueueDepth);
  TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 4).ok());
  snapshot = rtc.getSettings();
  TEST_ASSERT_FALSE(snapshot.jobBusy);
  TEST_ASSERT_FALSE(snapshot.eepromBusy);
  TEST_ASSERT_EQUAL_UINT8(0, snapshot.eepromQueueDepth);
  TEST_ASSERT_TRUE(snapshot.eepromLastStatus.ok());
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.eepromWriteCount);
  TEST_ASSERT_EQUAL_UINT32(0, snapshot.eepromWriteFailures);

  TEST_ASSERT_TRUE(rtc.setOffsetPpm(2.0f * 0.2384f).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_TRUE(rtc.pollEeprom(fake.nowMs, 1, used).inProgress());
  TEST_ASSERT_TRUE(rtc.pollEeprom(fake.nowMs, 1, used).inProgress());
  const RV3032::SettingsSnapshot beforeEepromDeadline = rtc.getSettings();
  const uint32_t callbacksBeforeEepromDeadline = fake.callbackCount;
  fake.nowMs += 4000U;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
      static_cast<uint8_t>(rtc.pollEeprom(fake.nowMs, 4, used).code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(callbacksBeforeEepromDeadline,
                           fake.callbackCount);
  snapshot = rtc.getSettings();
  TEST_ASSERT_FALSE(snapshot.jobBusy);
  TEST_ASSERT_FALSE(snapshot.eepromBusy);
  TEST_ASSERT_EQUAL_UINT8(0, snapshot.eepromQueueDepth);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
      static_cast<uint8_t>(snapshot.eepromLastStatus.code));
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.eepromWriteCount);
  TEST_ASSERT_EQUAL_UINT32(1, snapshot.eepromWriteFailures);
  TEST_ASSERT_EQUAL_UINT32(beforeEepromDeadline.totalSuccess,
                           snapshot.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(beforeEepromDeadline.totalFailures,
                           snapshot.totalFailures);
  TEST_ASSERT_EQUAL_UINT32(beforeEepromDeadline.lastOkMs,
                           snapshot.lastOkMs);
  TEST_ASSERT_EQUAL_UINT32(beforeEepromDeadline.lastErrorMs,
                           snapshot.lastErrorMs);
}

void test_fake_wait_request_log_is_bounded_and_reports_overflow() {
  FakeRv3032 fake;
  fake.earlyWait = true;
  uint32_t expectedActualWait = 0;
  for (size_t i = 0; i < FakeRv3032::WAIT_LOG_CAPACITY; ++i) {
    const uint32_t requested = static_cast<uint32_t>(i + 1U);
    FakeRv3032::waitCallback(requested, &fake);
    expectedActualWait += requested - 1U;
  }
  TEST_ASSERT_EQUAL_UINT32(FakeRv3032::WAIT_LOG_CAPACITY, fake.waitCount);
  TEST_ASSERT_EQUAL_UINT32(FakeRv3032::WAIT_LOG_CAPACITY,
                           fake.waitLogCount);
  TEST_ASSERT_EQUAL_UINT32(1, fake.waitRequests[0]);
  TEST_ASSERT_EQUAL_UINT32(FakeRv3032::WAIT_LOG_CAPACITY,
      fake.waitRequests[FakeRv3032::WAIT_LOG_CAPACITY - 1U]);
  TEST_ASSERT_EQUAL_UINT32(expectedActualWait, fake.waitedMs);
  TEST_ASSERT_EQUAL_UINT32(expectedActualWait, fake.nowMs);
  TEST_ASSERT_FALSE(fake.waitLogOverflow);
  TEST_ASSERT_FALSE(fake.logOverflow);

  FakeRv3032::waitCallback(99, &fake);
  TEST_ASSERT_EQUAL_UINT32(FakeRv3032::WAIT_LOG_CAPACITY + 1U,
                           fake.waitCount);
  TEST_ASSERT_EQUAL_UINT32(FakeRv3032::WAIT_LOG_CAPACITY,
                           fake.waitLogCount);
  TEST_ASSERT_TRUE(fake.waitLogOverflow);
  TEST_ASSERT_TRUE(fake.logOverflow);
  TEST_ASSERT_EQUAL_UINT32(FakeRv3032::WAIT_LOG_CAPACITY,
      fake.waitRequests[FakeRv3032::WAIT_LOG_CAPACITY - 1U]);
}

void test_generic_persistence_uses_full_budget_and_durable_protocol() {
  FakeRv3032 fake;
  fake.persistent[0] = 0;
  fake.resetFromPersistent();
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
      RV3032::BackupSwitchMode::Direct).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  uint8_t used = 0;
  const uint32_t callbacksBeforePoll = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.pollEeprom(fake.nowMs, 5, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(5, used);
  TEST_ASSERT_EQUAL_UINT32(5, fake.callbackCount - callbacksBeforePoll);
  TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::PMU_BSM_DIRECT, fake.persistent[0]);
  TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
  TEST_ASSERT_FALSE(fake.protocolViolation);
  TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
}

void test_generic_persistence_clears_stale_eef_and_restores_access_state() {
  FakeRv3032 fake;
  fake.direct[RV3032::cmd::REG_CONTROL1] = 0x3B;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
      RV3032::BackupSwitchMode::Direct).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  const uint8_t expectedActive = fake.activeConfig[0];
  fake.direct[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(
      RV3032::cmd::EEPROM_EEF_MASK | RV3032::cmd::TEMP_CLKF_MASK |
      RV3032::cmd::TEMP_BSF_MASK);

  TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteFailures());
  TEST_ASSERT_TRUE(rtc.getEepromStatus().ok());
  TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_HEX8(expectedActive, fake.persistent[0]);
  TEST_ASSERT_EQUAL_HEX8(expectedActive, fake.activeConfig[0]);
  TEST_ASSERT_EQUAL_HEX8(
      0x3B, fake.direct[RV3032::cmd::REG_CONTROL1] &
                RV3032::cmd::CONTROL1_IMPLEMENTED_MASK);
  TEST_ASSERT_EQUAL_HEX8(
      static_cast<uint8_t>(RV3032::cmd::TEMP_CLKF_MASK |
                           RV3032::cmd::TEMP_BSF_MASK),
      fake.direct[RV3032::cmd::REG_TEMP_LSB] & 0x0Bu);

  size_t clearEefWrites = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write &&
        fake.log[i].reg == RV3032::cmd::REG_TEMP_LSB) {
      ++clearEefWrites;
      TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::EEPROM_CLEAR_EEF_VALUE,
                             fake.log[i].data[0]);
    }
  }
  TEST_ASSERT_EQUAL_UINT32(1, clearEefWrites);
  TEST_ASSERT_FALSE(fake.protocolViolation);
  TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
}

void test_generic_persistence_minimum_write_wait_has_no_early_io() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  RV3032::Config config = fake.config(true);
  config.eepromTimeoutMs = 10;
  TEST_ASSERT_TRUE(rtc.begin(config).ok());
  fake.callbackDurationMs = 5;
  TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
      RV3032::BackupSwitchMode::Direct).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());

  uint8_t used = 0;
  RV3032::Status status = RV3032::Status::Error(
      RV3032::Err::IN_PROGRESS, "test queue not started");
  for (uint16_t i = 0; i < 500 && fake.writeOneAttempts == 0; ++i) {
    status = rtc.pollEeprom(fake.nowMs, 1, used);
    TEST_ASSERT_TRUE(status.inProgress());
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(1, used);
    if (used == 0) ++fake.nowMs;
  }
  TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
      fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
  const uint32_t commandTime = fake.nowMs;
  const uint32_t callbacksAtCommand = fake.callbackCount;

  status = rtc.pollEeprom(commandTime, 1, used);
  TEST_ASSERT_TRUE(status.inProgress());
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(callbacksAtCommand, fake.callbackCount);
  status = rtc.pollEeprom(commandTime + 9U, 1, used);
  TEST_ASSERT_TRUE(status.inProgress());
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(callbacksAtCommand, fake.callbackCount);
  status = rtc.pollEeprom(commandTime + 10U, 1, used);
  TEST_ASSERT_TRUE(status.inProgress());
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(callbacksAtCommand, fake.callbackCount);
  status = rtc.pollEeprom(commandTime + 10U, 1, used);
  TEST_ASSERT_TRUE(status.inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_EQUAL_UINT32(callbacksAtCommand + 1U, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(0, fake.waitLogCount);
  TEST_ASSERT_FALSE(fake.waitLogOverflow);

  fake.nowMs = commandTime + 10U;
  TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteFailures());
}

void test_generic_persistence_reports_low_vdd_and_initial_busy_failures() {
  {
    FakeRv3032 fake;
    const uint8_t persistentBefore = fake.persistent[0];
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
        RV3032::BackupSwitchMode::Direct).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    const uint8_t expectedActive = fake.activeConfig[0];
    fake.lowVdd = true;

    const RV3032::Status status = pollEepromToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_WRITE_FAILED),
        static_cast<uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(status.code),
                            static_cast<uint8_t>(rtc.getEepromStatus().code));
    TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteCount());
    TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteFailures());
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT16(4, fake.readOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_EQUAL_HEX8(persistentBefore, fake.persistent[0]);
    TEST_ASSERT_EQUAL_HEX8(expectedActive, fake.activeConfig[0]);
    TEST_ASSERT_BITS_HIGH(RV3032::cmd::EEPROM_EEF_MASK,
                          fake.direct[RV3032::cmd::REG_TEMP_LSB]);
    TEST_ASSERT_FALSE(fake.protocolViolation);
    TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
        RV3032::BackupSwitchMode::Direct).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    fake.setInitialBusy(1000);

    const RV3032::Status status = pollEepromToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(status.code),
                            static_cast<uint8_t>(rtc.getEepromStatus().code));
    TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteCount());
    TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteFailures());
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT16(0, fake.readOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_READ_ONE));
    TEST_ASSERT_FALSE(fake.protocolViolation);
    TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
  }
}

void test_generic_persistence_restores_intended_active_and_zero_write_compare() {
  {
    FakeRv3032 fake;
    fake.persistent[1] = 0;
    fake.resetFromPersistent();
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(4.0f * 0.2384f).inProgress());
    fake.ignoreWriteOrdinal = fake.callbackCount + 2U;
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_HEX8(0, fake.activeConfig[1]);
    TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_HEX8(4, fake.persistent[1]);
    TEST_ASSERT_EQUAL_HEX8(4, fake.activeConfig[1]);
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_FALSE(fake.protocolViolation);
  }

  {
    FakeRv3032 fake;
    fake.persistent[1] = 4;
    fake.resetFromPersistent();
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(4.0f * 0.2384f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    const uint32_t beforeBusy = fake.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(rtc.startReadConfigurationEepromJob(
            RV3032::ConfigurationEepromRegister::OFFSET,
            fake.nowMs, 250).code));
    TEST_ASSERT_EQUAL_UINT32(beforeBusy, fake.callbackCount);
    TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);

    TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::OFFSET,
        fake.nowMs, 250).inProgress());
    const uint32_t activeJobCallbacks = fake.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(rtc.setOffsetPpm(5.0f * 0.2384f).code));
    TEST_ASSERT_EQUAL_UINT32(activeJobCallbacks, fake.callbackCount);
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0x4C;
    fake.resetFromPersistent();
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
        RV3032::BackupSwitchMode::Direct).inProgress());
    fake.ignoreWriteOrdinal = fake.callbackCount + 2U;
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_HEX8(0x4C, fake.activeConfig[0]);
    TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_HEX8(0x5C, fake.persistent[0]);
    TEST_ASSERT_EQUAL_HEX8(0x5C, fake.activeConfig[0]);
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
  }
}

void test_generic_persistence_retains_first_batch_failure() {
  FakeRv3032 probe;
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config(true)).ok());
  for (uint8_t value = 1; value <= 2; ++value) {
    TEST_ASSERT_TRUE(probeRtc.setOffsetPpm(
        static_cast<float>(value) * 0.2384f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(probeRtc, probe, 1).ok());
  }
  TEST_ASSERT_TRUE(pollEepromToCompletion(probeRtc, probe, 1).ok());
  const uint32_t firstStagingOrdinal = findWriteDataStagingOrdinal(probe);
  TEST_ASSERT_NOT_EQUAL(0, firstStagingOrdinal);

  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  for (uint8_t value = 1; value <= 2; ++value) {
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(
        static_cast<float>(value) * 0.2384f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  }
  TEST_ASSERT_EQUAL_UINT8(2, rtc.eepromQueueDepth());
  fake.ignoreWriteOrdinal = firstStagingOrdinal;
  const RV3032::Status firstFailure =
      pollEepromToCompletion(rtc, fake, 1);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(firstFailure.code));
  TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteFailures());

  const RV3032::Status drained = pollEepromToCompletion(rtc, fake, 1);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(drained.code));
  TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteFailures());
  TEST_ASSERT_EQUAL_HEX8(2, fake.persistent[1]);
  TEST_ASSERT_EQUAL_HEX8(2, fake.activeConfig[1]);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(rtc.getEepromStatus().code));

  TEST_ASSERT_TRUE(rtc.setOffsetPpm(3.0f * 0.2384f).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_TRUE(rtc.getEepromStatus().ok());
  TEST_ASSERT_EQUAL_HEX8(3, fake.persistent[1]);
}

void test_generic_multi_instruction_poll_refreshes_mutation_cutoff() {
  FakeRv3032 probe;
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config(true)).ok());
  TEST_ASSERT_TRUE(probeRtc.setBackupSwitchMode(
      RV3032::BackupSwitchMode::Direct).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(probeRtc, probe, 1).ok());
  TEST_ASSERT_TRUE(pollEepromToCompletion(probeRtc, probe, 1).ok());
  const uint32_t commandOrdinal = findEepromCommandOrdinal(
      probe, RV3032::cmd::EEPROM_CMD_WRITE_ONE);
  TEST_ASSERT_GREATER_THAN_UINT32(2, commandOrdinal);

  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
      RV3032::BackupSwitchMode::Direct).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());

  uint8_t used = 0;
  while (fake.callbackCount < commandOrdinal - 2U) {
    const RV3032::Status st = rtc.pollEeprom(fake.nowMs, 1, used);
    TEST_ASSERT_TRUE(st.inProgress());
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(1, used);
    if (used == 0) ++fake.nowMs;
  }
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  fake.nowMs = 3699;
  fake.callbackDurationMs = 2;
  const RV3032::Status cutoffPoll = rtc.pollEeprom(fake.nowMs, 2, used);
  TEST_ASSERT_TRUE(cutoffPoll.inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
      fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));

  fake.callbackDurationMs = 0;
  const RV3032::Status terminal = pollEepromToCompletion(rtc, fake, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(terminal.code));
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  TEST_ASSERT_FALSE(fake.protocolViolation);
}

void test_generic_persistence_staging_and_ambiguous_write_one_paths() {
  FakeRv3032 probe;
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config(true)).ok());
  TEST_ASSERT_TRUE(probeRtc.setBackupSwitchMode(
      RV3032::BackupSwitchMode::Direct).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(probeRtc, probe).ok());
  TEST_ASSERT_TRUE(pollEepromToCompletion(probeRtc, probe, 1).ok());
  const uint32_t stagingOrdinal = findWriteDataStagingOrdinal(probe);
  const uint32_t commandOrdinal = findEepromCommandOrdinal(
      probe, RV3032::cmd::EEPROM_CMD_WRITE_ONE);
  TEST_ASSERT_NOT_EQUAL(0, stagingOrdinal);
  TEST_ASSERT_NOT_EQUAL(0, commandOrdinal);

  {
    FakeRv3032 fake;
    const uint8_t persistentBefore = fake.persistent[0];
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
        RV3032::BackupSwitchMode::Direct).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    const uint8_t expectedActive = fake.activeConfig[0];
    fake.ignoreWriteOrdinal = stagingOrdinal;
    const RV3032::Status status = pollEepromToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
        static_cast<uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(status.code),
                            static_cast<uint8_t>(rtc.getEepromStatus().code));
    TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteCount());
    TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteFailures());
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_EQUAL_HEX8(persistentBefore, fake.persistent[0]);
    TEST_ASSERT_EQUAL_HEX8(expectedActive, fake.activeConfig[0]);
    TEST_ASSERT_FALSE(fake.protocolViolation);
    TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
        RV3032::BackupSwitchMode::Direct).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    const uint8_t expected = fake.activeConfig[0];
    fake.failOrdinal = commandOrdinal;
    fake.failCommandAfterCommit = true;
    TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteCount());
    TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteFailures());
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_EQUAL_HEX8(expected, fake.persistent[0]);
    TEST_ASSERT_TRUE(rtc.getEepromStatus().ok());
    TEST_ASSERT_FALSE(fake.protocolViolation);
    TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
  }

  {
    FakeRv3032 fake;
    const uint8_t persistentBefore = fake.persistent[0];
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
        RV3032::BackupSwitchMode::Direct).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    const uint8_t expectedActive = fake.activeConfig[0];
    fake.failOrdinal = commandOrdinal;
    const RV3032::Status status = pollEepromToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
        static_cast<uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(status.code),
                            static_cast<uint8_t>(rtc.getEepromStatus().code));
    TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteCount());
    TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteFailures());
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_EQUAL_HEX8(persistentBefore, fake.persistent[0]);
    TEST_ASSERT_EQUAL_HEX8(expectedActive, fake.activeConfig[0]);
    TEST_ASSERT_FALSE(fake.protocolViolation);
    TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
  }
}

void test_public_unix_read_set_and_range_contract() {
  static constexpr uint32_t LEAP_DAY_UNIX = 1709210096UL;
  static constexpr uint32_t EPOCH_2000 = 946684800UL;
  static constexpr uint32_t EPOCH_2100 = 4102444800UL;

  FakeRv3032 fake;
  fake.setCalendar(2024, 2, 29, 12, 34, 56, 4);
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());

  uint32_t observed = 0;
  const uint32_t beforeRead = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.readUnix(observed).ok());
  TEST_ASSERT_EQUAL_UINT32(LEAP_DAY_UNIX, observed);
  TEST_ASSERT_EQUAL_UINT32(beforeRead + 1U, fake.callbackCount);

  fake.setCalendar(2000, 1, 1, 0, 0, 0, 6);
  const uint32_t beforeSet = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.setUnix(LEAP_DAY_UNIX).ok());
  TEST_ASSERT_EQUAL_UINT32(beforeSet + 1U, fake.callbackCount);
  RV3032::DateTime dateTime{};
  TEST_ASSERT_TRUE(rtc.readTime(dateTime).ok());
  const RV3032::DateTime expected{2024, 2, 29, 12, 34, 56, 4};
  assertDateTimeEquals(expected, dateTime);

  const uint32_t beforeInvalid = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.setUnix(EPOCH_2000 - 1U).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.setUnix(EPOCH_2100).code));
  TEST_ASSERT_EQUAL_UINT32(beforeInvalid, fake.callbackCount);
}

void test_periodic_timer_event_flags_and_guarded_status_clear() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());

  TEST_ASSERT_TRUE(rtc.setPeriodicUpdate(
      RV3032::PeriodicUpdateFrequency::MINUTE, true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  RV3032::PeriodicUpdateFrequency frequency =
      RV3032::PeriodicUpdateFrequency::SECOND;
  bool interruptEnabled = false;
  TEST_ASSERT_TRUE(rtc.getPeriodicUpdate(frequency, interruptEnabled).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PeriodicUpdateFrequency::MINUTE),
      static_cast<uint8_t>(frequency));
  TEST_ASSERT_TRUE(interruptEnabled);
  const uint32_t beforeInvalid = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setPeriodicUpdate(
          static_cast<RV3032::PeriodicUpdateFrequency>(2), true).code));
  TEST_ASSERT_EQUAL_UINT32(beforeInvalid, fake.callbackCount);

  const uint8_t timerMask = static_cast<uint8_t>(
      1u << RV3032::cmd::STATUS_TF_BIT);
  const uint8_t highTemperatureMask = static_cast<uint8_t>(
      1u << RV3032::cmd::STATUS_THF_BIT);
  fake.direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
      timerMask | highTemperatureMask);
  bool triggered = false;
  TEST_ASSERT_TRUE(rtc.getTimerFlag(triggered).ok());
  TEST_ASSERT_TRUE(triggered);
  const size_t writesBeforeGuard = countWritesTo(
      fake, RV3032::cmd::REG_STATUS);
  TEST_ASSERT_TRUE(rtc.clearTimerFlag().inProgress());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
  TEST_ASSERT_EQUAL_UINT32(writesBeforeGuard, countWritesTo(
      fake, RV3032::cmd::REG_STATUS));
  TEST_ASSERT_EQUAL_HEX8(
      static_cast<uint8_t>(timerMask | highTemperatureMask),
      fake.direct[RV3032::cmd::REG_STATUS]);

  TEST_ASSERT_TRUE(rtc.clearTemperatureFlags().inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_EQUAL_HEX8(timerMask,
                         fake.direct[RV3032::cmd::REG_STATUS]);

  const uint8_t updateMask = static_cast<uint8_t>(
      1u << RV3032::cmd::STATUS_UF_BIT);
  const uint8_t eventMask = static_cast<uint8_t>(
      1u << RV3032::cmd::STATUS_EVF_BIT);
  const uint8_t alarmMask = static_cast<uint8_t>(
      1u << RV3032::cmd::STATUS_AF_BIT);
  fake.direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
      updateMask | timerMask | eventMask | alarmMask);

  TEST_ASSERT_TRUE(rtc.getPeriodicUpdateFlag(triggered).ok());
  TEST_ASSERT_TRUE(triggered);
  TEST_ASSERT_TRUE(rtc.getTimerFlag(triggered).ok());
  TEST_ASSERT_TRUE(triggered);
  TEST_ASSERT_TRUE(rtc.getEventFlag(triggered).ok());
  TEST_ASSERT_TRUE(triggered);

  TEST_ASSERT_TRUE(rtc.clearPeriodicUpdateFlag().inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_EQUAL_HEX8(
      static_cast<uint8_t>(timerMask | eventMask | alarmMask),
      fake.direct[RV3032::cmd::REG_STATUS]);
  TEST_ASSERT_TRUE(rtc.clearTimerFlag().inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(eventMask | alarmMask),
                         fake.direct[RV3032::cmd::REG_STATUS]);
  TEST_ASSERT_TRUE(rtc.clearEventFlag().inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_EQUAL_HEX8(alarmMask,
                         fake.direct[RV3032::cmd::REG_STATUS]);

  // A lower-six W0C flag that asserts between the guard read and write must
  // not be collateral-cleared by the cooperative two-callback operation.
  fake.direct[RV3032::cmd::REG_STATUS] = timerMask;
  TEST_ASSERT_TRUE(rtc.clearTimerFlag().inProgress());
  uint8_t used = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, used);
  fake.direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
      fake.direct[RV3032::cmd::REG_STATUS] | alarmMask);
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).ok());
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_EQUAL_HEX8(alarmMask,
                         fake.direct[RV3032::cmd::REG_STATUS]);

  TEST_ASSERT_TRUE(rtc.getPeriodicUpdateFlag(triggered).ok());
  TEST_ASSERT_FALSE(triggered);
  TEST_ASSERT_TRUE(rtc.getTimerFlag(triggered).ok());
  TEST_ASSERT_FALSE(triggered);
  TEST_ASSERT_TRUE(rtc.getEventFlag(triggered).ok());
  TEST_ASSERT_FALSE(triggered);
}

void test_primary_cell_already_correct_is_read_only_and_latched() {
  FakeRv3032 fake;
  fake.persistent[0] = 0x2C;
  fake.resetFromPersistent();
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PrimaryCellConfigurationOutcome::ALREADY_CONFIGURED),
      static_cast<uint8_t>(report.outcome));
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(1, fake.readOneAttempts);
  TEST_ASSERT_FALSE(report.writeDurablyVerified);
  assertPrimaryTargetEvidence(report, true, true);
  TEST_ASSERT_TRUE(report.cleanupVerified);
  TEST_ASSERT_EQUAL_HEX8(0x2C, report.activeAfter);
  const uint32_t callbacks = fake.callbackCount;
  RV3032::PrimaryCellConfigurationReport unchanged = report;
  const RV3032::PrimaryCellConfigurationReport beforeSecondCall = unchanged;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::PRIMARY_CELL_ALREADY_ATTEMPTED),
      static_cast<uint8_t>(rtc.ensurePrimaryCellConfiguration(unchanged).code));
  TEST_ASSERT_EQUAL_MEMORY(&beforeSecondCall, &unchanged,
                           sizeof(beforeSecondCall));
  TEST_ASSERT_EQUAL_UINT32(callbacks, fake.callbackCount);
}

void test_primary_cell_updates_exact_target_once() {
  FakeRv3032 fake;
  fake.persistent[0] = 0x5F;
  fake.resetFromPersistent();
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PrimaryCellConfigurationOutcome::EEPROM_UPDATED),
      static_cast<uint8_t>(report.outcome));
  TEST_ASSERT_EQUAL_HEX8(0x6C, report.persistentTarget);
  TEST_ASSERT_EQUAL_HEX8(0x6C, fake.persistent[0]);
  TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(2, fake.readOneAttempts);
  TEST_ASSERT_EQUAL_UINT32(fake.callbackCount, fake.physicalAttemptCount);
  TEST_ASSERT_TRUE(report.writeDurablyVerified);
  assertPrimaryTargetEvidence(report, true, true);
  TEST_ASSERT_FALSE(fake.protocolViolation);
  TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
}

void test_primary_cell_preserves_all_nclke_tcr_and_control1_bits() {
  const uint8_t control1Before = 0x3B;
  for (uint8_t nclke = 0; nclke < 2; ++nclke) {
    for (uint8_t tcr = 0; tcr < 4; ++tcr) {
      const uint8_t preserved = static_cast<uint8_t>(
          (nclke != 0 ? RV3032::cmd::PMU_NCLKE_MASK : 0) |
          static_cast<uint8_t>(tcr << 2));
      const uint8_t persistentBefore = static_cast<uint8_t>(preserved | 0x13u);
      const uint8_t expectedTarget = static_cast<uint8_t>(
          (persistentBefore & 0x4Cu) | 0x20u);

      FakeRv3032 fake;
      fake.persistent[0] = persistentBefore;
      fake.resetFromPersistent();
      fake.direct[RV3032::cmd::REG_CONTROL1] = control1Before;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
      RV3032::PrimaryCellConfigurationReport report{};
      TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());

      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(
              RV3032::PrimaryCellConfigurationOutcome::EEPROM_UPDATED),
          static_cast<uint8_t>(report.outcome));
      TEST_ASSERT_EQUAL_HEX8(expectedTarget, report.persistentTarget);
      TEST_ASSERT_EQUAL_HEX8(expectedTarget, fake.persistent[0]);
      TEST_ASSERT_EQUAL_HEX8(expectedTarget, fake.activeConfig[0]);
      TEST_ASSERT_EQUAL_HEX8(control1Before, report.control1After);
      TEST_ASSERT_EQUAL_HEX8(
          control1Before,
          fake.direct[RV3032::cmd::REG_CONTROL1] &
              RV3032::cmd::CONTROL1_IMPLEMENTED_MASK);
      TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
      TEST_ASSERT_EQUAL_UINT16(2, fake.readOneAttempts);
      TEST_ASSERT_TRUE(report.writeDurablyVerified);
      TEST_ASSERT_TRUE(report.cleanupVerified);
      assertPrimaryTargetEvidence(report, true, true);
      TEST_ASSERT_FALSE(fake.protocolViolation);
      TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
    }
  }
}

void test_primary_cell_rejects_persistent_c0_bit7_without_target_write() {
  FakeRv3032 fake;
  fake.persistent[0] = 0xA0;
  fake.resetFromPersistent();
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  const RV3032::Status status = rtc.ensurePrimaryCellConfiguration(report);

  TEST_ASSERT_FALSE(status.ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PrimaryCellConfigurationOutcome::FAILED),
      static_cast<uint8_t>(report.outcome));
  TEST_ASSERT_FALSE(report.persistentBeforeValid);
  assertPrimaryTargetEvidence(report, false, false);
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(1, fake.readOneAttempts);
  TEST_ASSERT_TRUE(report.cleanupVerified);
  TEST_ASSERT_TRUE(report.autoRefreshHeldDisabledForSafety);
  TEST_ASSERT_EQUAL_HEX8(
      0, fake.activeConfig[0] &
             static_cast<uint8_t>(RV3032::cmd::PMU_BSM_MASK |
                                  RV3032::cmd::PMU_TCM_MASK));
  TEST_ASSERT_BITS_HIGH(RV3032::cmd::CONTROL1_EERD_MASK,
                        fake.direct[RV3032::cmd::REG_CONTROL1]);

  uint8_t eedataWrites = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write &&
        fake.log[i].reg == RV3032::cmd::REG_EE_DATA) {
      ++eedataWrites;
      TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::PERSISTENT_READ_SENTINEL,
                             fake.log[i].data[0]);
    }
  }
  TEST_ASSERT_EQUAL_UINT8(1, eedataWrites);
}

void test_primary_cell_repairs_stale_active_from_correct_persistence() {
  FakeRv3032 fake;
  fake.persistent[0] = 0x2C;
  fake.resetFromPersistent();
  fake.activeConfig[0] = 0x5F;
  fake.direct[RV3032::cmd::REG_CONTROL1] = 0x21;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(
          RV3032::PrimaryCellConfigurationOutcome::ALREADY_CONFIGURED),
      static_cast<uint8_t>(report.outcome));
  TEST_ASSERT_EQUAL_HEX8(0x2C, fake.persistent[0]);
  TEST_ASSERT_EQUAL_HEX8(0x2C, fake.activeConfig[0]);
  TEST_ASSERT_EQUAL_HEX8(0x21, report.control1After);
  TEST_ASSERT_EQUAL_HEX8(
      0x21, fake.direct[RV3032::cmd::REG_CONTROL1] &
                RV3032::cmd::CONTROL1_IMPLEMENTED_MASK);
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(1, fake.readOneAttempts);
  assertPrimaryTargetEvidence(report, true, true);
  TEST_ASSERT_TRUE(report.cleanupVerified);
  TEST_ASSERT_FALSE(fake.protocolViolation);
  TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
}

void test_primary_cell_preconditions_do_not_consume_attempt() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(false, false)).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  report.persistentTarget = 0xA5;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(rtc.ensurePrimaryCellConfiguration(
                              report).code));
  TEST_ASSERT_EQUAL_HEX8(0xA5, report.persistentTarget);
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
  TEST_ASSERT_FALSE(rtc.getSettings().primaryCellEnsureAttempted);

  FakeRv3032 missingNow;
  RV3032::Config missingNowConfig = missingNow.config();
  missingNowConfig.nowMs = nullptr;
  RV3032::RV3032 missingNowRtc;
  TEST_ASSERT_TRUE(missingNowRtc.begin(missingNowConfig).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
      static_cast<uint8_t>(
          missingNowRtc.ensurePrimaryCellConfiguration(report).code));
  TEST_ASSERT_EQUAL_UINT32(0, missingNow.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(0, missingNow.waitCount);
  TEST_ASSERT_FALSE(missingNowRtc.getSettings().primaryCellEnsureAttempted);

  FakeRv3032 busyMissingWait;
  RV3032::RV3032 busyMissingWaitRtc;
  TEST_ASSERT_TRUE(busyMissingWaitRtc.begin(
      busyMissingWait.config(false, false)).ok());
  TEST_ASSERT_TRUE(busyMissingWaitRtc.setClockInterruptEnabled(true).inProgress());
  RV3032::PrimaryCellConfigurationReport unchanged{};
  unchanged.outcome = RV3032::PrimaryCellConfigurationOutcome::FAILED;
  unchanged.persistentTarget = 0xA6;
  const RV3032::PrimaryCellConfigurationReport before = unchanged;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(busyMissingWaitRtc.ensurePrimaryCellConfiguration(
          unchanged).code));
  TEST_ASSERT_EQUAL_MEMORY(&before, &unchanged, sizeof(before));
  TEST_ASSERT_EQUAL_UINT32(0, busyMissingWait.callbackCount);
  TEST_ASSERT_FALSE(
      busyMissingWaitRtc.getSettings().primaryCellEnsureAttempted);
}

void test_primary_cell_ambiguous_write_is_reconciled_without_retry() {
  FakeRv3032 probeFake;
  probeFake.persistent[0] = 0;
  probeFake.resetFromPersistent();
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probeFake.config()).ok());
  RV3032::PrimaryCellConfigurationReport probeReport{};
  TEST_ASSERT_TRUE(probeRtc.ensurePrimaryCellConfiguration(probeReport).ok());
  uint32_t commandOrdinal = 0;
  for (size_t i = 0; i < probeFake.logCount; ++i) {
    if (probeFake.log[i].write &&
        probeFake.log[i].reg == RV3032::cmd::REG_EE_COMMAND &&
        probeFake.log[i].data[0] == RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
      commandOrdinal = static_cast<uint32_t>(i + 1U);
      break;
    }
  }
  TEST_ASSERT_NOT_EQUAL(0, commandOrdinal);

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    fake.failOrdinal = commandOrdinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status status =
        rtc.ensurePrimaryCellConfiguration(report);
    TEST_ASSERT_FALSE(status.ok());
    TEST_ASSERT_TRUE(report.writeCommandAttempted);
    TEST_ASSERT_FALSE(report.writeDurablyVerified);
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    assertPrimaryTargetEvidence(report, false, false);
    TEST_ASSERT_TRUE(report.cleanupVerified);
    TEST_ASSERT_TRUE(report.autoRefreshHeldDisabledForSafety);
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    fake.failOrdinal = commandOrdinal;
    fake.failCommandAfterCommit = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_TRUE(report.writeDurablyVerified);
    assertPrimaryTargetEvidence(report, true, true);
  }
}

void test_typed_controls_use_correct_vendor_bits() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  TEST_ASSERT_TRUE(rtc.enableAlarmInterrupt(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0x08,
      fake.direct[RV3032::cmd::REG_CONTROL2] & 0x0C);
  TEST_ASSERT_TRUE(rtc.setPeriodicUpdate(
      RV3032::PeriodicUpdateFrequency::MINUTE, true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_BITS_HIGH(static_cast<uint8_t>(1u << RV3032::cmd::CTRL1_USEL_BIT),
                        fake.direct[RV3032::cmd::REG_CONTROL1]);
  TEST_ASSERT_BITS_HIGH(static_cast<uint8_t>(1u << RV3032::cmd::CTRL2_UIE_BIT),
                        fake.direct[RV3032::cmd::REG_CONTROL2]);
}

void test_control_setters_and_flag_clears_are_cooperative() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());

  const uint32_t before = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
      RV3032::TrickleChargeMode::V3_0).inProgress());
  TEST_ASSERT_EQUAL_UINT32(before, fake.callbackCount);
  uint8_t used = 99;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 0, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(before, fake.callbackCount);
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0x02,
      fake.activeConfig[0] & RV3032::cmd::PMU_TCM_MASK);

  fake.direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_THF_BIT) |
      (1u << RV3032::cmd::STATUS_AF_BIT));
  TEST_ASSERT_TRUE(rtc.clearAlarmFlag().inProgress());
  RV3032::Status st = pollJobToCompletion(rtc, fake);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_BITS_HIGH(static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_THF_BIT) |
      (1u << RV3032::cmd::STATUS_AF_BIT)),
      fake.direct[RV3032::cmd::REG_STATUS]);

  const uint8_t clearMask = static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_THF_BIT) |
      (1u << RV3032::cmd::STATUS_AF_BIT));
  TEST_ASSERT_TRUE(rtc.clearStatus(clearMask).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0, fake.direct[RV3032::cmd::REG_STATUS]);
}

void test_primary_cell_wait_and_wrap_guards() {
  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
    fake.resetFromPersistent();
    fake.earlyWait = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    TEST_ASSERT_FALSE(rtc.ensurePrimaryCellConfiguration(report).ok());
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::PrimaryCellConfigurationOutcome::FAILED),
        static_cast<uint8_t>(report.outcome));
  }
  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
    fake.resetFromPersistent();
    fake.nowMs = UINT32_MAX - 5U;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
    TEST_ASSERT_TRUE(report.cleanupVerified);
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  }
}

void test_primary_cell_callback_timeouts_clip_to_transfer_and_overall() {
  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
    fake.resetFromPersistent();
    RV3032::Config config = fake.config();
    config.i2cTimeoutMs = 100;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(config).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());

    bool sawTransferCap = false;
    for (size_t i = 0; i < fake.logCount; ++i) {
      TEST_ASSERT_GREATER_THAN_UINT32(0, fake.log[i].timeoutMs);
      TEST_ASSERT_LESS_OR_EQUAL_UINT32(5, fake.log[i].timeoutMs);
      sawTransferCap = sawTransferCap || fake.log[i].timeoutMs == 5;
    }
    TEST_ASSERT_TRUE(sawTransferCap);
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
    fake.resetFromPersistent();
    fake.lateCallbackOrdinal = 1;
    fake.lateCallbackExtraMs = 997;
    RV3032::Config config = fake.config();
    config.i2cTimeoutMs = 100;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(config).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status status =
        rtc.ensurePrimaryCellConfiguration(report);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(status.code));
    TEST_ASSERT_TRUE(report.cleanupVerified);
    TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(997, fake.nowMs);
    TEST_ASSERT_EQUAL_UINT32(5, fake.log[0].timeoutMs);
  }
}

void test_primary_cell_initial_66ms_busy_polls_at_one_ms_and_succeeds() {
  FakeRv3032 fake;
  fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
  fake.resetFromPersistent();
  fake.setInitialBusy(66);
  RV3032::Config config = fake.config();
  config.i2cTimeoutMs = 100;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(config).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());

  size_t oneMsWaits = 0;
  for (size_t i = 0; i < fake.waitLogCount; ++i) {
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, fake.waitRequests[i]);
    if (fake.waitRequests[i] == 1) ++oneMsWaits;
  }
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(66,
      static_cast<uint32_t>(oneMsWaits));
  TEST_ASSERT_TRUE(report.cleanupVerified);
  TEST_ASSERT_EQUAL_UINT16(1, fake.readOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(1000, fake.nowMs);
  TEST_ASSERT_FALSE(fake.waitLogOverflow);
  TEST_ASSERT_FALSE(fake.protocolViolation);
}

void test_primary_cell_late_wait_is_detected_without_further_io() {
  FakeRv3032 fake;
  fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
  fake.resetFromPersistent();
  fake.lateWaitExtraMs = 1000;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  const RV3032::Status status =
      rtc.ensurePrimaryCellConfiguration(report);

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
      static_cast<uint8_t>(status.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(report.operationStatus.code));
  TEST_ASSERT_FALSE(report.cleanupVerified);
  TEST_ASSERT_EQUAL_UINT32(1, fake.waitCount);
  TEST_ASSERT_EQUAL_UINT32(1, fake.waitRequests[0]);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1000, fake.nowMs);
  TEST_ASSERT_EQUAL_UINT16(1, fake.readOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  TEST_ASSERT_TRUE(fake.logCount > 0);
  TEST_ASSERT_TRUE(fake.log[fake.logCount - 1U].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_EE_COMMAND,
                         fake.log[fake.logCount - 1U].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::EEPROM_CMD_READ_ONE,
                         fake.log[fake.logCount - 1U].data[0]);
}

void test_primary_cell_staging_crossing_cutoff_starts_no_write_one() {
  FakeRv3032 probe;
  probe.persistent[0] = 0;
  probe.resetFromPersistent();
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config()).ok());
  RV3032::PrimaryCellConfigurationReport probeReport{};
  TEST_ASSERT_TRUE(probeRtc.ensurePrimaryCellConfiguration(probeReport).ok());
  const uint32_t stagingOrdinal = findWriteDataStagingOrdinal(probe);
  TEST_ASSERT_NOT_EQUAL(0, stagingOrdinal);

  FakeRv3032 fake;
  fake.persistent[0] = 0;
  fake.resetFromPersistent();
  fake.lateCallbackOrdinal = stagingOrdinal;
  fake.lateCallbackExtraMs = 500;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  const RV3032::Status status =
      rtc.ensurePrimaryCellConfiguration(report);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(report.operationStatus.code));
  TEST_ASSERT_FALSE(report.writeCommandAttempted);
  assertPrimaryTargetEvidence(report, false, false);
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
      fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
  TEST_ASSERT_TRUE(report.cleanupVerified);
  TEST_ASSERT_TRUE(report.autoRefreshHeldDisabledForSafety);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(1000, fake.nowMs);
  TEST_ASSERT_FALSE(fake.protocolViolation);
  TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
}

void test_primary_cell_busy_phases_terminate_within_fixed_bounds() {
  FakeRv3032 fake;
  fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
  fake.resetFromPersistent();
  fake.setInitialBusy(2000);
  fake.callbackDurationMs = 3;
  RV3032::Config config = fake.config();
  config.i2cTimeoutMs = 100;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(config).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  const RV3032::Status status =
      rtc.ensurePrimaryCellConfiguration(report);

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
      static_cast<uint8_t>(status.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(report.operationStatus.code));
  TEST_ASSERT_FALSE(report.cleanupVerified);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(1000, fake.nowMs);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(515, fake.callbackCount);
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(512, fake.waitCount);
  TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
      fake, RV3032::cmd::EEPROM_CMD_READ_ONE));
  TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
      fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));

  bool sawPhaseRemainderClip = false;
  for (size_t i = 0; i < fake.logCount; ++i) {
    TEST_ASSERT_GREATER_THAN_UINT32(0, fake.log[i].timeoutMs);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(5, fake.log[i].timeoutMs);
    sawPhaseRemainderClip = sawPhaseRemainderClip ||
        fake.log[i].timeoutMs < 5;
  }
  TEST_ASSERT_TRUE(sawPhaseRemainderClip);
  TEST_ASSERT_FALSE(fake.logOverflow);
  TEST_ASSERT_FALSE(fake.waitLogOverflow);
  TEST_ASSERT_FALSE(fake.protocolViolation);
}

void test_primary_cell_fault_ordinals_never_retry_write_one() {
  FakeRv3032 writeProbe;
  writeProbe.persistent[0] = 0;
  writeProbe.resetFromPersistent();
  RV3032::RV3032 writeProbeRtc;
  TEST_ASSERT_TRUE(writeProbeRtc.begin(writeProbe.config()).ok());
  RV3032::PrimaryCellConfigurationReport writeProbeReport{};
  TEST_ASSERT_TRUE(
      writeProbeRtc.ensurePrimaryCellConfiguration(writeProbeReport).ok());
  const uint32_t writePathCallbacks = writeProbe.callbackCount;
  const uint32_t writePersistentProof =
      findPersistentProofOrdinal(writeProbe, 2);
  const uint32_t writeActiveProof =
      findActiveTargetProofOrdinal(writeProbe);
  TEST_ASSERT_NOT_EQUAL(0, writePersistentProof);
  TEST_ASSERT_NOT_EQUAL(0, writeActiveProof);

  for (uint32_t ordinal = 1; ordinal <= writePathCallbacks; ++ordinal) {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    const uint8_t initialActive = fake.activeConfig[0];
    const uint8_t initialControl1 =
        fake.direct[RV3032::cmd::REG_CONTROL1];
    fake.failOrdinal = ordinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status status =
        rtc.ensurePrimaryCellConfiguration(report);
    assertPrimaryFaultCleanup(fake, rtc, status, report, ordinal,
                              initialActive, initialControl1,
                              ordinal > writePersistentProof,
                              ordinal > writeActiveProof);
  }

  FakeRv3032 correctProbe;
  correctProbe.persistent[0] = 0x2C;
  correctProbe.resetFromPersistent();
  RV3032::RV3032 correctProbeRtc;
  TEST_ASSERT_TRUE(correctProbeRtc.begin(correctProbe.config()).ok());
  RV3032::PrimaryCellConfigurationReport correctProbeReport{};
  TEST_ASSERT_TRUE(
      correctProbeRtc.ensurePrimaryCellConfiguration(correctProbeReport).ok());
  const uint32_t correctPathCallbacks = correctProbe.callbackCount;
  const uint32_t correctPersistentProof =
      findPersistentProofOrdinal(correctProbe, 1);
  const uint32_t correctActiveProof =
      findActiveTargetProofOrdinal(correctProbe);
  TEST_ASSERT_NOT_EQUAL(0, correctPersistentProof);
  TEST_ASSERT_NOT_EQUAL(0, correctActiveProof);

  for (uint32_t ordinal = 1; ordinal <= correctPathCallbacks; ++ordinal) {
    FakeRv3032 fake;
    fake.persistent[0] = 0x2C;
    fake.resetFromPersistent();
    const uint8_t initialActive = fake.activeConfig[0];
    const uint8_t initialControl1 =
        fake.direct[RV3032::cmd::REG_CONTROL1];
    fake.failOrdinal = ordinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status status =
        rtc.ensurePrimaryCellConfiguration(report);
    assertPrimaryFaultCleanup(fake, rtc, status, report, ordinal,
                              initialActive, initialControl1,
                              ordinal > correctPersistentProof,
                              ordinal > correctActiveProof);
  }
}

void test_primary_cell_acked_ignore_and_low_vdd_fail_safely() {
  FakeRv3032 probe;
  probe.persistent[0] = 0;
  probe.resetFromPersistent();
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config()).ok());
  RV3032::PrimaryCellConfigurationReport probeReport{};
  TEST_ASSERT_TRUE(
      probeRtc.ensurePrimaryCellConfiguration(probeReport).ok());
  const uint32_t writeCommandOrdinal = findEepromCommandOrdinal(
      probe, RV3032::cmd::EEPROM_CMD_WRITE_ONE);
  TEST_ASSERT_NOT_EQUAL(0, writeCommandOrdinal);

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    const uint8_t initialActive = fake.activeConfig[0];
    const uint8_t initialControl1 =
        fake.direct[RV3032::cmd::REG_CONTROL1];
    fake.ignoreWriteOrdinal = writeCommandOrdinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status status =
        rtc.ensurePrimaryCellConfiguration(report);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
        static_cast<uint8_t>(status.code));
    assertPrimaryFaultCleanup(fake, rtc, status, report,
                              writeCommandOrdinal, initialActive,
                              initialControl1, false, false);
    TEST_ASSERT_TRUE(report.writeCommandAttempted);
    TEST_ASSERT_FALSE(report.writeDurablyVerified);
    TEST_ASSERT_TRUE(report.persistentAfterValid);
    TEST_ASSERT_EQUAL_HEX8(0, report.persistentAfter);
    TEST_ASSERT_EQUAL_HEX8(0, fake.persistent[0]);
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT16(2, fake.readOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_TRUE(report.cleanupVerified);
    TEST_ASSERT_TRUE(report.autoRefreshHeldDisabledForSafety);
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    const uint8_t initialActive = fake.activeConfig[0];
    const uint8_t initialControl1 =
        fake.direct[RV3032::cmd::REG_CONTROL1];
    fake.lowVdd = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status status =
        rtc.ensurePrimaryCellConfiguration(report);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_WRITE_FAILED),
        static_cast<uint8_t>(status.code));
    assertPrimaryFaultCleanup(fake, rtc, status, report, 1, initialActive,
                              initialControl1, false, false);
    TEST_ASSERT_TRUE(report.writeCommandAttempted);
    TEST_ASSERT_FALSE(report.writeDurablyVerified);
    TEST_ASSERT_TRUE(report.persistentAfterValid);
    TEST_ASSERT_EQUAL_HEX8(0, report.persistentAfter);
    TEST_ASSERT_EQUAL_HEX8(0, fake.persistent[0]);
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT16(2, fake.readOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_BITS_HIGH(RV3032::cmd::EEPROM_EEF_MASK,
                          fake.direct[RV3032::cmd::REG_TEMP_LSB]);
    TEST_ASSERT_TRUE(report.cleanupVerified);
    TEST_ASSERT_TRUE(report.autoRefreshHeldDisabledForSafety);
  }
}

void test_primary_cell_reconciles_committed_read_one_errors() {
  FakeRv3032 probe;
  probe.persistent[0] = 0;
  probe.resetFromPersistent();
  probe.activeConfig[0] = 0x13;
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config()).ok());
  RV3032::PrimaryCellConfigurationReport probeReport{};
  TEST_ASSERT_TRUE(
      probeRtc.ensurePrimaryCellConfiguration(probeReport).ok());

  uint32_t readCommandOrdinals[2] = {};
  uint8_t readCommandCount = 0;
  for (size_t i = 0; i < probe.logCount; ++i) {
    if (probe.log[i].write &&
        probe.log[i].reg == RV3032::cmd::REG_EE_COMMAND &&
        probe.log[i].length == 1 &&
        probe.log[i].data[0] == RV3032::cmd::EEPROM_CMD_READ_ONE) {
      TEST_ASSERT_LESS_THAN_UINT8(2, readCommandCount);
      readCommandOrdinals[readCommandCount++] =
          static_cast<uint32_t>(i + 1U);
    }
  }
  TEST_ASSERT_EQUAL_UINT8(2, readCommandCount);

  const RV3032::Err errors[] = {
      RV3032::Err::I2C_NACK_DATA,
      RV3032::Err::I2C_TIMEOUT,
      RV3032::Err::I2C_BUS,
  };
  for (RV3032::Err error : errors) {
    for (uint8_t commandIndex = 0; commandIndex < readCommandCount;
         ++commandIndex) {
      FakeRv3032 fake;
      fake.persistent[0] = 0;
      fake.resetFromPersistent();
      fake.activeConfig[0] = 0x13;
      fake.failOrdinal = readCommandOrdinals[commandIndex];
      fake.failError = error;
      fake.failCommandAfterCommit = true;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
      RV3032::PrimaryCellConfigurationReport report{};
      TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());

      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(
              RV3032::PrimaryCellConfigurationOutcome::EEPROM_UPDATED),
          static_cast<uint8_t>(report.outcome));
      TEST_ASSERT_TRUE(report.operationStatus.ok());
      TEST_ASSERT_TRUE(report.cleanupStatus.ok());
      TEST_ASSERT_TRUE(report.writeDurablyVerified);
      TEST_ASSERT_TRUE(report.cleanupVerified);
      assertPrimaryTargetEvidence(report, true, true);
      TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
      TEST_ASSERT_EQUAL_UINT16(2, fake.readOneAttempts);
      TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
          fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
      TEST_ASSERT_EQUAL_UINT32(2, countEepromCommands(
          fake, RV3032::cmd::EEPROM_CMD_READ_ONE));
      TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
      TEST_ASSERT_FALSE(fake.protocolViolation);
      TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
    }
  }
}

void test_primary_cell_committed_noncommand_errors_never_false_succeed() {
  FakeRv3032 probe;
  probe.persistent[0] = 0;
  probe.resetFromPersistent();
  probe.activeConfig[0] = 0x13;
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config()).ok());
  RV3032::PrimaryCellConfigurationReport probeReport{};
  TEST_ASSERT_TRUE(
      probeRtc.ensurePrimaryCellConfiguration(probeReport).ok());
  const uint32_t persistentProof = findPersistentProofOrdinal(probe, 2);
  const uint32_t activeProof = findActiveTargetProofOrdinal(probe);
  TEST_ASSERT_NOT_EQUAL(0, persistentProof);
  TEST_ASSERT_NOT_EQUAL(0, activeProof);

  uint32_t mutationOrdinals[24] = {};
  uint8_t mutationCount = 0;
  for (size_t i = 0; i < probe.logCount; ++i) {
    if (probe.log[i].write &&
        probe.log[i].reg != RV3032::cmd::REG_EE_COMMAND) {
      TEST_ASSERT_LESS_THAN_UINT8(24, mutationCount);
      mutationOrdinals[mutationCount++] = static_cast<uint32_t>(i + 1U);
    }
  }
  TEST_ASSERT_GREATER_THAN_UINT8(0, mutationCount);

  const RV3032::Err errors[] = {
      RV3032::Err::I2C_NACK_DATA,
      RV3032::Err::I2C_TIMEOUT,
      RV3032::Err::I2C_BUS,
  };
  for (RV3032::Err error : errors) {
    for (uint8_t mutationIndex = 0; mutationIndex < mutationCount;
         ++mutationIndex) {
      FakeRv3032 fake;
      fake.persistent[0] = 0;
      fake.resetFromPersistent();
      fake.activeConfig[0] = 0x13;
      const uint8_t initialActive = fake.activeConfig[0];
      const uint8_t initialControl1 =
          fake.direct[RV3032::cmd::REG_CONTROL1];
      fake.failOrdinal = mutationOrdinals[mutationIndex];
      fake.failError = error;
      fake.failWriteAfterCommit = true;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
      RV3032::PrimaryCellConfigurationReport report{};
      const RV3032::Status status =
          rtc.ensurePrimaryCellConfiguration(report);

      assertPrimaryFaultCleanup(fake, rtc, status, report,
                                mutationOrdinals[mutationIndex],
                                initialActive, initialControl1,
                                mutationOrdinals[mutationIndex] >
                                    persistentProof,
                                mutationOrdinals[mutationIndex] >
                                    activeProof);
      TEST_ASSERT_LESS_OR_EQUAL_UINT32(1, countEepromCommands(
          fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
      if (report.cleanupStatus.ok()) {
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(error),
                                static_cast<uint8_t>(status.code));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(error),
            static_cast<uint8_t>(report.operationStatus.code));
      } else {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
            static_cast<uint8_t>(status.code));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
            static_cast<uint8_t>(report.cleanupStatus.code));
      }
    }
  }
}

void test_primary_cell_cleanup_busy_and_settle_expiry_are_terminal() {
  FakeRv3032 probe;
  probe.persistent[0] = 0;
  probe.resetFromPersistent();
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config()).ok());
  RV3032::PrimaryCellConfigurationReport probeReport{};
  TEST_ASSERT_TRUE(
      probeRtc.ensurePrimaryCellConfiguration(probeReport).ok());

  uint32_t finalPersistentReadOrdinal = 0;
  uint8_t readOneCommands = 0;
  for (size_t i = 0; i < probe.logCount; ++i) {
    if (probe.log[i].write &&
        probe.log[i].reg == RV3032::cmd::REG_EE_COMMAND &&
        probe.log[i].length == 1 &&
        probe.log[i].data[0] == RV3032::cmd::EEPROM_CMD_READ_ONE) {
      ++readOneCommands;
    } else if (readOneCommands == 2 && !probe.log[i].write &&
               probe.log[i].reg == RV3032::cmd::REG_EE_DATA) {
      finalPersistentReadOrdinal = static_cast<uint32_t>(i + 1U);
      break;
    }
  }
  TEST_ASSERT_EQUAL_UINT8(2, readOneCommands);
  TEST_ASSERT_NOT_EQUAL(0, finalPersistentReadOrdinal);

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    const uint8_t initialActive = fake.activeConfig[0];
    const uint8_t initialControl1 =
        fake.direct[RV3032::cmd::REG_CONTROL1];
    fake.forceBusyAfterCallbackOrdinal = finalPersistentReadOrdinal;
    fake.forcedBusyDurationMs = 1000;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status status =
        rtc.ensurePrimaryCellConfiguration(report);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(status.code));
    assertPrimaryFaultCleanup(fake, rtc, status, report,
                              finalPersistentReadOrdinal, initialActive,
                              initialControl1, true, false);
    TEST_ASSERT_TRUE(report.operationStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::PrimaryCellFailureStage::CLEANUP),
        static_cast<uint8_t>(report.failureStage));
    TEST_ASSERT_FALSE(report.cleanupVerified);
    TEST_ASSERT_TRUE(report.writeDurablyVerified);
    TEST_ASSERT_TRUE(report.persistentAfterValid);
    TEST_ASSERT_EQUAL_HEX8(report.persistentTarget, report.persistentAfter);
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    TEST_ASSERT_EQUAL_HEX8(
        0, fake.activeConfig[0] &
               static_cast<uint8_t>(RV3032::cmd::PMU_BSM_MASK |
                                    RV3032::cmd::PMU_TCM_MASK));
    TEST_ASSERT_BITS_HIGH(RV3032::cmd::CONTROL1_EERD_MASK,
                          fake.direct[RV3032::cmd::REG_CONTROL1]);
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    const uint8_t initialActive = fake.activeConfig[0];
    const uint8_t initialControl1 =
        fake.direct[RV3032::cmd::REG_CONTROL1];
    fake.lateCallbackOrdinal = probe.callbackCount;
    fake.lateCallbackExtraMs = 1000;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status status =
        rtc.ensurePrimaryCellConfiguration(report);

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TIMEOUT),
        static_cast<uint8_t>(status.code));
    assertPrimaryFaultCleanup(fake, rtc, status, report,
                              probe.callbackCount, initialActive,
                              initialControl1, true, true);
    TEST_ASSERT_TRUE(report.operationStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::PrimaryCellFailureStage::CLEANUP),
        static_cast<uint8_t>(report.failureStage));
    TEST_ASSERT_FALSE(report.cleanupVerified);
    TEST_ASSERT_TRUE(report.writeDurablyVerified);
    TEST_ASSERT_EQUAL_HEX8(report.persistentTarget, report.activeAfter);
    TEST_ASSERT_EQUAL_HEX8(report.persistentTarget, fake.activeConfig[0]);
    TEST_ASSERT_BITS_LOW(RV3032::cmd::CONTROL1_EERD_MASK,
                         fake.direct[RV3032::cmd::REG_CONTROL1]);
    TEST_ASSERT_EQUAL_UINT32(probe.callbackCount, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    fake.lateWaitOrdinal = probe.waitCount;
    fake.lateWaitExtraMs = 1000;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status status =
        rtc.ensurePrimaryCellConfiguration(report);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(status.code));
    TEST_ASSERT_TRUE(report.operationStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::PrimaryCellFailureStage::SETTLE),
        static_cast<uint8_t>(report.failureStage));
    TEST_ASSERT_FALSE(report.cleanupVerified);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(report.cleanupStatus.code));
    assertPrimaryTargetEvidence(report, true, true);
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(probe.waitCount, fake.waitCount);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1000, fake.nowMs);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
  }
}

void test_password_bytes_are_not_exposed_by_raw_access_or_status() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  const RV3032::PasswordCredential credential{{0x12, 0x34, 0x56, 0x78}};
  TEST_ASSERT_TRUE(rtc.unlockPasswordProtection(credential).ok());
  RV3032::PasswordProtectionStatus status{};
  TEST_ASSERT_TRUE(rtc.getPasswordProtectionStatus(status).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PasswordProtectionEvidence::UNKNOWN),
      static_cast<uint8_t>(status.evidence));
  uint8_t value = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.readRegister(RV3032::cmd::REG_PASSWORD0,
                                            value).code));
}

void test_password_enable_uses_write_one_and_redacted_evidence() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  const RV3032::PasswordCredential current{{0, 0, 0, 0}};
  const RV3032::PasswordCredential desired{{0x12, 0x34, 0x56, 0x78}};
  TEST_ASSERT_TRUE(rtc.startConfigurePasswordProtectionJob(
      current, desired, true, fake.nowMs, 4000).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 4).ok());
  RV3032::PasswordProtectionStatus status{};
  TEST_ASSERT_TRUE(rtc.getPasswordProtectionStatus(status).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PasswordProtectionEvidence::VERIFIED_ENABLED),
      static_cast<uint8_t>(status.evidence));
  TEST_ASSERT_TRUE(status.persistentVerified);
  TEST_ASSERT_TRUE(status.cleanupVerified);
  TEST_ASSERT_FALSE(status.credentialAccepted);
  TEST_ASSERT_EQUAL_HEX8(0x12, fake.persistent[6]);
  TEST_ASSERT_EQUAL_HEX8(0x78, fake.persistent[9]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, fake.persistent[10]);
  TEST_ASSERT_FALSE(fake.passwordAuthenticated);
  TEST_ASSERT_TRUE(fake.passwordNewCredentialEstablished);
  TEST_ASSERT_TRUE(fake.passwordCleanupObserved);
  TEST_ASSERT_TRUE(fake.passwordLockObserved);
  TEST_ASSERT_FALSE(fake.passwordReferenceWriteWhileEnabled);
  TEST_ASSERT_FALSE(fake.passwordCleanupBeforeNewCredential);
  TEST_ASSERT_FALSE(fake.passwordProtectionEnabledBeforeCleanup);
  TEST_ASSERT_FALSE(fake.passwordLockBeforeCleanup);
  for (uint8_t i = 0; i < 4; ++i) {
    TEST_ASSERT_NOT_EQUAL(current.bytes[i], fake.passwordInput[i]);
    TEST_ASSERT_NOT_EQUAL(desired.bytes[i], fake.passwordInput[i]);
  }
  TEST_ASSERT_EQUAL_HEX8(0, fake.readByte(RV3032::cmd::REG_EEPROM_PASSWORD0));
  TEST_ASSERT_EQUAL_HEX8(0, fake.readByte(RV3032::cmd::REG_EEPROM_PW_ENABLE));
  TEST_ASSERT_EQUAL_UINT16(0, fake.updateAllAttempts);
  TEST_ASSERT_EQUAL_UINT16(0, fake.refreshAllAttempts);
  TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
}

void test_password_wrong_credential_cannot_change_protected_eeprom() {
  FakeRv3032 fake;
  const uint8_t passwordIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PASSWORD0 -
      RV3032::cmd::CONFIG_EEPROM_START);
  const uint8_t enableIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PW_ENABLE -
      RV3032::cmd::CONFIG_EEPROM_START);
  fake.persistent[passwordIndex + 0] = 0x11;
  fake.persistent[passwordIndex + 1] = 0x22;
  fake.persistent[passwordIndex + 2] = 0x33;
  fake.persistent[passwordIndex + 3] = 0x44;
  fake.persistent[enableIndex] = 0xFF;
  fake.resetFromPersistent();

  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  const RV3032::PasswordCredential wrong{{0x10, 0x20, 0x30, 0x40}};
  const RV3032::PasswordCredential desired{{0x55, 0x66, 0x77, 0x88}};
  TEST_ASSERT_TRUE(rtc.startConfigurePasswordProtectionJob(
      wrong, desired, true, fake.nowMs, 4000).inProgress());
  TEST_ASSERT_FALSE(pollJobToCompletion(rtc, fake, 4).ok());
  TEST_ASSERT_EQUAL_HEX8(0xFF, fake.persistent[enableIndex]);
  TEST_ASSERT_EQUAL_HEX8(0x11, fake.persistent[passwordIndex]);
  RV3032::PasswordProtectionStatus status{};
  TEST_ASSERT_TRUE(rtc.getPasswordProtectionStatus(status).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PasswordProtectionEvidence::UNKNOWN),
      static_cast<uint8_t>(status.evidence));
  TEST_ASSERT_FALSE(fake.passwordAuthenticated);
  TEST_ASSERT_FALSE(fake.passwordInputMatchesActiveReference());
}

void test_password_change_disables_before_reference_and_locks_after_cleanup() {
  const RV3032::PasswordCredential current{{0x11, 0x22, 0x33, 0x44}};
  const RV3032::PasswordCredential desired{{0x55, 0x66, 0x77, 0x88}};
  FakeRv3032 fake;
  const uint8_t passwordIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PASSWORD0 -
      RV3032::cmd::CONFIG_EEPROM_START);
  const uint8_t enableIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PW_ENABLE -
      RV3032::cmd::CONFIG_EEPROM_START);
  for (uint8_t i = 0; i < 4; ++i) {
    fake.persistent[passwordIndex + i] = current.bytes[i];
  }
  fake.persistent[enableIndex] = 0xFF;
  fake.resetFromPersistent();

  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  TEST_ASSERT_TRUE(rtc.startConfigurePasswordProtectionJob(
      current, desired, true, fake.nowMs, 4000).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 4).ok());

  TEST_ASSERT_TRUE(fake.passwordProtectionEnabled());
  TEST_ASSERT_FALSE(fake.passwordAuthenticated);
  TEST_ASSERT_FALSE(fake.passwordInputMatchesActiveReference());
  TEST_ASSERT_TRUE(fake.passwordNewCredentialEstablished);
  TEST_ASSERT_TRUE(fake.passwordCleanupObserved);
  TEST_ASSERT_TRUE(fake.passwordLockObserved);
  TEST_ASSERT_FALSE(fake.passwordReferenceWriteWhileEnabled);
  TEST_ASSERT_FALSE(fake.passwordCleanupBeforeNewCredential);
  TEST_ASSERT_FALSE(fake.passwordProtectionEnabledBeforeCleanup);
  TEST_ASSERT_FALSE(fake.passwordLockBeforeCleanup);
  TEST_ASSERT_EQUAL_HEX8(0xFF, fake.persistent[enableIndex]);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(
      desired.bytes, &fake.persistent[passwordIndex], sizeof(desired.bytes));
}

void test_password_authentication_errors_attempt_bounded_lock() {
  const RV3032::PasswordCredential current{{0x11, 0x22, 0x33, 0x44}};
  const RV3032::PasswordCredential desired{{0x55, 0x66, 0x77, 0x88}};
  const uint8_t passwordIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PASSWORD0 -
      RV3032::cmd::CONFIG_EEPROM_START);
  const uint8_t enableIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PW_ENABLE -
      RV3032::cmd::CONFIG_EEPROM_START);

  for (uint8_t committed = 0; committed < 2; ++committed) {
    FakeRv3032 fake;
    for (uint8_t i = 0; i < 4; ++i) {
      fake.persistent[passwordIndex + i] = current.bytes[i];
    }
    fake.persistent[enableIndex] = 0xFF;
    fake.resetFromPersistent();
    fake.failOrdinal = 1;
    fake.failWriteAfterCommit = committed != 0;

    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.startConfigurePasswordProtectionJob(
        current, desired, true, fake.nowMs, 4000).inProgress());
    const RV3032::Status st = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
    TEST_ASSERT_TRUE(fake.passwordProtectionEnabled());
    TEST_ASSERT_FALSE(fake.passwordAuthenticated);
    TEST_ASSERT_FALSE(fake.passwordInputMatchesActiveReference());
  }

  FakeRv3032 readFailure;
  for (uint8_t i = 0; i < 4; ++i) {
    readFailure.persistent[passwordIndex + i] = current.bytes[i];
  }
  readFailure.persistent[enableIndex] = 0xFF;
  readFailure.resetFromPersistent();
  readFailure.failOrdinal = 2;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(readFailure.config(true)).ok());
  TEST_ASSERT_TRUE(rtc.startConfigurePasswordProtectionJob(
      current, desired, true, readFailure.nowMs, 4000).inProgress());
  const RV3032::Status st = pollJobToCompletion(rtc, readFailure, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(3, readFailure.callbackCount);
  TEST_ASSERT_TRUE(readFailure.passwordProtectionEnabled());
  TEST_ASSERT_FALSE(readFailure.passwordAuthenticated);
  TEST_ASSERT_FALSE(readFailure.passwordInputMatchesActiveReference());

  FakeRv3032 probe;
  for (uint8_t i = 0; i < 4; ++i) {
    probe.persistent[passwordIndex + i] = current.bytes[i];
  }
  probe.persistent[enableIndex] = 0xFF;
  probe.resetFromPersistent();
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config(true)).ok());
  TEST_ASSERT_TRUE(probeRtc.startConfigurePasswordProtectionJob(
      current, desired, true, probe.nowMs, 4000).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(probeRtc, probe, 4).ok());
  uint32_t disableActiveOrdinal = 0;
  for (size_t i = 0; i < probe.logCount; ++i) {
    if (probe.log[i].write &&
        probe.log[i].reg == RV3032::cmd::REG_EEPROM_PW_ENABLE &&
        probe.log[i].length == 1 && probe.log[i].data[0] == 0) {
      disableActiveOrdinal = static_cast<uint32_t>(i + 1U);
      break;
    }
  }
  TEST_ASSERT_NOT_EQUAL(0, disableActiveOrdinal);

  FakeRv3032 ambiguousDisable;
  for (uint8_t i = 0; i < 4; ++i) {
    ambiguousDisable.persistent[passwordIndex + i] = current.bytes[i];
  }
  ambiguousDisable.persistent[enableIndex] = 0xFF;
  ambiguousDisable.resetFromPersistent();
  ambiguousDisable.failOrdinal = disableActiveOrdinal;
  ambiguousDisable.failWriteAfterCommit = true;
  RV3032::RV3032 ambiguousRtc;
  TEST_ASSERT_TRUE(ambiguousRtc.begin(ambiguousDisable.config(true)).ok());
  TEST_ASSERT_TRUE(ambiguousRtc.startConfigurePasswordProtectionJob(
      current, desired, true, ambiguousDisable.nowMs, 4000).inProgress());
  const RV3032::Status ambiguousStatus =
      pollJobToCompletion(ambiguousRtc, ambiguousDisable, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(ambiguousStatus.code));
  TEST_ASSERT_FALSE(ambiguousDisable.passwordProtectionEnabled());
  TEST_ASSERT_FALSE(ambiguousDisable.passwordInputMatchesActiveReference());
  TEST_ASSERT_TRUE(ambiguousDisable.passwordCleanupObserved);
  TEST_ASSERT_TRUE(ambiguousDisable.passwordLockObserved);
  TEST_ASSERT_FALSE(ambiguousDisable.passwordLockBeforeCleanup);
}

void test_static_calendar_and_status_utility_coverage() {
  for (uint8_t value = 0; value <= 99; ++value) {
    TEST_ASSERT_EQUAL_UINT8(
        value, RV3032::RV3032::bcdToBinary(
                   RV3032::RV3032::binaryToBcd(value)));
  }
  TEST_ASSERT_EQUAL_UINT8(1,
      RV3032::RV3032::computeWeekday(2026, 7, 13));

  RV3032::DateTime leap{2024, 2, 29, 23, 59, 58, 0};
  uint32_t unixTime = 0;
  TEST_ASSERT_TRUE(RV3032::RV3032::dateTimeToUnix(leap, unixTime));
  RV3032::DateTime decoded{};
  TEST_ASSERT_TRUE(RV3032::RV3032::unixToDateTime(unixTime, decoded));
  TEST_ASSERT_EQUAL_UINT16(leap.year, decoded.year);
  TEST_ASSERT_EQUAL_UINT8(leap.month, decoded.month);
  TEST_ASSERT_EQUAL_UINT8(leap.day, decoded.day);
  leap.day = 30;
  TEST_ASSERT_FALSE(RV3032::RV3032::isValidDateTime(leap));

  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  fake.direct[RV3032::cmd::REG_STATUS] = 0xFF;
  RV3032::StatusFlags flags{};
  TEST_ASSERT_TRUE(rtc.readStatusFlags(flags).ok());
  TEST_ASSERT_TRUE(flags.tempHigh && flags.tempLow && flags.update &&
                   flags.timer && flags.alarm && flags.event &&
                   flags.powerOnReset && flags.voltageLow);
}

void test_rebegin_rebinds_context_and_resets_only_lifecycle_latch() {
  FakeRv3032 first;
  first.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
  first.resetFromPersistent();
  FakeRv3032 second;
  second.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
  second.resetFromPersistent();
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(first.config()).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
  const uint32_t firstCallbacks = first.callbackCount;
  TEST_ASSERT_TRUE(rtc.recover().ok());
  TEST_ASSERT_EQUAL_UINT32(firstCallbacks + 1U, first.callbackCount);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::PRIMARY_CELL_ALREADY_ATTEMPTED),
      static_cast<uint8_t>(rtc.ensurePrimaryCellConfiguration(report).code));
  TEST_ASSERT_EQUAL_UINT32(firstCallbacks + 1U, first.callbackCount);

  rtc.end();
  TEST_ASSERT_TRUE(rtc.begin(second.config()).ok());
  TEST_ASSERT_EQUAL_UINT32(0, second.callbackCount);
  TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
  TEST_ASSERT_EQUAL_UINT16(1, second.readOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(0, second.writeOneAttempts);
}

void test_alarm_timer_and_pmu_round_trips_are_cooperative() {
  FakeRv3032 fake;
  fake.direct[RV3032::cmd::REG_STATUS] = 0;
  const uint8_t persistentBefore = fake.persistent[0];
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());

  const uint32_t beforeAlarm = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.setAlarmTime(45, 21, 17).inProgress());
  TEST_ASSERT_EQUAL_UINT32(beforeAlarm, fake.callbackCount);
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setAlarmMatch(true, false, true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.enableAlarmInterrupt(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::AlarmConfig alarm{};
  TEST_ASSERT_TRUE(rtc.getAlarmConfig(alarm).ok());
  TEST_ASSERT_EQUAL_UINT8(45, alarm.minute);
  TEST_ASSERT_EQUAL_UINT8(21, alarm.hour);
  TEST_ASSERT_EQUAL_UINT8(17, alarm.date);
  TEST_ASSERT_TRUE(alarm.matchMinute);
  TEST_ASSERT_FALSE(alarm.matchHour);
  TEST_ASSERT_TRUE(alarm.matchDate);
  TEST_ASSERT_TRUE(rtc.setAlarmMatch(false, false, false).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0x80,
      fake.direct[RV3032::cmd::REG_ALARM_MINUTE] & 0x80u);
  TEST_ASSERT_EQUAL_HEX8(0x80,
      fake.direct[RV3032::cmd::REG_ALARM_HOUR] & 0x80u);
  TEST_ASSERT_EQUAL_HEX8(0x80,
      fake.direct[RV3032::cmd::REG_ALARM_DATE] & 0x80u);
  TEST_ASSERT_TRUE(rtc.setAlarmTime(0, 0, 0).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0,
      fake.direct[RV3032::cmd::REG_ALARM_DATE] & 0xBFu);
  TEST_ASSERT_TRUE(rtc.getAlarmConfig(alarm).ok());
  TEST_ASSERT_EQUAL_UINT8(0, alarm.date);
  TEST_ASSERT_TRUE(alarm.matchDate);
  bool enabled = false;
  TEST_ASSERT_TRUE(rtc.getAlarmInterruptEnabled(enabled).ok());
  TEST_ASSERT_TRUE(enabled);
  const uint32_t beforeInvalid = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setAlarmTime(60, 0, 1).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setAlarmTime(0, 0, 32).code));
  TEST_ASSERT_EQUAL_UINT32(beforeInvalid, fake.callbackCount);
  fake.direct[RV3032::cmd::REG_ALARM_HOUR] |= 0x40;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.getAlarmConfig(alarm).code));
  fake.direct[RV3032::cmd::REG_ALARM_HOUR] &= 0xBF;

  TEST_ASSERT_TRUE(rtc.setTimer(0x345, RV3032::TimerFrequency::Hz64,
                                true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setTimerInterruptEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  uint16_t ticks = 0;
  RV3032::TimerFrequency timerFrequency = RV3032::TimerFrequency::Hz4096;
  TEST_ASSERT_TRUE(rtc.getTimer(ticks, timerFrequency, enabled).ok());
  TEST_ASSERT_EQUAL_UINT16(0x345, ticks);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::TimerFrequency::Hz64),
                          static_cast<uint8_t>(timerFrequency));
  TEST_ASSERT_TRUE(enabled);
  TEST_ASSERT_TRUE(rtc.getTimerInterruptEnabled(enabled).ok());
  TEST_ASSERT_TRUE(enabled);
  fake.direct[RV3032::cmd::REG_TIMER_HIGH] |= 0x80;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.getTimer(ticks, timerFrequency, enabled).code));
  fake.direct[RV3032::cmd::REG_TIMER_HIGH] &= 0x0F;
  const uint32_t beforeZeroTimer = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setTimer(
          0, RV3032::TimerFrequency::Hz1, true).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setTimer(
          0, RV3032::TimerFrequency::Hz1, false).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setTimer(
          1, static_cast<RV3032::TimerFrequency>(0xFF), false).code));
  TEST_ASSERT_EQUAL_UINT32(beforeZeroTimer, fake.callbackCount);

  TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
      RV3032::BackupSwitchMode::Direct).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
      RV3032::TrickleChargeMode::V3_0).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setTrickleChargeResistance(
      RV3032::TrickleChargeResistance::KOHM_7).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::BackupSwitchMode backupMode = RV3032::BackupSwitchMode::Off;
  RV3032::TrickleChargeMode chargeMode =
      RV3032::TrickleChargeMode::CHARGER_DISABLED;
  RV3032::TrickleChargeResistance resistance =
      RV3032::TrickleChargeResistance::OHM_600;
  TEST_ASSERT_TRUE(rtc.getBackupSwitchMode(backupMode).ok());
  TEST_ASSERT_TRUE(rtc.getTrickleChargeMode(chargeMode).ok());
  TEST_ASSERT_TRUE(rtc.getTrickleChargeResistance(resistance).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::BackupSwitchMode::Direct),
                          static_cast<uint8_t>(backupMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::TrickleChargeMode::V3_0),
                          static_cast<uint8_t>(chargeMode));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::TrickleChargeResistance::KOHM_7),
      static_cast<uint8_t>(resistance));
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::PMU_BSM_DIRECT |
                         (2u << 2) | 2u,
                         fake.activeConfig[0] &
                             (RV3032::cmd::PMU_BSM_MASK |
                              RV3032::cmd::PMU_TCR_MASK |
                              RV3032::cmd::PMU_TCM_MASK));
  TEST_ASSERT_EQUAL_HEX8(persistentBefore, fake.persistent[0]);
  TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteCount());
  const uint32_t beforeInvalidEnums = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setBackupSwitchMode(
          static_cast<RV3032::BackupSwitchMode>(0xFF)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setTrickleChargeMode(
          static_cast<RV3032::TrickleChargeMode>(0xFF)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setTrickleChargeResistance(
          static_cast<RV3032::TrickleChargeResistance>(0xFF)).code));
  TEST_ASSERT_EQUAL_UINT32(beforeInvalidEnums, fake.callbackCount);
}

void test_clkout_offset_temperature_and_event_round_trips() {
  FakeRv3032 fake;
  fake.activeConfig[1] = 0xC0;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());

  RV3032::ClkoutConfig clkout{};
  clkout.enabled = true;
  clkout.highFrequencyMode = true;
  clkout.xtalFrequency = RV3032::ClkoutFrequency::Hz1024;
  clkout.highFrequencyDivider = 8192;
  const uint32_t beforeClkoutLog = fake.logCount;
  TEST_ASSERT_TRUE(rtc.setClkoutConfig(clkout).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_FALSE(fake.log[beforeClkoutLog].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_ACTIVE_PMU,
                         fake.log[beforeClkoutLog].reg);
  TEST_ASSERT_FALSE(fake.log[beforeClkoutLog + 1U].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TEMP_LSB,
                         fake.log[beforeClkoutLog + 1U].reg);
  TEST_ASSERT_TRUE(fake.log[beforeClkoutLog + 2U].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_ACTIVE_PMU,
                         fake.log[beforeClkoutLog + 2U].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::PMU_NCLKE_MASK,
                         fake.log[beforeClkoutLog + 2U].data[0] &
                             RV3032::cmd::PMU_NCLKE_MASK);
  TEST_ASSERT_TRUE(fake.log[beforeClkoutLog + 3U].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_ACTIVE_PMU,
                         fake.log[beforeClkoutLog + 3U].reg);
  TEST_ASSERT_EQUAL_HEX8(0, fake.log[beforeClkoutLog + 3U].data[0] &
                                RV3032::cmd::PMU_NCLKE_MASK);
  RV3032::ClkoutConfig clkoutRead{};
  TEST_ASSERT_TRUE(rtc.getClkoutConfig(clkoutRead).ok());
  TEST_ASSERT_TRUE(clkoutRead.enabled);
  TEST_ASSERT_TRUE(clkoutRead.highFrequencyMode);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::ClkoutFrequency::Hz1024),
      static_cast<uint8_t>(clkoutRead.xtalFrequency));
  TEST_ASSERT_EQUAL_UINT16(8192, clkoutRead.highFrequencyDivider);
  TEST_ASSERT_EQUAL_HEX8(0xC0, fake.activeConfig[1] & 0xC0u);
  RV3032::ClkoutFrequency legacyFrequency = RV3032::ClkoutFrequency::Hz1;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.getClkoutFrequency(legacyFrequency).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::ClkoutFrequency::Hz1),
                          static_cast<uint8_t>(legacyFrequency));
  bool directClkoutEnabled = true;
  TEST_ASSERT_TRUE(rtc.getClkoutEnabled(directClkoutEnabled).ok());
  TEST_ASSERT_TRUE(directClkoutEnabled);
  RV3032::ClockInterruptMaskConfig clockMask{};
  clockMask.longStopDelay = true;
  clockMask.interruptDelayEnabled = true;
  clockMask.alarmSource = true;
  clockMask.timerSource = true;
  TEST_ASSERT_TRUE(rtc.setClockInterruptMaskConfig(clockMask).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::ClockInterruptMaskConfig clockMaskRead{};
  TEST_ASSERT_TRUE(rtc.getClockInterruptMaskConfig(clockMaskRead).ok());
  TEST_ASSERT_TRUE(clockMaskRead.longStopDelay);
  TEST_ASSERT_TRUE(clockMaskRead.interruptDelayEnabled);
  TEST_ASSERT_TRUE(clockMaskRead.alarmSource);
  TEST_ASSERT_TRUE(clockMaskRead.timerSource);
  TEST_ASSERT_FALSE(clockMaskRead.eventSource);

  fake.direct[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(
      RV3032::cmd::EEPROM_EEF_MASK | RV3032::cmd::TEMP_CLKF_MASK |
      RV3032::cmd::TEMP_BSF_MASK);
  bool clockTriggered = false;
  TEST_ASSERT_TRUE(rtc.getClockOutputFlag(clockTriggered).ok());
  TEST_ASSERT_TRUE(clockTriggered);
  TEST_ASSERT_TRUE(rtc.clearClockOutputFlag().inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_EQUAL_HEX8(
      RV3032::cmd::EEPROM_EEF_MASK | RV3032::cmd::TEMP_BSF_MASK,
      fake.direct[RV3032::cmd::REG_TEMP_LSB] & 0x0Bu);
  TEST_ASSERT_TRUE(rtc.getClockOutputFlag(clockTriggered).ok());
  TEST_ASSERT_FALSE(clockTriggered);

  fake.direct[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(
      RV3032::cmd::EEPROM_BUSY_MASK | RV3032::cmd::TEMP_CLKF_MASK);
  const uint32_t beforeBusyClear = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.clearClockOutputFlag().inProgress());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
  TEST_ASSERT_EQUAL_UINT32(beforeBusyClear + 1, fake.callbackCount);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::TEMP_CLKF_MASK,
                         fake.direct[RV3032::cmd::REG_TEMP_LSB] &
                             RV3032::cmd::TEMP_CLKF_MASK);
  fake.direct[RV3032::cmd::REG_TEMP_LSB] = 0;

  const uint32_t beforeInvalid = fake.callbackCount;
  clkout.highFrequencyDivider = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setClkoutConfig(clkout).code));
  clkout.highFrequencyDivider = 1;
  clkout.xtalFrequency = static_cast<RV3032::ClkoutFrequency>(0xFF);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setClkoutConfig(clkout).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setClkoutFrequency(
          static_cast<RV3032::ClkoutFrequency>(0xFF)).code));
  TEST_ASSERT_EQUAL_UINT32(beforeInvalid, fake.callbackCount);
  TEST_ASSERT_TRUE(rtc.setClkoutFrequency(
      RV3032::ClkoutFrequency::Hz64).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.getClkoutFrequency(legacyFrequency).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::ClkoutFrequency::Hz64),
                          static_cast<uint8_t>(legacyFrequency));
  TEST_ASSERT_TRUE(rtc.getClkoutConfig(clkoutRead).ok());
  TEST_ASSERT_EQUAL_UINT16(8192, clkoutRead.highFrequencyDivider);
  clkoutRead.highFrequencyMode = true;
  fake.direct[RV3032::cmd::REG_CONTROL2] = static_cast<uint8_t>(
      1u << RV3032::cmd::CTRL2_CLKIE_BIT);
  const uint32_t beforeGuardedWrites =
      countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU);
  TEST_ASSERT_TRUE(rtc.setClkoutConfig(clkoutRead).inProgress());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(pollJobToCompletion(rtc, fake).code));
  TEST_ASSERT_EQUAL_UINT32(beforeGuardedWrites,
      countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
  fake.direct[RV3032::cmd::REG_CONTROL2] = 0;
  fake.direct[RV3032::cmd::REG_TEMP_LSB] = RV3032::cmd::TEMP_CLKF_MASK;
  TEST_ASSERT_TRUE(rtc.setClkoutConfig(clkoutRead).inProgress());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(pollJobToCompletion(rtc, fake).code));
  TEST_ASSERT_EQUAL_UINT32(beforeGuardedWrites,
      countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
  fake.direct[RV3032::cmd::REG_TEMP_LSB] = 0;

  constexpr float offsetStep = 1000000.0f / (32768.0f * 128.0f);
  const float requestedPpm = 31.0f * offsetStep;
  TEST_ASSERT_TRUE(rtc.setOffsetPpm(requestedPpm).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  float ppm = 0.0f;
  TEST_ASSERT_TRUE(rtc.getOffsetPpm(ppm).ok());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, requestedPpm, ppm);
  TEST_ASSERT_EQUAL_HEX8(0xC0, fake.activeConfig[1] & 0xC0u);
  TEST_ASSERT_TRUE(rtc.setOffsetPpm(-32.0f * offsetStep).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.getOffsetPpm(ppm).ok());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -32.0f * offsetStep, ppm);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setOffsetPpm(32.0f * offsetStep).code));

  RV3032::TemperatureEventConfig temperature{};
  temperature.lowThresholdC = -10;
  temperature.highThresholdC = 55;
  temperature.lowEventEnabled = true;
  temperature.highEventEnabled = true;
  temperature.lowInterruptEnabled = true;
  temperature.highInterruptEnabled = false;
  temperature.lowTimestampOverwrite = true;
  temperature.highTimestampOverwrite = true;
  const uint32_t beforeTemperatureLog = fake.logCount;
  TEST_ASSERT_TRUE(rtc.setTemperatureEventConfig(temperature).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_FALSE(fake.log[beforeTemperatureLog].write);
  TEST_ASSERT_TRUE(fake.log[beforeTemperatureLog + 1U].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL3,
                         fake.log[beforeTemperatureLog + 1U].reg);
  TEST_ASSERT_EQUAL_HEX8(0, fake.log[beforeTemperatureLog + 1U].data[0] &
      static_cast<uint8_t>((1u << RV3032::cmd::CTRL3_THE_BIT) |
                           (1u << RV3032::cmd::CTRL3_TLE_BIT) |
                           (1u << RV3032::cmd::CTRL3_THIE_BIT) |
                           (1u << RV3032::cmd::CTRL3_TLIE_BIT)));
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TLOW_THRESHOLD,
                         fake.log[beforeTemperatureLog + 3U].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL3,
                         fake.log[beforeTemperatureLog + 5U].reg);
  RV3032::TemperatureEventConfig temperatureRead{};
  TEST_ASSERT_TRUE(rtc.getTemperatureEventConfig(temperatureRead).ok());
  TEST_ASSERT_EQUAL_INT(-10, temperatureRead.lowThresholdC);
  TEST_ASSERT_EQUAL_INT(55, temperatureRead.highThresholdC);
  TEST_ASSERT_TRUE(temperatureRead.lowEventEnabled);
  TEST_ASSERT_TRUE(temperatureRead.highEventEnabled);
  TEST_ASSERT_TRUE(temperatureRead.lowInterruptEnabled);
  TEST_ASSERT_FALSE(temperatureRead.highInterruptEnabled);
  TEST_ASSERT_TRUE(temperatureRead.lowTimestampOverwrite);
  TEST_ASSERT_TRUE(temperatureRead.highTimestampOverwrite);
  temperature.lowThresholdC = 60;
  temperature.highThresholdC = -20;
  TEST_ASSERT_TRUE(rtc.setTemperatureEventConfig(temperature).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.getTemperatureEventConfig(temperatureRead).ok());
  TEST_ASSERT_EQUAL_INT(60, temperatureRead.lowThresholdC);
  TEST_ASSERT_EQUAL_INT(-20, temperatureRead.highThresholdC);
  TEST_ASSERT_TRUE(rtc.setTemperatureReference(-12345).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  int16_t reference = 0;
  TEST_ASSERT_TRUE(rtc.getTemperatureReference(reference).ok());
  TEST_ASSERT_EQUAL_INT(-12345, reference);
  fake.direct[RV3032::cmd::REG_TEMP_LSB] = 0x80;
  fake.direct[RV3032::cmd::REG_TEMP_MSB] = 25;
  float celsius = 0.0f;
  TEST_ASSERT_TRUE(rtc.readTemperatureC(celsius).ok());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.5f, celsius);
  TEST_ASSERT_TRUE(rtc.startReadCoherentTemperatureJob(
      fake.nowMs).inProgress());
  RV3032::CoherentTemperatureResult coherent{};
  TEST_ASSERT_TRUE(rtc.getReadCoherentTemperatureJobResult(
      coherent).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.getReadCoherentTemperatureJobResult(coherent).ok());
  TEST_ASSERT_EQUAL_INT(408, coherent.raw);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.5f, coherent.celsius);

  TEST_ASSERT_TRUE(rtc.startReadCoherentTemperatureJob(
      fake.nowMs).inProgress());
  uint8_t temperatureInstructions = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1,
                              temperatureInstructions).inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, temperatureInstructions);
  fake.direct[RV3032::cmd::REG_TEMP_MSB] = 26;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INCOHERENT_DATA),
      static_cast<uint8_t>(rtc.pollJob(
          fake.nowMs, 1, temperatureInstructions).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INCOHERENT_DATA),
      static_cast<uint8_t>(rtc.getReadCoherentTemperatureJobResult(
          coherent).code));
  fake.direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_THF_BIT) |
      (1u << RV3032::cmd::STATUS_TLF_BIT));
  bool lowTriggered = false;
  bool highTriggered = false;
  TEST_ASSERT_TRUE(rtc.getTemperatureFlags(
      lowTriggered, highTriggered).ok());
  TEST_ASSERT_TRUE(lowTriggered);
  TEST_ASSERT_TRUE(highTriggered);
  TEST_ASSERT_TRUE(rtc.clearTemperatureFlags().inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.getTemperatureFlags(
      lowTriggered, highTriggered).ok());
  TEST_ASSERT_FALSE(lowTriggered);
  TEST_ASSERT_FALSE(highTriggered);

  TEST_ASSERT_TRUE(rtc.setEviEdge(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setEviDebounce(RV3032::EviDebounce::Hz64).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setEviOverwrite(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setEventSynchronizationEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setClkoutStopDelayEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setEventInterruptEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::EviConfig event{};
  TEST_ASSERT_TRUE(rtc.getEviConfig(event).ok());
  TEST_ASSERT_TRUE(event.rising);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::EviDebounce::Hz64),
                          static_cast<uint8_t>(event.debounce));
  TEST_ASSERT_TRUE(event.overwrite);
  bool enabled = false;
  TEST_ASSERT_TRUE(rtc.getEventInterruptEnabled(enabled).ok());
  TEST_ASSERT_TRUE(enabled);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setEviDebounce(
          static_cast<RV3032::EviDebounce>(4)).code));
}

void test_timestamp_stop_gp_and_ram_public_coverage() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());

  uint8_t* timestamp = &fake.direct[RV3032::cmd::REG_TS_EVI_COUNT];
  timestamp[0] = 3;
  timestamp[1] = FakeRv3032::bcd(42);
  timestamp[2] = FakeRv3032::bcd(56);
  timestamp[3] = FakeRv3032::bcd(34);
  timestamp[4] = FakeRv3032::bcd(12);
  timestamp[5] = FakeRv3032::bcd(13);
  timestamp[6] = FakeRv3032::bcd(7);
  timestamp[7] = FakeRv3032::bcd(26);
  RV3032::Timestamp captured{};
  TEST_ASSERT_TRUE(rtc.readTimestamp(RV3032::TimestampSource::Evi,
                                     captured).ok());
  TEST_ASSERT_EQUAL_UINT8(3, captured.count);
  TEST_ASSERT_TRUE(captured.timeValid);
  TEST_ASSERT_TRUE(captured.hasHundredths);
  TEST_ASSERT_EQUAL_UINT8(42, captured.hundredths);
  TEST_ASSERT_EQUAL_UINT16(2026, captured.time.year);
  const uint32_t beforeInvalid = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.readTimestamp(
          static_cast<RV3032::TimestampSource>(3), captured).code));
  TEST_ASSERT_EQUAL_UINT32(beforeInvalid, fake.callbackCount);
  TEST_ASSERT_TRUE(rtc.resetTimestamp(RV3032::TimestampSource::Evi).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0, fake.direct[RV3032::cmd::REG_TS_EVI_COUNT]);
  TEST_ASSERT_BITS_HIGH(
      static_cast<uint8_t>(1u << RV3032::cmd::TS_EVI_RESET_BIT),
      fake.direct[RV3032::cmd::REG_TS_CONTROL]);
  TEST_ASSERT_TRUE(rtc.readTimestamp(RV3032::TimestampSource::Evi,
                                     captured).ok());
  TEST_ASSERT_FALSE(captured.timeValid);
  fake.direct[RV3032::cmd::REG_TS_CONTROL] = 0xE7;
  timestamp[0] = 7;
  timestamp[1] = FakeRv3032::bcd(1);
  timestamp[2] = FakeRv3032::bcd(2);
  timestamp[3] = FakeRv3032::bcd(3);
  timestamp[4] = FakeRv3032::bcd(4);
  timestamp[5] = FakeRv3032::bcd(5);
  timestamp[6] = FakeRv3032::bcd(6);
  timestamp[7] = FakeRv3032::bcd(26);
  TEST_ASSERT_TRUE(rtc.resetTimestamp(
      RV3032::TimestampSource::Evi).inProgress());
  uint8_t resetInstructions = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1,
                              resetInstructions).inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, resetInstructions);
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1,
                              resetInstructions).inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, resetInstructions);
  TEST_ASSERT_EQUAL_UINT8(7,
      fake.direct[RV3032::cmd::REG_TS_EVI_COUNT]);
  TEST_ASSERT_BITS_LOW(
      static_cast<uint8_t>(1u << RV3032::cmd::TS_EVI_RESET_BIT),
      fake.direct[RV3032::cmd::REG_TS_CONTROL]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::TS_CONTROL_OVERWRITE_MASK,
      fake.direct[RV3032::cmd::REG_TS_CONTROL]);
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1,
                              resetInstructions).ok());
  TEST_ASSERT_EQUAL_UINT8(1, resetInstructions);
  TEST_ASSERT_EQUAL_UINT8(0,
      fake.direct[RV3032::cmd::REG_TS_EVI_COUNT]);
  TEST_ASSERT_BITS_HIGH(
      static_cast<uint8_t>(1u << RV3032::cmd::TS_EVI_RESET_BIT),
      fake.direct[RV3032::cmd::REG_TS_CONTROL]);
  TEST_ASSERT_EQUAL_HEX8(0, static_cast<uint8_t>(
      fake.direct[RV3032::cmd::REG_TS_CONTROL] & 0xC0u));
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::TS_CONTROL_OVERWRITE_MASK,
      static_cast<uint8_t>(fake.direct[RV3032::cmd::REG_TS_CONTROL] &
                           RV3032::cmd::TS_CONTROL_OVERWRITE_MASK));
  fake.direct[RV3032::cmd::REG_TS_TLOW_COUNT] = 1;
  fake.direct[RV3032::cmd::REG_TS_TLOW_SECONDS] = 0x80;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.readTimestamp(
          RV3032::TimestampSource::TLow, captured).code));

  fake.direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_THF_BIT) |
      (1u << RV3032::cmd::STATUS_TLF_BIT) |
      (1u << RV3032::cmd::STATUS_EVF_BIT));
  TEST_ASSERT_TRUE(rtc.resetTimestamp(
      RV3032::TimestampSource::TLow).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_BITS_LOW(
      static_cast<uint8_t>(1u << RV3032::cmd::STATUS_TLF_BIT),
      fake.direct[RV3032::cmd::REG_STATUS]);
  TEST_ASSERT_BITS_HIGH(static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_THF_BIT) |
      (1u << RV3032::cmd::STATUS_EVF_BIT)),
      fake.direct[RV3032::cmd::REG_STATUS]);
  TEST_ASSERT_TRUE(rtc.resetTimestamp(
      RV3032::TimestampSource::THigh).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_BITS_LOW(
      static_cast<uint8_t>(1u << RV3032::cmd::STATUS_THF_BIT),
      fake.direct[RV3032::cmd::REG_STATUS]);
  TEST_ASSERT_BITS_HIGH(
      static_cast<uint8_t>(1u << RV3032::cmd::STATUS_EVF_BIT),
      fake.direct[RV3032::cmd::REG_STATUS]);

  TEST_ASSERT_TRUE(rtc.setStopEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setGeneralPurposeBits(true, true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  bool stop = false;
  bool gp0 = false;
  bool gp1 = false;
  TEST_ASSERT_TRUE(rtc.getStopEnabled(stop).ok());
  TEST_ASSERT_TRUE(rtc.getGeneralPurposeBits(gp0, gp1).ok());
  TEST_ASSERT_TRUE(stop && gp0 && gp1);

  const uint8_t smallWrite[3] = {0xA1, 0xB2, 0xC3};
  const uint32_t beforeSmallWrite = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.writeUserRam(4, smallWrite, sizeof(smallWrite)).ok());
  TEST_ASSERT_EQUAL_UINT32(beforeSmallWrite + 1U, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(
      smallWrite, &fake.direct[RV3032::cmd::REG_USER_RAM_START + 4],
      sizeof(smallWrite));

  uint8_t writeData[16] = {};
  for (uint8_t i = 0; i < sizeof(writeData); ++i) writeData[i] = i;
  TEST_ASSERT_TRUE(rtc.writeUserRam(0, writeData, sizeof(writeData)).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  uint8_t readData[16] = {};
  TEST_ASSERT_TRUE(rtc.readUserRam(0, readData, sizeof(readData)).ok());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeData, readData, sizeof(writeData));
  const uint32_t beforeRange = fake.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.writeUserRam(1, writeData,
                                            sizeof(writeData)).code));
  TEST_ASSERT_EQUAL_UINT32(beforeRange, fake.callbackCount);
}

void test_fake_refresh_initial_busy_and_acked_ignore_faults() {
  FakeRv3032 refreshFake;
  refreshFake.activeConfig[0] = 0;
  refreshFake.persistent[0] = 0x6C;
  refreshFake.dailyRefresh();
  TEST_ASSERT_EQUAL_HEX8(0x6C, refreshFake.activeConfig[0]);
  refreshFake.direct[RV3032::cmd::REG_CONTROL1] =
      RV3032::cmd::CONTROL1_EERD_MASK;
  refreshFake.activeConfig[0] = 0x0C;
  refreshFake.persistent[0] = 0x20;
  refreshFake.dailyRefresh();
  TEST_ASSERT_EQUAL_HEX8(0x0C, refreshFake.activeConfig[0]);

  FakeRv3032 busyFake;
  busyFake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
  busyFake.resetFromPersistent();
  busyFake.setInitialBusy(5);
  RV3032::RV3032 busyRtc;
  TEST_ASSERT_TRUE(busyRtc.begin(busyFake.config()).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  TEST_ASSERT_TRUE(busyRtc.ensurePrimaryCellConfiguration(report).ok());
  TEST_ASSERT_TRUE(busyFake.waitedMs >= 5);
  TEST_ASSERT_EQUAL_UINT16(1, busyFake.readOneAttempts);
  TEST_ASSERT_FALSE(busyFake.protocolViolation);

  FakeRv3032 ignoredFake;
  const uint8_t value = 0x5A;
  RV3032::RV3032 ignoredRtc;
  TEST_ASSERT_TRUE(ignoredRtc.begin(ignoredFake.config(true)).ok());
  ignoredFake.ignoreWriteOrdinal = 8;
  TEST_ASSERT_TRUE(ignoredRtc.startWriteUserEepromJob(
      0, &value, 1, ignoredFake.nowMs).inProgress());
  TEST_ASSERT_FALSE(pollJobToCompletion(ignoredRtc, ignoredFake, 1).ok());
  TEST_ASSERT_EQUAL_UINT16(0, ignoredFake.writeOneAttempts);
  RV3032::UserEepromWriteReport ignoredReport{};
  TEST_ASSERT_FALSE(ignoredRtc.getUserEepromWriteJobResult(ignoredReport).ok());
  TEST_ASSERT_TRUE(ignoredReport.cleanupVerified);
  TEST_ASSERT_EQUAL_UINT8(0, ignoredReport.completedBytes);
}

void test_shared_job_transport_failure_is_observable_without_retry() {
  const RV3032::Err injected[] = {RV3032::Err::I2C_NACK_DATA,
                                  RV3032::Err::I2C_TIMEOUT,
                                  RV3032::Err::I2C_BUS};
  for (RV3032::Err error : injected) {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.setClockInterruptEnabled(true).inProgress());
    fake.failOrdinal = 1;
    fake.failError = error;
    const RV3032::Status st = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(error),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(1, fake.physicalAttemptCount);
  }

  FakeRv3032 persistentFake;
  persistentFake.failOrdinal = 1;
  persistentFake.failError = RV3032::Err::I2C_TIMEOUT;
  RV3032::RV3032 persistentRtc;
  TEST_ASSERT_TRUE(persistentRtc.begin(persistentFake.config()).ok());
  TEST_ASSERT_TRUE(persistentRtc.startReadConfigurationEepromJob(
      RV3032::ConfigurationEepromRegister::PMU,
      persistentFake.nowMs, 250).inProgress());
  const RV3032::Status persistentStatus =
      pollJobToCompletion(persistentRtc, persistentFake, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(persistentStatus.code));
  TEST_ASSERT_EQUAL_UINT32(1, persistentFake.physicalAttemptCount);
}

void test_queue_cleanup_failure_cancels_later_items() {
  RV3032::ClkoutConfig config{};
  config.enabled = false;
  config.xtalFrequency = RV3032::ClkoutFrequency::Hz1;

  FakeRv3032 probe;
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config(true)).ok());
  TEST_ASSERT_TRUE(probeRtc.setClkoutConfig(config).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(probeRtc, probe).ok());
  TEST_ASSERT_TRUE(pollEepromToCompletion(probeRtc, probe, 4).ok());
  uint32_t cleanupPmuWriteOrdinal = 0;
  bool writeOneSeen = false;
  for (size_t i = 0; i < probe.logCount; ++i) {
    if (probe.log[i].write &&
        probe.log[i].reg == RV3032::cmd::REG_EE_COMMAND &&
        probe.log[i].length == 1 &&
        probe.log[i].data[0] == RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
      writeOneSeen = true;
    } else if (writeOneSeen && probe.log[i].write &&
               probe.log[i].reg == RV3032::cmd::REG_ACTIVE_PMU) {
      cleanupPmuWriteOrdinal = static_cast<uint32_t>(i + 1U);
      break;
    }
  }
  TEST_ASSERT_NOT_EQUAL(0, cleanupPmuWriteOrdinal);

  FakeRv3032 failed;
  RV3032::RV3032 failedRtc;
  TEST_ASSERT_TRUE(failedRtc.begin(failed.config(true)).ok());
  TEST_ASSERT_TRUE(failedRtc.setClkoutConfig(config).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(failedRtc, failed).ok());
  TEST_ASSERT_EQUAL_UINT8(3, failedRtc.eepromQueueDepth());
  failed.failOrdinal = cleanupPmuWriteOrdinal;
  const RV3032::Status failure =
      pollEepromToCompletion(failedRtc, failed, 4);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
      static_cast<uint8_t>(failure.code));
  TEST_ASSERT_EQUAL_UINT8(0, failedRtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT32(1, failedRtc.eepromWriteFailures());
  TEST_ASSERT_EQUAL_UINT16(1, failed.writeOneAttempts);
}

void test_persistent_cutoff_starts_no_password_mutation_and_locks_if_needed() {
  const RV3032::PasswordCredential current{{0x11, 0x22, 0x33, 0x44}};
  const RV3032::PasswordCredential desired{{0x55, 0x66, 0x77, 0x88}};

  FakeRv3032 untouched;
  RV3032::RV3032 untouchedRtc;
  TEST_ASSERT_TRUE(untouchedRtc.begin(untouched.config(true)).ok());
  TEST_ASSERT_TRUE(untouchedRtc.startConfigurePasswordProtectionJob(
      current, desired, true, untouched.nowMs, 4000).inProgress());
  untouched.nowMs += 3700;
  uint8_t used = 99;
  const RV3032::Status beforeFirstCallback =
      untouchedRtc.pollJob(untouched.nowMs, 1, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(beforeFirstCallback.code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(0, untouched.callbackCount);

  const uint8_t passwordIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PASSWORD0 -
      RV3032::cmd::CONFIG_EEPROM_START);
  const uint8_t enableIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PW_ENABLE -
      RV3032::cmd::CONFIG_EEPROM_START);
  FakeRv3032 authenticated;
  for (uint8_t i = 0; i < 4; ++i) {
    authenticated.persistent[passwordIndex + i] = current.bytes[i];
  }
  authenticated.persistent[enableIndex] = 0xFF;
  authenticated.resetFromPersistent();
  RV3032::RV3032 authenticatedRtc;
  TEST_ASSERT_TRUE(authenticatedRtc.begin(authenticated.config(true)).ok());
  TEST_ASSERT_TRUE(authenticatedRtc.startConfigurePasswordProtectionJob(
      current, desired, true, authenticated.nowMs, 4000).inProgress());
  TEST_ASSERT_TRUE(
      authenticatedRtc.pollJob(authenticated.nowMs, 1, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_TRUE(authenticated.passwordAuthenticated);
  authenticated.nowMs += 3700;
  TEST_ASSERT_TRUE(
      authenticatedRtc.pollJob(authenticated.nowMs, 1, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(0, used);
  const RV3032::Status locked =
      authenticatedRtc.pollJob(authenticated.nowMs, 1, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(locked.code));
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_FALSE(authenticated.passwordAuthenticated);
  TEST_ASSERT_FALSE(authenticated.passwordInputMatchesActiveReference());
}

void test_persistent_deadlines_stop_callbacks_and_report_unverified_cleanup() {
  FakeRv3032 passwordFake;
  const uint8_t passwordIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PASSWORD0 -
      RV3032::cmd::CONFIG_EEPROM_START);
  const uint8_t enableIndex = static_cast<uint8_t>(
      RV3032::cmd::REG_EEPROM_PW_ENABLE -
      RV3032::cmd::CONFIG_EEPROM_START);
  const RV3032::PasswordCredential current{{0x11, 0x22, 0x33, 0x44}};
  const RV3032::PasswordCredential desired{{0x55, 0x66, 0x77, 0x88}};
  for (uint8_t i = 0; i < 4; ++i) {
    passwordFake.persistent[passwordIndex + i] = current.bytes[i];
  }
  passwordFake.persistent[enableIndex] = 0xFF;
  passwordFake.resetFromPersistent();

  RV3032::RV3032 passwordRtc;
  TEST_ASSERT_TRUE(passwordRtc.begin(passwordFake.config(true)).ok());
  TEST_ASSERT_TRUE(passwordRtc.startConfigurePasswordProtectionJob(
      current, desired, true, passwordFake.nowMs, 4000).inProgress());
  uint8_t used = 0;
  TEST_ASSERT_TRUE(passwordRtc.pollJob(passwordFake.nowMs, 1, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_TRUE(passwordFake.passwordAuthenticated);
  const uint32_t passwordCallbacks = passwordFake.callbackCount;
  passwordFake.nowMs += 4000;
  const RV3032::Status passwordDeadline =
      passwordRtc.pollJob(passwordFake.nowMs, 4, used);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
      static_cast<uint8_t>(passwordDeadline.code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(passwordCallbacks, passwordFake.callbackCount);

  FakeRv3032 queueFake;
  RV3032::RV3032 queueRtc;
  TEST_ASSERT_TRUE(queueRtc.begin(queueFake.config(true)).ok());
  RV3032::ClkoutConfig queuedConfig{};
  queuedConfig.enabled = false;
  queuedConfig.xtalFrequency = RV3032::ClkoutFrequency::Hz1;
  TEST_ASSERT_TRUE(queueRtc.setClkoutConfig(queuedConfig).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(queueRtc, queueFake).ok());
  TEST_ASSERT_EQUAL_UINT8(3, queueRtc.eepromQueueDepth());
  TEST_ASSERT_TRUE(queueRtc.pollEeprom(queueFake.nowMs, 1, used).inProgress());
  TEST_ASSERT_TRUE(queueRtc.pollEeprom(queueFake.nowMs, 1, used).inProgress());
  const uint32_t queueCallbacks = queueFake.callbackCount;
  queueFake.nowMs += 4000;
  const RV3032::Status queueDeadline =
      queueRtc.pollEeprom(queueFake.nowMs, 4, used);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
      static_cast<uint8_t>(queueDeadline.code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(queueCallbacks, queueFake.callbackCount);
  TEST_ASSERT_EQUAL_UINT8(0, queueRtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT32(1, queueRtc.eepromWriteFailures());
  const RV3032::Status queueStillFailed =
      queueRtc.pollEeprom(queueFake.nowMs, 4, used);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
      static_cast<uint8_t>(queueStillFailed.code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(queueCallbacks, queueFake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(1, queueRtc.eepromWriteFailures());
}

void test_typed_vendor_controls_and_fake_register_semantics() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());

  fake.direct[RV3032::cmd::REG_100TH_SECONDS] = 0x45;
  uint8_t hundredths = 0;
  TEST_ASSERT_TRUE(rtc.readHundredths(hundredths).ok());
  TEST_ASSERT_EQUAL_UINT8(45, hundredths);
  fake.direct[RV3032::cmd::REG_100TH_SECONDS] = 0xFA;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(rtc.readHundredths(hundredths).code));

  fake.direct[RV3032::cmd::REG_100TH_SECONDS] = 0x78;
  RV3032::DateTime set{2026, 7, 13, 12, 34, 56, 0};
  TEST_ASSERT_TRUE(rtc.setTime(set).ok());
  TEST_ASSERT_EQUAL_HEX8(0, fake.direct[RV3032::cmd::REG_100TH_SECONDS]);

  fake.activeConfig[1] = 0x15;
  TEST_ASSERT_TRUE(rtc.setPowerOnResetInterruptEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0x95, fake.activeConfig[1]);
  TEST_ASSERT_TRUE(rtc.setVoltageLowInterruptEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0xD5, fake.activeConfig[1]);
  bool enabled = false;
  TEST_ASSERT_TRUE(rtc.getPowerOnResetInterruptEnabled(enabled).ok());
  TEST_ASSERT_TRUE(enabled);
  enabled = false;
  TEST_ASSERT_TRUE(rtc.getVoltageLowInterruptEnabled(enabled).ok());
  TEST_ASSERT_TRUE(enabled);

  TEST_ASSERT_TRUE(rtc.setEventSynchronizationEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.setClkoutStopDelayEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  RV3032::EviConfig evi{};
  TEST_ASSERT_TRUE(rtc.getEviConfig(evi).ok());
  TEST_ASSERT_TRUE(evi.synchronized);
  TEST_ASSERT_TRUE(evi.clkoutStopDelay);
  enabled = false;
  TEST_ASSERT_TRUE(rtc.getEventSynchronizationEnabled(enabled).ok());
  TEST_ASSERT_TRUE(enabled);
  enabled = false;
  TEST_ASSERT_TRUE(rtc.getClkoutStopDelayEnabled(enabled).ok());
  TEST_ASSERT_TRUE(enabled);

  fake.direct[RV3032::cmd::REG_100TH_SECONDS] = 0x67;
  TEST_ASSERT_TRUE(rtc.setStopEnabled(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0, fake.direct[RV3032::cmd::REG_100TH_SECONDS]);

  fake.direct[RV3032::cmd::REG_TEMP_LSB] = 0xAF;
  RV3032::EepromHardwareFlags hardwareFlags{};
  TEST_ASSERT_TRUE(rtc.getEepromHardwareFlags(hardwareFlags).ok());
  TEST_ASSERT_TRUE(hardwareFlags.busy);
  TEST_ASSERT_TRUE(hardwareFlags.writeFailed);
  TEST_ASSERT_TRUE(rtc.clearEepromErrorFlag().inProgress());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::BUSY),
      static_cast<uint8_t>(pollJobToCompletion(rtc, fake).code));
  TEST_ASSERT_EQUAL_HEX8(0xAF, fake.direct[RV3032::cmd::REG_TEMP_LSB]);

  fake.direct[RV3032::cmd::REG_TEMP_LSB] = 0xAB;
  TEST_ASSERT_TRUE(rtc.clearEepromErrorFlag().inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0xA3, fake.direct[RV3032::cmd::REG_TEMP_LSB]);
}

void test_fake_busy_command_read_zero_and_transfer_evidence() {
  FakeRv3032 fake;
  fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DISABLED;
  fake.direct[RV3032::cmd::REG_CONTROL1] =
      RV3032::cmd::CONTROL1_EERD_MASK;
  fake.setInitialBusy(100);
  const uint8_t command[2] = {RV3032::cmd::REG_EE_COMMAND,
                              RV3032::cmd::EEPROM_CMD_WRITE_ONE};
  TEST_ASSERT_TRUE(FakeRv3032::writeCallback(
      RV3032::cmd::I2C_ADDR_7BIT, command, sizeof(command), 5, &fake).ok());
  TEST_ASSERT_FALSE(fake.protocolViolation);
  TEST_ASSERT_EQUAL_UINT8(0, fake.pendingCommand);
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);

  const uint8_t reg = RV3032::cmd::REG_EE_COMMAND;
  uint8_t value = 0xFF;
  TEST_ASSERT_TRUE(FakeRv3032::readCallback(
      RV3032::cmd::I2C_ADDR_7BIT, &reg, 1, &value, 1, 5, &fake).ok());
  TEST_ASSERT_EQUAL_HEX8(0, value);
  TEST_ASSERT_EQUAL_HEX8(0, fake.log[fake.logCount - 1U].data[0]);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::OK),
                          static_cast<uint8_t>(fake.log[0].statusCode));

  FakeRv3032 ambiguous;
  ambiguous.failOrdinal = 1;
  ambiguous.failWriteAfterCommit = true;
  const uint8_t ramWrite[2] = {RV3032::cmd::REG_USER_RAM_START, 0x5A};
  TEST_ASSERT_FALSE(FakeRv3032::writeCallback(
      RV3032::cmd::I2C_ADDR_7BIT, ramWrite, sizeof(ramWrite), 5,
      &ambiguous).ok());
  TEST_ASSERT_TRUE(ambiguous.log[0].mayHaveCommitted);
  TEST_ASSERT_EQUAL_HEX8(0x5A,
      ambiguous.direct[RV3032::cmd::REG_USER_RAM_START]);

  FakeRv3032 overflow;
  for (size_t i = 0; i <= FakeRv3032::LOG_CAPACITY; ++i) {
    (void)overflow.record(false, RV3032::cmd::REG_STATUS, nullptr, 1, 5);
  }
  TEST_ASSERT_TRUE(overflow.logOverflow);
  TEST_ASSERT_EQUAL_UINT32(FakeRv3032::LOG_CAPACITY, overflow.logCount);
}

void test_primary_busy_matrix_and_preserves_unrelated_device_state() {
  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.setClockInterruptEnabled(true).inProgress());
    const RV3032::SettingsSnapshot before = rtc.getSettings();
    RV3032::PrimaryCellConfigurationReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(rtc.ensurePrimaryCellConfiguration(report).code));
    const RV3032::SettingsSnapshot after = rtc.getSettings();
    TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
    TEST_ASSERT_TRUE(after.jobBusy);
    TEST_ASSERT_EQUAL(before.primaryCellEnsureAttempted,
                      after.primaryCellEnsureAttempted);
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setBackupSwitchMode(
        RV3032::BackupSwitchMode::Direct).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());
    const uint32_t beforeCallbacks = fake.callbackCount;
    RV3032::PrimaryCellConfigurationReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(rtc.ensurePrimaryCellConfiguration(report).code));
    TEST_ASSERT_EQUAL_UINT32(beforeCallbacks, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());
    TEST_ASSERT_FALSE(rtc.getSettings().primaryCellEnsureAttempted);

    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollEeprom(fake.nowMs, 1, used).inProgress());
    const RV3032::SettingsSnapshot activeBefore = rtc.getSettings();
    const uint32_t activeCallbacks = fake.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(rtc.ensurePrimaryCellConfiguration(report).code));
    const RV3032::SettingsSnapshot activeAfter = rtc.getSettings();
    TEST_ASSERT_EQUAL_UINT32(activeCallbacks, fake.callbackCount);
    TEST_ASSERT_EQUAL(activeBefore.eepromBusy, activeAfter.eepromBusy);
    TEST_ASSERT_EQUAL(activeBefore.jobBusy, activeAfter.jobBusy);
    TEST_ASSERT_FALSE(activeAfter.primaryCellEnsureAttempted);
  }

  FakeRv3032 fake;
  fake.persistent[0] = 0x4F;
  for (uint8_t i = 1; i < 10; ++i) {
    fake.persistent[i] = static_cast<uint8_t>(0x10u + i);
  }
  fake.persistent[10] = 0;
  for (uint8_t i = 11; i < sizeof(fake.persistent); ++i) {
    fake.persistent[i] = static_cast<uint8_t>(0x80u + i);
  }
  fake.resetFromPersistent();
  for (uint8_t i = 0; i <= RV3032::cmd::REG_TIMER_HIGH; ++i) {
    fake.direct[i] = static_cast<uint8_t>(i + 1U);
  }
  for (uint8_t i = RV3032::cmd::REG_USER_RAM_START;
       i <= RV3032::cmd::REG_USER_RAM_END; ++i) {
    fake.direct[i] = static_cast<uint8_t>(0xA0u + i);
  }
  uint8_t calendarAlarmTimer[13] = {};
  uint8_t activeConfigTail[10] = {};
  uint8_t userRam[16] = {};
  uint8_t userEeprom[32] = {};
  memcpy(calendarAlarmTimer, fake.direct, sizeof(calendarAlarmTimer));
  memcpy(activeConfigTail, &fake.activeConfig[1], sizeof(activeConfigTail));
  memcpy(userRam, &fake.direct[RV3032::cmd::REG_USER_RAM_START],
         sizeof(userRam));
  memcpy(userEeprom, &fake.persistent[11], sizeof(userEeprom));

  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
  TEST_ASSERT_EQUAL_MEMORY(calendarAlarmTimer, fake.direct,
                           sizeof(calendarAlarmTimer));
  TEST_ASSERT_EQUAL_MEMORY(activeConfigTail, &fake.activeConfig[1],
                           sizeof(activeConfigTail));
  TEST_ASSERT_EQUAL_MEMORY(userRam,
      &fake.direct[RV3032::cmd::REG_USER_RAM_START], sizeof(userRam));
  TEST_ASSERT_EQUAL_MEMORY(userEeprom, &fake.persistent[11],
                           sizeof(userEeprom));
}

void test_primary_ignored_staging_and_command_phase_deadlines() {
  FakeRv3032 baseline;
  baseline.persistent[0] = 0x4F;
  baseline.resetFromPersistent();
  RV3032::RV3032 baselineRtc;
  TEST_ASSERT_TRUE(baselineRtc.begin(baseline.config()).ok());
  RV3032::PrimaryCellConfigurationReport baselineReport{};
  TEST_ASSERT_TRUE(
      baselineRtc.ensurePrimaryCellConfiguration(baselineReport).ok());

  uint32_t targetDataOrdinal = 0;
  for (size_t i = 0; i < baseline.logCount; ++i) {
    if (baseline.log[i].write &&
        baseline.log[i].reg == RV3032::cmd::REG_EE_DATA &&
        baseline.log[i].length == 1 &&
        baseline.log[i].data[0] == baselineReport.persistentTarget) {
      targetDataOrdinal = static_cast<uint32_t>(i + 1U);
      break;
    }
  }
  const uint32_t ignoredOrdinals[] = {
      findWriteOrdinal(baseline, RV3032::cmd::REG_CONTROL1),
      findWriteOrdinal(baseline, RV3032::cmd::REG_ACTIVE_PMU),
      findWriteOrdinal(baseline, RV3032::cmd::REG_EE_ADDRESS),
      findWriteOrdinal(baseline, RV3032::cmd::REG_EE_DATA),
      targetDataOrdinal,
  };
  for (uint32_t ordinal : ignoredOrdinals) {
    TEST_ASSERT_NOT_EQUAL(0, ordinal);
    FakeRv3032 fake;
    fake.persistent[0] = 0x4F;
    fake.resetFromPersistent();
    fake.ignoreWriteOrdinal = ordinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status st = rtc.ensurePrimaryCellConfiguration(report);
    TEST_ASSERT_FALSE(st.ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::PrimaryCellConfigurationOutcome::FAILED),
        static_cast<uint8_t>(report.outcome));
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT16(0, fake.updateAllAttempts);
    TEST_ASSERT_EQUAL_UINT16(0, fake.refreshAllAttempts);
  }

  FakeRv3032 readBusyBaseline;
  readBusyBaseline.persistent[0] = 0x60;
  readBusyBaseline.resetFromPersistent();
  RV3032::RV3032 readBusyBaselineRtc;
  TEST_ASSERT_TRUE(readBusyBaselineRtc.begin(readBusyBaseline.config()).ok());
  RV3032::PrimaryCellConfigurationReport readBusyBaselineReport{};
  TEST_ASSERT_TRUE(readBusyBaselineRtc.ensurePrimaryCellConfiguration(
      readBusyBaselineReport).ok());
  const uint32_t readOneOrdinal = findEepromCommandOrdinal(
      readBusyBaseline, RV3032::cmd::EEPROM_CMD_READ_ONE);
  TEST_ASSERT_NOT_EQUAL(0, readOneOrdinal);

  FakeRv3032 readBusy;
  readBusy.persistent[0] = 0x60;
  readBusy.resetFromPersistent();
  readBusy.forceBusyAfterCallbackOrdinal = readOneOrdinal;
  readBusy.forcedBusyDurationMs = 50;
  RV3032::RV3032 readBusyRtc;
  TEST_ASSERT_TRUE(readBusyRtc.begin(readBusy.config()).ok());
  RV3032::PrimaryCellConfigurationReport readBusyReport{};
  TEST_ASSERT_FALSE(readBusyRtc.ensurePrimaryCellConfiguration(
      readBusyReport).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PrimaryCellFailureStage::READ_PERSISTENT),
      static_cast<uint8_t>(readBusyReport.failureStage));
  TEST_ASSERT_EQUAL_UINT16(1, readBusy.readOneAttempts);

  const uint32_t writeOneOrdinal = findEepromCommandOrdinal(
      baseline, RV3032::cmd::EEPROM_CMD_WRITE_ONE);
  TEST_ASSERT_NOT_EQUAL(0, writeOneOrdinal);
  FakeRv3032 writeBusy;
  writeBusy.persistent[0] = 0x4F;
  writeBusy.resetFromPersistent();
  writeBusy.forceBusyAfterCallbackOrdinal = writeOneOrdinal;
  writeBusy.forcedBusyDurationMs = 150;
  RV3032::RV3032 writeBusyRtc;
  TEST_ASSERT_TRUE(writeBusyRtc.begin(writeBusy.config()).ok());
  RV3032::PrimaryCellConfigurationReport writeBusyReport{};
  TEST_ASSERT_FALSE(writeBusyRtc.ensurePrimaryCellConfiguration(
      writeBusyReport).ok());
  TEST_ASSERT_EQUAL_UINT16(1, writeBusy.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT16(1, writeBusy.readOneAttempts);
  TEST_ASSERT_EQUAL_UINT32(
      1, countEepromCommands(writeBusy, RV3032::cmd::EEPROM_CMD_READ_ONE));
}

void test_capability_transport_failures_and_owner_read_retry_timing() {
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::DateTime value{};
    TEST_ASSERT_FALSE(rtc.readTime(value).ok());
    TEST_ASSERT_EQUAL_UINT32(1, fake.physicalAttemptCount);
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::AlarmConfig value{};
    TEST_ASSERT_FALSE(rtc.getAlarmConfig(value).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    uint16_t ticks = 0;
    RV3032::TimerFrequency frequency = RV3032::TimerFrequency::Hz1;
    bool enabled = false;
    TEST_ASSERT_FALSE(rtc.getTimer(ticks, frequency, enabled).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PeriodicUpdateFrequency frequency =
        RV3032::PeriodicUpdateFrequency::SECOND;
    bool enabled = false;
    TEST_ASSERT_FALSE(rtc.getPeriodicUpdate(frequency, enabled).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::EviConfig value{};
    TEST_ASSERT_FALSE(rtc.getEviConfig(value).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::Timestamp value{};
    TEST_ASSERT_FALSE(rtc.readTimestamp(RV3032::TimestampSource::Evi,
                                       value).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    float value = 0;
    TEST_ASSERT_FALSE(rtc.readTemperatureC(value).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::ClkoutConfig value{};
    TEST_ASSERT_FALSE(rtc.getClkoutConfig(value).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    float value = 0;
    TEST_ASSERT_FALSE(rtc.getOffsetPpm(value).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::BackupSwitchMode value = RV3032::BackupSwitchMode::Off;
    TEST_ASSERT_FALSE(rtc.getBackupSwitchMode(value).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    uint8_t value = 0;
    TEST_ASSERT_FALSE(rtc.readStatus(value).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::EepromHardwareFlags value{};
    TEST_ASSERT_FALSE(rtc.getEepromHardwareFlags(value).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    uint8_t value = 0;
    TEST_ASSERT_FALSE(rtc.readUserRam(0, &value, 1).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    const RV3032::PasswordCredential credential{{1, 2, 3, 4}};
    TEST_ASSERT_FALSE(rtc.unlockPasswordProtection(credential).ok());
  }
  {
    FakeRv3032 fake;
    fake.failOrdinal = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_FALSE(rtc.recover().ok());
  }

  FakeRv3032 retry;
  retry.setCalendar(2026, 7, 13, 12, 0, 0, 1);
  retry.direct[RV3032::cmd::REG_STATUS] = 0;
  retry.readRecoveryRetryOrdinal = 1;
  retry.readRecoveryDelayMs = 4;
  retry.callbackDurationMs = 5;
  RV3032::RV3032 retryRtc;
  TEST_ASSERT_TRUE(retryRtc.begin(retry.config()).ok());
  TEST_ASSERT_TRUE(retryRtc.startReadTimeSnapshotJob(retry.nowMs, 8).inProgress());
  uint8_t used = 0;
  TEST_ASSERT_TRUE(retryRtc.pollJob(retry.nowMs, 1, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_EQUAL_UINT32(1, retry.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(2, retry.physicalAttemptCount);
  TEST_ASSERT_TRUE(retry.log[0].recoveryUsed);
  TEST_ASSERT_EQUAL_UINT8(2, retry.log[0].physicalAttempts);
  const uint32_t beforeDeadlinePoll = retry.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::TIMEOUT),
      static_cast<uint8_t>(retryRtc.pollJob(retry.nowMs, 1, used).code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(beforeDeadlinePoll, retry.callbackCount);
}

void test_primary_phase4_fault_attribution_and_dispatch_evidence() {
  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
    fake.resetFromPersistent();
    fake.failOrdinal = 3;  // Control 1 verification after EERD-on write.
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status st = rtc.ensurePrimaryCellConfiguration(report);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::PrimaryCellFailureStage::PREPARE_ACCESS),
        static_cast<uint8_t>(report.failureStage));
    TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_READ_ONE));
    TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
  }

  FakeRv3032 readBaseline;
  readBaseline.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
  readBaseline.resetFromPersistent();
  RV3032::RV3032 readBaselineRtc;
  TEST_ASSERT_TRUE(readBaselineRtc.begin(readBaseline.config()).ok());
  RV3032::PrimaryCellConfigurationReport readBaselineReport{};
  TEST_ASSERT_TRUE(readBaselineRtc.ensurePrimaryCellConfiguration(
      readBaselineReport).ok());
  const uint32_t readCommandOrdinal = findEepromCommandOrdinal(
      readBaseline, RV3032::cmd::EEPROM_CMD_READ_ONE);
  TEST_ASSERT_NOT_EQUAL(0, readCommandOrdinal);

  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
    fake.resetFromPersistent();
    fake.failOrdinal = readCommandOrdinal;
    fake.failError = RV3032::Err::I2C_BUS;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status st = rtc.ensurePrimaryCellConfiguration(report);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(readCommandOrdinal), st.detail);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::PrimaryCellFailureStage::READ_PERSISTENT),
        static_cast<uint8_t>(report.failureStage));
    TEST_ASSERT_EQUAL_UINT16(0, fake.readOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_READ_ONE));
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  }

  FakeRv3032 writeBaseline;
  writeBaseline.persistent[0] = 0;
  writeBaseline.resetFromPersistent();
  RV3032::RV3032 writeBaselineRtc;
  TEST_ASSERT_TRUE(writeBaselineRtc.begin(writeBaseline.config()).ok());
  RV3032::PrimaryCellConfigurationReport writeBaselineReport{};
  TEST_ASSERT_TRUE(writeBaselineRtc.ensurePrimaryCellConfiguration(
      writeBaselineReport).ok());
  TEST_ASSERT_NOT_EQUAL(0, writeBaseline.writeOneDispatchNowCall);

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    fake.advanceNowOnCall = writeBaseline.writeOneDispatchNowCall;
    fake.advanceNowByMs = 500;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status st = rtc.ensurePrimaryCellConfiguration(report);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_FALSE(report.writeCommandAttempted);
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
  }

  {
    FakeRv3032 baseline;
    baseline.persistent[0] = 0x4F;
    baseline.resetFromPersistent();
    RV3032::RV3032 baselineRtc;
    TEST_ASSERT_TRUE(baselineRtc.begin(baseline.config()).ok());
    RV3032::PrimaryCellConfigurationReport baselineReport{};
    TEST_ASSERT_TRUE(baselineRtc.ensurePrimaryCellConfiguration(
        baselineReport).ok());
    const uint32_t safePmuWriteOrdinal = findWriteOrdinal(
        baseline, RV3032::cmd::REG_ACTIVE_PMU);
    TEST_ASSERT_NOT_EQUAL(0, safePmuWriteOrdinal);

    FakeRv3032 fake;
    fake.persistent[0] = 0x4F;
    fake.resetFromPersistent();
    fake.failOrdinal = safePmuWriteOrdinal;
    fake.failWriteAfterCommit = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status st = rtc.ensurePrimaryCellConfiguration(report);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::PrimaryCellFailureStage::PREPARE_ACCESS),
        static_cast<uint8_t>(report.failureStage));
    TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(
        fake, RV3032::cmd::REG_ACTIVE_PMU));
    TEST_ASSERT_EQUAL_UINT32(0, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_READ_ONE));
  }

  {
    FakeRv3032 baseline;
    baseline.persistent[0] = 0;
    baseline.resetFromPersistent();
    RV3032::RV3032 baselineRtc;
    TEST_ASSERT_TRUE(baselineRtc.begin(baseline.config()).ok());
    RV3032::PrimaryCellConfigurationReport baselineReport{};
    TEST_ASSERT_TRUE(baselineRtc.ensurePrimaryCellConfiguration(
        baselineReport).ok());
    const uint32_t writeCommandOrdinal = findEepromCommandOrdinal(
        baseline, RV3032::cmd::EEPROM_CMD_WRITE_ONE);

    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    fake.failOrdinal = writeCommandOrdinal;
    fake.failCommandAfterCommit = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport firstReport{};
    TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(firstReport).ok());
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    rtc.end();
    fake.failOrdinal = 0;
    fake.failCommandAfterCommit = false;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport secondReport{};
    TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(secondReport).ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::PrimaryCellConfigurationOutcome::ALREADY_CONFIGURED),
        static_cast<uint8_t>(secondReport.outcome));
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
  }
}

void test_primary_phase4_late_command_ready_window_and_settle_anchor() {
  FakeRv3032 readBaseline;
  readBaseline.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
  readBaseline.resetFromPersistent();
  RV3032::RV3032 readBaselineRtc;
  TEST_ASSERT_TRUE(readBaselineRtc.begin(readBaseline.config()).ok());
  RV3032::PrimaryCellConfigurationReport baselineReport{};
  TEST_ASSERT_TRUE(readBaselineRtc.ensurePrimaryCellConfiguration(
      baselineReport).ok());
  const uint32_t readCommandOrdinal = findEepromCommandOrdinal(
      readBaseline, RV3032::cmd::EEPROM_CMD_READ_ONE);

  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
    fake.resetFromPersistent();
    fake.lateCallbackOrdinal = readCommandOrdinal;
    fake.lateCallbackExtraMs = 6;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    const RV3032::Status st = rtc.ensurePrimaryCellConfiguration(report);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::PrimaryCellFailureStage::READ_PERSISTENT),
        static_cast<uint8_t>(report.failureStage));
    TEST_ASSERT_EQUAL_UINT16(1, fake.readOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_READ_ONE));
    TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
    fake.resetFromPersistent();
    fake.setInitialBusy(245);
    fake.callbackDurationMs = 1;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
    TEST_ASSERT_TRUE(report.cleanupVerified);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(1000, fake.nowMs);
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0x2C;
    fake.resetFromPersistent();
    fake.direct[RV3032::cmd::REG_CONTROL1] =
        RV3032::cmd::CONTROL1_EERD_MASK;
    fake.callbackDurationMs = 5;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
    TEST_ASSERT_TRUE(report.cleanupVerified);
    TEST_ASSERT_BITS_LOW(RV3032::cmd::CONTROL1_EERD_MASK,
                         report.control1After);
    // The two Control 1 cleanup callbacks already cover the 10 ms measured
    // from target-C0 readback, so no extra 10 ms settle request is needed.
    TEST_ASSERT_EQUAL_UINT32(1, fake.waitedMs);
    TEST_ASSERT_EQUAL_UINT32(1, fake.waitCount);
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = 0;
    fake.resetFromPersistent();
    fake.direct[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(
        RV3032::cmd::EEPROM_EEF_MASK | RV3032::cmd::TEMP_CLKF_MASK |
        RV3032::cmd::TEMP_BSF_MASK);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    RV3032::PrimaryCellConfigurationReport report{};
    TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
    TEST_ASSERT_BITS_LOW(RV3032::cmd::EEPROM_EEF_MASK,
                         fake.direct[RV3032::cmd::REG_TEMP_LSB]);
    TEST_ASSERT_BITS_HIGH(
        static_cast<uint8_t>(RV3032::cmd::TEMP_CLKF_MASK |
                             RV3032::cmd::TEMP_BSF_MASK),
        fake.direct[RV3032::cmd::REG_TEMP_LSB]);
  }
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_passive_lifecycle_and_explicit_presence);
  RUN_TEST(test_config_validation_is_zero_io_and_non_destructive);
  RUN_TEST(test_end_preserves_active_work_with_zero_io);
  RUN_TEST(test_tick_zero_budget_and_eeprom_end_guards);
  RUN_TEST(test_offline_is_observational);
  RUN_TEST(test_read_time_is_strict_single_transfer);
  RUN_TEST(test_calendar_weekday_is_user_assigned_and_range_only);
  RUN_TEST(test_status_first_snapshot_job_and_result_contract);
  RUN_TEST(test_status_first_snapshot_rejects_every_invalid_calendar_encoding);
  RUN_TEST(test_status_first_snapshot_exposes_typed_invalid_flags);
  RUN_TEST(test_verified_calendar_accepts_user_assigned_readback_weekday);
  RUN_TEST(test_verified_calendar_set_reports_status_side_effects);
  RUN_TEST(test_verified_calendar_status_payload_is_fixed_for_every_status);
  RUN_TEST(test_verified_calendar_preserves_flags_set_between_read_and_write);
  RUN_TEST(test_verified_calendar_set_validation_and_wrapped_deadline_are_zero_io);
  RUN_TEST(test_calendar_multi_instruction_budget_refreshes_deadlines_and_cutoffs);
  RUN_TEST(test_verified_calendar_accepts_one_second_rollover);
  RUN_TEST(test_verified_calendar_rejects_backward_final_read);
  RUN_TEST(test_verified_calendar_rejects_two_second_final_read);
  RUN_TEST(test_verified_calendar_rejects_2099_hardware_wrap);
  RUN_TEST(test_verified_calendar_reconciles_ambiguous_write_errors);
  RUN_TEST(test_verified_calendar_retains_proven_status_write_on_later_failure);
  RUN_TEST(test_job_budget_and_timer_reserved_bits);
  RUN_TEST(test_raw_access_allowlists_block_side_effect_routes);
  RUN_TEST(test_persistent_inspection_uses_direct_two_read_proof);
  RUN_TEST(test_persistent_read_result_contract_and_partial_evidence);
  RUN_TEST(test_user_eeprom_write_is_compare_once_and_durably_verified);
  RUN_TEST(test_user_eeprom_read_boundaries_and_maximum_chunk);
  RUN_TEST(test_user_eeprom_write_admission_copy_and_partial_failure_report);
  RUN_TEST(test_persistence_queue_capacity_duplicates_fifo_and_admission_guards);
  RUN_TEST(test_settings_snapshot_tracks_job_queue_health_and_deadlines);
  RUN_TEST(test_fake_wait_request_log_is_bounded_and_reports_overflow);
  RUN_TEST(test_generic_persistence_uses_full_budget_and_durable_protocol);
  RUN_TEST(test_generic_persistence_clears_stale_eef_and_restores_access_state);
  RUN_TEST(test_generic_persistence_minimum_write_wait_has_no_early_io);
  RUN_TEST(test_generic_persistence_reports_low_vdd_and_initial_busy_failures);
  RUN_TEST(test_generic_persistence_restores_intended_active_and_zero_write_compare);
  RUN_TEST(test_generic_persistence_retains_first_batch_failure);
  RUN_TEST(test_generic_multi_instruction_poll_refreshes_mutation_cutoff);
  RUN_TEST(test_generic_persistence_staging_and_ambiguous_write_one_paths);
  RUN_TEST(test_public_unix_read_set_and_range_contract);
  RUN_TEST(test_periodic_timer_event_flags_and_guarded_status_clear);
  RUN_TEST(test_primary_cell_already_correct_is_read_only_and_latched);
  RUN_TEST(test_primary_cell_updates_exact_target_once);
  RUN_TEST(test_primary_cell_preserves_all_nclke_tcr_and_control1_bits);
  RUN_TEST(test_primary_cell_rejects_persistent_c0_bit7_without_target_write);
  RUN_TEST(test_primary_cell_repairs_stale_active_from_correct_persistence);
  RUN_TEST(test_primary_cell_preconditions_do_not_consume_attempt);
  RUN_TEST(test_primary_cell_ambiguous_write_is_reconciled_without_retry);
  RUN_TEST(test_typed_controls_use_correct_vendor_bits);
  RUN_TEST(test_control_setters_and_flag_clears_are_cooperative);
  RUN_TEST(test_primary_cell_wait_and_wrap_guards);
  RUN_TEST(test_primary_cell_callback_timeouts_clip_to_transfer_and_overall);
  RUN_TEST(test_primary_cell_initial_66ms_busy_polls_at_one_ms_and_succeeds);
  RUN_TEST(test_primary_cell_late_wait_is_detected_without_further_io);
  RUN_TEST(test_primary_cell_staging_crossing_cutoff_starts_no_write_one);
  RUN_TEST(test_primary_cell_busy_phases_terminate_within_fixed_bounds);
  RUN_TEST(test_primary_cell_fault_ordinals_never_retry_write_one);
  RUN_TEST(test_primary_cell_acked_ignore_and_low_vdd_fail_safely);
  RUN_TEST(test_primary_cell_reconciles_committed_read_one_errors);
  RUN_TEST(test_primary_cell_committed_noncommand_errors_never_false_succeed);
  RUN_TEST(test_primary_cell_cleanup_busy_and_settle_expiry_are_terminal);
  RUN_TEST(test_password_bytes_are_not_exposed_by_raw_access_or_status);
  RUN_TEST(test_password_enable_uses_write_one_and_redacted_evidence);
  RUN_TEST(test_password_wrong_credential_cannot_change_protected_eeprom);
  RUN_TEST(test_password_change_disables_before_reference_and_locks_after_cleanup);
  RUN_TEST(test_password_authentication_errors_attempt_bounded_lock);
  RUN_TEST(test_static_calendar_and_status_utility_coverage);
  RUN_TEST(test_rebegin_rebinds_context_and_resets_only_lifecycle_latch);
  RUN_TEST(test_alarm_timer_and_pmu_round_trips_are_cooperative);
  RUN_TEST(test_clkout_offset_temperature_and_event_round_trips);
  RUN_TEST(test_timestamp_stop_gp_and_ram_public_coverage);
  RUN_TEST(test_fake_refresh_initial_busy_and_acked_ignore_faults);
  RUN_TEST(test_shared_job_transport_failure_is_observable_without_retry);
  RUN_TEST(test_queue_cleanup_failure_cancels_later_items);
  RUN_TEST(test_persistent_cutoff_starts_no_password_mutation_and_locks_if_needed);
  RUN_TEST(test_persistent_deadlines_stop_callbacks_and_report_unverified_cleanup);
  RUN_TEST(test_typed_vendor_controls_and_fake_register_semantics);
  RUN_TEST(test_fake_busy_command_read_zero_and_transfer_evidence);
  RUN_TEST(test_primary_busy_matrix_and_preserves_unrelated_device_state);
  RUN_TEST(test_primary_ignored_staging_and_command_phase_deadlines);
  RUN_TEST(test_capability_transport_failures_and_owner_read_retry_timing);
  RUN_TEST(test_primary_phase4_fault_attribution_and_dispatch_evidence);
  RUN_TEST(test_primary_phase4_late_command_ready_window_and_settle_anchor);
  return UNITY_END();
}
