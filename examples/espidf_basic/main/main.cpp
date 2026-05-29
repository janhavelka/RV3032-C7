/**
 * @file main.cpp
 * @brief Native ESP-IDF bring-up CLI for RV3032-C7.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "RV3032/RV3032.h"

namespace {

static constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
static constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
static constexpr uint32_t I2C_FREQ_HZ = 400000U;
static constexpr uint32_t I2C_TIMEOUT_MS = 50U;
static constexpr size_t LINE_LEN = 192U;
static constexpr const char* CONFIRM_TOKEN = "confirm";

struct NativeBus {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t device = nullptr;
  uint8_t deviceAddress = 0;
  uint32_t freqHz = I2C_FREQ_HZ;
};

NativeBus gBus;
RV3032::RV3032 gRtc;
RV3032::Config gCfg;
bool gVerbose = false;

uint32_t nowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

int timeoutArg(uint32_t timeoutMs) {
  return timeoutMs > static_cast<uint32_t>(INT_MAX) ? INT_MAX : static_cast<int>(timeoutMs);
}

RV3032::Status mapI2c(esp_err_t err, const char* msg) {
  if (err == ESP_OK) {
    return RV3032::Status::Ok();
  }
  if (err == ESP_ERR_TIMEOUT) {
    return RV3032::Status::Error(RV3032::Err::I2C_TIMEOUT, msg, err);
  }
  if (err == ESP_ERR_INVALID_ARG) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, msg, err);
  }
  if (err == ESP_ERR_INVALID_RESPONSE || err == ESP_ERR_NOT_FOUND) {
    return RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR, msg, err);
  }
  return RV3032::Status::Error(RV3032::Err::I2C_BUS, msg, err);
}

esp_err_t addDevice(NativeBus& bus, uint8_t addr, i2c_master_dev_handle_t* out) {
  i2c_device_config_t dev = {};
  dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev.device_address = addr;
  dev.scl_speed_hz = bus.freqHz;
  return i2c_master_bus_add_device(bus.bus, &dev, out);
}

esp_err_t ensureDevice(NativeBus& bus, uint8_t addr) {
  if (bus.device != nullptr && bus.deviceAddress == addr) {
    return ESP_OK;
  }
  if (bus.device != nullptr) {
    (void)i2c_master_bus_rm_device(bus.device);
    bus.device = nullptr;
    bus.deviceAddress = 0;
  }
  esp_err_t err = addDevice(bus, addr, &bus.device);
  if (err == ESP_OK) {
    bus.deviceAddress = addr;
  }
  return err;
}

RV3032::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                        uint32_t timeoutMs, void* user) {
  NativeBus* bus = static_cast<NativeBus*>(user);
  if (bus == nullptr || bus->bus == nullptr) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "I2C bus not initialized");
  }
  esp_err_t err = ensureDevice(*bus, addr);
  if (err == ESP_OK) {
    err = i2c_master_transmit(bus->device, data, len, timeoutArg(timeoutMs));
  }
  return mapI2c(err, "I2C write failed");
}

RV3032::Status i2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                            uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                            void* user) {
  NativeBus* bus = static_cast<NativeBus*>(user);
  if (bus == nullptr || bus->bus == nullptr) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "I2C bus not initialized");
  }
  esp_err_t err = ensureDevice(*bus, addr);
  if (err == ESP_OK) {
    err = i2c_master_transmit_receive(bus->device, tx, txLen, rx, rxLen, timeoutArg(timeoutMs));
  }
  return mapI2c(err, "I2C write-read failed");
}

bool initBus() {
  i2c_master_bus_config_t cfg = {};
  cfg.i2c_port = I2C_NUM_0;
  cfg.sda_io_num = I2C_SDA;
  cfg.scl_io_num = I2C_SCL;
  cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  cfg.glitch_ignore_cnt = 7;
  cfg.flags.enable_internal_pullup = true;
  return i2c_new_master_bus(&cfg, &gBus.bus) == ESP_OK;
}

void printStatus(const char* op, RV3032::Status st) {
  const char* result = st.ok() ? "OK" : (st.code == RV3032::Err::IN_PROGRESS ? "IN_PROGRESS" : "FAIL");
  printf("%s: %s (code=%u detail=%ld)\n", op, result,
         static_cast<unsigned>(st.code), static_cast<long>(st.detail));
  if (!st.ok() && st.msg != nullptr) {
    printf("  %s\n", st.msg);
  }
}

char* trim(char* text) {
  while (*text != '\0' && isspace(static_cast<unsigned char>(*text))) {
    ++text;
  }
  char* end = text + strlen(text);
  while (end > text && isspace(static_cast<unsigned char>(end[-1]))) {
    *--end = '\0';
  }
  return text;
}

bool parseU32(const char* text, uint32_t* out, const char** tail = nullptr) {
  if (text == nullptr || out == nullptr) {
    return false;
  }
  while (*text != '\0' && isspace(static_cast<unsigned char>(*text))) {
    ++text;
  }
  if (*text == '\0' || *text == '-') {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const unsigned long v = strtoul(text, &end, 0);
  if (end == text || errno == ERANGE || v > UINT32_MAX) {
    return false;
  }
  while (*end != '\0' && isspace(static_cast<unsigned char>(*end))) {
    ++end;
  }
  *out = static_cast<uint32_t>(v);
  if (tail != nullptr) {
    *tail = end;
    return true;
  }
  return *end == '\0';
}

bool parseFloatArg(const char* text, float* out) {
  if (text == nullptr || out == nullptr) {
    return false;
  }
  while (*text != '\0' && isspace(static_cast<unsigned char>(*text))) {
    ++text;
  }
  if (*text == '\0') {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const float value = strtof(text, &end);
  if (end == text || errno == ERANGE || !isfinite(value)) {
    return false;
  }
  while (*end != '\0' && isspace(static_cast<unsigned char>(*end))) {
    ++end;
  }
  if (*end != '\0') {
    return false;
  }
  *out = value;
  return true;
}

bool parseBoolArg(const char* text, bool* out) {
  uint32_t value = 0;
  if (!parseU32(text, &value) || value > 1U || out == nullptr) {
    return false;
  }
  *out = (value != 0U);
  return true;
}

char* splitFirstToken(char* text, char** rest) {
  char* token = trim(text);
  char* end = token;
  while (*end != '\0' && !isspace(static_cast<unsigned char>(*end))) {
    ++end;
  }
  if (*end != '\0') {
    *end = '\0';
    ++end;
  }
  if (rest != nullptr) {
    *rest = trim(end);
  }
  return token;
}

bool stripTrailingConfirm(char* args) {
  char* text = trim(args);
  const size_t len = strlen(text);
  if (len == 0U) {
    return false;
  }
  char* tokenStart = text + len;
  while (tokenStart > text && !isspace(static_cast<unsigned char>(tokenStart[-1]))) {
    --tokenStart;
  }
  if (strcmp(tokenStart, CONFIRM_TOKEN) != 0) {
    return false;
  }
  if (tokenStart == text) {
    *text = '\0';
    return true;
  }
  char* before = tokenStart;
  while (before > text && isspace(static_cast<unsigned char>(before[-1]))) {
    --before;
  }
  *before = '\0';
  return true;
}

void printConfirmationRequired(const char* original, const char* wouldChange,
                               const char* scope, const char* reason) {
  puts("Confirmation required.");
  printf("Would change: %s\n", wouldChange);
  printf("Scope: %s\n", scope);
  printf("Reason: %s\n", reason);
  printf("Run exactly: %s %s\n", original, CONFIRM_TOKEN);
}

bool requireConfirmation(bool confirmed, const char* original, const char* wouldChange,
                         const char* scope, const char* reason) {
  if (confirmed) {
    return true;
  }
  printConfirmationRequired(original, wouldChange, scope, reason);
  return false;
}

const char* eepromBackedScope() {
  RV3032::SettingsSnapshot snap = gRtc.getSettings();
  return snap.enableEepromWrites
             ? "persistent (EEPROM-backed; queues an EEPROM commit)"
             : "volatile (RAM mirror only; EEPROM persistence disabled)";
}

const char* registerWriteScope(uint8_t reg) {
  if ((reg >= RV3032::cmd::REG_EE_ADDRESS && reg <= RV3032::cmd::REG_EE_COMMAND) ||
      (reg >= RV3032::cmd::REG_EEPROM_PMU && reg <= RV3032::cmd::USER_EEPROM_END)) {
    return "persistent (direct EEPROM/control register access)";
  }
  return "volatile (direct RTC register access)";
}

bool parseBackupMode(const char* text, RV3032::BackupSwitchMode* mode, bool* usualDefaults) {
  if (text == nullptr || mode == nullptr || usualDefaults == nullptr) {
    return false;
  }
  *usualDefaults = false;
  if (strcmp(text, "usual") == 0 || strcmp(text, "coin") == 0 || strcmp(text, "primary") == 0) {
    *usualDefaults = true;
    *mode = RV3032::BackupSwitchMode::Level;
    return true;
  }
  if (strcmp(text, "off") == 0 || strcmp(text, "0") == 0) {
    *mode = RV3032::BackupSwitchMode::Off;
    return true;
  }
  if (strcmp(text, "level") == 0 || strcmp(text, "1") == 0) {
    *mode = RV3032::BackupSwitchMode::Level;
    return true;
  }
  if (strcmp(text, "direct") == 0 || strcmp(text, "2") == 0) {
    *mode = RV3032::BackupSwitchMode::Direct;
    return true;
  }
  return false;
}

void beginDriver() {
  gCfg.i2cWrite = i2cWrite;
  gCfg.i2cWriteRead = i2cWriteRead;
  gCfg.i2cUser = &gBus;
  gCfg.nowMs = nowMs;
  gCfg.i2cTimeoutMs = I2C_TIMEOUT_MS;
  printStatus("begin", gRtc.begin(gCfg));
}

void printDateTime(const RV3032::DateTime& t) {
  printf("%04u-%02u-%02u %02u:%02u:%02u weekday=%u\n",
         static_cast<unsigned>(t.year), t.month, t.day, t.hour, t.minute,
         t.second, t.weekday);
}

const char* timerFreqName(RV3032::TimerFrequency freq) {
  switch (freq) {
    case RV3032::TimerFrequency::Hz4096: return "4096Hz";
    case RV3032::TimerFrequency::Hz64: return "64Hz";
    case RV3032::TimerFrequency::Hz1: return "1Hz";
    case RV3032::TimerFrequency::Hz1_60: return "1/60Hz";
    default: return "unknown";
  }
}

const char* clkoutFreqName(RV3032::ClkoutFrequency freq) {
  switch (freq) {
    case RV3032::ClkoutFrequency::Hz32768: return "32768Hz";
    case RV3032::ClkoutFrequency::Hz1024: return "1024Hz";
    case RV3032::ClkoutFrequency::Hz64: return "64Hz";
    case RV3032::ClkoutFrequency::Hz1: return "1Hz";
    default: return "unknown";
  }
}

const char* backupModeName(RV3032::BackupSwitchMode mode) {
  switch (mode) {
    case RV3032::BackupSwitchMode::Off: return "off";
    case RV3032::BackupSwitchMode::Level: return "level";
    case RV3032::BackupSwitchMode::Direct: return "direct";
    default: return "unknown";
  }
}

const char* debounceName(RV3032::EviDebounce debounce) {
  switch (debounce) {
    case RV3032::EviDebounce::None: return "none";
    case RV3032::EviDebounce::Hz256: return "256Hz";
    case RV3032::EviDebounce::Hz64: return "64Hz";
    case RV3032::EviDebounce::Hz8: return "8Hz";
    default: return "unknown";
  }
}

bool parseTimestampSource(const char* text, RV3032::TimestampSource* source) {
  if (text == nullptr || source == nullptr) {
    return false;
  }
  while (*text != '\0' && isspace(static_cast<unsigned char>(*text))) {
    ++text;
  }
  if (strcmp(text, "tlow") == 0) {
    *source = RV3032::TimestampSource::TLow;
    return true;
  }
  if (strcmp(text, "thigh") == 0) {
    *source = RV3032::TimestampSource::THigh;
    return true;
  }
  if (strcmp(text, "evi") == 0) {
    *source = RV3032::TimestampSource::Evi;
    return true;
  }
  return false;
}

const char* timestampSourceName(RV3032::TimestampSource source) {
  switch (source) {
    case RV3032::TimestampSource::TLow: return "tlow";
    case RV3032::TimestampSource::THigh: return "thigh";
    case RV3032::TimestampSource::Evi: return "evi";
    default: return "unknown";
  }
}

RV3032::Status readUserEepromByte(uint8_t addr, uint8_t* value) {
  if (value == nullptr) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "EEPROM read buffer is null");
  }
  if (addr < RV3032::cmd::USER_EEPROM_START || addr > RV3032::cmd::USER_EEPROM_END) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "EEPROM address out of range");
  }
  if (gRtc.isEepromBusy()) {
    return RV3032::Status::Error(RV3032::Err::BUSY, "EEPROM update in progress");
  }

  uint8_t control1 = 0;
  RV3032::Status st = gRtc.readRegister(RV3032::cmd::REG_CONTROL1, control1);
  if (!st.ok()) {
    return st;
  }

  const uint8_t eerdMask = static_cast<uint8_t>(1U << RV3032::cmd::CTRL1_EERD_BIT);
  const uint8_t newControl1 = static_cast<uint8_t>(control1 | eerdMask);
  if (newControl1 != control1) {
    st = gRtc.writeRegister(RV3032::cmd::REG_CONTROL1, newControl1);
    if (!st.ok()) {
      return st;
    }
  }

  st = gRtc.writeRegister(RV3032::cmd::REG_EE_ADDRESS, addr);
  if (st.ok()) {
    st = gRtc.writeRegister(RV3032::cmd::REG_EE_COMMAND, RV3032::cmd::EEPROM_CMD_READ);
  }
  if (st.ok()) {
    st = gRtc.readRegister(RV3032::cmd::REG_EE_DATA, *value);
  }

  if (newControl1 != control1) {
    const RV3032::Status restore = gRtc.writeRegister(RV3032::cmd::REG_CONTROL1, control1);
    if (!restore.ok() && st.ok()) {
      st = restore;
    }
  }

  return st;
}

void printHelp() {
  puts("Native ESP-IDF RV3032-C7 CLI");
  puts("  help / ? | version / ver | scan | read | cfg / settings");
  puts("  time | set [YYYY MM DD HH MM SS] | setbuild | unix [ts] | temp");
  puts("  alarm | alarm_set [MM HH DD] | alarm_match [M H D] | alarm_int [0|1] | alarm_clear");
  puts("  timer | timer <ticks> <freq 0..3> <en 0|1>");
  puts("  clkout [0|1] | clkout_freq [0..3] | offset [ppm]");
  puts("  evi | evi edge [0|1] | evi debounce [0..3] | evi overwrite [0|1]");
  puts("  ts <tlow|thigh|evi> | ts_reset <tlow|thigh|evi>");
  puts("  status | statusf | status_clear [mask] | validity");
  puts("  ram [offset len] | ram_write <offset> <byte...> | reg <addr> [val]");
  puts("  eeprom | backup [usual|off|level|direct]");
  puts("  clear_porf | clear_vlf | clear_bsf");
  puts("  drv | probe | recover | verbose [0|1] | stress [N] | stress_mix [N] | selftest");
}

void scanBus() {
  puts("I2C scan:");
  for (uint8_t addr = 0x08U; addr <= 0x77U; ++addr) {
    if (i2c_master_probe(gBus.bus, addr, timeoutArg(I2C_TIMEOUT_MS)) == ESP_OK) {
      printf("  0x%02X\n", addr);
    }
  }
}

void printDrv() {
  RV3032::SettingsSnapshot snap = gRtc.getSettings();
  printf("state=%u initialized=%d online=%d addr=0x%02X ok=%lu fail=%lu consecutive=%u eepromBusy=%d queue=%u\n",
         static_cast<unsigned>(snap.state), snap.initialized ? 1 : 0,
         gRtc.isOnline() ? 1 : 0, snap.i2cAddress,
         static_cast<unsigned long>(snap.totalSuccess),
         static_cast<unsigned long>(snap.totalFailures), snap.consecutiveFailures,
         snap.eepromBusy ? 1 : 0, snap.eepromQueueDepth);
}

void cmdTime() {
  RV3032::DateTime t;
  RV3032::Status st = gRtc.readTime(t);
  printStatus("time", st);
  if (st.ok()) {
    printDateTime(t);
  }
}

void cmdSet(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0') {
    cmdTime();
    return;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  char extra = '\0';
  if (sscanf(args, "%d %d %d %d %d %d %c",
             &year, &month, &day, &hour, &minute, &second, &extra) != 6) {
    puts("Usage: set YYYY MM DD HH MM SS");
    return;
  }

  RV3032::DateTime t;
  t.year = static_cast<uint16_t>(year);
  t.month = static_cast<uint8_t>(month);
  t.day = static_cast<uint8_t>(day);
  t.hour = static_cast<uint8_t>(hour);
  t.minute = static_cast<uint8_t>(minute);
  t.second = static_cast<uint8_t>(second);
  if (year >= 2000 && year <= 2099 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
    t.weekday = RV3032::RV3032::computeWeekday(t.year, t.month, t.day);
  }

  char would[128] = {};
  snprintf(would, sizeof(would), "RTC time registers to %04d-%02d-%02d %02d:%02d:%02d",
           year, month, day, hour, minute, second);
  if (!requireConfirmation(confirmed, original, would,
                           "volatile (RTC calendar/time registers)",
                           "time changes can invalidate logs, alarms, and application scheduling")) {
    return;
  }

  RV3032::Status st = gRtc.setTime(t);
  printStatus("set", st);
  if (st.ok()) {
    printDateTime(t);
  }
}

void cmdSetBuild(bool confirmed, const char* original) {
  RV3032::DateTime t;
  if (!RV3032::RV3032::parseBuildTime(t)) {
    puts("setbuild: parse failed");
    return;
  }
  char would[128] = {};
  snprintf(would, sizeof(would), "RTC time registers to build timestamp %04u-%02u-%02u %02u:%02u:%02u",
           static_cast<unsigned>(t.year), t.month, t.day, t.hour, t.minute, t.second);
  if (!requireConfirmation(confirmed, original, would,
                           "volatile (RTC calendar/time registers)",
                           "time changes can invalidate logs, alarms, and application scheduling")) {
    return;
  }
  printStatus("setbuild", gRtc.setTime(t));
}

void cmdUnix(char* args, bool confirmed, const char* original) {
  args = trim(args);
  uint32_t ts = 0;
  if (*args == '\0') {
    RV3032::Status st = gRtc.readUnix(ts);
    printStatus("unix", st);
    if (st.ok()) {
      printf("%lu\n", static_cast<unsigned long>(ts));
    }
    return;
  }
  if (!parseU32(args, &ts)) {
    puts("Usage: unix [timestamp]");
    return;
  }
  char would[96] = {};
  snprintf(would, sizeof(would), "RTC time registers from Unix timestamp %lu",
           static_cast<unsigned long>(ts));
  if (!requireConfirmation(confirmed, original, would,
                           "volatile (RTC calendar/time registers)",
                           "time changes can invalidate logs, alarms, and application scheduling")) {
    return;
  }
  printStatus("unix", gRtc.setUnix(ts));
}

void cmdTemp() {
  float temp = 0.0f;
  RV3032::Status st = gRtc.readTemperatureC(temp);
  printStatus("temp", st);
  if (st.ok()) {
    printf("%.2f C\n", static_cast<double>(temp));
  }
}

void cmdAlarm() {
  RV3032::AlarmConfig cfg;
  bool intEnabled = false;
  bool triggered = false;
  RV3032::Status st = gRtc.getAlarmConfig(cfg);
  if (st.ok()) {
    st = gRtc.getAlarmInterruptEnabled(intEnabled);
  }
  if (st.ok()) {
    st = gRtc.getAlarmFlag(triggered);
  }
  printStatus("alarm", st);
  if (st.ok()) {
    printf("time=%02u:%02u date=%02u match(min=%d hour=%d date=%d) int=%d flag=%d\n",
           cfg.hour, cfg.minute, cfg.date, cfg.matchMinute ? 1 : 0,
           cfg.matchHour ? 1 : 0, cfg.matchDate ? 1 : 0,
           intEnabled ? 1 : 0, triggered ? 1 : 0);
  }
}

void cmdAlarmSet(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0') {
    cmdAlarm();
    return;
  }
  int minute = 0;
  int hour = 0;
  int date = 0;
  char extra = '\0';
  if (sscanf(args, "%d %d %d %c", &minute, &hour, &date, &extra) != 3) {
    puts("Usage: alarm_set MM HH DD");
    return;
  }
  char would[96] = {};
  snprintf(would, sizeof(would), "alarm time to %02d:%02d date=%02d", hour, minute, date);
  if (!requireConfirmation(confirmed, original, would,
                           "volatile (RTC alarm registers)",
                           "alarm changes can alter wake/interrupt behavior")) {
    return;
  }
  printStatus("alarm_set", gRtc.setAlarmTime(static_cast<uint8_t>(minute),
                                             static_cast<uint8_t>(hour),
                                             static_cast<uint8_t>(date)));
}

void cmdAlarmMatch(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0') {
    cmdAlarm();
    return;
  }
  int minute = 0;
  int hour = 0;
  int date = 0;
  char extra = '\0';
  if (sscanf(args, "%d %d %d %c", &minute, &hour, &date, &extra) != 3 ||
      (minute != 0 && minute != 1) || (hour != 0 && hour != 1) ||
      (date != 0 && date != 1)) {
    puts("Usage: alarm_match M H D (1=on, 0=off)");
    return;
  }
  char would[96] = {};
  snprintf(would, sizeof(would), "alarm match flags to minute=%d hour=%d date=%d",
           minute, hour, date);
  if (!requireConfirmation(confirmed, original, would,
                           "volatile (RTC alarm registers)",
                           "alarm changes can alter wake/interrupt behavior")) {
    return;
  }
  printStatus("alarm_match", gRtc.setAlarmMatch(minute != 0, hour != 0, date != 0));
}

void cmdAlarmInt(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0') {
    bool enabled = false;
    RV3032::Status st = gRtc.getAlarmInterruptEnabled(enabled);
    printStatus("alarm_int", st);
    if (st.ok()) {
      printf("alarm interrupt=%d\n", enabled ? 1 : 0);
    }
    return;
  }
  bool enable = false;
  if (!parseBoolArg(args, &enable)) {
    puts("Usage: alarm_int [0|1]");
    return;
  }
  if (!requireConfirmation(confirmed, original,
                           enable ? "alarm interrupt output to enabled" : "alarm interrupt output to disabled",
                           "volatile (RTC interrupt control register)",
                           "interrupt changes can alter wake and GPIO behavior")) {
    return;
  }
  printStatus("alarm_int", gRtc.enableAlarmInterrupt(enable));
}

void cmdAlarmClear(bool confirmed, const char* original) {
  if (!requireConfirmation(confirmed, original, "alarm flag AF to cleared",
                           "volatile (RTC status register)",
                           "clearing flags acknowledges events and cannot be reconstructed from the RTC")) {
    return;
  }
  printStatus("alarm_clear", gRtc.clearAlarmFlag());
}

void cmdTimer(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0') {
    uint16_t ticks = 0;
    RV3032::TimerFrequency freq = RV3032::TimerFrequency::Hz1;
    bool enabled = false;
    RV3032::Status st = gRtc.getTimer(ticks, freq, enabled);
    printStatus("timer", st);
    if (st.ok()) {
      printf("ticks=%u freq=%s enabled=%d\n", ticks, timerFreqName(freq), enabled ? 1 : 0);
    }
    return;
  }
  int ticks = 0;
  int freq = 0;
  int enable = 0;
  char extra = '\0';
  if (sscanf(args, "%d %d %d %c", &ticks, &freq, &enable, &extra) != 3 ||
      ticks < 0 || ticks > 4095 || freq < 0 || freq > 3 ||
      (enable != 0 && enable != 1)) {
    puts("Usage: timer <ticks 0..4095> <freq 0..3> <en 0|1>");
    return;
  }
  char would[96] = {};
  snprintf(would, sizeof(would), "timer to ticks=%d freq=%s enabled=%d",
           ticks, timerFreqName(static_cast<RV3032::TimerFrequency>(freq)), enable);
  if (!requireConfirmation(confirmed, original, would,
                           "volatile (RTC timer/control registers)",
                           "timer changes can alter periodic interrupt behavior")) {
    return;
  }
  printStatus("timer", gRtc.setTimer(static_cast<uint16_t>(ticks),
                                     static_cast<RV3032::TimerFrequency>(freq),
                                     enable != 0));
}

void cmdClkout(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0') {
    bool enabled = false;
    RV3032::Status st = gRtc.getClkoutEnabled(enabled);
    printStatus("clkout", st);
    if (st.ok()) {
      printf("clkout=%s\n", enabled ? "enabled" : "disabled");
    }
    return;
  }
  bool enable = false;
  if (!parseBoolArg(args, &enable)) {
    puts("Usage: clkout [0|1]");
    return;
  }
  if (!requireConfirmation(confirmed, original,
                           enable ? "CLKOUT output to enabled" : "CLKOUT output to disabled",
                           eepromBackedScope(),
                           "CLKOUT can drive an external pin and may be persisted to EEPROM")) {
    return;
  }
  printStatus("clkout", gRtc.setClkoutEnabled(enable));
}

void cmdClkoutFreq(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0') {
    RV3032::ClkoutFrequency freq = RV3032::ClkoutFrequency::Hz32768;
    RV3032::Status st = gRtc.getClkoutFrequency(freq);
    printStatus("clkout_freq", st);
    if (st.ok()) {
      printf("freq=%s\n", clkoutFreqName(freq));
    }
    return;
  }
  uint32_t raw = 0;
  if (!parseU32(args, &raw) || raw > 3U) {
    puts("Usage: clkout_freq [0..3]");
    return;
  }
  const RV3032::ClkoutFrequency freq = static_cast<RV3032::ClkoutFrequency>(raw);
  char would[96] = {};
  snprintf(would, sizeof(would), "CLKOUT frequency to %s", clkoutFreqName(freq));
  if (!requireConfirmation(confirmed, original, would, eepromBackedScope(),
                           "CLKOUT frequency can affect attached circuitry and may be persisted to EEPROM")) {
    return;
  }
  printStatus("clkout_freq", gRtc.setClkoutFrequency(freq));
}

void cmdOffset(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0') {
    float ppm = 0.0f;
    RV3032::Status st = gRtc.getOffsetPpm(ppm);
    printStatus("offset", st);
    if (st.ok()) {
      printf("offset=%.2f ppm\n", static_cast<double>(ppm));
    }
    return;
  }
  float ppm = 0.0f;
  if (!parseFloatArg(args, &ppm)) {
    puts("Usage: offset [ppm]");
    return;
  }
  char would[96] = {};
  snprintf(would, sizeof(would), "frequency offset calibration to %.2f ppm",
           static_cast<double>(ppm));
  if (!requireConfirmation(confirmed, original, would, eepromBackedScope(),
                           "offset calibration affects RTC rate and may be persisted to EEPROM")) {
    return;
  }
  printStatus("offset", gRtc.setOffsetPpm(ppm));
}

void cmdEvi(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0') {
    RV3032::EviConfig cfg;
    RV3032::Status st = gRtc.getEviConfig(cfg);
    printStatus("evi", st);
    if (st.ok()) {
      printf("edge=%s debounce=%s overwrite=%d\n",
             cfg.rising ? "rising" : "falling",
             debounceName(cfg.debounce),
             cfg.overwrite ? 1 : 0);
    }
    return;
  }

  char* rest = nullptr;
  char* sub = splitFirstToken(args, &rest);
  if (strcmp(sub, "edge") == 0) {
    rest = trim(rest);
    if (*rest == '\0') {
      cmdEvi(rest, false, original);
      return;
    }
    bool rising = false;
    if (!parseBoolArg(rest, &rising)) {
      puts("Usage: evi edge [0|1]");
      return;
    }
    if (!requireConfirmation(confirmed, original,
                             rising ? "EVI edge to rising" : "EVI edge to falling",
                             "volatile (RTC EVI control register)",
                             "event-input changes can alter timestamp capture behavior")) {
      return;
    }
    printStatus("evi edge", gRtc.setEviEdge(rising));
    return;
  }
  if (strcmp(sub, "debounce") == 0) {
    rest = trim(rest);
    if (*rest == '\0') {
      cmdEvi(rest, false, original);
      return;
    }
    uint32_t raw = 0;
    if (!parseU32(rest, &raw) || raw > 3U) {
      puts("Usage: evi debounce [0..3]");
      return;
    }
    const RV3032::EviDebounce debounce = static_cast<RV3032::EviDebounce>(raw);
    char would[96] = {};
    snprintf(would, sizeof(would), "EVI debounce to %s", debounceName(debounce));
    if (!requireConfirmation(confirmed, original, would,
                             "volatile (RTC EVI control register)",
                             "event-input changes can alter timestamp capture behavior")) {
      return;
    }
    printStatus("evi debounce", gRtc.setEviDebounce(debounce));
    return;
  }
  if (strcmp(sub, "overwrite") == 0) {
    rest = trim(rest);
    if (*rest == '\0') {
      cmdEvi(rest, false, original);
      return;
    }
    bool enable = false;
    if (!parseBoolArg(rest, &enable)) {
      puts("Usage: evi overwrite [0|1]");
      return;
    }
    if (!requireConfirmation(confirmed, original,
                             enable ? "timestamp overwrite to enabled" : "timestamp overwrite to disabled",
                             "volatile (RTC timestamp control register)",
                             "timestamp overwrite changes whether earlier captured events are retained")) {
      return;
    }
    printStatus("evi overwrite", gRtc.setEviOverwrite(enable));
    return;
  }
  puts("Usage: evi [edge|debounce|overwrite] ...");
}

void cmdTs(char* args) {
  RV3032::TimestampSource source = RV3032::TimestampSource::TLow;
  RV3032::Timestamp ts;
  RV3032::Status st = parseTimestampSource(args, &source)
                          ? gRtc.readTimestamp(source, ts)
                          : RV3032::Status::Error(RV3032::Err::INVALID_PARAM,
                                                  "usage: ts <tlow|thigh|evi>");
  printStatus("ts", st);
  if (st.ok()) {
    printf("%s count=%u", timestampSourceName(source), static_cast<unsigned>(ts.count));
    if (ts.hasHundredths) {
      printf(" hundredths=%u", static_cast<unsigned>(ts.hundredths));
    }
    putchar('\n');
    printDateTime(ts.time);
  }
}

void cmdTsReset(char* args, bool confirmed, const char* original) {
  RV3032::TimestampSource source = RV3032::TimestampSource::TLow;
  if (!parseTimestampSource(args, &source)) {
    puts("Usage: ts_reset <tlow|thigh|evi>");
    return;
  }
  char would[96] = {};
  snprintf(would, sizeof(would), "%s timestamp block reset bit", timestampSourceName(source));
  if (!requireConfirmation(confirmed, original, would,
                           "volatile (RTC timestamp control register)",
                           "timestamp reset clears captured event data for that source")) {
    return;
  }
  printStatus("ts_reset", gRtc.resetTimestamp(source));
}

void cmdStatus() {
  uint8_t value = 0;
  RV3032::Status st = gRtc.readStatus(value);
  printStatus("status", st);
  if (st.ok()) {
    printf("status=0x%02X\n", value);
  }
}

void cmdStatusFlags() {
  RV3032::StatusFlags flags;
  RV3032::Status st = gRtc.readStatusFlags(flags);
  printStatus("statusf", st);
  if (st.ok()) {
    printf("THF=%d TLF=%d UF=%d TF=%d AF=%d EVF=%d PORF=%d VLF=%d\n",
           flags.tempHigh ? 1 : 0, flags.tempLow ? 1 : 0, flags.update ? 1 : 0,
           flags.timer ? 1 : 0, flags.alarm ? 1 : 0, flags.event ? 1 : 0,
           flags.powerOnReset ? 1 : 0, flags.voltageLow ? 1 : 0);
  }
}

void cmdValidity() {
  RV3032::ValidityFlags flags;
  RV3032::Status st = gRtc.readValidity(flags);
  printStatus("validity", st);
  if (st.ok()) {
    printf("PORF=%d VLF=%d BSF=%d invalid=%d\n", flags.powerOnReset ? 1 : 0,
           flags.voltageLow ? 1 : 0, flags.backupSwitched ? 1 : 0,
           flags.timeInvalid ? 1 : 0);
  }
}

void cmdStatusClear(char* args, bool confirmed, const char* original) {
  args = trim(args);
  uint32_t mask = 0xFFU;
  if (*args != '\0' && !parseU32(args, &mask)) {
    puts("Usage: status_clear [mask 0..0xFF]");
    return;
  }
  if (mask > 0xFFU) {
    puts("Usage: status_clear [mask 0..0xFF]");
    return;
  }
  char would[96] = {};
  snprintf(would, sizeof(would), "status flags selected by mask 0x%02lX to cleared",
           static_cast<unsigned long>(mask));
  if (!requireConfirmation(confirmed, original, would,
                           "volatile (RTC status register)",
                           "clearing flags acknowledges events and cannot be reconstructed from the RTC")) {
    return;
  }
  printStatus("status_clear", gRtc.clearStatus(static_cast<uint8_t>(mask)));
}

void cmdRam(char* args) {
  uint32_t offset = 0;
  uint32_t len = 16;
  const char* tail = nullptr;
  args = trim(args);
  if (*args != '\0') {
    if (!parseU32(args, &offset, &tail)) {
      puts("Usage: ram [offset 0..15] [len 1..16]");
      return;
    }
    if (*tail != '\0' && !parseU32(tail, &len)) {
      puts("Usage: ram [offset 0..15] [len 1..16]");
      return;
    }
  }
  if (offset >= 16U || len == 0U || len > (16U - offset)) {
    puts("Usage: ram [offset 0..15] [len 1..16]");
    return;
  }
  uint8_t buf[16] = {};
  RV3032::Status st = gRtc.readUserRam(static_cast<uint8_t>(offset), buf, len);
  printStatus("ram", st);
  if (st.ok()) {
    for (uint32_t i = 0; i < len; ++i) {
      printf("%02X ", buf[i]);
    }
    putchar('\n');
  }
}

void cmdRamWrite(char* args, bool confirmed, const char* original) {
  char* rest = trim(args);
  char* tail = nullptr;
  char* token = splitFirstToken(rest, &tail);
  uint32_t offset = 0;
  if (*token == '\0' || !parseU32(token, &offset) || offset >= 16U) {
    puts("Usage: ram_write <offset 0..15> <byte...>");
    return;
  }
  uint8_t values[16] = {};
  size_t len = 0;
  rest = tail;
  while (*(rest = trim(rest)) != '\0') {
    token = splitFirstToken(rest, &tail);
    uint32_t value = 0;
    if (*token == '\0' || !parseU32(token, &value) || value > 0xFFU ||
        len >= static_cast<size_t>(16U - offset)) {
      puts("Usage: ram_write <offset 0..15> <byte...>");
      return;
    }
    values[len++] = static_cast<uint8_t>(value);
    rest = tail;
  }
  if (len == 0U) {
    puts("Usage: ram_write <offset 0..15> <byte...>");
    return;
  }
  char would[96] = {};
  snprintf(would, sizeof(would), "user RAM offset %lu with %u byte(s)",
           static_cast<unsigned long>(offset), static_cast<unsigned>(len));
  if (!requireConfirmation(confirmed, original, would,
                           "volatile (RTC user RAM)",
                           "user RAM writes replace application-owned scratch data")) {
    return;
  }
  printStatus("ram_write", gRtc.writeUserRam(static_cast<uint8_t>(offset), values, len));
}

void cmdReg(char* args, bool confirmed, const char* original) {
  char* rest = trim(args);
  char* tail = nullptr;
  char* regToken = splitFirstToken(rest, &tail);
  uint32_t reg = 0;
  if (*regToken == '\0' || !parseU32(regToken, &reg) || reg > 0xFFU) {
    puts("Usage: reg <addr> [val]");
    return;
  }
  rest = trim(tail);
  if (*rest == '\0') {
    uint8_t read = 0;
    RV3032::Status st = gRtc.readRegister(static_cast<uint8_t>(reg), read);
    printStatus("reg", st);
    if (st.ok()) {
      printf("reg[0x%02lX]=0x%02X\n", static_cast<unsigned long>(reg), read);
    }
    return;
  }
  char* valueToken = splitFirstToken(rest, &tail);
  tail = trim(tail);
  uint32_t value = 0;
  if (*tail != '\0' || !parseU32(valueToken, &value) || value > 0xFFU) {
    puts("Usage: reg <addr> [val]");
    return;
  }
  char would[96] = {};
  snprintf(would, sizeof(would), "register 0x%02lX to 0x%02lX",
           static_cast<unsigned long>(reg), static_cast<unsigned long>(value));
  if (!requireConfirmation(confirmed, original, would,
                           registerWriteScope(static_cast<uint8_t>(reg)),
                           "direct register writes can disrupt RTC operation or EEPROM access state")) {
    return;
  }
  printStatus("reg", gRtc.writeRegister(static_cast<uint8_t>(reg),
                                        static_cast<uint8_t>(value)));
}

void cmdEeprom() {
  RV3032::SettingsSnapshot snap = gRtc.getSettings();
  puts("EEPROM:");
  printf("busy=%d writes=%lu failures=%lu queue=%u persistence=%d\n",
         snap.eepromBusy ? 1 : 0, static_cast<unsigned long>(snap.eepromWriteCount),
         static_cast<unsigned long>(snap.eepromWriteFailures), snap.eepromQueueDepth,
         snap.enableEepromWrites ? 1 : 0);
  RV3032::Status eepromSt = gRtc.getEepromStatus();
  printStatus("eeprom_status", eepromSt);

  struct EepromReg {
    uint8_t reg;
    const char* name;
  };
  static constexpr EepromReg REGS[] = {
    {RV3032::cmd::REG_EEPROM_PMU, "PMU"},
    {RV3032::cmd::REG_EEPROM_OFFSET, "OFFSET"},
    {RV3032::cmd::REG_EEPROM_CLKOUT1, "CLKOUT1"},
    {RV3032::cmd::REG_EEPROM_CLKOUT2, "CLKOUT2"},
    {RV3032::cmd::REG_EEPROM_TREFERENCE0, "TREF0"},
    {RV3032::cmd::REG_EEPROM_TREFERENCE1, "TREF1"},
  };
  for (size_t i = 0; i < (sizeof(REGS) / sizeof(REGS[0])); ++i) {
    uint8_t value = 0;
    RV3032::Status st = gRtc.readRegister(REGS[i].reg, value);
    if (!st.ok()) {
      printStatus("eeprom_reg", st);
      return;
    }
    printf("%s[0x%02X]=0x%02X\n", REGS[i].name, REGS[i].reg, value);
  }

  if (snap.eepromBusy) {
    puts("User EEPROM dump skipped while EEPROM update is busy.");
    return;
  }
  puts("User EEPROM 0xCB..0xEA:");
  static constexpr uint8_t START = RV3032::cmd::USER_EEPROM_START;
  static constexpr uint8_t END = RV3032::cmd::USER_EEPROM_END;
  for (uint8_t addr = START; addr <= END; ++addr) {
    uint8_t value = 0;
    RV3032::Status st = readUserEepromByte(addr, &value);
    if (!st.ok()) {
      printStatus("eeprom_read", st);
      return;
    }
    if (((addr - START) % 8U) == 0U) {
      printf("0x%02X: ", addr);
    }
    printf("%02X ", value);
    if (((addr - START) % 8U) == 7U || addr == END) {
      putchar('\n');
    }
  }
}

void cmdBackup(char* args, bool confirmed, const char* original) {
  args = trim(args);
  if (*args == '\0' || strcmp(args, "status") == 0) {
    RV3032::BackupSwitchMode mode = RV3032::BackupSwitchMode::Level;
    RV3032::Status st = gRtc.getBackupSwitchMode(mode);
    printStatus("backup", st);
    if (st.ok()) {
      printf("backup=%s\n", backupModeName(mode));
    }
    cmdValidity();
    return;
  }
  RV3032::BackupSwitchMode mode = RV3032::BackupSwitchMode::Off;
  bool usualDefaults = false;
  if (!parseBackupMode(args, &mode, &usualDefaults)) {
    puts("Usage: backup [usual|off|level|direct]");
    return;
  }
  char would[128] = {};
  if (usualDefaults) {
    snprintf(would, sizeof(would), "battery backup PMU to primary-cell defaults (level switching, trickle off)");
  } else {
    snprintf(would, sizeof(would), "battery backup switch mode to %s", backupModeName(mode));
  }
  if (!requireConfirmation(confirmed, original, would, eepromBackedScope(),
                           "backup PMU changes affect time retention and may be persisted to EEPROM")) {
    return;
  }
  printStatus("backup", usualDefaults ? gRtc.setPrimaryBatteryBackupDefaults()
                                      : gRtc.setBackupSwitchMode(mode));
}

void cmdClearPorf(bool confirmed, const char* original) {
  if (!requireConfirmation(confirmed, original, "PORF power-on-reset flag to cleared",
                           "volatile (RTC status register)",
                           "clearing validity flags acknowledges power events after time verification")) {
    return;
  }
  printStatus("clear_porf", gRtc.clearPowerOnResetFlag());
}

void cmdClearVlf(bool confirmed, const char* original) {
  if (!requireConfirmation(confirmed, original, "VLF voltage-low flag to cleared",
                           "volatile (RTC status register)",
                           "clearing validity flags acknowledges power events after time verification")) {
    return;
  }
  printStatus("clear_vlf", gRtc.clearVoltageLowFlag());
}

void cmdClearBsf(bool confirmed, const char* original) {
  if (!requireConfirmation(confirmed, original, "BSF backup-switchover flag to cleared",
                           "volatile (RTC temperature/status register)",
                           "clearing backup flags acknowledges switchover events")) {
    return;
  }
  printStatus("clear_bsf", gRtc.clearBackupSwitchFlag());
}

void cmdVerbose(char* args) {
  args = trim(args);
  if (*args == '\0') {
    printf("verbose=%d\n", gVerbose ? 1 : 0);
    return;
  }
  bool enable = false;
  if (!parseBoolArg(args, &enable)) {
    puts("Usage: verbose [0|1]");
    return;
  }
  gVerbose = enable;
  printf("verbose=%d\n", gVerbose ? 1 : 0);
}

void cmdStress(char* args) {
  uint32_t iterations = 100;
  args = trim(args);
  if (*args != '\0' && !parseU32(args, &iterations)) {
    puts("Usage: stress [N]");
    return;
  }
  if (iterations < 1U) {
    iterations = 1U;
  }
  if (iterations > 100000U) {
    iterations = 100000U;
  }

  uint32_t ok = 0;
  uint32_t fail = 0;
  int64_t minUs = INT64_MAX;
  int64_t maxUs = 0;
  int64_t totalUs = 0;
  for (uint32_t i = 0; i < iterations; ++i) {
    RV3032::DateTime t;
    const int64_t start = esp_timer_get_time();
    RV3032::Status st = gRtc.readTime(t);
    const int64_t elapsed = esp_timer_get_time() - start;
    if (st.ok()) {
      ok++;
      totalUs += elapsed;
      if (elapsed < minUs) {
        minUs = elapsed;
      }
      if (elapsed > maxUs) {
        maxUs = elapsed;
      }
    } else {
      fail++;
      if (gVerbose) {
        printStatus("stress readTime", st);
      }
    }
    if ((i % 16U) == 15U) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  printf("stress: iterations=%lu ok=%lu fail=%lu\n",
         static_cast<unsigned long>(iterations), static_cast<unsigned long>(ok),
         static_cast<unsigned long>(fail));
  if (ok > 0U) {
    printf("timing_us min=%lld max=%lld avg=%lld\n",
           static_cast<long long>(minUs), static_cast<long long>(maxUs),
           static_cast<long long>(totalUs / static_cast<int64_t>(ok)));
  }
}

void cmdStressMix(char* args) {
  uint32_t iterations = 50;
  args = trim(args);
  if (*args != '\0' && !parseU32(args, &iterations)) {
    puts("Usage: stress_mix [N]");
    return;
  }
  if (iterations < 1U) {
    iterations = 1U;
  }
  if (iterations > 100000U) {
    iterations = 100000U;
  }

  uint32_t ok = 0;
  uint32_t fail = 0;
  for (uint32_t i = 0; i < iterations; ++i) {
    RV3032::Status st;
    switch (i % 7U) {
      case 0: {
        RV3032::DateTime t;
        st = gRtc.readTime(t);
        break;
      }
      case 1: {
        uint32_t ts = 0;
        st = gRtc.readUnix(ts);
        break;
      }
      case 2: {
        float c = 0.0f;
        st = gRtc.readTemperatureC(c);
        break;
      }
      case 3: {
        uint8_t status = 0;
        st = gRtc.readStatus(status);
        break;
      }
      case 4: {
        RV3032::ValidityFlags flags;
        st = gRtc.readValidity(flags);
        break;
      }
      case 5: {
        RV3032::AlarmConfig alarm;
        st = gRtc.getAlarmConfig(alarm);
        break;
      }
      default: {
        float ppm = 0.0f;
        st = gRtc.getOffsetPpm(ppm);
        break;
      }
    }
    if (st.ok()) {
      ok++;
    } else {
      fail++;
      if (gVerbose) {
        printStatus("stress_mix op", st);
      }
    }
    if ((i % 16U) == 15U) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  printf("stress_mix: iterations=%lu ok=%lu fail=%lu\n",
         static_cast<unsigned long>(iterations), static_cast<unsigned long>(ok),
         static_cast<unsigned long>(fail));
}

void cmdSelftest() {
  struct Counts {
    uint32_t pass;
    uint32_t fail;
  } counts = {0, 0};
  auto report = [&counts](const char* name, RV3032::Status st) {
    const bool ok = st.ok();
    printf("[%s] %s", ok ? "PASS" : "FAIL", name);
    if (!ok && st.msg != nullptr) {
      printf(" - %s", st.msg);
    }
    putchar('\n');
    if (ok) {
      counts.pass++;
    } else {
      counts.fail++;
    }
  };

  report("probe", gRtc.probe());
  RV3032::DateTime t;
  report("readTime", gRtc.readTime(t));
  uint32_t ts = 0;
  report("readUnix", gRtc.readUnix(ts));
  float temp = 0.0f;
  report("readTemperatureC", gRtc.readTemperatureC(temp));
  uint8_t status = 0;
  report("readStatus", gRtc.readStatus(status));
  RV3032::StatusFlags statusFlags;
  report("readStatusFlags", gRtc.readStatusFlags(statusFlags));
  RV3032::ValidityFlags validity;
  report("readValidity", gRtc.readValidity(validity));
  RV3032::AlarmConfig alarm;
  report("getAlarmConfig", gRtc.getAlarmConfig(alarm));
  uint16_t ticks = 0;
  RV3032::TimerFrequency timerFreq = RV3032::TimerFrequency::Hz1;
  bool timerEnabled = false;
  report("getTimer", gRtc.getTimer(ticks, timerFreq, timerEnabled));
  bool clkoutEnabled = false;
  report("getClkoutEnabled", gRtc.getClkoutEnabled(clkoutEnabled));
  RV3032::ClkoutFrequency clkFreq = RV3032::ClkoutFrequency::Hz32768;
  report("getClkoutFrequency", gRtc.getClkoutFrequency(clkFreq));
  RV3032::EviConfig evi;
  report("getEviConfig", gRtc.getEviConfig(evi));
  uint8_t ram = 0;
  report("readUserRam", gRtc.readUserRam(0, &ram, 1));
  printf("selftest: pass=%lu fail=%lu\n", static_cast<unsigned long>(counts.pass),
         static_cast<unsigned long>(counts.fail));
}

void handleCommand(char* line) {
  char* full = trim(line);
  if (*full == '\0') {
    return;
  }

  char original[LINE_LEN] = {};
  snprintf(original, sizeof(original), "%s", full);

  char* args = nullptr;
  char* cmd = splitFirstToken(full, &args);
  const bool confirmed = stripTrailingConfirm(args);
  args = trim(args);

  if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
    printHelp();
  } else if (strcmp(cmd, "version") == 0 || strcmp(cmd, "ver") == 0) {
    printf("RV3032 %s %s\n", RV3032::VERSION, RV3032::VERSION_FULL);
  } else if (strcmp(cmd, "scan") == 0) {
    scanBus();
  } else if (strcmp(cmd, "probe") == 0) {
    printStatus("probe", gRtc.probe());
  } else if (strcmp(cmd, "recover") == 0) {
    printStatus("recover", gRtc.recover());
  } else if (strcmp(cmd, "drv") == 0 || strcmp(cmd, "cfg") == 0 ||
             strcmp(cmd, "settings") == 0) {
    printDrv();
  } else if (strcmp(cmd, "read") == 0 || strcmp(cmd, "time") == 0) {
    cmdTime();
  } else if (strcmp(cmd, "set") == 0) {
    cmdSet(args, confirmed, original);
  } else if (strcmp(cmd, "setbuild") == 0) {
    cmdSetBuild(confirmed, original);
  } else if (strcmp(cmd, "unix") == 0) {
    cmdUnix(args, confirmed, original);
  } else if (strcmp(cmd, "temp") == 0) {
    cmdTemp();
  } else if (strcmp(cmd, "alarm") == 0) {
    cmdAlarm();
  } else if (strcmp(cmd, "alarm_set") == 0) {
    cmdAlarmSet(args, confirmed, original);
  } else if (strcmp(cmd, "alarm_match") == 0) {
    cmdAlarmMatch(args, confirmed, original);
  } else if (strcmp(cmd, "alarm_int") == 0) {
    cmdAlarmInt(args, confirmed, original);
  } else if (strcmp(cmd, "alarm_clear") == 0) {
    cmdAlarmClear(confirmed, original);
  } else if (strcmp(cmd, "timer") == 0) {
    cmdTimer(args, confirmed, original);
  } else if (strcmp(cmd, "clkout") == 0) {
    cmdClkout(args, confirmed, original);
  } else if (strcmp(cmd, "clkout_freq") == 0) {
    cmdClkoutFreq(args, confirmed, original);
  } else if (strcmp(cmd, "offset") == 0) {
    cmdOffset(args, confirmed, original);
  } else if (strcmp(cmd, "evi") == 0) {
    cmdEvi(args, confirmed, original);
  } else if (strcmp(cmd, "ts") == 0) {
    cmdTs(args);
  } else if (strcmp(cmd, "ts_reset") == 0) {
    cmdTsReset(args, confirmed, original);
  } else if (strcmp(cmd, "status") == 0) {
    cmdStatus();
  } else if (strcmp(cmd, "statusf") == 0) {
    cmdStatusFlags();
  } else if (strcmp(cmd, "status_clear") == 0) {
    cmdStatusClear(args, confirmed, original);
  } else if (strcmp(cmd, "validity") == 0) {
    cmdValidity();
  } else if (strcmp(cmd, "ram") == 0) {
    cmdRam(args);
  } else if (strcmp(cmd, "ram_write") == 0) {
    cmdRamWrite(args, confirmed, original);
  } else if (strcmp(cmd, "reg") == 0) {
    cmdReg(args, confirmed, original);
  } else if (strcmp(cmd, "eeprom") == 0) {
    cmdEeprom();
  } else if (strcmp(cmd, "backup") == 0) {
    cmdBackup(args, confirmed, original);
  } else if (strcmp(cmd, "clear_porf") == 0) {
    cmdClearPorf(confirmed, original);
  } else if (strcmp(cmd, "clear_vlf") == 0) {
    cmdClearVlf(confirmed, original);
  } else if (strcmp(cmd, "clear_bsf") == 0) {
    cmdClearBsf(confirmed, original);
  } else if (strcmp(cmd, "verbose") == 0) {
    cmdVerbose(args);
  } else if (strcmp(cmd, "stress") == 0) {
    cmdStress(args);
  } else if (strcmp(cmd, "stress_mix") == 0) {
    cmdStressMix(args);
  } else if (strcmp(cmd, "selftest") == 0) {
    cmdSelftest();
  } else {
    puts("Unknown command. Try 'help'.");
  }
}

}  // namespace

extern "C" void app_main(void) {
  setvbuf(stdin, nullptr, _IONBF, 0);
  setvbuf(stdout, nullptr, _IONBF, 0);
  puts("\nRV3032-C7 native ESP-IDF CLI");
  if (!initBus()) {
    puts("I2C init failed");
  }
  beginDriver();
  printHelp();
  char line[LINE_LEN] = {};
  while (true) {
    printf("> ");
    if (fgets(line, sizeof(line), stdin) != nullptr) {
      handleCommand(line);
    }
    gRtc.tick(nowMs(nullptr));
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
