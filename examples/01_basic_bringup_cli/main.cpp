/**
 * @file main.cpp
 * @brief Interactive CLI example for RV3032-C7 RTC
 *
 * Demonstrates complete RTC functionality:
 * - Time reading and setting
 * - Alarm configuration
 * - Timer operations
 * - Clock output control
 * - Calibration (offset adjustment)
 * - Temperature monitoring
 *
 * Type 'help' for available commands.
 */

#include <Arduino.h>
#include <Wire.h>
#include <cstdlib>
#include <cstring>

#include "examples/common/BoardConfig.h"
#include "examples/common/BusDiag.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/CliStyle.h"
#include "examples/common/Log.h"
#include "RV3032/CommandTable.h"
#include "RV3032/Version.h"
#include "RV3032/RV3032.h"

static RV3032::RV3032 g_rtc;

static bool operation_accepted(const RV3032::Status& st) {
  return st.ok() || st.inProgress();
}

static const char* cooperative_suffix(const RV3032::Status& st) {
  return st.inProgress() ? " (cooperative job accepted)" : "";
}
static bool g_verbose = false;  // Verbose mode - disabled by default for production examples

static uint32_t rtc_now_ms(void*) {
  return millis();
}

static void rtc_wait_ms(uint32_t delayMs, void*) {
  delay(delayMs);
}

/**
 * @brief Convert DriverState enum to string.
 */
static const char* stateToStr(RV3032::DriverState state) {
  switch (state) {
    case RV3032::DriverState::UNINIT:   return "UNINIT";
    case RV3032::DriverState::READY:    return "READY";
    case RV3032::DriverState::DEGRADED: return "DEGRADED";
    case RV3032::DriverState::OFFLINE:  return "OFFLINE";
    default: return "UNKNOWN";
  }
}

static const char* stateColor(RV3032::DriverState state, bool online, uint8_t consecutiveFailures) {
  if (state == RV3032::DriverState::UNINIT) {
    return LOG_COLOR_RESET;
  }
  return LOG_COLOR_STATE(online, consecutiveFailures);
}

static const char* healthChangeColor(bool changed) {
  return changed ? LOG_COLOR_RED : LOG_COLOR_GREEN;
}

static const char* goodIfZeroColor(uint32_t value) {
  return (value == 0U) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
}

static const char* goodIfNonZeroColor(uint32_t value) {
  return (value > 0U) ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

static const char* skipCountColor(uint32_t value) {
  return (value > 0U) ? LOG_COLOR_YELLOW : LOG_COLOR_RESET;
}

static const char* onOffColor(bool enabled) {
  return enabled ? LOG_COLOR_GREEN : LOG_COLOR_RESET;
}

/**
 * @brief Convert Err enum to string.
 */
static const char* errToStr(RV3032::Err code) {
  switch (code) {
    case RV3032::Err::OK:                   return "OK";
    case RV3032::Err::NOT_INITIALIZED:      return "NOT_INITIALIZED";
    case RV3032::Err::INVALID_CONFIG:       return "INVALID_CONFIG";
    case RV3032::Err::I2C_ERROR:            return "I2C_ERROR";
    case RV3032::Err::TIMEOUT:              return "TIMEOUT";
    case RV3032::Err::INVALID_PARAM:        return "INVALID_PARAM";
    case RV3032::Err::INVALID_DATETIME:     return "INVALID_DATETIME";
    case RV3032::Err::DEVICE_NOT_FOUND:     return "DEVICE_NOT_FOUND";
    case RV3032::Err::EEPROM_WRITE_FAILED:  return "EEPROM_WRITE_FAILED";
    case RV3032::Err::REGISTER_READ_FAILED: return "REGISTER_READ_FAILED";
    case RV3032::Err::REGISTER_WRITE_FAILED:return "REGISTER_WRITE_FAILED";
    case RV3032::Err::QUEUE_FULL:           return "QUEUE_FULL";
    case RV3032::Err::BUSY:                 return "BUSY";
    case RV3032::Err::IN_PROGRESS:          return "IN_PROGRESS";
    case RV3032::Err::I2C_NACK_ADDR:        return "I2C_NACK_ADDR";
    case RV3032::Err::I2C_NACK_DATA:        return "I2C_NACK_DATA";
    case RV3032::Err::I2C_TIMEOUT:          return "I2C_TIMEOUT";
    case RV3032::Err::I2C_BUS:              return "I2C_BUS";
    case RV3032::Err::EEPROM_VERIFY_FAILED: return "EEPROM_VERIFY_FAILED";
    case RV3032::Err::EEPROM_CLEANUP_FAILED:return "EEPROM_CLEANUP_FAILED";
    case RV3032::Err::PRIMARY_CELL_ALREADY_ATTEMPTED:
      return "PRIMARY_CELL_ALREADY_ATTEMPTED";
    case RV3032::Err::JOB_RESULT_UNAVAILABLE:return "JOB_RESULT_UNAVAILABLE";
    case RV3032::Err::INCOHERENT_DATA:      return "INCOHERENT_DATA";
    default: return "UNKNOWN";
  }
}

/**
 * @brief Print verbose status details after any operation.
 */
static void print_verbose_status(const char* op, const RV3032::Status& st) {
  if (!g_verbose) return;
  
  Serial.println(F("  --- Verbose Status ---"));
  Serial.printf("  Operation: %s\n", op);
  Serial.printf("  Result: %s%s%s (code=%s, detail=%ld)\n",
                LOG_COLOR_RESULT(st.ok()),
                st.ok() ? "OK" : "FAILED",
                LOG_COLOR_RESET,
                errToStr(st.code),
                static_cast<long>(st.detail));
  if (st.msg) {
    Serial.printf("  Message: %s\n", st.msg);
  }
  
  // Driver health snapshot
  const RV3032::DriverState drvState = g_rtc.state();
  const bool online = g_rtc.isOnline();
  Serial.printf("  Driver State: %s%s%s\n",
                stateColor(drvState, online, g_rtc.consecutiveFailures()),
                stateToStr(drvState),
                LOG_COLOR_RESET);
  Serial.printf("  isOnline: %s%s%s\n",
                online ? LOG_COLOR_GREEN : LOG_COLOR_RED,
                log_bool_str(online),
                LOG_COLOR_RESET);
  Serial.printf("  Consecutive Failures: %d\n", g_rtc.consecutiveFailures());
  Serial.printf("  Total: success=%lu, failures=%lu\n",
                static_cast<unsigned long>(g_rtc.totalSuccess()),
                static_cast<unsigned long>(g_rtc.totalFailures()));
  
  uint32_t now = millis();
  uint32_t lastOk = g_rtc.lastOkMs();
  uint32_t lastErr = g_rtc.lastErrorMs();
  Serial.printf("  Last OK: %lu ms ago\n",
                static_cast<unsigned long>(lastOk > 0 ? (now - lastOk) : 0));
  if (lastErr > 0) {
    RV3032::Status lastError = g_rtc.lastError();
    Serial.printf("  Last Error: %lu ms ago (%s: %s)\n",
                  static_cast<unsigned long>(now - lastErr),
                  errToStr(lastError.code),
                  lastError.msg ? lastError.msg : "");
  }
  Serial.println(F("  ----------------------"));
}

static RV3032::Status read_user_eeprom_chunk(uint8_t offset, uint8_t length,
                                              uint8_t* out) {
  RV3032::Status st = g_rtc.startReadUserEepromJob(offset, length, millis(), 1000);
  const uint32_t started = millis();
  while (st.inProgress() && static_cast<uint32_t>(millis() - started) < 1000) {
    uint8_t used = 0;
    st = g_rtc.pollJob(millis(), 1, used);
    if (st.inProgress()) delay(1);
  }
  if (!st.ok()) return st;
  RV3032::PersistentReadResult result{};
  st = g_rtc.getPersistentReadJobResult(result);
  if (!st.ok() || !result.persistentVerified || result.length != length) return st;
  memcpy(out, result.data, length);
  return RV3032::Status::Ok();
}

/**
 * @brief Non-blocking line reader from Serial.
 */
static String read_line() {
  static String buffer;
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      String result = buffer;
      buffer = "";
      return result;
    }
    if (buffer.length() < 128) {
      buffer += c;
    }
  }
  return "";
}

static bool pop_token(String& text, String& token) {
  text.trim();
  if (text.length() == 0) {
    return false;
  }
  const int split = text.indexOf(' ');
  if (split < 0) {
    token = text;
    text = "";
    return true;
  }
  token = text.substring(0, split);
  text = text.substring(split + 1);
  return true;
}

static bool parse_u8_token(const String& token, uint8_t& value) {
  if (token.length() == 0) {
    return false;
  }
  char* end = nullptr;
  const unsigned long parsed = strtoul(token.c_str(), &end, 0);
  if (end == token.c_str() || *end != '\0' || parsed > 0xFFUL) {
    return false;
  }
  value = static_cast<uint8_t>(parsed);
  return true;
}

static bool parse_timestamp_source(String token, RV3032::TimestampSource& source) {
  token.trim();
  token.toLowerCase();
  if (token == "tlow" || token == "low") {
    source = RV3032::TimestampSource::TLow;
    return true;
  }
  if (token == "thigh" || token == "high") {
    source = RV3032::TimestampSource::THigh;
    return true;
  }
  if (token == "evi" || token == "event") {
    source = RV3032::TimestampSource::Evi;
    return true;
  }
  return false;
}

static const char* timestamp_source_name(RV3032::TimestampSource source) {
  switch (source) {
    case RV3032::TimestampSource::TLow: return "TLow";
    case RV3032::TimestampSource::THigh: return "THigh";
    case RV3032::TimestampSource::Evi: return "EVI";
    default: return "unknown";
  }
}

static RV3032::BackupSwitchMode backup_mode_from_pmu(uint8_t pmu) {
  switch (pmu & RV3032::cmd::PMU_BSM_MASK) {
    case RV3032::cmd::PMU_BSM_DIRECT:
      return RV3032::BackupSwitchMode::Direct;
    case RV3032::cmd::PMU_BSM_LEVEL:
      return RV3032::BackupSwitchMode::Level;
    default:
      return RV3032::BackupSwitchMode::Off;
  }
}

static const char* backup_mode_name(RV3032::BackupSwitchMode mode) {
  switch (mode) {
    case RV3032::BackupSwitchMode::Off: return "off";
    case RV3032::BackupSwitchMode::Level: return "level";
    case RV3032::BackupSwitchMode::Direct: return "direct";
    default: return "unknown";
  }
}

static bool parse_backup_mode(String token, RV3032::BackupSwitchMode& mode) {
  token.trim();
  token.toLowerCase();
  if (token == "off" || token == "0") {
    mode = RV3032::BackupSwitchMode::Off;
    return true;
  }
  if (token == "level" || token == "lsm" || token == "1") {
    mode = RV3032::BackupSwitchMode::Level;
    return true;
  }
  if (token == "direct" || token == "dsm" || token == "2") {
    mode = RV3032::BackupSwitchMode::Direct;
    return true;
  }
  return false;
}

/**
 * @brief Print available commands.
 */
static void print_help() {
  Serial.println();
  cli::printHelpHeader("RV3032-C7 CLI Help");
  Serial.printf("Version: %s\n", RV3032::VERSION);
  Serial.printf("Built:   %s\n", RV3032::BUILD_TIMESTAMP);
  Serial.printf("Commit:  %s (%s)\n", RV3032::GIT_COMMIT, RV3032::GIT_STATUS);

  cli::printHelpSection("Common");
  cli::printHelpItem("help / ?", "Show this help");
  cli::printHelpItem("version / ver", "Print firmware and library version info");
  cli::printHelpItem("scan", "Scan I2C bus");
  cli::printHelpItem("read", "Alias of time");
  cli::printHelpItem("cfg / settings", "Alias of drv");
  cli::printHelpItem("time", "Read current time");
  cli::printHelpItem("set [YYYY MM DD HH MM SS]", "Set time (no args = show)");
  cli::printHelpItem("setbuild", "Set time to build timestamp");
  cli::printHelpItem("unix [ts]", "Read or set Unix timestamp");
  cli::printHelpItem("temp", "Read temperature");

  cli::printHelpSection("Alarm And Timer");
  cli::printHelpItem("alarm", "Show alarm configuration");
  cli::printHelpItem("alarm_set [MM HH DD]", "Set alarm time (no args = show)");
  cli::printHelpItem("alarm_match [M H D]", "Set alarm match flags (no args = show)");
  cli::printHelpItem("alarm_int [0|1]", "Disable/enable alarm interrupt (no args = show)");
  cli::printHelpItem("alarm_clear", "Clear alarm flag");
  cli::printHelpItem("timer", "Show timer config");
  cli::printHelpItem("timer <ticks> <freq 0..3> <en 0|1>", "Set timer");

  cli::printHelpSection("Clock And Event");
  cli::printHelpItem("clkout [0|1]", "Disable/enable clock output (no args = show)");
  cli::printHelpItem("clkout_freq [0..3]", "Set clock frequency (no args = show)");
  cli::printHelpItem("offset [ppm]", "Read or set frequency offset");
  cli::printHelpItem("evi", "Show EVI config");
  cli::printHelpItem("evi edge [0|1]", "Set/read EVI edge (0=falling,1=rising)");
  cli::printHelpItem("evi debounce [0..3]", "Set/read EVI debounce");
  cli::printHelpItem("evi overwrite [0|1]", "Set/read EVI overwrite");
  cli::printHelpItem("ts <tlow|thigh|evi>", "Read decoded timestamp block");
  cli::printHelpItem("ts_reset <tlow|thigh|evi>", "Reset timestamp block");

  cli::printHelpSection("Status And Registers");
  cli::printHelpItem("status", "Read status register");
  cli::printHelpItem("statusf", "Read decoded status flags");
  cli::printHelpItem("status_clear <mask>", "Clear status flags by explicit mask");
  cli::printHelpItem("validity", "Read PORF/VLF/BSF validity flags");
  cli::printHelpItem("ram [offset len]", "Dump user RAM (default all 16 bytes)");
  cli::printHelpItem("ram_write <offset> <byte...>", "Write user RAM bytes");
  cli::printHelpItem("reg <addr>", "Read register byte");
  cli::printHelpItem("reg <addr> <val> [confirm]", "Write register byte; confirm required outside user RAM");
  cli::printHelpItem("eeprom", "EEPROM stats and user EEPROM dump");
  cli::printHelpItem("backup [off|level|direct]", "Show or set active battery backup PMU");
  cli::printHelpItem("primary-cell ensure CONFIRM-PRIMARY-CELL",
                     "Explicit one-shot primary-cell provisioning");
  cli::printHelpItem("clear_porf", "Clear power-on reset flag");
  cli::printHelpItem("clear_vlf", "Clear voltage low flag");
  cli::printHelpItem("clear_bsf", "Clear backup switchover flag");

  cli::printHelpSection("Diagnostics");
  cli::printHelpItem("drv", "Show driver state and health");
  cli::printHelpItem("probe", "Probe device (no health tracking)");
  cli::printHelpItem("recover", "Manual recovery attempt");
  cli::printHelpItem("verbose [0|1]", "Enable verbose status output (no args = show)");
  cli::printHelpItem("stress [N]", "Run N iterations stress test (default 100)");
  cli::printHelpItem("stress_mix [N]", "Run N iterations mixed operations test");
  cli::printHelpItem("selftest", "Run safe command self-test report");
  Serial.println();
}

static void cmd_version() {
  Serial.println("=== Version Info ===");
  Serial.printf("  Example firmware build: %s %s\n", __DATE__, __TIME__);
  Serial.printf("  RV3032 library version: %s\n", RV3032::VERSION);
  Serial.printf("  RV3032 library full: %s\n", RV3032::VERSION_FULL);
  Serial.printf("  RV3032 library build: %s\n", RV3032::BUILD_TIMESTAMP);
  Serial.printf("  RV3032 library commit: %s (%s)\n", RV3032::GIT_COMMIT, RV3032::GIT_STATUS);
}

/**
 * @brief Format and print DateTime structure.
 */
static void print_datetime(const RV3032::DateTime& dt) {
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d (weekday=%d)\n",
                dt.year, dt.month, dt.day,
                dt.hour, dt.minute, dt.second,
                dt.weekday);
}

static void warn_if_time_invalid() {
  RV3032::ValidityFlags flags;
  RV3032::Status st = g_rtc.readValidity(flags);
  if (!st.ok()) {
    LOGW("Time validity check failed: %s", st.msg);
    return;
  }
  if (flags.timeInvalid) {
    LOGW("RTC time may be invalid (PORF=%d, VLF=%d, BSF=%d)",
         flags.powerOnReset ? 1 : 0,
         flags.voltageLow ? 1 : 0,
         flags.backupSwitched ? 1 : 0);
  }
}

/**
 * @brief Handle 'time' command - read current time.
 */
static void cmd_time() {
  RV3032::DateTime dt;
  RV3032::Status st = g_rtc.readTime(dt);
  print_verbose_status("readTime", st);
  if (!st.ok()) {
    LOGE("readTime() failed: %s", st.msg);
    return;
  }
  Serial.print(F("Current time: "));
  print_datetime(dt);
  warn_if_time_invalid();
}

/**
 * @brief Handle 'set' command - set time.
 * Example: "set 2026 01 10 15 30 00"
 */
static void cmd_set(const String& args) {
  if (args.length() == 0) {
    cmd_time();
    return;
  }

  int year, month, day, hour, minute, second;
  if (sscanf(args.c_str(), "%d %d %d %d %d %d",
             &year, &month, &day, &hour, &minute, &second) != 6) {
    LOGE("Invalid format. Usage: set YYYY MM DD HH MM SS");
    return;
  }

  RV3032::DateTime dt;
  dt.year = year;
  dt.month = month;
  dt.day = day;
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;
  dt.weekday = RV3032::RV3032::computeWeekday(year, month, day);

  RV3032::Status st = g_rtc.setTime(dt);
  if (!st.ok()) {
    LOGE("setTime() failed: %s", st.msg);
  } else {
    LOGI("Time set successfully");
    print_datetime(dt);
  }
}

/**
 * @brief Handle 'setbuild' command - set time to build timestamp.
 */
static void cmd_setbuild() {
  RV3032::DateTime dt;
  if (!RV3032::RV3032::parseBuildTime(dt)) {
    LOGE("parseBuildTime() failed");
    return;
  }

  RV3032::Status st = g_rtc.setTime(dt);
  if (!st.ok()) {
    LOGE("setTime() failed: %s", st.msg);
  } else {
    LOGI("Time set to build timestamp:");
    print_datetime(dt);
  }
}

/**
 * @brief Handle 'unix' command - read/set Unix timestamp.
 */
static void cmd_unix(const String& args) {
  if (args.length() == 0) {
    uint32_t ts = 0;
    RV3032::Status st = g_rtc.readUnix(ts);
    if (!st.ok()) {
      LOGE("readUnix() failed: %s", st.msg);
      return;
    }
    Serial.printf("Unix timestamp: %lu\n", static_cast<unsigned long>(ts));
    warn_if_time_invalid();
    return;
  }

  const uint32_t ts = static_cast<uint32_t>(strtoul(args.c_str(), nullptr, 0));
  RV3032::Status st = g_rtc.setUnix(ts);
  if (!st.ok()) {
    LOGE("setUnix() failed: %s", st.msg);
    return;
  }
  LOGI("Unix timestamp set to %lu", static_cast<unsigned long>(ts));
}

/**
 * @brief Handle 'temp' command - read temperature.
 */
static void cmd_temp() {
  float celsius = 0;
  RV3032::Status st = g_rtc.readTemperatureC(celsius);
  if (!st.ok()) {
    LOGE("readTemperatureC() failed: %s", st.msg);
    return;
  }
  Serial.printf("Temperature: %.2f C\n", celsius);
}

static void cmd_ts(const String& args) {
  RV3032::TimestampSource source;
  if (!parse_timestamp_source(args, source)) {
    LOGE("Usage: ts <tlow|thigh|evi>");
    return;
  }

  RV3032::Timestamp ts;
  RV3032::Status st = g_rtc.readTimestamp(source, ts);
  if (!st.ok()) {
    LOGE("readTimestamp(%s) failed: %s", timestamp_source_name(source), st.msg);
    return;
  }

  Serial.printf("%s timestamp: count=%u", timestamp_source_name(source), ts.count);
  if (ts.hasHundredths) {
    Serial.printf(" hundredths=%u", ts.hundredths);
  }
  Serial.println();
  Serial.print(F("  Time: "));
  print_datetime(ts.time);
}

static void cmd_ts_reset(const String& args) {
  RV3032::TimestampSource source;
  if (!parse_timestamp_source(args, source)) {
    LOGE("Usage: ts_reset <tlow|thigh|evi>");
    return;
  }

  RV3032::Status st = g_rtc.resetTimestamp(source);
  if (!operation_accepted(st)) {
    LOGE("resetTimestamp(%s) failed: %s", timestamp_source_name(source), st.msg);
    return;
  }
  LOGI("%s timestamp reset requested%s", timestamp_source_name(source),
       cooperative_suffix(st));
}

/**
 * @brief Handle 'alarm' command - show alarm configuration.
 */
static void cmd_alarm() {
  RV3032::AlarmConfig cfg;
  RV3032::Status st = g_rtc.getAlarmConfig(cfg);
  if (!st.ok()) {
    LOGE("getAlarmConfig() failed: %s", st.msg);
    return;
  }

  Serial.printf("Alarm time: %02d:%02d (date=%02d)\n",
                cfg.hour, cfg.minute, cfg.date);
  Serial.printf("Match: minute=%d hour=%d date=%d\n",
                cfg.matchMinute, cfg.matchHour, cfg.matchDate);

  bool intEnabled = false;
  st = g_rtc.getAlarmInterruptEnabled(intEnabled);
  if (st.ok()) {
    Serial.printf("Interrupt: %s\n", intEnabled ? "enabled" : "disabled");
  }

  bool triggered = false;
  st = g_rtc.getAlarmFlag(triggered);
  if (st.ok()) {
    Serial.printf("Flag: %s\n", triggered ? "TRIGGERED" : "clear");
  }
}

/**
 * @brief Handle 'alarm_set' command - set alarm time.
 * Example: "alarm_set 30 15 10" (minute hour date)
 */
static void cmd_alarm_set(const String& args) {
  if (args.length() == 0) {
    RV3032::AlarmConfig cfg;
    RV3032::Status st = g_rtc.getAlarmConfig(cfg);
    if (!st.ok()) {
      LOGE("getAlarmConfig() failed: %s", st.msg);
      return;
    }
    Serial.printf("Alarm time: %02d:%02d (date=%02d)\n",
                  cfg.hour, cfg.minute, cfg.date);
    return;
  }

  int minute, hour, date;
  if (sscanf(args.c_str(), "%d %d %d", &minute, &hour, &date) != 3) {
    LOGE("Invalid format. Usage: alarm_set MM HH DD");
    return;
  }

  RV3032::Status st = g_rtc.setAlarmTime(minute, hour, date);
  if (!operation_accepted(st)) {
    LOGE("setAlarmTime() failed: %s", st.msg);
  } else {
    LOGI("Alarm time requested: %02d:%02d (date=%02d)%s",
         hour, minute, date, cooperative_suffix(st));
  }
}

/**
 * @brief Handle 'alarm_match' command - configure alarm matching.
 * Example: "alarm_match 1 1 0" (minute hour date)
 */
static void cmd_alarm_match(const String& args) {
  if (args.length() == 0) {
    RV3032::AlarmConfig cfg;
    RV3032::Status st = g_rtc.getAlarmConfig(cfg);
    if (!st.ok()) {
      LOGE("getAlarmConfig() failed: %s", st.msg);
      return;
    }
    Serial.printf("Match: minute=%d hour=%d date=%d\n",
                  cfg.matchMinute, cfg.matchHour, cfg.matchDate);
    return;
  }

  int matchMin, matchHour, matchDate;
  if (sscanf(args.c_str(), "%d %d %d", &matchMin, &matchHour, &matchDate) != 3) {
    LOGE("Invalid format. Usage: alarm_match M H D (1=on, 0=off)");
    return;
  }

  RV3032::Status st = g_rtc.setAlarmMatch(matchMin != 0, matchHour != 0, matchDate != 0);
  if (!operation_accepted(st)) {
    LOGE("setAlarmMatch() failed: %s", st.msg);
  } else {
    LOGI("Alarm match requested: minute=%d hour=%d date=%d%s",
         matchMin, matchHour, matchDate, cooperative_suffix(st));
  }
}

/**
 * @brief Handle 'alarm_int' command - enable/disable alarm interrupt.
 * Example: "alarm_int 1"
 */
static void cmd_alarm_int(const String& args) {
  if (args.length() == 0) {
    bool enabled = false;
    RV3032::Status st = g_rtc.getAlarmInterruptEnabled(enabled);
    if (!st.ok()) {
      LOGE("getAlarmInterruptEnabled() failed: %s", st.msg);
      return;
    }
    Serial.printf("Alarm interrupt: %s\n", enabled ? "enabled" : "disabled");
    return;
  }

  bool enable = (args.toInt() != 0);
  RV3032::Status st = g_rtc.enableAlarmInterrupt(enable);
  if (!operation_accepted(st)) {
    LOGE("enableAlarmInterrupt() failed: %s", st.msg);
  } else {
    LOGI("Alarm interrupt %s requested%s",
         enable ? "enabled" : "disabled", cooperative_suffix(st));
  }
}

/**
 * @brief Handle 'alarm_clear' command - clear alarm flag.
 */
static void cmd_alarm_clear() {
  RV3032::Status st = g_rtc.clearAlarmFlag();
  if (!operation_accepted(st)) {
    LOGE("clearAlarmFlag() failed: %s", st.msg);
  } else {
    LOGI("Alarm flag clear requested%s", cooperative_suffix(st));
  }
}

/**
 * @brief Handle 'clkout' command - enable/disable clock output.
 */
static void cmd_clkout(const String& args) {
  if (args.length() == 0) {
    bool enabled = false;
    RV3032::Status st = g_rtc.getClkoutEnabled(enabled);
    if (!st.ok()) {
      LOGE("getClkoutEnabled() failed: %s", st.msg);
      return;
    }
    Serial.printf("Clock output: %s\n", enabled ? "enabled" : "disabled");
    return;
  }

  bool enable = (args.toInt() != 0);
  RV3032::Status st = g_rtc.setClkoutEnabled(enable);
  if (st.ok()) {
    LOGI("Clock output %s", enable ? "enabled" : "disabled");
  } else if (st.code == RV3032::Err::IN_PROGRESS) {
    LOGI("Clock output %s requested%s", enable ? "enabled" : "disabled",
         cooperative_suffix(st));
  } else {
    LOGE("setClkoutEnabled() failed: %s", st.msg);
  }
}

/**
 * @brief Handle 'clkout_freq' command - set clock output frequency.
 * Example: "clkout_freq 3" (0=32768Hz, 1=1024Hz, 2=64Hz, 3=1Hz)
 */
static void cmd_clkout_freq(const String& args) {
  const char* freqStr[] = {"32768Hz", "1024Hz", "64Hz", "1Hz"};
  if (args.length() == 0) {
    RV3032::ClkoutFrequency freq = RV3032::ClkoutFrequency::Hz32768;
    RV3032::Status st = g_rtc.getClkoutFrequency(freq);
    if (!st.ok()) {
      LOGE("getClkoutFrequency() failed: %s", st.msg);
      return;
    }
    const uint8_t idx = static_cast<uint8_t>(freq);
    Serial.printf("Clock output frequency: %s\n", freqStr[idx]);
    return;
  }

  int freq = args.toInt();
  if (freq < 0 || freq > 3) {
    LOGE("Invalid frequency. Range: 0..3");
    return;
  }

  RV3032::ClkoutFrequency freqEnum = static_cast<RV3032::ClkoutFrequency>(freq);
  RV3032::Status st = g_rtc.setClkoutFrequency(freqEnum);
  if (st.ok()) {
    LOGI("Clock output frequency set to %s", freqStr[freq]);
  } else if (st.code == RV3032::Err::IN_PROGRESS) {
    LOGI("Clock output frequency %s requested%s", freqStr[freq],
         cooperative_suffix(st));
  } else {
    LOGE("setClkoutFrequency() failed: %s", st.msg);
  }
}

/**
 * @brief Handle 'offset' command - read or set frequency offset.
 * Example: "offset" (read) or "offset 5.2" (set to +5.2 ppm)
 */
static void cmd_offset(const String& args) {
  if (args.length() == 0) {
    // Read offset
    float ppm = 0;
    RV3032::Status st = g_rtc.getOffsetPpm(ppm);
    if (!st.ok()) {
      LOGE("getOffsetPpm() failed: %s", st.msg);
    } else {
      Serial.printf("Frequency offset: %.2f ppm\n", ppm);
    }
  } else {
    // Set offset
    float ppm = args.toFloat();
    RV3032::Status st = g_rtc.setOffsetPpm(ppm);
    if (st.ok()) {
      LOGI("Frequency offset set to %.2f ppm", ppm);
    } else if (st.code == RV3032::Err::IN_PROGRESS) {
      LOGI("Frequency offset %.2f ppm requested%s", ppm,
           cooperative_suffix(st));
    } else {
      LOGE("setOffsetPpm() failed: %s", st.msg);
    }
  }
}

static const char* timerFreqToStr(RV3032::TimerFrequency freq) {
  switch (freq) {
    case RV3032::TimerFrequency::Hz4096: return "4096Hz";
    case RV3032::TimerFrequency::Hz64:   return "64Hz";
    case RV3032::TimerFrequency::Hz1:    return "1Hz";
    case RV3032::TimerFrequency::Hz1_60: return "1/60Hz";
    default:                             return "UNKNOWN";
  }
}

static const char* eviDebounceToStr(RV3032::EviDebounce debounce) {
  switch (debounce) {
    case RV3032::EviDebounce::None:  return "None";
    case RV3032::EviDebounce::Hz256: return "256Hz";
    case RV3032::EviDebounce::Hz64:  return "64Hz";
    case RV3032::EviDebounce::Hz8:   return "8Hz";
    default:                         return "UNKNOWN";
  }
}

static void cmd_timer(const String& args) {
  if (args.length() == 0) {
    uint16_t ticks = 0;
    RV3032::TimerFrequency freq = RV3032::TimerFrequency::Hz1;
    bool enabled = false;
    RV3032::Status st = g_rtc.getTimer(ticks, freq, enabled);
    if (!st.ok()) {
      LOGE("getTimer() failed: %s", st.msg);
      return;
    }
    Serial.printf("Timer: ticks=%u freq=%s enabled=%d\n",
                  static_cast<unsigned>(ticks),
                  timerFreqToStr(freq),
                  enabled ? 1 : 0);
    return;
  }

  int ticks = 0;
  int freq = 0;
  int enable = 0;
  if (sscanf(args.c_str(), "%d %d %d", &ticks, &freq, &enable) != 3 ||
      ticks < 0 || ticks > 4095 || freq < 0 || freq > 3 ||
      (enable != 0 && enable != 1)) {
    LOGE("Usage: timer <ticks 0..4095> <freq 0..3> <en 0|1>");
    return;
  }

  RV3032::Status st = g_rtc.setTimer(static_cast<uint16_t>(ticks),
                                     static_cast<RV3032::TimerFrequency>(freq),
                                     enable != 0);
  if (!operation_accepted(st)) {
    LOGE("setTimer() failed: %s", st.msg);
    return;
  }
  LOGI("Timer requested: ticks=%d freq=%d enable=%d%s",
       ticks, freq, enable, cooperative_suffix(st));
}

static void cmd_evi(const String& args) {
  if (args.length() == 0) {
    RV3032::EviConfig cfg;
    RV3032::Status st = g_rtc.getEviConfig(cfg);
    if (!st.ok()) {
      LOGE("getEviConfig() failed: %s", st.msg);
      return;
    }
    Serial.printf("EVI: edge=%s debounce=%s overwrite=%d\n",
                  cfg.rising ? "rising" : "falling",
                  eviDebounceToStr(cfg.debounce),
                  cfg.overwrite ? 1 : 0);
    return;
  }

  const int split = args.indexOf(' ');
  const String sub = (split >= 0) ? args.substring(0, split) : args;
  String rest = (split >= 0) ? args.substring(split + 1) : "";
  rest.trim();

  if (sub == "edge") {
    if (rest.length() == 0) {
      cmd_evi("");
      return;
    }
    const int v = rest.toInt();
    if (v != 0 && v != 1) {
      LOGE("Usage: evi edge [0|1]");
      return;
    }
    RV3032::Status st = g_rtc.setEviEdge(v != 0);
    if (!operation_accepted(st)) {
      LOGE("setEviEdge() failed: %s", st.msg);
      return;
    }
    LOGI("EVI edge %s requested%s", v ? "rising" : "falling",
         cooperative_suffix(st));
    return;
  }

  if (sub == "debounce") {
    if (rest.length() == 0) {
      cmd_evi("");
      return;
    }
    const int v = rest.toInt();
    if (v < 0 || v > 3) {
      LOGE("Usage: evi debounce [0..3]");
      return;
    }
    RV3032::Status st = g_rtc.setEviDebounce(static_cast<RV3032::EviDebounce>(v));
    if (!operation_accepted(st)) {
      LOGE("setEviDebounce() failed: %s", st.msg);
      return;
    }
    LOGI("EVI debounce %d requested%s", v, cooperative_suffix(st));
    return;
  }

  if (sub == "overwrite") {
    if (rest.length() == 0) {
      cmd_evi("");
      return;
    }
    const int v = rest.toInt();
    if (v != 0 && v != 1) {
      LOGE("Usage: evi overwrite [0|1]");
      return;
    }
    RV3032::Status st = g_rtc.setEviOverwrite(v != 0);
    if (!operation_accepted(st)) {
      LOGE("setEviOverwrite() failed: %s", st.msg);
      return;
    }
    LOGI("EVI overwrite %d requested%s", v, cooperative_suffix(st));
    return;
  }

  LOGE("Usage: evi [edge|debounce|overwrite] ...");
}

static void cmd_statusf() {
  RV3032::StatusFlags flags;
  RV3032::Status st = g_rtc.readStatusFlags(flags);
  if (!st.ok()) {
    LOGE("readStatusFlags() failed: %s", st.msg);
    return;
  }
  Serial.println("Status flags:");
  Serial.printf("  tempHigh=%d tempLow=%d update=%d timer=%d\n",
                flags.tempHigh ? 1 : 0,
                flags.tempLow ? 1 : 0,
                flags.update ? 1 : 0,
                flags.timer ? 1 : 0);
  Serial.printf("  alarm=%d event=%d porf=%d vlf=%d\n",
                flags.alarm ? 1 : 0,
                flags.event ? 1 : 0,
                flags.powerOnReset ? 1 : 0,
                flags.voltageLow ? 1 : 0);
}

static void cmd_status_clear(const String& args) {
  if (args.length() == 0) {
    LOGE("Usage: status_clear <mask 0..0xFF>");
    return;
  }

  const unsigned long parsed = strtoul(args.c_str(), nullptr, 0);
  if (parsed > 0xFFUL) {
    LOGE("Usage: status_clear <mask 0..0xFF>");
    return;
  }
  const uint8_t mask = static_cast<uint8_t>(parsed);

  RV3032::Status st = g_rtc.clearStatus(mask);
  if (!operation_accepted(st)) {
    LOGE("clearStatus() failed: %s", st.msg);
    return;
  }
  LOGI("Status clear requested with mask=0x%02X%s", mask,
       cooperative_suffix(st));
}

static void cmd_reg(const String& args) {
  String trimmed = args;
  trimmed.trim();
  if (trimmed.length() == 0) {
    LOGE("Usage: reg <addr> [value]");
    return;
  }

  const int split = trimmed.indexOf(' ');
  const String addrTok = (split >= 0) ? trimmed.substring(0, split) : trimmed;
  String valueTok = (split >= 0) ? trimmed.substring(split + 1) : "";
  valueTok.trim();
  String confirmTok;
  const int valueSplit = valueTok.indexOf(' ');
  if (valueSplit >= 0) {
    confirmTok = valueTok.substring(valueSplit + 1);
    valueTok = valueTok.substring(0, valueSplit);
    valueTok.trim();
    confirmTok.trim();
    confirmTok.toLowerCase();
  }

  const unsigned long addrRaw = strtoul(addrTok.c_str(), nullptr, 0);
  if (addrRaw > 0xFFUL) {
    LOGE("Register address out of range");
    return;
  }
  const uint8_t reg = static_cast<uint8_t>(addrRaw);

  if (valueTok.length() == 0) {
    uint8_t value = 0;
    RV3032::Status st = g_rtc.readRegister(reg, value);
    if (!st.ok()) {
      LOGE("readRegister(0x%02X) failed: %s", reg, st.msg);
      return;
    }
    Serial.printf("reg[0x%02X] = 0x%02X\n", reg, value);
    return;
  }

  const unsigned long valueRaw = strtoul(valueTok.c_str(), nullptr, 0);
  if (valueRaw > 0xFFUL) {
    LOGE("Register value out of range");
    return;
  }

  const bool userRamReg = (reg >= RV3032::cmd::REG_USER_RAM_START &&
                           reg <= RV3032::cmd::REG_USER_RAM_END);
  if (!userRamReg && confirmTok != "confirm") {
    LOGE("Register writes outside user RAM require: reg <addr> <value> confirm");
    return;
  }

  RV3032::Status st = g_rtc.writeRegister(reg, static_cast<uint8_t>(valueRaw));
  if (!st.ok()) {
    LOGE("writeRegister(0x%02X) failed: %s", reg, st.msg);
    return;
  }
  LOGI("reg[0x%02X] <= 0x%02lX", reg, valueRaw);
}

static void cmd_ram(const String& args) {
  String rest = args;
  String token;
  uint8_t offset = 0;
  uint8_t len = 16;

  if (pop_token(rest, token)) {
    if (!parse_u8_token(token, offset) || offset >= 16) {
      LOGE("Usage: ram [offset 0..15] [len 1..16]");
      return;
    }
    len = static_cast<uint8_t>(16U - offset);
    if (pop_token(rest, token)) {
      if (!parse_u8_token(token, len) || len == 0 || len > static_cast<uint8_t>(16U - offset)) {
        LOGE("Usage: ram [offset 0..15] [len 1..16]");
        return;
      }
    }
  }
  rest.trim();
  if (rest.length() != 0) {
    LOGE("Usage: ram [offset 0..15] [len 1..16]");
    return;
  }

  uint8_t buf[16] = {};
  RV3032::Status st = g_rtc.readUserRam(offset, buf, len);
  if (!st.ok()) {
    LOGE("readUserRam() failed: %s", st.msg);
    return;
  }

  Serial.printf("User RAM offset %u len %u:\n",
                static_cast<unsigned>(offset),
                static_cast<unsigned>(len));
  for (uint8_t i = 0; i < len; ++i) {
    if ((i % 8) == 0) {
      Serial.printf("  0x%02X: ", static_cast<unsigned>(offset + i));
    }
    Serial.printf("%02X ", buf[i]);
    if ((i % 8) == 7 || i == static_cast<uint8_t>(len - 1U)) {
      Serial.println();
    }
  }
}

static void cmd_ram_write(const String& args) {
  String rest = args;
  String token;
  uint8_t offset = 0;
  if (!pop_token(rest, token) || !parse_u8_token(token, offset) || offset >= 16) {
    LOGE("Usage: ram_write <offset 0..15> <byte...>");
    return;
  }

  uint8_t values[16] = {};
  size_t len = 0;
  while (pop_token(rest, token)) {
    if (len >= static_cast<size_t>(16U - offset) || !parse_u8_token(token, values[len])) {
      LOGE("Usage: ram_write <offset 0..15> <byte...>");
      return;
    }
    len++;
  }
  if (len == 0) {
    LOGE("Usage: ram_write <offset 0..15> <byte...>");
    return;
  }

  RV3032::Status st = g_rtc.writeUserRam(offset, values, len);
  if (!st.ok()) {
    LOGE("writeUserRam() failed: %s", st.msg);
    return;
  }
  LOGI("Wrote %u byte(s) to user RAM offset %u",
       static_cast<unsigned>(len),
       static_cast<unsigned>(offset));
}

/**
 * @brief Handle 'status' command - read status register.
 */
static void cmd_status() {
  uint8_t status = 0;
  RV3032::Status st = g_rtc.readStatus(status);
  if (!st.ok()) {
    LOGE("readStatus() failed: %s", st.msg);
    return;
  }
  Serial.printf("Status register: 0x%02X (binary: ", status);
  for (int i = 7; i >= 0; --i) {
    Serial.print((status & (1 << i)) ? '1' : '0');
  }
  Serial.println(')');
}

/**
 * @brief Handle 'validity' command - read validity flags.
 */
static void cmd_validity() {
  RV3032::ValidityFlags flags;
  RV3032::Status st = g_rtc.readValidity(flags);
  if (!st.ok()) {
    LOGE("readValidity() failed: %s", st.msg);
    return;
  }

  Serial.printf("PORF: %s\n", flags.powerOnReset ? "set" : "clear");
  Serial.printf("VLF:  %s\n", flags.voltageLow ? "set" : "clear");
  Serial.printf("BSF:  %s\n", flags.backupSwitched ? "set" : "clear");
  Serial.printf("Time: %s\n", flags.timeInvalid ? "invalid" : "valid");
}

/**
 * @brief Handle 'eeprom' command - show EEPROM stats and dump user EEPROM.
 */
static void cmd_eeprom() {
  Serial.println();
  Serial.println(F("=== EEPROM ==="));

  const bool busy = g_rtc.isEepromBusy();
  Serial.printf("Busy: %s\n", busy ? "true" : "false");
  RV3032::Status eepromSt = g_rtc.getEepromStatus();
  Serial.printf("Status: %s%s%s\n",
                LOG_COLOR_RESULT(eepromSt.ok()),
                eepromSt.ok() ? "OK" : eepromSt.msg,
                LOG_COLOR_RESET);
  Serial.printf("Writes: %lu (failures: %lu)\n",
                static_cast<unsigned long>(g_rtc.eepromWriteCount()),
                static_cast<unsigned long>(g_rtc.eepromWriteFailures()));
  Serial.printf("Queue depth: %u\n", g_rtc.eepromQueueDepth());

  struct EepromReg {
    uint8_t reg;
    const char* name;
  };
  static const EepromReg kRegs[] = {
    {RV3032::cmd::REG_EEPROM_PMU, "PMU"},
    {RV3032::cmd::REG_EEPROM_OFFSET, "OFFSET"},
    {RV3032::cmd::REG_EEPROM_CLKOUT1, "CLKOUT1"},
    {RV3032::cmd::REG_EEPROM_CLKOUT2, "CLKOUT2"},
    {RV3032::cmd::REG_EEPROM_TREFERENCE0, "TREF0"},
    {RV3032::cmd::REG_EEPROM_TREFERENCE1, "TREF1"},
  };

  Serial.println(F("Active configuration mirrors (not persistent EEPROM):"));
  for (size_t i = 0; i < (sizeof(kRegs) / sizeof(kRegs[0])); ++i) {
    uint8_t value = 0;
    RV3032::Status st = g_rtc.readRegister(kRegs[i].reg, value);
    if (!st.ok()) {
      LOGE("readRegister(0x%02X) failed: %s", kRegs[i].reg, st.msg);
      return;
    }
    Serial.printf("  %s (0x%02X): 0x%02X\n", kRegs[i].name, kRegs[i].reg, value);
  }

  if (busy) {
    Serial.println(F("User EEPROM dump skipped while busy."));
    return;
  }

  Serial.println(F("User EEPROM (0xCB..0xEA):"));
  static constexpr uint8_t kSize = RV3032::USER_EEPROM_SIZE;
  uint8_t nonFF = 0;

  for (uint8_t offset = 0; offset < kSize; offset += 16) {
    uint8_t values[16] = {};
    RV3032::Status st = read_user_eeprom_chunk(offset, 16, values);
    if (!st.ok()) {
      LOGE("User EEPROM read failed at offset %u: %s", offset, st.msg);
      return;
    }
    for (uint8_t i = 0; i < 16; ++i) {
      if (values[i] != 0xFF) ++nonFF;
      if ((i % 8) == 0) {
        Serial.printf("  0x%02X: ",
                      RV3032::cmd::USER_EEPROM_START + offset + i);
      }
      Serial.printf("%02X ", values[i]);
      if ((i % 8) == 7) Serial.println();
    }
  }

  Serial.printf("Non-0xFF bytes: %u/%u (heuristic)\n", nonFF, kSize);
}

/**
 * @brief Handle 'backup' command - show or set battery-backup PMU settings.
 */
static void cmd_backup(const String& args) {
  String modeArg = args;
  modeArg.trim();
  modeArg.toLowerCase();

  if (modeArg.length() == 0 || modeArg == "status") {
    uint8_t pmu = 0;
    RV3032::Status st = g_rtc.readRegister(RV3032::cmd::REG_EEPROM_PMU, pmu);
    if (!st.ok()) {
      LOGE("readRegister(PMU) failed: %s", st.msg);
      return;
    }

    RV3032::SettingsSnapshot snap;
    (void)g_rtc.getSettings(snap);
    RV3032::ValidityFlags flags;
    st = g_rtc.readValidity(flags);
    if (!st.ok()) {
      LOGE("readValidity() failed: %s", st.msg);
      return;
    }

    const RV3032::BackupSwitchMode mode = backup_mode_from_pmu(pmu);
    const bool clkoutDisabled = (pmu & RV3032::cmd::PMU_CLKOUT_DISABLE) != 0;
    const bool trickleEnabled = (pmu & RV3032::cmd::PMU_TCM_MASK) != 0;

    Serial.println();
    Serial.println(F("=== Battery Backup ==="));
    Serial.printf("PMU 0xC0: 0x%02X\n", pmu);
    Serial.printf("Backup mode: %s%s%s\n",
                  (mode == RV3032::BackupSwitchMode::Level || mode == RV3032::BackupSwitchMode::Direct)
                      ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW,
                  backup_mode_name(mode),
                  LOG_COLOR_RESET);
    Serial.printf("EEPROM persistence: %s busy=%s queue=%u\n",
                  log_bool_str(snap.enableEepromWrites),
                  log_bool_str(snap.eepromBusy),
                  static_cast<unsigned>(snap.eepromQueueDepth));
    Serial.printf("CLKOUT disabled bit: %s\n", log_bool_str(clkoutDisabled));
    Serial.printf("Trickle charger bits: 0x%X (%s)\n",
                   static_cast<unsigned>(pmu &
                                         (RV3032::cmd::PMU_TCR_MASK |
                                          RV3032::cmd::PMU_TCM_MASK)),
                  trickleEnabled ? "enabled/configured" : "off");
    Serial.printf("Flags: PORF=%s VLF=%s BSF=%s time=%s\n",
                  flags.powerOnReset ? "set" : "clear",
                  flags.voltageLow ? "set" : "clear",
                  flags.backupSwitched ? "set" : "clear",
                  flags.timeInvalid ? "invalid" : "valid");
    Serial.println(F("Time is held by the RTC counter while VBACKUP is present; it is not stored in EEPROM."));
    Serial.println(F("Primary-cell provisioning is separate: primary-cell ensure CONFIRM-PRIMARY-CELL"));
    return;
  }

  RV3032::Status st = RV3032::Status::Ok();
  RV3032::BackupSwitchMode mode = RV3032::BackupSwitchMode::Off;
  if (!parse_backup_mode(modeArg, mode)) {
    LOGE("Expected backup [off|level|direct]");
    return;
  }

  st = g_rtc.setBackupSwitchMode(mode);
  if (st.ok() || st.code == RV3032::Err::IN_PROGRESS) {
    LOGI("Backup mode %s requested%s",
         backup_mode_name(mode),
         cooperative_suffix(st));
  } else {
    LOGE("setBackupSwitchMode() failed: %s", st.msg);
  }
}

static void cmd_primary_cell(const String& args) {
  String confirmation = args;
  confirmation.trim();
  if (confirmation != "ensure CONFIRM-PRIMARY-CELL") {
    LOGE("Exact confirmation required: primary-cell ensure CONFIRM-PRIMARY-CELL");
    return;
  }

  RV3032::PrimaryCellConfigurationReport report{};
  const RV3032::Status st = g_rtc.ensurePrimaryCellConfiguration(report);
  const char* outcome = "NOT_ATTEMPTED";
  switch (report.outcome) {
    case RV3032::PrimaryCellConfigurationOutcome::ALREADY_CONFIGURED:
      outcome = "ALREADY_CONFIGURED";
      break;
    case RV3032::PrimaryCellConfigurationOutcome::EEPROM_UPDATED:
      outcome = "EEPROM_UPDATED";
      break;
    case RV3032::PrimaryCellConfigurationOutcome::FAILED:
      outcome = "FAILED";
      break;
    default:
      break;
  }
  Serial.printf("Primary-cell ensure: %s status=%s before=%s0x%02X target=0x%02X after=%s0x%02X cleanup=%s\n",
                outcome, errToStr(st.code),
                report.persistentBeforeValid ? "" : "?",
                report.persistentBefore,
                report.persistentTarget,
                report.persistentAfterValid ? "" : "?",
                report.persistentAfter,
                report.cleanupVerified ? "verified" : "unverified");
}

/**
 * @brief Handle 'clear_bsf' command - clear backup switchover flag.
 */
static void cmd_clear_bsf() {
  RV3032::Status st = g_rtc.clearBackupSwitchFlag();
  if (!operation_accepted(st)) {
    LOGE("clearBackupSwitchFlag() failed: %s", st.msg);
  } else {
    LOGI("Backup switchover flag clear requested%s", cooperative_suffix(st));
  }
}

/**
 * @brief Handle 'clear_porf' command - clear power-on reset flag.
 */
static void cmd_clear_porf() {
  RV3032::Status st = g_rtc.clearPowerOnResetFlag();
  if (!operation_accepted(st)) {
    LOGE("clearPowerOnResetFlag() failed: %s", st.msg);
  } else {
    LOGI("Power-on reset flag clear requested%s", cooperative_suffix(st));
  }
}

/**
 * @brief Handle 'clear_vlf' command - clear voltage low flag.
 */
static void cmd_clear_vlf() {
  RV3032::Status st = g_rtc.clearVoltageLowFlag();
  print_verbose_status("clearVoltageLowFlag", st);
  if (!operation_accepted(st)) {
    LOGE("clearVoltageLowFlag() failed: %s", st.msg);
  } else {
    LOGI("Voltage low flag clear requested%s", cooperative_suffix(st));
  }
}

// ===== Driver Debugging Commands =====

/**
 * @brief Handle 'drv' command - show driver state and health.
 */
static void cmd_drv() {
  Serial.println();
  Serial.println(F("=== Driver Health ==="));
  RV3032::SettingsSnapshot snap;
  (void)g_rtc.getSettings(snap);
  const RV3032::DriverState state = snap.state;
  const bool online = g_rtc.isOnline();
  const bool initialized = snap.initialized;
  const uint32_t totalOk = g_rtc.totalSuccess();
  const uint32_t totalFail = g_rtc.totalFailures();
  const uint32_t total = totalOk + totalFail;
  const float successRate = (total > 0U)
                                ? (100.0f * static_cast<float>(totalOk) / static_cast<float>(total))
                                : 0.0f;
  Serial.printf("State: %s%s%s\n",
                stateColor(state, online, g_rtc.consecutiveFailures()),
                stateToStr(state),
                LOG_COLOR_RESET);
  Serial.printf("isOnline: %s%s%s\n",
                online ? LOG_COLOR_GREEN : LOG_COLOR_RED,
                log_bool_str(online),
                LOG_COLOR_RESET);
  Serial.printf("isInitialized: %s%s%s\n",
                initialized ? LOG_COLOR_GREEN : LOG_COLOR_RED,
                log_bool_str(initialized),
                LOG_COLOR_RESET);
  Serial.printf("Config: addr=0x%02X i2cTimeout=%lu offlineThreshold=%u nowMs=%s\n",
                static_cast<unsigned>(snap.i2cAddress),
                static_cast<unsigned long>(snap.i2cTimeoutMs),
                static_cast<unsigned>(snap.offlineThreshold),
                log_bool_str(snap.hasNowMsHook));
  Serial.printf("EEPROM: busy=%s writes=%s timeout=%lu queue=%u ok=%lu fail=%lu\n",
                log_bool_str(snap.eepromBusy),
                log_bool_str(snap.enableEepromWrites),
                static_cast<unsigned long>(snap.eepromTimeoutMs),
                static_cast<unsigned>(snap.eepromQueueDepth),
                static_cast<unsigned long>(snap.eepromWriteCount),
                static_cast<unsigned long>(snap.eepromWriteFailures));
  Serial.println();
  
  Serial.println(F("=== Counters ==="));
  Serial.printf("Consecutive Failures: %s%d%s\n",
                goodIfZeroColor(g_rtc.consecutiveFailures()),
                g_rtc.consecutiveFailures(),
                LOG_COLOR_RESET);
  Serial.printf("Total Successes: %s%lu%s\n",
                goodIfNonZeroColor(totalOk),
                static_cast<unsigned long>(totalOk),
                LOG_COLOR_RESET);
  Serial.printf("Total Failures: %s%lu%s\n",
                goodIfZeroColor(totalFail),
                static_cast<unsigned long>(totalFail),
                LOG_COLOR_RESET);
  Serial.printf("Success rate: %s%.1f%%%s\n",
                (successRate >= 99.9f) ? LOG_COLOR_GREEN :
                (successRate >= 80.0f) ? LOG_COLOR_YELLOW : LOG_COLOR_RED,
                successRate,
                LOG_COLOR_RESET);
  Serial.println();
  
  Serial.println(F("=== Timestamps ==="));
  uint32_t now = millis();
  uint32_t lastOk = g_rtc.lastOkMs();
  uint32_t lastErr = g_rtc.lastErrorMs();
  
  if (lastOk > 0) {
    Serial.printf("Last OK: %lu ms ago (at %lu ms)\n",
                  static_cast<unsigned long>(now - lastOk),
                  static_cast<unsigned long>(lastOk));
  } else {
    Serial.println(F("Last OK: never"));
  }
  
  if (lastErr > 0) {
    Serial.printf("Last Error: %lu ms ago (at %lu ms)\n",
                  static_cast<unsigned long>(now - lastErr),
                  static_cast<unsigned long>(lastErr));
  } else {
    Serial.println(F("Last Error: never"));
  }
  Serial.println();
  
  Serial.println(F("=== Last Error Details ==="));
  RV3032::Status lastError = g_rtc.lastError();
  Serial.printf("Code: %s%s%s (%d)\n",
                LOG_COLOR_RESULT(lastError.code == RV3032::Err::OK),
                errToStr(lastError.code),
                LOG_COLOR_RESET,
                static_cast<int>(lastError.code));
  Serial.printf("Detail: %ld\n", static_cast<long>(lastError.detail));
  Serial.printf("Message: %s\n", lastError.msg ? lastError.msg : "(none)");
  Serial.println();
  
  Serial.println(F("=== EEPROM State ==="));
  Serial.printf("Busy: %s%s%s\n",
                g_rtc.isEepromBusy() ? LOG_COLOR_YELLOW : LOG_COLOR_GREEN,
                g_rtc.isEepromBusy() ? "true" : "false",
                LOG_COLOR_RESET);
  RV3032::Status eepromSt = g_rtc.getEepromStatus();
  Serial.printf("Status: %s%s%s\n",
                LOG_COLOR_RESULT(eepromSt.ok()),
                eepromSt.ok() ? "OK" : eepromSt.msg,
                LOG_COLOR_RESET);
  Serial.println();
}

/**
 * @brief Handle 'probe' command - probe device without health tracking.
 */
static void cmd_probe() {
  Serial.println(F("Probing device (no health tracking)..."));
  
  // Capture health before
  uint8_t failsBefore = g_rtc.consecutiveFailures();
  uint32_t successBefore = g_rtc.totalSuccess();
  uint32_t failureBefore = g_rtc.totalFailures();
  
  RV3032::Status st = g_rtc.probe();
  
  // Capture health after
  uint8_t failsAfter = g_rtc.consecutiveFailures();
  uint32_t successAfter = g_rtc.totalSuccess();
  uint32_t failureAfter = g_rtc.totalFailures();
  
  if (st.ok()) {
    LOGI("Probe OK - device responding");
  } else {
    LOGE("Probe FAILED: %s (code=%s, detail=%ld)",
         st.msg, errToStr(st.code), static_cast<long>(st.detail));
  }
  
  // Verify no health change
  bool healthChanged = (failsBefore != failsAfter) ||
                       (successBefore != successAfter) ||
                       (failureBefore != failureAfter);
  
  Serial.printf("Health tracking: %s%s%s\n",
                healthChangeColor(healthChanged),
                healthChanged ? "CHANGED (unexpected!)" : "unchanged (correct)",
                LOG_COLOR_RESET);
}

/**
 * @brief Handle 'recover' command - manual recovery attempt.
 */
static void cmd_recover() {
  Serial.println(F("Attempting recovery..."));
  
  // Capture state before
  RV3032::DriverState stateBefore = g_rtc.state();
  uint8_t failsBefore = g_rtc.consecutiveFailures();
  
  RV3032::Status st = g_rtc.recover();
  print_verbose_status("recover", st);
  
  // Capture state after
  RV3032::DriverState stateAfter = g_rtc.state();
  uint8_t failsAfter = g_rtc.consecutiveFailures();
  
  if (st.ok()) {
    LOGI("Recovery successful");
  } else {
    LOGE("Recovery FAILED: %s (code=%s, detail=%ld)",
         st.msg, errToStr(st.code), static_cast<long>(st.detail));
  }
  
  Serial.printf("State: %s%s%s -> %s%s%s\n",
                stateColor(stateBefore, g_rtc.isOnline(), failsBefore), stateToStr(stateBefore), LOG_COLOR_RESET,
                stateColor(stateAfter, g_rtc.isOnline(), failsAfter), stateToStr(stateAfter), LOG_COLOR_RESET);
  Serial.printf("Consecutive failures: %s%d%s -> %s%d%s\n",
                goodIfZeroColor(failsBefore), failsBefore, LOG_COLOR_RESET,
                goodIfZeroColor(failsAfter), failsAfter, LOG_COLOR_RESET);
}

/**
 * @brief Handle 'verbose' command - enable/disable verbose mode.
 */
static void cmd_verbose(const String& args) {
  if (args.length() == 0) {
    Serial.printf("Verbose mode: %s%s%s\n",
                  onOffColor(g_verbose),
                  g_verbose ? "ON" : "OFF",
                  LOG_COLOR_RESET);
    return;
  }
  
  g_verbose = (args.toInt() != 0);
  LOGI("Verbose mode: %s%s%s",
       onOffColor(g_verbose),
       g_verbose ? "ON" : "OFF",
       LOG_COLOR_RESET);
}

/**
 * @brief Handle 'stress' command - rapid time reads stress test.
 * Tests I2C reliability and health tracking under load.
 */
static void cmd_stress(const String& args) {
  int iterations = 100;
  if (args.length() > 0) {
    iterations = args.toInt();
    if (iterations < 1) iterations = 1;
    if (iterations > 100000) iterations = 100000;
  }
  
  Serial.printf("\n=== Stress Test: %d iterations ===\n", iterations);
  
  // Capture initial state
  uint32_t successBefore = g_rtc.totalSuccess();
  uint32_t failureBefore = g_rtc.totalFailures();
  RV3032::DriverState stateBefore = g_rtc.state();
  
  uint32_t startMs = millis();
  int okCount = 0;
  int failCount = 0;
  uint32_t minTimeUs = UINT32_MAX;
  uint32_t maxTimeUs = 0;
  uint64_t totalTimeUs = 0;
  
  RV3032::DateTime dt;
  
  for (int i = 0; i < iterations; i++) {
    uint32_t opStart = micros();
    RV3032::Status st = g_rtc.readTime(dt);
    uint32_t opTime = micros() - opStart;
    
    if (st.ok()) {
      okCount++;
      totalTimeUs += opTime;
      if (opTime < minTimeUs) minTimeUs = opTime;
      if (opTime > maxTimeUs) maxTimeUs = opTime;
    } else {
      failCount++;
      Serial.printf("  [%d] FAIL: %s (code=%s)\n", i, st.msg, errToStr(st.code));
    }
    
    // Progress indicator every 10% (guard against small iteration counts)
    const int progressStep = (iterations >= 10) ? (iterations / 10) : iterations;
    if (progressStep > 0 && ((i + 1) % progressStep) == 0) {
      Serial.printf("  Progress: %d%%\n", ((i + 1) * 100) / iterations);
    }
    
    // Allow watchdog/system tasks
    yield();
  }
  
  uint32_t totalMs = millis() - startMs;
  
  // Results
  Serial.println(F("\n--- Results ---"));
  const bool stressAllOk = (failCount == 0);
  const char* successColor = stressAllOk ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
  const char* failColor = (failCount == 0) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
  Serial.printf("%sOK%s: %d, %sFAIL%s: %d (%.2f%% %ssuccess%s)\n",
                LOG_COLOR_GREEN, LOG_COLOR_RESET, okCount,
                failColor, LOG_COLOR_RESET, failCount,
                iterations > 0 ? (okCount * 100.0f / iterations) : 0.0f,
                successColor, LOG_COLOR_RESET);
  Serial.printf("Total time: %lu ms (%.1f ops/sec)\n",
                static_cast<unsigned long>(totalMs),
                totalMs > 0 ? (iterations * 1000.0f / totalMs) : 0.0f);
  
  if (okCount > 0) {
    Serial.printf("Per-op timing: min=%lu us, max=%lu us, avg=%lu us\n",
                  static_cast<unsigned long>(minTimeUs),
                  static_cast<unsigned long>(maxTimeUs),
                  static_cast<unsigned long>(static_cast<uint32_t>(totalTimeUs / okCount)));
  }
  
  // Health tracking verification
  Serial.println(F("\n--- Health Tracking ---"));
  uint32_t successAfter = g_rtc.totalSuccess();
  uint32_t failureAfter = g_rtc.totalFailures();
  RV3032::DriverState stateAfter = g_rtc.state();
  
  uint32_t expectedSuccess = successBefore + okCount;
  uint32_t expectedFailure = failureBefore + failCount;
  
  const bool successMatch = (successAfter == expectedSuccess);
  const bool failureMatch = (failureAfter == expectedFailure);
  Serial.printf("Total success: %lu -> %lu (expected %lu) %s%s%s\n",
                static_cast<unsigned long>(successBefore),
                static_cast<unsigned long>(successAfter),
                static_cast<unsigned long>(expectedSuccess),
                LOG_COLOR_RESULT(successMatch),
                successMatch ? "OK" : "MISMATCH!",
                LOG_COLOR_RESET);
  Serial.printf("Total failures: %lu -> %lu (expected %lu) %s%s%s\n",
                static_cast<unsigned long>(failureBefore),
                static_cast<unsigned long>(failureAfter),
                static_cast<unsigned long>(expectedFailure),
                LOG_COLOR_RESULT(failureMatch),
                failureMatch ? "OK" : "MISMATCH!",
                LOG_COLOR_RESET);
  Serial.printf("Driver state: %s%s%s -> %s%s%s\n",
                stateColor(stateBefore, g_rtc.isOnline(), g_rtc.consecutiveFailures()),
                stateToStr(stateBefore),
                LOG_COLOR_RESET,
                stateColor(stateAfter, g_rtc.isOnline(), g_rtc.consecutiveFailures()),
                stateToStr(stateAfter),
                LOG_COLOR_RESET);
  Serial.printf("Consecutive failures: %d\n", g_rtc.consecutiveFailures());
  Serial.println();
}

/**
 * @brief Handle 'stress_mix' command - mixed operations stress test.
 * Tests various API calls to exercise different code paths.
 */
static void cmd_stress_mix(const String& args) {
  int iterations = 50;
  if (args.length() > 0) {
    iterations = args.toInt();
    if (iterations < 1) iterations = 1;
    if (iterations > 100000) iterations = 100000;
  }
  
  Serial.printf("\n=== Mixed Operations Stress Test: %d iterations ===\n", iterations);
  
  // Capture initial state
  uint32_t successBefore = g_rtc.totalSuccess();
  uint32_t failureBefore = g_rtc.totalFailures();
  
  uint32_t startMs = millis();
  int okCount = 0;
  int failCount = 0;
  
  // Track per-operation stats
  struct OpStats {
    const char* name;
    int ok;
    int fail;
  };
  OpStats stats[] = {
    {"readTime", 0, 0},
    {"readUnix", 0, 0},
    {"readTemp", 0, 0},
    {"readStatus", 0, 0},
    {"getOffset", 0, 0},
    {"getClkout", 0, 0},
    {"readValidity", 0, 0},
  };
  constexpr int numOps = sizeof(stats) / sizeof(stats[0]);
  
  for (int i = 0; i < iterations; i++) {
    RV3032::Status st;
    int opIdx = i % numOps;
    
    switch (opIdx) {
      case 0: {
        RV3032::DateTime dt;
        st = g_rtc.readTime(dt);
        break;
      }
      case 1: {
        uint32_t ts;
        st = g_rtc.readUnix(ts);
        break;
      }
      case 2: {
        float temp;
        st = g_rtc.readTemperatureC(temp);
        break;
      }
      case 3: {
        uint8_t status;
        st = g_rtc.readStatus(status);
        break;
      }
      case 4: {
        float ppm;
        st = g_rtc.getOffsetPpm(ppm);
        break;
      }
      case 5: {
        bool enabled;
        st = g_rtc.getClkoutEnabled(enabled);
        break;
      }
      case 6: {
        RV3032::ValidityFlags flags;
        st = g_rtc.readValidity(flags);
        break;
      }
    }
    
    if (st.ok()) {
      okCount++;
      stats[opIdx].ok++;
    } else {
      failCount++;
      stats[opIdx].fail++;
      Serial.printf("  [%d] %s FAIL: %s\n", i, stats[opIdx].name, st.msg);
    }
    
    // Progress indicator every 25% (guard against small iteration counts)
    const int progressStep = (iterations >= 4) ? (iterations / 4) : iterations;
    if (progressStep > 0 && ((i + 1) % progressStep) == 0) {
      Serial.printf("  Progress: %d%%\n", ((i + 1) * 100) / iterations);
    }
    
    yield();
  }
  
  uint32_t totalMs = millis() - startMs;
  
  // Results
  Serial.println(F("\n--- Results ---"));
  const bool mixAllOk = (failCount == 0);
  const char* mixSuccessColor = mixAllOk ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
  const char* mixFailColor = (failCount == 0) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
  Serial.printf("Total: %sOK%s=%d, %sFAIL%s=%d (%.2f%% %ssuccess%s)\n",
                LOG_COLOR_GREEN, LOG_COLOR_RESET, okCount,
                mixFailColor, LOG_COLOR_RESET, failCount,
                (iterations > 0) ? (okCount * 100.0f / iterations) : 0.0f,
                mixSuccessColor, LOG_COLOR_RESET);
  Serial.printf("Time: %lu ms (%.1f ops/sec)\n\n",
                static_cast<unsigned long>(totalMs),
                totalMs > 0 ? (iterations * 1000.0f / totalMs) : 0.0f);
  
  Serial.println(F("Per-operation breakdown:"));
  for (int i = 0; i < numOps; i++) {
    const char* perFailColor = (stats[i].fail == 0) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
    Serial.printf("  %-12s: %sOK%s=%d, %sFAIL%s=%d\n",
                  stats[i].name,
                  LOG_COLOR_GREEN, LOG_COLOR_RESET, stats[i].ok,
                  perFailColor, LOG_COLOR_RESET, stats[i].fail);
  }
  
  // Health tracking verification
  Serial.println(F("\n--- Health Tracking ---"));
  uint32_t successAfter = g_rtc.totalSuccess();
  uint32_t failureAfter = g_rtc.totalFailures();
  
  // Note: some operations do multiple I2C calls, so we can't predict exact count
  Serial.printf("Success delta: %s+%lu%s (ops had %d %sOK%s results)\n",
                LOG_COLOR_GREEN,
                static_cast<unsigned long>(successAfter - successBefore),
                LOG_COLOR_RESET,
                okCount,
                LOG_COLOR_GREEN,
                LOG_COLOR_RESET);
  const char* failureDeltaColor = (failureAfter > failureBefore) ? LOG_COLOR_RED : LOG_COLOR_GREEN;
  const char* failResultsColor = (failCount == 0) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
  Serial.printf("Failure delta: %s+%lu%s (ops had %d %sFAIL%s results)\n",
                failureDeltaColor,
                static_cast<unsigned long>(failureAfter - failureBefore),
                LOG_COLOR_RESET,
                failCount,
                failResultsColor,
                LOG_COLOR_RESET);
  const RV3032::DriverState mixState = g_rtc.state();
  Serial.printf("Driver state: %s%s%s\n",
                stateColor(mixState, g_rtc.isOnline(), g_rtc.consecutiveFailures()),
                stateToStr(mixState),
                LOG_COLOR_RESET);
  Serial.printf("Consecutive failures: %d\n", g_rtc.consecutiveFailures());
  Serial.println();
}

/**
 * @brief Handle 'selftest' command - safe command coverage with report.
 */
static void cmd_selftest() {
  struct SelfTestStats {
    uint32_t pass = 0;
    uint32_t fail = 0;
    uint32_t skip = 0;
  } stats;

  enum class SelftestOutcome : uint8_t { PASS, FAIL, SKIP };
  auto report = [&](const char* name, SelftestOutcome outcome, const char* note) {
    const bool ok = (outcome == SelftestOutcome::PASS);
    const bool skip = (outcome == SelftestOutcome::SKIP);
    const char* color = skip ? LOG_COLOR_YELLOW : LOG_COLOR_RESULT(ok);
    const char* tag = skip ? "SKIP" : (ok ? "PASS" : "FAIL");
    Serial.printf("  [%s%s%s] %s", color, tag, LOG_COLOR_RESET, name);
    if (note && note[0]) {
      Serial.printf(" - %s", note);
    }
    Serial.println();
    if (skip) {
      stats.skip++;
    } else if (ok) {
      stats.pass++;
    } else {
      stats.fail++;
    }
  };
  auto reportCheck = [&](const char* name, bool ok, const char* note) {
    report(name, ok ? SelftestOutcome::PASS : SelftestOutcome::FAIL, note);
  };
  auto reportSkip = [&](const char* name, const char* note) {
    report(name, SelftestOutcome::SKIP, note);
  };

  Serial.println();
  Serial.println(F("=== RV3032 Selftest (safe commands) ==="));

  const uint32_t succBefore = g_rtc.totalSuccess();
  const uint32_t failBefore = g_rtc.totalFailures();
  const uint8_t consBefore = g_rtc.consecutiveFailures();

  RV3032::Status st = g_rtc.probe();
  if (st.code == RV3032::Err::NOT_INITIALIZED) {
    reportSkip("probe responds", "driver not initialized");
    reportSkip("remaining checks", "selftest aborted");
    Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                  goodIfNonZeroColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                  goodIfZeroColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                  skipCountColor(stats.skip), static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
    Serial.println();
    return;
  }
  reportCheck("probe responds", st.ok(), st.ok() ? "" : st.msg);
  const bool probeNoTrack = (g_rtc.totalSuccess() == succBefore) &&
                            (g_rtc.totalFailures() == failBefore) &&
                            (g_rtc.consecutiveFailures() == consBefore);
  reportCheck("probe no-health-side-effects", probeNoTrack, "");

  RV3032::DateTime dt;
  st = g_rtc.readTime(dt);
  reportCheck("readTime", st.ok(), st.ok() ? "" : st.msg);
  if (st.ok()) {
    const bool dtRange = (dt.year >= 2000 && dt.year <= 2099) &&
                         (dt.month >= 1 && dt.month <= 12) &&
                         (dt.day >= 1 && dt.day <= 31) &&
                         (dt.hour <= 23) && (dt.minute <= 59) && (dt.second <= 59);
    reportCheck("time fields plausible", dtRange, "");
  } else {
    reportCheck("time fields plausible", false, "readTime failed");
  }

  uint32_t unixTs = 0;
  st = g_rtc.readUnix(unixTs);
  reportCheck("readUnix", st.ok(), st.ok() ? "" : st.msg);

  float tempC = 0.0f;
  st = g_rtc.readTemperatureC(tempC);
  reportCheck("readTemperatureC", st.ok(), st.ok() ? "" : st.msg);
  reportCheck("temperature plausible", st.ok() && tempC > -60.0f && tempC < 120.0f, "");

  uint8_t status = 0;
  st = g_rtc.readStatus(status);
  reportCheck("readStatus", st.ok(), st.ok() ? "" : st.msg);

  RV3032::ValidityFlags vf;
  st = g_rtc.readValidity(vf);
  reportCheck("readValidity", st.ok(), st.ok() ? "" : st.msg);

  RV3032::AlarmConfig alarmCfg;
  st = g_rtc.getAlarmConfig(alarmCfg);
  reportCheck("getAlarmConfig", st.ok(), st.ok() ? "" : st.msg);

  bool alarmInt = false;
  st = g_rtc.getAlarmInterruptEnabled(alarmInt);
  reportCheck("getAlarmInterruptEnabled", st.ok(), st.ok() ? "" : st.msg);

  bool alarmFlag = false;
  st = g_rtc.getAlarmFlag(alarmFlag);
  reportCheck("getAlarmFlag", st.ok(), st.ok() ? "" : st.msg);

  bool clkoutEnabled = false;
  st = g_rtc.getClkoutEnabled(clkoutEnabled);
  reportCheck("getClkoutEnabled", st.ok(), st.ok() ? "" : st.msg);

  RV3032::ClkoutFrequency clkFreq = RV3032::ClkoutFrequency::Hz32768;
  st = g_rtc.getClkoutFrequency(clkFreq);
  reportCheck("getClkoutFrequency", st.ok(), st.ok() ? "" : st.msg);

  float offset = 0.0f;
  st = g_rtc.getOffsetPpm(offset);
  reportCheck("getOffsetPpm", st.ok(), st.ok() ? "" : st.msg);

  if (g_rtc.state() == RV3032::DriverState::OFFLINE) {
    reportSkip("recover", "selftest is read-only; use recover command");
  }
  reportCheck("isOnline", g_rtc.isOnline(), "");

  Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                goodIfNonZeroColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                goodIfZeroColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                skipCountColor(stats.skip), static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
  Serial.println();
}

/**
 * @brief Process command string.
 */
static void process_command(const String& line) {
  if (line.length() == 0) {
    return;
  }

  const int spaceIdx = line.indexOf(' ');
  const String cmd = (spaceIdx >= 0) ? line.substring(0, spaceIdx) : line;
  const String args = (spaceIdx >= 0) ? line.substring(spaceIdx + 1) : "";

  if (cmd == "help" || cmd == "?") {
    print_help();
  } else if (cmd == "version" || cmd == "ver") {
    cmd_version();
  } else if (cmd == "scan") {
    bus_diag::scan();
  } else if (cmd == "read") {
    cmd_time();
  } else if (cmd == "cfg" || cmd == "settings") {
    cmd_drv();
  } else if (cmd == "time") {
    cmd_time();
  } else if (cmd == "set") {
    cmd_set(args);
  } else if (cmd == "setbuild") {
    cmd_setbuild();
  } else if (cmd == "unix") {
    cmd_unix(args);
  } else if (cmd == "temp") {
    cmd_temp();
  } else if (cmd == "ts") {
    cmd_ts(args);
  } else if (cmd == "ts_reset") {
    cmd_ts_reset(args);
  } else if (cmd == "alarm") {
    cmd_alarm();
  } else if (cmd == "alarm_set") {
    cmd_alarm_set(args);
  } else if (cmd == "alarm_match") {
    cmd_alarm_match(args);
  } else if (cmd == "alarm_int") {
    cmd_alarm_int(args);
  } else if (cmd == "alarm_clear") {
    cmd_alarm_clear();
  } else if (cmd == "clkout") {
    cmd_clkout(args);
  } else if (cmd == "clkout_freq") {
    cmd_clkout_freq(args);
  } else if (cmd == "offset") {
    cmd_offset(args);
  } else if (cmd == "timer") {
    cmd_timer(args);
  } else if (cmd == "evi") {
    cmd_evi(args);
  } else if (cmd == "status") {
    cmd_status();
  } else if (cmd == "statusf") {
    cmd_statusf();
  } else if (cmd == "status_clear") {
    cmd_status_clear(args);
  } else if (cmd == "validity") {
    cmd_validity();
  } else if (cmd == "ram") {
    cmd_ram(args);
  } else if (cmd == "ram_write") {
    cmd_ram_write(args);
  } else if (cmd == "reg") {
    cmd_reg(args);
  } else if (cmd == "eeprom") {
    cmd_eeprom();
  } else if (cmd == "backup") {
    cmd_backup(args);
  } else if (cmd == "primary-cell") {
    cmd_primary_cell(args);
  } else if (cmd == "clear_porf") {
    cmd_clear_porf();
  } else if (cmd == "clear_vlf") {
    cmd_clear_vlf();
  } else if (cmd == "clear_bsf") {
    cmd_clear_bsf();
  } else if (cmd == "drv") {
    cmd_drv();
  } else if (cmd == "probe") {
    cmd_probe();
  } else if (cmd == "recover") {
    cmd_recover();
  } else if (cmd == "verbose") {
    cmd_verbose(args);
  } else if (cmd == "stress") {
    cmd_stress(args);
  } else if (cmd == "stress_mix") {
    cmd_stress_mix(args);
  } else if (cmd == "selftest") {
    cmd_selftest();
  } else {
    LOGW("Unknown command: '%s'. Type 'help' for available commands.", cmd.c_str());
  }
}

void setup() {
  delay(1000);  // USB-CDC enumeration delay - helps Windows recognize device after reset
  Serial.begin(115200);
  while (!Serial && millis() < 4000) {
    delay(10);  // Wait for serial (max 3 seconds after the initial delay)
  }

  print_help();

  // Initialize I2C
  LOGI("Initializing I2C (SDA=%d, SCL=%d)...", board::I2C_SDA, board::I2C_SCL);
  if (!board::initI2c()) {
    LOGE("I2C init failed");
    return;
  }

  // Initialize RTC
  LOGI("Initializing RTC...");
  RV3032::Config cfg;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = &Wire;
  cfg.nowMs = rtc_now_ms;
  cfg.waitMs = rtc_wait_ms;
  cfg.enableEepromWrites = false;

  RV3032::Status st = g_rtc.begin(cfg);
  if (!st.ok()) {
    LOGE("RTC init failed: %s (code=%s, detail=%ld)",
         st.msg, errToStr(st.code), static_cast<long>(st.detail));
    LOGE("Check I2C wiring and RTC power");
    return;
  }

  LOGI("RTC callbacks bound; probing device explicitly...");
  st = g_rtc.probe();
  if (!st.ok()) {
    LOGE("RTC probe failed: %s (code=%s, detail=%ld)",
         st.msg, errToStr(st.code), static_cast<long>(st.detail));
    return;
  }
  LOGI("RTC probe successful; no backup chemistry or EEPROM policy was applied");

  LOGI("Driver state: %s", stateToStr(g_rtc.state()));
  LOGI("Type 'help' for available commands");
  LOGI("Type 'drv' for driver health, 'verbose 1' for detailed output");
  cli::printPrompt();
}

void loop() {
  g_rtc.tick(millis());

  if (g_rtc.isJobBusy()) {
    uint8_t used = 0;
    const RV3032::Status jobStatus = g_rtc.pollJob(millis(), 1, used);
    if (!jobStatus.ok() && !jobStatus.inProgress() &&
        jobStatus.code != RV3032::Err::BUSY) {
      LOGE("RTC cooperative job failed: %s (code=%s, detail=%ld)",
           jobStatus.msg, errToStr(jobStatus.code),
           static_cast<long>(jobStatus.detail));
    }
  }

  const String line = read_line();
  if (line.length() > 0) {
    process_command(line);
    cli::printPrompt();
  }

  delay(10);
}
