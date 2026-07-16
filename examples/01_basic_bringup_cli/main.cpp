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
#include <cstring>

#include "examples/common/BoardConfig.h"
#include "examples/common/CliShell.h"
#include "examples/common/CommandHandler.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/CliStyle.h"
#include "examples/common/Log.h"
#include "RV3032/CommandTable.h"
#include "RV3032/Version.h"
#include "RV3032/RV3032.h"

static RV3032::RV3032 g_rtc;

enum class PendingSurface : uint8_t {
  NONE,
  ORDINARY_JOB,
  EEPROM
};

struct PendingOperation {
  PendingSurface surface = PendingSurface::NONE;
  const char* name = nullptr;
  RV3032::Status ordinaryStatus = RV3032::Status::Ok();
};

static PendingOperation g_pendingOperation{};

static bool operationAccepted(const char* name, const RV3032::Status& status) {
  if (status.inProgress()) {
    g_pendingOperation.surface = PendingSurface::ORDINARY_JOB;
    g_pendingOperation.name = name;
    g_pendingOperation.ordinaryStatus = RV3032::Status::Ok();
    return true;
  }
  return status.ok();
}

static const char* primary_failure_stage_name(
    RV3032::PrimaryCellFailureStage stage) {
  switch (stage) {
    case RV3032::PrimaryCellFailureStage::NONE: return "NONE";
    case RV3032::PrimaryCellFailureStage::PRECONDITION: return "PRECONDITION";
    case RV3032::PrimaryCellFailureStage::PREPARE_ACCESS: return "PREPARE_ACCESS";
    case RV3032::PrimaryCellFailureStage::READ_PERSISTENT: return "READ_PERSISTENT";
    case RV3032::PrimaryCellFailureStage::WRITE_PERSISTENT: return "WRITE_PERSISTENT";
    case RV3032::PrimaryCellFailureStage::VERIFY_PERSISTENT: return "VERIFY_PERSISTENT";
    case RV3032::PrimaryCellFailureStage::CLEANUP: return "CLEANUP";
    case RV3032::PrimaryCellFailureStage::SETTLE: return "SETTLE";
    default: return "UNKNOWN";
  }
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

static const char* stateColor(RV3032::DriverState state) {
  switch (state) {
    case RV3032::DriverState::READY: return LOG_COLOR_GREEN;
    case RV3032::DriverState::DEGRADED: return LOG_COLOR_YELLOW;
    case RV3032::DriverState::OFFLINE: return LOG_COLOR_RED;
    case RV3032::DriverState::UNINIT: return LOG_COLOR_GRAY;
    default: return LOG_COLOR_RESET;
  }
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
    case RV3032::Err::CONFIGURATION_CLEANUP_FAILED:
      return "CONFIGURATION_CLEANUP_FAILED";
    case RV3032::Err::TRANSPORT_CONTRACT_VIOLATION:
      return "TRANSPORT_CONTRACT_VIOLATION";
    case RV3032::Err::INTERNAL_STATE_ERROR: return "INTERNAL_STATE_ERROR";
    default: return "UNKNOWN";
  }
}

static const char* configurationFinalStateToStr(
    RV3032::ConfigurationFinalState state) {
  switch (state) {
    case RV3032::ConfigurationFinalState::UNCHANGED: return "UNCHANGED";
    case RV3032::ConfigurationFinalState::REQUESTED_VERIFIED:
      return "REQUESTED_VERIFIED";
    case RV3032::ConfigurationFinalState::SAFE_DISABLED_VERIFIED:
      return "SAFE_DISABLED_VERIFIED";
    case RV3032::ConfigurationFinalState::UNKNOWN: return "UNKNOWN";
    default: return "INVALID";
  }
}

static void printConfigurationReport(const char* name) {
  RV3032::ConfigurationJobReport report{};
  RV3032::Status result = RV3032::Status::Error(
      RV3032::Err::JOB_RESULT_UNAVAILABLE, "No configuration result");
  if (strcmp(name, "timer configuration") == 0) {
    result = g_rtc.getSetTimerJobResult(report);
  } else if (strcmp(name, "backup configuration") == 0) {
    result = g_rtc.getSetBackupSwitchModeJobResult(report);
  } else if (strcmp(name, "CLKOUT configuration") == 0) {
    result = g_rtc.getSetClkoutConfigJobResult(report);
  } else {
    return;
  }

  if (result.code == RV3032::Err::JOB_RESULT_UNAVAILABLE ||
      result.code == RV3032::Err::IN_PROGRESS) {
    LOGE("%s result evidence unavailable: %s", name, errToStr(result.code));
    return;
  }
  Serial.printf(
      "%s evidence: final=%s mutation_attempted=%s operation=%s detail=%ld cleanup=%s detail=%ld\n",
      name, configurationFinalStateToStr(report.finalState),
      cli::boolText(report.mutationAttempted),
      errToStr(report.operationStatus.code),
      static_cast<long>(report.operationStatus.detail),
      errToStr(report.cleanupStatus.code),
      static_cast<long>(report.cleanupStatus.detail));
}

static void reportPendingCompletion(const RV3032::Status* persistenceStatus) {
  const char* name = g_pendingOperation.name == nullptr
                         ? "cooperative operation"
                         : g_pendingOperation.name;
  const RV3032::Status operationStatus = g_pendingOperation.ordinaryStatus;
  if (operationStatus.ok()) {
    LOGI("%s terminal status: OK", name);
  } else {
    LOGE("%s terminal status: %s (detail=%ld, message=%s)",
         name, errToStr(operationStatus.code),
         static_cast<long>(operationStatus.detail),
         operationStatus.msg == nullptr ? "" : operationStatus.msg);
  }
  printConfigurationReport(name);

  if (persistenceStatus != nullptr) {
    RV3032::SettingsSnapshot snapshot{};
    const RV3032::Status settingsStatus = g_rtc.getSettings(snapshot);
    if (settingsStatus.ok()) {
      Serial.printf(
          "%s persistence: terminal=%s detail=%ld operation=%s detail=%ld cleanup=%s detail=%ld\n",
          name, errToStr(persistenceStatus->code),
          static_cast<long>(persistenceStatus->detail),
          errToStr(snapshot.eepromOperationStatus.code),
          static_cast<long>(snapshot.eepromOperationStatus.detail),
          errToStr(snapshot.eepromCleanupStatus.code),
          static_cast<long>(snapshot.eepromCleanupStatus.detail));
    } else {
      LOGE("%s persistence terminal status: %s", name,
           errToStr(persistenceStatus->code));
    }
  }
}

static void pollPendingOperation(uint32_t nowMs) {
  if (g_pendingOperation.surface == PendingSurface::ORDINARY_JOB) {
    uint8_t instructionsUsed = 0;
    const RV3032::Status status =
        g_rtc.pollJob(nowMs, 1, instructionsUsed);
    if (status.inProgress()) {
      return;
    }
    g_pendingOperation.ordinaryStatus = status;
    if (g_rtc.isEepromBusy()) {
      g_pendingOperation.surface = PendingSurface::EEPROM;
      return;
    }
    reportPendingCompletion(nullptr);
    g_pendingOperation = PendingOperation{};
    cli::printPrompt();
    return;
  }

  if (g_pendingOperation.surface == PendingSurface::EEPROM) {
    uint8_t instructionsUsed = 0;
    const RV3032::Status status =
        g_rtc.pollEeprom(nowMs, 1, instructionsUsed);
    if (status.inProgress() || g_rtc.isEepromBusy()) {
      return;
    }
    reportPendingCompletion(&status);
    g_pendingOperation = PendingOperation{};
    cli::printPrompt();
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
                cli::resultColor(st.ok()),
                st.ok() ? "OK" : "FAILED",
                LOG_COLOR_RESET,
                errToStr(st.code),
                static_cast<long>(st.detail));
  if (st.msg) {
    Serial.printf("  Message: %s\n", st.msg);
  }
  
  // Driver health snapshot
  const RV3032::DriverState drvState = g_rtc.state();
  Serial.printf("  Driver State: %s%s%s\n",
                stateColor(drvState),
                stateToStr(drvState),
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
  static constexpr uint32_t TIMEOUT_MS = 1000U;
  const uint32_t startedMs = millis();
  const uint32_t deadlineMs = startedMs + TIMEOUT_MS;
  RV3032::Status st = g_rtc.startReadUserEepromJob(
      offset, length, startedMs, TIMEOUT_MS);
  while (st.inProgress() &&
         static_cast<int32_t>(deadlineMs - millis()) > 0) {
    uint8_t used = 0;
    st = g_rtc.pollJob(millis(), 1, used);
    if (st.inProgress()) delay(1);
  }
  if (st.inProgress()) {
    uint8_t used = 0;
    st = g_rtc.pollJob(deadlineMs, 1, used);
  }
  if (!st.ok()) return st;
  RV3032::PersistentReadResult result{};
  st = g_rtc.getPersistentReadJobResult(result);
  if (!st.ok()) return st;
  if (!result.persistentVerified || result.length != length) {
    return RV3032::Status::Error(RV3032::Err::INCOHERENT_DATA,
                                 "Persistent read proof is incomplete");
  }
  memcpy(out, result.data, length);
  return RV3032::Status::Ok();
}

static bool popToken(String& text, String& token) {
  text.trim();
  if (text.length() == 0) {
    return false;
  }
  size_t split = 0;
  while (split < text.length() &&
         !isspace(static_cast<unsigned char>(text[split]))) {
    ++split;
  }
  if (split == text.length()) {
    token = text;
    text = "";
    return true;
  }
  token = text.substring(0, split);
  text = text.substring(split + 1);
  return true;
}

static bool parseExactTokens(String text, String* tokens, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (!popToken(text, tokens[i])) {
      return false;
    }
  }
  String extra;
  return !popToken(text, extra);
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
  cli::printHelpItem("timer <ticks 1..4095> <freq 0..3> <en 0|1>",
                     "Set timer");

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
  cli::printHelpItem("status_clear <mask 0..255>", "Clear status flags by explicit decimal mask");
  cli::printHelpItem("validity", "Read PORF/VLF/BSF validity flags");
  cli::printHelpItem("ram [offset len]", "Dump user RAM (default all 16 bytes)");
  cli::printHelpItem("ram_write <offset> <byte...>", "Write decimal user RAM bytes");
  cli::printHelpItem("reg <addr>", "Read register byte");
  cli::printHelpItem("reg <addr> <val> [confirm]", "Write register byte; confirm required outside user RAM");
  cli::printHelpItem("eeprom", "EEPROM stats and user EEPROM dump");
  cli::printHelpItem("backup [status|off|direct|level]", "Show or cooperatively configure backup PMU");
  cli::printHelpItem("primary-cell ensure CONFIRM-PRIMARY-CELL",
                     "Explicit one-shot primary-cell provisioning");
  cli::printHelpItem("clear_porf", "Clear power-on reset flag");
  cli::printHelpItem("clear_vlf", "Clear voltage low flag");
  cli::printHelpItem("clear_bsf", "Clear backup switchover flag");

  cli::printHelpSection("Diagnostics");
  cli::printHelpItem("drv", "Show driver state and health");
  cli::printHelpItem("probe", "Read 0x51 Status register without health tracking");
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

  String tokens[6];
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  if (!parseExactTokens(args, tokens, 6) ||
      !cmd::parseU16Token(tokens[0], year) ||
      !cmd::parseU8Token(tokens[1], month) ||
      !cmd::parseU8Token(tokens[2], day) ||
      !cmd::parseU8Token(tokens[3], hour) ||
      !cmd::parseU8Token(tokens[4], minute) ||
      !cmd::parseU8Token(tokens[5], second)) {
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
  RV3032::Status st = RV3032::RV3032::computeWeekday(
      year, month, day, dt.weekday);
  if (!st.ok()) {
    LOGE("Invalid date: %s", st.msg);
    return;
  }

  st = g_rtc.setTime(dt);
  if (!st.ok()) {
    LOGE("setTime() failed: %s", st.msg);
  } else {
    LOGI("Calendar set completed");
    print_datetime(dt);
  }
}

/**
 * @brief Handle 'setbuild' command - set time to build timestamp.
 */
static void cmd_setbuild() {
  RV3032::DateTime dt;
  if (!RV3032::RV3032::parseBuildTime(dt).ok()) {
    LOGE("parseBuildTime() failed");
    return;
  }

  RV3032::Status st = g_rtc.setTime(dt);
  if (!st.ok()) {
    LOGE("setTime() failed: %s", st.msg);
  } else {
    LOGI("Build timestamp calendar set completed");
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

  String token;
  uint32_t ts = 0;
  if (!parseExactTokens(args, &token, 1) ||
      !cmd::parseU32Token(token, ts)) {
    LOGE("Usage: unix [timestamp]");
    return;
  }
  RV3032::Status st = g_rtc.setUnix(ts);
  if (!st.ok()) {
    LOGE("setUnix() failed: %s", st.msg);
    return;
  }
  LOGI("Unix timestamp %lu set completed", static_cast<unsigned long>(ts));
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
  if (!ts.timeValid) {
    Serial.println(F("  Time: empty/unset"));
    return;
  }
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
  if (!operationAccepted("timestamp reset", st)) {
    LOGE("resetTimestamp(%s) failed: %s", timestamp_source_name(source), st.msg);
    return;
  }
  LOGI("%s timestamp reset %s", timestamp_source_name(source),
       st.inProgress() ? "accepted" : "completed");
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

  String tokens[3];
  uint8_t minute = 0;
  uint8_t hour = 0;
  uint8_t date = 0;
  if (!parseExactTokens(args, tokens, 3) ||
      !cmd::parseU8Token(tokens[0], minute) ||
      !cmd::parseU8Token(tokens[1], hour) ||
      !cmd::parseU8Token(tokens[2], date)) {
    LOGE("Invalid format. Usage: alarm_set MM HH DD");
    return;
  }

  RV3032::Status st = g_rtc.setAlarmTime(minute, hour, date);
  if (!operationAccepted("alarm time", st)) {
    LOGE("setAlarmTime() failed: %s", st.msg);
  } else {
    LOGI("Alarm time %s: %02u:%02u (date=%02u)",
         st.inProgress() ? "accepted" : "completed",
         static_cast<unsigned>(hour), static_cast<unsigned>(minute),
         static_cast<unsigned>(date));
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

  String tokens[3];
  bool matchMin = false;
  bool matchHour = false;
  bool matchDate = false;
  if (!parseExactTokens(args, tokens, 3) ||
      !cmd::parseBool01Token(tokens[0], matchMin) ||
      !cmd::parseBool01Token(tokens[1], matchHour) ||
      !cmd::parseBool01Token(tokens[2], matchDate)) {
    LOGE("Invalid format. Usage: alarm_match M H D (1=on, 0=off)");
    return;
  }

  RV3032::Status st = g_rtc.setAlarmMatch(matchMin, matchHour, matchDate);
  if (!operationAccepted("alarm match", st)) {
    LOGE("setAlarmMatch() failed: %s", st.msg);
  } else {
    LOGI("Alarm match %s: minute=%d hour=%d date=%d",
         st.inProgress() ? "accepted" : "completed",
         matchMin ? 1 : 0, matchHour ? 1 : 0, matchDate ? 1 : 0);
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

  String token;
  bool enable = false;
  if (!parseExactTokens(args, &token, 1) ||
      !cmd::parseBool01Token(token, enable)) {
    LOGE("Usage: alarm_int [0|1]");
    return;
  }
  RV3032::Status st = g_rtc.enableAlarmInterrupt(enable);
  if (!operationAccepted("alarm interrupt", st)) {
    LOGE("enableAlarmInterrupt() failed: %s", st.msg);
  } else {
    LOGI("Alarm interrupt %s %s", enable ? "enable" : "disable",
         st.inProgress() ? "accepted" : "completed");
  }
}

/**
 * @brief Handle 'alarm_clear' command - clear alarm flag.
 */
static void cmd_alarm_clear() {
  RV3032::Status st = g_rtc.clearAlarmFlag();
  if (!operationAccepted("alarm flag clear", st)) {
    LOGE("clearAlarmFlag() failed: %s", st.msg);
  } else {
    LOGI("Alarm flag clear %s", st.inProgress() ? "accepted" : "completed");
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

  String token;
  bool enable = false;
  if (!parseExactTokens(args, &token, 1) ||
      !cmd::parseBool01Token(token, enable)) {
    LOGE("Usage: clkout [0|1]");
    return;
  }
  RV3032::Status st = g_rtc.setClkoutEnabled(enable);
  const char* operationName = enable ? "CLKOUT enable" : "CLKOUT disable";
  if (!operationAccepted(operationName, st)) {
    LOGE("setClkoutEnabled() failed: %s", st.msg);
    return;
  }
  LOGI("Clock output %s %s", enable ? "enable" : "disable",
       st.inProgress() ? "accepted" : "completed");
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

  String token;
  uint8_t freq = 0;
  if (!parseExactTokens(args, &token, 1) ||
      !cmd::parseU8Token(token, freq) || freq > 3U) {
    LOGE("Invalid frequency. Range: 0..3");
    return;
  }

  RV3032::ClkoutFrequency freqEnum = static_cast<RV3032::ClkoutFrequency>(freq);
  RV3032::Status st = g_rtc.setClkoutFrequency(freqEnum);
  if (!operationAccepted("CLKOUT configuration", st)) {
    LOGE("setClkoutFrequency() failed: %s", st.msg);
    return;
  }
  LOGI("Clock output frequency %s %s", freqStr[freq],
       st.inProgress() ? "accepted" : "completed");
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
    String token;
    float ppm = 0.0f;
    if (!parseExactTokens(args, &token, 1) ||
        !cmd::parseFloatToken(token, ppm)) {
      LOGE("Usage: offset [finite-ppm]");
      return;
    }
    RV3032::Status st = g_rtc.setOffsetPpm(ppm);
    if (!operationAccepted("frequency offset", st)) {
      LOGE("setOffsetPpm() failed: %s", st.msg);
      return;
    }
    LOGI("Frequency offset %.2f ppm %s", ppm,
         st.inProgress() ? "accepted" : "completed");
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

  String tokens[3];
  uint16_t ticks = 0;
  uint8_t freq = 0;
  bool enable = false;
  if (!parseExactTokens(args, tokens, 3) ||
      !cmd::parseU16Token(tokens[0], ticks) || ticks < 1U || ticks > 4095U ||
      !cmd::parseU8Token(tokens[1], freq) || freq > 3U ||
      !cmd::parseBool01Token(tokens[2], enable)) {
    LOGE("Usage: timer <ticks 1..4095> <freq 0..3> <en 0|1>");
    return;
  }

  RV3032::Status st = g_rtc.setTimer(ticks,
                                     static_cast<RV3032::TimerFrequency>(freq),
                                     enable);
  if (!operationAccepted("timer configuration", st)) {
    LOGE("setTimer() failed: %s", st.msg);
    return;
  }
  LOGI("Timer %s: ticks=%u freq=%u enable=%d",
       st.inProgress() ? "accepted" : "completed",
       static_cast<unsigned>(ticks), static_cast<unsigned>(freq),
       enable ? 1 : 0);
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

  String tokens[2];
  if (!parseExactTokens(args, tokens, 2)) {
    LOGE("Usage: evi <edge|debounce|overwrite> <value>");
    return;
  }
  String sub = tokens[0];
  sub.toLowerCase();

  if (sub == "edge") {
    bool rising = false;
    if (!cmd::parseBool01Token(tokens[1], rising)) {
      LOGE("Usage: evi edge [0|1]");
      return;
    }
    RV3032::Status st = g_rtc.setEviEdge(rising);
    if (!operationAccepted("EVI edge", st)) {
      LOGE("setEviEdge() failed: %s", st.msg);
      return;
    }
    LOGI("EVI edge %s %s", rising ? "rising" : "falling",
         st.inProgress() ? "accepted" : "completed");
    return;
  }

  if (sub == "debounce") {
    uint8_t value = 0;
    if (!cmd::parseU8Token(tokens[1], value) || value > 3U) {
      LOGE("Usage: evi debounce [0..3]");
      return;
    }
    RV3032::Status st = g_rtc.setEviDebounce(
        static_cast<RV3032::EviDebounce>(value));
    if (!operationAccepted("EVI debounce", st)) {
      LOGE("setEviDebounce() failed: %s", st.msg);
      return;
    }
    LOGI("EVI debounce %u %s", static_cast<unsigned>(value),
         st.inProgress() ? "accepted" : "completed");
    return;
  }

  if (sub == "overwrite") {
    bool overwrite = false;
    if (!cmd::parseBool01Token(tokens[1], overwrite)) {
      LOGE("Usage: evi overwrite [0|1]");
      return;
    }
    RV3032::Status st = g_rtc.setEviOverwrite(overwrite);
    if (!operationAccepted("EVI overwrite", st)) {
      LOGE("setEviOverwrite() failed: %s", st.msg);
      return;
    }
    LOGI("EVI overwrite %d %s", overwrite ? 1 : 0,
         st.inProgress() ? "accepted" : "completed");
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
    LOGE("Usage: status_clear <decimal mask 0..255>");
    return;
  }

  String token;
  uint8_t mask = 0;
  if (!parseExactTokens(args, &token, 1) ||
      !cmd::parseU8Token(token, mask)) {
    LOGE("Usage: status_clear <decimal mask 0..255>");
    return;
  }
  RV3032::Status st = g_rtc.clearStatus(mask);
  if (!operationAccepted("Status flag clear", st)) {
    LOGE("clearStatus() failed: %s", st.msg);
    return;
  }
  LOGI("Status clear mask=0x%02X %s", mask,
       st.inProgress() ? "accepted" : "completed");
}

static void cmd_reg(const String& args) {
  String trimmed = args;
  trimmed.trim();
  if (trimmed.length() == 0) {
    LOGE("Usage: reg <addr> [value]");
    return;
  }

  String addrTok;
  if (!popToken(trimmed, addrTok)) {
    LOGE("Usage: reg <addr> [value] [confirm]");
    return;
  }
  uint8_t reg = 0;
  if (!cmd::parseRegisterToken(addrTok, reg)) {
    LOGE("Register address out of range");
    return;
  }
  String valueTok;
  if (!popToken(trimmed, valueTok)) {
    uint8_t value = 0;
    RV3032::Status st = g_rtc.readRegister(reg, value);
    if (!st.ok()) {
      LOGE("readRegister(0x%02X) failed: %s", reg, st.msg);
      return;
    }
    Serial.printf("reg[0x%02X] = 0x%02X\n", reg, value);
    return;
  }

  uint8_t value = 0;
  if (!cmd::parseRegisterToken(valueTok, value)) {
    LOGE("Register value out of range");
    return;
  }

  const bool userRamReg = (reg >= RV3032::cmd::REG_USER_RAM_START &&
                           reg <= RV3032::cmd::REG_USER_RAM_END);
  String confirmTok;
  const bool hasConfirmation = popToken(trimmed, confirmTok);
  String extra;
  if (popToken(trimmed, extra)) {
    LOGE("Usage: reg <addr> [value] [confirm]");
    return;
  }
  confirmTok.toLowerCase();
  if ((!userRamReg && (!hasConfirmation || confirmTok != "confirm")) ||
      (userRamReg && hasConfirmation)) {
    LOGE("Register writes outside user RAM require: reg <addr> <value> confirm");
    return;
  }

  RV3032::Status st = g_rtc.writeRegister(reg, value);
  if (!st.ok()) {
    LOGE("writeRegister(0x%02X) failed: %s", reg, st.msg);
    return;
  }
  LOGI("reg[0x%02X] <= 0x%02X", reg, value);
}

static void cmd_ram(const String& args) {
  String rest = args;
  String token;
  uint8_t offset = 0;
  uint8_t len = 16;

  if (popToken(rest, token)) {
    if (!cmd::parseU8Token(token, offset) || offset >= 16) {
      LOGE("Usage: ram [offset 0..15] [len 1..16]");
      return;
    }
    len = static_cast<uint8_t>(16U - offset);
    if (popToken(rest, token)) {
      if (!cmd::parseU8Token(token, len) || len == 0 || len > static_cast<uint8_t>(16U - offset)) {
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
  if (!popToken(rest, token) || !cmd::parseU8Token(token, offset) || offset >= 16) {
    LOGE("Usage: ram_write <offset 0..15> <byte...>");
    return;
  }

  uint8_t values[16] = {};
  size_t len = 0;
  while (popToken(rest, token)) {
    if (len >= static_cast<size_t>(16U - offset) ||
        !cmd::parseU8Token(token, values[len])) {
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
  if (!operationAccepted("user RAM write", st)) {
    LOGE("writeUserRam() failed: %s", st.msg);
    return;
  }
  LOGI("User RAM write of %u byte(s) at offset %u %s",
       static_cast<unsigned>(len), static_cast<unsigned>(offset),
       st.inProgress() ? "accepted" : "completed");
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
                cli::resultColor(eepromSt.ok()),
                eepromSt.ok() ? "OK" : eepromSt.msg,
                LOG_COLOR_RESET);
  Serial.printf("Generic queue items: %lu succeeded (%lu failed)\n",
                static_cast<unsigned long>(g_rtc.eepromWriteCount()),
                static_cast<unsigned long>(g_rtc.eepromWriteFailures()));
  Serial.printf("Queue depth: %u\n", g_rtc.eepromQueueDepth());

  struct EepromReg {
    uint8_t reg;
    const char* name;
  };
  static const EepromReg kRegs[] = {
    {RV3032::cmd::REG_ACTIVE_PMU, "PMU"},
    {RV3032::cmd::REG_ACTIVE_OFFSET, "OFFSET"},
    {RV3032::cmd::REG_ACTIVE_CLKOUT1, "CLKOUT1"},
    {RV3032::cmd::REG_ACTIVE_CLKOUT2, "CLKOUT2"},
    {RV3032::cmd::REG_ACTIVE_TREFERENCE0, "TREF0"},
    {RV3032::cmd::REG_ACTIVE_TREFERENCE1, "TREF1"},
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
 * @brief Handle the read-only 'backup' battery-backup PMU diagnostic.
 */
static void cmd_backup(const String& args) {
  String modeArg = args;
  modeArg.trim();
  modeArg.toLowerCase();

  if (modeArg.length() == 0 || modeArg == "status") {
    uint8_t pmu = 0;
    RV3032::Status st = g_rtc.readRegister(RV3032::cmd::REG_ACTIVE_PMU, pmu);
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
    const bool clkoutDisabled = (pmu & RV3032::cmd::PMU_NCLKE_MASK) != 0;
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
                  cli::boolText(snap.enableEepromWrites),
                  cli::boolText(snap.eepromBusy),
                  static_cast<unsigned>(snap.eepromQueueDepth));
    Serial.printf("CLKOUT disabled bit: %s\n", cli::boolText(clkoutDisabled));
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

  RV3032::BackupSwitchMode mode = RV3032::BackupSwitchMode::Off;
  if (modeArg == "direct") {
    mode = RV3032::BackupSwitchMode::Direct;
  } else if (modeArg == "level") {
    mode = RV3032::BackupSwitchMode::Level;
  } else if (modeArg != "off") {
    LOGE("Usage: backup [status|off|direct|level]");
    return;
  }

  const RV3032::Status st =
      g_rtc.startSetBackupSwitchModeJob(mode, millis());
  if (!operationAccepted("backup configuration", st)) {
    LOGE("startSetBackupSwitchModeJob() failed: %s", st.msg);
    return;
  }
  LOGI("Backup mode %s %s", backup_mode_name(mode),
       st.inProgress() ? "accepted" : "completed");
}

static void cmd_primary_cell(const String& args) {
  String tokens[2];
  if (!parseExactTokens(args, tokens, 2) || tokens[0] != "ensure" ||
      tokens[1] != "CONFIRM-PRIMARY-CELL") {
    LOGE("Exact confirmation required: primary-cell ensure CONFIRM-PRIMARY-CELL");
    return;
  }

  LOGW("Proceed only after proving primary-cell chemistry, stable VDD >=1.6 V, ");
  LOGW("VDD >=2.0 V at 400 kHz, VDD >2.2 V through level-switch settle, and safe backfeed topology");

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
  Serial.printf("Primary-cell ensure: %s status=%s stage=%s before=%s0x%02X target=0x%02X after=%s0x%02X cleanup=%s\n",
                outcome, errToStr(st.code),
                primary_failure_stage_name(report.failureStage),
                report.persistentBeforeValid ? "" : "?",
                report.persistentBefore,
                report.persistentTarget,
                report.persistentAfterValid ? "" : "?",
                report.persistentAfter,
                report.cleanupVerified ? "verified" : "unverified");
  Serial.printf("  operation=%s detail=%ld cleanup_status=%s detail=%ld write_attempted=%s durable=%s persistent_target_verified=%s active_target_verified=%s active_after=0x%02X control1_after=0x%02X auto_refresh_held=%s\n",
                errToStr(report.operationStatus.code),
                static_cast<long>(report.operationStatus.detail),
                errToStr(report.cleanupStatus.code),
                static_cast<long>(report.cleanupStatus.detail),
                cli::boolText(report.writeCommandAttempted),
                cli::boolText(report.writeDurablyVerified),
                cli::boolText(report.persistentTargetVerified),
                cli::boolText(report.activeTargetVerified),
                report.activeAfter, report.control1After,
                cli::boolText(report.autoRefreshHeldDisabledForSafety));
}

/**
 * @brief Handle 'clear_bsf' command - clear backup switchover flag.
 */
static void cmd_clear_bsf() {
  RV3032::Status st = g_rtc.clearBackupSwitchFlag();
  if (!operationAccepted("backup flag clear", st)) {
    LOGE("clearBackupSwitchFlag() failed: %s", st.msg);
  } else {
    LOGI("Backup switchover flag clear %s",
         st.inProgress() ? "accepted" : "completed");
  }
}

/**
 * @brief Handle 'clear_porf' command - clear power-on reset flag.
 */
static void cmd_clear_porf() {
  RV3032::Status st = g_rtc.clearPowerOnResetFlag();
  if (!operationAccepted("power-on reset flag clear", st)) {
    LOGE("clearPowerOnResetFlag() failed: %s", st.msg);
  } else {
    LOGI("Power-on reset flag clear %s",
         st.inProgress() ? "accepted" : "completed");
  }
}

/**
 * @brief Handle 'clear_vlf' command - clear voltage low flag.
 */
static void cmd_clear_vlf() {
  RV3032::Status st = g_rtc.clearVoltageLowFlag();
  print_verbose_status("clearVoltageLowFlag", st);
  if (!operationAccepted("voltage-low flag clear", st)) {
    LOGE("clearVoltageLowFlag() failed: %s", st.msg);
  } else {
    LOGI("Voltage low flag clear %s",
         st.inProgress() ? "accepted" : "completed");
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
  const uint32_t totalOk = snap.totalSuccess;
  const uint32_t totalFail = snap.totalFailures;
  const uint64_t total =
      static_cast<uint64_t>(totalOk) + static_cast<uint64_t>(totalFail);
  const double successRate = total > 0U
      ? 100.0 * static_cast<double>(totalOk) / static_cast<double>(total)
      : 0.0;
  Serial.printf("State: %s%s%s\n",
                stateColor(state),
                stateToStr(state),
                LOG_COLOR_RESET);
  Serial.printf("isInitialized: %s%s%s\n",
                snap.initialized ? LOG_COLOR_GREEN : LOG_COLOR_RED,
                cli::boolText(snap.initialized),
                LOG_COLOR_RESET);
  Serial.printf("Config: addr=0x%02X i2cTimeout=%lu offlineThreshold=%u nowMs=%s\n",
                static_cast<unsigned>(snap.i2cAddress),
                static_cast<unsigned long>(snap.i2cTimeoutMs),
                static_cast<unsigned>(snap.offlineThreshold),
                cli::boolText(snap.hasNowMsHook));
  Serial.printf("EEPROM: busy=%s generic_persistence=%s timeout=%lu queue=%u queue_ok=%lu queue_fail=%lu\n",
                cli::boolText(snap.eepromBusy),
                cli::boolText(snap.enableEepromWrites),
                static_cast<unsigned long>(snap.eepromTimeoutMs),
                static_cast<unsigned>(snap.eepromQueueDepth),
                static_cast<unsigned long>(snap.eepromWriteCount),
                static_cast<unsigned long>(snap.eepromWriteFailures));
  Serial.println();
  
  Serial.println(F("=== Counters ==="));
  Serial.printf("Consecutive Failures: %s%d%s\n",
                cli::zeroGoodColor(snap.consecutiveFailures),
                snap.consecutiveFailures,
                LOG_COLOR_RESET);
  Serial.printf("Total Successes: %s%lu%s\n",
                cli::nonZeroGoodColor(totalOk),
                static_cast<unsigned long>(totalOk),
                LOG_COLOR_RESET);
  Serial.printf("Total Failures: %s%lu%s\n",
                cli::zeroGoodColor(totalFail),
                static_cast<unsigned long>(totalFail),
                LOG_COLOR_RESET);
  Serial.printf("Success rate: %s%.1f%%%s\n",
                (successRate >= 99.9f) ? LOG_COLOR_GREEN :
                (successRate >= 80.0f) ? LOG_COLOR_YELLOW : LOG_COLOR_RED,
                successRate,
                LOG_COLOR_RESET);
  Serial.println();
  
  Serial.println(F("=== Timestamps ==="));
  const uint32_t now = millis();
  const uint32_t lastOk = snap.lastOkMs;
  const uint32_t lastErr = snap.lastErrorMs;
  
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
  const RV3032::Status lastError = snap.lastError;
  Serial.printf("Code: %s%s%s (%d)\n",
                cli::resultColor(lastError.code == RV3032::Err::OK),
                errToStr(lastError.code),
                LOG_COLOR_RESET,
                static_cast<int>(lastError.code));
  Serial.printf("Detail: %ld\n", static_cast<long>(lastError.detail));
  Serial.printf("Message: %s\n", lastError.msg ? lastError.msg : "(none)");
  Serial.println();
  
  Serial.println(F("=== EEPROM State ==="));
  Serial.printf("Busy: %s%s%s\n",
                snap.eepromBusy ? LOG_COLOR_YELLOW : LOG_COLOR_GREEN,
                snap.eepromBusy ? "true" : "false",
                LOG_COLOR_RESET);
  RV3032::Status eepromSt = g_rtc.getEepromStatus();
  Serial.printf("Status: %s%s%s\n",
                cli::resultColor(eepromSt.ok()),
                eepromSt.ok() ? "OK" : eepromSt.msg,
                LOG_COLOR_RESET);
  Serial.println();
}

/**
 * @brief Handle one raw 0x51 Status-register communication probe.
 */
static void cmd_probe() {
  Serial.println(F("Probing address 0x51 Status-register communication (no health tracking)..."));
  
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
    LOGI("Probe OK - address 0x51 responded for this transaction; chip identity is not proven");
  } else {
    LOGE("Probe FAILED: %s (code=%s, detail=%ld)",
         st.msg, errToStr(st.code), static_cast<long>(st.detail));
  }
  
  // Verify no health change
  bool healthChanged = (failsBefore != failsAfter) ||
                       (successBefore != successAfter) ||
                       (failureBefore != failureAfter);
  
  Serial.printf("Health tracking: %s%s%s\n",
                cli::resultColor(!healthChanged),
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
                stateColor(stateBefore), stateToStr(stateBefore), LOG_COLOR_RESET,
                stateColor(stateAfter), stateToStr(stateAfter), LOG_COLOR_RESET);
  Serial.printf("Consecutive failures: %s%d%s -> %s%d%s\n",
                cli::zeroGoodColor(failsBefore), failsBefore, LOG_COLOR_RESET,
                cli::zeroGoodColor(failsAfter), failsAfter, LOG_COLOR_RESET);
}

/**
 * @brief Handle 'verbose' command - enable/disable verbose mode.
 */
static void cmd_verbose(const String& args) {
  if (args.length() == 0) {
    Serial.printf("Verbose mode: %s%s%s\n",
                  cli::enabledColor(g_verbose),
                  g_verbose ? "ON" : "OFF",
                  LOG_COLOR_RESET);
    return;
  }
  
  String token;
  bool verbose = false;
  if (!parseExactTokens(args, &token, 1) ||
      !cmd::parseBool01Token(token, verbose)) {
    LOGE("Usage: verbose [0|1]");
    return;
  }
  g_verbose = verbose;
  LOGI("Verbose mode: %s%s%s",
       cli::enabledColor(g_verbose),
       g_verbose ? "ON" : "OFF",
       LOG_COLOR_RESET);
}

/**
 * @brief Handle 'stress' command - rapid time reads stress test.
 * Tests I2C reliability and health tracking under load.
 */
static void cmd_stress(const String& args) {
  uint32_t iterationCount = 100;
  if (args.length() > 0) {
    String token;
    if (!parseExactTokens(args, &token, 1) ||
        !cmd::parseU32Token(token, iterationCount) ||
        iterationCount < 1U || iterationCount > 100000U) {
      LOGE("Usage: stress [1..100000]");
      return;
    }
  }
  const int iterations = static_cast<int>(iterationCount);
  
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
                cli::resultColor(successMatch),
                successMatch ? "OK" : "MISMATCH!",
                LOG_COLOR_RESET);
  Serial.printf("Total failures: %lu -> %lu (expected %lu) %s%s%s\n",
                static_cast<unsigned long>(failureBefore),
                static_cast<unsigned long>(failureAfter),
                static_cast<unsigned long>(expectedFailure),
                cli::resultColor(failureMatch),
                failureMatch ? "OK" : "MISMATCH!",
                LOG_COLOR_RESET);
  Serial.printf("Driver state: %s%s%s -> %s%s%s\n",
                stateColor(stateBefore),
                stateToStr(stateBefore),
                LOG_COLOR_RESET,
                stateColor(stateAfter),
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
  uint32_t iterationCount = 50;
  if (args.length() > 0) {
    String token;
    if (!parseExactTokens(args, &token, 1) ||
        !cmd::parseU32Token(token, iterationCount) ||
        iterationCount < 1U || iterationCount > 100000U) {
      LOGE("Usage: stress_mix [1..100000]");
      return;
    }
  }
  const int iterations = static_cast<int>(iterationCount);
  
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
                stateColor(mixState),
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
    const char* color = skip ? LOG_COLOR_YELLOW : cli::resultColor(ok);
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
                  cli::nonZeroGoodColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                  cli::zeroGoodColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                  cli::warningIfNonZeroColor(stats.skip), static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
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
  reportCheck("driver state initialized",
              g_rtc.state() != RV3032::DriverState::UNINIT, "");

  Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                cli::nonZeroGoodColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                cli::zeroGoodColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                cli::warningIfNonZeroColor(stats.skip), static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
  Serial.println();
}

/**
 * @brief Process command string.
 */
static void process_command(const String& line) {
  if (line.length() == 0) {
    return;
  }

  String remaining = line;
  String command;
  if (!popToken(remaining, command)) {
    return;
  }
  command.toLowerCase();
  remaining.trim();
  const String args = remaining;

  const auto noArguments = [&](const char* usage) {
    if (args.length() == 0U) {
      return true;
    }
    LOGE("Usage: %s", usage);
    return false;
  };

  if (command == "help" || command == "?") {
    if (noArguments("help")) print_help();
  } else if (command == "version" || command == "ver") {
    if (noArguments("version")) cmd_version();
  } else if (command == "scan") {
    if (noArguments("scan")) i2c_scanner::scan(Wire);
  } else if (command == "read") {
    if (noArguments("read")) cmd_time();
  } else if (command == "cfg" || command == "settings") {
    if (noArguments("cfg")) cmd_drv();
  } else if (command == "time") {
    if (noArguments("time")) cmd_time();
  } else if (command == "set") {
    cmd_set(args);
  } else if (command == "setbuild") {
    if (noArguments("setbuild")) cmd_setbuild();
  } else if (command == "unix") {
    cmd_unix(args);
  } else if (command == "temp") {
    if (noArguments("temp")) cmd_temp();
  } else if (command == "ts") {
    cmd_ts(args);
  } else if (command == "ts_reset") {
    cmd_ts_reset(args);
  } else if (command == "alarm") {
    if (noArguments("alarm")) cmd_alarm();
  } else if (command == "alarm_set") {
    cmd_alarm_set(args);
  } else if (command == "alarm_match") {
    cmd_alarm_match(args);
  } else if (command == "alarm_int") {
    cmd_alarm_int(args);
  } else if (command == "alarm_clear") {
    if (noArguments("alarm_clear")) cmd_alarm_clear();
  } else if (command == "clkout") {
    cmd_clkout(args);
  } else if (command == "clkout_freq") {
    cmd_clkout_freq(args);
  } else if (command == "offset") {
    cmd_offset(args);
  } else if (command == "timer") {
    cmd_timer(args);
  } else if (command == "evi") {
    cmd_evi(args);
  } else if (command == "status") {
    if (noArguments("status")) cmd_status();
  } else if (command == "statusf") {
    if (noArguments("statusf")) cmd_statusf();
  } else if (command == "status_clear") {
    cmd_status_clear(args);
  } else if (command == "validity") {
    if (noArguments("validity")) cmd_validity();
  } else if (command == "ram") {
    cmd_ram(args);
  } else if (command == "ram_write") {
    cmd_ram_write(args);
  } else if (command == "reg") {
    cmd_reg(args);
  } else if (command == "eeprom") {
    if (noArguments("eeprom")) cmd_eeprom();
  } else if (command == "backup") {
    cmd_backup(args);
  } else if (command == "primary-cell") {
    cmd_primary_cell(args);
  } else if (command == "clear_porf") {
    if (noArguments("clear_porf")) cmd_clear_porf();
  } else if (command == "clear_vlf") {
    if (noArguments("clear_vlf")) cmd_clear_vlf();
  } else if (command == "clear_bsf") {
    if (noArguments("clear_bsf")) cmd_clear_bsf();
  } else if (command == "drv") {
    if (noArguments("drv")) cmd_drv();
  } else if (command == "probe") {
    if (noArguments("probe")) cmd_probe();
  } else if (command == "recover") {
    if (noArguments("recover")) cmd_recover();
  } else if (command == "verbose") {
    cmd_verbose(args);
  } else if (command == "stress") {
    cmd_stress(args);
  } else if (command == "stress_mix") {
    cmd_stress_mix(args);
  } else if (command == "selftest") {
    if (noArguments("selftest")) cmd_selftest();
  } else {
    LOGW("Unknown command: '%s'. Type 'help' for available commands.",
         command.c_str());
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

  LOGI("RTC callbacks bound; probing address 0x51 communication explicitly...");
  st = g_rtc.probe();
  if (!st.ok()) {
    LOGE("RTC probe failed: %s (code=%s, detail=%ld)",
         st.msg, errToStr(st.code), static_cast<long>(st.detail));
    return;
  }
  LOGI("RTC address communication succeeded; chip identity, backup chemistry, and EEPROM policy were not proven or applied");

  LOGI("Driver state: %s", stateToStr(g_rtc.state()));
  LOGI("Type 'help' for available commands");
  LOGI("Type 'drv' for driver health, 'verbose 1' for detailed output");
  cli::printPrompt();
}

void loop() {
  if (g_pendingOperation.surface != PendingSurface::NONE) {
    pollPendingOperation(millis());
    delay(10);
    return;
  }

  String line;
  if (cli_shell::readLine(line)) {
    process_command(line);
    if (g_pendingOperation.surface == PendingSurface::NONE) {
      cli::printPrompt();
    }
  }

  delay(10);
}
