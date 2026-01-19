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

#include "examples/common/BoardConfig.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"
#include "RV3032/Version.h"
#include "RV3032/RV3032.h"

static RV3032::RV3032 g_rtc;
static bool g_verbose = false;  // Verbose mode - disabled by default for production examples

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
  Serial.printf("  Result: %s (code=%s, detail=%d)\n",
                st.ok() ? "OK" : "FAILED",
                errToStr(st.code),
                st.detail);
  if (st.msg) {
    Serial.printf("  Message: %s\n", st.msg);
  }
  
  // Driver health snapshot
  Serial.printf("  Driver State: %s\n", stateToStr(g_rtc.state()));
  Serial.printf("  isOnline: %s\n", g_rtc.isOnline() ? "true" : "false");
  Serial.printf("  Consecutive Failures: %d\n", g_rtc.consecutiveFailures());
  Serial.printf("  Total: success=%lu, failures=%lu\n",
                static_cast<unsigned long>(g_rtc.totalSuccess()),
                static_cast<unsigned long>(g_rtc.totalFailures()));
  
  uint32_t now = millis();
  uint32_t lastOk = g_rtc.lastOkMs();
  uint32_t lastErr = g_rtc.lastErrorMs();
  Serial.printf("  Last OK: %u ms ago\n", lastOk > 0 ? (now - lastOk) : 0);
  if (lastErr > 0) {
    RV3032::Status lastError = g_rtc.lastError();
    Serial.printf("  Last Error: %u ms ago (%s: %s)\n",
                  now - lastErr,
                  errToStr(lastError.code),
                  lastError.msg ? lastError.msg : "");
  }
  Serial.println(F("  ----------------------"));
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

/**
 * @brief Print available commands.
 */
static void print_help() {
  Serial.println();
  Serial.println(F("=== RV3032-C7 RTC CLI ==="));
  Serial.print(F("Version: "));
  Serial.println(RV3032::VERSION);
  Serial.print(F("Built:   "));
  Serial.println(RV3032::BUILD_TIMESTAMP);
  Serial.print(F("Commit:  "));
  Serial.print(RV3032::GIT_COMMIT);
  Serial.print(F(" ("));
  Serial.print(RV3032::GIT_STATUS);
  Serial.println(F(")"));
  Serial.println();
  Serial.println(F("Commands:"));
  Serial.println(F("  help              - Show this help"));
  Serial.println(F("  time              - Read current time"));
  Serial.println(F("  set YYYY MM DD HH MM SS - Set time (e.g., set 2026 01 10 15 30 00)"));
  Serial.println(F("  setbuild          - Set time to build timestamp"));
  Serial.println(F("  unix              - Read Unix timestamp"));
  Serial.println(F("  temp              - Read temperature"));
  Serial.println(F("  alarm             - Show alarm configuration"));
  Serial.println(F("  alarm_set MM HH DD - Set alarm time"));
  Serial.println(F("  alarm_match M H D  - Enable match (1=on, 0=off)"));
  Serial.println(F("  alarm_int 0|1     - Disable/enable alarm interrupt"));
  Serial.println(F("  alarm_clear       - Clear alarm flag"));
  Serial.println(F("  clkout 0|1        - Disable/enable clock output"));
  Serial.println(F("  clkout_freq 0..3  - Set frequency (0=32768Hz, 1=1024Hz, 2=64Hz, 3=1Hz)"));
  Serial.println(F("  offset [ppm]      - Read or set frequency offset"));
  Serial.println(F("  status            - Read status register"));
  Serial.println(F("  validity          - Read PORF/VLF/BSF validity flags"));
  Serial.println(F("  clear_porf        - Clear power-on reset flag"));
  Serial.println(F("  clear_vlf         - Clear voltage low flag"));
  Serial.println(F("  clear_bsf         - Clear backup switchover flag"));
  Serial.println();
  Serial.println(F("Driver Debugging:"));
  Serial.println(F("  drv               - Show driver state and health"));
  Serial.println(F("  probe             - Probe device (no health tracking)"));
  Serial.println(F("  recover           - Manual recovery attempt"));
  Serial.println(F("  verbose 0|1       - Disable/enable verbose status output"));
  Serial.println(F("  stress [N]        - Run N iterations stress test (default 100)"));
  Serial.println(F("  stress_mix [N]    - Run N iterations mixed operations test"));
  Serial.println();
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
}

/**
 * @brief Handle 'set' command - set time.
 * Example: "set 2026 01 10 15 30 00"
 */
static void cmd_set(const String& args) {
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
 * @brief Handle 'unix' command - read Unix timestamp.
 */
static void cmd_unix() {
  uint32_t ts = 0;
  RV3032::Status st = g_rtc.readUnix(ts);
  if (!st.ok()) {
    LOGE("readUnix() failed: %s", st.msg);
    return;
  }
  Serial.printf("Unix timestamp: %lu\n", static_cast<unsigned long>(ts));
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
  int minute, hour, date;
  if (sscanf(args.c_str(), "%d %d %d", &minute, &hour, &date) != 3) {
    LOGE("Invalid format. Usage: alarm_set MM HH DD");
    return;
  }

  RV3032::Status st = g_rtc.setAlarmTime(minute, hour, date);
  if (!st.ok()) {
    LOGE("setAlarmTime() failed: %s", st.msg);
  } else {
    LOGI("Alarm time set: %02d:%02d (date=%02d)", hour, minute, date);
  }
}

/**
 * @brief Handle 'alarm_match' command - configure alarm matching.
 * Example: "alarm_match 1 1 0" (minute hour date)
 */
static void cmd_alarm_match(const String& args) {
  int matchMin, matchHour, matchDate;
  if (sscanf(args.c_str(), "%d %d %d", &matchMin, &matchHour, &matchDate) != 3) {
    LOGE("Invalid format. Usage: alarm_match M H D (1=on, 0=off)");
    return;
  }

  RV3032::Status st = g_rtc.setAlarmMatch(matchMin != 0, matchHour != 0, matchDate != 0);
  if (!st.ok()) {
    LOGE("setAlarmMatch() failed: %s", st.msg);
  } else {
    LOGI("Alarm match set: minute=%d hour=%d date=%d", matchMin, matchHour, matchDate);
  }
}

/**
 * @brief Handle 'alarm_int' command - enable/disable alarm interrupt.
 * Example: "alarm_int 1"
 */
static void cmd_alarm_int(const String& args) {
  bool enable = (args.toInt() != 0);
  RV3032::Status st = g_rtc.enableAlarmInterrupt(enable);
  if (!st.ok()) {
    LOGE("enableAlarmInterrupt() failed: %s", st.msg);
  } else {
    LOGI("Alarm interrupt %s", enable ? "enabled" : "disabled");
  }
}

/**
 * @brief Handle 'alarm_clear' command - clear alarm flag.
 */
static void cmd_alarm_clear() {
  RV3032::Status st = g_rtc.clearAlarmFlag();
  if (!st.ok()) {
    LOGE("clearAlarmFlag() failed: %s", st.msg);
  } else {
    LOGI("Alarm flag cleared");
  }
}

/**
 * @brief Handle 'clkout' command - enable/disable clock output.
 */
static void cmd_clkout(const String& args) {
  bool enable = (args.toInt() != 0);
  RV3032::Status st = g_rtc.setClkoutEnabled(enable);
  if (!st.ok()) {
    LOGE("setClkoutEnabled() failed: %s", st.msg);
  } else {
    LOGI("Clock output %s", enable ? "enabled" : "disabled");
  }
}

/**
 * @brief Handle 'clkout_freq' command - set clock output frequency.
 * Example: "clkout_freq 3" (0=32768Hz, 1=1024Hz, 2=64Hz, 3=1Hz)
 */
static void cmd_clkout_freq(const String& args) {
  int freq = args.toInt();
  if (freq < 0 || freq > 3) {
    LOGE("Invalid frequency. Range: 0..3");
    return;
  }

  RV3032::ClkoutFrequency freqEnum = static_cast<RV3032::ClkoutFrequency>(freq);
  RV3032::Status st = g_rtc.setClkoutFrequency(freqEnum);
  if (!st.ok()) {
    LOGE("setClkoutFrequency() failed: %s", st.msg);
  } else {
    const char* freqStr[] = {"32768Hz", "1024Hz", "64Hz", "1Hz"};
    LOGI("Clock output frequency set to %s", freqStr[freq]);
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
    if (!st.ok()) {
      LOGE("setOffsetPpm() failed: %s", st.msg);
    } else {
      LOGI("Frequency offset set to %.2f ppm", ppm);
    }
  }
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
 * @brief Handle 'clear_bsf' command - clear backup switchover flag.
 */
static void cmd_clear_bsf() {
  RV3032::Status st = g_rtc.clearBackupSwitchFlag();
  if (!st.ok()) {
    LOGE("clearBackupSwitchFlag() failed: %s", st.msg);
  } else {
    LOGI("Backup switchover flag cleared");
  }
}

/**
 * @brief Handle 'clear_porf' command - clear power-on reset flag.
 */
static void cmd_clear_porf() {
  RV3032::Status st = g_rtc.clearPowerOnResetFlag();
  if (!st.ok()) {
    LOGE("clearPowerOnResetFlag() failed: %s", st.msg);
  } else {
    LOGI("Power-on reset flag cleared");
  }
}

/**
 * @brief Handle 'clear_vlf' command - clear voltage low flag.
 */
static void cmd_clear_vlf() {
  RV3032::Status st = g_rtc.clearVoltageLowFlag();
  print_verbose_status("clearVoltageLowFlag", st);
  if (!st.ok()) {
    LOGE("clearVoltageLowFlag() failed: %s", st.msg);
  } else {
    LOGI("Voltage low flag cleared");
  }
}

// ===== Driver Debugging Commands =====

/**
 * @brief Handle 'drv' command - show driver state and health.
 */
static void cmd_drv() {
  Serial.println();
  Serial.println(F("=== Driver State ==="));
  Serial.printf("State: %s\n", stateToStr(g_rtc.state()));
  Serial.printf("isOnline: %s\n", g_rtc.isOnline() ? "true" : "false");
  Serial.println();
  
  Serial.println(F("=== Health Counters ==="));
  Serial.printf("Consecutive Failures: %d\n", g_rtc.consecutiveFailures());
  Serial.printf("Total Successes: %lu\n", static_cast<unsigned long>(g_rtc.totalSuccess()));
  Serial.printf("Total Failures: %lu\n", static_cast<unsigned long>(g_rtc.totalFailures()));
  Serial.println();
  
  Serial.println(F("=== Timestamps ==="));
  uint32_t now = millis();
  uint32_t lastOk = g_rtc.lastOkMs();
  uint32_t lastErr = g_rtc.lastErrorMs();
  
  if (lastOk > 0) {
    Serial.printf("Last OK: %u ms (%.1f sec ago)\n", 
                  lastOk, (now - lastOk) / 1000.0f);
  } else {
    Serial.println(F("Last OK: never"));
  }
  
  if (lastErr > 0) {
    Serial.printf("Last Error: %u ms (%.1f sec ago)\n",
                  lastErr, (now - lastErr) / 1000.0f);
  } else {
    Serial.println(F("Last Error: never"));
  }
  Serial.println();
  
  Serial.println(F("=== Last Error Details ==="));
  RV3032::Status lastError = g_rtc.lastError();
  Serial.printf("Code: %s (%d)\n", errToStr(lastError.code), static_cast<int>(lastError.code));
  Serial.printf("Detail: %d\n", lastError.detail);
  Serial.printf("Message: %s\n", lastError.msg ? lastError.msg : "(none)");
  Serial.println();
  
  Serial.println(F("=== EEPROM State ==="));
  Serial.printf("Busy: %s\n", g_rtc.isEepromBusy() ? "true" : "false");
  RV3032::Status eepromSt = g_rtc.getEepromStatus();
  Serial.printf("Status: %s\n", eepromSt.ok() ? "OK" : eepromSt.msg);
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
    LOGE("Probe FAILED: %s (code=%s, detail=%d)", 
         st.msg, errToStr(st.code), st.detail);
  }
  
  // Verify no health change
  bool healthChanged = (failsBefore != failsAfter) ||
                       (successBefore != successAfter) ||
                       (failureBefore != failureAfter);
  
  Serial.printf("Health tracking: %s\n", 
                healthChanged ? "CHANGED (unexpected!)" : "unchanged (correct)");
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
    LOGE("Recovery FAILED: %s (code=%s, detail=%d)",
         st.msg, errToStr(st.code), st.detail);
  }
  
  Serial.printf("State: %s -> %s\n", stateToStr(stateBefore), stateToStr(stateAfter));
  Serial.printf("Consecutive failures: %d -> %d\n", failsBefore, failsAfter);
}

/**
 * @brief Handle 'verbose' command - enable/disable verbose mode.
 */
static void cmd_verbose(const String& args) {
  if (args.length() == 0) {
    Serial.printf("Verbose mode: %s\n", g_verbose ? "ON" : "OFF");
    return;
  }
  
  g_verbose = (args.toInt() != 0);
  LOGI("Verbose mode %s", g_verbose ? "enabled" : "disabled");
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
    
    // Progress indicator every 10%
    if ((i + 1) % (iterations / 10) == 0) {
      Serial.printf("  Progress: %d%%\n", ((i + 1) * 100) / iterations);
    }
    
    // Allow watchdog/system tasks
    yield();
  }
  
  uint32_t totalMs = millis() - startMs;
  
  // Results
  Serial.println(F("\n--- Results ---"));
  Serial.printf("OK: %d, FAIL: %d (%.2f%% success)\n", 
                okCount, failCount,
                iterations > 0 ? (okCount * 100.0f / iterations) : 0.0f);
  Serial.printf("Total time: %u ms (%.1f ops/sec)\n",
                totalMs,
                totalMs > 0 ? (iterations * 1000.0f / totalMs) : 0.0f);
  
  if (okCount > 0) {
    Serial.printf("Per-op timing: min=%u us, max=%u us, avg=%u us\n",
                  minTimeUs, maxTimeUs,
                  static_cast<uint32_t>(totalTimeUs / okCount));
  }
  
  // Health tracking verification
  Serial.println(F("\n--- Health Tracking ---"));
  uint32_t successAfter = g_rtc.totalSuccess();
  uint32_t failureAfter = g_rtc.totalFailures();
  RV3032::DriverState stateAfter = g_rtc.state();
  
  uint32_t expectedSuccess = successBefore + okCount;
  uint32_t expectedFailure = failureBefore + failCount;
  
  Serial.printf("Total success: %lu -> %lu (expected %lu) %s\n",
                static_cast<unsigned long>(successBefore),
                static_cast<unsigned long>(successAfter),
                static_cast<unsigned long>(expectedSuccess),
                successAfter == expectedSuccess ? "OK" : "MISMATCH!");
  Serial.printf("Total failures: %lu -> %lu (expected %lu) %s\n",
                static_cast<unsigned long>(failureBefore),
                static_cast<unsigned long>(failureAfter),
                static_cast<unsigned long>(expectedFailure),
                failureAfter == expectedFailure ? "OK" : "MISMATCH!");
  Serial.printf("Driver state: %s -> %s\n", 
                stateToStr(stateBefore), stateToStr(stateAfter));
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
    
    // Progress indicator every 25%
    if (iterations >= 4 && (i + 1) % (iterations / 4) == 0) {
      Serial.printf("  Progress: %d%%\n", ((i + 1) * 100) / iterations);
    }
    
    yield();
  }
  
  uint32_t totalMs = millis() - startMs;
  
  // Results
  Serial.println(F("\n--- Results ---"));
  Serial.printf("Total: OK=%d, FAIL=%d (%.2f%% success)\n", 
                okCount, failCount,
                (iterations > 0) ? (okCount * 100.0f / iterations) : 0.0f);
  Serial.printf("Time: %u ms (%.1f ops/sec)\n\n",
                totalMs,
                totalMs > 0 ? (iterations * 1000.0f / totalMs) : 0.0f);
  
  Serial.println(F("Per-operation breakdown:"));
  for (int i = 0; i < numOps; i++) {
    Serial.printf("  %-12s: OK=%d, FAIL=%d\n", 
                  stats[i].name, stats[i].ok, stats[i].fail);
  }
  
  // Health tracking verification
  Serial.println(F("\n--- Health Tracking ---"));
  uint32_t successAfter = g_rtc.totalSuccess();
  uint32_t failureAfter = g_rtc.totalFailures();
  
  // Note: some operations do multiple I2C calls, so we can't predict exact count
  Serial.printf("Success delta: +%lu (ops had %d OK results)\n",
                static_cast<unsigned long>(successAfter - successBefore), okCount);
  Serial.printf("Failure delta: +%lu (ops had %d FAIL results)\n",
                static_cast<unsigned long>(failureAfter - failureBefore), failCount);
  Serial.printf("Driver state: %s\n", stateToStr(g_rtc.state()));
  Serial.printf("Consecutive failures: %d\n", g_rtc.consecutiveFailures());
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

  if (cmd == "help") {
    print_help();
  } else if (cmd == "time") {
    cmd_time();
  } else if (cmd == "set") {
    cmd_set(args);
  } else if (cmd == "setbuild") {
    cmd_setbuild();
  } else if (cmd == "unix") {
    cmd_unix();
  } else if (cmd == "temp") {
    cmd_temp();
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
  } else if (cmd == "status") {
    cmd_status();
  } else if (cmd == "validity") {
    cmd_validity();
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
  } else {
    LOGW("Unknown command: '%s'. Type 'help' for available commands.", cmd.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    delay(10);  // Wait for serial (max 3 seconds)
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
  cfg.backupMode = RV3032::BackupSwitchMode::Level;
  cfg.enableEepromWrites = false;  // RAM-only mode for frequent testing

  RV3032::Status st = g_rtc.begin(cfg);
  if (!st.ok()) {
    LOGE("RTC init failed: %s (code=%s, detail=%d)",
         st.msg, errToStr(st.code), st.detail);
    LOGE("Check I2C wiring and RTC power");
    return;
  }

  LOGI("RTC initialized successfully");
  LOGI("Driver state: %s", stateToStr(g_rtc.state()));
  LOGI("Type 'help' for available commands");
  LOGI("Type 'drv' for driver health, 'verbose 1' for detailed output");
  Serial.print("> ");
}

void loop() {
  g_rtc.tick(millis());

  const String line = read_line();
  if (line.length() > 0) {
    process_command(line);
    Serial.print("> ");
  }

  delay(10);
}
