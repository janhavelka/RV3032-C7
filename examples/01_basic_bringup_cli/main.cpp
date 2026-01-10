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

#include "examples/common/BoardPins.h"
#include "examples/common/Log.h"
#include "RV3032/Version.h"
#include "RV3032/RV3032.h"

static RV3032::RV3032 g_rtc;

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
  Serial.printf("Temperature: %.2f °C\n", celsius);
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
  LOGI("Initializing I2C (SDA=%d, SCL=%d)...", pins::I2C_SDA, pins::I2C_SCL);
  Wire.begin(pins::I2C_SDA, pins::I2C_SCL);

  // Initialize RTC
  LOGI("Initializing RTC...");
  RV3032::Config cfg;
  cfg.wire = &Wire;
  cfg.backupMode = RV3032::BackupSwitchMode::Level;
  cfg.enableEepromWrites = false;  // RAM-only mode for frequent testing

  RV3032::Status st = g_rtc.begin(cfg);
  if (!st.ok()) {
    LOGE("RTC init failed: %s (code=%d, detail=%d)",
         st.msg, static_cast<int>(st.code), st.detail);
    LOGE("Check I2C wiring and RTC power");
    return;
  }

  LOGI("RTC initialized successfully");
  LOGI("Type 'help' for available commands");
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
