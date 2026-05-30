#include <stddef.h>
#include <stdint.h>
#include <cstring>
#include <math.h>
#include <type_traits>
#include <unity.h>

#include "RV3032/CommandTable.h"
#include "RV3032/RV3032.h"

namespace {

struct I2cLogEntry {
  uint8_t reg = 0;
  size_t len = 0;
  uint8_t data[16] = {};
  RV3032::Status status = RV3032::Status::Ok();
  char op = 0;
};

struct FakeI2cBus {
  uint8_t regs[256];
  uint8_t userEeprom[RV3032::cmd::USER_EEPROM_SIZE];
  RV3032::Status nextReadStatus = RV3032::Status::Ok();
  RV3032::Status nextWriteStatus = RV3032::Status::Ok();
  uint32_t failReadCall = 0;
  uint32_t failWriteCall = 0;
  RV3032::Status failReadStatus = RV3032::Status::Ok();
  RV3032::Status failWriteStatus = RV3032::Status::Ok();
  uint32_t readCalls = 0;
  uint32_t writeCalls = 0;
  I2cLogEntry readLog[64];
  I2cLogEntry writeLog[64];
  I2cLogEntry txnLog[192];
  uint32_t readLogCount = 0;
  uint32_t writeLogCount = 0;
  uint32_t txnLogCount = 0;
  uint8_t tempLsbReadScript[160];
  uint32_t tempLsbReadScriptCount = 0;
  uint32_t tempLsbReadScriptIndex = 0;
  bool eepromWriteSetsEef = false;
};

uint32_t fakeNowMs(void*) {
  return 222u;
}

void resetBus(FakeI2cBus& bus) {
  std::memset(bus.regs, 0, sizeof(bus.regs));
  std::memset(bus.userEeprom, 0, sizeof(bus.userEeprom));
  bus.nextReadStatus = RV3032::Status::Ok();
  bus.nextWriteStatus = RV3032::Status::Ok();
  bus.failReadCall = 0;
  bus.failWriteCall = 0;
  bus.failReadStatus = RV3032::Status::Ok();
  bus.failWriteStatus = RV3032::Status::Ok();
  bus.readCalls = 0;
  bus.writeCalls = 0;
  bus.readLogCount = 0;
  bus.writeLogCount = 0;
  bus.txnLogCount = 0;
  bus.tempLsbReadScriptCount = 0;
  bus.tempLsbReadScriptIndex = 0;
  bus.eepromWriteSetsEef = false;
  std::memset(bus.readLog, 0, sizeof(bus.readLog));
  std::memset(bus.writeLog, 0, sizeof(bus.writeLog));
  std::memset(bus.txnLog, 0, sizeof(bus.txnLog));
  std::memset(bus.tempLsbReadScript, 0, sizeof(bus.tempLsbReadScript));
  bus.regs[RV3032::cmd::REG_STATUS] = 0x00;
  bus.regs[RV3032::cmd::REG_EEPROM_PMU] = RV3032::cmd::PMU_BSM_LEVEL;
  bus.regs[RV3032::cmd::REG_TIMER_HIGH] = 0xA0;
}

void clearHistory(FakeI2cBus& bus) {
  bus.nextReadStatus = RV3032::Status::Ok();
  bus.nextWriteStatus = RV3032::Status::Ok();
  bus.failReadCall = 0;
  bus.failWriteCall = 0;
  bus.failReadStatus = RV3032::Status::Ok();
  bus.failWriteStatus = RV3032::Status::Ok();
  bus.readCalls = 0;
  bus.writeCalls = 0;
  bus.readLogCount = 0;
  bus.writeLogCount = 0;
  bus.txnLogCount = 0;
  bus.tempLsbReadScriptCount = 0;
  bus.tempLsbReadScriptIndex = 0;
  bus.eepromWriteSetsEef = false;
  std::memset(bus.readLog, 0, sizeof(bus.readLog));
  std::memset(bus.writeLog, 0, sizeof(bus.writeLog));
  std::memset(bus.txnLog, 0, sizeof(bus.txnLog));
  std::memset(bus.tempLsbReadScript, 0, sizeof(bus.tempLsbReadScript));
}

void scriptTempLsbReads(FakeI2cBus& bus, const uint8_t* values, size_t len) {
  bus.tempLsbReadScriptCount =
      static_cast<uint32_t>(len > sizeof(bus.tempLsbReadScript) ? sizeof(bus.tempLsbReadScript) : len);
  bus.tempLsbReadScriptIndex = 0;
  if (bus.tempLsbReadScriptCount > 0) {
    std::memcpy(bus.tempLsbReadScript, values, bus.tempLsbReadScriptCount);
  }
}

void logTxn(FakeI2cBus& bus, char op, uint8_t reg, size_t len,
            const uint8_t* data, RV3032::Status st) {
  if (bus.txnLogCount >= (sizeof(bus.txnLog) / sizeof(bus.txnLog[0]))) {
    return;
  }
  I2cLogEntry& entry = bus.txnLog[bus.txnLogCount++];
  entry.op = op;
  entry.reg = reg;
  entry.len = len;
  const size_t copyLen = len > sizeof(entry.data) ? sizeof(entry.data) : len;
  if (data && copyLen > 0) {
    std::memcpy(entry.data, data, copyLen);
  }
  entry.status = st;
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
  RV3032::Status st = RV3032::Status::Ok();
  if (bus->failWriteCall != 0 && bus->writeCalls == bus->failWriteCall) {
    st = bus->failWriteStatus.ok()
             ? RV3032::Status::Error(RV3032::Err::I2C_ERROR, "forced indexed write")
             : bus->failWriteStatus;
  } else if (!bus->nextWriteStatus.ok()) {
    st = bus->nextWriteStatus;
    bus->nextWriteStatus = RV3032::Status::Ok();
  }

  if (bus->writeLogCount < (sizeof(bus->writeLog) / sizeof(bus->writeLog[0]))) {
    I2cLogEntry& entry = bus->writeLog[bus->writeLogCount++];
    entry.op = 'W';
    entry.reg = reg;
    entry.len = (len > 0) ? (len - 1) : 0;
    const size_t copyLen = entry.len > sizeof(entry.data) ? sizeof(entry.data) : entry.len;
    if (copyLen > 0) {
      std::memcpy(entry.data, &data[1], copyLen);
    }
    entry.status = st;
  }
  logTxn(*bus, 'W', reg, (len > 0) ? (len - 1) : 0, (len > 1) ? &data[1] : nullptr, st);

  if (!st.ok()) {
    return st;
  }

  for (size_t i = 1; i < len; ++i) {
    bus->regs[static_cast<uint8_t>(reg + static_cast<uint8_t>(i - 1))] = data[i];
  }
  if (reg == RV3032::cmd::REG_EE_COMMAND && len >= 2) {
    const uint8_t command = data[1];
    const uint8_t eeAddr = bus->regs[RV3032::cmd::REG_EE_ADDRESS];
    if (eeAddr >= RV3032::cmd::USER_EEPROM_START &&
        eeAddr <= RV3032::cmd::USER_EEPROM_END) {
      const uint8_t offset = static_cast<uint8_t>(eeAddr - RV3032::cmd::USER_EEPROM_START);
      if (command == RV3032::cmd::EEPROM_CMD_WRITE_BYTE) {
        bus->userEeprom[offset] = bus->regs[RV3032::cmd::REG_EE_DATA];
        if (bus->eepromWriteSetsEef) {
          bus->regs[RV3032::cmd::REG_TEMP_LSB] =
              static_cast<uint8_t>(bus->regs[RV3032::cmd::REG_TEMP_LSB] |
                                   (1u << RV3032::cmd::EEPROM_ERROR_BIT));
        }
      } else if (command == RV3032::cmd::EEPROM_CMD_READ_BYTE) {
        bus->regs[RV3032::cmd::REG_EE_DATA] = bus->userEeprom[offset];
      }
    }
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

  const uint8_t reg = tx[0];
  RV3032::Status st = RV3032::Status::Ok();
  if (bus->failReadCall != 0 && bus->readCalls == bus->failReadCall) {
    st = bus->failReadStatus.ok()
             ? RV3032::Status::Error(RV3032::Err::I2C_ERROR, "forced indexed read")
             : bus->failReadStatus;
  } else if (!bus->nextReadStatus.ok()) {
    st = bus->nextReadStatus;
    bus->nextReadStatus = RV3032::Status::Ok();
  }

  if (bus->readLogCount < (sizeof(bus->readLog) / sizeof(bus->readLog[0]))) {
    I2cLogEntry& entry = bus->readLog[bus->readLogCount++];
    entry.op = 'R';
    entry.reg = reg;
    entry.len = rxLen;
    entry.status = st;
  }
  logTxn(*bus, 'R', reg, rxLen, nullptr, st);

  if (!st.ok()) {
    return st;
  }

  for (size_t i = 0; i < rxLen; ++i) {
    const uint8_t sourceReg = static_cast<uint8_t>(reg + static_cast<uint8_t>(i));
    if (sourceReg == RV3032::cmd::REG_TEMP_LSB &&
        bus->tempLsbReadScriptIndex < bus->tempLsbReadScriptCount) {
      rx[i] = bus->tempLsbReadScript[bus->tempLsbReadScriptIndex++];
      bus->regs[sourceReg] = rx[i];
    } else {
      rx[i] = bus->regs[sourceReg];
    }
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

void seedTimeRegisters(FakeI2cBus& bus, uint16_t year, uint8_t month, uint8_t day,
                       uint8_t hour, uint8_t minute, uint8_t second, uint8_t weekday) {
  bus.regs[RV3032::cmd::REG_SECONDS] = RV3032::RV3032::binaryToBcd(second);
  bus.regs[RV3032::cmd::REG_MINUTES] = RV3032::RV3032::binaryToBcd(minute);
  bus.regs[RV3032::cmd::REG_HOURS] = RV3032::RV3032::binaryToBcd(hour);
  bus.regs[RV3032::cmd::REG_WEEKDAY] = static_cast<uint8_t>(weekday & 0x07);
  bus.regs[RV3032::cmd::REG_DATE] = RV3032::RV3032::binaryToBcd(day);
  bus.regs[RV3032::cmd::REG_MONTH] = RV3032::RV3032::binaryToBcd(month);
  bus.regs[RV3032::cmd::REG_YEAR] =
      RV3032::RV3032::binaryToBcd(static_cast<uint8_t>(year % 100));
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

void test_driver_is_not_copyable_or_movable() {
  static_assert(!std::is_copy_constructible<RV3032::RV3032>::value,
                "RV3032 must not be copy constructible");
  static_assert(!std::is_copy_assignable<RV3032::RV3032>::value,
                "RV3032 must not be copy assignable");
  static_assert(!std::is_move_constructible<RV3032::RV3032>::value,
                "RV3032 must not be move constructible");
  static_assert(!std::is_move_assignable<RV3032::RV3032>::value,
                "RV3032 must not be move assignable");
  TEST_ASSERT_TRUE(true);
}

void test_control2_interrupt_bits_match_register_reference() {
  static_assert(RV3032::cmd::CTRL2_STOP_BIT == 0,
                "Control2 STOP bit must match the RV3032-C7 register reference");
  static_assert(RV3032::cmd::CTRL2_GP1_BIT == 1,
                "Control2 GP1 bit must match the RV3032-C7 register reference");
  static_assert(RV3032::cmd::CTRL2_EIE_BIT == 2,
                "Control2 EIE bit must match the RV3032-C7 register reference");
  static_assert(RV3032::cmd::CTRL2_AIE_BIT == 3,
                "Control2 AIE bit must match the RV3032-C7 register reference");
  static_assert(RV3032::cmd::CTRL2_TIE_BIT == 4,
                "Control2 TIE bit must match the RV3032-C7 register reference");
  static_assert(RV3032::cmd::CTRL2_UIE_BIT == 5,
                "Control2 UIE bit must match the RV3032-C7 register reference");
  static_assert(RV3032::cmd::CTRL2_CLKIE_BIT == 6,
                "Control2 CLKIE bit must match the RV3032-C7 register reference");
  static_assert(RV3032::cmd::EVI_ESYN_BIT == 0,
                "EVI Control ESYN bit must match the RV3032-C7 register reference");
  static_assert(RV3032::cmd::CTRL3_BSIE_BIT == 4,
                "Control3 BSIE bit must match the RV3032-C7 register reference");
  static_assert(RV3032::cmd::CLOCK_INT_CTIE_BIT == 3,
                "Clock interrupt timer mask bit must match the register reference");
  static_assert(RV3032::cmd::CLOCK_INT_CAIE_BIT == 4,
                "Clock interrupt alarm mask bit must match the register reference");
  static_assert(RV3032::cmd::CLOCK_INT_CEIE_BIT == 5,
                "Clock interrupt EVI mask bit must match the register reference");
  static_assert(RV3032::cmd::EEPROM_CMD_UPDATE_ALL == 0x11,
                "EEPROM update-all command must match the manual");
  static_assert(RV3032::cmd::EEPROM_CMD_REFRESH_ALL == 0x12,
                "EEPROM refresh-all command must match the manual");
  static_assert(RV3032::cmd::EEPROM_CMD_WRITE_BYTE == 0x21,
                "EEPROM byte-write command must match the manual");
  static_assert(RV3032::cmd::EEPROM_CMD_READ_BYTE == 0x22,
                "EEPROM byte-read command must match the manual");
  static_assert(RV3032::cmd::USER_EEPROM_SIZE == 32,
                "User EEPROM size must match the RV3032-C7 register reference");
  TEST_ASSERT_TRUE(true);
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

void test_read_time_decodes_valid_bcd_and_uses_single_burst() {
  FakeI2cBus bus;
  resetBus(bus);
  seedTimeRegisters(bus, 2099, 12, 31, 23, 58, 59, 6);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  RV3032::DateTime out;
  st = rtc.readTime(out);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(2099, out.year);
  TEST_ASSERT_EQUAL_UINT8(12, out.month);
  TEST_ASSERT_EQUAL_UINT8(31, out.day);
  TEST_ASSERT_EQUAL_UINT8(23, out.hour);
  TEST_ASSERT_EQUAL_UINT8(58, out.minute);
  TEST_ASSERT_EQUAL_UINT8(59, out.second);
  TEST_ASSERT_EQUAL_UINT8(6, out.weekday);
  TEST_ASSERT_EQUAL_UINT32(1u, bus.readLogCount);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_SECONDS, bus.readLog[0].reg);
  TEST_ASSERT_EQUAL_UINT(7u, bus.readLog[0].len);
}

void test_read_time_accepts_leap_days_and_rejects_non_leap_feb29() {
  const uint16_t leapYears[] = {2000, 2024, 2096};
  for (size_t i = 0; i < (sizeof(leapYears) / sizeof(leapYears[0])); ++i) {
    FakeI2cBus bus;
    resetBus(bus);
    seedTimeRegisters(bus, leapYears[i], 2, 29, 1, 2, 3, 2);

    RV3032::RV3032 rtc;
    RV3032::Status st = rtc.begin(makeConfig(bus));
    TEST_ASSERT_TRUE(st.ok());

    RV3032::DateTime out;
    st = rtc.readTime(out);
    TEST_ASSERT_TRUE(st.ok());
    TEST_ASSERT_EQUAL_UINT16(leapYears[i], out.year);
    TEST_ASSERT_EQUAL_UINT8(2, out.month);
    TEST_ASSERT_EQUAL_UINT8(29, out.day);
  }

  FakeI2cBus bus;
  resetBus(bus);
  seedTimeRegisters(bus, 2023, 2, 29, 1, 2, 3, 2);
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  RV3032::DateTime out;
  out.year = 2040;
  st = rtc.readTime(out);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT16(2040, out.year);
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
}

void test_read_time_rejects_invalid_bcd_and_all_ff() {
  struct Case {
    uint8_t index;
    uint8_t value;
  };
  const Case cases[] = {
      {0, 0x6A},
      {1, 0x6A},
      {2, 0x2A},
      {4, 0x3A},
      {5, 0x1A},
      {6, 0xFA},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); ++i) {
    FakeI2cBus bus;
    resetBus(bus);
    seedTimeRegisters(bus, 2024, 3, 14, 15, 9, 26, 4);
    bus.regs[static_cast<uint8_t>(RV3032::cmd::REG_SECONDS + cases[i].index)] = cases[i].value;

    RV3032::RV3032 rtc;
    RV3032::Status st = rtc.begin(makeConfig(bus));
    TEST_ASSERT_TRUE(st.ok());
    RV3032::DateTime out;
    out.year = 2030;
    st = rtc.readTime(out);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT16(2030, out.year);
    TEST_ASSERT_EQUAL_UINT32(0u, rtc.totalFailures());
  }

  FakeI2cBus bus;
  resetBus(bus);
  for (uint8_t i = 0; i < 7; ++i) {
    bus.regs[static_cast<uint8_t>(RV3032::cmd::REG_SECONDS + i)] = 0xFF;
  }
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  RV3032::DateTime out;
  st = rtc.readTime(out);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, rtc.totalFailures());
}

void test_read_time_rejects_bcd_valid_out_of_range_values() {
  struct Case {
    uint8_t reg;
    uint8_t value;
  };
  const Case cases[] = {
      {RV3032::cmd::REG_SECONDS, 0x60},
      {RV3032::cmd::REG_MINUTES, 0x60},
      {RV3032::cmd::REG_HOURS, 0x24},
      {RV3032::cmd::REG_DATE, 0x00},
      {RV3032::cmd::REG_DATE, 0x32},
      {RV3032::cmd::REG_MONTH, 0x00},
      {RV3032::cmd::REG_MONTH, 0x13},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); ++i) {
    FakeI2cBus bus;
    resetBus(bus);
    seedTimeRegisters(bus, 2024, 4, 30, 12, 0, 0, 2);
    bus.regs[cases[i].reg] = cases[i].value;

    RV3032::RV3032 rtc;
    RV3032::Status st = rtc.begin(makeConfig(bus));
    TEST_ASSERT_TRUE(st.ok());
    RV3032::DateTime out;
    st = rtc.readTime(out);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, rtc.totalFailures());
  }

  FakeI2cBus bus;
  resetBus(bus);
  seedTimeRegisters(bus, 2024, 4, 31, 12, 0, 0, 3);
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  RV3032::DateTime out;
  st = rtc.readTime(out);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                          static_cast<uint8_t>(st.code));
}

void test_set_time_uses_stop_sequence_and_preserves_control2_bits() {
  FakeI2cBus bus;
  resetBus(bus);
  const uint8_t original = static_cast<uint8_t>((1u << RV3032::cmd::CTRL2_CLKIE_BIT) |
                                               (1u << RV3032::cmd::CTRL2_UIE_BIT) |
                                               (1u << RV3032::cmd::CTRL2_TIE_BIT) |
                                               (1u << RV3032::cmd::CTRL2_AIE_BIT) |
                                               (1u << RV3032::cmd::CTRL2_EIE_BIT) |
                                               (1u << RV3032::cmd::CTRL2_GP1_BIT));
  bus.regs[RV3032::cmd::REG_CONTROL2] = original;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  RV3032::DateTime dt = {2024, 2, 29, 12, 34, 56, 0};
  st = rtc.setTime(dt);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1u, bus.readLogCount);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL2, bus.readLog[0].reg);
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeLogCount);

  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL2, bus.writeLog[0].reg);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(original | (1u << RV3032::cmd::CTRL2_STOP_BIT)),
                         bus.writeLog[0].data[0]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_SECONDS, bus.writeLog[1].reg);
  TEST_ASSERT_EQUAL_UINT(7u, bus.writeLog[1].len);
  TEST_ASSERT_EQUAL_HEX8(0x56, bus.writeLog[1].data[0]);
  TEST_ASSERT_EQUAL_HEX8(0x34, bus.writeLog[1].data[1]);
  TEST_ASSERT_EQUAL_HEX8(0x12, bus.writeLog[1].data[2]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::RV3032::computeWeekday(2024, 2, 29), bus.writeLog[1].data[3]);
  TEST_ASSERT_EQUAL_HEX8(0x29, bus.writeLog[1].data[4]);
  TEST_ASSERT_EQUAL_HEX8(0x02, bus.writeLog[1].data[5]);
  TEST_ASSERT_EQUAL_HEX8(0x24, bus.writeLog[1].data[6]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL2, bus.writeLog[2].reg);
  TEST_ASSERT_EQUAL_HEX8(original, bus.writeLog[2].data[0]);
  TEST_ASSERT_EQUAL_HEX8(original, bus.regs[RV3032::cmd::REG_CONTROL2]);
}

void test_set_time_rejects_invalid_datetime_without_i2c_or_uncertain_state() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  RV3032::DateTime cases[4] = {};
  cases[0] = {2023, 2, 29, 0, 0, 0, 0};
  cases[1] = {1999, 1, 1, 0, 0, 0, 0};
  cases[2] = {2100, 1, 1, 0, 0, 0, 0};
  cases[3] = {2024, 1, 1, 0, 0, 60, 0};

  for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); ++i) {
    clearHistory(bus);
    st = rtc.setTime(cases[i]);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_DATETIME),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readLogCount);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeLogCount);
    RV3032::SettingsSnapshot snap;
    TEST_ASSERT_TRUE(rtc.getSettings(snap).ok());
    TEST_ASSERT_FALSE(snap.clockCalendarUncertain);
  }
}

void test_set_time_partial_failure_marks_clock_calendar_uncertain() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  bus.failWriteCall = 2;
  bus.failWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA,
                                              "forced time burst fail", -41);

  RV3032::DateTime dt = {2024, 5, 6, 7, 8, 9, 0};
  st = rtc.setTime(dt);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-41, st.detail);
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeLogCount);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL2, bus.writeLog[0].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_SECONDS, bus.writeLog[1].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL2, bus.writeLog[2].reg);
  TEST_ASSERT_TRUE(bus.writeLog[2].status.ok());

  RV3032::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(rtc.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.clockCalendarUncertain);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(snap.clockCalendarLastStatus.code));
  TEST_ASSERT_EQUAL_INT32(-41, snap.clockCalendarLastStatus.detail);
}

void test_set_time_release_stop_failure_marks_uncertain_and_leaves_error_visible() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  bus.failWriteCall = 3;
  bus.failWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_BUS,
                                              "forced stop release fail", -42);

  RV3032::DateTime dt = {2024, 5, 6, 7, 8, 9, 0};
  st = rtc.setTime(dt);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-42, st.detail);
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeLogCount);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::CTRL2_STOP_BIT),
                         bus.regs[RV3032::cmd::REG_CONTROL2]);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(rtc.lastError().code));

  RV3032::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(rtc.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.clockCalendarUncertain);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(snap.clockCalendarLastStatus.code));
}

void test_successful_set_time_clears_clock_calendar_uncertain() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  RV3032::DateTime dt = {2024, 5, 6, 7, 8, 9, 0};
  bus.failWriteCall = 2;
  bus.failWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA,
                                              "forced time burst fail", -43);
  st = rtc.setTime(dt);
  TEST_ASSERT_FALSE(st.ok());

  RV3032::SettingsSnapshot snap;
  TEST_ASSERT_TRUE(rtc.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.clockCalendarUncertain);

  clearHistory(bus);
  st = rtc.setTime(dt);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(rtc.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.clockCalendarUncertain);
  TEST_ASSERT_TRUE(snap.clockCalendarLastStatus.ok());
}

void test_read_status_flags_decodes_each_status_bit() {
  const uint8_t bits[] = {
      RV3032::cmd::STATUS_THF_BIT,
      RV3032::cmd::STATUS_TLF_BIT,
      RV3032::cmd::STATUS_UF_BIT,
      RV3032::cmd::STATUS_TF_BIT,
      RV3032::cmd::STATUS_AF_BIT,
      RV3032::cmd::STATUS_EVF_BIT,
      RV3032::cmd::STATUS_PORF_BIT,
      RV3032::cmd::STATUS_VLF_BIT,
  };

  for (size_t i = 0; i < (sizeof(bits) / sizeof(bits[0])); ++i) {
    FakeI2cBus bus;
    resetBus(bus);
    bus.regs[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(1u << bits[i]);

    RV3032::RV3032 rtc;
    RV3032::Status st = rtc.begin(makeConfig(bus));
    TEST_ASSERT_TRUE(st.ok());

    RV3032::StatusFlags flags;
    st = rtc.readStatusFlags(flags);
    TEST_ASSERT_TRUE(st.ok());
    TEST_ASSERT_EQUAL(bits[i] == RV3032::cmd::STATUS_THF_BIT, flags.tempHigh);
    TEST_ASSERT_EQUAL(bits[i] == RV3032::cmd::STATUS_TLF_BIT, flags.tempLow);
    TEST_ASSERT_EQUAL(bits[i] == RV3032::cmd::STATUS_UF_BIT, flags.update);
    TEST_ASSERT_EQUAL(bits[i] == RV3032::cmd::STATUS_TF_BIT, flags.timer);
    TEST_ASSERT_EQUAL(bits[i] == RV3032::cmd::STATUS_AF_BIT, flags.alarm);
    TEST_ASSERT_EQUAL(bits[i] == RV3032::cmd::STATUS_EVF_BIT, flags.event);
    TEST_ASSERT_EQUAL(bits[i] == RV3032::cmd::STATUS_PORF_BIT, flags.powerOnReset);
    TEST_ASSERT_EQUAL(bits[i] == RV3032::cmd::STATUS_VLF_BIT, flags.voltageLow);
  }

  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_STATUS] = 0xFF;
  bus.regs[RV3032::cmd::REG_TEMP_LSB] =
      static_cast<uint8_t>((1u << RV3032::cmd::EEPROM_ERROR_BIT) |
                           (1u << RV3032::cmd::EEPROM_BUSY_BIT) |
                           (1u << RV3032::cmd::TEMP_CLKF_BIT) |
                           (1u << RV3032::cmd::TEMP_BSF_BIT));
  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  RV3032::StatusFlags flags;
  st = rtc.readStatusFlags(flags);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(flags.tempHigh);
  TEST_ASSERT_TRUE(flags.tempLow);
  TEST_ASSERT_TRUE(flags.update);
  TEST_ASSERT_TRUE(flags.timer);
  TEST_ASSERT_TRUE(flags.alarm);
  TEST_ASSERT_TRUE(flags.event);
  TEST_ASSERT_TRUE(flags.powerOnReset);
  TEST_ASSERT_TRUE(flags.voltageLow);
  TEST_ASSERT_TRUE(flags.eepromError);
  TEST_ASSERT_TRUE(flags.eepromBusy);
  TEST_ASSERT_TRUE(flags.clockFlag);
  TEST_ASSERT_TRUE(flags.backupSwitched);
}

void test_read_validity_decodes_porf_vlf_bsf_and_fault_bits() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_STATUS] =
      static_cast<uint8_t>((1u << RV3032::cmd::STATUS_PORF_BIT) |
                           (1u << RV3032::cmd::STATUS_VLF_BIT));
  bus.regs[RV3032::cmd::REG_TEMP_LSB] =
      static_cast<uint8_t>((1u << RV3032::cmd::EEPROM_ERROR_BIT) |
                           (1u << RV3032::cmd::EEPROM_BUSY_BIT) |
                           (1u << RV3032::cmd::TEMP_CLKF_BIT) |
                           (1u << RV3032::cmd::TEMP_BSF_BIT));

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  RV3032::ValidityFlags flags;
  st = rtc.readValidity(flags);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(flags.powerOnReset);
  TEST_ASSERT_TRUE(flags.voltageLow);
  TEST_ASSERT_TRUE(flags.backupSwitched);
  TEST_ASSERT_TRUE(flags.eepromError);
  TEST_ASSERT_TRUE(flags.eepromBusy);
  TEST_ASSERT_TRUE(flags.clockFlag);
  TEST_ASSERT_TRUE(flags.timeInvalid);
}

void test_clear_status_zero_mask_does_not_write_status() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.clearStatus(0);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeLogCount);
}

void test_clear_status_writes_only_requested_clear_mask() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.clearAlarmFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_STATUS, bus.writeLog[0].reg);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(~(1u << RV3032::cmd::STATUS_AF_BIT)),
                         bus.writeLog[0].data[0]);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readLogCount);

  clearHistory(bus);
  const uint8_t mask = static_cast<uint8_t>((1u << RV3032::cmd::STATUS_AF_BIT) |
                                            (1u << RV3032::cmd::STATUS_TF_BIT) |
                                            (1u << RV3032::cmd::STATUS_UF_BIT) |
                                            (1u << RV3032::cmd::STATUS_EVF_BIT));
  st = rtc.clearStatus(mask);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(~mask),
                         bus.writeLog[0].data[0]);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readLogCount);
}

void test_clear_validity_flags_are_explicit() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_STATUS] =
      static_cast<uint8_t>((1u << RV3032::cmd::STATUS_PORF_BIT) |
                           (1u << RV3032::cmd::STATUS_VLF_BIT) |
                           (1u << RV3032::cmd::STATUS_EVF_BIT));
  bus.regs[RV3032::cmd::REG_TEMP_LSB] = 0xFF;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.clearPowerOnResetFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(~(1u << RV3032::cmd::STATUS_PORF_BIT)),
                         bus.writeLog[0].data[0]);

  clearHistory(bus);
  bus.regs[RV3032::cmd::REG_STATUS] =
      static_cast<uint8_t>((1u << RV3032::cmd::STATUS_PORF_BIT) |
                           (1u << RV3032::cmd::STATUS_VLF_BIT) |
                           (1u << RV3032::cmd::STATUS_EVF_BIT));
  st = rtc.clearVoltageLowFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(~(1u << RV3032::cmd::STATUS_VLF_BIT)),
                         bus.writeLog[0].data[0]);

  clearHistory(bus);
  bus.regs[RV3032::cmd::REG_TEMP_LSB] = 0xFF;
  st = rtc.clearBackupSwitchFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TEMP_LSB, bus.writeLog[0].reg);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(~(1u << RV3032::cmd::TEMP_BSF_BIT)),
                         bus.writeLog[0].data[0]);

  clearHistory(bus);
  bus.regs[RV3032::cmd::REG_TEMP_LSB] = 0xFF;
  st = rtc.clearEepromErrorFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(~(1u << RV3032::cmd::EEPROM_ERROR_BIT)),
                         bus.writeLog[0].data[0]);

  clearHistory(bus);
  bus.regs[RV3032::cmd::REG_TEMP_LSB] = 0xFF;
  st = rtc.clearClockFlag();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(~(1u << RV3032::cmd::TEMP_CLKF_BIT)),
                         bus.writeLog[0].data[0]);
}

void test_clear_fault_flags_ignore_nonclearable_bits_and_combined_mask() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.clearFaultFlags(static_cast<uint8_t>(0xF0u | (1u << RV3032::cmd::EEPROM_BUSY_BIT)));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeLogCount);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readLogCount);

  st = rtc.clearFaultFlags(static_cast<uint8_t>((1u << RV3032::cmd::EEPROM_ERROR_BIT) |
                                                (1u << RV3032::cmd::TEMP_CLKF_BIT) |
                                                (1u << RV3032::cmd::TEMP_BSF_BIT) |
                                                (1u << RV3032::cmd::EEPROM_BUSY_BIT)));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeLogCount);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TEMP_LSB, bus.writeLog[0].reg);
  TEST_ASSERT_EQUAL_HEX8(0xF4, bus.writeLog[0].data[0]);
}

void test_begin_probe_read_time_set_time_do_not_clear_fault_flags() {
  FakeI2cBus bus;
  resetBus(bus);
  seedTimeRegisters(bus, 2024, 1, 2, 3, 4, 5, 2);
  const uint8_t statusFlags =
      static_cast<uint8_t>((1u << RV3032::cmd::STATUS_PORF_BIT) |
                           (1u << RV3032::cmd::STATUS_VLF_BIT) |
                           (1u << RV3032::cmd::STATUS_AF_BIT) |
                           (1u << RV3032::cmd::STATUS_TF_BIT));
  const uint8_t tempFlags =
      static_cast<uint8_t>((1u << RV3032::cmd::EEPROM_ERROR_BIT) |
                           (1u << RV3032::cmd::TEMP_CLKF_BIT) |
                           (1u << RV3032::cmd::TEMP_BSF_BIT));
  bus.regs[RV3032::cmd::REG_STATUS] = statusFlags;
  bus.regs[RV3032::cmd::REG_TEMP_LSB] = tempFlags;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.probe();
  TEST_ASSERT_TRUE(st.ok());
  RV3032::DateTime out;
  st = rtc.readTime(out);
  TEST_ASSERT_TRUE(st.ok());
  RV3032::DateTime set = {2024, 1, 2, 3, 4, 5, 0};
  st = rtc.setTime(set);
  TEST_ASSERT_TRUE(st.ok());

  for (uint32_t i = 0; i < bus.writeLogCount; ++i) {
    TEST_ASSERT_NOT_EQUAL(RV3032::cmd::REG_STATUS, bus.writeLog[i].reg);
    TEST_ASSERT_NOT_EQUAL(RV3032::cmd::REG_TEMP_LSB, bus.writeLog[i].reg);
  }
  TEST_ASSERT_EQUAL_HEX8(statusFlags, bus.regs[RV3032::cmd::REG_STATUS]);
  TEST_ASSERT_EQUAL_HEX8(tempFlags, bus.regs[RV3032::cmd::REG_TEMP_LSB]);
}

void test_user_eeprom_write_byte_uses_command_sequence_and_restores_control1() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_CONTROL1] = (1u << RV3032::cmd::CTRL1_USEL_BIT);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.writeUserEepromByte(5, 0xA5);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0xA5, bus.userEeprom[5]);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::CTRL1_USEL_BIT),
                         bus.regs[RV3032::cmd::REG_CONTROL1]);
  TEST_ASSERT_EQUAL_UINT32(8u, bus.txnLogCount);
  TEST_ASSERT_EQUAL('R', bus.txnLog[0].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[0].reg);
  TEST_ASSERT_EQUAL('W', bus.txnLog[1].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[1].reg);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((1u << RV3032::cmd::CTRL1_USEL_BIT) |
                                             (1u << RV3032::cmd::CTRL1_EERD_BIT)),
                         bus.txnLog[1].data[0]);
  TEST_ASSERT_EQUAL('R', bus.txnLog[2].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TEMP_LSB, bus.txnLog[2].reg);
  TEST_ASSERT_EQUAL('W', bus.txnLog[3].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_EE_ADDRESS, bus.txnLog[3].reg);
  TEST_ASSERT_EQUAL_HEX8(0xD0, bus.txnLog[3].data[0]);
  TEST_ASSERT_EQUAL('W', bus.txnLog[4].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_EE_DATA, bus.txnLog[4].reg);
  TEST_ASSERT_EQUAL_HEX8(0xA5, bus.txnLog[4].data[0]);
  TEST_ASSERT_EQUAL('W', bus.txnLog[5].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_EE_COMMAND, bus.txnLog[5].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::EEPROM_CMD_WRITE_BYTE, bus.txnLog[5].data[0]);
  TEST_ASSERT_EQUAL('R', bus.txnLog[6].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TEMP_LSB, bus.txnLog[6].reg);
  TEST_ASSERT_EQUAL('W', bus.txnLog[7].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[7].reg);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::CTRL1_USEL_BIT),
                         bus.txnLog[7].data[0]);
  RV3032::SettingsSnapshot snap = rtc.getSettings();
  TEST_ASSERT_TRUE(snap.eepromCommandLastStatus.ok());
  TEST_ASSERT_FALSE(snap.eepromWritePendingDirty);
}

void test_user_eeprom_read_byte_uses_command_sequence_and_restores_control1() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_CONTROL1] = (1u << RV3032::cmd::CTRL1_EERD_BIT);
  bus.userEeprom[31] = 0x5A;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  uint8_t value = 0;
  st = rtc.readUserEepromByte(31, value);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0x5A, value);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::CTRL1_EERD_BIT),
                         bus.regs[RV3032::cmd::REG_CONTROL1]);
  TEST_ASSERT_EQUAL_UINT32(8u, bus.txnLogCount);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[0].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[1].reg);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::CTRL1_EERD_BIT),
                         bus.txnLog[1].data[0]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TEMP_LSB, bus.txnLog[2].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_EE_ADDRESS, bus.txnLog[3].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::USER_EEPROM_END, bus.txnLog[3].data[0]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_EE_COMMAND, bus.txnLog[4].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::EEPROM_CMD_READ_BYTE, bus.txnLog[4].data[0]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TEMP_LSB, bus.txnLog[5].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_EE_DATA, bus.txnLog[6].reg);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[7].reg);
}

void test_user_eeprom_rejects_offset_and_length_bounds_without_i2c() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  uint8_t one = 0;
  const uint8_t src[2] = {1, 2};
  st = rtc.readUserEepromByte(RV3032::cmd::USER_EEPROM_SIZE, one);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = rtc.writeUserEepromByte(RV3032::cmd::USER_EEPROM_SIZE, 0xAA);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = rtc.readUserEeprom(31, &one, 2);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = rtc.writeUserEeprom(31, src, sizeof(src));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = rtc.readUserEeprom(0, nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = rtc.writeUserEeprom(0, nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = rtc.readUserEeprom(0, &one, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                          static_cast<uint8_t>(rtc.state()));
}

void test_user_eeprom_busy_polling_waits_before_command() {
  FakeI2cBus bus;
  resetBus(bus);
  const uint8_t polls[] = {
      static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_BUSY_BIT),
      static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_BUSY_BIT),
      0x00,
      0x00
  };
  scriptTempLsbReads(bus, polls, sizeof(polls));

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);
  scriptTempLsbReads(bus, polls, sizeof(polls));

  st = rtc.writeUserEepromByte(0, 0x11);
  TEST_ASSERT_TRUE(st.ok());

  uint32_t commandIndex = UINT32_MAX;
  for (uint32_t i = 0; i < bus.txnLogCount; ++i) {
    if (bus.txnLog[i].op == 'W' && bus.txnLog[i].reg == RV3032::cmd::REG_EE_COMMAND) {
      commandIndex = i;
      break;
    }
  }
  TEST_ASSERT_NOT_EQUAL(UINT32_MAX, commandIndex);
  uint32_t preCommandPolls = 0;
  for (uint32_t i = 0; i < commandIndex; ++i) {
    if (bus.txnLog[i].op == 'R' && bus.txnLog[i].reg == RV3032::cmd::REG_TEMP_LSB) {
      ++preCommandPolls;
    }
  }
  TEST_ASSERT_EQUAL_UINT32(3u, preCommandPolls);
}

void test_user_eeprom_pre_command_busy_timeout_restores_without_command() {
  FakeI2cBus bus;
  resetBus(bus);
  uint8_t busy[128] = {};
  for (size_t i = 0; i < sizeof(busy); ++i) {
    busy[i] = static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_BUSY_BIT);
  }
  scriptTempLsbReads(bus, busy, sizeof(busy));

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);
  scriptTempLsbReads(bus, busy, sizeof(busy));

  st = rtc.writeUserEepromByte(0, 0x22);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  for (uint32_t i = 0; i < bus.txnLogCount; ++i) {
    TEST_ASSERT_FALSE(bus.txnLog[i].op == 'W' &&
                      bus.txnLog[i].reg == RV3032::cmd::REG_EE_COMMAND);
  }
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.regs[RV3032::cmd::REG_CONTROL1]);
  RV3032::SettingsSnapshot snap = rtc.getSettings();
  TEST_ASSERT_TRUE(snap.eepromBusyTimeout);
  TEST_ASSERT_TRUE(snap.eepromWritePendingDirty);
}

void test_user_eeprom_post_command_busy_timeout_reports_dirty() {
  FakeI2cBus bus;
  resetBus(bus);
  uint8_t polls[129] = {};
  polls[0] = 0x00;
  for (size_t i = 1; i < sizeof(polls); ++i) {
    polls[i] = static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_BUSY_BIT);
  }
  scriptTempLsbReads(bus, polls, sizeof(polls));

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);
  scriptTempLsbReads(bus, polls, sizeof(polls));

  st = rtc.writeUserEepromByte(3, 0x33);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  bool sawCommand = false;
  for (uint32_t i = 0; i < bus.txnLogCount; ++i) {
    sawCommand = sawCommand ||
                 (bus.txnLog[i].op == 'W' && bus.txnLog[i].reg == RV3032::cmd::REG_EE_COMMAND);
  }
  TEST_ASSERT_TRUE(sawCommand);
  RV3032::SettingsSnapshot snap = rtc.getSettings();
  TEST_ASSERT_TRUE(snap.eepromBusyTimeout);
  TEST_ASSERT_TRUE(snap.eepromWritePendingDirty);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(RV3032::cmd::USER_EEPROM_START + 3),
                         snap.eepromDirtyAddress);
  TEST_ASSERT_EQUAL_HEX8(0x33, snap.eepromDirtyValue);
}

void test_user_eeprom_write_reports_eef_and_does_not_clear_flag() {
  FakeI2cBus bus;
  resetBus(bus);
  const uint8_t polls[] = {
      0x00,
      static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_ERROR_BIT)
  };
  scriptTempLsbReads(bus, polls, sizeof(polls));

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);
  scriptTempLsbReads(bus, polls, sizeof(polls));

  st = rtc.writeUserEepromByte(1, 0x44);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::EEPROM_WRITE_FAILED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE((bus.regs[RV3032::cmd::REG_TEMP_LSB] &
                    (1u << RV3032::cmd::EEPROM_ERROR_BIT)) != 0);
  for (uint32_t i = 0; i < bus.txnLogCount; ++i) {
    TEST_ASSERT_FALSE(bus.txnLog[i].op == 'W' && bus.txnLog[i].reg == RV3032::cmd::REG_TEMP_LSB);
  }
  RV3032::SettingsSnapshot snap = rtc.getSettings();
  TEST_ASSERT_TRUE(snap.eepromErrorObserved);
  TEST_ASSERT_TRUE(snap.eepromWritePendingDirty);
}

void test_user_eeprom_read_preserves_existing_eef_without_read_failure() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.userEeprom[2] = 0x66;
  const uint8_t eef = static_cast<uint8_t>(1u << RV3032::cmd::EEPROM_ERROR_BIT);
  const uint8_t polls[] = {eef, eef};
  scriptTempLsbReads(bus, polls, sizeof(polls));

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);
  scriptTempLsbReads(bus, polls, sizeof(polls));

  uint8_t value = 0;
  st = rtc.readUserEepromByte(2, value);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0x66, value);
  TEST_ASSERT_TRUE((bus.regs[RV3032::cmd::REG_TEMP_LSB] &
                    (1u << RV3032::cmd::EEPROM_ERROR_BIT)) != 0);
  TEST_ASSERT_FALSE(rtc.getSettings().eepromWritePendingDirty);
  TEST_ASSERT_TRUE(rtc.getSettings().eepromErrorObserved);
}

void test_user_eeprom_partial_i2c_failure_records_dirty_and_preserves_status() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);
  bus.failWriteCall = 3;
  bus.failWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_BUS, "bus", -77);

  st = rtc.writeUserEepromByte(4, 0x77);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-77, st.detail);
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.regs[RV3032::cmd::REG_CONTROL1]);
  RV3032::SettingsSnapshot snap = rtc.getSettings();
  TEST_ASSERT_TRUE(snap.eepromWritePendingDirty);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(RV3032::cmd::USER_EEPROM_START + 4),
                         snap.eepromDirtyAddress);
  TEST_ASSERT_EQUAL_HEX8(0x77, snap.eepromDirtyValue);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_BUS),
                          static_cast<uint8_t>(snap.eepromCommandLastStatus.code));
}

void test_user_eeprom_restore_control1_failure_is_visible_after_command_success() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);
  bus.failWriteCall = 5;
  bus.failWriteStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_DATA, "restore", -12);

  st = rtc.writeUserEepromByte(6, 0x88);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_HEX8(0x88, bus.userEeprom[6]);
  RV3032::SettingsSnapshot snap = rtc.getSettings();
  TEST_ASSERT_TRUE(snap.eepromWritePendingDirty);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(snap.eepromCommandLastStatus.code));
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
  TEST_ASSERT_TRUE(snap.eepromCommandLastStatus.ok());
  TEST_ASSERT_FALSE(snap.eepromBusyTimeout);
  TEST_ASSERT_FALSE(snap.eepromErrorObserved);
  TEST_ASSERT_FALSE(snap.eepromWritePendingDirty);
  TEST_ASSERT_EQUAL_UINT8(0u, snap.eepromDirtyAddress);
  TEST_ASSERT_EQUAL_UINT8(0u, snap.eepromDirtyValue);
  TEST_ASSERT_EQUAL_STRING("", snap.eepromRecoveryRecommendation);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.lastOkMs);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.lastErrorMs);
  TEST_ASSERT_TRUE(snap.lastError.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, snap.consecutiveFailures);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.totalSuccess);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.totalFailures);
  TEST_ASSERT_FALSE(snap.clockCalendarUncertain);
  TEST_ASSERT_TRUE(snap.clockCalendarLastStatus.ok());
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
}

void test_probe_address_nack_maps_to_device_not_found() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  bus.nextReadStatus = RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR,
                                             "forced probe address nack", -17);
  st = rtc.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-17, st.detail);
  TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());
}

void test_probe_preserves_non_address_transport_errors_without_health_updates() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  struct Case {
    RV3032::Err code;
    int32_t detail;
  };
  const Case cases[] = {
      {RV3032::Err::I2C_NACK_DATA, -21},
      {RV3032::Err::I2C_TIMEOUT, -22},
      {RV3032::Err::I2C_BUS, -23},
      {RV3032::Err::I2C_ERROR, -24},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); ++i) {
    bus.nextReadStatus = RV3032::Status::Error(cases[i].code, "forced probe", cases[i].detail);
    st = rtc.probe();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(cases[i].code),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(cases[i].detail, st.detail);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::READY),
                            static_cast<uint8_t>(rtc.state()));
    TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());
    TEST_ASSERT_EQUAL_UINT32(0u, rtc.totalFailures());
  }
}

void test_recover_preserves_transport_errors_and_updates_health_once() {
  struct Case {
    RV3032::Err code;
    int32_t detail;
  };
  const Case cases[] = {
      {RV3032::Err::I2C_NACK_ADDR, -31},
      {RV3032::Err::I2C_NACK_DATA, -32},
      {RV3032::Err::I2C_TIMEOUT, -33},
      {RV3032::Err::I2C_BUS, -34},
      {RV3032::Err::I2C_ERROR, -35},
  };

  for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); ++i) {
    FakeI2cBus bus;
    resetBus(bus);

    RV3032::RV3032 rtc;
    RV3032::Status st = rtc.begin(makeConfig(bus));
    TEST_ASSERT_TRUE(st.ok());
    TEST_ASSERT_EQUAL_UINT8(0, rtc.consecutiveFailures());

    bus.nextReadStatus = RV3032::Status::Error(cases[i].code, "forced recover",
                                               cases[i].detail);
    st = rtc.recover();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(cases[i].code),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(cases[i].detail, st.detail);
    TEST_ASSERT_EQUAL_UINT8(1, rtc.consecutiveFailures());
    TEST_ASSERT_EQUAL_UINT32(1u, rtc.totalFailures());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::DEGRADED),
                            static_cast<uint8_t>(rtc.state()));

    const RV3032::Status last = rtc.lastError();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(cases[i].code),
                            static_cast<uint8_t>(last.code));
    TEST_ASSERT_EQUAL_INT32(cases[i].detail, last.detail);
  }
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_ADDR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-8, st.detail);
  TEST_ASSERT_EQUAL_UINT8(1, rtc.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::DriverState::DEGRADED),
                          static_cast<uint8_t>(rtc.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_ADDR),
                          static_cast<uint8_t>(rtc.lastError().code));
  TEST_ASSERT_EQUAL_INT32(-8, rtc.lastError().detail);
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::I2C_NACK_ADDR),
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

void test_set_timer_disables_before_writing_value_and_enables_last() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  bus.regs[RV3032::cmd::REG_CONTROL1] =
      static_cast<uint8_t>((1u << RV3032::cmd::CTRL1_TRPT_BIT) |
                           (1u << RV3032::cmd::CTRL1_TE_BIT) |
                           static_cast<uint8_t>(RV3032::TimerFrequency::Hz64));
  clearHistory(bus);

  st = rtc.setTimer(0x0456, RV3032::TimerFrequency::Hz1, true);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0x56, bus.regs[RV3032::cmd::REG_TIMER_LOW]);
  TEST_ASSERT_EQUAL_HEX8(0x04, bus.regs[RV3032::cmd::REG_TIMER_HIGH]);
  TEST_ASSERT_EQUAL_UINT32(5u, bus.txnLogCount);
  TEST_ASSERT_EQUAL('R', bus.txnLog[0].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[0].reg);
  TEST_ASSERT_EQUAL('W', bus.txnLog[1].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[1].reg);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((1u << RV3032::cmd::CTRL1_TRPT_BIT) |
                                             static_cast<uint8_t>(RV3032::TimerFrequency::Hz1)),
                         bus.txnLog[1].data[0]);
  TEST_ASSERT_EQUAL('W', bus.txnLog[2].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TIMER_LOW, bus.txnLog[2].reg);
  TEST_ASSERT_EQUAL('W', bus.txnLog[3].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TIMER_HIGH, bus.txnLog[3].reg);
  TEST_ASSERT_EQUAL('W', bus.txnLog[4].op);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[4].reg);
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((1u << RV3032::cmd::CTRL1_TRPT_BIT) |
                                             (1u << RV3032::cmd::CTRL1_TE_BIT) |
                                              static_cast<uint8_t>(RV3032::TimerFrequency::Hz1)),
                         bus.txnLog[4].data[0]);
}

void test_set_timer_disable_mode_does_not_reenable_timer() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_CONTROL1] =
      static_cast<uint8_t>((1u << RV3032::cmd::CTRL1_TE_BIT) |
                           static_cast<uint8_t>(RV3032::TimerFrequency::Hz64));

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.setTimer(0x0000, RV3032::TimerFrequency::Hz4096, false);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(4u, bus.txnLogCount);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_CONTROL1, bus.txnLog[1].reg);
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.txnLog[1].data[0]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TIMER_LOW, bus.txnLog[2].reg);
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.txnLog[2].data[0]);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_TIMER_HIGH, bus.txnLog[3].reg);
  TEST_ASSERT_EQUAL_HEX8(0x00, bus.txnLog[3].data[0]);
}

void test_alarm_interrupt_uses_documented_control2_aie_bit() {
  FakeI2cBus bus;
  resetBus(bus);

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());

  bus.regs[RV3032::cmd::REG_CONTROL2] = 0;
  st = rtc.enableAlarmInterrupt(true);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::CTRL2_AIE_BIT),
                         bus.regs[RV3032::cmd::REG_CONTROL2]);

  bool enabled = false;
  st = rtc.getAlarmInterruptEnabled(enabled);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(enabled);
}

void test_set_alarm_time_preserves_ae_bits_and_writes_bcd_payloads() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_ALARM_MINUTE] = 0x80;
  bus.regs[RV3032::cmd::REG_ALARM_HOUR] = 0x00;
  bus.regs[RV3032::cmd::REG_ALARM_DATE] = 0x80;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.setAlarmTime(12, 3, 31);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1u, bus.readLogCount);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_ALARM_MINUTE, bus.readLog[0].reg);
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeLogCount);
  TEST_ASSERT_EQUAL_HEX8(RV3032::cmd::REG_ALARM_MINUTE, bus.writeLog[0].reg);
  TEST_ASSERT_EQUAL_HEX8(0x92, bus.writeLog[0].data[0]);
  TEST_ASSERT_EQUAL_HEX8(0x03, bus.writeLog[0].data[1]);
  TEST_ASSERT_EQUAL_HEX8(0xB1, bus.writeLog[0].data[2]);
}

void test_set_alarm_match_maps_true_to_ae_zero_false_to_ae_one() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_ALARM_MINUTE] = 0x12;
  bus.regs[RV3032::cmd::REG_ALARM_HOUR] = 0x03;
  bus.regs[RV3032::cmd::REG_ALARM_DATE] = 0x31;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.setAlarmMatch(true, false, true);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(0x12, bus.writeLog[0].data[0]);
  TEST_ASSERT_EQUAL_HEX8(0x83, bus.writeLog[0].data[1]);
  TEST_ASSERT_EQUAL_HEX8(0x31, bus.writeLog[0].data[2]);
}

void test_timer_and_event_interrupt_enable_bits_are_separate_from_flags() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_CONTROL2] = 0;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.enableTimerInterrupt(true);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::CTRL2_TIE_BIT),
                         bus.regs[RV3032::cmd::REG_CONTROL2]);
  st = rtc.enableEventInterrupt(true);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((1u << RV3032::cmd::CTRL2_TIE_BIT) |
                                             (1u << RV3032::cmd::CTRL2_EIE_BIT)),
                         bus.regs[RV3032::cmd::REG_CONTROL2]);

  bool enabled = false;
  st = rtc.getTimerInterruptEnabled(enabled);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(enabled);
  st = rtc.getEventInterruptEnabled(enabled);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(enabled);

  for (uint32_t i = 0; i < bus.writeLogCount; ++i) {
    TEST_ASSERT_NOT_EQUAL(RV3032::cmd::REG_STATUS, bus.writeLog[i].reg);
  }
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

  st = rtc.setTimer(0, RV3032::TimerFrequency::Hz1, true);
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

void test_evi_setters_mask_reserved_control_bits() {
  FakeI2cBus bus;
  resetBus(bus);
  bus.regs[RV3032::cmd::REG_EVI_CONTROL] = 0x0E;

  RV3032::RV3032 rtc;
  RV3032::Status st = rtc.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  clearHistory(bus);

  st = rtc.setEviEdge(true);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(1u << RV3032::cmd::EVI_EB_BIT),
                         bus.regs[RV3032::cmd::REG_EVI_CONTROL]);

  bus.regs[RV3032::cmd::REG_EVI_CONTROL] = 0x0E;
  st = rtc.setEviDebounce(RV3032::EviDebounce::Hz64);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(static_cast<uint8_t>(RV3032::EviDebounce::Hz64)
                                             << RV3032::cmd::EVI_DB_SHIFT),
                         bus.regs[RV3032::cmd::REG_EVI_CONTROL]);
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

  st = rtc.readRegisters(RV3032::cmd::USER_EEPROM_START, &one, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.writeRegisters(RV3032::cmd::USER_EEPROM_START, &one, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.readRegister(RV3032::cmd::USER_EEPROM_START, one);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.writeRegister(RV3032::cmd::USER_EEPROM_START, one);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.readRegister(RV3032::cmd::USER_EEPROM_END, one);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.writeRegister(RV3032::cmd::USER_EEPROM_END, one);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  uint8_t fullUserEeprom[RV3032::cmd::USER_EEPROM_SIZE] = {};
  st = rtc.readRegisters(RV3032::cmd::USER_EEPROM_START, fullUserEeprom, sizeof(fullUserEeprom));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.writeRegisters(RV3032::cmd::USER_EEPROM_START, fullUserEeprom, sizeof(fullUserEeprom));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.readRegisters(RV3032::cmd::REG_EEPROM_PW_ENABLE, two, sizeof(two));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RV3032::Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  st = rtc.writeRegisters(RV3032::cmd::REG_EEPROM_PW_ENABLE, two, sizeof(two));
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
  RUN_TEST(test_driver_is_not_copyable_or_movable);
  RUN_TEST(test_control2_interrupt_bits_match_register_reference);
  RUN_TEST(test_compute_weekday);
  RUN_TEST(test_unix_roundtrip_epoch_2000);
  RUN_TEST(test_unix_leap_day);
  RUN_TEST(test_unix_out_of_rtc_range);
  RUN_TEST(test_invalid_date);
  RUN_TEST(test_read_time_decodes_valid_bcd_and_uses_single_burst);
  RUN_TEST(test_read_time_accepts_leap_days_and_rejects_non_leap_feb29);
  RUN_TEST(test_read_time_rejects_invalid_bcd_and_all_ff);
  RUN_TEST(test_read_time_rejects_bcd_valid_out_of_range_values);
  RUN_TEST(test_set_time_uses_stop_sequence_and_preserves_control2_bits);
  RUN_TEST(test_set_time_rejects_invalid_datetime_without_i2c_or_uncertain_state);
  RUN_TEST(test_set_time_partial_failure_marks_clock_calendar_uncertain);
  RUN_TEST(test_set_time_release_stop_failure_marks_uncertain_and_leaves_error_visible);
  RUN_TEST(test_successful_set_time_clears_clock_calendar_uncertain);
  RUN_TEST(test_read_status_flags_decodes_each_status_bit);
  RUN_TEST(test_read_validity_decodes_porf_vlf_bsf_and_fault_bits);
  RUN_TEST(test_clear_status_zero_mask_does_not_write_status);
  RUN_TEST(test_clear_status_writes_only_requested_clear_mask);
  RUN_TEST(test_clear_validity_flags_are_explicit);
  RUN_TEST(test_clear_fault_flags_ignore_nonclearable_bits_and_combined_mask);
  RUN_TEST(test_begin_probe_read_time_set_time_do_not_clear_fault_flags);
  RUN_TEST(test_user_eeprom_write_byte_uses_command_sequence_and_restores_control1);
  RUN_TEST(test_user_eeprom_read_byte_uses_command_sequence_and_restores_control1);
  RUN_TEST(test_user_eeprom_rejects_offset_and_length_bounds_without_i2c);
  RUN_TEST(test_user_eeprom_busy_polling_waits_before_command);
  RUN_TEST(test_user_eeprom_pre_command_busy_timeout_restores_without_command);
  RUN_TEST(test_user_eeprom_post_command_busy_timeout_reports_dirty);
  RUN_TEST(test_user_eeprom_write_reports_eef_and_does_not_clear_flag);
  RUN_TEST(test_user_eeprom_read_preserves_existing_eef_without_read_failure);
  RUN_TEST(test_user_eeprom_partial_i2c_failure_records_dirty_and_preserves_status);
  RUN_TEST(test_user_eeprom_restore_control1_failure_is_visible_after_command_success);
  RUN_TEST(test_begin_rejects_non_default_address);
  RUN_TEST(test_begin_rejects_invalid_backup_mode);
  RUN_TEST(test_invalid_begin_after_success_resets_without_i2c);
  RUN_TEST(test_get_settings_snapshot);
  RUN_TEST(test_runtime_i2c_after_begin_updates_health);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_probe_address_nack_maps_to_device_not_found);
  RUN_TEST(test_probe_preserves_non_address_transport_errors_without_health_updates);
  RUN_TEST(test_recover_preserves_transport_errors_and_updates_health_once);
  RUN_TEST(test_recover_failure_updates_health_once);
  RUN_TEST(test_offline_latches_public_read_without_i2c);
  RUN_TEST(test_failed_recover_from_offline_keeps_latch_after_intermediate_success);
  RUN_TEST(test_transport_validation_status_does_not_update_health);
  RUN_TEST(test_set_timer_disables_before_writing_value_and_enables_last);
  RUN_TEST(test_set_timer_disable_mode_does_not_reenable_timer);
  RUN_TEST(test_alarm_interrupt_uses_documented_control2_aie_bit);
  RUN_TEST(test_set_alarm_time_preserves_ae_bits_and_writes_bcd_payloads);
  RUN_TEST(test_set_alarm_match_maps_true_to_ae_zero_false_to_ae_one);
  RUN_TEST(test_timer_and_event_interrupt_enable_bits_are_separate_from_flags);
  RUN_TEST(test_invalid_runtime_params_are_rejected);
  RUN_TEST(test_backup_mode_helpers_update_pmu_bits);
  RUN_TEST(test_get_alarm_config_rejects_invalid_bcd);
  RUN_TEST(test_get_alarm_config_tolerates_por_inactive_date_alarm);
  RUN_TEST(test_get_alarm_config_tolerates_disabled_field_garbage);
  RUN_TEST(test_read_timestamp_decodes_evi_and_reset_sets_control_bit);
  RUN_TEST(test_evi_setters_mask_reserved_control_bits);
  RUN_TEST(test_read_timestamp_decodes_tlow_without_hundredths);
  RUN_TEST(test_read_timestamp_rejects_invalid_bcd);
  RUN_TEST(test_user_ram_helpers_round_trip_and_reject_invalid_ranges);
  RUN_TEST(test_public_block_register_access);
  RUN_TEST(test_public_block_register_access_rejects_invalid_params);
  RUN_TEST(test_public_block_register_access_rejects_invalid_ranges_without_i2c);
  return UNITY_END();
}
