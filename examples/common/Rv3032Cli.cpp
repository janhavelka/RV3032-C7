#include "Rv3032Cli.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "CommandHandler.h"
#include "RV3032/CommandTable.h"
#include "RV3032/Version.h"

namespace rv3032cli {

namespace {
constexpr const char* COLOR_RESET = "\033[0m";
constexpr const char* COLOR_RED = "\033[31m";
constexpr const char* COLOR_GREEN = "\033[32m";
constexpr const char* COLOR_YELLOW = "\033[33m";
constexpr const char* COLOR_CYAN = "\033[36m";
constexpr const char* COLOR_GRAY = "\033[90m";

const char* resultColor(bool ok) { return ok ? COLOR_GREEN : COLOR_RED; }
const char* boolText(bool value) { return value ? "yes" : "no"; }
const char* enabledColor(bool enabled) { return enabled ? COLOR_GREEN : COLOR_RESET; }
const char* zeroGoodColor(uint32_t value) { return value == 0U ? COLOR_GREEN : COLOR_RED; }
const char* nonZeroGoodColor(uint32_t value) { return value > 0U ? COLOR_GREEN : COLOR_YELLOW; }
const char* warningIfNonZeroColor(uint32_t value) { return value > 0U ? COLOR_YELLOW : COLOR_RESET; }

bool equals(const char* token, const char* expected) {
  return std::strcmp(token, expected) == 0;
}

bool equalsIgnoreCase(const char* token, const char* expected) {
  while (*token && *expected &&
         std::tolower(static_cast<unsigned char>(*token)) == *expected) {
    ++token;
    ++expected;
  }
  return *token == '\0' && *expected == '\0';
}

}  // namespace

struct Engine::Arguments {
  // Even the shortest tokens need one byte and a separator in the input line.
  const char* values[(MAX_LINE_LENGTH + 1U) / 2U];
  size_t count = 0;
};

Engine::Engine(RV3032::RV3032& rtc, Hooks hooks) : _rtc(rtc), _hooks(hooks) {}

void Engine::reset() {
  _pending = {};
  _verbose = false;
  _length = 0;
  _discardRemaining = 0;
  _discardUntilNewline = false;
}

void Engine::print(const char* format, ...) {
  va_list args;
  va_start(args, format);
  writeFormatted(false, format, args);
  va_end(args);
}

void Engine::println(const char* format, ...) {
  va_list args;
  va_start(args, format);
  writeFormatted(true, format, args);
  va_end(args);
}

void Engine::writeFormatted(bool endLine, const char* format, va_list args) {
  // Sized for a full input line in an error and the longest diagnostic row.
  char text[512];
  const int written = std::vsnprintf(text, sizeof(text), format, args);
  if (written < 0 || static_cast<size_t>(written) >= sizeof(text)) {
    _hooks.write(_hooks.user, "CLI output formatting failed", true);
    return;
  }
  _hooks.write(_hooks.user, text, endLine);
}

#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif
static_assert(LOG_LEVEL >= 0 && LOG_LEVEL <= 2, "Invalid CLI log level");
// Keep the newline in the formatted message. A line-ending write has the
// platform's println semantics, which can append CRLF.
#define LOGE(fmt, ...) do { if (LOG_LEVEL >= 1) print("%s[E]%s " fmt "\n", COLOR_RED, COLOR_RESET, ##__VA_ARGS__); } while (0)
#define LOGW(fmt, ...) do { if (LOG_LEVEL >= 2) print("%s[W]%s " fmt "\n", COLOR_YELLOW, COLOR_RESET, ##__VA_ARGS__); } while (0)
#define LOGI(fmt, ...) do { if (LOG_LEVEL >= 2) print("%s[I]%s " fmt "\n", COLOR_CYAN, COLOR_RESET, ##__VA_ARGS__); } while (0)

void Engine::printHelpSection(const char* title) {
  print("\n%s[%s]%s\n", COLOR_GREEN, title, COLOR_RESET);
}

void Engine::printHelpItem(const char* command, const char* description) {
  print("  %s%-32s%s - %s\n", COLOR_CYAN, command, COLOR_RESET, description);
}

void Engine::scan(uint16_t timeoutMs) {
  LOGI("Scanning I2C bus (timeout=%dms)...", timeoutMs);
  LOGI("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
  uint8_t count = 0;
  for (unsigned row = 0; row < 8; ++row) {
    print("%02X: ", row * 16U);
    for (unsigned col = 0; col < 16; ++col) {
      const uint8_t address = static_cast<uint8_t>(row * 16U + col);
      if (address < 0x08 || address > 0x77) {
        print("   ");
        continue;
      }
      const ScanResult result = _hooks.probeAddress(_hooks.user, address, timeoutMs);
      if (result == ScanResult::PRESENT) {
        print("%02X ", address);
        ++count;
      } else {
        print("%s", result == ScanResult::TIMEOUT ? "TO " : "-- ");
      }
      _hooks.sleepMs(_hooks.user, 0);
      _hooks.sleepMs(_hooks.user, 1);
    }
    println();
  }
  LOGI("Scan complete. Found %d device(s).", count);
  if (count) {
    LOGI("Common addresses: 0x3C/0x3D=OLED, 0x48-0x4B=ADS1115, 0x51=RV3032, 0x76/0x77=BME280");
  }
}

bool Engine::operationAccepted(const char* name, const RV3032::Status& status) {
  if (status.inProgress()) {
    _pending = PendingOperation{};
    _pending.surface = PendingSurface::ORDINARY_JOB;
    _pending.name = name;
    _pending.ordinaryStatus = RV3032::Status::Ok();
    _pending.lastStatus = status;
    _pending.startedMs = _hooks.nowMs(_hooks.user);
    return true;
  }
  return status.ok();
}

static const char* primaryFailureStageName(
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
    case RV3032::DriverState::READY: return COLOR_GREEN;
    case RV3032::DriverState::DEGRADED: return COLOR_YELLOW;
    case RV3032::DriverState::OFFLINE: return COLOR_RED;
    case RV3032::DriverState::UNINIT: return COLOR_GRAY;
    default: return COLOR_RESET;
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

void Engine::printConfigurationReport(const char* name) {
  RV3032::ConfigurationJobReport report{};
  RV3032::Status result = RV3032::Status::Error(
      RV3032::Err::JOB_RESULT_UNAVAILABLE, "No configuration result");
  if (strcmp(name, "timer configuration") == 0) {
    result = _rtc.getSetTimerJobResult(report);
  } else if (strcmp(name, "backup configuration") == 0) {
    result = _rtc.getSetBackupSwitchModeJobResult(report);
  } else if (strcmp(name, "CLKOUT configuration") == 0) {
    result = _rtc.getSetClkoutConfigJobResult(report);
  } else {
    return;
  }

  if (result.code == RV3032::Err::JOB_RESULT_UNAVAILABLE ||
      result.code == RV3032::Err::IN_PROGRESS) {
    LOGE("%s result evidence unavailable: %s", name, errToStr(result.code));
    return;
  }
  print(
      "%s evidence: final=%s mutation_attempted=%s operation=%s detail=%ld cleanup=%s detail=%ld\n",
      name, configurationFinalStateToStr(report.finalState),
      boolText(report.mutationAttempted),
      errToStr(report.operationStatus.code),
      static_cast<long>(report.operationStatus.detail),
      errToStr(report.cleanupStatus.code),
      static_cast<long>(report.cleanupStatus.detail));
}

void Engine::reportPendingCompletion(const RV3032::Status* persistenceStatus) {
  const char* name = _pending.name == nullptr
                         ? "cooperative operation"
                         : _pending.name;
  const RV3032::Status operationStatus = _pending.ordinaryStatus;
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
    const RV3032::Status settingsStatus = _rtc.getSettings(snapshot);
    if (settingsStatus.ok()) {
      print(
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

void Engine::reportPendingOverdue(uint32_t nowMs) {
  if (_pending.overdueReported ||
      nowMs - _pending.startedMs < PENDING_WARNING_MS) return;
  _pending.overdueReported = true;
  LOGE("Pending %s operation '%s' exceeded 15 s: %s (detail=%ld, message=%s); polling continues",
       _pending.surface == PendingSurface::ORDINARY_JOB
           ? "ordinary job" : "EEPROM",
       _pending.name == nullptr ? "unnamed" : _pending.name,
       errToStr(_pending.lastStatus.code),
       static_cast<long>(_pending.lastStatus.detail),
       _pending.lastStatus.msg == nullptr
           ? "" : _pending.lastStatus.msg);
}

void Engine::pollPendingOperation(uint32_t nowMs) {
  if (_pending.surface == PendingSurface::ORDINARY_JOB) {
    uint8_t instructionsUsed = 0;
    const RV3032::Status status =
        _rtc.pollJob(nowMs, 1, instructionsUsed);
    _pending.lastStatus = status;
    if (status.inProgress()) {
      reportPendingOverdue(nowMs);
      return;
    }
    _pending.ordinaryStatus = status;
    if (_rtc.isEepromBusy()) {
      _pending.surface = PendingSurface::EEPROM;
      return;
    }
    _discardRemaining = _hooks.available(_hooks.user);
    reportPendingCompletion(nullptr);
    _pending = PendingOperation{};
    print("> ");
    return;
  }

  if (_pending.surface == PendingSurface::EEPROM) {
    uint8_t instructionsUsed = 0;
    const RV3032::Status status =
        _rtc.pollEeprom(nowMs, 1, instructionsUsed);
    _pending.lastStatus = status;
    if (status.inProgress() || _rtc.isEepromBusy()) {
      reportPendingOverdue(nowMs);
      return;
    }
    _discardRemaining = _hooks.available(_hooks.user);
    reportPendingCompletion(&status);
    _pending = PendingOperation{};
    print("> ");
  }
}

/**
 * @brief Print verbose status details after any operation.
 */
void Engine::printVerboseStatus(const char* op, const RV3032::Status& st) {
  if (!_verbose) return;

  println("  --- Verbose Status ---");
  print("  Operation: %s\n", op);
  print("  Result: %s%s%s (code=%s, detail=%ld)\n",
        resultColor(st.ok()),
        st.ok() ? "OK" : "FAILED",
        COLOR_RESET,
        errToStr(st.code),
        static_cast<long>(st.detail));
  if (st.msg) {
    print("  Message: %s\n", st.msg);
  }

  // Driver health snapshot
  const RV3032::DriverState drvState = _rtc.state();
  print("  Driver State: %s%s%s\n",
        stateColor(drvState),
        stateToStr(drvState),
        COLOR_RESET);
  print("  Consecutive Failures: %d\n", _rtc.consecutiveFailures());
  print("  Total: success=%lu, failures=%lu\n",
        static_cast<unsigned long>(_rtc.totalSuccess()),
        static_cast<unsigned long>(_rtc.totalFailures()));

  uint32_t now = _hooks.nowMs(_hooks.user);
  uint32_t lastOk = _rtc.lastOkMs();
  uint32_t lastErr = _rtc.lastErrorMs();
  print("  Last OK: %lu ms ago\n",
        static_cast<unsigned long>(lastOk > 0 ? (now - lastOk) : 0));
  if (lastErr > 0) {
    RV3032::Status lastError = _rtc.lastError();
    print("  Last Error: %lu ms ago (%s: %s)\n",
          static_cast<unsigned long>(now - lastErr),
          errToStr(lastError.code),
          lastError.msg ? lastError.msg : "");
  }
  println("  ----------------------");
}

RV3032::Status Engine::readUserEepromChunk(uint8_t offset, uint8_t length,
                                          uint8_t* out) {
  if (out == nullptr || offset >= RV3032::USER_EEPROM_SIZE || length == 0U ||
      length > RV3032::USER_EEPROM_JOB_MAX_BYTES ||
      length > static_cast<uint8_t>(RV3032::USER_EEPROM_SIZE - offset)) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM,
                                 "Invalid user EEPROM chunk bounds");
  }
  static constexpr uint32_t TIMEOUT_MS = 1000U;
  const uint32_t startedMs = _hooks.nowMs(_hooks.user);
  const uint32_t deadlineMs = startedMs + TIMEOUT_MS;
  RV3032::Status st = _rtc.startReadUserEepromJob(
      offset, length, startedMs, TIMEOUT_MS);
  // Each continuing poll sleeps at least 1 ms. The count also bounds a
  // misbehaving clock/sleep hook before the final deadline poll.
  for (uint32_t pass = 0; st.inProgress() && pass < TIMEOUT_MS &&
       static_cast<int32_t>(deadlineMs - _hooks.nowMs(_hooks.user)) > 0; ++pass) {
    uint8_t used = 0;
    st = _rtc.pollJob(_hooks.nowMs(_hooks.user), 1, used);
    if (st.inProgress()) _hooks.sleepMs(_hooks.user, 1);
  }
  if (st.inProgress()) {
    uint8_t used = 0;
    st = _rtc.pollJob(deadlineMs, 1, used);
  }
  if (!st.ok()) return st;
  RV3032::PersistentReadResult result{};
  st = _rtc.getPersistentReadJobResult(result);
  if (!st.ok()) return st;
  if (!result.persistentVerified || result.length != length) {
    return RV3032::Status::Error(RV3032::Err::INCOHERENT_DATA,
                                 "Persistent read proof is incomplete");
  }
  memcpy(out, result.data, length);
  return RV3032::Status::Ok();
}

static bool parseTimestampSource(const char* token, RV3032::TimestampSource& source) {
  if (equalsIgnoreCase(token, "tlow") || equalsIgnoreCase(token, "low")) {
    source = RV3032::TimestampSource::TLow;
    return true;
  }
  if (equalsIgnoreCase(token, "thigh") || equalsIgnoreCase(token, "high")) {
    source = RV3032::TimestampSource::THigh;
    return true;
  }
  if (equalsIgnoreCase(token, "evi") || equalsIgnoreCase(token, "event")) {
    source = RV3032::TimestampSource::Evi;
    return true;
  }
  return false;
}

static const char* timestampSourceName(RV3032::TimestampSource source) {
  switch (source) {
    case RV3032::TimestampSource::TLow: return "TLow";
    case RV3032::TimestampSource::THigh: return "THigh";
    case RV3032::TimestampSource::Evi: return "EVI";
    default: return "unknown";
  }
}

static RV3032::BackupSwitchMode backupModeFromPmu(uint8_t pmu) {
  switch (pmu & RV3032::cmd::PMU_BSM_MASK) {
    case RV3032::cmd::PMU_BSM_DIRECT:
      return RV3032::BackupSwitchMode::Direct;
    case RV3032::cmd::PMU_BSM_LEVEL:
      return RV3032::BackupSwitchMode::Level;
    default:
      return RV3032::BackupSwitchMode::Off;
  }
}

static const char* backupModeName(RV3032::BackupSwitchMode mode) {
  switch (mode) {
    case RV3032::BackupSwitchMode::Off: return "off";
    case RV3032::BackupSwitchMode::Level: return "level";
    case RV3032::BackupSwitchMode::Direct: return "direct";
    default: return "unknown";
  }
}

const char* clkoutFrequencyName(RV3032::ClkoutFrequency frequency) {
  switch (frequency) {
    case RV3032::ClkoutFrequency::Hz32768: return "32768Hz";
    case RV3032::ClkoutFrequency::Hz1024: return "1024Hz";
    case RV3032::ClkoutFrequency::Hz64: return "64Hz";
    case RV3032::ClkoutFrequency::Hz1: return "1Hz";
    default: return "unknown";
  }
}

/**
 * @brief Print available commands.
 */
void Engine::printHelp() {
  println();
  print("%s=== RV3032-C7 CLI Help ===%s\n", COLOR_CYAN, COLOR_RESET);
  print("Version: %s\n", RV3032::VERSION);
  print("Built:   %s\n", RV3032::BUILD_TIMESTAMP);
  print("Commit:  %s (%s)\n", RV3032::GIT_COMMIT, RV3032::GIT_STATUS);

  printHelpSection("Common");
  printHelpItem("help / ?", "Show this help");
  printHelpItem("version / ver", "Print firmware and library version info");
  printHelpItem("scan", "Scan I2C bus");
  printHelpItem("read", "Alias of time");
  printHelpItem("cfg / settings", "Alias of drv");
  printHelpItem("time", "Read current time");
  printHelpItem("set [YYYY MM DD HH MM SS]", "Set time (no args = show)");
  printHelpItem("setbuild", "Set time to build timestamp");
  printHelpItem("unix [ts]", "Read or set Unix timestamp");
  printHelpItem("temp", "Read temperature");

  printHelpSection("Alarm And Timer");
  printHelpItem("alarm", "Show alarm configuration");
  printHelpItem("alarm_set [MM HH DD]", "Set alarm time (no args = show)");
  printHelpItem("alarm_match [M H D]", "Set alarm match flags (no args = show)");
  printHelpItem("alarm_int [0|1]", "Disable/enable alarm interrupt (no args = show)");
  printHelpItem("alarm_clear", "Clear alarm flag");
  printHelpItem("timer", "Show timer config");
  printHelpItem("timer <ticks 0..4095> <freq 0..3> <en 0|1>",
                     "Set timer; ticks=0 requires en=0");

  printHelpSection("Clock And Event");
  printHelpItem("clkout [0|1]", "Disable/enable clock output (no args = show)");
  printHelpItem("clkout_freq [0..3]", "Set clock frequency (no args = show)");
  printHelpItem("offset [ppm]", "Read or set frequency offset");
  printHelpItem("evi", "Show EVI config");
  printHelpItem("evi edge [0|1]", "Set/read EVI edge (0=falling,1=rising)");
  printHelpItem("evi debounce [0..3]", "Set/read EVI debounce");
  printHelpItem("evi overwrite [0|1]", "Set/read EVI overwrite");
  printHelpItem("ts <tlow|thigh|evi>", "Read decoded timestamp block");
  printHelpItem("ts_reset <tlow|thigh|evi>", "Reset timestamp block");

  printHelpSection("Status And Registers");
  printHelpItem("status", "Read status register");
  printHelpItem("statusf", "Read decoded status flags");
  printHelpItem("status_clear <mask 0..255>", "Clear status flags by explicit decimal mask");
  printHelpItem("validity", "Read PORF/VLF/BSF validity flags");
  printHelpItem("ram [offset len]", "Dump user RAM (default all 16 bytes)");
  printHelpItem("ram_write <offset> <byte...>", "Write decimal user RAM bytes");
  printHelpItem("reg <addr>", "Read register byte");
  printHelpItem("reg <addr> <val> [confirm]", "Write register byte; confirm required outside user RAM");
  printHelpItem("eeprom", "EEPROM stats and user EEPROM dump");
  printHelpItem("backup [status|off|direct|level]", "Show or cooperatively configure backup PMU");
  printHelpItem("primary-cell ensure CONFIRM-PRIMARY-CELL",
                     "Explicit one-shot primary-cell provisioning");
  printHelpItem("clear_porf", "Clear power-on reset flag");
  printHelpItem("clear_vlf", "Clear voltage low flag");
  printHelpItem("clear_bsf", "Clear backup switchover flag");

  printHelpSection("Diagnostics");
  printHelpItem("drv", "Show driver state and health");
  printHelpItem("probe", "Read 0x51 Status register without health tracking");
  printHelpItem("recover", "Manual recovery attempt");
  printHelpItem("verbose [0|1]", "Detailed status for time, clear_vlf, and recover");
  printHelpItem("stress [N]", "Run N iterations stress test (default 100)");
  printHelpItem("stress_mix [N]", "Run N iterations mixed operations test");
  printHelpItem("selftest", "Run safe command self-test report");
  println();
}

void Engine::cmdVersion() {
  println("=== Version Info ===");
  _hooks.platformVersion(_hooks.user, *this);
  print("  RV3032 library version: %s\n", RV3032::VERSION);
  print("  RV3032 library full: %s\n", RV3032::VERSION_FULL);
  print("  RV3032 library build: %s\n", RV3032::BUILD_TIMESTAMP);
  print("  RV3032 library commit: %s (%s)\n", RV3032::GIT_COMMIT, RV3032::GIT_STATUS);
}

/**
 * @brief Format and print DateTime structure.
 */
void Engine::printDatetime(const RV3032::DateTime& dt) {
  print("%04d-%02d-%02d %02d:%02d:%02d (weekday=%d)\n",
        dt.year, dt.month, dt.day,
        dt.hour, dt.minute, dt.second,
        dt.weekday);
}

void Engine::warnIfTimeInvalid() {
  RV3032::ValidityFlags flags;
  RV3032::Status st = _rtc.readValidity(flags);
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
void Engine::cmdTime() {
  RV3032::DateTime dt;
  RV3032::Status st = _rtc.readTime(dt);
  printVerboseStatus("readTime", st);
  if (!st.ok()) {
    LOGE("readTime() failed: %s", st.msg);
    return;
  }
  print("Current time: ");
  printDatetime(dt);
  warnIfTimeInvalid();
}

/**
 * @brief Handle 'set' command - set time.
 * Example: "set 2026 01 10 15 30 00"
 */
void Engine::cmdSet(const Arguments& args) {
  if (args.count == 0) {
    cmdTime();
    return;
  }

  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  if (args.count != 6 ||
      !cmd::parseU16Token(args.values[0], year) ||
      !cmd::parseU8Token(args.values[1], month) ||
      !cmd::parseU8Token(args.values[2], day) ||
      !cmd::parseU8Token(args.values[3], hour) ||
      !cmd::parseU8Token(args.values[4], minute) ||
      !cmd::parseU8Token(args.values[5], second)) {
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

  st = _rtc.setTime(dt);
  if (!st.ok()) {
    LOGE("setTime() failed: %s", st.msg);
  } else {
    LOGI("Calendar set completed");
    printDatetime(dt);
  }
}

/**
 * @brief Handle 'setbuild' command - set time to build timestamp.
 */
void Engine::cmdSetbuild() {
  RV3032::DateTime dt;
  if (!RV3032::RV3032::parseBuildTime(dt).ok()) {
    LOGE("parseBuildTime() failed");
    return;
  }

  RV3032::Status st = _rtc.setTime(dt);
  if (!st.ok()) {
    LOGE("setTime() failed: %s", st.msg);
  } else {
    LOGI("Build timestamp calendar set completed");
    printDatetime(dt);
  }
}

/**
 * @brief Handle 'unix' command - read/set Unix timestamp.
 */
void Engine::cmdUnix(const Arguments& args) {
  if (args.count == 0) {
    uint32_t ts = 0;
    RV3032::Status st = _rtc.readUnix(ts);
    if (!st.ok()) {
      LOGE("readUnix() failed: %s", st.msg);
      return;
    }
    print("Unix timestamp: %lu\n", static_cast<unsigned long>(ts));
    warnIfTimeInvalid();
    return;
  }

  uint32_t ts = 0;
  if (args.count != 1 ||
      !cmd::parseU32Token(args.values[0], ts)) {
    LOGE("Usage: unix [timestamp]");
    return;
  }
  RV3032::Status st = _rtc.setUnix(ts);
  if (!st.ok()) {
    LOGE("setUnix() failed: %s", st.msg);
    return;
  }
  LOGI("Unix timestamp %lu set completed", static_cast<unsigned long>(ts));
}

/**
 * @brief Handle 'temp' command - read temperature.
 */
void Engine::cmdTemp() {
  float celsius = 0;
  RV3032::Status st = _rtc.readTemperatureC(celsius);
  if (!st.ok()) {
    LOGE("readTemperatureC() failed: %s", st.msg);
    return;
  }
  print("Temperature: %.2f C\n", celsius);
}

void Engine::cmdTs(const char* argument) {
  RV3032::TimestampSource source;
  if (!parseTimestampSource(argument, source)) {
    LOGE("Usage: ts <tlow|thigh|evi>");
    return;
  }

  RV3032::Timestamp ts;
  RV3032::Status st = _rtc.readTimestamp(source, ts);
  if (!st.ok()) {
    LOGE("readTimestamp(%s) failed: %s", timestampSourceName(source), st.msg);
    return;
  }

  print("%s timestamp: count=%u", timestampSourceName(source), ts.count);
  if (ts.hasHundredths) {
    print(" hundredths=%u", ts.hundredths);
  }
  println();
  if (!ts.timeValid) {
    println("  Time: empty/unset");
    return;
  }
  print("  Time: ");
  printDatetime(ts.time);
}

void Engine::cmdTsReset(const char* argument) {
  RV3032::TimestampSource source;
  if (!parseTimestampSource(argument, source)) {
    LOGE("Usage: ts_reset <tlow|thigh|evi>");
    return;
  }

  RV3032::Status st = _rtc.resetTimestamp(source);
  if (!operationAccepted("timestamp reset", st)) {
    LOGE("resetTimestamp(%s) failed: %s", timestampSourceName(source), st.msg);
    return;
  }
  LOGI("%s timestamp reset %s", timestampSourceName(source),
       st.inProgress() ? "accepted" : "completed");
}

/**
 * @brief Handle 'alarm' command - show alarm configuration.
 */
void Engine::cmdAlarm() {
  RV3032::AlarmConfig cfg;
  RV3032::Status st = _rtc.getAlarmConfig(cfg);
  if (!st.ok()) {
    LOGE("getAlarmConfig() failed: %s", st.msg);
    return;
  }

  print("Alarm time: %02d:%02d (date=%02d)\n",
        cfg.hour, cfg.minute, cfg.date);
  print("Match: minute=%d hour=%d date=%d\n",
        cfg.matchMinute, cfg.matchHour, cfg.matchDate);

  bool intEnabled = false;
  st = _rtc.getAlarmInterruptEnabled(intEnabled);
  if (st.ok()) {
    print("Interrupt: %s\n", intEnabled ? "enabled" : "disabled");
  }

  bool triggered = false;
  st = _rtc.getAlarmFlag(triggered);
  if (st.ok()) {
    print("Flag: %s\n", triggered ? "TRIGGERED" : "clear");
  }
}

/**
 * @brief Handle 'alarm_set' command - set alarm time.
 * Example: "alarm_set 30 15 10" (minute hour date)
 */
void Engine::cmdAlarmSet(const Arguments& args) {
  if (args.count == 0) {
    RV3032::AlarmConfig cfg;
    RV3032::Status st = _rtc.getAlarmConfig(cfg);
    if (!st.ok()) {
      LOGE("getAlarmConfig() failed: %s", st.msg);
      return;
    }
    print("Alarm time: %02d:%02d (date=%02d)\n",
          cfg.hour, cfg.minute, cfg.date);
    return;
  }

  uint8_t minute = 0;
  uint8_t hour = 0;
  uint8_t date = 0;
  if (args.count != 3 ||
      !cmd::parseU8Token(args.values[0], minute) ||
      !cmd::parseU8Token(args.values[1], hour) ||
      !cmd::parseU8Token(args.values[2], date)) {
    LOGE("Invalid format. Usage: alarm_set MM HH DD");
    return;
  }

  RV3032::Status st = _rtc.setAlarmTime(minute, hour, date);
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
void Engine::cmdAlarmMatch(const Arguments& args) {
  if (args.count == 0) {
    RV3032::AlarmConfig cfg;
    RV3032::Status st = _rtc.getAlarmConfig(cfg);
    if (!st.ok()) {
      LOGE("getAlarmConfig() failed: %s", st.msg);
      return;
    }
    print("Match: minute=%d hour=%d date=%d\n",
          cfg.matchMinute, cfg.matchHour, cfg.matchDate);
    return;
  }

  bool matchMin = false;
  bool matchHour = false;
  bool matchDate = false;
  if (args.count != 3 ||
      !cmd::parseBool01Token(args.values[0], matchMin) ||
      !cmd::parseBool01Token(args.values[1], matchHour) ||
      !cmd::parseBool01Token(args.values[2], matchDate)) {
    LOGE("Invalid format. Usage: alarm_match M H D (1=on, 0=off)");
    return;
  }

  RV3032::Status st = _rtc.setAlarmMatch(matchMin, matchHour, matchDate);
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
void Engine::cmdAlarmInt(const Arguments& args) {
  if (args.count == 0) {
    bool enabled = false;
    RV3032::Status st = _rtc.getAlarmInterruptEnabled(enabled);
    if (!st.ok()) {
      LOGE("getAlarmInterruptEnabled() failed: %s", st.msg);
      return;
    }
    print("Alarm interrupt: %s\n", enabled ? "enabled" : "disabled");
    return;
  }

  bool enable = false;
  if (args.count != 1 ||
      !cmd::parseBool01Token(args.values[0], enable)) {
    LOGE("Usage: alarm_int [0|1]");
    return;
  }
  RV3032::Status st = _rtc.enableAlarmInterrupt(enable);
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
void Engine::cmdAlarmClear() {
  RV3032::Status st = _rtc.clearAlarmFlag();
  if (!operationAccepted("alarm flag clear", st)) {
    LOGE("clearAlarmFlag() failed: %s", st.msg);
  } else {
    LOGI("Alarm flag clear %s", st.inProgress() ? "accepted" : "completed");
  }
}

/**
 * @brief Handle 'clkout' command - enable/disable clock output.
 */
void Engine::cmdClkout(const Arguments& args) {
  if (args.count == 0) {
    bool enabled = false;
    RV3032::Status st = _rtc.getClkoutEnabled(enabled);
    if (!st.ok()) {
      LOGE("getClkoutEnabled() failed: %s", st.msg);
      return;
    }
    print("Clock output: %s\n", enabled ? "enabled" : "disabled");
    return;
  }

  bool enable = false;
  if (args.count != 1 ||
      !cmd::parseBool01Token(args.values[0], enable)) {
    LOGE("Usage: clkout [0|1]");
    return;
  }
  RV3032::Status st = _rtc.setClkoutEnabled(enable);
  // This single-register update has terminal Status but does not own the
  // staged CLKOUT ConfigurationJobReport used by setClkoutConfig().
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
void Engine::cmdClkoutFreq(const Arguments& args) {
  if (args.count == 0) {
    RV3032::ClkoutFrequency freq = RV3032::ClkoutFrequency::Hz32768;
    RV3032::Status st = _rtc.getClkoutFrequency(freq);
    if (!st.ok()) {
      LOGE("getClkoutFrequency() failed: %s", st.msg);
      return;
    }
    print("Clock output frequency: %s\n",
          clkoutFrequencyName(freq));
    return;
  }

  uint8_t freq = 0;
  if (args.count != 1 ||
      !cmd::parseU8Token(args.values[0], freq) || freq > 3U) {
    LOGE("Invalid frequency. Range: 0..3");
    return;
  }

  RV3032::ClkoutFrequency freqEnum = static_cast<RV3032::ClkoutFrequency>(freq);
  RV3032::Status st = _rtc.setClkoutFrequency(freqEnum);
  if (!operationAccepted("CLKOUT configuration", st)) {
    LOGE("setClkoutFrequency() failed: %s", st.msg);
    return;
  }
  LOGI("Clock output frequency %s %s", clkoutFrequencyName(freqEnum),
       st.inProgress() ? "accepted" : "completed");
}

/**
 * @brief Handle 'offset' command - read or set frequency offset.
 * Example: "offset" (read) or "offset 5.2" (set to +5.2 ppm)
 */
void Engine::cmdOffset(const Arguments& args) {
  if (args.count == 0) {
    // Read offset
    float ppm = 0;
    RV3032::Status st = _rtc.getOffsetPpm(ppm);
    if (!st.ok()) {
      LOGE("getOffsetPpm() failed: %s", st.msg);
    } else {
      print("Frequency offset: %.2f ppm\n", ppm);
    }
  } else {
    float ppm = 0.0f;
    if (args.count != 1 ||
        !cmd::parseFloatToken(args.values[0], ppm)) {
      LOGE("Usage: offset [finite-ppm]");
      return;
    }
    RV3032::Status st = _rtc.setOffsetPpm(ppm);
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

void Engine::cmdTimer(const Arguments& args) {
  if (args.count == 0) {
    uint16_t ticks = 0;
    RV3032::TimerFrequency freq = RV3032::TimerFrequency::Hz1;
    bool enabled = false;
    RV3032::Status st = _rtc.getTimer(ticks, freq, enabled);
    if (!st.ok()) {
      LOGE("getTimer() failed: %s", st.msg);
      return;
    }
    print("Timer: ticks=%u freq=%s enabled=%d\n",
          static_cast<unsigned>(ticks),
          timerFreqToStr(freq),
          enabled ? 1 : 0);
    return;
  }

  uint16_t ticks = 0;
  uint8_t freq = 0;
  bool enable = false;
  if (args.count != 3 ||
      !cmd::parseU16Token(args.values[0], ticks) ||
      !cmd::parseU8Token(args.values[1], freq) ||
      !cmd::parseBool01Token(args.values[2], enable) || ticks > 4095U ||
      (ticks == 0U && enable) || freq > 3U) {
    LOGE("Usage: timer <ticks 0..4095> <freq 0..3> <en 0|1>; ticks=0 requires en=0");
    return;
  }

  RV3032::Status st = _rtc.setTimer(ticks,
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

void Engine::cmdEvi(const Arguments& args) {
  if (args.count == 0) {
    RV3032::EviConfig cfg;
    RV3032::Status st = _rtc.getEviConfig(cfg);
    if (!st.ok()) {
      LOGE("getEviConfig() failed: %s", st.msg);
      return;
    }
    print("EVI: edge=%s debounce=%s overwrite=%d\n",
          cfg.rising ? "rising" : "falling",
          eviDebounceToStr(cfg.debounce),
          cfg.overwrite ? 1 : 0);
    return;
  }

  if (args.count != 2) {
    LOGE("Usage: evi <edge|debounce|overwrite> <value>");
    return;
  }
  const char* sub = args.values[0];

  if (equalsIgnoreCase(sub, "edge")) {
    bool rising = false;
    if (!cmd::parseBool01Token(args.values[1], rising)) {
      LOGE("Usage: evi edge [0|1]");
      return;
    }
    RV3032::Status st = _rtc.setEviEdge(rising);
    if (!operationAccepted("EVI edge", st)) {
      LOGE("setEviEdge() failed: %s", st.msg);
      return;
    }
    LOGI("EVI edge %s %s", rising ? "rising" : "falling",
         st.inProgress() ? "accepted" : "completed");
    return;
  }

  if (equalsIgnoreCase(sub, "debounce")) {
    uint8_t value = 0;
    if (!cmd::parseU8Token(args.values[1], value) || value > 3U) {
      LOGE("Usage: evi debounce [0..3]");
      return;
    }
    RV3032::Status st = _rtc.setEviDebounce(
        static_cast<RV3032::EviDebounce>(value));
    if (!operationAccepted("EVI debounce", st)) {
      LOGE("setEviDebounce() failed: %s", st.msg);
      return;
    }
    LOGI("EVI debounce %u %s", static_cast<unsigned>(value),
         st.inProgress() ? "accepted" : "completed");
    return;
  }

  if (equalsIgnoreCase(sub, "overwrite")) {
    bool overwrite = false;
    if (!cmd::parseBool01Token(args.values[1], overwrite)) {
      LOGE("Usage: evi overwrite [0|1]");
      return;
    }
    RV3032::Status st = _rtc.setEviOverwrite(overwrite);
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

void Engine::cmdStatusf() {
  RV3032::StatusFlags flags;
  RV3032::Status st = _rtc.readStatusFlags(flags);
  if (!st.ok()) {
    LOGE("readStatusFlags() failed: %s", st.msg);
    return;
  }
  println("Status flags:");
  print("  tempHigh=%d tempLow=%d update=%d timer=%d\n",
        flags.tempHigh ? 1 : 0,
        flags.tempLow ? 1 : 0,
        flags.update ? 1 : 0,
        flags.timer ? 1 : 0);
  print("  alarm=%d event=%d porf=%d vlf=%d\n",
        flags.alarm ? 1 : 0,
        flags.event ? 1 : 0,
        flags.powerOnReset ? 1 : 0,
        flags.voltageLow ? 1 : 0);
}

void Engine::cmdStatusClear(const Arguments& args) {
  if (args.count == 0) {
    LOGE("Usage: status_clear <decimal mask 0..255>");
    return;
  }

  uint8_t mask = 0;
  if (args.count != 1 ||
      !cmd::parseU8Token(args.values[0], mask)) {
    LOGE("Usage: status_clear <decimal mask 0..255>");
    return;
  }
  RV3032::Status st = _rtc.clearStatus(mask);
  if (!operationAccepted("Status flag clear", st)) {
    LOGE("clearStatus() failed: %s", st.msg);
    return;
  }
  LOGI("Status clear mask=0x%02X %s", mask,
       st.inProgress() ? "accepted" : "completed");
}

void Engine::cmdReg(const Arguments& args) {
  if (args.count == 0) {
    LOGE("Usage: reg <addr> [value]");
    return;
  }

  uint8_t reg = 0;
  if (!cmd::parseRegisterToken(args.values[0], reg)) {
    LOGE("Register address out of range");
    return;
  }
  if (args.count == 1) {
    uint8_t value = 0;
    RV3032::Status st = _rtc.readRegister(reg, value);
    if (!st.ok()) {
      LOGE("readRegister(0x%02X) failed: %s", reg, st.msg);
      return;
    }
    print("reg[0x%02X] = 0x%02X\n", reg, value);
    return;
  }

  uint8_t value = 0;
  if (!cmd::parseRegisterToken(args.values[1], value)) {
    LOGE("Register value out of range");
    return;
  }

  const bool userRamReg = (reg >= RV3032::cmd::REG_USER_RAM_START &&
                           reg <= RV3032::cmd::REG_USER_RAM_END);
  const bool hasConfirmation = args.count >= 3;
  if (args.count > 3) {
    LOGE("Usage: reg <addr> [value] [confirm]");
    return;
  }
  if (userRamReg && hasConfirmation) {
    LOGE("User RAM writes do not use confirm: reg <addr> <value>");
    return;
  }
  if (!userRamReg && (!hasConfirmation || !equalsIgnoreCase(args.values[2], "confirm"))) {
    LOGE("Register writes outside user RAM require: reg <addr> <value> confirm");
    return;
  }

  RV3032::Status st = _rtc.writeRegister(reg, value);
  if (!st.ok()) {
    LOGE("writeRegister(0x%02X) failed: %s", reg, st.msg);
    return;
  }
  LOGI("reg[0x%02X] <= 0x%02X", reg, value);
}

void Engine::cmdRam(const Arguments& args) {
  uint8_t offset = 0;
  uint8_t len = 16;

  if (args.count > 0) {
    if (!cmd::parseU8Token(args.values[0], offset) || offset >= 16) {
      LOGE("Usage: ram [offset 0..15] [len 1..16]");
      return;
    }
    len = static_cast<uint8_t>(16U - offset);
    if (args.count > 1) {
      if (!cmd::parseU8Token(args.values[1], len) || len == 0 || len > static_cast<uint8_t>(16U - offset)) {
        LOGE("Usage: ram [offset 0..15] [len 1..16]");
        return;
      }
    }
  }
  if (args.count > 2) {
    LOGE("Usage: ram [offset 0..15] [len 1..16]");
    return;
  }

  uint8_t buf[16] = {};
  RV3032::Status st = _rtc.readUserRam(offset, buf, len);
  if (!st.ok()) {
    LOGE("readUserRam() failed: %s", st.msg);
    return;
  }

  print("User RAM offset %u len %u:\n",
        static_cast<unsigned>(offset),
        static_cast<unsigned>(len));
  for (uint8_t i = 0; i < len; ++i) {
    if ((i % 8) == 0) {
      print("  0x%02X: ", static_cast<unsigned>(offset + i));
    }
    print("%02X ", buf[i]);
    if ((i % 8) == 7 || i == static_cast<uint8_t>(len - 1U)) {
      println();
    }
  }
}

void Engine::cmdRamWrite(const Arguments& args) {
  uint8_t offset = 0;
  if (args.count == 0 || !cmd::parseU8Token(args.values[0], offset) || offset >= 16) {
    LOGE("Usage: ram_write <offset 0..15> <byte...>");
    return;
  }

  uint8_t values[16] = {};
  size_t len = 0;
  for (size_t i = 1; i < args.count; ++i) {
    if (len >= static_cast<size_t>(16U - offset) ||
        !cmd::parseU8Token(args.values[i], values[len])) {
      LOGE("Usage: ram_write <offset 0..15> <byte...>");
      return;
    }
    len++;
  }
  if (len == 0) {
    LOGE("Usage: ram_write <offset 0..15> <byte...>");
    return;
  }

  RV3032::Status st = _rtc.writeUserRam(offset, values, len);
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
void Engine::cmdStatus() {
  uint8_t status = 0;
  RV3032::Status st = _rtc.readStatus(status);
  if (!st.ok()) {
    LOGE("readStatus() failed: %s", st.msg);
    return;
  }
  print("Status register: 0x%02X (binary: ", status);
  for (int i = 7; i >= 0; --i) {
    print("%c", (status & (1 << i)) ? '1' : '0');
  }
  println(")");
}

/**
 * @brief Handle 'validity' command - read validity flags.
 */
void Engine::cmdValidity() {
  RV3032::ValidityFlags flags;
  RV3032::Status st = _rtc.readValidity(flags);
  if (!st.ok()) {
    LOGE("readValidity() failed: %s", st.msg);
    return;
  }

  print("PORF: %s\n", flags.powerOnReset ? "set" : "clear");
  print("VLF:  %s\n", flags.voltageLow ? "set" : "clear");
  print("BSF:  %s\n", flags.backupSwitched ? "set" : "clear");
  print("Time: %s\n", flags.timeInvalid ? "invalid" : "valid");
}

/**
 * @brief Handle 'eeprom' command - show EEPROM stats and dump user EEPROM.
 */
void Engine::cmdEeprom() {
  println();
  println("=== EEPROM ===");

  const bool busy = _rtc.isEepromBusy();
  print("Busy: %s\n", busy ? "true" : "false");
  RV3032::Status eepromSt = _rtc.getEepromStatus();
  print("Status: %s%s%s\n",
        resultColor(eepromSt.ok()),
        eepromSt.ok() ? "OK" : eepromSt.msg,
        COLOR_RESET);
  print("Generic queue items: %lu succeeded (%lu failed)\n",
        static_cast<unsigned long>(_rtc.eepromWriteCount()),
        static_cast<unsigned long>(_rtc.eepromWriteFailures()));
  print("Queue depth: %u\n", _rtc.eepromQueueDepth());

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

  println("Active configuration mirrors (not persistent EEPROM):");
  for (size_t i = 0; i < (sizeof(kRegs) / sizeof(kRegs[0])); ++i) {
    uint8_t value = 0;
    RV3032::Status st = _rtc.readRegister(kRegs[i].reg, value);
    if (!st.ok()) {
      LOGE("readRegister(0x%02X) failed: %s", kRegs[i].reg, st.msg);
      return;
    }
    print("  %s (0x%02X): 0x%02X\n", kRegs[i].name, kRegs[i].reg, value);
  }

  if (busy) {
    println("User EEPROM dump skipped while busy.");
    return;
  }

  println("User EEPROM (0xCB..0xEA):");
  static constexpr uint8_t kSize = RV3032::USER_EEPROM_SIZE;
  uint8_t nonFF = 0;

  for (uint8_t offset = 0; offset < kSize; offset += 16) {
    uint8_t values[16] = {};
    RV3032::Status st = readUserEepromChunk(offset, 16, values);
    if (!st.ok()) {
      LOGE("User EEPROM read failed at offset %u: %s", offset, st.msg);
      return;
    }
    for (uint8_t i = 0; i < 16; ++i) {
      if (values[i] != 0xFF) ++nonFF;
      if ((i % 8) == 0) {
        print("  0x%02X: ",
              RV3032::cmd::USER_EEPROM_START + offset + i);
      }
      print("%02X ", values[i]);
      if ((i % 8) == 7) println();
    }
  }

  print("Non-0xFF bytes: %u/%u (heuristic)\n", nonFF, kSize);
}

/**
 * @brief Handle the read-only 'backup' battery-backup PMU diagnostic.
 */
void Engine::cmdBackup(const char* modeArg, bool hasArgument) {
  if (!hasArgument || equalsIgnoreCase(modeArg, "status")) {
    uint8_t pmu = 0;
    RV3032::Status st = _rtc.readRegister(RV3032::cmd::REG_ACTIVE_PMU, pmu);
    if (!st.ok()) {
      LOGE("readRegister(PMU) failed: %s", st.msg);
      return;
    }

    RV3032::SettingsSnapshot snap;
    (void)_rtc.getSettings(snap);
    RV3032::ValidityFlags flags;
    st = _rtc.readValidity(flags);
    if (!st.ok()) {
      LOGE("readValidity() failed: %s", st.msg);
      return;
    }

    const RV3032::BackupSwitchMode mode = backupModeFromPmu(pmu);
    const bool clkoutDisabled = (pmu & RV3032::cmd::PMU_NCLKE_MASK) != 0;
    const bool trickleEnabled = (pmu & RV3032::cmd::PMU_TCM_MASK) != 0;

    println();
    println("=== Battery Backup ===");
    print("PMU 0xC0: 0x%02X\n", pmu);
    print("Backup mode: %s%s%s\n",
          (mode == RV3032::BackupSwitchMode::Level || mode == RV3032::BackupSwitchMode::Direct)
              ? COLOR_GREEN : COLOR_YELLOW,
          backupModeName(mode),
          COLOR_RESET);
    print("EEPROM persistence: %s busy=%s queue=%u\n",
          boolText(snap.enableEepromWrites),
          boolText(snap.eepromBusy),
          static_cast<unsigned>(snap.eepromQueueDepth));
    print("CLKOUT disabled bit: %s\n", boolText(clkoutDisabled));
    print("Trickle charger bits: 0x%X (%s)\n",
           static_cast<unsigned>(pmu &
                                 (RV3032::cmd::PMU_TCR_MASK |
                                  RV3032::cmd::PMU_TCM_MASK)),
          trickleEnabled ? "enabled/configured" : "off");
    print("Flags: PORF=%s VLF=%s BSF=%s time=%s\n",
          flags.powerOnReset ? "set" : "clear",
          flags.voltageLow ? "set" : "clear",
          flags.backupSwitched ? "set" : "clear",
          flags.timeInvalid ? "invalid" : "valid");
    println("Time is held by the RTC counter while VBACKUP is present; it is not stored in EEPROM.");
    println("Primary-cell provisioning is separate: primary-cell ensure CONFIRM-PRIMARY-CELL");
    return;
  }

  RV3032::BackupSwitchMode mode = RV3032::BackupSwitchMode::Off;
  if (equalsIgnoreCase(modeArg, "direct")) {
    mode = RV3032::BackupSwitchMode::Direct;
  } else if (equalsIgnoreCase(modeArg, "level")) {
    mode = RV3032::BackupSwitchMode::Level;
  } else if (!equalsIgnoreCase(modeArg, "off")) {
    LOGE("Usage: backup [status|off|direct|level]");
    return;
  }

  const RV3032::Status st =
      _rtc.startSetBackupSwitchModeJob(mode, _hooks.nowMs(_hooks.user));
  if (!operationAccepted("backup configuration", st)) {
    LOGE("startSetBackupSwitchModeJob() failed: %s", st.msg);
    return;
  }
  LOGI("Backup mode %s %s", backupModeName(mode),
       st.inProgress() ? "accepted" : "completed");
}

void Engine::cmdPrimaryCell(const Arguments& args) {
  if (args.count != 2 || !equals(args.values[0], "ensure") ||
      !equals(args.values[1], "CONFIRM-PRIMARY-CELL")) {
    LOGE("Exact confirmation required: primary-cell ensure CONFIRM-PRIMARY-CELL");
    return;
  }

  LOGW("Proceed only after proving primary-cell chemistry, stable VDD >=1.6 V, ");
  LOGW("VDD >=2.0 V at 400 kHz, VDD >2.2 V through level-switch settle, and safe backfeed topology");

  RV3032::PrimaryCellConfigurationReport report{};
  const RV3032::Status st = _rtc.ensurePrimaryCellConfiguration(report);
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
  print("Primary-cell ensure: %s status=%s stage=%s before=%s0x%02X target=0x%02X after=%s0x%02X cleanup=%s\n",
        outcome, errToStr(st.code),
        primaryFailureStageName(report.failureStage),
        report.persistentBeforeValid ? "" : "?",
        report.persistentBefore,
        report.persistentTarget,
        report.persistentAfterValid ? "" : "?",
        report.persistentAfter,
        report.cleanupVerified ? "verified" : "unverified");
  print("  operation=%s detail=%ld cleanup_status=%s detail=%ld write_attempted=%s durable=%s persistent_target_verified=%s active_target_verified=%s active_after=0x%02X control1_after=0x%02X auto_refresh_held=%s\n",
        errToStr(report.operationStatus.code),
        static_cast<long>(report.operationStatus.detail),
        errToStr(report.cleanupStatus.code),
        static_cast<long>(report.cleanupStatus.detail),
        boolText(report.writeCommandAttempted),
        boolText(report.writeDurablyVerified),
        boolText(report.persistentTargetVerified),
        boolText(report.activeTargetVerified),
        report.activeAfter, report.control1After,
        boolText(report.autoRefreshHeldDisabledForSafety));
}

/**
 * @brief Handle 'clear_bsf' command - clear backup switchover flag.
 */
void Engine::cmdClearBsf() {
  RV3032::Status st = _rtc.clearBackupSwitchFlag();
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
void Engine::cmdClearPorf() {
  RV3032::Status st = _rtc.clearPowerOnResetFlag();
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
void Engine::cmdClearVlf() {
  RV3032::Status st = _rtc.clearVoltageLowFlag();
  printVerboseStatus("clearVoltageLowFlag", st);
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
void Engine::cmdDrv() {
  println();
  println("=== Driver Health ===");
  RV3032::SettingsSnapshot snap;
  (void)_rtc.getSettings(snap);
  const RV3032::DriverState state = snap.state;
  const uint32_t totalOk = snap.totalSuccess;
  const uint32_t totalFail = snap.totalFailures;
  const uint64_t total =
      static_cast<uint64_t>(totalOk) + static_cast<uint64_t>(totalFail);
  const double successRate = total > 0U
      ? 100.0 * static_cast<double>(totalOk) / static_cast<double>(total)
      : 0.0;
  print("State: %s%s%s\n",
        stateColor(state),
        stateToStr(state),
        COLOR_RESET);
  print("isInitialized: %s%s%s\n",
        snap.initialized ? COLOR_GREEN : COLOR_RED,
        boolText(snap.initialized),
        COLOR_RESET);
  print("Config: addr=0x%02X i2cTimeout=%lu offlineThreshold=%u nowMs=%s\n",
        static_cast<unsigned>(snap.i2cAddress),
        static_cast<unsigned long>(snap.i2cTimeoutMs),
        static_cast<unsigned>(snap.offlineThreshold),
        boolText(snap.hasNowMsHook));
  print("EEPROM: busy=%s generic_persistence=%s timeout=%lu queue=%u queue_ok=%lu queue_fail=%lu\n",
        boolText(snap.eepromBusy),
        boolText(snap.enableEepromWrites),
        static_cast<unsigned long>(snap.eepromTimeoutMs),
        static_cast<unsigned>(snap.eepromQueueDepth),
        static_cast<unsigned long>(snap.eepromWriteCount),
        static_cast<unsigned long>(snap.eepromWriteFailures));
  println();

  println("=== Counters ===");
  print("Consecutive Failures: %s%d%s\n",
        zeroGoodColor(snap.consecutiveFailures),
        snap.consecutiveFailures,
        COLOR_RESET);
  print("Total Successes: %s%lu%s\n",
        nonZeroGoodColor(totalOk),
        static_cast<unsigned long>(totalOk),
        COLOR_RESET);
  print("Total Failures: %s%lu%s\n",
        zeroGoodColor(totalFail),
        static_cast<unsigned long>(totalFail),
        COLOR_RESET);
  print("Success rate: %s%.1f%%%s\n",
        (successRate >= 99.9f) ? COLOR_GREEN :
        (successRate >= 80.0f) ? COLOR_YELLOW : COLOR_RED,
        successRate,
        COLOR_RESET);
  println();

  println("=== Timestamps ===");
  const uint32_t now = _hooks.nowMs(_hooks.user);
  const uint32_t lastOk = snap.lastOkMs;
  const uint32_t lastErr = snap.lastErrorMs;

  if (lastOk > 0) {
    print("Last OK: %lu ms ago (at %lu ms)\n",
          static_cast<unsigned long>(now - lastOk),
          static_cast<unsigned long>(lastOk));
  } else {
    println("Last OK: never");
  }

  if (lastErr > 0) {
    print("Last Error: %lu ms ago (at %lu ms)\n",
          static_cast<unsigned long>(now - lastErr),
          static_cast<unsigned long>(lastErr));
  } else {
    println("Last Error: never");
  }
  println();

  println("=== Last Error Details ===");
  const RV3032::Status lastError = snap.lastError;
  print("Code: %s%s%s (%d)\n",
        resultColor(lastError.code == RV3032::Err::OK),
        errToStr(lastError.code),
        COLOR_RESET,
        static_cast<int>(lastError.code));
  print("Detail: %ld\n", static_cast<long>(lastError.detail));
  print("Message: %s\n", lastError.msg ? lastError.msg : "(none)");
  println();

  println("=== EEPROM State ===");
  print("Busy: %s%s%s\n",
        snap.eepromBusy ? COLOR_YELLOW : COLOR_GREEN,
        snap.eepromBusy ? "true" : "false",
        COLOR_RESET);
  RV3032::Status eepromSt = _rtc.getEepromStatus();
  print("Status: %s%s%s\n",
        resultColor(eepromSt.ok()),
        eepromSt.ok() ? "OK" : eepromSt.msg,
        COLOR_RESET);
  println();
}

/**
 * @brief Handle one raw 0x51 Status-register communication probe.
 */
void Engine::cmdProbe() {
  println("Probing address 0x51 Status-register communication (no health tracking)...");

  // Capture health before
  uint8_t failsBefore = _rtc.consecutiveFailures();
  uint32_t successBefore = _rtc.totalSuccess();
  uint32_t failureBefore = _rtc.totalFailures();

  RV3032::Status st = _rtc.probe();

  // Capture health after
  uint8_t failsAfter = _rtc.consecutiveFailures();
  uint32_t successAfter = _rtc.totalSuccess();
  uint32_t failureAfter = _rtc.totalFailures();

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

  print("Health tracking: %s%s%s\n",
        resultColor(!healthChanged),
        healthChanged ? "CHANGED (unexpected!)" : "unchanged (correct)",
        COLOR_RESET);
}

/**
 * @brief Handle 'recover' command - manual recovery attempt.
 */
void Engine::cmdRecover() {
  println("Attempting recovery...");

  // Capture state before
  RV3032::DriverState stateBefore = _rtc.state();
  uint8_t failsBefore = _rtc.consecutiveFailures();

  RV3032::Status st = _rtc.recover();
  printVerboseStatus("recover", st);

  // Capture state after
  RV3032::DriverState stateAfter = _rtc.state();
  uint8_t failsAfter = _rtc.consecutiveFailures();

  if (st.ok()) {
    LOGI("Recovery successful");
  } else {
    LOGE("Recovery FAILED: %s (code=%s, detail=%ld)",
         st.msg, errToStr(st.code), static_cast<long>(st.detail));
  }

  print("State: %s%s%s -> %s%s%s\n",
        stateColor(stateBefore), stateToStr(stateBefore), COLOR_RESET,
        stateColor(stateAfter), stateToStr(stateAfter), COLOR_RESET);
  print("Consecutive failures: %s%d%s -> %s%d%s\n",
        zeroGoodColor(failsBefore), failsBefore, COLOR_RESET,
        zeroGoodColor(failsAfter), failsAfter, COLOR_RESET);
}

/**
 * @brief Handle 'verbose' command - enable/disable verbose mode.
 */
void Engine::cmdVerbose(const Arguments& args) {
  if (args.count == 0) {
    print("Verbose mode: %s%s%s\n",
          enabledColor(_verbose),
          _verbose ? "ON" : "OFF",
          COLOR_RESET);
    return;
  }

  bool verbose = false;
  if (args.count != 1 ||
      !cmd::parseBool01Token(args.values[0], verbose)) {
    LOGE("Usage: verbose [0|1]");
    return;
  }
  _verbose = verbose;
  LOGI("Verbose mode: %s%s%s",
       enabledColor(_verbose),
       _verbose ? "ON" : "OFF",
       COLOR_RESET);
}

/**
 * @brief Handle 'stress' command - rapid time reads stress test.
 * Tests I2C reliability and health tracking under load.
 */
void Engine::cmdStress(const Arguments& args) {
  uint32_t iterationCount = 100;
  if (args.count > 0) {
    if (args.count != 1 ||
        !cmd::parseU32Token(args.values[0], iterationCount) ||
        iterationCount < 1U || iterationCount > 100000U) {
      LOGE("Usage: stress [1..100000]");
      return;
    }
  }
  const int iterations = static_cast<int>(iterationCount);

  print("\n=== Stress Test: %d iterations ===\n", iterations);

  // Capture initial state
  uint32_t successBefore = _rtc.totalSuccess();
  uint32_t failureBefore = _rtc.totalFailures();
  RV3032::DriverState stateBefore = _rtc.state();

  uint32_t startMs = _hooks.nowMs(_hooks.user);
  int okCount = 0;
  int failCount = 0;
  uint32_t minTimeUs = UINT32_MAX;
  uint32_t maxTimeUs = 0;
  uint64_t totalTimeUs = 0;

  RV3032::DateTime dt;

  for (int i = 0; i < iterations; i++) {
    uint32_t opStart = _hooks.nowUs(_hooks.user);
    RV3032::Status st = _rtc.readTime(dt);
    uint32_t opTime = _hooks.nowUs(_hooks.user) - opStart;

    if (st.ok()) {
      okCount++;
      totalTimeUs += opTime;
      if (opTime < minTimeUs) minTimeUs = opTime;
      if (opTime > maxTimeUs) maxTimeUs = opTime;
    } else {
      failCount++;
      print("  [%d] FAIL: %s (code=%s)\n", i, st.msg, errToStr(st.code));
    }

    // Progress indicator every 10% (guard against small iteration counts)
    const int progressStep = (iterations >= 10) ? (iterations / 10) : iterations;
    if (progressStep > 0 && ((i + 1) % progressStep) == 0) {
      print("  Progress: %d%%\n", ((i + 1) * 100) / iterations);
    }

    // Allow watchdog/system tasks
    _hooks.sleepMs(_hooks.user, 0);
  }

  uint32_t totalMs = _hooks.nowMs(_hooks.user) - startMs;

  // Results
  println("\n--- Results ---");
  const bool stressAllOk = (failCount == 0);
  const char* successColor = stressAllOk ? COLOR_GREEN : COLOR_YELLOW;
  const char* failColor = (failCount == 0) ? COLOR_GREEN : COLOR_RED;
  print("%sOK%s: %d, %sFAIL%s: %d (%.2f%% %ssuccess%s)\n",
        COLOR_GREEN, COLOR_RESET, okCount,
        failColor, COLOR_RESET, failCount,
        iterations > 0 ? (okCount * 100.0f / iterations) : 0.0f,
        successColor, COLOR_RESET);
  print("Total time: %lu ms (%.1f ops/sec)\n",
        static_cast<unsigned long>(totalMs),
        totalMs > 0 ? (iterations * 1000.0f / totalMs) : 0.0f);

  if (okCount > 0) {
    print("Per-op timing: min=%lu us, max=%lu us, avg=%lu us\n",
          static_cast<unsigned long>(minTimeUs),
          static_cast<unsigned long>(maxTimeUs),
          static_cast<unsigned long>(static_cast<uint32_t>(totalTimeUs / okCount)));
  }

  // Health tracking verification
  println("\n--- Health Tracking ---");
  uint32_t successAfter = _rtc.totalSuccess();
  uint32_t failureAfter = _rtc.totalFailures();
  RV3032::DriverState stateAfter = _rtc.state();

  uint32_t expectedSuccess = successBefore + okCount;
  uint32_t expectedFailure = failureBefore + failCount;

  const bool successMatch = (successAfter == expectedSuccess);
  const bool failureMatch = (failureAfter == expectedFailure);
  print("Total success: %lu -> %lu (expected %lu) %s%s%s\n",
        static_cast<unsigned long>(successBefore),
        static_cast<unsigned long>(successAfter),
        static_cast<unsigned long>(expectedSuccess),
        resultColor(successMatch),
        successMatch ? "OK" : "MISMATCH!",
        COLOR_RESET);
  print("Total failures: %lu -> %lu (expected %lu) %s%s%s\n",
        static_cast<unsigned long>(failureBefore),
        static_cast<unsigned long>(failureAfter),
        static_cast<unsigned long>(expectedFailure),
        resultColor(failureMatch),
        failureMatch ? "OK" : "MISMATCH!",
        COLOR_RESET);
  print("Driver state: %s%s%s -> %s%s%s\n",
        stateColor(stateBefore),
        stateToStr(stateBefore),
        COLOR_RESET,
        stateColor(stateAfter),
        stateToStr(stateAfter),
        COLOR_RESET);
  print("Consecutive failures: %d\n", _rtc.consecutiveFailures());
  println();
}

/**
 * @brief Handle 'stress_mix' command - mixed operations stress test.
 * Tests various API calls to exercise different code paths.
 */
void Engine::cmdStressMix(const Arguments& args) {
  uint32_t iterationCount = 50;
  if (args.count > 0) {
    if (args.count != 1 ||
        !cmd::parseU32Token(args.values[0], iterationCount) ||
        iterationCount < 1U || iterationCount > 100000U) {
      LOGE("Usage: stress_mix [1..100000]");
      return;
    }
  }
  const int iterations = static_cast<int>(iterationCount);

  print("\n=== Mixed Operations Stress Test: %d iterations ===\n", iterations);

  // Capture initial state
  uint32_t successBefore = _rtc.totalSuccess();
  uint32_t failureBefore = _rtc.totalFailures();

  uint32_t startMs = _hooks.nowMs(_hooks.user);
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
        st = _rtc.readTime(dt);
        break;
      }
      case 1: {
        uint32_t ts;
        st = _rtc.readUnix(ts);
        break;
      }
      case 2: {
        float temp;
        st = _rtc.readTemperatureC(temp);
        break;
      }
      case 3: {
        uint8_t status;
        st = _rtc.readStatus(status);
        break;
      }
      case 4: {
        float ppm;
        st = _rtc.getOffsetPpm(ppm);
        break;
      }
      case 5: {
        bool enabled;
        st = _rtc.getClkoutEnabled(enabled);
        break;
      }
      case 6: {
        RV3032::ValidityFlags flags;
        st = _rtc.readValidity(flags);
        break;
      }
    }

    if (st.ok()) {
      okCount++;
      stats[opIdx].ok++;
    } else {
      failCount++;
      stats[opIdx].fail++;
      print("  [%d] %s FAIL: %s\n", i, stats[opIdx].name, st.msg);
    }

    // Progress indicator every 25% (guard against small iteration counts)
    const int progressStep = (iterations >= 4) ? (iterations / 4) : iterations;
    if (progressStep > 0 && ((i + 1) % progressStep) == 0) {
      print("  Progress: %d%%\n", ((i + 1) * 100) / iterations);
    }

    _hooks.sleepMs(_hooks.user, 0);
  }

  uint32_t totalMs = _hooks.nowMs(_hooks.user) - startMs;

  // Results
  println("\n--- Results ---");
  const bool mixAllOk = (failCount == 0);
  const char* mixSuccessColor = mixAllOk ? COLOR_GREEN : COLOR_YELLOW;
  const char* mixFailColor = (failCount == 0) ? COLOR_GREEN : COLOR_RED;
  print("Total: %sOK%s=%d, %sFAIL%s=%d (%.2f%% %ssuccess%s)\n",
        COLOR_GREEN, COLOR_RESET, okCount,
        mixFailColor, COLOR_RESET, failCount,
        (iterations > 0) ? (okCount * 100.0f / iterations) : 0.0f,
        mixSuccessColor, COLOR_RESET);
  print("Time: %lu ms (%.1f ops/sec)\n\n",
        static_cast<unsigned long>(totalMs),
        totalMs > 0 ? (iterations * 1000.0f / totalMs) : 0.0f);

  println("Per-operation breakdown:");
  for (int i = 0; i < numOps; i++) {
    const char* perFailColor = (stats[i].fail == 0) ? COLOR_GREEN : COLOR_RED;
    print("  %-12s: %sOK%s=%d, %sFAIL%s=%d\n",
          stats[i].name,
          COLOR_GREEN, COLOR_RESET, stats[i].ok,
          perFailColor, COLOR_RESET, stats[i].fail);
  }

  // Health tracking verification
  println("\n--- Health Tracking ---");
  uint32_t successAfter = _rtc.totalSuccess();
  uint32_t failureAfter = _rtc.totalFailures();

  // Note: some operations do multiple I2C calls, so we can't predict exact count
  print("Success delta: %s+%lu%s (ops had %d %sOK%s results)\n",
        COLOR_GREEN,
        static_cast<unsigned long>(successAfter - successBefore),
        COLOR_RESET,
        okCount,
        COLOR_GREEN,
        COLOR_RESET);
  const char* failureDeltaColor = (failureAfter > failureBefore) ? COLOR_RED : COLOR_GREEN;
  const char* failResultsColor = (failCount == 0) ? COLOR_GREEN : COLOR_RED;
  print("Failure delta: %s+%lu%s (ops had %d %sFAIL%s results)\n",
        failureDeltaColor,
        static_cast<unsigned long>(failureAfter - failureBefore),
        COLOR_RESET,
        failCount,
        failResultsColor,
        COLOR_RESET);
  const RV3032::DriverState mixState = _rtc.state();
  print("Driver state: %s%s%s\n",
        stateColor(mixState),
        stateToStr(mixState),
        COLOR_RESET);
  print("Consecutive failures: %d\n", _rtc.consecutiveFailures());
  println();
}

/**
 * @brief Handle 'selftest' command - safe command coverage with report.
 */
void Engine::cmdSelftest() {
  struct SelfTestStats {
    uint32_t pass = 0;
    uint32_t fail = 0;
    uint32_t skip = 0;
  } stats;

  enum class SelftestOutcome : uint8_t { PASS, FAIL, SKIP };
  auto report = [&](const char* name, SelftestOutcome outcome, const char* note) {
    const bool ok = (outcome == SelftestOutcome::PASS);
    const bool skip = (outcome == SelftestOutcome::SKIP);
    const char* color = skip ? COLOR_YELLOW : resultColor(ok);
    const char* tag = skip ? "SKIP" : (ok ? "PASS" : "FAIL");
    print("  [%s%s%s] %s", color, tag, COLOR_RESET, name);
    if (note && note[0]) {
      print(" - %s", note);
    }
    println();
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

  println();
  println("=== RV3032 Selftest (safe commands) ===");

  const uint32_t succBefore = _rtc.totalSuccess();
  const uint32_t failBefore = _rtc.totalFailures();
  const uint8_t consBefore = _rtc.consecutiveFailures();

  RV3032::Status st = _rtc.probe();
  if (st.code == RV3032::Err::NOT_INITIALIZED) {
    reportSkip("probe responds", "driver not initialized");
    reportSkip("remaining checks", "selftest aborted");
    print("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
          nonZeroGoodColor(stats.pass), static_cast<unsigned long>(stats.pass), COLOR_RESET,
          zeroGoodColor(stats.fail), static_cast<unsigned long>(stats.fail), COLOR_RESET,
          warningIfNonZeroColor(stats.skip), static_cast<unsigned long>(stats.skip), COLOR_RESET);
    println();
    return;
  }
  reportCheck("probe responds", st.ok(), st.ok() ? "" : st.msg);
  const bool probeNoTrack = (_rtc.totalSuccess() == succBefore) &&
                            (_rtc.totalFailures() == failBefore) &&
                            (_rtc.consecutiveFailures() == consBefore);
  reportCheck("probe no-health-side-effects", probeNoTrack, "");

  RV3032::DateTime dt;
  st = _rtc.readTime(dt);
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
  st = _rtc.readUnix(unixTs);
  reportCheck("readUnix", st.ok(), st.ok() ? "" : st.msg);

  float tempC = 0.0f;
  st = _rtc.readTemperatureC(tempC);
  reportCheck("readTemperatureC", st.ok(), st.ok() ? "" : st.msg);
  reportCheck("temperature plausible", st.ok() && tempC > -60.0f && tempC < 120.0f, "");

  uint8_t status = 0;
  st = _rtc.readStatus(status);
  reportCheck("readStatus", st.ok(), st.ok() ? "" : st.msg);

  RV3032::ValidityFlags vf;
  st = _rtc.readValidity(vf);
  reportCheck("readValidity", st.ok(), st.ok() ? "" : st.msg);

  RV3032::AlarmConfig alarmCfg;
  st = _rtc.getAlarmConfig(alarmCfg);
  reportCheck("getAlarmConfig", st.ok(), st.ok() ? "" : st.msg);

  bool alarmInt = false;
  st = _rtc.getAlarmInterruptEnabled(alarmInt);
  reportCheck("getAlarmInterruptEnabled", st.ok(), st.ok() ? "" : st.msg);

  bool alarmFlag = false;
  st = _rtc.getAlarmFlag(alarmFlag);
  reportCheck("getAlarmFlag", st.ok(), st.ok() ? "" : st.msg);

  bool clkoutEnabled = false;
  st = _rtc.getClkoutEnabled(clkoutEnabled);
  reportCheck("getClkoutEnabled", st.ok(), st.ok() ? "" : st.msg);

  RV3032::ClkoutFrequency clkFreq = RV3032::ClkoutFrequency::Hz32768;
  st = _rtc.getClkoutFrequency(clkFreq);
  reportCheck("getClkoutFrequency", st.ok(), st.ok() ? "" : st.msg);

  float offset = 0.0f;
  st = _rtc.getOffsetPpm(offset);
  reportCheck("getOffsetPpm", st.ok(), st.ok() ? "" : st.msg);

  if (_rtc.state() == RV3032::DriverState::OFFLINE) {
    reportSkip("recover", "selftest is read-only; use recover command");
  }
  reportCheck("driver state initialized",
              _rtc.state() != RV3032::DriverState::UNINIT, "");

  print("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
        nonZeroGoodColor(stats.pass), static_cast<unsigned long>(stats.pass), COLOR_RESET,
        zeroGoodColor(stats.fail), static_cast<unsigned long>(stats.fail), COLOR_RESET,
        warningIfNonZeroColor(stats.skip), static_cast<unsigned long>(stats.skip), COLOR_RESET);
  println();
}

/**
 * @brief Process command string.
 */
void Engine::dispatch(const char* line) {
  if (!line) return;
  char storage[MAX_LINE_LENGTH + 1U];
  size_t length = 0;
  while (length < MAX_LINE_LENGTH && line[length]) {
    storage[length] = line[length];
    ++length;
  }
  if (line[length]) return;
  storage[length] = '\0';
  dispatchLine(storage, length);
}

void Engine::dispatchLine(char* line, size_t length) {
  char* end = line + length;
  while (end != line && std::isspace(static_cast<unsigned char>(end[-1]))) --end;
  *end = '\0';
  char* cursor = line;
  char* command = cmd::nextToken(cursor, end);
  if (!command) return;
  for (char* p = command; *p; ++p) {
    *p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
  }
  // These commands compare the complete trimmed argument. Keep it intact.
  while (cursor != end && std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
  if (equals(command, "ts")) return cmdTs(cursor);
  if (equals(command, "ts_reset")) return cmdTsReset(cursor);
  if (equals(command, "backup")) return cmdBackup(cursor, cursor != end);

  Arguments args;
  args.count = cmd::splitTokens(cursor, end, args.values,
                                sizeof(args.values) / sizeof(args.values[0]));

  const auto noArguments = [&](const char* usage) {
    if (args.count == 0U) {
      return true;
    }
    LOGE("Usage: %s", usage);
    return false;
  };

  if (equals(command, "help") || equals(command, "?")) {
    if (noArguments("help")) printHelp();
  } else if (equals(command, "version") || equals(command, "ver")) {
    if (noArguments("version")) cmdVersion();
  } else if (equals(command, "scan")) {
    if (noArguments("scan")) scan();
  } else if (equals(command, "read")) {
    if (noArguments("read")) cmdTime();
  } else if (equals(command, "cfg") || equals(command, "settings")) {
    if (noArguments("cfg")) cmdDrv();
  } else if (equals(command, "time")) {
    if (noArguments("time")) cmdTime();
  } else if (equals(command, "set")) {
    cmdSet(args);
  } else if (equals(command, "setbuild")) {
    if (noArguments("setbuild")) cmdSetbuild();
  } else if (equals(command, "unix")) {
    cmdUnix(args);
  } else if (equals(command, "temp")) {
    if (noArguments("temp")) cmdTemp();
  } else if (equals(command, "alarm")) {
    if (noArguments("alarm")) cmdAlarm();
  } else if (equals(command, "alarm_set")) {
    cmdAlarmSet(args);
  } else if (equals(command, "alarm_match")) {
    cmdAlarmMatch(args);
  } else if (equals(command, "alarm_int")) {
    cmdAlarmInt(args);
  } else if (equals(command, "alarm_clear")) {
    if (noArguments("alarm_clear")) cmdAlarmClear();
  } else if (equals(command, "clkout")) {
    cmdClkout(args);
  } else if (equals(command, "clkout_freq")) {
    cmdClkoutFreq(args);
  } else if (equals(command, "offset")) {
    cmdOffset(args);
  } else if (equals(command, "timer")) {
    cmdTimer(args);
  } else if (equals(command, "evi")) {
    cmdEvi(args);
  } else if (equals(command, "status")) {
    if (noArguments("status")) cmdStatus();
  } else if (equals(command, "statusf")) {
    if (noArguments("statusf")) cmdStatusf();
  } else if (equals(command, "status_clear")) {
    cmdStatusClear(args);
  } else if (equals(command, "validity")) {
    if (noArguments("validity")) cmdValidity();
  } else if (equals(command, "ram")) {
    cmdRam(args);
  } else if (equals(command, "ram_write")) {
    cmdRamWrite(args);
  } else if (equals(command, "reg")) {
    cmdReg(args);
  } else if (equals(command, "eeprom")) {
    if (noArguments("eeprom")) cmdEeprom();
  } else if (equals(command, "primary-cell")) {
    cmdPrimaryCell(args);
  } else if (equals(command, "clear_porf")) {
    if (noArguments("clear_porf")) cmdClearPorf();
  } else if (equals(command, "clear_vlf")) {
    if (noArguments("clear_vlf")) cmdClearVlf();
  } else if (equals(command, "clear_bsf")) {
    if (noArguments("clear_bsf")) cmdClearBsf();
  } else if (equals(command, "drv")) {
    if (noArguments("drv")) cmdDrv();
  } else if (equals(command, "probe")) {
    if (noArguments("probe")) cmdProbe();
  } else if (equals(command, "recover")) {
    if (noArguments("recover")) cmdRecover();
  } else if (equals(command, "verbose")) {
    cmdVerbose(args);
  } else if (equals(command, "stress")) {
    cmdStress(args);
  } else if (equals(command, "stress_mix")) {
    cmdStressMix(args);
  } else if (equals(command, "selftest")) {
    if (noArguments("selftest")) cmdSelftest();
  } else {
    LOGW("Unknown command: '%s'. Type 'help' for available commands.",
         command);
  }
}


void Engine::startBus(int sda, int scl) {
  printHelp();
  LOGI("Initializing I2C (SDA=%d, SCL=%d)...", sda, scl);
}

bool Engine::busResult(bool ok) {
  if (!ok) {
    LOGE("I2C init failed");
    return false;
  }
  LOGI("Initializing RTC...");
  return true;
}

bool Engine::beginResult(const RV3032::Status& status) {
  if (!status.ok()) {
    LOGE("RTC init failed: %s (code=%s, detail=%ld)",
         status.msg, errToStr(status.code), static_cast<long>(status.detail));
    LOGE("Check I2C wiring and RTC power");
    return false;
  }
  LOGI("RTC callbacks bound; probing address 0x51 communication explicitly...");
  return true;
}

bool Engine::probeResult(const RV3032::Status& status) {
  if (!status.ok()) {
    LOGE("RTC probe failed: %s (code=%s, detail=%ld)",
         status.msg, errToStr(status.code), static_cast<long>(status.detail));
    return false;
  }
  LOGI("RTC address communication succeeded; chip identity, backup chemistry, and EEPROM policy were not proven or applied");
  LOGI("Driver state: %s", stateToStr(_rtc.state()));
  LOGI("Type 'help' for available commands");
  LOGI("Type 'drv' for driver health, 'verbose 1' for detailed output");
  print("> ");
  return true;
}

bool Engine::readLine(bool discardInput) {
  if (discardInput) {
    _discardRemaining = _hooks.available(_hooks.user);
    if (_length != 0) {
      _length = 0;
      _discardUntilNewline = true;
    }
  }
  for (size_t consumed = 0; consumed < MAX_INPUT_BYTES_PER_POLL; ++consumed) {
    if (!_hooks.available(_hooks.user)) break;
    const int byte = _hooks.readByte(_hooks.user);
    if (byte < 0) break;
    const char c = static_cast<char>(byte);
    if (discardInput || _discardRemaining != 0U) {
      if (_discardRemaining != 0U) --_discardRemaining;
      _discardUntilNewline = c != '\r' && c != '\n';
      continue;
    }
    if (c == '\b' || c == 0x7F) {
      if (!_discardUntilNewline && _length > 0U) --_length;
      continue;
    }
    if (c == '\r' || c == '\n') {
      if (_discardUntilNewline) {
        _length = 0;
        _discardUntilNewline = false;
      } else if (_length != 0) {
        _line[_length] = '\0';
        return true;
      }
      continue;
    }
    if (_discardUntilNewline) continue;
    if (_length < MAX_LINE_LENGTH) {
      _line[_length++] = c;
    } else {
      _length = 0;
      _discardUntilNewline = true;
    }
  }
  return false;
}

void Engine::poll() {
  if (_pending.surface != PendingSurface::NONE) {
    (void)readLine(true);
    pollPendingOperation(_hooks.nowMs(_hooks.user));
  } else if (readLine(false)) {
    dispatchLine(_line, _length);
    _length = 0;
    if (_pending.surface == PendingSurface::NONE) print("> ");
  }
}

}  // namespace rv3032cli
