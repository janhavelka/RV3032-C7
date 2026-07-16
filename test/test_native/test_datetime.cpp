#include <stdint.h>
#include <cstring>
#include <type_traits>

#include <unity.h>

#include "examples/common/I2cTransport.h"
#include "examples/common/CliShell.h"
#include "examples/common/CommandHandler.h"
#include "FakeRv3032.h"
#include "RV3032/RV3032.h"
#include "examples/01_basic_bringup_cli/main.cpp"

using test_rv3032::FakeRv3032;

static_assert(!std::is_copy_constructible<RV3032::RV3032>::value,
              "RV3032 must not be copy constructible");
static_assert(!std::is_copy_assignable<RV3032::RV3032>::value,
              "RV3032 must not be copy assignable");
static_assert(!std::is_move_constructible<RV3032::RV3032>::value,
              "RV3032 must not be move constructible");
static_assert(!std::is_move_assignable<RV3032::RV3032>::value,
              "RV3032 must not be move assignable");
constexpr uint32_t wrappedCounter = UINT32_MAX + uint32_t{1};
static_assert(wrappedCounter == 0U,
              "uint32_t lifetime counters must wrap");

namespace {

uint8_t computedWeekday(uint16_t year, uint8_t month, uint8_t day) {
  uint8_t weekday = 0xFF;
  TEST_ASSERT_TRUE(
      RV3032::RV3032::computeWeekday(year, month, day, weekday).ok());
  return weekday;
}

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

bool isAllowedTransportStatus(RV3032::Err code) {
  return code == RV3032::Err::OK || code == RV3032::Err::I2C_ERROR ||
         code == RV3032::Err::I2C_NACK_ADDR ||
         code == RV3032::Err::I2C_NACK_DATA ||
         code == RV3032::Err::I2C_TIMEOUT || code == RV3032::Err::I2C_BUS;
}

[[maybe_unused]] void compileMaintainedReadmePollingSnippets(
    RV3032::RV3032& rtc, RV3032::NowMsFn nowMs,
    const RV3032::DateTime& value, uint8_t offset, uint8_t length,
    const uint8_t* data) {
  {
    const uint32_t now = nowMs(nullptr);
    RV3032::Status persistence = rtc.tick(now);
    uint8_t used = 0;
    RV3032::Status job = rtc.pollJob(now, 1, used);
    (void)persistence;
    (void)job;
  }
  {
    const uint32_t now = nowMs(nullptr);
    (void)rtc.startReadTimeSnapshotJob(now, 100);
    (void)rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(value, now, 250);
    uint8_t used = 0;
    (void)rtc.pollJob(now, 1, used);
  }
  {
    const uint32_t now = nowMs(nullptr);
    (void)rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::PMU, now);
    (void)rtc.startReadUserEepromJob(offset, length, now);
    (void)rtc.startWriteUserEepromJob(offset, data, length, now, 4000);
  }
  {
    const uint32_t now = nowMs(nullptr);
    (void)rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Level, now);
  }
}

size_t countWritesTo(const FakeRv3032& fake, uint8_t reg) {
  size_t count = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write && fake.log[i].reg == reg) ++count;
  }
  return count;
}

size_t countWritesMatching(const FakeRv3032& fake, uint8_t reg,
                           const uint8_t* data, uint8_t length) {
  size_t count = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write && fake.log[i].reg == reg &&
        fake.log[i].length == length &&
        std::memcmp(fake.log[i].data, data, length) == 0) {
      ++count;
    }
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

enum class PersistentAdvanceTarget : uint8_t {
  CALLBACK_COUNT,
  READ_ONE_COUNT,
  WRITE_ONE_COUNT,
};

bool advancePersistentJobUntil(RV3032::RV3032& rtc, FakeRv3032& fake,
                               PersistentAdvanceTarget target,
                               uint32_t expected,
                               uint16_t pollCap = 1000) {
  auto reached = [&]() -> bool {
    switch (target) {
      case PersistentAdvanceTarget::CALLBACK_COUNT:
        return fake.callbackCount >= expected;
      case PersistentAdvanceTarget::READ_ONE_COUNT:
        return countEepromCommands(
            fake, RV3032::cmd::EEPROM_CMD_READ_ONE) >= expected;
      case PersistentAdvanceTarget::WRITE_ONE_COUNT:
        return fake.writeOneAttempts >= expected;
      default:
        return false;
    }
  };

  for (uint16_t i = 0; i < pollCap; ++i) {
    if (reached()) return true;
    uint8_t used = 0;
    const uint32_t before = fake.callbackCount;
    const RV3032::Status st = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(used, fake.callbackCount - before);
    if (!st.inProgress()) return false;
    if (used == 0) ++fake.nowMs;
  }
  return reached();
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

uint32_t findLastWriteOrdinal(const FakeRv3032& fake, uint8_t reg) {
  uint32_t ordinal = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write && fake.log[i].reg == reg) {
      ordinal = static_cast<uint32_t>(i + 1U);
    }
  }
  return ordinal;
}

uint32_t findLastReadOrdinal(const FakeRv3032& fake, uint8_t reg) {
  uint32_t ordinal = 0;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (!fake.log[i].write && fake.log[i].reg == reg) {
      ordinal = static_cast<uint32_t>(i + 1U);
    }
  }
  return ordinal;
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

void test_passive_lifecycle_and_explicit_probe_evidence() {
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

  fake.failError = RV3032::Err::I2C_BUS;
  fake.failOrdinal = 1;
  uint8_t statusRegister = 0;
  st = rtc.readStatus(statusRegister);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::DEGRADED),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_UINT32(1, rtc.totalFailures());

  fake.failOrdinal = 0;
  const RV3032::SettingsSnapshot beforeSuccessfulProbe = rtc.getSettings();
  st = rtc.probe();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(0, rtc.totalSuccess());
  TEST_ASSERT_FALSE(fake.log[1].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, fake.log[1].reg);
  const RV3032::SettingsSnapshot afterSuccessfulProbe = rtc.getSettings();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeSuccessfulProbe.state),
                          static_cast<uint8_t>(afterSuccessfulProbe.state));
  TEST_ASSERT_EQUAL_UINT32(beforeSuccessfulProbe.totalSuccess,
                           afterSuccessfulProbe.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(beforeSuccessfulProbe.totalFailures,
                           afterSuccessfulProbe.totalFailures);
  TEST_ASSERT_EQUAL_UINT8(beforeSuccessfulProbe.consecutiveFailures,
                          afterSuccessfulProbe.consecutiveFailures);
  TEST_ASSERT_EQUAL_UINT32(beforeSuccessfulProbe.lastOkMs,
                           afterSuccessfulProbe.lastOkMs);
  TEST_ASSERT_EQUAL_UINT32(beforeSuccessfulProbe.lastErrorMs,
                           afterSuccessfulProbe.lastErrorMs);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(beforeSuccessfulProbe.lastError.code),
      static_cast<uint8_t>(afterSuccessfulProbe.lastError.code));
  TEST_ASSERT_EQUAL_INT32(beforeSuccessfulProbe.lastError.detail,
                          afterSuccessfulProbe.lastError.detail);
  TEST_ASSERT_TRUE(beforeSuccessfulProbe.lastError.msg ==
                   afterSuccessfulProbe.lastError.msg);
  st = rtc.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(1, rtc.totalSuccess());
  TEST_ASSERT_FALSE(fake.log[2].write);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, fake.log[2].reg);

  rtc.end();
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
  rtc.end();
  TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);

  FakeRv3032 missing;
  missing.failError = RV3032::Err::I2C_NACK_ADDR;
  missing.failOrdinal = 1;
  RV3032::RV3032 missingRtc;
  TEST_ASSERT_TRUE(missingRtc.begin(missing.config()).ok());
  const RV3032::SettingsSnapshot beforeFailedProbe = missingRtc.getSettings();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
      static_cast<uint8_t>(missingRtc.probe().code));
  TEST_ASSERT_EQUAL_UINT32(0, missingRtc.totalFailures());
  const RV3032::SettingsSnapshot afterFailedProbe = missingRtc.getSettings();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeFailedProbe.state),
                          static_cast<uint8_t>(afterFailedProbe.state));
  TEST_ASSERT_EQUAL_UINT32(beforeFailedProbe.totalSuccess,
                           afterFailedProbe.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(beforeFailedProbe.totalFailures,
                           afterFailedProbe.totalFailures);
  TEST_ASSERT_EQUAL_UINT8(beforeFailedProbe.consecutiveFailures,
                          afterFailedProbe.consecutiveFailures);
  TEST_ASSERT_EQUAL_UINT32(beforeFailedProbe.lastOkMs,
                           afterFailedProbe.lastOkMs);
  TEST_ASSERT_EQUAL_UINT32(beforeFailedProbe.lastErrorMs,
                           afterFailedProbe.lastErrorMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeFailedProbe.lastError.code),
                          static_cast<uint8_t>(afterFailedProbe.lastError.code));
  TEST_ASSERT_EQUAL_INT32(beforeFailedProbe.lastError.detail,
                          afterFailedProbe.lastError.detail);
  TEST_ASSERT_TRUE(beforeFailedProbe.lastError.msg ==
                   afterFailedProbe.lastError.msg);
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

void test_end_unconditionally_abandons_work_with_zero_io() {
  FakeRv3032 first;
  FakeRv3032 second;
  first.nowMs = 100;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(first.config()).ok());
  uint8_t status = 0;
  TEST_ASSERT_TRUE(rtc.readStatus(status).ok());
  first.failOrdinal = first.callbackCount + 1U;
  TEST_ASSERT_FALSE(rtc.readStatus(status).ok());
  TEST_ASSERT_EQUAL_UINT32(1, rtc.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(1, rtc.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(100, rtc.lastOkMs());
  TEST_ASSERT_EQUAL_UINT32(100, rtc.lastErrorMs());
  TEST_ASSERT_FALSE(rtc.lastError().ok());
  TEST_ASSERT_TRUE(rtc.setClockInterruptEnabled(true).inProgress());
  const uint32_t beforeEnd = first.callbackCount;
  rtc.end();
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_FALSE(rtc.isJobBusy());
  TEST_ASSERT_FALSE(rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::DriverState::UNINIT),
      static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_UINT32(0, rtc.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0, rtc.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(0, rtc.lastOkMs());
  TEST_ASSERT_EQUAL_UINT32(0, rtc.lastErrorMs());
  TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());
  TEST_ASSERT_TRUE(rtc.lastError().ok());
  TEST_ASSERT_EQUAL_UINT32(beforeEnd, first.callbackCount);
  rtc.end();
  TEST_ASSERT_EQUAL_UINT32(beforeEnd, first.callbackCount);

  TEST_ASSERT_TRUE(rtc.begin(second.config()).ok());
  TEST_ASSERT_EQUAL_UINT32(0, rtc.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0, rtc.totalFailures());
  TEST_ASSERT_TRUE(rtc.lastError().ok());
  TEST_ASSERT_TRUE(rtc.readStatus(status).ok());
  TEST_ASSERT_EQUAL_UINT32(beforeEnd, first.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(1, second.callbackCount);
  rtc.end();
}

void test_tick_zero_budget_and_eeprom_end_guards() {
  FakeRv3032 fake;
  FakeRv3032 rebound;
  RV3032::RV3032 rtc;
  auto assertFullyReset = [&]() {
    const RV3032::SettingsSnapshot settings = rtc.getSettings();
    TEST_ASSERT_FALSE(rtc.isInitialized());
    TEST_ASSERT_FALSE(rtc.isJobBusy());
    TEST_ASSERT_FALSE(rtc.isEepromBusy());
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::DriverState::UNINIT),
        static_cast<uint8_t>(rtc.state()));
    TEST_ASSERT_EQUAL_UINT32(0, rtc.totalSuccess());
    TEST_ASSERT_EQUAL_UINT32(0, rtc.totalFailures());
    TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteCount());
    TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteFailures());
    TEST_ASSERT_EQUAL_UINT32(0, rtc.lastOkMs());
    TEST_ASSERT_EQUAL_UINT32(0, rtc.lastErrorMs());
    TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());
    TEST_ASSERT_TRUE(rtc.lastError().ok());
    TEST_ASSERT_FALSE(settings.primaryCellEnsureAttempted);
    TEST_ASSERT_NULL(rtc.getConfig().i2cWrite);
    TEST_ASSERT_NULL(rtc.getConfig().i2cWriteRead);
    TEST_ASSERT_NULL(rtc.getConfig().i2cUser);
  };
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
  assertFullyReset();
  TEST_ASSERT_EQUAL_UINT32(queuedCallbacks, fake.callbackCount);

  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  TEST_ASSERT_TRUE(rtc.setOffsetPpm(0.2384f).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  const uint32_t beforeActive = fake.callbackCount;
  for (uint8_t callback = 0; callback < 5; ++callback) {
    rtc.tick(fake.nowMs);
  }
  TEST_ASSERT_EQUAL_UINT32(beforeActive + 5U, fake.callbackCount);
  TEST_ASSERT_TRUE(rtc.isEepromBusy());
  const uint32_t activeCallbacks = fake.callbackCount;
  rtc.end();
  assertFullyReset();
  TEST_ASSERT_EQUAL_UINT32(activeCallbacks, fake.callbackCount);

  TEST_ASSERT_TRUE(rtc.begin(rebound.config(true)).ok());
  rtc.tick(rebound.nowMs);
  TEST_ASSERT_EQUAL_UINT32(0, rebound.callbackCount);
  uint8_t status = 0;
  TEST_ASSERT_TRUE(rtc.readStatus(status).ok());
  TEST_ASSERT_EQUAL_UINT32(activeCallbacks, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(1, rebound.callbackCount);
  rtc.end();
  assertFullyReset();
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
    TEST_ASSERT_EQUAL_HEX8(weekday,
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
    TEST_ASSERT_EQUAL_HEX8(weekday,
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
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
          valid, fake.nowMs, 126).code));
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
        requested, fake.nowMs, 127).inProgress());
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

  {
    FakeRv3032 fake;
    RV3032::Config config = fake.config();
    config.nowMs = nullptr;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(config).ok());
    const RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 0};
    // i2cTimeoutMs=5: max(7*5, 125 + 4*5 + 2) = 147 ms.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
            requested, 0, 146).code));
    TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
    TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
        requested, 0, 147).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(0, 7, used).ok());
    TEST_ASSERT_EQUAL_UINT8(7, used);
    TEST_ASSERT_EQUAL_UINT32(7, fake.callbackCount);
  }
}

void test_verified_calendar_retains_callback_status_on_timed_reconciliation() {
  const RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 1};

  {
    FakeRv3032 fake;
    fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
        requested, 0, 250).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(0, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(1, used);

    // The calendar callback is admitted 6 ms before the mutation cutoff,
    // receives 5 ms, returns OK after 6 ms, and has committed the write.
    fake.nowMs = 119;
    fake.callbackDurationMs = 5;
    fake.lateCallbackOrdinal = 2;
    fake.lateCallbackExtraMs = 1;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
        static_cast<uint8_t>(rtc.lastError().code));
    fake.callbackDurationMs = 0;

    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(terminal.code));
    RV3032::VerifiedTimeSetReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TIMEOUT),
        static_cast<uint8_t>(
            rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).code));
    TEST_ASSERT_TRUE(report.calendarWriteAttempted);
    TEST_ASSERT_TRUE(report.calendarWriteStatus.ok());
    TEST_ASSERT_TRUE(report.calendarWriteAmbiguous);
    TEST_ASSERT_TRUE(report.verifiedValid);
    TEST_ASSERT_FALSE(report.statusWriteAttempted);
    TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_SECONDS));
  }

  {
    FakeRv3032 fake;
    fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
        requested, 0, 250).inProgress());
    uint8_t used = 0;
    for (uint8_t callback = 0; callback < 4; ++callback) {
      TEST_ASSERT_TRUE(rtc.pollJob(0, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(1, used);
    }

    // The Status callback has the same OK-after-timeout outcome. The later
    // Status and calendar readbacks reconcile it without replay.
    fake.nowMs = 119;
    fake.callbackDurationMs = 5;
    fake.lateCallbackOrdinal = 5;
    fake.lateCallbackExtraMs = 1;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
        static_cast<uint8_t>(rtc.lastError().code));
    fake.callbackDurationMs = 0;
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());

    RV3032::VerifiedTimeSetReport report{};
    TEST_ASSERT_TRUE(
        rtc.getSetTimeAndClearInvalidFlagsVerifiedJobResult(report).ok());
    TEST_ASSERT_TRUE(report.statusWriteAttempted);
    TEST_ASSERT_TRUE(report.statusWriteStatus.ok());
    TEST_ASSERT_TRUE(report.statusWriteAmbiguous);
    TEST_ASSERT_TRUE(report.verifiedAfterAmbiguousWrite);
    TEST_ASSERT_EQUAL_UINT32(1, countWritesTo(fake, RV3032::cmd::REG_STATUS));
  }
}

void test_verified_calendar_accepts_one_second_rollover() {
  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
  const RV3032::DateTime requested{
      2026, 12, 31, 23, 59, 59,
      computedWeekday(2026, 12, 31)};
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      requested, fake.nowMs, 250).inProgress());

  uint8_t used = 0;
  for (uint8_t phase = 0; phase < 2; ++phase) {
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(1, used);
  }
  const RV3032::DateTime rolled{
      2027, 1, 1, 0, 0, 0,
      computedWeekday(2027, 1, 1)};
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
      computedWeekday(2026, 7, 13)};
  const RV3032::DateTime first{
      2026, 7, 13, 12, 0, 1,
      computedWeekday(2026, 7, 13)};
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
      computedWeekday(2026, 7, 13)};
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
      computedWeekday(2099, 12, 31)};
  TEST_ASSERT_TRUE(rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
      requested, fake.nowMs, 250).inProgress());
  uint8_t used = 0;
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
  fake.setCalendar(2000, 1, 1, 0, 0, 0,
                   computedWeekday(2000, 1, 1));
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
      computedWeekday(2026, 7, 13)};

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

  const uint8_t unsupportedStarts[] = {
      RV3032::cmd::REG_PASSWORD0,
      RV3032::cmd::REG_EEPROM_PASSWORD0};
  const uint8_t unsupportedEnds[] = {
      RV3032::cmd::REG_PASSWORD3,
      RV3032::cmd::REG_EEPROM_PW_ENABLE};
  for (size_t range = 0; range < 2; ++range) {
    for (uint16_t address = unsupportedStarts[range];
         address <= unsupportedEnds[range]; ++address) {
      const uint32_t callbacks = fake.callbackCount;
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
          static_cast<uint8_t>(rtc.readRegister(
              static_cast<uint8_t>(address), value).code));
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
          static_cast<uint8_t>(rtc.writeRegister(
              static_cast<uint8_t>(address), 0xA5).code));
      TEST_ASSERT_EQUAL_UINT32(callbacks, fake.callbackCount);
      fake.writeByte(static_cast<uint8_t>(address), 0xA5);
      TEST_ASSERT_EQUAL_HEX8(0, fake.readByte(static_cast<uint8_t>(address)));
    }
  }

  uint8_t crossing[2] = {};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.readRegisters(0x38, crossing, 2).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.readRegisters(
          RV3032::cmd::REG_ACTIVE_TREFERENCE1, crossing, 2).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.writeRegisters(0x38, crossing, 2).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.writeRegisters(
          RV3032::cmd::REG_ACTIVE_TREFERENCE1, crossing, 2).code));
  TEST_ASSERT_EQUAL_UINT32(before, fake.callbackCount);

  for (uint16_t address = RV3032::cmd::REG_EEPROM_PASSWORD0;
       address <= RV3032::cmd::REG_EEPROM_PW_ENABLE; ++address) {
    const uint32_t callbacks = fake.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startReadConfigurationEepromJob(
            static_cast<RV3032::ConfigurationEepromRegister>(address),
            fake.nowMs, 1000).code));
    TEST_ASSERT_EQUAL_UINT32(callbacks, fake.callbackCount);
  }

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
      fake.nowMs, 1000).inProgress());
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
  TEST_ASSERT_TRUE(complete.operationStatus.ok());
  TEST_ASSERT_TRUE(complete.cleanupStatus.ok());
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(partial.operationStatus.code));
  TEST_ASSERT_TRUE(partial.cleanupStatus.ok());
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
      wrapped.nowMs, 1000).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(wrappedRtc, wrapped, 1).ok());
  TEST_ASSERT_LESS_THAN_UINT32(100, wrapped.nowMs);
  RV3032::PersistentReadResult wrappedResult{};
  TEST_ASSERT_TRUE(
      wrappedRtc.getPersistentReadJobResult(wrappedResult).ok());
  TEST_ASSERT_EQUAL_HEX8(0x5C, wrappedResult.data[0]);
  TEST_ASSERT_TRUE(wrappedResult.cleanupVerified);
}

void test_persistent_dynamic_cleanup_reserve_admission_is_zero_io() {
  const uint32_t callbackTimeouts[] = {1, 5, 100};
  for (uint32_t callbackTimeout : callbackTimeouts) {
    FakeRv3032 fake;
    RV3032::Config config = fake.config(true);
    config.i2cTimeoutMs = callbackTimeout;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(config).ok());
    const uint32_t cleanupReserve =
        250U + 6U * callbackTimeout + 10U;
    const uint32_t minimum = cleanupReserve + callbackTimeout + 1U;
    const uint32_t before = fake.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startReadConfigurationEepromJob(
            RV3032::ConfigurationEepromRegister::PMU,
            fake.nowMs, minimum - 1U).code));
    const uint8_t value = 0xA5;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startWriteUserEepromJob(
            0, &value, 1, fake.nowMs, minimum - 1U).code));
    TEST_ASSERT_EQUAL_UINT32(before, fake.callbackCount);
    TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::PMU,
        fake.nowMs, minimum).inProgress());
    TEST_ASSERT_EQUAL_UINT32(before, fake.callbackCount);
    rtc.end();
  }
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
  bool sawUserAddress = false;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write && fake.log[i].reg == RV3032::cmd::REG_EE_ADDRESS) {
      sawUserAddress = true;
      TEST_ASSERT_GREATER_OR_EQUAL_HEX8(RV3032::cmd::USER_EEPROM_START,
                                        fake.log[i].data[0]);
      TEST_ASSERT_LESS_OR_EQUAL_HEX8(RV3032::cmd::USER_EEPROM_END,
                                     fake.log[i].data[0]);
    }
  }
  TEST_ASSERT_TRUE(sawUserAddress);
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
  TEST_ASSERT_TRUE(copiedReport.operationStatus.ok());
  TEST_ASSERT_TRUE(copiedReport.cleanupStatus.ok());
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(failedReport.operationStatus.code));
  TEST_ASSERT_TRUE(failedReport.cleanupStatus.ok());
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
      fake.nowMs, 2).inProgress());
  snapshot = rtc.getSettings();
  TEST_ASSERT_TRUE(snapshot.jobBusy);
  TEST_ASSERT_FALSE(snapshot.eepromBusy);
  fake.nowMs += 2U;
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
  TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
      RV3032::TrickleChargeMode::V3_0).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  uint8_t used = 0;
  const uint32_t callbacksBeforePoll = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.pollEeprom(fake.nowMs, 5, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(5, used);
  TEST_ASSERT_EQUAL_UINT32(5, fake.callbackCount - callbacksBeforePoll);
  TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake).ok());
  TEST_ASSERT_EQUAL_HEX8(
      static_cast<uint8_t>(RV3032::TrickleChargeMode::V3_0),
      fake.persistent[0]);
  TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
  TEST_ASSERT_FALSE(fake.protocolViolation);
  TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
  bool sawConfigurationAddress = false;
  for (size_t i = 0; i < fake.logCount; ++i) {
    if (fake.log[i].write && fake.log[i].reg == RV3032::cmd::REG_EE_ADDRESS) {
      sawConfigurationAddress = true;
      TEST_ASSERT_GREATER_OR_EQUAL_HEX8(RV3032::cmd::CONFIG_EEPROM_START,
                                        fake.log[i].data[0]);
      TEST_ASSERT_LESS_OR_EQUAL_HEX8(RV3032::cmd::REG_ACTIVE_TREFERENCE1,
                                     fake.log[i].data[0]);
    }
  }
  TEST_ASSERT_TRUE(sawConfigurationAddress);
}

void test_generic_persistence_clears_stale_eef_and_restores_access_state() {
  FakeRv3032 fake;
  fake.direct[RV3032::cmd::REG_CONTROL1] = 0x3B;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
      RV3032::TrickleChargeMode::V3_0).inProgress());
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
  TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
      RV3032::TrickleChargeMode::V3_0).inProgress());
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
    TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
        RV3032::TrickleChargeMode::V3_0).inProgress());
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
    TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
        RV3032::TrickleChargeMode::V3_0).inProgress());
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
        fake.nowMs, 1000).inProgress());
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
    TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
        RV3032::TrickleChargeMode::V3_0).inProgress());
    fake.ignoreWriteOrdinal = fake.callbackCount + 2U;
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_HEX8(0x4C, fake.activeConfig[0]);
    TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_HEX8(0x4E, fake.persistent[0]);
    TEST_ASSERT_EQUAL_HEX8(0x4E, fake.activeConfig[0]);
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
  TEST_ASSERT_TRUE(probeRtc.setTrickleChargeMode(
      RV3032::TrickleChargeMode::V3_0).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(probeRtc, probe, 1).ok());
  TEST_ASSERT_TRUE(pollEepromToCompletion(probeRtc, probe, 1).ok());
  const uint32_t commandOrdinal = findEepromCommandOrdinal(
      probe, RV3032::cmd::EEPROM_CMD_WRITE_ONE);
  TEST_ASSERT_GREATER_THAN_UINT32(2, commandOrdinal);

  FakeRv3032 fake;
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
  TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
      RV3032::TrickleChargeMode::V3_0).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());

  uint8_t used = 0;
  while (fake.callbackCount < commandOrdinal - 2U) {
    const RV3032::Status st = rtc.pollEeprom(fake.nowMs, 1, used);
    TEST_ASSERT_TRUE(st.inProgress());
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(1, used);
    if (used == 0) ++fake.nowMs;
  }
  TEST_ASSERT_EQUAL_UINT16(0, fake.writeOneAttempts);
  fake.nowMs = 3709;
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
  TEST_ASSERT_TRUE(probeRtc.setTrickleChargeMode(
      RV3032::TrickleChargeMode::V3_0).inProgress());
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
    TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
        RV3032::TrickleChargeMode::V3_0).inProgress());
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
    TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
        RV3032::TrickleChargeMode::V3_0).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    const uint8_t expected = fake.activeConfig[0];
    fake.failOrdinal = commandOrdinal;
    fake.failCommandAfterCommit = true;
    const RV3032::Status terminal = pollEepromToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT32(0, rtc.eepromWriteCount());
    TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteFailures());
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_EQUAL_HEX8(expected, fake.persistent[0]);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(rtc.getEepromStatus().code));
    const RV3032::SettingsSnapshot snapshot = rtc.getSettings();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(snapshot.eepromOperationStatus.code));
    TEST_ASSERT_TRUE(snapshot.eepromCleanupStatus.ok());
    TEST_ASSERT_FALSE(fake.protocolViolation);
    TEST_ASSERT_FALSE(fake.unsafeAccessStateAtCommand);
  }

  {
    FakeRv3032 fake;
    const uint8_t persistentBefore = fake.persistent[0];
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
        RV3032::TrickleChargeMode::V3_0).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    const uint8_t expectedActive = fake.activeConfig[0];
    fake.failOrdinal = commandOrdinal;
    const RV3032::Status status = pollEepromToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_BUS),
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
  bool updateEventEnabled = false;
  TEST_ASSERT_TRUE(rtc.getPeriodicUpdate(frequency, updateEventEnabled).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::PeriodicUpdateFrequency::MINUTE),
      static_cast<uint8_t>(frequency));
  TEST_ASSERT_TRUE(updateEventEnabled);
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

void test_temp_lsb_flag_clear_uses_race_safe_fixed_payloads() {
  const uint8_t targets[] = {
      RV3032::cmd::TEMP_CLKF_MASK,
      RV3032::cmd::EEPROM_EEF_MASK,
      RV3032::cmd::TEMP_BSF_MASK};
  const uint8_t payloads[] = {0x09, 0x03, 0x0A};
  auto startClear = [](RV3032::RV3032& rtc, uint8_t target) {
    if (target == RV3032::cmd::TEMP_CLKF_MASK) {
      return rtc.clearClockOutputFlag();
    }
    if (target == RV3032::cmd::EEPROM_EEF_MASK) {
      return rtc.clearEepromErrorFlag();
    }
    return rtc.clearBackupSwitchFlag();
  };

  for (size_t targetIndex = 0; targetIndex < 3; ++targetIndex) {
    for (size_t neighborIndex = 0; neighborIndex < 3; ++neighborIndex) {
      if (targetIndex == neighborIndex) continue;
      FakeRv3032 fake;
      fake.direct[RV3032::cmd::REG_TEMP_LSB] =
          static_cast<uint8_t>(0xA0 | targets[targetIndex]);
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
      TEST_ASSERT_TRUE(startClear(rtc, targets[targetIndex]).inProgress());
      uint8_t used = 0;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(1, used);
      fake.direct[RV3032::cmd::REG_TEMP_LSB] |= targets[neighborIndex];
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).ok());
      TEST_ASSERT_EQUAL_UINT8(1, used);
      TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
      TEST_ASSERT_EQUAL_UINT8(1, fake.log[fake.logCount - 1U].length);
      TEST_ASSERT_EQUAL_HEX8(payloads[targetIndex],
                             fake.log[fake.logCount - 1U].data[0]);
      TEST_ASSERT_EQUAL_HEX8(
          0, fake.direct[RV3032::cmd::REG_TEMP_LSB] & targets[targetIndex]);
      TEST_ASSERT_NOT_EQUAL(
          0, fake.direct[RV3032::cmd::REG_TEMP_LSB] & targets[neighborIndex]);
      TEST_ASSERT_EQUAL_HEX8(
          0xA0, fake.direct[RV3032::cmd::REG_TEMP_LSB] & 0xF0);
    }

    FakeRv3032 allNeighbors;
    allNeighbors.direct[RV3032::cmd::REG_TEMP_LSB] = targets[targetIndex];
    RV3032::RV3032 allRtc;
    TEST_ASSERT_TRUE(allRtc.begin(allNeighbors.config()).ok());
    TEST_ASSERT_TRUE(startClear(allRtc, targets[targetIndex]).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(allRtc.pollJob(allNeighbors.nowMs, 1, used).inProgress());
    allNeighbors.direct[RV3032::cmd::REG_TEMP_LSB] |=
        RV3032::cmd::TEMP_LSB_W0C_MASK;
    TEST_ASSERT_TRUE(allRtc.pollJob(allNeighbors.nowMs, 1, used).ok());
    TEST_ASSERT_EQUAL_HEX8(
        static_cast<uint8_t>(RV3032::cmd::TEMP_LSB_W0C_MASK &
                             ~targets[targetIndex]),
        static_cast<uint8_t>(allNeighbors.direct[RV3032::cmd::REG_TEMP_LSB] &
                             RV3032::cmd::TEMP_LSB_W0C_MASK));

    FakeRv3032 clear;
    clear.direct[RV3032::cmd::REG_TEMP_LSB] =
        static_cast<uint8_t>(RV3032::cmd::TEMP_LSB_W0C_MASK &
                             ~targets[targetIndex]);
    RV3032::RV3032 clearRtc;
    TEST_ASSERT_TRUE(clearRtc.begin(clear.config()).ok());
    TEST_ASSERT_TRUE(startClear(clearRtc, targets[targetIndex]).inProgress());
    TEST_ASSERT_TRUE(clearRtc.pollJob(clear.nowMs, 2, used).ok());
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(1, clear.callbackCount);

    FakeRv3032 busy;
    busy.direct[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(
        targets[targetIndex] | RV3032::cmd::EEPROM_BUSY_MASK);
    RV3032::RV3032 busyRtc;
    TEST_ASSERT_TRUE(busyRtc.begin(busy.config()).ok());
    TEST_ASSERT_TRUE(startClear(busyRtc, targets[targetIndex]).inProgress());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(busyRtc.pollJob(busy.nowMs, 2, used).code));
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(1, busy.callbackCount);
  }
}

void test_static_calendar_and_status_utility_coverage() {
  TEST_ASSERT_EQUAL_UINT8(1,
      computedWeekday(2026, 7, 13));
  uint8_t unchangedWeekday = 0xA5;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(RV3032::RV3032::computeWeekday(
          2023, 2, 29, unchangedWeekday).code));
  TEST_ASSERT_EQUAL_HEX8(0xA5, unchangedWeekday);

  RV3032::DateTime leap{2024, 2, 29, 23, 59, 58, 0};
  uint32_t unixTime = 0;
  TEST_ASSERT_TRUE(RV3032::RV3032::dateTimeToUnix(leap, unixTime).ok());
  RV3032::DateTime decoded{};
  TEST_ASSERT_TRUE(RV3032::RV3032::unixToDateTime(unixTime, decoded).ok());
  TEST_ASSERT_EQUAL_UINT16(leap.year, decoded.year);
  TEST_ASSERT_EQUAL_UINT8(leap.month, decoded.month);
  TEST_ASSERT_EQUAL_UINT8(leap.day, decoded.day);
  leap.day = 30;
  TEST_ASSERT_FALSE(RV3032::RV3032::isValidDateTime(leap));
  uint32_t unchangedTimestamp = 0x12345678UL;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(RV3032::RV3032::dateTimeToUnix(
          leap, unchangedTimestamp).code));
  TEST_ASSERT_EQUAL_HEX32(0x12345678UL, unchangedTimestamp);

  RV3032::DateTime lower{};
  TEST_ASSERT_TRUE(RV3032::RV3032::unixToDateTime(
      946684800UL, lower).ok());
  TEST_ASSERT_EQUAL_UINT16(2000, lower.year);
  TEST_ASSERT_EQUAL_UINT8(1, lower.month);
  TEST_ASSERT_EQUAL_UINT8(1, lower.day);
  uint32_t lowerTimestamp = 0;
  TEST_ASSERT_TRUE(
      RV3032::RV3032::dateTimeToUnix(lower, lowerTimestamp).ok());
  TEST_ASSERT_EQUAL_UINT32(946684800UL, lowerTimestamp);
  RV3032::DateTime upper{};
  TEST_ASSERT_TRUE(RV3032::RV3032::unixToDateTime(
      4102444799UL, upper).ok());
  TEST_ASSERT_EQUAL_UINT16(2099, upper.year);
  TEST_ASSERT_EQUAL_UINT8(12, upper.month);
  TEST_ASSERT_EQUAL_UINT8(31, upper.day);
  uint32_t upperTimestamp = 0;
  TEST_ASSERT_TRUE(
      RV3032::RV3032::dateTimeToUnix(upper, upperTimestamp).ok());
  TEST_ASSERT_EQUAL_UINT32(4102444799UL, upperTimestamp);
  RV3032::DateTime unchanged{2026, 7, 13, 1, 2, 3, 4};
  const RV3032::DateTime expectedUnchanged = unchanged;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(RV3032::RV3032::unixToDateTime(
          946684799UL, unchanged).code));
  assertDateTimeEquals(expectedUnchanged, unchanged);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
      static_cast<uint8_t>(RV3032::RV3032::unixToDateTime(
          4102444800UL, unchanged).code));
  assertDateTimeEquals(expectedUnchanged, unchanged);

  RV3032::DateTime build{};
  TEST_ASSERT_TRUE(RV3032::RV3032::parseBuildTime(build).ok());

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
  TEST_ASSERT_TRUE(rtc.enableAlarmInterrupt(false).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
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
  TEST_ASSERT_TRUE(rtc.enableAlarmInterrupt(true).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
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

  TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
      RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
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
      static_cast<uint8_t>(rtc.startSetBackupSwitchModeJob(
          static_cast<RV3032::BackupSwitchMode>(0xFF), fake.nowMs).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setTrickleChargeMode(
          static_cast<RV3032::TrickleChargeMode>(0xFF)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(rtc.setTrickleChargeResistance(
          static_cast<RV3032::TrickleChargeResistance>(0xFF)).code));
  TEST_ASSERT_EQUAL_UINT32(beforeInvalidEnums, fake.callbackCount);
}

void test_clkout_factory_defaults_and_persistence_contract() {
  TEST_ASSERT_EQUAL_HEX8(0, RV3032::cmd::PMU_DEFAULT_ON_DELIVERY);
  TEST_ASSERT_EQUAL_HEX8(0, RV3032::cmd::CLKOUT1_DEFAULT_ON_DELIVERY);
  TEST_ASSERT_EQUAL_HEX8(0, RV3032::cmd::CLKOUT2_DEFAULT_ON_DELIVERY);

  RV3032::ClkoutConfig defaults{};
  TEST_ASSERT_TRUE(defaults.enabled);
  TEST_ASSERT_FALSE(defaults.highFrequencyMode);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::ClkoutFrequency::Hz32768),
      static_cast<uint8_t>(defaults.xtalFrequency));
  TEST_ASSERT_EQUAL_UINT16(1, defaults.highFrequencyDivider);

  constexpr uint8_t targetC0 = RV3032::cmd::PMU_NCLKE_MASK;
  constexpr uint8_t targetC2 = 15;
  constexpr uint8_t targetC3 = static_cast<uint8_t>(
      RV3032::cmd::CLKOUT_OS_MASK |
      (static_cast<uint8_t>(RV3032::ClkoutFrequency::Hz1024)
       << RV3032::cmd::CLKOUT_FREQ_SHIFT));
  RV3032::ClkoutConfig requested{};
  requested.enabled = false;
  requested.highFrequencyMode = true;
  requested.xtalFrequency = RV3032::ClkoutFrequency::Hz1024;
  requested.highFrequencyDivider = 16;

  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_DEFAULT_ON_DELIVERY;
    fake.persistent[2] = RV3032::cmd::CLKOUT1_DEFAULT_ON_DELIVERY;
    fake.persistent[3] = RV3032::cmd::CLKOUT2_DEFAULT_ON_DELIVERY;
    fake.resetFromPersistent();
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());

    RV3032::ClkoutConfig observed{};
    TEST_ASSERT_TRUE(rtc.getClkoutConfig(observed).ok());
    TEST_ASSERT_TRUE(observed.enabled);
    TEST_ASSERT_FALSE(observed.highFrequencyMode);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ClkoutFrequency::Hz32768),
        static_cast<uint8_t>(observed.xtalFrequency));
    TEST_ASSERT_EQUAL_UINT16(1, observed.highFrequencyDivider);

    TEST_ASSERT_TRUE(rtc.setClkoutConfig(requested).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::PMU_DEFAULT_ON_DELIVERY,
                           fake.persistent[0]);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::CLKOUT1_DEFAULT_ON_DELIVERY,
                           fake.persistent[2]);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::CLKOUT2_DEFAULT_ON_DELIVERY,
                           fake.persistent[3]);
    TEST_ASSERT_EQUAL_HEX8(targetC0, fake.activeConfig[0]);
    TEST_ASSERT_EQUAL_HEX8(targetC2, fake.activeConfig[2]);
    TEST_ASSERT_EQUAL_HEX8(targetC3, fake.activeConfig[3]);

    fake.dailyRefresh();
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::PMU_DEFAULT_ON_DELIVERY,
                           fake.activeConfig[0]);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::CLKOUT1_DEFAULT_ON_DELIVERY,
                           fake.activeConfig[2]);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::CLKOUT2_DEFAULT_ON_DELIVERY,
                           fake.activeConfig[3]);
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_DEFAULT_ON_DELIVERY;
    fake.resetFromPersistent();
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setClkoutEnabled(false).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());
    TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 4).ok());
    TEST_ASSERT_EQUAL_HEX8(targetC0, fake.persistent[0]);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::CLKOUT1_DEFAULT_ON_DELIVERY,
                           fake.persistent[2]);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::CLKOUT2_DEFAULT_ON_DELIVERY,
                           fake.persistent[3]);
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
  }

  {
    FakeRv3032 fake;
    fake.persistent[0] = RV3032::cmd::PMU_DEFAULT_ON_DELIVERY;
    fake.resetFromPersistent();
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setClkoutConfig(requested).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT8(3, rtc.eepromQueueDepth());
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::PMU_DEFAULT_ON_DELIVERY,
                           fake.persistent[0]);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::CLKOUT1_DEFAULT_ON_DELIVERY,
                           fake.persistent[2]);
    TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::CLKOUT2_DEFAULT_ON_DELIVERY,
                           fake.persistent[3]);

    TEST_ASSERT_TRUE(pollEepromToCompletion(rtc, fake, 4).ok());
    TEST_ASSERT_TRUE(rtc.getEepromStatus().ok());
    TEST_ASSERT_EQUAL_HEX8(targetC0, fake.persistent[0]);
    TEST_ASSERT_EQUAL_HEX8(targetC2, fake.persistent[2]);
    TEST_ASSERT_EQUAL_HEX8(targetC3, fake.persistent[3]);
    TEST_ASSERT_EQUAL_HEX8(targetC0, fake.activeConfig[0]);
    TEST_ASSERT_EQUAL_HEX8(targetC2, fake.activeConfig[2]);
    TEST_ASSERT_EQUAL_HEX8(targetC3, fake.activeConfig[3]);
    TEST_ASSERT_EQUAL_UINT16(3, fake.writeOneAttempts);

    fake.resetFromPersistent();
    RV3032::ClkoutConfig observed{};
    TEST_ASSERT_TRUE(rtc.getClkoutConfig(observed).ok());
    TEST_ASSERT_FALSE(observed.enabled);
    TEST_ASSERT_TRUE(observed.highFrequencyMode);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ClkoutFrequency::Hz1024),
        static_cast<uint8_t>(observed.xtalFrequency));
    TEST_ASSERT_EQUAL_UINT16(16, observed.highFrequencyDivider);
  }
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
  const uint32_t beforeCoherentRead = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.startReadCoherentTemperatureJob(
      fake.nowMs).inProgress());
  RV3032::CoherentTemperatureResult coherent{};
  TEST_ASSERT_TRUE(rtc.getReadCoherentTemperatureJobResult(
      coherent).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
  TEST_ASSERT_TRUE(rtc.getReadCoherentTemperatureJobResult(coherent).ok());
  TEST_ASSERT_EQUAL_UINT32(beforeCoherentRead + 2U, fake.callbackCount);
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
  TEST_ASSERT_BITS_HIGH(
      static_cast<uint8_t>(1u << RV3032::cmd::TS_EVI_RESET_BIT),
      fake.direct[RV3032::cmd::REG_TS_CONTROL]);
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
  const uint32_t beforeCooperativeWrite = fake.callbackCount;
  TEST_ASSERT_TRUE(rtc.writeUserRam(0, writeData, sizeof(writeData)).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
  TEST_ASSERT_EQUAL_UINT32(beforeCooperativeWrite + 2U, fake.callbackCount);
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
      persistentFake.nowMs, 1000).inProgress());
  const RV3032::Status persistentStatus =
      pollJobToCompletion(persistentRtc, persistentFake, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(persistentStatus.code));
  TEST_ASSERT_EQUAL_UINT32(1, persistentFake.physicalAttemptCount);
}

void test_transport_status_domain_and_health_attempt_evidence() {
  const RV3032::Err invalidCodes[] = {
      RV3032::Err::IN_PROGRESS,
      RV3032::Err::BUSY,
      RV3032::Err::INVALID_PARAM,
      static_cast<RV3032::Err>(0xFE)};

  for (RV3032::Err injected : invalidCodes) {
    FakeRv3032 readFake;
    readFake.failOrdinal = 1;
    readFake.failError = injected;
    RV3032::RV3032 readRtc;
    TEST_ASSERT_TRUE(readRtc.begin(readFake.config()).ok());
    TEST_ASSERT_TRUE(readRtc.startRegisterUpdateJob(
        RV3032::cmd::REG_USER_RAM_START, 0xFF, 0x5A).inProgress());
    uint8_t used = 0;
    const RV3032::Status readStatus =
        readRtc.pollJob(readFake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TRANSPORT_CONTRACT_VIOLATION),
        static_cast<uint8_t>(readStatus.code));
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(injected), readStatus.detail);
    TEST_ASSERT_EQUAL_STRING("Transport callback returned invalid status",
                             readStatus.msg);
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_FALSE(readRtc.isJobBusy());
    TEST_ASSERT_EQUAL_UINT32(1, readRtc.totalFailures());
    const uint32_t readCallbacks = readFake.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TRANSPORT_CONTRACT_VIOLATION),
        static_cast<uint8_t>(readRtc.pollJob(readFake.nowMs, 1, used).code));
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(readCallbacks, readFake.callbackCount);

    FakeRv3032 writeFake;
    writeFake.failOrdinal = 2;
    writeFake.failError = injected;
    RV3032::RV3032 writeRtc;
    TEST_ASSERT_TRUE(writeRtc.begin(writeFake.config()).ok());
    TEST_ASSERT_TRUE(writeRtc.startRegisterUpdateJob(
        RV3032::cmd::REG_USER_RAM_START, 0xFF, 0xA5).inProgress());
    const RV3032::Status writeStatus =
        writeRtc.pollJob(writeFake.nowMs, 2, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TRANSPORT_CONTRACT_VIOLATION),
        static_cast<uint8_t>(writeStatus.code));
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(injected),
                            writeStatus.detail);
    TEST_ASSERT_EQUAL_STRING("Transport callback returned invalid status",
                             writeStatus.msg);
    TEST_ASSERT_EQUAL_UINT8(2, used);
    TEST_ASSERT_FALSE(writeRtc.isJobBusy());
    TEST_ASSERT_EQUAL_UINT32(1, writeRtc.totalSuccess());
    TEST_ASSERT_EQUAL_UINT32(1, writeRtc.totalFailures());
  }

  FakeRv3032 persistent;
  persistent.failOrdinal = 1;
  persistent.failError = RV3032::Err::IN_PROGRESS;
  RV3032::RV3032 persistentRtc;
  TEST_ASSERT_TRUE(persistentRtc.begin(persistent.config()).ok());
  TEST_ASSERT_TRUE(persistentRtc.startReadConfigurationEepromJob(
      RV3032::ConfigurationEepromRegister::PMU,
      persistent.nowMs, 1000).inProgress());
  uint8_t used = 0;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::TRANSPORT_CONTRACT_VIOLATION),
      static_cast<uint8_t>(persistentRtc.pollJob(
          persistent.nowMs, 1, used).code));
  TEST_ASSERT_FALSE(persistentRtc.isJobBusy());
  const uint32_t persistentCallbacks = persistent.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::TRANSPORT_CONTRACT_VIOLATION),
      static_cast<uint8_t>(persistentRtc.pollJob(
          persistent.nowMs, 1, used).code));
  TEST_ASSERT_EQUAL_UINT8(0, used);
  TEST_ASSERT_EQUAL_UINT32(persistentCallbacks, persistent.callbackCount);

  FakeRv3032 probe;
  probe.failOrdinal = 1;
  probe.failError = RV3032::Err::BUSY;
  RV3032::RV3032 probeRtc;
  TEST_ASSERT_TRUE(probeRtc.begin(probe.config()).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::TRANSPORT_CONTRACT_VIOLATION),
      static_cast<uint8_t>(probeRtc.probe().code));
  TEST_ASSERT_EQUAL_UINT32(0, probeRtc.totalFailures());
  TEST_ASSERT_TRUE(probeRtc.lastError().ok());

  const uint32_t beforeLocal = probe.callbackCount;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(probeRtc.readRegisters(
          RV3032::cmd::REG_STATUS, nullptr, 1).code));
  TEST_ASSERT_EQUAL_UINT32(beforeLocal, probe.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(0, probeRtc.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
      static_cast<uint8_t>(probeRtc.writeRegisters(
          RV3032::cmd::REG_TLOW_THRESHOLD, nullptr, 1).code));
  TEST_ASSERT_EQUAL_UINT32(beforeLocal, probe.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(0, probeRtc.totalFailures());

  FakeRv3032 recover;
  recover.failOrdinal = 1;
  recover.failError = RV3032::Err::I2C_NACK_ADDR;
  RV3032::RV3032 recoverRtc;
  TEST_ASSERT_TRUE(recoverRtc.begin(recover.config()).ok());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
      static_cast<uint8_t>(recoverRtc.recover().code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
      static_cast<uint8_t>(recoverRtc.lastError().code));
  TEST_ASSERT_EQUAL_UINT32(1, recoverRtc.totalFailures());
}

void test_timed_callback_completion_clipping_no_hook_and_wrap() {
  {
    FakeRv3032 fake;
    fake.setCalendar(2026, 7, 13, 12, 0, 0, 1);
    fake.direct[RV3032::cmd::REG_STATUS] = 0;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs, 3).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(2, fake.log[0].timeoutMs);
  }

  for (uint32_t lateBy = 1; lateBy <= 2; ++lateBy) {
    FakeRv3032 fake;
    fake.setCalendar(2026, 7, 13, 12, 0, 0, 1);
    fake.direct[RV3032::cmd::REG_STATUS] = 0;
    fake.callbackDurationMs = 5;
    fake.lateCallbackOrdinal = 1;
    fake.lateCallbackExtraMs = lateBy;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs, 6).inProgress());
    uint8_t used = 0;
    const RV3032::Status st = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
        static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(5, fake.log[0].timeoutMs);
    TEST_ASSERT_FALSE(rtc.isJobBusy());
    TEST_ASSERT_EQUAL_UINT32(1, rtc.totalFailures());
  }

  {
    FakeRv3032 fake;
    fake.setCalendar(2026, 7, 13, 12, 0, 0, 1);
    fake.direct[RV3032::cmd::REG_STATUS] = 0;
    RV3032::Config config = fake.config();
    config.nowMs = nullptr;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(config).ok());
    TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(100, 10).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(100, 2, used).ok());
    TEST_ASSERT_EQUAL_UINT8(2, used);
    TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(5, fake.log[0].timeoutMs);
    TEST_ASSERT_EQUAL_UINT32(4, fake.log[1].timeoutMs);
  }

  {
    FakeRv3032 fake;
    fake.nowMs = UINT32_MAX - 2U;
    fake.setCalendar(2026, 7, 13, 12, 0, 0, 1);
    fake.direct[RV3032::cmd::REG_STATUS] = 0;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(fake.nowMs, 8).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 2, used).ok());
    TEST_ASSERT_EQUAL_UINT8(2, used);
    TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
  }
}

void test_ordinary_deadline_states_are_exclusive_at_boundary_and_late() {
  enum class DeadlineJob : uint8_t {
    COHERENT_TEMPERATURE,
    TIME_SNAPSHOT,
    VERIFIED_SET,
  };
  struct DeadlineCase {
    DeadlineJob job;
    uint8_t callbackCap;
    uint32_t timeoutMs;
  };
  const DeadlineCase cases[] = {
      {DeadlineJob::COHERENT_TEMPERATURE, 2, 100},
      {DeadlineJob::TIME_SNAPSHOT, 2, 100},
      {DeadlineJob::VERIFIED_SET, 7, 250},
  };
  const RV3032::DateTime requested{2026, 7, 13, 12, 0, 0, 1};

  for (const DeadlineCase& testCase : cases) {
    for (uint8_t stateIndex = 0; stateIndex < testCase.callbackCap;
         ++stateIndex) {
      for (uint32_t lateMs = 0; lateMs <= 1; ++lateMs) {
        FakeRv3032 fake;
        fake.setCalendar(2020, 1, 1, 0, 0, 0, 3);
        fake.direct[RV3032::cmd::REG_STATUS] = 0;
        RV3032::RV3032 rtc;
        TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());

        RV3032::Status admitted = RV3032::Status::Ok();
        switch (testCase.job) {
          case DeadlineJob::COHERENT_TEMPERATURE:
            admitted = rtc.startReadCoherentTemperatureJob(
                0, testCase.timeoutMs);
            break;
          case DeadlineJob::TIME_SNAPSHOT:
            admitted = rtc.startReadTimeSnapshotJob(0, testCase.timeoutMs);
            break;
          case DeadlineJob::VERIFIED_SET:
            admitted = rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(
                requested, 0, testCase.timeoutMs);
            break;
        }
        TEST_ASSERT_TRUE(admitted.inProgress());

        uint8_t used = 0;
        for (uint8_t callback = 0; callback < stateIndex; ++callback) {
          TEST_ASSERT_TRUE(rtc.pollJob(0, 1, used).inProgress());
          TEST_ASSERT_EQUAL_UINT8(1, used);
        }
        const uint32_t callbacksAtBoundary = fake.callbackCount;
        fake.nowMs = testCase.timeoutMs + lateMs;
        const RV3032::Status terminal =
            rtc.pollJob(fake.nowMs, testCase.callbackCap, used);
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                                static_cast<uint8_t>(terminal.code));
        TEST_ASSERT_EQUAL_UINT8(0, used);
        TEST_ASSERT_EQUAL_UINT32(callbacksAtBoundary, fake.callbackCount);
        TEST_ASSERT_FALSE(rtc.isJobBusy());
      }
    }
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startReadTimeSnapshotJob(0, 1).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(
            rtc.startReadCoherentTemperatureJob(0, 1).code));
    TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
  }

  {
    FakeRv3032 fake;
    fake.setCalendar(2026, 7, 13, 12, 0, 0, 1);
    fake.direct[RV3032::cmd::REG_STATUS] = 0;
    RV3032::Config config = fake.config();
    config.nowMs = nullptr;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(config).ok());
    // i2cTimeoutMs=5: max(2*5, 5+2) = 10 ms.
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startReadTimeSnapshotJob(0, 9).code));
    TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
    TEST_ASSERT_TRUE(rtc.startReadTimeSnapshotJob(0, 10).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(0, 2, used).ok());
    TEST_ASSERT_EQUAL_UINT8(2, used);

    const uint32_t beforeTemperature = fake.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(
            rtc.startReadCoherentTemperatureJob(0, 9).code));
    TEST_ASSERT_EQUAL_UINT32(beforeTemperature, fake.callbackCount);
    TEST_ASSERT_TRUE(rtc.startReadCoherentTemperatureJob(0, 10).inProgress());
    TEST_ASSERT_TRUE(rtc.pollJob(0, 2, used).ok());
    TEST_ASSERT_EQUAL_UINT8(2, used);
    TEST_ASSERT_EQUAL_UINT32(beforeTemperature + 2U, fake.callbackCount);
  }
}

void test_phase1_mutating_jobs_do_not_replay_ambiguous_writes() {
  {
    FakeRv3032 fake;
    fake.failOrdinal = 2;
    fake.failWriteAfterCommit = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startRegisterUpdateJob(
        RV3032::cmd::REG_USER_RAM_START, 0xFF, 0x5A).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(rtc.pollJob(0, 2, used).code));
    TEST_ASSERT_EQUAL_UINT8(2, used);
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_USER_RAM_START));
    const uint32_t callbacks = fake.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(rtc.pollJob(0, 2, used).code));
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(callbacks, fake.callbackCount);
  }

  {
    FakeRv3032 fake;
    fake.failOrdinal = 2;
    fake.failWriteAfterCommit = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.setTemperatureReference(0x1234).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(rtc.pollJob(0, 2, used).code));
    TEST_ASSERT_EQUAL_UINT8(2, used);
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_ACTIVE_TREFERENCE0));
    const uint32_t callbacks = fake.callbackCount;
    (void)rtc.pollJob(0, 2, used);
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(callbacks, fake.callbackCount);
  }

  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_TEMP_LSB] =
        RV3032::cmd::TEMP_CLKF_MASK;
    fake.failOrdinal = 2;
    fake.failWriteAfterCommit = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.clearClockOutputFlag().inProgress());
    uint8_t used = 0;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(rtc.pollJob(0, 2, used).code));
    TEST_ASSERT_EQUAL_UINT8(2, used);
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_TEMP_LSB));
    TEST_ASSERT_BITS_LOW(RV3032::cmd::TEMP_CLKF_MASK,
                         fake.direct[RV3032::cmd::REG_TEMP_LSB]);
    const uint32_t callbacks = fake.callbackCount;
    (void)rtc.pollJob(0, 2, used);
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(callbacks, fake.callbackCount);
  }

  {
    FakeRv3032 fake;
    fake.failOrdinal = 2;
    fake.failWriteAfterCommit = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    uint8_t values[16] = {};
    for (uint8_t i = 0; i < sizeof(values); ++i) {
      values[i] = static_cast<uint8_t>(0x80U + i);
    }
    TEST_ASSERT_TRUE(rtc.writeUserRam(0, values, sizeof(values)).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(rtc.pollJob(0, 2, used).code));
    TEST_ASSERT_EQUAL_UINT8(2, used);
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_USER_RAM_START));
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_USER_RAM_END));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(
        values, &fake.direct[RV3032::cmd::REG_USER_RAM_START], sizeof(values));
    const uint32_t callbacks = fake.callbackCount;
    (void)rtc.pollJob(0, 2, used);
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(callbacks, fake.callbackCount);
  }
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

void test_persistent_deadlines_stop_callbacks_and_report_unverified_cleanup() {
  uint8_t used = 0;
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

void test_persistent_whole_deadline_preserves_effective_callback_failure() {
  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config()).ok());
    TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::PMU,
        fake.nowMs, 1000).inProgress());
    fake.failOrdinal = 1;
    fake.failError = RV3032::Err::TRANSPORT_CONTRACT_VIOLATION;
    fake.lateCallbackOrdinal = 1;
    fake.lateCallbackExtraMs = 1000;

    uint8_t used = 0;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TRANSPORT_CONTRACT_VIOLATION),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(1, rtc.totalFailures());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TRANSPORT_CONTRACT_VIOLATION),
        static_cast<uint8_t>(rtc.lastError().code));
    TEST_ASSERT_FALSE(rtc.isJobBusy());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::TRANSPORT_CONTRACT_VIOLATION),
        static_cast<uint8_t>(rtc.pollJob(fake.nowMs, 1, used).code));
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(0.2384f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake).ok());
    TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());
    const uint32_t callbacksBefore = fake.callbackCount;
    const uint32_t failuresBefore = rtc.totalFailures();
    fake.lateCallbackOrdinal = callbacksBefore + 1U;
    fake.lateCallbackExtraMs = 4000;

    uint8_t used = 0;
    const RV3032::Status terminal =
        rtc.pollEeprom(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(callbacksBefore + 1U, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(failuresBefore + 1U, rtc.totalFailures());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
        static_cast<uint8_t>(rtc.lastError().code));
    TEST_ASSERT_FALSE(rtc.isEepromBusy());
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    TEST_ASSERT_EQUAL_UINT32(1, rtc.eepromWriteFailures());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
        static_cast<uint8_t>(
            rtc.pollEeprom(fake.nowMs, 1, used).code));
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(callbacksBefore + 1U, fake.callbackCount);
  }
}

void test_persistent_phase_deadlines_are_exclusive_at_boundary_and_late() {
  enum class DeadlinePoint : uint8_t {
    INITIAL_READY,
    READ_ONE_POLL_FIRST,
    READ_ONE_DATA_FIRST,
    READ_ONE_POLL_SECOND,
    READ_ONE_DATA_SECOND,
    WRITE_ONE_READY,
    CLEANUP_READY,
  };
  const DeadlinePoint points[] = {
      DeadlinePoint::INITIAL_READY,
      DeadlinePoint::READ_ONE_POLL_FIRST,
      DeadlinePoint::READ_ONE_DATA_FIRST,
      DeadlinePoint::READ_ONE_POLL_SECOND,
      DeadlinePoint::READ_ONE_DATA_SECOND,
      DeadlinePoint::WRITE_ONE_READY,
      DeadlinePoint::CLEANUP_READY,
  };

  for (DeadlinePoint point : points) {
    for (uint32_t lateMs = 0; lateMs <= 1; ++lateMs) {
      FakeRv3032 fake;
      const bool writePoint = point == DeadlinePoint::WRITE_ONE_READY;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config(writePoint)).ok());
      if (writePoint) {
        const uint8_t desired = 0x5A;
        TEST_ASSERT_TRUE(rtc.startWriteUserEepromJob(
            0, &desired, 1, fake.nowMs, 1000).inProgress());
      } else {
        if (point == DeadlinePoint::CLEANUP_READY) fake.failOrdinal = 3;
        TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
            RV3032::ConfigurationEepromRegister::PMU,
            fake.nowMs, 1000).inProgress());
      }

      uint32_t phaseStartedAt = 0;
      if (point == DeadlinePoint::INITIAL_READY ||
          point == DeadlinePoint::CLEANUP_READY) {
        TEST_ASSERT_TRUE(advancePersistentJobUntil(
            rtc, fake, PersistentAdvanceTarget::CALLBACK_COUNT, 3));
        phaseStartedAt = fake.nowMs;
      } else if (writePoint) {
        TEST_ASSERT_TRUE(advancePersistentJobUntil(
            rtc, fake, PersistentAdvanceTarget::WRITE_ONE_COUNT, 1));
        phaseStartedAt = fake.nowMs;
      } else {
        const uint32_t readOneTarget =
            (point == DeadlinePoint::READ_ONE_POLL_FIRST ||
             point == DeadlinePoint::READ_ONE_DATA_FIRST) ? 1U : 2U;
        TEST_ASSERT_TRUE(advancePersistentJobUntil(
            rtc, fake, PersistentAdvanceTarget::READ_ONE_COUNT,
            readOneTarget));
        phaseStartedAt = fake.nowMs;

        if (point == DeadlinePoint::READ_ONE_DATA_FIRST ||
            point == DeadlinePoint::READ_ONE_DATA_SECOND) {
          fake.nowMs = phaseStartedAt + 1U;
          uint8_t used = 0;
          TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
          TEST_ASSERT_EQUAL_UINT8(0, used);  // WAIT_READ -> POLL_READ
          const uint32_t beforeReadyRead = fake.callbackCount;
          TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
          TEST_ASSERT_EQUAL_UINT8(1, used);  // POLL_READ -> READ_DATA
          TEST_ASSERT_EQUAL_UINT32(beforeReadyRead + 1U, fake.callbackCount);
        }
      }

      const uint32_t phaseDurationMs =
          point == DeadlinePoint::WRITE_ONE_READY ? 110U :
          (point == DeadlinePoint::INITIAL_READY ||
           point == DeadlinePoint::CLEANUP_READY) ? 250U : 25U;
      fake.nowMs = phaseStartedAt + phaseDurationMs + lateMs;

      uint8_t used = 0;
      if (point == DeadlinePoint::READ_ONE_POLL_FIRST ||
          point == DeadlinePoint::READ_ONE_POLL_SECOND ||
          point == DeadlinePoint::WRITE_ONE_READY) {
        TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
        TEST_ASSERT_EQUAL_UINT8(0, used);  // Wait state selects deadline state.
      }
      const uint32_t callbacksAtBoundary = fake.callbackCount;
      const RV3032::Status boundary = rtc.pollJob(fake.nowMs, 1, used);
      TEST_ASSERT_EQUAL_UINT8(0, used);
      TEST_ASSERT_EQUAL_UINT32(callbacksAtBoundary, fake.callbackCount);
      if (point == DeadlinePoint::CLEANUP_READY) {
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
            static_cast<uint8_t>(boundary.code));
        TEST_ASSERT_FALSE(rtc.isJobBusy());
      } else {
        TEST_ASSERT_TRUE(boundary.inProgress());
        TEST_ASSERT_TRUE(rtc.isJobBusy());
      }
      if (writePoint) {
        TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
        TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
            fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
      }
      rtc.end();
    }
  }
}

void test_dispatched_write_one_crossing_cutoff_enters_cleanup_without_replay() {
  FakeRv3032 baseline;
  RV3032::RV3032 baselineRtc;
  TEST_ASSERT_TRUE(baselineRtc.begin(baseline.config(true)).ok());
  const uint8_t desired = 0x5A;
  TEST_ASSERT_TRUE(baselineRtc.startWriteUserEepromJob(
      0, &desired, 1, baseline.nowMs, 1000).inProgress());
  TEST_ASSERT_FALSE(pollJobToCompletion(baselineRtc, baseline).inProgress());
  const uint32_t writeCommandOrdinal = findEepromCommandOrdinal(
      baseline, RV3032::cmd::EEPROM_CMD_WRITE_ONE);
  TEST_ASSERT_GREATER_THAN_UINT32(1, writeCommandOrdinal);

  for (uint32_t lateMs = 0; lateMs <= 1; ++lateMs) {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.startWriteUserEepromJob(
        0, &desired, 1, fake.nowMs, 1000).inProgress());
    TEST_ASSERT_TRUE(advancePersistentJobUntil(
        rtc, fake, PersistentAdvanceTarget::CALLBACK_COUNT,
        writeCommandOrdinal - 1U));

    // timeout=1000, cleanup reserve=250 + 6*5 + 10 = 290, cutoff=710.
    // Dispatch with 2 ms remaining: the wrapper clips to 1 ms, and the injected
    // completion reaches the exclusive cutoff or finishes one millisecond late.
    fake.nowMs = 708;
    fake.lateCallbackOrdinal = fake.callbackCount + 1U;
    fake.lateCallbackExtraMs = 2U + lateMs;
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(1, used);
    TEST_ASSERT_EQUAL_UINT32(710U + lateMs, fake.nowMs);
    TEST_ASSERT_EQUAL_UINT32(1, fake.log[fake.logCount - 1U].timeoutMs);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
        static_cast<uint8_t>(rtc.lastError().code));
    TEST_ASSERT_EQUAL_UINT32(1, rtc.totalFailures());
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));

    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_FALSE(terminal.ok());
    TEST_ASSERT_FALSE(rtc.isJobBusy());
    TEST_ASSERT_EQUAL_UINT16(1, fake.writeOneAttempts);
    TEST_ASSERT_EQUAL_UINT32(1, countEepromCommands(
        fake, RV3032::cmd::EEPROM_CMD_WRITE_ONE));
    TEST_ASSERT_EQUAL_HEX8(
        desired,
        fake.persistent[RV3032::cmd::USER_EEPROM_START -
                        RV3032::cmd::CONFIG_EEPROM_START]);
    RV3032::UserEepromWriteReport report{};
    TEST_ASSERT_FALSE(rtc.getUserEepromWriteJobResult(report).ok());
    TEST_ASSERT_EQUAL_UINT8(0, report.durablyVerifiedBytes);
    TEST_ASSERT_TRUE(report.cleanupVerified);
  }
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
    TEST_ASSERT_TRUE(rtc.setTrickleChargeMode(
        RV3032::TrickleChargeMode::V3_0).inProgress());
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
  for (uint8_t i = 1; i < sizeof(fake.activeConfig); ++i) {
    fake.persistent[i] = static_cast<uint8_t>(0x10u + i);
  }
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
  uint8_t activeConfigTail[5] = {};
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
  const RV3032::Status retryStatus =
      retryRtc.pollJob(retry.nowMs, 1, used);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
      static_cast<uint8_t>(retryStatus.code));
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_EQUAL_UINT32(1, retry.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(2, retry.physicalAttemptCount);
  TEST_ASSERT_TRUE(retry.log[0].recoveryUsed);
  TEST_ASSERT_EQUAL_UINT8(2, retry.log[0].physicalAttempts);
  TEST_ASSERT_FALSE(retryRtc.isJobBusy());

  FakeRv3032 boundedRetry;
  boundedRetry.setCalendar(2026, 7, 13, 12, 0, 0, 1);
  boundedRetry.direct[RV3032::cmd::REG_STATUS] = 0;
  boundedRetry.readRecoveryRetryOrdinal = 1;
  boundedRetry.readRecoveryDelayMs = 2;
  boundedRetry.callbackDurationMs = 2;
  RV3032::RV3032 boundedRetryRtc;
  TEST_ASSERT_TRUE(boundedRetryRtc.begin(boundedRetry.config()).ok());
  TEST_ASSERT_TRUE(boundedRetryRtc.startReadTimeSnapshotJob(
      boundedRetry.nowMs, 8).inProgress());
  used = 0;
  TEST_ASSERT_TRUE(boundedRetryRtc.pollJob(
      boundedRetry.nowMs, 1, used).inProgress());
  TEST_ASSERT_EQUAL_UINT8(1, used);
  TEST_ASSERT_EQUAL_UINT32(1, boundedRetry.callbackCount);
  TEST_ASSERT_EQUAL_UINT32(2, boundedRetry.physicalAttemptCount);
  TEST_ASSERT_TRUE(boundedRetry.log[0].recoveryUsed);
  TEST_ASSERT_EQUAL_UINT8(2, boundedRetry.log[0].physicalAttempts);
  TEST_ASSERT_TRUE(boundedRetryRtc.isJobBusy());
  boundedRetryRtc.end();
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

void test_phase2_backup_activation_reconciliation_and_admission() {
  const uint8_t disabledModes[] = {
      RV3032::cmd::PMU_BSM_DISABLED,
      RV3032::cmd::PMU_BSM_DISABLED_ALT};
  const RV3032::BackupSwitchMode requestedModes[] = {
      RV3032::BackupSwitchMode::Direct,
      RV3032::BackupSwitchMode::Level};

  for (uint8_t disabled : disabledModes) {
    for (RV3032::BackupSwitchMode requested : requestedModes) {
      FakeRv3032 fake;
      fake.activeConfig[0] = disabled;
      fake.nowMs = requested == RV3032::BackupSwitchMode::Level
          ? UINT32_MAX - 5U : 100U;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
      const uint32_t startMs = fake.nowMs;
      TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
          requested, startMs).inProgress());

      RV3032::ConfigurationJobReport unchanged{};
      unchanged.operationStatus = RV3032::Status::Error(
          RV3032::Err::INVALID_PARAM, "sentinel");
      unchanged.finalState = RV3032::ConfigurationFinalState::UNKNOWN;
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
          static_cast<uint8_t>(
              rtc.getSetBackupSwitchModeJobResult(unchanged).code));
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
          static_cast<uint8_t>(unchanged.operationStatus.code));
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNKNOWN),
          static_cast<uint8_t>(unchanged.finalState));

      for (uint8_t callback = 0; callback < 4; ++callback) {
        uint8_t used = 0;
        TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
        TEST_ASSERT_EQUAL_UINT8(1, used);
      }
      TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
      TEST_ASSERT_EQUAL_UINT32(1,
          countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));

      const uint32_t activationMs =
          requested == RV3032::BackupSwitchMode::Direct ? 2U : 10U;
      uint8_t used = 0;
      fake.nowMs = startMs + activationMs - 1U;
      const uint32_t callbacksBeforeWait = fake.callbackCount;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(0, used);
      TEST_ASSERT_EQUAL_UINT32(callbacksBeforeWait, fake.callbackCount);
      fake.nowMs = startMs + activationMs;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).ok());
      TEST_ASSERT_EQUAL_UINT8(0, used);
      TEST_ASSERT_EQUAL_UINT32(callbacksBeforeWait, fake.callbackCount);

      RV3032::ConfigurationJobReport report{};
      TEST_ASSERT_TRUE(rtc.getSetBackupSwitchModeJobResult(report).ok());
      TEST_ASSERT_TRUE(report.operationStatus.ok());
      TEST_ASSERT_TRUE(report.cleanupStatus.ok());
      TEST_ASSERT_TRUE(report.mutationAttempted);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(
              RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
          static_cast<uint8_t>(report.finalState));
    }
  }

  {
    FakeRv3032 fake;
    fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DIRECT;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_TRUE(rtc.getSetBackupSwitchModeJobResult(report).ok());
    TEST_ASSERT_FALSE(report.mutationAttempted);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
  }

  {
    FakeRv3032 fake;
    fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DISABLED;
    fake.failOrdinal = 3;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(terminal.code));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(
            rtc.getSetBackupSwitchModeJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNCHANGED),
        static_cast<uint8_t>(report.finalState));
    TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
  }

  {
    FakeRv3032 fake;
    fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DISABLED;
    fake.failOrdinal = 3;
    fake.failWriteAfterCommit = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(terminal.code));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(
            rtc.getSetBackupSwitchModeJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
    TEST_ASSERT_TRUE(report.cleanupStatus.ok());
    TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
  }

  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_CONTROL3] = static_cast<uint8_t>(
        1u << RV3032::cmd::CTRL3_BSIE_BIT);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    const uint32_t minimum = 4U * 5U + 10U + 1U;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startSetBackupSwitchModeJob(
            RV3032::BackupSwitchMode::Level, fake.nowMs,
            minimum - 1U).code));
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Level, fake.nowMs, minimum).inProgress());
    rtc.end();
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Level, fake.nowMs,
        RV3032::BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS).inProgress());
    rtc.end();
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startSetBackupSwitchModeJob(
            RV3032::BackupSwitchMode::Level, fake.nowMs,
            RV3032::BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS + 1U).code));
    TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    for (uint8_t value = 1; value <= 8; ++value) {
      TEST_ASSERT_TRUE(rtc.setOffsetPpm(
          static_cast<float>(value) * 0.2384f).inProgress());
      TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    }
    TEST_ASSERT_EQUAL_UINT8(8, rtc.eepromQueueDepth());
    const uint32_t writesBefore = countWritesTo(
        fake, RV3032::cmd::REG_ACTIVE_PMU);
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::QUEUE_FULL),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT32(writesBefore,
        countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
  }
}

void test_phase2_backup_fault_timing_and_evidence_matrix() {
  const uint8_t preserved = static_cast<uint8_t>(
      RV3032::cmd::PMU_NCLKE_MASK | RV3032::cmd::PMU_TCR_MASK | 0x01u);

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    RV3032::ConfigurationJobReport sentinel{};
    sentinel.operationStatus = RV3032::Status::Error(
        RV3032::Err::INVALID_PARAM, "operation sentinel", 11);
    sentinel.cleanupStatus = RV3032::Status::Error(
        RV3032::Err::I2C_BUS, "cleanup sentinel", 22);
    sentinel.finalState = RV3032::ConfigurationFinalState::UNKNOWN;
    sentinel.mutationAttempted = true;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
        static_cast<uint8_t>(
            rtc.getSetBackupSwitchModeJobResult(sentinel).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                            static_cast<uint8_t>(sentinel.operationStatus.code));
    TEST_ASSERT_EQUAL_INT32(11, sentinel.operationStatus.detail);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(sentinel.cleanupStatus.code));
    TEST_ASSERT_EQUAL_INT32(22, sentinel.cleanupStatus.detail);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNKNOWN),
        static_cast<uint8_t>(sentinel.finalState));
    TEST_ASSERT_TRUE(sentinel.mutationAttempted);
  }

  for (uint32_t faultOrdinal = 1; faultOrdinal <= 4; ++faultOrdinal) {
    FakeRv3032 fake;
    fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DISABLED;
    fake.failOrdinal = faultOrdinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    RV3032::ConfigurationJobReport report{};
    const RV3032::Status retrieved =
        rtc.getSetBackupSwitchModeJobResult(report);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(terminal.code),
                            static_cast<uint8_t>(retrieved.code));
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(4, fake.callbackCount);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
    if (faultOrdinal <= 2) {
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                              static_cast<uint8_t>(terminal.code));
      TEST_ASSERT_FALSE(report.mutationAttempted);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNCHANGED),
          static_cast<uint8_t>(report.finalState));
    } else if (faultOrdinal == 3) {
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                              static_cast<uint8_t>(terminal.code));
      TEST_ASSERT_TRUE(report.mutationAttempted);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNCHANGED),
          static_cast<uint8_t>(report.finalState));
      TEST_ASSERT_TRUE(report.cleanupStatus.ok());
      TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
    } else {
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
          static_cast<uint8_t>(terminal.code));
      TEST_ASSERT_TRUE(report.operationStatus.ok());
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                              static_cast<uint8_t>(report.cleanupStatus.code));
      TEST_ASSERT_EQUAL_INT32(4, report.cleanupStatus.detail);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNKNOWN),
          static_cast<uint8_t>(report.finalState));
    }
  }

  {
    FakeRv3032 fake;
    const uint8_t original = preserved;
    const uint8_t target = static_cast<uint8_t>(
        preserved | RV3032::cmd::PMU_BSM_DIRECT);
    fake.activeConfig[0] = original;
    fake.ignoreWriteOrdinal = 3;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_STRING("Backup PMU verification failed", terminal.msg);
    TEST_ASSERT_EQUAL_INT32(
        (static_cast<int32_t>(target) << 8) | original, terminal.detail);
    TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
        static_cast<uint8_t>(
            rtc.getSetBackupSwitchModeJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
        static_cast<uint8_t>(report.operationStatus.code));
    TEST_ASSERT_EQUAL_STRING("Backup PMU verification failed",
                             report.operationStatus.msg);
    TEST_ASSERT_EQUAL_INT32(
        (static_cast<int32_t>(target) << 8) | original,
        report.operationStatus.detail);
    TEST_ASSERT_TRUE(report.cleanupStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNCHANGED),
        static_cast<uint8_t>(report.finalState));
  }

  {
    FakeRv3032 fake;
    fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DISABLED;
    fake.callbackDurationMs = 1;
    fake.nowMs = 100;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    for (uint8_t i = 0; i < 4; ++i) {
      uint8_t used = 0;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(1, used);
    }
    TEST_ASSERT_EQUAL_UINT32(104, fake.nowMs);
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(0, used);
    fake.nowMs = 105;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).ok());
    TEST_ASSERT_EQUAL_UINT8(0, used);
  }

  struct BackupTransition {
    uint8_t originalBsm;
    RV3032::BackupSwitchMode requested;
    uint8_t expectedBsm;
  };
  const BackupTransition transitions[] = {
      {RV3032::cmd::PMU_BSM_DIRECT, RV3032::BackupSwitchMode::Level,
       RV3032::cmd::PMU_BSM_LEVEL},
      {RV3032::cmd::PMU_BSM_LEVEL, RV3032::BackupSwitchMode::Off,
       RV3032::cmd::PMU_BSM_DISABLED},
  };
  for (const BackupTransition& transition : transitions) {
    FakeRv3032 fake;
    fake.activeConfig[0] = static_cast<uint8_t>(
        preserved | transition.originalBsm);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        transition.requested, fake.nowMs).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
    TEST_ASSERT_EQUAL_HEX8(
        static_cast<uint8_t>(preserved | transition.expectedBsm),
        fake.activeConfig[0]);
    TEST_ASSERT_EQUAL_HEX8(
        preserved,
        static_cast<uint8_t>(fake.activeConfig[0] &
                             ~RV3032::cmd::PMU_BSM_MASK));
  }

  const BackupTransition alreadyRequested[] = {
      {RV3032::cmd::PMU_BSM_DISABLED, RV3032::BackupSwitchMode::Off,
       RV3032::cmd::PMU_BSM_DISABLED},
      {RV3032::cmd::PMU_BSM_DISABLED_ALT, RV3032::BackupSwitchMode::Off,
       RV3032::cmd::PMU_BSM_DISABLED_ALT},
      {RV3032::cmd::PMU_BSM_LEVEL, RV3032::BackupSwitchMode::Level,
       RV3032::cmd::PMU_BSM_LEVEL},
  };
  for (const BackupTransition& transition : alreadyRequested) {
    FakeRv3032 fake;
    fake.activeConfig[0] = static_cast<uint8_t>(
        preserved | transition.originalBsm);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        transition.requested, fake.nowMs).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(
        0, countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
    TEST_ASSERT_EQUAL_HEX8(
        static_cast<uint8_t>(preserved | transition.expectedBsm),
        fake.activeConfig[0]);
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_TRUE(rtc.getSetBackupSwitchModeJobResult(report).ok());
    TEST_ASSERT_FALSE(report.mutationAttempted);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
  }

  {
    FakeRv3032 fake;
    fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DISABLED;
    fake.failOrdinal = 3;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
  }

  {
    FakeRv3032 fake;
    fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DISABLED;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Level, fake.nowMs).inProgress());
    for (uint8_t i = 0; i < 4; ++i) {
      uint8_t used = 0;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    }
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    fake.nowMs = 9;
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    fake.nowMs = 10;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).ok());
    TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());
  }

  {
    FakeRv3032 fake;
    fake.nowMs = 100;
    fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DISABLED;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    constexpr uint32_t timeoutMs = 31;
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Level, fake.nowMs, timeoutMs).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 2, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(2, used);
    fake.nowMs = 116;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
  }

  {
    FakeRv3032 fake;
    fake.nowMs = 100;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs, 31).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 3, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(3, used);
    fake.nowMs = 131;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
  }

  {
    FakeRv3032 fake;
    fake.activeConfig[0] = RV3032::cmd::PMU_BSM_DISABLED;
    fake.lateCallbackOrdinal = 3;
    fake.lateCallbackExtraMs = 6;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    for (uint8_t callback = 0; callback < 4; ++callback) {
      uint8_t used = 0;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(1, used);
    }
    TEST_ASSERT_EQUAL_UINT32(6, fake.nowMs);
    const uint32_t callbacksAtWait = fake.callbackCount;
    fake.nowMs = 7;
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(callbacksAtWait, fake.callbackCount);
    fake.nowMs = 8;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(callbacksAtWait, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
        static_cast<uint8_t>(
            rtc.getSetBackupSwitchModeJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
    TEST_ASSERT_TRUE(report.cleanupStatus.ok());
  }

  {
    FakeRv3032 fake;
    fake.activeConfig[0] = preserved;
    fake.ignoreWriteOrdinal = 3;
    fake.lateCallbackOrdinal = 3;
    fake.lateCallbackExtraMs = 6;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
        static_cast<uint8_t>(
            rtc.getSetBackupSwitchModeJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNCHANGED),
        static_cast<uint8_t>(report.finalState));
    TEST_ASSERT_TRUE(report.cleanupStatus.ok());
  }

  {
    FakeRv3032 fake;
    const uint8_t original = preserved;
    const uint8_t target = static_cast<uint8_t>(
        preserved | RV3032::cmd::PMU_BSM_DIRECT);
    const uint8_t neither = static_cast<uint8_t>(
        preserved | RV3032::cmd::PMU_BSM_DISABLED_ALT);
    fake.activeConfig[0] = original;
    fake.lateCallbackOrdinal = 3;
    fake.lateCallbackExtraMs = 6;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 3, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(3, used);
    fake.activeConfig[0] = neither;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_ACTIVE_PMU));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
        static_cast<uint8_t>(
            rtc.getSetBackupSwitchModeJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                            static_cast<uint8_t>(
                                report.operationStatus.code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
        static_cast<uint8_t>(report.cleanupStatus.code));
    TEST_ASSERT_EQUAL_STRING("Backup PMU reconciliation inconclusive",
                             report.cleanupStatus.msg);
    TEST_ASSERT_EQUAL_INT32(
        (static_cast<int32_t>(target) << 8) | neither,
        report.cleanupStatus.detail);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNKNOWN),
        static_cast<uint8_t>(report.finalState));
  }

  {
    FakeRv3032 fake;
    const uint8_t original = preserved;
    const uint8_t target = static_cast<uint8_t>(
        preserved | RV3032::cmd::PMU_BSM_DIRECT);
    const uint8_t neither = static_cast<uint8_t>(
        preserved | RV3032::cmd::PMU_BSM_DISABLED_ALT);
    fake.activeConfig[0] = original;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs).inProgress());
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 3, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(3, used);
    fake.activeConfig[0] = neither;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_STRING("Backup PMU reconciliation inconclusive",
                             terminal.msg);
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
        static_cast<uint8_t>(
            rtc.getSetBackupSwitchModeJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
        static_cast<uint8_t>(report.operationStatus.code));
    TEST_ASSERT_EQUAL_STRING("Backup PMU verification failed",
                             report.operationStatus.msg);
    TEST_ASSERT_EQUAL_INT32(
        (static_cast<int32_t>(target) << 8) | neither,
        report.operationStatus.detail);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
        static_cast<uint8_t>(report.cleanupStatus.code));
    TEST_ASSERT_EQUAL_STRING("Backup PMU reconciliation inconclusive",
                             report.cleanupStatus.msg);
    TEST_ASSERT_EQUAL_INT32(
        (static_cast<int32_t>(target) << 8) | neither,
        report.cleanupStatus.detail);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNKNOWN),
        static_cast<uint8_t>(report.finalState));
  }

  {
    FakeRv3032 fake;
    RV3032::Config cfg = fake.config(false);
    cfg.i2cTimeoutMs = 100;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(cfg).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startSetBackupSwitchModeJob(
            RV3032::BackupSwitchMode::Direct, fake.nowMs, 402).code));
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Direct, fake.nowMs, 403).inProgress());
    rtc.end();
    TEST_ASSERT_TRUE(rtc.begin(cfg).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
        static_cast<uint8_t>(rtc.startSetBackupSwitchModeJob(
            RV3032::BackupSwitchMode::Level, fake.nowMs, 410).code));
    TEST_ASSERT_TRUE(rtc.startSetBackupSwitchModeJob(
        RV3032::BackupSwitchMode::Level, fake.nowMs, 411).inProgress());
  }
}

enum class Phase2ConfigurationPath : uint8_t {
  TIMER,
  PERIODIC,
  CLKOUT,
  TEMPERATURE,
};

RV3032::Status startPhase2ConfigurationPath(
    Phase2ConfigurationPath path, RV3032::RV3032& rtc) {
  switch (path) {
    case Phase2ConfigurationPath::TIMER:
      return rtc.setTimer(0x123, RV3032::TimerFrequency::Hz64, true);
    case Phase2ConfigurationPath::PERIODIC:
      return rtc.setPeriodicUpdate(
          RV3032::PeriodicUpdateFrequency::MINUTE, true);
    case Phase2ConfigurationPath::CLKOUT: {
      RV3032::ClkoutConfig config{};
      config.enabled = true;
      config.highFrequencyMode = true;
      config.xtalFrequency = RV3032::ClkoutFrequency::Hz1024;
      config.highFrequencyDivider = 16;
      return rtc.setClkoutConfig(config);
    }
    case Phase2ConfigurationPath::TEMPERATURE: {
      RV3032::TemperatureEventConfig config{};
      config.lowThresholdC = -5;
      config.highThresholdC = 40;
      config.lowEventEnabled = true;
      config.highEventEnabled = true;
      config.lowInterruptEnabled = true;
      config.highInterruptEnabled = true;
      return rtc.setTemperatureEventConfig(config);
    }
    default:
      return RV3032::Status::Error(
          RV3032::Err::INVALID_PARAM, "invalid test configuration path");
  }
}

RV3032::Status getPhase2ConfigurationPathResult(
    Phase2ConfigurationPath path, const RV3032::RV3032& rtc,
    RV3032::ConfigurationJobReport& report) {
  switch (path) {
    case Phase2ConfigurationPath::TIMER:
      return rtc.getSetTimerJobResult(report);
    case Phase2ConfigurationPath::PERIODIC:
      return rtc.getSetPeriodicUpdateJobResult(report);
    case Phase2ConfigurationPath::CLKOUT:
      return rtc.getSetClkoutConfigJobResult(report);
    case Phase2ConfigurationPath::TEMPERATURE:
      return rtc.getSetTemperatureEventConfigJobResult(report);
    default:
      return RV3032::Status::Error(
          RV3032::Err::INVALID_PARAM, "invalid test configuration path");
  }
}

uint8_t phase2ConfigurationSuccessCap(Phase2ConfigurationPath path) {
  switch (path) {
    case Phase2ConfigurationPath::TIMER: return 6;
    case Phase2ConfigurationPath::PERIODIC: return 5;
    case Phase2ConfigurationPath::CLKOUT: return 5;
    case Phase2ConfigurationPath::TEMPERATURE: return 7;
    default: return 0;
  }
}

uint8_t phase2ConfigurationWorstCap(Phase2ConfigurationPath path) {
  switch (path) {
    case Phase2ConfigurationPath::TIMER: return 9;
    case Phase2ConfigurationPath::PERIODIC: return 8;
    case Phase2ConfigurationPath::CLKOUT: return 8;
    case Phase2ConfigurationPath::TEMPERATURE: return 10;
    default: return 0;
  }
}

uint8_t phase2ConfigurationFirstWriteOrdinal(
    Phase2ConfigurationPath path) {
  switch (path) {
    case Phase2ConfigurationPath::TIMER: return 3;
    case Phase2ConfigurationPath::PERIODIC: return 2;
    case Phase2ConfigurationPath::CLKOUT: return 3;
    case Phase2ConfigurationPath::TEMPERATURE: return 2;
    default: return 0;
  }
}

uint8_t phase2ConfigurationMismatchWriteOrdinal(
    Phase2ConfigurationPath path) {
  switch (path) {
    case Phase2ConfigurationPath::TIMER: return 4;
    case Phase2ConfigurationPath::PERIODIC: return 3;
    case Phase2ConfigurationPath::CLKOUT: return 3;
    case Phase2ConfigurationPath::TEMPERATURE: return 4;
    default: return 0;
  }
}

size_t phase2ConfigurationForwardWriteOrdinals(
    Phase2ConfigurationPath path, uint8_t* ordinals) {
  switch (path) {
    case Phase2ConfigurationPath::TIMER:
      ordinals[0] = 3;
      ordinals[1] = 4;
      ordinals[2] = 5;
      return 3;
    case Phase2ConfigurationPath::PERIODIC:
      ordinals[0] = 2;
      ordinals[1] = 3;
      ordinals[2] = 4;
      return 3;
    case Phase2ConfigurationPath::CLKOUT:
      ordinals[0] = 3;
      ordinals[1] = 4;
      return 2;
    case Phase2ConfigurationPath::TEMPERATURE:
      ordinals[0] = 2;
      ordinals[1] = 3;
      ordinals[2] = 4;
      ordinals[3] = 5;
      ordinals[4] = 6;
      return 5;
    default:
      TEST_FAIL_MESSAGE("invalid test configuration path");
      return 0;
  }
}

void assertPhase2RequestedWritesNotReplayed(
    Phase2ConfigurationPath path, const FakeRv3032& baseline,
    const FakeRv3032& actual) {
  uint8_t ordinals[5] = {};
  const size_t count = phase2ConfigurationForwardWriteOrdinals(
      path, ordinals);
  const uint8_t safeWriteOrdinal = phase2ConfigurationFirstWriteOrdinal(path);
  for (size_t i = 0; i < count; ++i) {
    const uint8_t ordinal = ordinals[i];
    TEST_ASSERT_LESS_THAN_UINT32(baseline.logCount + 1U, ordinal);
    const test_rv3032::Transfer& expected = baseline.log[ordinal - 1U];
    TEST_ASSERT_TRUE(expected.write);
    const uint32_t maximum = ordinal == safeWriteOrdinal ? 2U : 1U;
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        maximum,
        countWritesMatching(actual, expected.reg,
                            expected.data, expected.length));
  }
}

void test_phase2_staged_fault_ordinals_getters_and_no_replay() {
  const Phase2ConfigurationPath paths[] = {
      Phase2ConfigurationPath::TIMER,
      Phase2ConfigurationPath::PERIODIC,
      Phase2ConfigurationPath::CLKOUT,
      Phase2ConfigurationPath::TEMPERATURE,
  };

  for (Phase2ConfigurationPath path : paths) {
    FakeRv3032 baseline;
    RV3032::RV3032 baselineRtc;
    TEST_ASSERT_TRUE(baselineRtc.begin(baseline.config(false)).ok());
    TEST_ASSERT_TRUE(
        startPhase2ConfigurationPath(path, baselineRtc).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(baselineRtc, baseline, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(
        phase2ConfigurationSuccessCap(path), baseline.callbackCount);

    {
      FakeRv3032 fake;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
      RV3032::ConfigurationJobReport sentinel{};
      sentinel.operationStatus = RV3032::Status::Error(
          RV3032::Err::INVALID_PARAM, "operation sentinel", 101);
      sentinel.cleanupStatus = RV3032::Status::Error(
          RV3032::Err::I2C_BUS, "cleanup sentinel", 202);
      sentinel.finalState = RV3032::ConfigurationFinalState::UNKNOWN;
      sentinel.mutationAttempted = true;
      const RV3032::ConfigurationJobReport before = sentinel;
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
          static_cast<uint8_t>(
              getPhase2ConfigurationPathResult(path, rtc, sentinel).code));
      TEST_ASSERT_EQUAL_MEMORY(&before, &sentinel, sizeof(before));
      TEST_ASSERT_TRUE(startPhase2ConfigurationPath(path, rtc).inProgress());
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
          static_cast<uint8_t>(
              getPhase2ConfigurationPathResult(path, rtc, sentinel).code));
      TEST_ASSERT_EQUAL_MEMORY(&before, &sentinel, sizeof(before));
    }

    const uint8_t successCap = phase2ConfigurationSuccessCap(path);
    const uint8_t worstCap = phase2ConfigurationWorstCap(path);
    const uint8_t firstWrite = phase2ConfigurationFirstWriteOrdinal(path);
    for (uint8_t faultOrdinal = 1; faultOrdinal <= successCap;
         ++faultOrdinal) {
      FakeRv3032 fake;
      fake.failOrdinal = faultOrdinal;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
      TEST_ASSERT_TRUE(startPhase2ConfigurationPath(path, rtc).inProgress());
      const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                              static_cast<uint8_t>(terminal.code));
      TEST_ASSERT_LESS_OR_EQUAL_UINT32(worstCap, fake.callbackCount);
      RV3032::ConfigurationJobReport report{};
      const RV3032::Status retrieved =
          getPhase2ConfigurationPathResult(path, rtc, report);
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(terminal.code),
                              static_cast<uint8_t>(retrieved.code));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                              static_cast<uint8_t>(report.operationStatus.code));
      TEST_ASSERT_EQUAL_INT32(faultOrdinal,
                              report.operationStatus.detail);
      TEST_ASSERT_EQUAL(faultOrdinal >= firstWrite,
                        report.mutationAttempted);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(
              faultOrdinal < firstWrite
                  ? RV3032::ConfigurationFinalState::UNCHANGED
                  : RV3032::ConfigurationFinalState::SAFE_DISABLED_VERIFIED),
          static_cast<uint8_t>(report.finalState));
      TEST_ASSERT_TRUE(report.cleanupStatus.ok());
      assertPhase2RequestedWritesNotReplayed(path, baseline, fake);
    }

    uint8_t forwardWriteOrdinals[5] = {};
    const size_t forwardWriteCount = phase2ConfigurationForwardWriteOrdinals(
        path, forwardWriteOrdinals);
    for (size_t i = 0; i < forwardWriteCount; ++i) {
      const uint8_t faultOrdinal = forwardWriteOrdinals[i];
      for (uint8_t ambiguity = 0; ambiguity < 2; ++ambiguity) {
        FakeRv3032 fake;
        if (ambiguity == 0) {
          fake.failOrdinal = faultOrdinal;
          fake.failWriteAfterCommit = true;
        } else {
          fake.lateCallbackOrdinal = faultOrdinal;
          fake.lateCallbackExtraMs = 6;
        }
        RV3032::RV3032 rtc;
        TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
        TEST_ASSERT_TRUE(startPhase2ConfigurationPath(path, rtc).inProgress());
        const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
        const RV3032::Err expectedError = ambiguity == 0
            ? RV3032::Err::I2C_BUS : RV3032::Err::I2C_TIMEOUT;
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expectedError),
                                static_cast<uint8_t>(terminal.code));
        TEST_ASSERT_LESS_OR_EQUAL_UINT32(
            phase2ConfigurationWorstCap(path), fake.callbackCount);
        const test_rv3032::Transfer& attempted =
            baseline.log[faultOrdinal - 1U];
        TEST_ASSERT_EQUAL_UINT32(
            1, countWritesMatching(fake, attempted.reg,
                                   attempted.data, attempted.length));
        assertPhase2RequestedWritesNotReplayed(path, baseline, fake);
        TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
        RV3032::ConfigurationJobReport report{};
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(expectedError),
            static_cast<uint8_t>(
                getPhase2ConfigurationPathResult(path, rtc, report).code));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expectedError),
                                static_cast<uint8_t>(
                                    report.operationStatus.code));
        TEST_ASSERT_TRUE(report.cleanupStatus.ok());
        TEST_ASSERT_TRUE(report.mutationAttempted);
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(
                RV3032::ConfigurationFinalState::SAFE_DISABLED_VERIFIED),
            static_cast<uint8_t>(report.finalState));
      }
    }
  }
}

void test_phase2_staged_cleanup_fault_matrix_and_caps() {
  const Phase2ConfigurationPath paths[] = {
      Phase2ConfigurationPath::TIMER,
      Phase2ConfigurationPath::PERIODIC,
      Phase2ConfigurationPath::CLKOUT,
      Phase2ConfigurationPath::TEMPERATURE,
  };

  for (Phase2ConfigurationPath path : paths) {
    FakeRv3032 baseline;
    RV3032::RV3032 baselineRtc;
    TEST_ASSERT_TRUE(baselineRtc.begin(baseline.config(false)).ok());
    TEST_ASSERT_TRUE(
        startPhase2ConfigurationPath(path, baselineRtc).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(baselineRtc, baseline, 1).ok());
    const uint8_t successCap = phase2ConfigurationSuccessCap(path);
    const uint8_t worstCap = phase2ConfigurationWorstCap(path);
    const uint8_t mismatchWrite =
        phase2ConfigurationMismatchWriteOrdinal(path);
    for (uint8_t cleanupFault = static_cast<uint8_t>(successCap + 1U);
         cleanupFault <= worstCap; ++cleanupFault) {
      FakeRv3032 fake;
      fake.ignoreWriteOrdinal = mismatchWrite;
      fake.failOrdinal = cleanupFault;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
      TEST_ASSERT_TRUE(startPhase2ConfigurationPath(path, rtc).inProgress());
      const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
          static_cast<uint8_t>(terminal.code));
      TEST_ASSERT_LESS_OR_EQUAL_UINT32(worstCap, fake.callbackCount);
      RV3032::ConfigurationJobReport report{};
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
          static_cast<uint8_t>(
              getPhase2ConfigurationPathResult(path, rtc, report).code));
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
          static_cast<uint8_t>(report.operationStatus.code));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                              static_cast<uint8_t>(report.cleanupStatus.code));
      TEST_ASSERT_EQUAL_INT32(cleanupFault, report.cleanupStatus.detail);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNKNOWN),
          static_cast<uint8_t>(report.finalState));
      assertPhase2RequestedWritesNotReplayed(path, baseline, fake);
    }

    {
      FakeRv3032 fake;
      fake.ignoreWriteOrdinal = mismatchWrite;
      fake.failOrdinal = static_cast<uint32_t>(successCap + 2U);
      fake.failWriteAfterCommit = true;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
      TEST_ASSERT_TRUE(startPhase2ConfigurationPath(path, rtc).inProgress());
      const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
          static_cast<uint8_t>(terminal.code));
      TEST_ASSERT_EQUAL_UINT32(worstCap, fake.callbackCount);
      RV3032::ConfigurationJobReport report{};
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
          static_cast<uint8_t>(
              getPhase2ConfigurationPathResult(path, rtc, report).code));
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                              static_cast<uint8_t>(report.cleanupStatus.code));
      TEST_ASSERT_TRUE(report.mutationAttempted);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(
              RV3032::ConfigurationFinalState::SAFE_DISABLED_VERIFIED),
          static_cast<uint8_t>(report.finalState));
      assertPhase2RequestedWritesNotReplayed(path, baseline, fake);
    }

    {
      FakeRv3032 fake;
      fake.failOrdinal = successCap;
      RV3032::RV3032 rtc;
      TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
      TEST_ASSERT_TRUE(startPhase2ConfigurationPath(path, rtc).inProgress());
      for (uint8_t i = 0; i < successCap; ++i) {
        uint8_t used = 0;
        TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
        TEST_ASSERT_EQUAL_UINT8(1, used);
      }
      uint8_t used = 0;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(1, used);
      const uint32_t cleanupWriteOrdinal = fake.callbackCount + 1U;
      fake.ignoreWriteOrdinal = cleanupWriteOrdinal;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
      const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 1, used);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
          static_cast<uint8_t>(terminal.code));
      TEST_ASSERT_EQUAL_UINT32(worstCap, fake.callbackCount);
      RV3032::ConfigurationJobReport report{};
      (void)getPhase2ConfigurationPathResult(path, rtc, report);
      TEST_ASSERT_EQUAL_UINT8(
          static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
          static_cast<uint8_t>(report.cleanupStatus.code));
      TEST_ASSERT_EQUAL_STRING(
          "Configuration safe-state verification failed",
          report.cleanupStatus.msg);
      const test_rv3032::Transfer& cleanupWrite =
          fake.log[cleanupWriteOrdinal - 1U];
      const test_rv3032::Transfer& cleanupVerify =
          fake.log[cleanupWriteOrdinal];
      TEST_ASSERT_EQUAL_INT32(
          (static_cast<int32_t>(cleanupWrite.data[0]) << 8) |
              cleanupVerify.data[0],
          report.cleanupStatus.detail);
      assertPhase2RequestedWritesNotReplayed(path, baseline, fake);
    }
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(startPhase2ConfigurationPath(
        Phase2ConfigurationPath::CLKOUT, rtc).inProgress());
    for (uint8_t callback = 0; callback < 4; ++callback) {
      uint8_t used = 0;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    }
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).ok());
    TEST_ASSERT_EQUAL_UINT8(3, rtc.eepromQueueDepth());
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_TRUE(rtc.getSetClkoutConfigJobResult(report).ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
  }

  {
    FakeRv3032 fake;
    fake.ignoreWriteOrdinal = 3;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(startPhase2ConfigurationPath(
        Phase2ConfigurationPath::CLKOUT, rtc).inProgress());
    TEST_ASSERT_FALSE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
  }
}

void test_clkout_verification_reports_last_byte_mismatch_safely() {
  FakeRv3032 fake;
  fake.ignoreWriteOrdinal = 3;

  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());

  RV3032::ClkoutConfig config{};
  config.enabled = true;
  config.highFrequencyMode = true;
  config.xtalFrequency = RV3032::ClkoutFrequency::Hz32768;
  config.highFrequencyDivider = 1;
  TEST_ASSERT_TRUE(rtc.setClkoutConfig(config).inProgress());

  const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
      static_cast<uint8_t>(terminal.code));
  TEST_ASSERT_EQUAL_STRING("CLKOUT verification failed", terminal.msg);
  TEST_ASSERT_EQUAL_INT32(
      static_cast<int32_t>(RV3032::cmd::CLKOUT_OS_MASK) << 8,
      terminal.detail);
  TEST_ASSERT_EQUAL_UINT32(8, fake.callbackCount);

  RV3032::ConfigurationJobReport report{};
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
      static_cast<uint8_t>(rtc.getSetClkoutConfigJobResult(report).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::ConfigurationFinalState::SAFE_DISABLED_VERIFIED),
      static_cast<uint8_t>(report.finalState));
  TEST_ASSERT_EQUAL_INT32(terminal.detail, report.operationStatus.detail);
}

void test_phase4_staged_safe_payloads_preserve_neighbor_bits() {
  const Phase2ConfigurationPath paths[] = {
      Phase2ConfigurationPath::TIMER,
      Phase2ConfigurationPath::PERIODIC,
      Phase2ConfigurationPath::CLKOUT,
      Phase2ConfigurationPath::TEMPERATURE,
  };

  const auto prepareGate = [](
      Phase2ConfigurationPath path, FakeRv3032& fake,
      uint8_t& gateRegister) -> uint8_t {
    switch (path) {
      case Phase2ConfigurationPath::TIMER:
        gateRegister = RV3032::cmd::REG_CONTROL1;
        fake.direct[gateRegister] = RV3032::cmd::CONTROL1_IMPLEMENTED_MASK;
        return static_cast<uint8_t>(
            fake.direct[gateRegister] &
            ~(1u << RV3032::cmd::CTRL1_TE_BIT));
      case Phase2ConfigurationPath::PERIODIC:
        gateRegister = RV3032::cmd::REG_CONTROL2;
        fake.direct[RV3032::cmd::REG_CONTROL1] =
            static_cast<uint8_t>((1u << RV3032::cmd::CTRL1_GP0_BIT) |
                                 (1u << RV3032::cmd::CTRL1_USEL_BIT));
        fake.direct[gateRegister] = 0x6Fu;
        return static_cast<uint8_t>(
            fake.direct[gateRegister] &
            ~(1u << RV3032::cmd::CTRL2_UIE_BIT));
      case Phase2ConfigurationPath::CLKOUT:
        gateRegister = RV3032::cmd::REG_ACTIVE_PMU;
        fake.activeConfig[0] = static_cast<uint8_t>(
            RV3032::cmd::PMU_BSM_LEVEL | RV3032::cmd::PMU_TCR_MASK | 0x01u);
        return static_cast<uint8_t>(
            fake.activeConfig[0] | RV3032::cmd::PMU_NCLKE_MASK);
      case Phase2ConfigurationPath::TEMPERATURE:
        gateRegister = RV3032::cmd::REG_CONTROL3;
        fake.direct[gateRegister] = RV3032::cmd::CONTROL3_IMPLEMENTED_MASK;
        return static_cast<uint8_t>(1u << RV3032::cmd::CTRL3_BSIE_BIT);
      default:
        TEST_FAIL_MESSAGE("invalid staged safe-payload path");
        gateRegister = 0;
        return 0;
    }
  };

  const auto observedGate = [](
      Phase2ConfigurationPath path, const FakeRv3032& fake,
      uint8_t gateRegister) -> uint8_t {
    return path == Phase2ConfigurationPath::CLKOUT
        ? static_cast<uint8_t>(fake.activeConfig[0] &
                               RV3032::cmd::PMU_IMPLEMENTED_MASK)
        : fake.direct[gateRegister];
  };

  for (Phase2ConfigurationPath path : paths) {
    FakeRv3032 fake;
    uint8_t gateRegister = 0;
    const uint8_t safeValue = prepareGate(path, fake, gateRegister);
    const uint8_t firstWrite = phase2ConfigurationFirstWriteOrdinal(path);
    fake.failOrdinal = static_cast<uint32_t>(firstWrite + 1U);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(startPhase2ConfigurationPath(path, rtc).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT32(firstWrite + 2U, fake.callbackCount);
    TEST_ASSERT_EQUAL_HEX8(safeValue,
                           fake.log[firstWrite - 1U].data[0]);
    TEST_ASSERT_EQUAL_HEX8(safeValue,
                           observedGate(path, fake, gateRegister));
    const uint32_t expectedGateWrites =
        path == Phase2ConfigurationPath::CLKOUT ? 2U : 1U;
    TEST_ASSERT_EQUAL_UINT32(expectedGateWrites,
                             countWritesTo(fake, gateRegister));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(
            getPhase2ConfigurationPathResult(path, rtc, report).code));
    TEST_ASSERT_TRUE(report.cleanupStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::SAFE_DISABLED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
  }

  for (Phase2ConfigurationPath path : paths) {
    FakeRv3032 fake;
    uint8_t gateRegister = 0;
    const uint8_t safeValue = prepareGate(path, fake, gateRegister);
    fake.failOrdinal = phase2ConfigurationSuccessCap(path);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(startPhase2ConfigurationPath(path, rtc).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT32(phase2ConfigurationWorstCap(path),
                             fake.callbackCount);
    TEST_ASSERT_EQUAL_HEX8(safeValue,
                           observedGate(path, fake, gateRegister));
    const uint32_t expectedGateWrites =
        path == Phase2ConfigurationPath::TIMER ? 3U :
        path == Phase2ConfigurationPath::PERIODIC ? 3U :
        path == Phase2ConfigurationPath::CLKOUT ? 3U : 4U;
    TEST_ASSERT_EQUAL_UINT32(expectedGateWrites,
                             countWritesTo(fake, gateRegister));
    const uint8_t expectedSingleByteSafeWrites =
        path == Phase2ConfigurationPath::CLKOUT ? 1U : 2U;
    TEST_ASSERT_EQUAL_UINT32(
        expectedSingleByteSafeWrites,
        countWritesMatching(fake, gateRegister, &safeValue, 1));
    const test_rv3032::Transfer* lastGateWrite = nullptr;
    for (size_t i = 0; i < fake.logCount; ++i) {
      if (fake.log[i].write && fake.log[i].reg == gateRegister) {
        lastGateWrite = &fake.log[i];
      }
    }
    TEST_ASSERT_NOT_NULL(lastGateWrite);
    TEST_ASSERT_EQUAL_UINT8(1, lastGateWrite->length);
    TEST_ASSERT_EQUAL_HEX8(safeValue, lastGateWrite->data[0]);
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(
            getPhase2ConfigurationPathResult(path, rtc, report).code));
    TEST_ASSERT_TRUE(report.cleanupStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::SAFE_DISABLED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
  }
}

void test_phase2_configuration_reports_caps_and_safe_cleanup() {
  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    RV3032::ConfigurationJobReport sentinel{};
    sentinel.finalState = RV3032::ConfigurationFinalState::UNKNOWN;
    TEST_ASSERT_TRUE(rtc.setTimer(
        0x123, RV3032::TimerFrequency::Hz64, true).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
        static_cast<uint8_t>(rtc.getSetTimerJobResult(sentinel).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNKNOWN),
        static_cast<uint8_t>(sentinel.finalState));
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(6, fake.callbackCount);
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_TRUE(rtc.getSetTimerJobResult(report).ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
    TEST_ASSERT_TRUE(report.mutationAttempted);
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.setPeriodicUpdate(
        RV3032::PeriodicUpdateFrequency::MINUTE, true).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(5, fake.callbackCount);
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_TRUE(rtc.getSetPeriodicUpdateJobResult(report).ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    RV3032::ClkoutConfig config{};
    config.enabled = true;
    config.highFrequencyMode = true;
    config.xtalFrequency = RV3032::ClkoutFrequency::Hz1024;
    config.highFrequencyDivider = 16;
    TEST_ASSERT_TRUE(rtc.setClkoutConfig(config).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(5, fake.callbackCount);
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_TRUE(rtc.getSetClkoutConfigJobResult(report).ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    RV3032::TemperatureEventConfig config{};
    config.lowThresholdC = -5;
    config.highThresholdC = 40;
    config.lowEventEnabled = true;
    config.highEventEnabled = true;
    config.lowInterruptEnabled = true;
    config.highInterruptEnabled = true;
    TEST_ASSERT_TRUE(rtc.setTemperatureEventConfig(config).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(7, fake.callbackCount);
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_TRUE(
        rtc.getSetTemperatureEventConfigJobResult(report).ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::REQUESTED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
  }

  {
    FakeRv3032 fake;
    fake.failOrdinal = 5;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.setTimer(
        0x123, RV3032::TimerFrequency::Hz64, true).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(terminal.code));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(rtc.getSetTimerJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(
            RV3032::ConfigurationFinalState::SAFE_DISABLED_VERIFIED),
        static_cast<uint8_t>(report.finalState));
    TEST_ASSERT_TRUE(report.cleanupStatus.ok());
    TEST_ASSERT_TRUE(report.mutationAttempted);
    TEST_ASSERT_EQUAL_UINT32(6, fake.callbackCount);
    TEST_ASSERT_BITS_LOW(
        static_cast<uint8_t>(1u << RV3032::cmd::CTRL1_TE_BIT),
        fake.direct[RV3032::cmd::REG_CONTROL1]);
  }

  {
    FakeRv3032 fake;
    fake.failOrdinal = 6;
    fake.ignoreWriteOrdinal = 8;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.setTimer(
        0x123, RV3032::TimerFrequency::Hz64, true).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    RV3032::ConfigurationJobReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::CONFIGURATION_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.getSetTimerJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(report.operationStatus.code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::REGISTER_WRITE_FAILED),
        static_cast<uint8_t>(report.cleanupStatus.code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::ConfigurationFinalState::UNKNOWN),
        static_cast<uint8_t>(report.finalState));
    TEST_ASSERT_EQUAL_UINT32(9, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_TIMER_LOW));
  }
}

void test_phase2_quiescence_uie_and_tick_contracts() {
  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_CONTROL2] = static_cast<uint8_t>(
        1u << RV3032::cmd::CTRL2_TIE_BIT);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.setTimer(
        1, RV3032::TimerFrequency::Hz1, true).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_CONTROL1));
  }

  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_CONTROL2] = static_cast<uint8_t>(
        1u << RV3032::cmd::CTRL2_AIE_BIT);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.setAlarmTime(1, 2, 3).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_ALARM_MINUTE));
  }

  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_CONTROL2] = static_cast<uint8_t>(
        1u << RV3032::cmd::CTRL2_EIE_BIT);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.setEviEdge(true).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_TRUE(rtc.setEviDebounce(RV3032::EviDebounce::Hz64).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_TRUE(rtc.setEviOverwrite(true).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_TRUE(rtc.setEventSynchronizationEnabled(true).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_TRUE(rtc.resetTimestamp(
        RV3032::TimestampSource::Evi).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_EQUAL_UINT32(5, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_EVI_CONTROL));
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_TS_CONTROL));

    TEST_ASSERT_TRUE(rtc.setClkoutStopDelayEnabled(true).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(7, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_EVI_CONTROL));
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    fake.direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
        fake.direct[RV3032::cmd::REG_STATUS] |
        (1u << RV3032::cmd::STATUS_UF_BIT));
    TEST_ASSERT_TRUE(rtc.setPeriodicUpdate(
        RV3032::PeriodicUpdateFrequency::SECOND, false).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(5, fake.callbackCount);
    bool updateEventEnabled = true;
    RV3032::PeriodicUpdateFrequency frequency =
        RV3032::PeriodicUpdateFrequency::MINUTE;
    TEST_ASSERT_TRUE(rtc.getPeriodicUpdate(
        frequency, updateEventEnabled).ok());
    TEST_ASSERT_FALSE(updateEventEnabled);
    bool flag = false;
    TEST_ASSERT_TRUE(rtc.getPeriodicUpdateFlag(flag).ok());
    TEST_ASSERT_TRUE(flag);
    TEST_ASSERT_TRUE(rtc.clearPeriodicUpdateFlag().inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    fake.triggerPeriodicUpdateEvent();
    TEST_ASSERT_BITS_LOW(
        static_cast<uint8_t>(1u << RV3032::cmd::STATUS_UF_BIT),
        fake.direct[RV3032::cmd::REG_STATUS]);
    TEST_ASSERT_FALSE(fake.intAsserted);

    const uint32_t beforeEnabled = fake.callbackCount;
    TEST_ASSERT_TRUE(rtc.setPeriodicUpdate(
        RV3032::PeriodicUpdateFrequency::MINUTE, true).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(5, fake.callbackCount - beforeEnabled);
    fake.triggerPeriodicUpdateEvent();
    TEST_ASSERT_BITS_HIGH(
        static_cast<uint8_t>(1u << RV3032::cmd::STATUS_UF_BIT),
        fake.direct[RV3032::cmd::REG_STATUS]);
    TEST_ASSERT_TRUE(fake.intAsserted);
  }

  {
    FakeRv3032 uninitialized;
    RV3032::RV3032 rtc;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::NOT_INITIALIZED),
        static_cast<uint8_t>(rtc.tick(uninitialized.nowMs).code));
    TEST_ASSERT_TRUE(rtc.begin(uninitialized.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setTimer(
        1, RV3032::TimerFrequency::Hz1, false).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(rtc.tick(uninitialized.nowMs).code));
    TEST_ASSERT_EQUAL_UINT32(0, uninitialized.callbackCount);
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, uninitialized, 1).ok());
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(0.2384f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, uninitialized, 1).ok());
    RV3032::Status st = RV3032::Status::Error(
        RV3032::Err::IN_PROGRESS, "tick not started");
    for (uint16_t i = 0; i < 1000 && st.inProgress(); ++i) {
      const uint32_t before = uninitialized.callbackCount;
      st = rtc.tick(uninitialized.nowMs);
      TEST_ASSERT_LESS_OR_EQUAL_UINT32(
          1, uninitialized.callbackCount - before);
      if (st.inProgress()) ++uninitialized.nowMs;
    }
    TEST_ASSERT_TRUE(st.ok());

    TEST_ASSERT_TRUE(rtc.setOffsetPpm(2.0f * 0.2384f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, uninitialized, 1).ok());
    uninitialized.failOrdinal = uninitialized.callbackCount + 1U;
    st = rtc.tick(uninitialized.nowMs);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    const uint32_t callbacksAtFailure = uninitialized.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(rtc.tick(uninitialized.nowMs).code));
    TEST_ASSERT_EQUAL_UINT32(callbacksAtFailure,
                             uninitialized.callbackCount);
  }
}

void test_phase2_quiescence_guard_caps_and_evi_reset_no_replay() {
  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_CONTROL2] = static_cast<uint8_t>(
        1u << RV3032::cmd::CTRL2_AIE_BIT);
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.setAlarmMatch(true, false, true).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_EQUAL_UINT32(1, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_ALARM_MINUTE));
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.setAlarmMatch(true, false, true).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_ALARM_MINUTE));
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.setEviEdge(true).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(1,
        countWritesTo(fake, RV3032::cmd::REG_EVI_CONTROL));
  }

  for (uint32_t faultOrdinal = 0; faultOrdinal <= 4; ++faultOrdinal) {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_TS_CONTROL] = static_cast<uint8_t>(
        1u << RV3032::cmd::TS_EVI_RESET_BIT);
    fake.failOrdinal = faultOrdinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.resetTimestamp(
        RV3032::TimestampSource::Evi).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    if (faultOrdinal == 0) {
      TEST_ASSERT_TRUE(terminal.ok());
      TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
      TEST_ASSERT_EQUAL_UINT32(2,
          countWritesTo(fake, RV3032::cmd::REG_TS_CONTROL));
    } else {
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                              static_cast<uint8_t>(terminal.code));
      TEST_ASSERT_LESS_OR_EQUAL_UINT32(4, fake.callbackCount);
      TEST_ASSERT_EQUAL_UINT32(
          faultOrdinal < 3 ? 0 : faultOrdinal == 3 ? 1 : 2,
          countWritesTo(fake, RV3032::cmd::REG_TS_CONTROL));
    }
  }
}

void test_phase2_persistent_operation_and_cleanup_evidence_are_separate() {
  FakeRv3032 baseline;
  baseline.persistent[1] = 0x5A;
  RV3032::RV3032 baselineRtc;
  TEST_ASSERT_TRUE(baselineRtc.begin(baseline.config(false)).ok());
  TEST_ASSERT_TRUE(baselineRtc.startReadConfigurationEepromJob(
      RV3032::ConfigurationEepromRegister::OFFSET,
      baseline.nowMs, 1000).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(baselineRtc, baseline, 1).ok());
  uint32_t finalControl1Read = 0;
  for (size_t i = 0; i < baseline.logCount; ++i) {
    if (!baseline.log[i].write &&
        baseline.log[i].reg == RV3032::cmd::REG_CONTROL1) {
      finalControl1Read = static_cast<uint32_t>(i + 1U);
    }
  }
  TEST_ASSERT_NOT_EQUAL(0, finalControl1Read);

  {
    FakeRv3032 fake;
    fake.persistent[1] = 0x5A;
    fake.failOrdinal = finalControl1Read;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::OFFSET,
        fake.nowMs, 1000).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    RV3032::PersistentReadResult result{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.getPersistentReadJobResult(result).code));
    TEST_ASSERT_EQUAL_HEX8(0x5A, result.data[0]);
    TEST_ASSERT_EQUAL_UINT8(1, result.length);
    TEST_ASSERT_TRUE(result.persistentVerified);
    TEST_ASSERT_TRUE(result.operationStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(result.cleanupStatus.code));
    TEST_ASSERT_FALSE(result.cleanupVerified);
  }

  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_CONTROL1] =
        RV3032::cmd::CONTROL1_EERD_MASK;
    fake.failOrdinal = 2;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::OFFSET,
        fake.nowMs, 1000).inProgress());
    const uint8_t expectedUsed[] = {1, 0, 1, 1, 0};
    for (uint8_t expected : expectedUsed) {
      uint8_t used = 0;
      TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(expected, used);
    }
    TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
    fake.nowMs = 999;
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollJob(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
    fake.nowMs = 1000;
    const RV3032::Status terminal = rtc.pollJob(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_CONTROL1));
    RV3032::PersistentReadResult result{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.getPersistentReadJobResult(result).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(
                                result.operationStatus.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(
                                result.cleanupStatus.code));
    TEST_ASSERT_FALSE(result.cleanupVerified);
  }

  {
    FakeRv3032 writeBaseline;
    RV3032::RV3032 writeBaselineRtc;
    const uint8_t desired = 0x66;
    TEST_ASSERT_TRUE(writeBaselineRtc.begin(
        writeBaseline.config(true)).ok());
    TEST_ASSERT_TRUE(writeBaselineRtc.startWriteUserEepromJob(
        0, &desired, 1, writeBaseline.nowMs, 1000).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(
        writeBaselineRtc, writeBaseline, 1).ok());
    uint32_t finalWriteControl1Read = 0;
    for (size_t i = 0; i < writeBaseline.logCount; ++i) {
      if (!writeBaseline.log[i].write &&
          writeBaseline.log[i].reg == RV3032::cmd::REG_CONTROL1) {
        finalWriteControl1Read = static_cast<uint32_t>(i + 1U);
      }
    }
    TEST_ASSERT_NOT_EQUAL(0, finalWriteControl1Read);

    FakeRv3032 fake;
    fake.failOrdinal = finalWriteControl1Read;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.startWriteUserEepromJob(
        0, &desired, 1, fake.nowMs, 1000).inProgress());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    RV3032::UserEepromWriteReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.getUserEepromWriteJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(1, report.completedBytes);
    TEST_ASSERT_EQUAL_UINT8(1, report.durablyVerifiedBytes);
    TEST_ASSERT_TRUE(report.operationStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(report.cleanupStatus.code));
    TEST_ASSERT_FALSE(report.cleanupVerified);
  }
}

void test_phase2_typed_persistence_mixed_failure_and_getter_matrix() {
  FakeRv3032 readBaseline;
  readBaseline.persistent[1] = 0x5A;
  RV3032::RV3032 readBaselineRtc;
  TEST_ASSERT_TRUE(readBaselineRtc.begin(readBaseline.config(false)).ok());
  TEST_ASSERT_TRUE(readBaselineRtc.startReadConfigurationEepromJob(
      RV3032::ConfigurationEepromRegister::OFFSET,
      readBaseline.nowMs, 1000).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(
      readBaselineRtc, readBaseline, 1).ok());
  const uint32_t proofOrdinal =
      findPersistentProofOrdinal(readBaseline, 2);
  const uint32_t cleanupWriteOrdinal =
      findLastWriteOrdinal(readBaseline, RV3032::cmd::REG_CONTROL1);
  TEST_ASSERT_NOT_EQUAL(0, proofOrdinal);
  TEST_ASSERT_NOT_EQUAL(0, cleanupWriteOrdinal);

  {
    FakeRv3032 fake;
    fake.persistent[1] = 0x5A;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    RV3032::PersistentReadResult sentinel{};
    sentinel.eepromAddress = 0xA5;
    sentinel.data[0] = 0x5A;
    sentinel.length = 7;
    sentinel.operationStatus = RV3032::Status::Error(
        RV3032::Err::INVALID_PARAM, "operation sentinel", 33);
    sentinel.cleanupStatus = RV3032::Status::Error(
        RV3032::Err::I2C_BUS, "cleanup sentinel", 44);
    sentinel.persistentVerified = true;
    sentinel.cleanupVerified = true;
    const RV3032::PersistentReadResult before = sentinel;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
        static_cast<uint8_t>(rtc.getPersistentReadJobResult(sentinel).code));
    TEST_ASSERT_EQUAL_MEMORY(&before, &sentinel, sizeof(before));
    TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::OFFSET,
        fake.nowMs, 1000).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
        static_cast<uint8_t>(rtc.getPersistentReadJobResult(sentinel).code));
    TEST_ASSERT_EQUAL_MEMORY(&before, &sentinel, sizeof(before));
  }

  {
    FakeRv3032 fake;
    fake.persistent[1] = 0x5A;
    fake.failOrdinal = proofOrdinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::OFFSET,
        fake.nowMs, 1000).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(terminal.code));
    RV3032::PersistentReadResult result{};
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(rtc.getPersistentReadJobResult(result).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(result.operationStatus.code));
    TEST_ASSERT_EQUAL_INT32(proofOrdinal, result.operationStatus.detail);
    TEST_ASSERT_EQUAL_STRING("injected callback failure",
                             result.operationStatus.msg);
    TEST_ASSERT_TRUE(result.cleanupStatus.ok());
    TEST_ASSERT_FALSE(result.persistentVerified);
    TEST_ASSERT_TRUE(result.cleanupVerified);
  }

  {
    FakeRv3032 fake;
    fake.persistent[1] = 0x5A;
    fake.failOrdinal = cleanupWriteOrdinal;
    fake.failWriteAfterCommit = true;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::OFFSET,
        fake.nowMs, 1000).inProgress());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    RV3032::PersistentReadResult result{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.getPersistentReadJobResult(result).code));
    TEST_ASSERT_TRUE(result.operationStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(result.cleanupStatus.code));
    TEST_ASSERT_EQUAL_INT32(cleanupWriteOrdinal,
                            result.cleanupStatus.detail);
    TEST_ASSERT_EQUAL_STRING("injected callback failure",
                             result.cleanupStatus.msg);
    TEST_ASSERT_TRUE(result.persistentVerified);
    TEST_ASSERT_TRUE(result.cleanupVerified);
  }

  const uint8_t desired = 0x66;
  FakeRv3032 writeBaseline;
  RV3032::RV3032 writeBaselineRtc;
  TEST_ASSERT_TRUE(writeBaselineRtc.begin(writeBaseline.config(true)).ok());
  TEST_ASSERT_TRUE(writeBaselineRtc.startWriteUserEepromJob(
      0, &desired, 1, writeBaseline.nowMs, 1000).inProgress());
  TEST_ASSERT_TRUE(pollJobToCompletion(
      writeBaselineRtc, writeBaseline, 1).ok());
  const uint32_t addressWriteOrdinal =
      findWriteOrdinal(writeBaseline, RV3032::cmd::REG_EE_ADDRESS);
  TEST_ASSERT_NOT_EQUAL(0, addressWriteOrdinal);

  FakeRv3032 operationProbe;
  operationProbe.ignoreWriteOrdinal = addressWriteOrdinal;
  RV3032::RV3032 operationProbeRtc;
  TEST_ASSERT_TRUE(operationProbeRtc.begin(operationProbe.config(true)).ok());
  TEST_ASSERT_TRUE(operationProbeRtc.startWriteUserEepromJob(
      0, &desired, 1, operationProbe.nowMs, 1000).inProgress());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(pollJobToCompletion(
          operationProbeRtc, operationProbe, 1).code));
  const uint32_t combinedCleanupRead =
      findLastReadOrdinal(operationProbe, RV3032::cmd::REG_CONTROL1);
  TEST_ASSERT_NOT_EQUAL(0, combinedCleanupRead);

  {
    FakeRv3032 fake;
    fake.ignoreWriteOrdinal = addressWriteOrdinal;
    fake.failOrdinal = combinedCleanupRead;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.startWriteUserEepromJob(
        0, &desired, 1, fake.nowMs, 1000).inProgress());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(pollJobToCompletion(rtc, fake, 1).code));
    RV3032::UserEepromWriteReport report{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.getUserEepromWriteJobResult(report).code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
        static_cast<uint8_t>(report.operationStatus.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(report.cleanupStatus.code));
    TEST_ASSERT_EQUAL_INT32(combinedCleanupRead,
                            report.cleanupStatus.detail);
    TEST_ASSERT_EQUAL_UINT8(0, report.completedBytes);
    TEST_ASSERT_EQUAL_UINT8(0, report.durablyVerifiedBytes);
    TEST_ASSERT_FALSE(report.cleanupVerified);
  }

  {
    FakeRv3032 fake;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    RV3032::UserEepromWriteReport sentinel{};
    sentinel.offset = 7;
    sentinel.requestedLength = 8;
    sentinel.completedBytes = 3;
    sentinel.durablyVerifiedBytes = 2;
    sentinel.operationStatus = RV3032::Status::Error(
        RV3032::Err::INVALID_PARAM, "operation sentinel", 55);
    sentinel.cleanupStatus = RV3032::Status::Error(
        RV3032::Err::I2C_BUS, "cleanup sentinel", 66);
    sentinel.cleanupVerified = true;
    const RV3032::UserEepromWriteReport before = sentinel;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::JOB_RESULT_UNAVAILABLE),
        static_cast<uint8_t>(rtc.getUserEepromWriteJobResult(sentinel).code));
    TEST_ASSERT_EQUAL_MEMORY(&before, &sentinel, sizeof(before));
    TEST_ASSERT_TRUE(rtc.startWriteUserEepromJob(
        0, &desired, 1, fake.nowMs, 1000).inProgress());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
        static_cast<uint8_t>(rtc.getUserEepromWriteJobResult(sentinel).code));
    TEST_ASSERT_EQUAL_MEMORY(&before, &sentinel, sizeof(before));
  }
}

void test_phase2_generic_batch_evidence_precedence_and_reset() {
  auto queueTwoOffsets = [](RV3032::RV3032& rtc, FakeRv3032& fake) {
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(0.238418579f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(2.0f * 0.238418579f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT8(2, rtc.eepromQueueDepth());
  };

  FakeRv3032 baseline;
  RV3032::RV3032 baselineRtc;
  TEST_ASSERT_TRUE(baselineRtc.begin(baseline.config(true)).ok());
  queueTwoOffsets(baselineRtc, baseline);
  TEST_ASSERT_TRUE(pollEepromToCompletion(baselineRtc, baseline, 1).ok());
  const uint32_t forwardFailureOrdinal =
      findPersistentProofOrdinal(baseline, 2);
  const uint32_t addressWriteOrdinal =
      findWriteOrdinal(baseline, RV3032::cmd::REG_EE_ADDRESS);
  TEST_ASSERT_NOT_EQUAL(0, forwardFailureOrdinal);
  TEST_ASSERT_NOT_EQUAL(0, addressWriteOrdinal);

  {
    FakeRv3032 fake;
    fake.failOrdinal = forwardFailureOrdinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    queueTwoOffsets(rtc, fake);
    const RV3032::Status first = pollEepromToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(first.code));
    TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());
    RV3032::SettingsSnapshot during{};
    TEST_ASSERT_TRUE(rtc.getSettings(during).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(during.eepromOperationStatus.code));
    TEST_ASSERT_EQUAL_INT32(forwardFailureOrdinal,
                            during.eepromOperationStatus.detail);
    TEST_ASSERT_TRUE(during.eepromCleanupStatus.ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                            static_cast<uint8_t>(rtc.getEepromStatus().code));

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(pollEepromToCompletion(rtc, fake, 1).code));
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(rtc.getEepromStatus().code));
    RV3032::SettingsSnapshot completed{};
    TEST_ASSERT_TRUE(rtc.getSettings(completed).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(completed.eepromOperationStatus.code));
    TEST_ASSERT_TRUE(completed.eepromCleanupStatus.ok());

    TEST_ASSERT_TRUE(rtc.setOffsetPpm(
        3.0f * 0.238418579f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    RV3032::SettingsSnapshot reset{};
    TEST_ASSERT_TRUE(rtc.getSettings(reset).ok());
    TEST_ASSERT_TRUE(reset.eepromOperationStatus.ok());
    TEST_ASSERT_TRUE(reset.eepromCleanupStatus.ok());
  }

  FakeRv3032 operationProbe;
  operationProbe.ignoreWriteOrdinal = addressWriteOrdinal;
  RV3032::RV3032 operationProbeRtc;
  TEST_ASSERT_TRUE(operationProbeRtc.begin(operationProbe.config(true)).ok());
  queueTwoOffsets(operationProbeRtc, operationProbe);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
      static_cast<uint8_t>(pollEepromToCompletion(
          operationProbeRtc, operationProbe, 1).code));
  const uint32_t cleanupReadOrdinal =
      findLastReadOrdinal(operationProbe, RV3032::cmd::REG_CONTROL1);
  TEST_ASSERT_NOT_EQUAL(0, cleanupReadOrdinal);

  {
    FakeRv3032 fake;
    fake.ignoreWriteOrdinal = addressWriteOrdinal;
    fake.failOrdinal = cleanupReadOrdinal;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    queueTwoOffsets(rtc, fake);
    const RV3032::Status terminal = pollEepromToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    RV3032::SettingsSnapshot report{};
    TEST_ASSERT_TRUE(rtc.getSettings(report).ok());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_VERIFY_FAILED),
        static_cast<uint8_t>(report.eepromOperationStatus.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
        static_cast<uint8_t>(report.eepromCleanupStatus.code));
    TEST_ASSERT_EQUAL_INT32(cleanupReadOrdinal,
                            report.eepromCleanupStatus.detail);
    const uint32_t callbacks = fake.callbackCount;
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.getEepromStatus().code));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.tick(fake.nowMs).code));
    TEST_ASSERT_EQUAL_UINT32(callbacks, fake.callbackCount);

    rtc.end();
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    RV3032::SettingsSnapshot reset{};
    TEST_ASSERT_TRUE(rtc.getSettings(reset).ok());
    TEST_ASSERT_TRUE(reset.eepromOperationStatus.ok());
    TEST_ASSERT_TRUE(reset.eepromCleanupStatus.ok());
  }
}

void test_phase4_late_cleanup_write_requires_semantic_cleanup_failure() {
  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_CONTROL1] =
        RV3032::cmd::CONTROL1_EERD_MASK;
    fake.failOrdinal = 2;
    fake.lateCallbackOrdinal = 4;
    fake.lateCallbackExtraMs = 1000;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(false)).ok());
    TEST_ASSERT_TRUE(rtc.startReadConfigurationEepromJob(
        RV3032::ConfigurationEepromRegister::OFFSET,
        fake.nowMs, 1000).inProgress());
    const RV3032::Status terminal = pollJobToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_CONTROL1));
    RV3032::PersistentReadResult result{};
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.getPersistentReadJobResult(result).code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(
                                result.operationStatus.code));
    TEST_ASSERT_EQUAL_INT32(2, result.operationStatus.detail);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                            static_cast<uint8_t>(
                                result.cleanupStatus.code));
    TEST_ASSERT_FALSE(result.cleanupVerified);
  }

  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_CONTROL1] =
        RV3032::cmd::CONTROL1_EERD_MASK;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(0.238418579f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());

    fake.callbackCount = 0;
    fake.logCount = 0;
    fake.nowMs = 0;
    fake.failOrdinal = 2;
    fake.lateCallbackOrdinal = 4;
    fake.lateCallbackExtraMs = 4000;
    const RV3032::Status terminal = pollEepromToCompletion(rtc, fake, 1);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT32(4, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT32(
        1, countWritesTo(fake, RV3032::cmd::REG_CONTROL1));
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(rtc.getEepromStatus().code));
    RV3032::SettingsSnapshot result{};
    TEST_ASSERT_TRUE(rtc.getSettings(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(
                                result.eepromOperationStatus.code));
    TEST_ASSERT_EQUAL_INT32(2, result.eepromOperationStatus.detail);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                            static_cast<uint8_t>(
                                result.eepromCleanupStatus.code));
  }

  {
    FakeRv3032 fake;
    fake.direct[RV3032::cmd::REG_CONTROL1] =
        RV3032::cmd::CONTROL1_EERD_MASK;
    RV3032::RV3032 rtc;
    TEST_ASSERT_TRUE(rtc.begin(fake.config(true)).ok());
    TEST_ASSERT_TRUE(rtc.setOffsetPpm(0.238418579f).inProgress());
    TEST_ASSERT_TRUE(pollJobToCompletion(rtc, fake, 1).ok());
    TEST_ASSERT_EQUAL_UINT8(1, rtc.eepromQueueDepth());

    fake.callbackCount = 0;
    fake.logCount = 0;
    fake.nowMs = 0;
    fake.failOrdinal = 2;
    const uint8_t expectedUsed[] = {1, 0, 1, 1, 0};
    for (uint8_t expected : expectedUsed) {
      uint8_t used = 0;
      TEST_ASSERT_TRUE(rtc.pollEeprom(fake.nowMs, 1, used).inProgress());
      TEST_ASSERT_EQUAL_UINT8(expected, used);
    }
    TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
    fake.nowMs = 3999;
    uint8_t used = 0;
    TEST_ASSERT_TRUE(rtc.pollEeprom(fake.nowMs, 1, used).inProgress());
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(3, fake.callbackCount);
    fake.nowMs = 4000;
    const RV3032::Status terminal = rtc.pollEeprom(fake.nowMs, 1, used);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(RV3032::Err::EEPROM_CLEANUP_FAILED),
        static_cast<uint8_t>(terminal.code));
    TEST_ASSERT_EQUAL_UINT8(0, used);
    TEST_ASSERT_EQUAL_UINT32(0,
        countWritesTo(fake, RV3032::cmd::REG_CONTROL1));
    TEST_ASSERT_EQUAL_UINT8(0, rtc.eepromQueueDepth());
    RV3032::SettingsSnapshot result{};
    TEST_ASSERT_TRUE(rtc.getSettings(result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(
                                result.eepromOperationStatus.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                            static_cast<uint8_t>(
                                result.eepromCleanupStatus.code));
  }
}

void beginCliHarness(FakeRv3032& fake, bool enableEepromWrites) {
  g_rtc.end();
  g_pendingOperation = PendingOperation{};
  g_verbose = false;
  Serial.resetInput();
  Serial.resetOutput();
  arduinoStubMillis = 0;
  arduinoStubMicros = 0;
  fake.nowMs = 0;
  TEST_ASSERT_TRUE(g_rtc.begin(fake.config(enableEepromWrites)).ok());
}

void driveCliPending(
    FakeRv3032& fake, PendingSurface stopAt = PendingSurface::NONE,
    uint32_t pollCap = 5000U) {
  for (uint32_t poll = 0;
       poll < pollCap &&
       g_pendingOperation.surface != stopAt;
       ++poll) {
    const uint32_t callbacksBefore = fake.callbackCount;
    pollPendingOperation(fake.nowMs);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        1U, fake.callbackCount - callbacksBefore);
    ++fake.nowMs;
    arduinoStubMillis = fake.nowMs;
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(stopAt),
                          static_cast<uint8_t>(g_pendingOperation.surface));
  if (stopAt == PendingSurface::NONE) {
    TEST_ASSERT_FALSE(g_rtc.isJobBusy());
    TEST_ASSERT_FALSE(g_rtc.isEepromBusy());
  }
}

void stageCliEepromQueue(FakeRv3032& fake) {
  RV3032::Status status = g_rtc.setOffsetPpm(1.0f);
  TEST_ASSERT_TRUE(status.inProgress());
  status = pollJobToCompletion(g_rtc, fake, 1);
  TEST_ASSERT_TRUE(status.ok());
  TEST_ASSERT_TRUE(g_rtc.isEepromBusy());
}

void test_phase3_cli_invalid_mutating_commands_are_zero_io() {
  FakeRv3032 fake;
  beginCliHarness(fake, true);

  const char* invalidCommands[] = {
      "set 2026 1 1 0 0 0 trailing",
      "set 2026 2 30 0 0 0",
      "setbuild trailing",
      "unix -1",
      "ts_reset tlow trailing",
      "alarm_set 256 1 1",
      "alarm_match 1 0 2",
      "alarm_int 2",
      "alarm_clear trailing",
      "clkout 2",
      "clkout_freq 4",
      "offset nan",
      "offset 0x1p0",
      "timer 0 2 0",
      "timer 1 2 0 trailing",
      "evi edge 2",
      "evi debounce 4",
      "evi overwrite 2",
      "status_clear 0x0F",
      "reg 0x0D 1",
      "reg 0x0D 1 confirm trailing",
      "ram_write 0 256",
      "backup direct trailing",
      "primary-cell ensure CONFIRM-PRIMARY-CELL trailing",
      "clear_porf trailing",
      "clear_vlf trailing",
      "clear_bsf trailing",
      "verbose 2",
      "stress 0",
      "stress_mix 100001",
  };

  for (const char* command : invalidCommands) {
    const uint32_t callbacksBefore = fake.callbackCount;
    process_command(String(command));
    TEST_ASSERT_EQUAL_UINT32(callbacksBefore, fake.callbackCount);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PendingSurface::NONE),
                            static_cast<uint8_t>(g_pendingOperation.surface));
    TEST_ASSERT_FALSE(g_rtc.isJobBusy());
    TEST_ASSERT_FALSE(g_rtc.isEepromBusy());
  }

  String confirmation[2];
  TEST_ASSERT_TRUE(parseExactTokens(
      String("ensure\t  CONFIRM-PRIMARY-CELL"), confirmation, 2));
  TEST_ASSERT_TRUE(confirmation[0] == "ensure");
  TEST_ASSERT_TRUE(confirmation[1] == "CONFIRM-PRIMARY-CELL");
  process_command(String("verbose\t 1"));
  TEST_ASSERT_TRUE(g_verbose);
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
}

void test_phase3_cli_ram_and_timestamp_terminal_output() {
  FakeRv3032 fake;
  beginCliHarness(fake, false);

  process_command(String("ts tlow"));
  TEST_ASSERT_NOT_EQUAL(
      std::string::npos, Serial.output().find("timestamp: count=0"));
  TEST_ASSERT_NOT_EQUAL(
      std::string::npos, Serial.output().find("Time: empty/unset"));
  TEST_ASSERT_EQUAL(std::string::npos, Serial.output().find("Time: 2000-"));

  Serial.resetOutput();
  process_command(String(
      "ram_write 0 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PendingSurface::ORDINARY_JOB),
                          static_cast<uint8_t>(g_pendingOperation.surface));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output().find("accepted"));
  TEST_ASSERT_EQUAL(std::string::npos,
                    Serial.output().find("terminal status"));
  const uint32_t callbacksBefore = fake.callbackCount;
  driveCliPending(fake);
  TEST_ASSERT_EQUAL_UINT32(2, fake.callbackCount - callbacksBefore);
  TEST_ASSERT_NOT_EQUAL(
      std::string::npos,
      Serial.output().find("user RAM write terminal status: OK"));

  FakeRv3032 failing;
  beginCliHarness(failing, false);
  process_command(String(
      "ram_write 0 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"));
  failing.failOrdinal = failing.callbackCount + 1U;
  driveCliPending(failing);
  TEST_ASSERT_NOT_EQUAL(
      std::string::npos,
      Serial.output().find("user RAM write terminal status: I2C_BUS"));
}

void test_phase3_cli_owner_handoff_is_single_callback_and_preserves_status() {
  FakeRv3032 success;
  beginCliHarness(success, true);
  process_command(String("clkout_freq 3"));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PendingSurface::ORDINARY_JOB),
                          static_cast<uint8_t>(g_pendingOperation.surface));

  driveCliPending(success, PendingSurface::EEPROM, 100U);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PendingSurface::EEPROM),
                          static_cast<uint8_t>(g_pendingOperation.surface));
  TEST_ASSERT_TRUE(g_pendingOperation.ordinaryStatus.ok());

  Serial.inject("status\n");
  const int queuedInput = Serial.available();
  const uint32_t callbacksBeforeBlockedLoop = success.callbackCount;
  loop();
  TEST_ASSERT_EQUAL_INT(queuedInput, Serial.available());
  TEST_ASSERT_LESS_OR_EQUAL_UINT32(
      1U, success.callbackCount - callbacksBeforeBlockedLoop);
  Serial.resetInput();

  driveCliPending(success);
  TEST_ASSERT_NOT_EQUAL(
      std::string::npos,
      Serial.output().find("CLKOUT configuration terminal status: OK"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos,
                        Serial.output().find("persistence: terminal=OK"));

  FakeRv3032 persistentFailure;
  beginCliHarness(persistentFailure, true);
  process_command(String("clkout_freq 3"));
  driveCliPending(persistentFailure, PendingSurface::EEPROM);
  TEST_ASSERT_TRUE(g_pendingOperation.ordinaryStatus.ok());
  persistentFailure.failOrdinal = persistentFailure.callbackCount + 1U;
  driveCliPending(persistentFailure);
  TEST_ASSERT_NOT_EQUAL(
      std::string::npos,
      Serial.output().find("CLKOUT configuration terminal status: OK"));
  TEST_ASSERT_NOT_EQUAL(
      std::string::npos,
      Serial.output().find("persistence: terminal=I2C_BUS"));

  for (bool failPersistence : {false, true}) {
    FakeRv3032 ordinaryFailure;
    beginCliHarness(ordinaryFailure, true);
    stageCliEepromQueue(ordinaryFailure);
    Serial.resetOutput();
    process_command(String("offset 2.0"));
    ordinaryFailure.failOrdinal = ordinaryFailure.callbackCount + 1U;
    driveCliPending(ordinaryFailure, PendingSurface::EEPROM);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                            static_cast<uint8_t>(
                                g_pendingOperation.ordinaryStatus.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PendingSurface::EEPROM),
                            static_cast<uint8_t>(
                                g_pendingOperation.surface));
    if (failPersistence) {
      ordinaryFailure.failOrdinal = ordinaryFailure.callbackCount + 1U;
    }
    driveCliPending(ordinaryFailure);
    TEST_ASSERT_NOT_EQUAL(
        std::string::npos,
        Serial.output().find("frequency offset terminal status: I2C_BUS"));
    TEST_ASSERT_NOT_EQUAL(
        std::string::npos,
        Serial.output().find(failPersistence
                                 ? "persistence: terminal=I2C_BUS"
                                 : "persistence: terminal=OK"));
  }
}

void test_phase3_cli_persistent_helper_deadline_does_not_orphan() {
  g_rtc.end();
  g_pendingOperation = PendingOperation{};
  Serial.resetInput();
  Serial.resetOutput();
  Wire.reset();
  arduinoStubMillis = 0;
  arduinoStubMicros = 0;

  RV3032::Config config;
  config.i2cWrite = transport::wireWrite;
  config.i2cWriteRead = transport::wireWriteRead;
  config.i2cUser = &Wire;
  config.nowMs = rtc_now_ms;
  config.waitMs = rtc_wait_ms;
  TEST_ASSERT_TRUE(g_rtc.begin(config).ok());

  // The callback and blocking helper deliberately share the Arduino clock.
  // The first read crosses both its clipped callback timeout and the exact
  // 1000 ms operation deadline; the helper must force a terminal poll and
  // cannot leave cooperative work behind.
  Wire.requestDurationMs = 1001U;
  uint8_t value = 0xA5;
  const RV3032::Status status = read_user_eeprom_chunk(0, 1, &value);
  TEST_ASSERT_FALSE(status.inProgress());
  TEST_ASSERT_FALSE(status.ok());
  TEST_ASSERT_FALSE(g_rtc.isJobBusy());
  TEST_ASSERT_FALSE(g_rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_HEX8(0xA5, value);
}

void test_phase3_wire_validation_and_closed_status_domain() {
  TwoWire wire;
  uint8_t tx[2] = {0x0D, 0x00};
  uint8_t rx[2] = {};

  struct InvalidWriteCase {
    const uint8_t* data;
    size_t length;
    uint32_t timeoutMs;
    void* user;
    int32_t detail;
  };
  const InvalidWriteCase invalidWrites[] = {
      {nullptr, 1, 5, &wire, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 0, 5, &wire, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 129, 5, &wire, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 1, 5, nullptr, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 1, 0, &wire, transport::I2C_DETAIL_INVALID_TIMEOUT},
      {tx, 1, UINT16_MAX + uint32_t{1}, &wire,
       transport::I2C_DETAIL_INVALID_TIMEOUT},
  };
  for (const InvalidWriteCase& item : invalidWrites) {
    wire.reset();
    const RV3032::Status status = transport::wireWrite(
        0x51, item.data, item.length, item.timeoutMs, item.user);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_ERROR),
                            static_cast<uint8_t>(status.code));
    TEST_ASSERT_EQUAL_INT32(item.detail, status.detail);
    TEST_ASSERT_EQUAL_UINT32(0, wire.beginTransmissionCalls);
  }

  struct InvalidReadCase {
    const uint8_t* tx;
    size_t txLength;
    uint8_t* rx;
    size_t rxLength;
    uint32_t timeoutMs;
    void* user;
    int32_t detail;
  };
  const InvalidReadCase invalidReads[] = {
      {nullptr, 1, rx, 1, 5, &wire, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 0, rx, 1, 5, &wire, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 1, nullptr, 1, 5, &wire, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 1, rx, 0, 5, &wire, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 129, rx, 1, 5, &wire, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 1, rx, 129, 5, &wire, transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 1, rx, 1, 5, nullptr,
       transport::I2C_DETAIL_INVALID_ARGUMENT},
      {tx, 1, rx, 1, 0, &wire, transport::I2C_DETAIL_INVALID_TIMEOUT},
      {tx, 1, rx, 1, UINT16_MAX + uint32_t{1}, &wire,
       transport::I2C_DETAIL_INVALID_TIMEOUT},
  };
  for (const InvalidReadCase& item : invalidReads) {
    wire.reset();
    const RV3032::Status status = transport::wireWriteRead(
        0x51, item.tx, item.txLength, item.rx, item.rxLength,
        item.timeoutMs, item.user);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_ERROR),
                            static_cast<uint8_t>(status.code));
    TEST_ASSERT_EQUAL_INT32(item.detail, status.detail);
    TEST_ASSERT_EQUAL_UINT32(0, wire.beginTransmissionCalls);
  }

  const RV3032::Err expectedWireResults[] = {
      RV3032::Err::OK,
      RV3032::Err::I2C_ERROR,
      RV3032::Err::I2C_NACK_ADDR,
      RV3032::Err::I2C_NACK_DATA,
      RV3032::Err::I2C_BUS,
      RV3032::Err::I2C_TIMEOUT,
      RV3032::Err::I2C_ERROR,
      RV3032::Err::I2C_ERROR,
  };
  for (uint8_t result = 0; result <= 7; ++result) {
    wire.reset();
    arduinoStubMillis = 10;
    wire.queueEndResult(result);
    const RV3032::Status status = transport::wireWrite(
        0x51, tx, sizeof(tx), 5, &wire);
    TEST_ASSERT_TRUE(isAllowedTransportStatus(status.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expectedWireResults[result]),
                            static_cast<uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());
  }

  for (uint8_t result = 1; result <= 7; ++result) {
    wire.reset();
    arduinoStubMillis = 10;
    wire.queueEndResult(result);
    const RV3032::Status status = transport::wireWriteRead(
        0x51, tx, 1, rx, 1, 5, &wire);
    TEST_ASSERT_TRUE(isAllowedTransportStatus(status.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expectedWireResults[result]),
                            static_cast<uint8_t>(status.code));
    TEST_ASSERT_EQUAL_UINT32(2, wire.endTransmissionCalls);
    TEST_ASSERT_FALSE(wire.transactionActive);
    TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());
  }
}

void test_phase3_wire_complete_deadline_timeout_restoration_and_order() {
  TwoWire wire;
  uint8_t tx[2] = {0x0D, 0x00};
  uint8_t rx[2] = {};

  wire.reset();
  arduinoStubMillis = 100;
  wire.endDurationMs = 4;
  RV3032::Status status = transport::wireWrite(
      0x51, tx, sizeof(tx), 5, &wire);
  TEST_ASSERT_TRUE(status.ok());
  TEST_ASSERT_EQUAL_UINT32(104, arduinoStubMillis);
  TEST_ASSERT_EQUAL_UINT32(3, wire.callCount);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TwoWire::Call::BEGIN_TRANSMISSION),
                          static_cast<uint8_t>(wire.calls[0]));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TwoWire::Call::WRITE),
                          static_cast<uint8_t>(wire.calls[1]));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TwoWire::Call::END_WITH_STOP),
                          static_cast<uint8_t>(wire.calls[2]));
  TEST_ASSERT_EQUAL_UINT32(1, wire.endTransmissionCalls);
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(4, wire.effectiveTimeouts[2]);
  TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());

  wire.reset();
  arduinoStubMillis = 100;
  wire.endDurationMs = 5;
  status = transport::wireWrite(
      0x51, tx, sizeof(tx), 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());

  wire.reset();
  arduinoStubMillis = 100;
  wire.endDurationMs = 6;
  status = transport::wireWrite(
      0x51, tx, sizeof(tx), 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());

  wire.reset();
  arduinoStubMillis = UINT32_MAX - 2U;
  wire.endDurationMs = 4;
  status = transport::wireWrite(
      0x51, tx, sizeof(tx), 5, &wire);
  TEST_ASSERT_TRUE(status.ok());
  TEST_ASSERT_EQUAL_UINT32(1, arduinoStubMillis);

  wire.reset();
  arduinoStubMillis = 200;
  wire.beginDurationMs = 1;
  wire.requestDurationMs = 3;
  status = transport::wireWriteRead(
      0x51, tx, 1, rx, 2, 5, &wire);
  TEST_ASSERT_TRUE(status.ok());
  TEST_ASSERT_EQUAL_UINT32(4, wire.callCount);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TwoWire::Call::BEGIN_TRANSMISSION),
                          static_cast<uint8_t>(wire.calls[0]));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TwoWire::Call::WRITE),
                          static_cast<uint8_t>(wire.calls[1]));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TwoWire::Call::END_WITHOUT_STOP),
                          static_cast<uint8_t>(wire.calls[2]));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TwoWire::Call::REQUEST_WITH_STOP),
                          static_cast<uint8_t>(wire.calls[3]));
  TEST_ASSERT_EQUAL_UINT16(4, wire.effectiveTimeouts[0]);
  TEST_ASSERT_EQUAL_UINT16(3, wire.effectiveTimeouts[2]);
  TEST_ASSERT_EQUAL_UINT16(3, wire.effectiveTimeouts[3]);
  TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());

  wire.reset();
  arduinoStubMillis = 200;
  wire.requestDurationMs = 5;
  status = transport::wireWriteRead(
      0x51, tx, 1, rx, 1, 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());

  wire.reset();
  arduinoStubMillis = 200;
  wire.requestDurationMs = 6;
  status = transport::wireWriteRead(
      0x51, tx, 1, rx, 1, 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());
}

void test_phase3_wire_short_stage_release_and_initialization() {
  TwoWire wire;
  uint8_t tx[2] = {0x0D, 0x00};
  uint8_t rx[2] = {};

  wire.reset();
  arduinoStubMillis = 0;
  wire.stagedLengthLimit = 1;
  RV3032::Status status = transport::wireWrite(
      0x51, tx, sizeof(tx), 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_ERROR),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_EQUAL_INT32(transport::I2C_DETAIL_SHORT_STAGING, status.detail);
  TEST_ASSERT_EQUAL_UINT32(1, wire.endTransmissionCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TwoWire::Call::END_WITH_STOP),
                          static_cast<uint8_t>(wire.calls[2]));
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());

  wire.reset();
  arduinoStubMillis = 0;
  wire.setTimeOut(37);
  wire.stagedLengthLimit = 0;
  status = transport::wireWriteRead(
      0x51, tx, 1, rx, 1, 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_ERROR),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_EQUAL_INT32(transport::I2C_DETAIL_SHORT_STAGING, status.detail);
  TEST_ASSERT_EQUAL_UINT32(1, wire.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(1, wire.endTransmissionCalls);
  TEST_ASSERT_EQUAL_UINT32(0, wire.requestCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(TwoWire::Call::END_WITH_STOP),
                          static_cast<uint8_t>(wire.calls[2]));
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(37, wire.getTimeOut());

  wire.stagedLengthLimit = SIZE_MAX;
  status = transport::wireWriteRead(
      0x51, tx, 1, rx, 1, 5, &wire);
  TEST_ASSERT_TRUE(status.ok());
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(37, wire.getTimeOut());

  wire.reset();
  arduinoStubMillis = 0;
  wire.setTimeOut(37);
  wire.requestLength = 1;
  status = transport::wireWriteRead(
      0x51, tx, 1, rx, 2, 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_ERROR),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(37, wire.getTimeOut());

  wire.reset();
  arduinoStubMillis = 0;
  wire.setTimeOut(37);
  wire.availableLength = 0;
  status = transport::wireWriteRead(
      0x51, tx, 1, rx, 1, 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_ERROR),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(37, wire.getTimeOut());

  wire.reset();
  arduinoStubMillis = 0;
  wire.setTimeOut(37);
  wire.readFailureIndex = 0;
  status = transport::wireWriteRead(
      0x51, tx, 1, rx, 1, 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_ERROR),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(37, wire.getTimeOut());

  wire.stagedLengthLimit = SIZE_MAX;
  status = transport::wireWrite(
      0x51, tx, sizeof(tx), 5, &wire);
  TEST_ASSERT_TRUE(status.ok());
  TEST_ASSERT_FALSE(wire.transactionActive);

  wire.reset();
  arduinoStubMillis = 0;
  wire.stagedLengthLimit = 1;
  wire.endDurationMs = 5;
  status = transport::wireWrite(
      0x51, tx, sizeof(tx), 5, &wire);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(status.code));
  TEST_ASSERT_EQUAL_UINT32(1, wire.endTransmissionCalls);
  TEST_ASSERT_FALSE(wire.transactionActive);
  TEST_ASSERT_EQUAL_UINT16(50, wire.getTimeOut());

  Wire.reset();
  Wire.beginResult = false;
  TEST_ASSERT_FALSE(transport::initWire(1, 2, 400000, 37));
  TEST_ASSERT_EQUAL_UINT32(1, Wire.beginCalls);
  TEST_ASSERT_EQUAL_UINT32(0, Wire.setClockCalls);
  TEST_ASSERT_EQUAL_UINT16(50, Wire.getTimeOut());

  Wire.reset();
  Wire.setClockResult = false;
  TEST_ASSERT_FALSE(transport::initWire(1, 2, 400000, 37));
  TEST_ASSERT_EQUAL_UINT32(1, Wire.beginCalls);
  TEST_ASSERT_EQUAL_UINT32(1, Wire.setClockCalls);
  TEST_ASSERT_EQUAL_UINT16(50, Wire.getTimeOut());

  Wire.reset();
  TEST_ASSERT_TRUE(transport::initWire(1, 2, 400000, 37));
  TEST_ASSERT_EQUAL_UINT16(37, Wire.getTimeOut());

  TwoWire scannerWire;
  scannerWire.reset();
  scannerWire.setTimeOut(37);
  i2c_scanner::scan(scannerWire, 5);
  TEST_ASSERT_EQUAL_UINT32(112, scannerWire.beginTransmissionCalls);
  TEST_ASSERT_EQUAL_UINT32(112, scannerWire.endTransmissionCalls);
  TEST_ASSERT_EQUAL_UINT16(37, scannerWire.getTimeOut());
}

void test_phase3_strict_cli_numeric_tokens_preserve_outputs() {
  uint8_t u8 = 77;
  TEST_ASSERT_TRUE(cmd::parseU8Token(String("255"), u8));
  TEST_ASSERT_EQUAL_UINT8(255, u8);
  const char* invalidU8[] = {"", "256", "-1", "1x", " 1", "01 ",
                             "999999999999999999999999"};
  for (const char* token : invalidU8) {
    u8 = 77;
    TEST_ASSERT_FALSE(cmd::parseU8Token(String(token), u8));
    TEST_ASSERT_EQUAL_UINT8(77, u8);
  }

  uint16_t u16 = 33;
  TEST_ASSERT_TRUE(cmd::parseU16Token(String("65535"), u16));
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, u16);
  TEST_ASSERT_FALSE(cmd::parseU16Token(String("65536"), u16));
  TEST_ASSERT_EQUAL_UINT16(UINT16_MAX, u16);

  uint32_t u32 = 99;
  TEST_ASSERT_TRUE(cmd::parseU32Token(String("4294967295"), u32));
  TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, u32);
  u32 = 99;
  TEST_ASSERT_FALSE(cmd::parseU32Token(String("4294967296"), u32));
  TEST_ASSERT_EQUAL_UINT32(99, u32);

  int32_t i32 = 44;
  TEST_ASSERT_TRUE(cmd::parseInt32Token(String("-2147483648"), i32));
  TEST_ASSERT_EQUAL_INT32(INT32_MIN, i32);
  TEST_ASSERT_TRUE(cmd::parseInt32Token(String("2147483647"), i32));
  TEST_ASSERT_EQUAL_INT32(INT32_MAX, i32);
  i32 = 44;
  TEST_ASSERT_FALSE(cmd::parseInt32Token(String("2147483648"), i32));
  TEST_ASSERT_EQUAL_INT32(44, i32);
  TEST_ASSERT_FALSE(cmd::parseInt32Token(String("-2147483649"), i32));
  TEST_ASSERT_EQUAL_INT32(44, i32);

  float value = 12.5f;
  TEST_ASSERT_TRUE(cmd::parseFloatToken(String("-1.25"), value));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, -1.25f, value);
  const char* invalidFloat[] = {"", "nan", "NaN", "inf", "-inf", "1.0x",
                                "1e9999", "3.5e38", "1e-50", "0x1p0",
                                "+0X1p1", "-0x1p0"};
  for (const char* token : invalidFloat) {
    value = 12.5f;
    TEST_ASSERT_FALSE(cmd::parseFloatToken(String(token), value));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.5f, value);
  }

  bool boolean = true;
  TEST_ASSERT_TRUE(cmd::parseBool01Token(String("0"), boolean));
  TEST_ASSERT_FALSE(boolean);
  TEST_ASSERT_TRUE(cmd::parseBool01Token(String("1"), boolean));
  TEST_ASSERT_TRUE(boolean);
  for (const char* token : {"2", "-1", "true", "01x"}) {
    boolean = true;
    TEST_ASSERT_FALSE(cmd::parseBool01Token(String(token), boolean));
    TEST_ASSERT_TRUE(boolean);
  }

  uint8_t reg = 0xAA;
  TEST_ASSERT_TRUE(cmd::parseRegisterToken(String("10"), reg));
  TEST_ASSERT_EQUAL_HEX8(0x0A, reg);
  TEST_ASSERT_TRUE(cmd::parseRegisterToken(String("010"), reg));
  TEST_ASSERT_EQUAL_HEX8(0x0A, reg);
  TEST_ASSERT_TRUE(cmd::parseRegisterToken(String("0x10"), reg));
  TEST_ASSERT_EQUAL_HEX8(0x10, reg);
  TEST_ASSERT_TRUE(cmd::parseRegisterToken(String("0Xff"), reg));
  TEST_ASSERT_EQUAL_HEX8(0xFF, reg);
  reg = 0xAA;
  for (const char* token : {"0x", "0x100", "-1", "09junk"}) {
    TEST_ASSERT_FALSE(cmd::parseRegisterToken(String(token), reg));
    TEST_ASSERT_EQUAL_HEX8(0xAA, reg);
  }
}

void test_phase3_cli_line_reader_discards_overflow_through_terminator() {
  FakeRv3032 fake;
  beginCliHarness(fake, true);

  // A truncating reader would trim this prefix to the valid mutating command
  // "clear_porf". The sole reader must instead discard the entire overlength
  // line through its terminator, then execute only the following valid line.
  std::string input("clear_porf");
  input.append(cli_shell::MAX_LINE_LENGTH + 5U - input.length(), ' ');
  input += "\n\tverbose  1\t\n";
  Serial.inject(input);

  loop();
  TEST_ASSERT_TRUE(g_verbose);
  TEST_ASSERT_EQUAL_INT(0, Serial.available());
  TEST_ASSERT_EQUAL_UINT32(0, fake.callbackCount);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PendingSurface::NONE),
                          static_cast<uint8_t>(g_pendingOperation.surface));
  TEST_ASSERT_FALSE(g_rtc.isJobBusy());
  TEST_ASSERT_FALSE(g_rtc.isEepromBusy());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_passive_lifecycle_and_explicit_probe_evidence);
  RUN_TEST(test_config_validation_is_zero_io_and_non_destructive);
  RUN_TEST(test_end_unconditionally_abandons_work_with_zero_io);
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
  RUN_TEST(test_verified_calendar_retains_callback_status_on_timed_reconciliation);
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
  RUN_TEST(test_persistent_dynamic_cleanup_reserve_admission_is_zero_io);
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
  RUN_TEST(test_temp_lsb_flag_clear_uses_race_safe_fixed_payloads);
  RUN_TEST(test_static_calendar_and_status_utility_coverage);
  RUN_TEST(test_rebegin_rebinds_context_and_resets_only_lifecycle_latch);
  RUN_TEST(test_alarm_timer_and_pmu_round_trips_are_cooperative);
  RUN_TEST(test_clkout_factory_defaults_and_persistence_contract);
  RUN_TEST(test_clkout_offset_temperature_and_event_round_trips);
  RUN_TEST(test_timestamp_stop_gp_and_ram_public_coverage);
  RUN_TEST(test_fake_refresh_initial_busy_and_acked_ignore_faults);
  RUN_TEST(test_shared_job_transport_failure_is_observable_without_retry);
  RUN_TEST(test_transport_status_domain_and_health_attempt_evidence);
  RUN_TEST(test_timed_callback_completion_clipping_no_hook_and_wrap);
  RUN_TEST(test_ordinary_deadline_states_are_exclusive_at_boundary_and_late);
  RUN_TEST(test_phase1_mutating_jobs_do_not_replay_ambiguous_writes);
  RUN_TEST(test_queue_cleanup_failure_cancels_later_items);
  RUN_TEST(test_persistent_deadlines_stop_callbacks_and_report_unverified_cleanup);
  RUN_TEST(test_persistent_whole_deadline_preserves_effective_callback_failure);
  RUN_TEST(test_persistent_phase_deadlines_are_exclusive_at_boundary_and_late);
  RUN_TEST(test_dispatched_write_one_crossing_cutoff_enters_cleanup_without_replay);
  RUN_TEST(test_typed_vendor_controls_and_fake_register_semantics);
  RUN_TEST(test_fake_busy_command_read_zero_and_transfer_evidence);
  RUN_TEST(test_primary_busy_matrix_and_preserves_unrelated_device_state);
  RUN_TEST(test_primary_ignored_staging_and_command_phase_deadlines);
  RUN_TEST(test_capability_transport_failures_and_owner_read_retry_timing);
  RUN_TEST(test_primary_phase4_fault_attribution_and_dispatch_evidence);
  RUN_TEST(test_primary_phase4_late_command_ready_window_and_settle_anchor);
  RUN_TEST(test_phase2_backup_activation_reconciliation_and_admission);
  RUN_TEST(test_phase2_backup_fault_timing_and_evidence_matrix);
  RUN_TEST(test_phase2_staged_fault_ordinals_getters_and_no_replay);
  RUN_TEST(test_phase2_staged_cleanup_fault_matrix_and_caps);
  RUN_TEST(test_clkout_verification_reports_last_byte_mismatch_safely);
  RUN_TEST(test_phase4_staged_safe_payloads_preserve_neighbor_bits);
  RUN_TEST(test_phase2_configuration_reports_caps_and_safe_cleanup);
  RUN_TEST(test_phase2_quiescence_uie_and_tick_contracts);
  RUN_TEST(test_phase2_quiescence_guard_caps_and_evi_reset_no_replay);
  RUN_TEST(test_phase2_persistent_operation_and_cleanup_evidence_are_separate);
  RUN_TEST(test_phase2_typed_persistence_mixed_failure_and_getter_matrix);
  RUN_TEST(test_phase2_generic_batch_evidence_precedence_and_reset);
  RUN_TEST(test_phase4_late_cleanup_write_requires_semantic_cleanup_failure);
  RUN_TEST(test_phase3_cli_invalid_mutating_commands_are_zero_io);
  RUN_TEST(test_phase3_cli_ram_and_timestamp_terminal_output);
  RUN_TEST(test_phase3_cli_owner_handoff_is_single_callback_and_preserves_status);
  RUN_TEST(test_phase3_cli_persistent_helper_deadline_does_not_orphan);
  RUN_TEST(test_phase3_wire_validation_and_closed_status_domain);
  RUN_TEST(test_phase3_wire_complete_deadline_timeout_restoration_and_order);
  RUN_TEST(test_phase3_wire_short_stage_release_and_initialization);
  RUN_TEST(test_phase3_strict_cli_numeric_tokens_preserve_outputs);
  RUN_TEST(test_phase3_cli_line_reader_discards_overflow_through_terminator);
  return UNITY_END();
}
