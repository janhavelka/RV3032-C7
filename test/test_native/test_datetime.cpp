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
  bool failNextRead = false;
  bool failNextWrite = false;
};

void resetBus(FakeI2cBus& bus) {
  std::memset(bus.regs, 0, sizeof(bus.regs));
  bus.failNextRead = false;
  bus.failNextWrite = false;
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
  if (!data || len == 0) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "write invalid");
  }

  FakeI2cBus* bus = static_cast<FakeI2cBus*>(user);
  if (bus->failNextWrite) {
    bus->failNextWrite = false;
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR, "forced write failure", -2);
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
  if (!tx || txLen != 1 || !rx || rxLen == 0) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "read invalid");
  }

  FakeI2cBus* bus = static_cast<FakeI2cBus*>(user);
  if (bus->failNextRead) {
    bus->failNextRead = false;
    return RV3032::Status::Error(RV3032::Err::I2C_ERROR, "forced read failure", -3);
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

void test_probe_failure_does_not_update_health() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());

  bus.failNextRead = true;
  st = rtc.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
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

  bus.failNextRead = true;
  st = rtc.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(1, rtc.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::DEGRADED),
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

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_bcd_roundtrip);
  RUN_TEST(test_compute_weekday);
  RUN_TEST(test_unix_roundtrip_epoch_2000);
  RUN_TEST(test_unix_leap_day);
  RUN_TEST(test_unix_out_of_rtc_range);
  RUN_TEST(test_invalid_date);
  RUN_TEST(test_begin_rejects_non_default_address);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_recover_failure_updates_health_once);
  RUN_TEST(test_set_timer_preserves_reserved_high_bits);
  RUN_TEST(test_invalid_runtime_params_are_rejected);
  RUN_TEST(test_get_alarm_config_rejects_invalid_bcd);
  return UNITY_END();
}
