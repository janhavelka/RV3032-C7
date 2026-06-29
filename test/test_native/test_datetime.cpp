#include <stddef.h>
#include <stdint.h>
#include <cstring>
#include <math.h>
#include <unity.h>

#include "RV3032/CommandTable.h"
#include "RV3032/RV3032.h"

namespace {

static constexpr size_t kWriteLogSize = 128;

struct FakeI2cBus {
  uint8_t regs[256];
  RV3032::Status nextReadStatus = RV3032::Status::Ok();
  RV3032::Status nextWriteStatus = RV3032::Status::Ok();
  uint32_t readCalls = 0;
  uint32_t writeCalls = 0;
  uint8_t writeRegLog[kWriteLogSize];
  uint8_t writeValueLog[kWriteLogSize];
  uint8_t writeLenLog[kWriteLogSize];
  uint32_t writeLogCount = 0;
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
  std::memset(bus.writeRegLog, 0, sizeof(bus.writeRegLog));
  std::memset(bus.writeValueLog, 0, sizeof(bus.writeValueLog));
  std::memset(bus.writeLenLog, 0, sizeof(bus.writeLenLog));
  bus.writeLogCount = 0;
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

  const uint8_t reg = data[0];
  if (bus->writeLogCount < kWriteLogSize) {
    const uint32_t index = bus->writeLogCount;
    bus->writeRegLog[index] = reg;
    bus->writeValueLog[index] = (len > 1) ? data[1] : 0;
    bus->writeLenLog[index] = static_cast<uint8_t>(len);
  }
  bus->writeLogCount++;

  if (!bus->nextWriteStatus.ok()) {
    RV3032::Status st = bus->nextWriteStatus;
    bus->nextWriteStatus = RV3032::Status::Ok();
    return st;
  }

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

RV3032::Config makePersistentConfig(FakeI2cBus& bus) {
  RV3032::Config cfg = makeConfig(bus);
  cfg.enableEepromWrites = true;
  cfg.eepromTimeoutMs = 10;
  cfg.nowMs = fakeNowMs;
  return cfg;
}

uint32_t countWritesTo(const FakeI2cBus& bus, uint8_t reg) {
  uint32_t count = 0;
  const uint32_t used = (bus.writeLogCount < kWriteLogSize) ? bus.writeLogCount : kWriteLogSize;
  for (uint32_t i = 0; i < used; ++i) {
    if (bus.writeRegLog[i] == reg) {
      ++count;
    }
  }
  return count;
}

uint32_t totalCalls(const FakeI2cBus& bus) {
  return bus.readCalls + bus.writeCalls;
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

void test_begin_rejects_invalid_eeprom_enabled_timeouts() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Config cfg = makeConfig(bus);
  cfg.enableEepromWrites = true;
  cfg.eepromTimeoutMs = 0;

  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);

  resetBus(bus);
  cfg = makeConfig(bus);
  cfg.enableEepromWrites = true;
  cfg.i2cTimeoutMs = 49;
  st = rtc.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
}

void test_begin_probe_failure_preserves_transport_errors() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_BUS,
                                             "bus stuck", -33);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-33, st.detail);
  TEST_ASSERT_FALSE(rtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::UNINIT),
                          static_cast<uint8_t>(rtc.state()));

  resetBus(bus);
  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR,
                                             "address nack", -34);
  RV3032::RV3032 missingRtc;
  st = missingRtc.begin(makeConfig(bus));

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-34, st.detail);
  TEST_ASSERT_FALSE(missingRtc.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::UNINIT),
                          static_cast<uint8_t>(missingRtc.state()));
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

  const uint32_t readsBeforeSettings = bus.readCalls;
  const uint32_t writesBeforeSettings = bus.writeCalls;

  RV3032::SettingsSnapshot snap;
  st = rtc.getSettings(snap);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(snap.initialized);
  TEST_ASSERT_EQUAL(static_cast<int>(RV3032::DriverState::READY),
                    static_cast<int>(snap.state));
  const RV3032::SettingsSnapshot byValue = rtc.getSettings();
  TEST_ASSERT_EQUAL(static_cast<int>(snap.state), static_cast<int>(byValue.state));
  TEST_ASSERT_EQUAL(static_cast<int>(rtc.state()), static_cast<int>(rtc.driverState()));
  TEST_ASSERT_EQUAL_UINT32(readsBeforeSettings, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBeforeSettings, bus.writeCalls);
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-7, st.detail);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());

  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR,
                                             "forced probe nack", -8);
  st = rtc.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-8, st.detail);
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

void test_successful_recover_from_offline_unlatches_driver() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());

  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR,
                                             "forced offline", -13);
  uint8_t status = 0;
  st = rtc.readStatus(status);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_ADDR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::OFFLINE),
                          static_cast<uint8_t>(rtc.state()));

  st = rtc.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_UINT8(0u, rtc.consecutiveFailures());

  const uint32_t readsBefore = bus.readCalls;
  st = rtc.readStatus(status);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 1u, bus.readCalls);
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

void test_runtime_i2c_failure_codes_update_health() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::Config cfg = makeConfig(bus);
  cfg.nowMs = fakeNowMs;
  cfg.offlineThreshold = 10;
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());

  const RV3032::Err codes[] = {
      RV3032::Err::I2C_NACK_ADDR,
      RV3032::Err::I2C_NACK_DATA,
      RV3032::Err::I2C_TIMEOUT,
      RV3032::Err::I2C_BUS,
      RV3032::Err::I2C_ERROR,
  };

  for (uint8_t i = 0; i < (sizeof(codes) / sizeof(codes[0])); ++i) {
    bus.nextReadStatus = RV3032::Status::Error(codes[i], "forced read failure",
                                               static_cast<int32_t>(-20 - i));
    uint8_t status = 0;
    st = rtc.readStatus(status);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(codes[i]), static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i + 1), rtc.consecutiveFailures());
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(i + 1), rtc.totalFailures());
    TEST_ASSERT_EQUAL_UINT32(222u, rtc.lastErrorMs());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::DEGRADED),
                            static_cast<uint8_t>(rtc.state()));
  }
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
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((1u << RV3032::cmd::CTRL1_TE_BIT) |
                                             static_cast<uint8_t>(RV3032::TimerFrequency::Hz1)),
                         bus.regs[RV3032::cmd::REG_CONTROL1]);
}

void test_budgeted_set_timer_job_honors_instruction_budget() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  st = rtc.startSetTimerJob(0x0456, RV3032::TimerFrequency::Hz1, true);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(rtc.isJobBusy());
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  uint8_t used = 99;
  st = rtc.pollJob(0, 0, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(0u, used);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  st = rtc.pollJob(1, 2, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(2u, used);
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  st = rtc.pollJob(2, 10, used);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(4u, used);
  TEST_ASSERT_FALSE(rtc.isJobBusy());
  TEST_ASSERT_TRUE(rtc.getJobStatus().ok());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 2u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 4u, bus.writeCalls);
  TEST_ASSERT_EQUAL_HEX8(0x56, bus.regs[RV3032::cmd::REG_TIMER_LOW]);
  TEST_ASSERT_EQUAL_HEX8(0xA4, bus.regs[RV3032::cmd::REG_TIMER_HIGH]);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((1u << RV3032::cmd::CTRL1_TE_BIT) |
                                             static_cast<uint8_t>(RV3032::TimerFrequency::Hz1)),
                         bus.regs[RV3032::cmd::REG_CONTROL1]);
}

void test_budgeted_register_update_job_honors_instruction_budget() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_CONTROL2] = 0xA0;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  st = rtc.startRegisterUpdateJob(RV3032::cmd::REG_CONTROL2, 0x20, 0x08);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));

  uint8_t used = 0;
  st = rtc.pollJob(10, 1, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(1u, used);
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 1u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  st = rtc.pollJob(11, 1, used);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(1u, used);
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 1u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_HEX8(0x88, bus.regs[RV3032::cmd::REG_CONTROL2]);
}

void test_budgeted_user_ram_write_job_chunks_large_write() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  uint8_t fullWrite[16] = {};
  for (uint8_t i = 0; i < sizeof(fullWrite); ++i) {
    fullWrite[i] = static_cast<uint8_t>(0x30U + i);
  }

  const uint32_t writesBefore = bus.writeCalls;
  st = rtc.startWriteUserRamJob(0, fullWrite, sizeof(fullWrite));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  uint8_t used = 0;
  st = rtc.pollJob(20, 1, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(1u, used);
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_HEX8(0x30, bus.regs[RV3032::cmd::REG_USER_RAM_START]);
  TEST_ASSERT_EQUAL_HEX8(0x3E, bus.regs[RV3032::cmd::REG_USER_RAM_START + 14]);
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.regs[RV3032::cmd::REG_USER_RAM_END]);

  st = rtc.pollJob(21, 1, used);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(1u, used);
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 2u, bus.writeCalls);
  TEST_ASSERT_EQUAL_HEX8(0x3F, bus.regs[RV3032::cmd::REG_USER_RAM_END]);
}

void test_budgeted_job_aborts_when_driver_is_offline() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.startSetTimerJob(0x0123, RV3032::TimerFrequency::Hz1, true);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(rtc.isJobBusy());

  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_BUS,
                                             "forced offline", -30);
  uint8_t status = 0;
  st = rtc.readStatus(status);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::OFFLINE),
                          static_cast<uint8_t>(rtc.state()));

  uint8_t used = 99;
  st = rtc.pollJob(0, 1, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(0u, used);
  TEST_ASSERT_FALSE(rtc.isJobBusy());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
                          static_cast<uint8_t>(rtc.getJobStatus().code));
}

void test_default_config_keeps_eeprom_persistence_disabled() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_EEPROM_PMU] = 0x00;

  RV3032::Config cfg;
  cfg.i2cWrite = fakeI2cWrite;
  cfg.i2cWriteRead = fakeI2cWriteRead;
  cfg.i2cUser = &bus;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(rtc.getSettings().enableEepromWrites);
  TEST_ASSERT_FALSE(rtc.isEepromBusy());
  TEST_ASSERT_TRUE(rtc.getEepromStatus().ok());
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::PMU_BSM_LEVEL,
                         static_cast<uint8_t>(bus.regs[RV3032::cmd::REG_EEPROM_PMU] &
                                              RV3032::cmd::PMU_BSM_MASK));
  TEST_ASSERT_EQUAL_UINT32(0u, countWritesTo(bus, RV3032::cmd::REG_EE_COMMAND));

  st = rtc.setClkoutEnabled(false);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_UINT8(0u, rtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT32(0u, countWritesTo(bus, RV3032::cmd::REG_EE_COMMAND));
}

void test_eeprom_poll_uses_one_instruction_per_call() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makePersistentConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.setBackupSwitchMode(RV3032::BackupSwitchMode::Direct);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(rtc.isEepromBusy());

  uint8_t used = 99;
  const uint32_t callsBeforeZeroBudget = totalCalls(bus);
  st = rtc.pollEeprom(0, 0, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(0u, used);
  TEST_ASSERT_EQUAL_UINT32(callsBeforeZeroBudget, totalCalls(bus));

  uint8_t polls = 0;
  while (rtc.isEepromBusy()) {
    const uint32_t callsBefore = totalCalls(bus);
    st = rtc.pollEeprom(static_cast<uint32_t>(polls + 1U), 5, used);
    TEST_ASSERT_TRUE(used <= 1U);
    TEST_ASSERT_EQUAL_UINT32(callsBefore + used, totalCalls(bus));
    if (rtc.isEepromBusy()) {
      TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                              static_cast<uint8_t>(st.code));
    } else {
      TEST_ASSERT_TRUE(st.ok());
    }
    ++polls;
    TEST_ASSERT_TRUE(polls < 16U);
  }

  TEST_ASSERT_EQUAL_UINT8(8u, polls);
  TEST_ASSERT_EQUAL_UINT32(1u, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.eepromWriteFailures());
  TEST_ASSERT_EQUAL_UINT32(1u, countWritesTo(bus, RV3032::cmd::REG_EE_COMMAND));
}

void test_eeprom_busy_timeout_restores_control_and_fails() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_CONTROL1] = 0x80;
  bus.regs[RV3032::cmd::REG_TEMP_LSB] = (1u << RV3032::cmd::EEPROM_BUSY_BIT);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makePersistentConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.setBackupSwitchMode(RV3032::BackupSwitchMode::Direct);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));

  uint8_t used = 0;
  for (uint8_t i = 0; i < 4; ++i) {
    st = rtc.pollEeprom(i, 1, used);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(1u, used);
  }

  TEST_ASSERT_EQUAL_HEX8(0x84, bus.regs[RV3032::cmd::REG_CONTROL1]);
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.regs[RV3032::cmd::REG_EE_COMMAND]);

  st = rtc.pollEeprom(4, 1, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(1u, used);
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.regs[RV3032::cmd::REG_EE_COMMAND]);

  st = rtc.pollEeprom(60, 1, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(1u, used);
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.regs[RV3032::cmd::REG_EE_COMMAND]);

  st = rtc.pollEeprom(61, 1, used);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(1u, used);
  TEST_ASSERT_FALSE(rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(1u, rtc.eepromWriteFailures());
  TEST_ASSERT_EQUAL_HEX8(0x80, bus.regs[RV3032::cmd::REG_CONTROL1]);
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.regs[RV3032::cmd::REG_EE_COMMAND]);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(rtc.getEepromStatus().code));
}

void test_read_time_decodes_valid_burst_and_masks_control_bits() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_SECONDS] = 0x81;
  bus.regs[RV3032::cmd::REG_MINUTES] = 0x59;
  bus.regs[RV3032::cmd::REG_HOURS] = 0x23;
  bus.regs[RV3032::cmd::REG_WEEKDAY] = 0x85;
  bus.regs[RV3032::cmd::REG_DATE] = 0x31;
  bus.regs[RV3032::cmd::REG_MONTH] = 0x12;
  bus.regs[RV3032::cmd::REG_YEAR] = 0x26;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  const uint32_t readsBefore = bus.readCalls;
  RV3032::DateTime dt;
  st = rtc.readTime(dt);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(readsBefore + 1u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT16(2026, dt.year);
  TEST_ASSERT_EQUAL_UINT8(12, dt.month);
  TEST_ASSERT_EQUAL_UINT8(31, dt.day);
  TEST_ASSERT_EQUAL_UINT8(23, dt.hour);
  TEST_ASSERT_EQUAL_UINT8(59, dt.minute);
  TEST_ASSERT_EQUAL_UINT8(1, dt.second);
  TEST_ASSERT_EQUAL_UINT8(5, dt.weekday);
}

void test_read_time_rejects_invalid_bcd_and_calendar() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_SECONDS] = 0x6A;
  bus.regs[RV3032::cmd::REG_MINUTES] = 0x00;
  bus.regs[RV3032::cmd::REG_HOURS] = 0x00;
  bus.regs[RV3032::cmd::REG_WEEKDAY] = 0x00;
  bus.regs[RV3032::cmd::REG_DATE] = 0x01;
  bus.regs[RV3032::cmd::REG_MONTH] = 0x01;
  bus.regs[RV3032::cmd::REG_YEAR] = 0x24;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::DateTime dt;
  st = rtc.readTime(dt);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                          static_cast<uint8_t>(st.code));

  bus.regs[RV3032::cmd::REG_SECONDS] = 0x00;
  bus.regs[RV3032::cmd::REG_DATE] = 0x29;
  bus.regs[RV3032::cmd::REG_MONTH] = 0x02;
  bus.regs[RV3032::cmd::REG_YEAR] = 0x23;
  st = rtc.readTime(dt);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                          static_cast<uint8_t>(st.code));
}

void test_set_time_rejects_invalid_without_i2c_and_writes_one_burst() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::DateTime dt;
  dt.year = 2024;
  dt.month = 13;
  dt.day = 1;
  dt.hour = 0;
  dt.minute = 0;
  dt.second = 0;

  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  st = rtc.setTime(dt);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  dt.year = 2024;
  dt.month = 2;
  dt.day = 29;
  dt.hour = 12;
  dt.minute = 34;
  dt.second = 56;
  st = rtc.setTime(dt);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_HEX8(0x56, bus.regs[RV3032::cmd::REG_SECONDS]);
  TEST_ASSERT_EQUAL_HEX8(0x34, bus.regs[RV3032::cmd::REG_MINUTES]);
  TEST_ASSERT_EQUAL_HEX8(0x12, bus.regs[RV3032::cmd::REG_HOURS]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::RV3032::computeWeekday(2024, 2, 29),
                         bus.regs[RV3032::cmd::REG_WEEKDAY]);
  TEST_ASSERT_EQUAL_HEX8(0x29, bus.regs[RV3032::cmd::REG_DATE]);
  TEST_ASSERT_EQUAL_HEX8(0x02, bus.regs[RV3032::cmd::REG_MONTH]);
  TEST_ASSERT_EQUAL_HEX8(0x24, bus.regs[RV3032::cmd::REG_YEAR]);
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

  st = rtc.setBackupSwitchMode(static_cast<RV3032::BackupSwitchMode>(9));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
}

void test_backup_mode_helpers_update_pmu_bits() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::Config cfg = makeConfig(bus);
  cfg.enableEepromWrites = false;
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());

  bus.regs[RV3032::cmd::REG_EEPROM_PMU] =
      static_cast<uint8_t>(RV3032::cmd::PMU_CLKOUT_DISABLE | RV3032::cmd::PMU_TRICKLE_MASK);

  st = rtc.setBackupSwitchMode(RV3032::BackupSwitchMode::Direct);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(RV3032::cmd::PMU_CLKOUT_DISABLE |
                                             RV3032::cmd::PMU_BSM_DIRECT |
                                             RV3032::cmd::PMU_TRICKLE_MASK),
                         bus.regs[RV3032::cmd::REG_EEPROM_PMU]);

  RV3032::BackupSwitchMode mode = RV3032::BackupSwitchMode::Off;
  st = rtc.getBackupSwitchMode(mode);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::BackupSwitchMode::Direct),
                          static_cast<uint8_t>(mode));

  st = rtc.setPrimaryBatteryBackupDefaults();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(RV3032::cmd::PMU_CLKOUT_DISABLE |
                                             RV3032::cmd::PMU_BSM_LEVEL),
                         bus.regs[RV3032::cmd::REG_EEPROM_PMU]);

  st = rtc.getBackupSwitchMode(mode);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::BackupSwitchMode::Level),
                          static_cast<uint8_t>(mode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::BackupSwitchMode::Level),
                          static_cast<uint8_t>(rtc.getSettings().backupMode));
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

void test_eeprom_enabled_success_waits_while_busy_then_restores() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makePersistentConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.setClkoutEnabled(false);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(rtc.isEepromBusy());

  rtc.tick(0);
  rtc.tick(1);
  rtc.tick(2);
  rtc.tick(3);
  rtc.tick(4);
  rtc.tick(5);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::EEPROM_CMD_UPDATE,
                         bus.regs[RV3032::cmd::REG_EE_COMMAND]);

  bus.regs[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_BUSY_BIT);
  rtc.tick(6);
  TEST_ASSERT_TRUE(rtc.getEepromStatus().inProgress());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.eepromWriteFailures());

  rtc.tick(14);
  TEST_ASSERT_TRUE(rtc.getEepromStatus().inProgress());

  bus.regs[RV3032::cmd::REG_TEMP_LSB] = 0;
  rtc.tick(15);
  TEST_ASSERT_TRUE(rtc.getEepromStatus().inProgress());
  rtc.tick(16);

  TEST_ASSERT_FALSE(rtc.isEepromBusy());
  TEST_ASSERT_TRUE(rtc.getEepromStatus().ok());
  TEST_ASSERT_EQUAL_UINT32(1u, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.eepromWriteFailures());
  TEST_ASSERT_EQUAL_HEX8(0x00, static_cast<uint8_t>(bus.regs[RV3032::cmd::REG_CONTROL1] &
                                                    (1u << RV3032::cmd::CTRL1_EERD_BIT)));
}

void test_eeprom_precommand_busy_timeout_restores_control() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_CONTROL1] = 0xA0;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makePersistentConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.setClkoutEnabled(false);
  TEST_ASSERT_TRUE(st.inProgress());

  rtc.tick(0);
  rtc.tick(1);
  rtc.tick(2);
  rtc.tick(3);
  bus.regs[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_BUSY_BIT);
  rtc.tick(4);
  TEST_ASSERT_TRUE(rtc.getEepromStatus().inProgress());
  rtc.tick(52);
  TEST_ASSERT_TRUE(rtc.getEepromStatus().inProgress());
  rtc.tick(53);
  TEST_ASSERT_TRUE(rtc.getEepromStatus().inProgress());
  rtc.tick(54);

  st = rtc.getEepromStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(1u, rtc.eepromWriteFailures());
  TEST_ASSERT_EQUAL_HEX8(0xA0, bus.regs[RV3032::cmd::REG_CONTROL1]);
  TEST_ASSERT_EQUAL_UINT32(0u, countWritesTo(bus, RV3032::cmd::REG_EE_COMMAND));
}

void test_eeprom_restore_after_write_failure() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_CONTROL1] = 0xA0;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makePersistentConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.setClkoutEnabled(false);
  TEST_ASSERT_TRUE(st.inProgress());

  rtc.tick(0);
  rtc.tick(1);
  rtc.tick(2);
  bus.nextWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA,
                                              "write data failed", -44);
  rtc.tick(3);
  TEST_ASSERT_TRUE(rtc.getEepromStatus().inProgress());
  rtc.tick(4);

  st = rtc.getEepromStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-44, st.detail);
  TEST_ASSERT_FALSE(rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(1u, rtc.eepromWriteFailures());
  TEST_ASSERT_EQUAL_HEX8(0xA0, bus.regs[RV3032::cmd::REG_CONTROL1]);
}

void test_lifecycle_refuses_to_reset_active_eeprom_work() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makePersistentConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.setClkoutEnabled(false);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(rtc.isInitialized());
  TEST_ASSERT_TRUE(rtc.isEepromBusy());

  st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(rtc.isInitialized());
  TEST_ASSERT_TRUE(rtc.isEepromBusy());

  rtc.end();
  TEST_ASSERT_TRUE(rtc.isInitialized());
  TEST_ASSERT_TRUE(rtc.isEepromBusy());
}

void test_same_value_eeprom_setter_reports_pending_or_failed_persistence() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makePersistentConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  static constexpr float kOffsetStepPpm = 0.2384f;
  st = rtc.setOffsetPpm(kOffsetStepPpm);
  TEST_ASSERT_TRUE(st.inProgress());

  st = rtc.setOffsetPpm(kOffsetStepPpm);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::IN_PROGRESS),
                          static_cast<uint8_t>(st.code));

  rtc.tick(0);
  rtc.tick(1);
  rtc.tick(2);
  bus.nextWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA,
                                              "write data failed", -45);
  rtc.tick(3);
  rtc.tick(4);

  st = rtc.getEepromStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(rtc.isEepromBusy());

  st = rtc.setOffsetPpm(kOffsetStepPpm);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-45, st.detail);
}

void test_eeprom_failure_stops_queue_and_preserves_first_error() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makePersistentConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  static constexpr float kOffsetStepPpm = 0.2384f;
  st = rtc.setOffsetPpm(kOffsetStepPpm);
  TEST_ASSERT_TRUE(st.inProgress());
  st = rtc.setOffsetPpm(kOffsetStepPpm * 2.0f);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_EQUAL_UINT8(1u, rtc.eepromQueueDepth());

  rtc.tick(0);
  rtc.tick(1);
  rtc.tick(2);
  bus.nextWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA,
                                              "first write failed", -46);
  rtc.tick(3);
  rtc.tick(4);

  st = rtc.getEepromStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-46, st.detail);
  TEST_ASSERT_FALSE(rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_UINT8(0u, rtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.eepromWriteCount());
  TEST_ASSERT_EQUAL_UINT32(1u, rtc.eepromWriteFailures());
}

void test_eeprom_cleanup_can_finish_after_offline_failure() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_CONTROL1] = 0xA0;

  RV3032::Config cfg = makePersistentConfig(bus);
  cfg.offlineThreshold = 1;
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());

  st = rtc.setClkoutEnabled(false);
  TEST_ASSERT_TRUE(st.inProgress());

  rtc.tick(0);
  rtc.tick(1);
  rtc.tick(2);
  bus.nextWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA,
                                              "write data failed", -47);
  rtc.tick(3);
  TEST_ASSERT_TRUE(rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::OFFLINE),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_HEX8(0xA4, bus.regs[RV3032::cmd::REG_CONTROL1]);

  rtc.tick(4);
  st = rtc.getEepromStatus();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(rtc.isEepromBusy());
  TEST_ASSERT_EQUAL_HEX8(0xA0, bus.regs[RV3032::cmd::REG_CONTROL1]);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::OFFLINE),
                          static_cast<uint8_t>(rtc.state()));
}

void test_eeprom_queue_full_does_not_change_ram_shadow() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makePersistentConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  static constexpr float kOffsetStepPpm = 0.2384f;
  st = rtc.setOffsetPpm(kOffsetStepPpm);
  TEST_ASSERT_TRUE(st.inProgress());

  for (uint8_t i = 2; i <= 9; ++i) {
    st = rtc.setOffsetPpm(kOffsetStepPpm * static_cast<float>(i));
    TEST_ASSERT_TRUE(st.inProgress());
  }
  TEST_ASSERT_EQUAL_UINT8(8u, rtc.eepromQueueDepth());
  const uint32_t writesBeforeFull = bus.writeCalls;
  const uint8_t lastAccepted = bus.regs[RV3032::cmd::REG_EEPROM_OFFSET];

  st = rtc.setOffsetPpm(kOffsetStepPpm * 10.0f);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::QUEUE_FULL),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(8u, rtc.eepromQueueDepth());
  TEST_ASSERT_EQUAL_UINT32(writesBeforeFull, bus.writeCalls);
  TEST_ASSERT_EQUAL_HEX8(lastAccepted, bus.regs[RV3032::cmd::REG_EEPROM_OFFSET]);
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

void test_read_timestamp_rejects_invalid_calendar() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_TS_TLOW_COUNT] = 0x01;
  bus.regs[RV3032::cmd::REG_TS_TLOW_SECONDS] = 0x00;
  bus.regs[RV3032::cmd::REG_TS_TLOW_MINUTES] = 0x00;
  bus.regs[RV3032::cmd::REG_TS_TLOW_HOURS] = 0x00;
  bus.regs[RV3032::cmd::REG_TS_TLOW_DATE] = 0x29;
  bus.regs[RV3032::cmd::REG_TS_TLOW_MONTH] = 0x02;
  bus.regs[RV3032::cmd::REG_TS_TLOW_YEAR] = 0x23;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::Timestamp ts;
  st = rtc.readTimestamp(RV3032::TimestampSource::TLow, ts);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                          static_cast<uint8_t>(st.code));
}

void test_clear_flag_helpers_skip_writes_when_flags_are_absent() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(1u << RV3032::cmd::STATUS_THF_BIT);
  bus.regs[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_BUSY_BIT);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  const uint32_t writesBeforeAlarm = bus.writeCalls;
  st = rtc.clearAlarmFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(writesBeforeAlarm, bus.writeCalls);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::STATUS_THF_BIT),
                         bus.regs[RV3032::cmd::REG_STATUS]);

  const uint32_t writesBeforePorf = bus.writeCalls;
  st = rtc.clearPowerOnResetFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(writesBeforePorf, bus.writeCalls);

  const uint32_t writesBeforeBsf = bus.writeCalls;
  st = rtc.clearBackupSwitchFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(writesBeforeBsf, bus.writeCalls);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_BUSY_BIT),
                         bus.regs[RV3032::cmd::REG_TEMP_LSB]);
}

void test_clear_flag_helpers_write_only_target_flag_when_present() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  bus.regs[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_AF_BIT) |
      (1u << RV3032::cmd::STATUS_PORF_BIT));
  st = rtc.clearAlarmFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS,
                         bus.writeRegLog[bus.writeLogCount - 1]);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::STATUS_PORF_BIT),
                         bus.writeValueLog[bus.writeLogCount - 1]);

  bus.regs[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_PORF_BIT) |
      (1u << RV3032::cmd::STATUS_VLF_BIT));
  st = rtc.clearPowerOnResetFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS,
                         bus.writeRegLog[bus.writeLogCount - 1]);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::STATUS_VLF_BIT),
                         bus.writeValueLog[bus.writeLogCount - 1]);

  bus.regs[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(
      (1u << RV3032::cmd::TEMP_BSF_BIT));
  st = rtc.clearBackupSwitchFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TEMP_LSB,
                         bus.writeRegLog[bus.writeLogCount - 1]);
  TEST_ASSERT_EQUAL_HEX8(0u, bus.writeValueLog[bus.writeLogCount - 1]);
}

void test_clear_flag_helpers_reject_implicit_temperature_and_support_flag_clears() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  bus.regs[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_AF_BIT) |
      (1u << RV3032::cmd::STATUS_THF_BIT));
  const uint32_t writesBeforeTempGuard = bus.writeCalls;
  st = rtc.clearAlarmFlag();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBeforeTempGuard, bus.writeCalls);

  st = rtc.clearStatus(static_cast<uint8_t>(
      (1u << RV3032::cmd::STATUS_AF_BIT) |
      (1u << RV3032::cmd::STATUS_THF_BIT)));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS,
                         bus.writeRegLog[bus.writeLogCount - 1]);
  TEST_ASSERT_EQUAL_HEX8(0u, bus.writeValueLog[bus.writeLogCount - 1]);

  bus.regs[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(
      (1u << RV3032::cmd::TEMP_BSF_BIT) |
      (1u << RV3032::cmd::EEPROM_BUSY_BIT));
  const uint32_t writesBeforeBsfGuard = bus.writeCalls;
  st = rtc.clearBackupSwitchFlag();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBeforeBsfGuard, bus.writeCalls);
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
  RUN_TEST(test_begin_rejects_invalid_eeprom_enabled_timeouts);
  RUN_TEST(test_begin_probe_failure_preserves_transport_errors);
  RUN_TEST(test_invalid_begin_after_success_resets_without_i2c);
  RUN_TEST(test_get_settings_snapshot);
  RUN_TEST(test_runtime_i2c_after_begin_updates_health);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_recover_failure_updates_health_once);
  RUN_TEST(test_offline_latches_public_read_without_i2c);
  RUN_TEST(test_failed_recover_from_offline_keeps_latch_after_intermediate_success);
  RUN_TEST(test_successful_recover_from_offline_unlatches_driver);
  RUN_TEST(test_transport_validation_status_does_not_update_health);
  RUN_TEST(test_runtime_i2c_failure_codes_update_health);
  RUN_TEST(test_set_timer_preserves_reserved_high_bits);
  RUN_TEST(test_budgeted_set_timer_job_honors_instruction_budget);
  RUN_TEST(test_budgeted_register_update_job_honors_instruction_budget);
  RUN_TEST(test_budgeted_user_ram_write_job_chunks_large_write);
  RUN_TEST(test_budgeted_job_aborts_when_driver_is_offline);
  RUN_TEST(test_default_config_keeps_eeprom_persistence_disabled);
  RUN_TEST(test_eeprom_poll_uses_one_instruction_per_call);
  RUN_TEST(test_eeprom_busy_timeout_restores_control_and_fails);
  RUN_TEST(test_read_time_decodes_valid_burst_and_masks_control_bits);
  RUN_TEST(test_read_time_rejects_invalid_bcd_and_calendar);
  RUN_TEST(test_set_time_rejects_invalid_without_i2c_and_writes_one_burst);
  RUN_TEST(test_invalid_runtime_params_are_rejected);
  RUN_TEST(test_backup_mode_helpers_update_pmu_bits);
  RUN_TEST(test_get_alarm_config_rejects_invalid_bcd);
  RUN_TEST(test_get_alarm_config_tolerates_por_inactive_date_alarm);
  RUN_TEST(test_get_alarm_config_tolerates_disabled_field_garbage);
  RUN_TEST(test_eeprom_enabled_success_waits_while_busy_then_restores);
  RUN_TEST(test_eeprom_precommand_busy_timeout_restores_control);
  RUN_TEST(test_eeprom_restore_after_write_failure);
  RUN_TEST(test_lifecycle_refuses_to_reset_active_eeprom_work);
  RUN_TEST(test_same_value_eeprom_setter_reports_pending_or_failed_persistence);
  RUN_TEST(test_eeprom_failure_stops_queue_and_preserves_first_error);
  RUN_TEST(test_eeprom_cleanup_can_finish_after_offline_failure);
  RUN_TEST(test_eeprom_queue_full_does_not_change_ram_shadow);
  RUN_TEST(test_read_timestamp_decodes_evi_and_reset_sets_control_bit);
  RUN_TEST(test_read_timestamp_decodes_tlow_without_hundredths);
  RUN_TEST(test_read_timestamp_rejects_invalid_bcd);
  RUN_TEST(test_read_timestamp_rejects_invalid_calendar);
  RUN_TEST(test_clear_flag_helpers_skip_writes_when_flags_are_absent);
  RUN_TEST(test_clear_flag_helpers_write_only_target_flag_when_present);
  RUN_TEST(test_clear_flag_helpers_reject_implicit_temperature_and_support_flag_clears);
  RUN_TEST(test_user_ram_helpers_round_trip_and_reject_invalid_ranges);
  RUN_TEST(test_public_block_register_access);
  RUN_TEST(test_public_block_register_access_rejects_invalid_params);
  RUN_TEST(test_public_block_register_access_rejects_invalid_ranges_without_i2c);
  return UNITY_END();
}
