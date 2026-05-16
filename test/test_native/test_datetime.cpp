#include <stddef.h>
#include <stdint.h>
#include <cstring>
#include <math.h>
#include <unity.h>

#include "RV3032/CommandTable.h"
#include "RV3032/RV3032.h"

namespace {

struct FakeI2cBus {
  uint8_t regs[256];
  RV3032::Status nextReadStatus = RV3032::Status::Ok();
  RV3032::Status nextWriteStatus = RV3032::Status::Ok();
  uint32_t readCalls = 0;
  uint32_t writeCalls = 0;
};

uint32_t fakeNowMs(void*) {
  return 222u;
}

void resetBus(FakeI2cBus& bus) {
  std::memset(bus.regs, 0, sizeof(bus.regs));
  bus.nextReadStatus = RV3032::Status::Ok();
  bus.nextWriteStatus = RV3032::Status::Ok();
  bus.readCalls = 0;
  bus.writeCalls = 0;
  bus.regs[RV3032::cmd::REG_STATUS] = 0x00;
  bus.regs[RV3032::cmd::REG_EEPROM_PMU] = RV3032::cmd::PMU_BSM_LEVEL;
  bus.regs[RV3032::cmd::REG_TIMER_HIGH] = 0xA0;
}

RV3032::Status fakeI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                            uint32_t, void* user) {
  if (!user) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "user null");
  }
  if (addr != RV3032::cmd::I2C_ADDR_7BIT) {
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR, "address mismatch", -1);
  }

  FakeI2cBus* bus = static_cast<FakeI2cBus*>(user);
  bus->writeCalls++;
  if (!data || len == 0) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "write invalid");
  }

  if (!bus->nextWriteStatus.ok()) {
    RV3032::Status st = bus->nextWriteStatus;
    bus->nextWriteStatus = RV3032::Status::Ok();
    return st;
  }

  const uint8_t reg = data[0];
  for (size_t i = 1; i < len; ++i) {
    bus->regs[static_cast<uint8_t>(reg + static_cast<uint8_t>(i - 1))] = data[i];
  }
  return RV3032::Status::Ok();
}

RV3032::Status fakeI2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                                uint8_t* rx, size_t rxLen, uint32_t, void* user) {
  if (!user) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "user null");
  }
  if (addr != RV3032::cmd::I2C_ADDR_7BIT) {
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR, "address mismatch", -1);
  }

  FakeI2cBus* bus = static_cast<FakeI2cBus*>(user);
  bus->readCalls++;
  if (!tx || txLen != 1 || !rx || rxLen == 0) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "read invalid");
  }

  if (!bus->nextReadStatus.ok()) {
    RV3032::Status st = bus->nextReadStatus;
    bus->nextReadStatus = RV3032::Status::Ok();
    return st;
  }

  const uint8_t reg = tx[0];
  for (size_t i = 0; i < rxLen; ++i) {
    rx[i] = bus->regs[static_cast<uint8_t>(reg + static_cast<uint8_t>(i))];
  }
  return RV3032::Status::Ok();
}

RV3032::Config makeConfig(FakeI2cBus& bus) {
  RV3032::Config cfg;
  cfg.i2cWrite = fakeI2cWrite;
  cfg.i2cWriteRead = fakeI2cWriteRead;
  cfg.i2cUser = &bus;
  cfg.enableEepromWrites = false;
  cfg.i2cTimeoutMs = 50;
  return cfg;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_bcd_roundtrip() {
  const uint8_t values[] = {0, 1, 9, 10, 12, 59, 99};
  for (size_t i = 0; i < (sizeof(values) / sizeof(values[0])); ++i) {
    const uint8_t value = values[i];
    const uint8_t bcd = RV3032::RV3032::binaryToBcd(value);
    const uint8_t roundtrip = RV3032::RV3032::bcdToBinary(bcd);
    TEST_ASSERT_EQUAL_UINT8(value, roundtrip);
  }
}

void test_status_helpers() {
  RV3032::Status st = RV3032::Status::Error(RV3032::Err::I2C_ERROR, "fail", 7);
  TEST_ASSERT_TRUE(st.is(RV3032::Err::I2C_ERROR));
  TEST_ASSERT_FALSE(st.is(RV3032::Err::TIMEOUT));
  TEST_ASSERT_FALSE(static_cast<bool>(st));
  TEST_ASSERT_FALSE(st.inProgress());

  RV3032::Status inProgress{RV3032::Err::IN_PROGRESS, 0, "queued"};
  TEST_ASSERT_TRUE(inProgress.inProgress());
  TEST_ASSERT_TRUE(static_cast<bool>(RV3032::Status::Ok()));
}

void test_compute_weekday() {
  const uint8_t weekday = RV3032::RV3032::computeWeekday(2000, 1, 1);
  TEST_ASSERT_EQUAL_UINT8(6, weekday);  // Saturday
}

void test_unix_roundtrip_epoch_2000() {
  RV3032::DateTime dt;
  dt.year = 2000;
  dt.month = 1;
  dt.day = 1;
  dt.hour = 0;
  dt.minute = 0;
  dt.second = 0;
  dt.weekday = 0;

  uint32_t ts = 0;
  TEST_ASSERT_TRUE(RV3032::RV3032::dateTimeToUnix(dt, ts));
  TEST_ASSERT_EQUAL_UINT32(946684800UL, ts);

  RV3032::DateTime out;
  TEST_ASSERT_TRUE(RV3032::RV3032::unixToDateTime(ts, out));
  TEST_ASSERT_EQUAL_UINT16(2000, out.year);
  TEST_ASSERT_EQUAL_UINT8(1, out.month);
  TEST_ASSERT_EQUAL_UINT8(1, out.day);
  TEST_ASSERT_EQUAL_UINT8(0, out.hour);
  TEST_ASSERT_EQUAL_UINT8(0, out.minute);
  TEST_ASSERT_EQUAL_UINT8(0, out.second);
}

void test_unix_leap_day() {
  RV3032::DateTime dt;
  dt.year = 2020;
  dt.month = 2;
  dt.day = 29;
  dt.hour = 12;
  dt.minute = 34;
  dt.second = 56;
  dt.weekday = 0;

  uint32_t ts = 0;
  TEST_ASSERT_TRUE(RV3032::RV3032::dateTimeToUnix(dt, ts));
  TEST_ASSERT_EQUAL_UINT32(1582979696UL, ts);

  RV3032::DateTime out;
  TEST_ASSERT_TRUE(RV3032::RV3032::unixToDateTime(ts, out));
  TEST_ASSERT_EQUAL_UINT16(2020, out.year);
  TEST_ASSERT_EQUAL_UINT8(2, out.month);
  TEST_ASSERT_EQUAL_UINT8(29, out.day);
  TEST_ASSERT_EQUAL_UINT8(12, out.hour);
  TEST_ASSERT_EQUAL_UINT8(34, out.minute);
  TEST_ASSERT_EQUAL_UINT8(56, out.second);
}

void test_unix_out_of_rtc_range() {
  RV3032::DateTime out;
  TEST_ASSERT_FALSE(RV3032::RV3032::unixToDateTime(0xFFFFFFFFu, out));
}

void test_invalid_date() {
  RV3032::DateTime dt;
  dt.year = 2021;
  dt.month = 2;
  dt.day = 29;
  dt.hour = 0;
  dt.minute = 0;
  dt.second = 0;
  dt.weekday = 0;

  uint32_t ts = 0;
  TEST_ASSERT_FALSE(RV3032::RV3032::dateTimeToUnix(dt, ts));
  TEST_ASSERT_FALSE(RV3032::RV3032::isValidDateTime(dt));
}

void test_begin_rejects_non_default_address() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Config cfg = makeConfig(bus);
  cfg.i2cAddress = 0x52;

  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
}

void test_begin_rejects_invalid_backup_mode() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Config cfg = makeConfig(bus);
  cfg.backupMode = static_cast<RV3032::BackupSwitchMode>(9);

  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
}

void test_invalid_begin_after_success_resets_without_i2c() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(rtc.isInitialized());

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;

  RV3032::Config bad = makeConfig(bus);
  bad.i2cAddress = 0x52;
  st = rtc.begin(bad);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::UNINIT),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  RV3032::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(rtc.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.initialized);
  TEST_ASSERT_EQUAL_HEX8(0x51, snap.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(50u, snap.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(5u, snap.offlineThreshold);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.lastOkMs);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.lastErrorMs);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.totalFailures);
}

void test_get_settings_snapshot() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Config cfg = makeConfig(bus);
  cfg.nowMs = fakeNowMs;
  cfg.i2cTimeoutMs = 77;
  cfg.offlineThreshold = 4;
  cfg.backupMode = RV3032::BackupSwitchMode::Direct;
  cfg.enableEepromWrites = false;
  cfg.eepromTimeoutMs = 88;

  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());

  RV3032::SettingsSnapshot snap;
  st = rtc.getSettings(snap);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(snap.initialized);
  TEST_ASSERT_EQUAL(static_cast<int>(RV3032::DriverState::READY),
                    static_cast<int>(snap.state));
  const RV3032::SettingsSnapshot byValue = rtc.getSettings();
  TEST_ASSERT_EQUAL(static_cast<int>(snap.state), static_cast<int>(byValue.state));
  TEST_ASSERT_EQUAL(static_cast<int>(rtc.state()), static_cast<int>(rtc.driverState()));
  TEST_ASSERT_EQUAL_HEX8(0x51, snap.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(77u, snap.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(4u, snap.offlineThreshold);
  TEST_ASSERT_TRUE(snap.hasNowMsHook);
  TEST_ASSERT_FALSE(snap.beginInProgress);
  TEST_ASSERT_EQUAL(static_cast<uint8_t>(RV3032::BackupSwitchMode::Direct),
                    static_cast<uint8_t>(snap.backupMode));
  TEST_ASSERT_FALSE(snap.enableEepromWrites);
  TEST_ASSERT_EQUAL_UINT32(88u, snap.eepromTimeoutMs);
  TEST_ASSERT_FALSE(snap.eepromBusy);
  TEST_ASSERT_TRUE(snap.eepromLastStatus.ok());
  TEST_ASSERT_EQUAL_UINT32(0u, snap.eepromQueueDepth);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.lastOkMs);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.lastErrorMs);
  TEST_ASSERT_TRUE(snap.lastError.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, snap.consecutiveFailures);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.totalFailures);
}

void test_runtime_i2c_after_begin_updates_health() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::Config cfg = makeConfig(bus);
  cfg.nowMs = fakeNowMs;
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.lastOkMs());

  uint8_t status = 0;
  st = rtc.readStatus(status);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1u, rtc.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(222u, rtc.lastOkMs());
}

void test_probe_failure_does_not_update_health() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());

  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_TIMEOUT,
                                              "forced probe timeout", -7);
  st = rtc.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-7, st.detail);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());
}

void test_recover_failure_updates_health_once() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());

  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR,
                                             "forced recover nack", -8);
  st = rtc.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-8, st.detail);
  TEST_ASSERT_EQUAL_UINT8(1, rtc.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::DEGRADED),
                          static_cast<uint8_t>(rtc.state()));
}

void test_offline_latches_public_read_without_i2c() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());

  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR,
                                             "forced offline", -10);
  st = rtc.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::OFFLINE),
                          static_cast<uint8_t>(rtc.state()));

  const uint32_t readsBefore = bus.readCalls;
  uint8_t status = 0;
  st = rtc.readStatus(status);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
}

void test_failed_recover_from_offline_keeps_latch_after_intermediate_success() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 3;
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());

  for (uint8_t i = 0; i < cfg.offlineThreshold; ++i) {
    bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR,
                                               "forced offline", -11);
    (void)rtc.recover();
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::OFFLINE),
                          static_cast<uint8_t>(rtc.state()));

  bus.regs[RV3032::cmd::REG_EEPROM_PMU] = 0x00;
  bus.nextWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA,
                                              "recover apply failed", -12);
  st = rtc.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::OFFLINE),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_TRUE(rtc.consecutiveFailures() >= cfg.offlineThreshold);

  const uint32_t readsBefore = bus.readCalls;
  uint8_t status = 0;
  st = rtc.readStatus(status);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
}

void test_transport_validation_status_does_not_update_health() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::INVALID_PARAM,
                                              "transport validation", -9);
  uint8_t status = 0;
  st = rtc.readStatus(status);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
}

void test_set_timer_preserves_reserved_high_bits() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.setTimer(0x0456, RV3032::TimerFrequency::Hz1, true);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0x56, bus.regs[RV3032::cmd::REG_TIMER_LOW]);
  TEST_ASSERT_EQUAL_HEX8(0xA4, bus.regs[RV3032::cmd::REG_TIMER_HIGH]);
}

void test_invalid_runtime_params_are_rejected() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.setTimer(1, static_cast<RV3032::TimerFrequency>(9), true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.setClkoutFrequency(static_cast<RV3032::ClkoutFrequency>(7));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.setEviDebounce(static_cast<RV3032::EviDebounce>(5));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.setOffsetPpm(NAN);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  RV3032::Timestamp ts;
  st = rtc.readTimestamp(static_cast<RV3032::TimestampSource>(9), ts);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
}

void test_get_alarm_config_rejects_invalid_bcd() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_ALARM_MINUTE] = 0x7A;
  bus.regs[RV3032::cmd::REG_ALARM_HOUR] = 0x12;
  bus.regs[RV3032::cmd::REG_ALARM_DATE] = 0x15;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::AlarmConfig alarm;
  st = rtc.getAlarmConfig(alarm);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
}

void test_get_alarm_config_tolerates_por_inactive_date_alarm() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_ALARM_MINUTE] = 0x00;
  bus.regs[RV3032::cmd::REG_ALARM_HOUR] = 0x00;
  bus.regs[RV3032::cmd::REG_ALARM_DATE] = 0x00;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::AlarmConfig alarm;
  st = rtc.getAlarmConfig(alarm);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(alarm.matchMinute);
  TEST_ASSERT_TRUE(alarm.matchHour);
  TEST_ASSERT_TRUE(alarm.matchDate);
  TEST_ASSERT_EQUAL_UINT8(0, alarm.minute);
  TEST_ASSERT_EQUAL_UINT8(0, alarm.hour);
  TEST_ASSERT_EQUAL_UINT8(0, alarm.date);
}

void test_get_alarm_config_tolerates_disabled_field_garbage() {
  FakeI2cBus bus;
  resetBus(bus);
  // Minute/hour enabled and valid.
  bus.regs[RV3032::cmd::REG_ALARM_MINUTE] = 0x12;  // 12
  bus.regs[RV3032::cmd::REG_ALARM_HOUR] = 0x08;    // 08
  // Date disabled (bit7=1) with invalid BCD payload in lower bits.
  bus.regs[RV3032::cmd::REG_ALARM_DATE] = 0xFA;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::AlarmConfig alarm;
  st = rtc.getAlarmConfig(alarm);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(alarm.matchMinute);
  TEST_ASSERT_TRUE(alarm.matchHour);
  TEST_ASSERT_FALSE(alarm.matchDate);
  TEST_ASSERT_EQUAL_UINT8(12, alarm.minute);
  TEST_ASSERT_EQUAL_UINT8(8, alarm.hour);
  TEST_ASSERT_EQUAL_UINT8(1, alarm.date);
}

void test_read_timestamp_decodes_evi_and_reset_sets_control_bit() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_TS_EVI_COUNT] = 0x03;
  bus.regs[RV3032::cmd::REG_TS_EVI_100TH_SECONDS] = 0x45;
  bus.regs[RV3032::cmd::REG_TS_EVI_SECONDS] = 0x56;
  bus.regs[RV3032::cmd::REG_TS_EVI_MINUTES] = 0x34;
  bus.regs[RV3032::cmd::REG_TS_EVI_HOURS] = 0x12;
  bus.regs[RV3032::cmd::REG_TS_EVI_DATE] = 0x29;
  bus.regs[RV3032::cmd::REG_TS_EVI_MONTH] = 0x02;
  bus.regs[RV3032::cmd::REG_TS_EVI_YEAR] = 0x24;
  bus.regs[RV3032::cmd::REG_TS_CONTROL] = (1u << RV3032::cmd::TS_EVI_OVERWRITE_BIT);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::Timestamp ts;
  st = rtc.readTimestamp(RV3032::TimestampSource::Evi, ts);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0x03, ts.count);
  TEST_ASSERT_TRUE(ts.hasHundredths);
  TEST_ASSERT_EQUAL_UINT8(45, ts.hundredths);
  TEST_ASSERT_EQUAL_UINT16(2024, ts.time.year);
  TEST_ASSERT_EQUAL_UINT8(2, ts.time.month);
  TEST_ASSERT_EQUAL_UINT8(29, ts.time.day);
  TEST_ASSERT_EQUAL_UINT8(12, ts.time.hour);
  TEST_ASSERT_EQUAL_UINT8(34, ts.time.minute);
  TEST_ASSERT_EQUAL_UINT8(56, ts.time.second);
  TEST_ASSERT_EQUAL_UINT8(RV3032::RV3032::computeWeekday(2024, 2, 29), ts.time.weekday);

  st = rtc.resetTimestamp(RV3032::TimestampSource::Evi);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE((bus.regs[RV3032::cmd::REG_TS_CONTROL] &
                    (1u << RV3032::cmd::TS_EVI_RESET_BIT)) != 0);
  TEST_ASSERT_TRUE((bus.regs[RV3032::cmd::REG_TS_CONTROL] &
                    (1u << RV3032::cmd::TS_EVI_OVERWRITE_BIT)) != 0);
}

void test_read_timestamp_decodes_tlow_without_hundredths() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_TS_TLOW_COUNT] = 0x02;
  bus.regs[RV3032::cmd::REG_TS_TLOW_SECONDS] = 0x01;
  bus.regs[RV3032::cmd::REG_TS_TLOW_MINUTES] = 0x02;
  bus.regs[RV3032::cmd::REG_TS_TLOW_HOURS] = 0x03;
  bus.regs[RV3032::cmd::REG_TS_TLOW_DATE] = 0x04;
  bus.regs[RV3032::cmd::REG_TS_TLOW_MONTH] = 0x05;
  bus.regs[RV3032::cmd::REG_TS_TLOW_YEAR] = 0x26;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::Timestamp ts;
  st = rtc.readTimestamp(RV3032::TimestampSource::TLow, ts);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0x02, ts.count);
  TEST_ASSERT_FALSE(ts.hasHundredths);
  TEST_ASSERT_EQUAL_UINT8(0, ts.hundredths);
  TEST_ASSERT_EQUAL_UINT16(2026, ts.time.year);
  TEST_ASSERT_EQUAL_UINT8(5, ts.time.month);
  TEST_ASSERT_EQUAL_UINT8(4, ts.time.day);
  TEST_ASSERT_EQUAL_UINT8(3, ts.time.hour);
  TEST_ASSERT_EQUAL_UINT8(2, ts.time.minute);
  TEST_ASSERT_EQUAL_UINT8(1, ts.time.second);
}

void test_read_timestamp_rejects_invalid_bcd() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_TS_THIGH_SECONDS] = 0x6A;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::Timestamp ts;
  st = rtc.readTimestamp(RV3032::TimestampSource::THigh, ts);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                          static_cast<uint8_t>(st.code));
}

void test_user_ram_helpers_round_trip_and_reject_invalid_ranges() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  uint8_t fullWrite[16] = {};
  for (uint8_t i = 0; i < sizeof(fullWrite); ++i) {
    fullWrite[i] = static_cast<uint8_t>(0x10U + i);
  }
  st = rtc.writeUserRam(0, fullWrite, sizeof(fullWrite));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0x10, bus.regs[RV3032::cmd::REG_USER_RAM_START]);
  TEST_ASSERT_EQUAL_HEX8(0x1F, bus.regs[RV3032::cmd::REG_USER_RAM_END]);

  const uint8_t writeBuf[2] = {0xAA, 0x55};
  st = rtc.writeUserRam(14, writeBuf, sizeof(writeBuf));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0xAA, bus.regs[RV3032::cmd::REG_USER_RAM_END - 1]);
  TEST_ASSERT_EQUAL_HEX8(0x55, bus.regs[RV3032::cmd::REG_USER_RAM_END]);

  uint8_t readBuf[2] = {};
  st = rtc.readUserRam(14, readBuf, sizeof(readBuf));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(writeBuf, readBuf, sizeof(writeBuf));

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  st = rtc.readUserRam(15, readBuf, sizeof(readBuf));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = rtc.writeUserRam(15, writeBuf, sizeof(writeBuf));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

void test_public_block_register_access() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_SECONDS] = 0x01;
  bus.regs[RV3032::cmd::REG_MINUTES] = 0x23;
  bus.regs[RV3032::cmd::REG_HOURS] = 0x45;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  uint8_t readBuf[3] = {};
  st = rtc.readRegisters(RV3032::cmd::REG_SECONDS, readBuf, sizeof(readBuf));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0x01, readBuf[0]);
  TEST_ASSERT_EQUAL_HEX8(0x23, readBuf[1]);
  TEST_ASSERT_EQUAL_HEX8(0x45, readBuf[2]);

  const uint8_t writeBuf[3] = {0x10, 0x11, 0x12};
  st = rtc.writeRegisters(RV3032::cmd::REG_USER_RAM_START, writeBuf, sizeof(writeBuf));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0x10, bus.regs[RV3032::cmd::REG_USER_RAM_START]);
  TEST_ASSERT_EQUAL_HEX8(0x11, bus.regs[static_cast<uint8_t>(RV3032::cmd::REG_USER_RAM_START + 1)]);
  TEST_ASSERT_EQUAL_HEX8(0x12, bus.regs[static_cast<uint8_t>(RV3032::cmd::REG_USER_RAM_START + 2)]);
}

void test_public_block_register_access_rejects_invalid_params() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  uint8_t one = 0;
  st = rtc.readRegisters(RV3032::cmd::REG_SECONDS, nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.readRegisters(RV3032::cmd::REG_SECONDS, &one, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.writeRegisters(RV3032::cmd::REG_SECONDS, nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.writeRegisters(RV3032::cmd::REG_SECONDS, &one, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
}

void test_public_block_register_access_rejects_invalid_ranges_without_i2c() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  const uint32_t readsAfterBegin = bus.readCalls;
  const uint32_t writesAfterBegin = bus.writeCalls;
  uint8_t one = 0;
  uint8_t two[2] = {};

  st = rtc.readRegisters(0x2E, &one, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.readRegisters(RV3032::cmd::REG_TS_EVI_YEAR, two, sizeof(two));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.writeRegisters(0x50, &one, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.writeRegisters(RV3032::cmd::REG_USER_RAM_END, two, sizeof(two));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  TEST_ASSERT_EQUAL_UINT32(readsAfterBegin, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesAfterBegin, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_bcd_roundtrip);
  RUN_TEST(test_status_helpers);
  RUN_TEST(test_compute_weekday);
  RUN_TEST(test_unix_roundtrip_epoch_2000);
  RUN_TEST(test_unix_leap_day);
  RUN_TEST(test_unix_out_of_rtc_range);
  RUN_TEST(test_invalid_date);
  RUN_TEST(test_begin_rejects_non_default_address);
  RUN_TEST(test_begin_rejects_invalid_backup_mode);
  RUN_TEST(test_invalid_begin_after_success_resets_without_i2c);
  RUN_TEST(test_get_settings_snapshot);
  RUN_TEST(test_runtime_i2c_after_begin_updates_health);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_recover_failure_updates_health_once);
  RUN_TEST(test_offline_latches_public_read_without_i2c);
  RUN_TEST(test_failed_recover_from_offline_keeps_latch_after_intermediate_success);
  RUN_TEST(test_transport_validation_status_does_not_update_health);
  RUN_TEST(test_set_timer_preserves_reserved_high_bits);
  RUN_TEST(test_invalid_runtime_params_are_rejected);
  RUN_TEST(test_get_alarm_config_rejects_invalid_bcd);
  RUN_TEST(test_get_alarm_config_tolerates_por_inactive_date_alarm);
  RUN_TEST(test_get_alarm_config_tolerates_disabled_field_garbage);
  RUN_TEST(test_read_timestamp_decodes_evi_and_reset_sets_control_bit);
  RUN_TEST(test_read_timestamp_decodes_tlow_without_hundredths);
  RUN_TEST(test_read_timestamp_rejects_invalid_bcd);
  RUN_TEST(test_user_ram_helpers_round_trip_and_reject_invalid_ranges);
  RUN_TEST(test_public_block_register_access);
  RUN_TEST(test_public_block_register_access_rejects_invalid_params);
  RUN_TEST(test_public_block_register_access_rejects_invalid_ranges_without_i2c);
  return UNITY_END();
}
