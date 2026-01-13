/**
 * @file RV3032.h
 * @brief Driver for Micro Crystal RV-3032-C7 real-time clock (RTC) module
 * 
 * This library provides a comprehensive interface for the RV-3032-C7, a high-precision
 * real-time clock IC with I2C interface. The RV-3032-C7 features:
 * - Ultra-low power consumption (<1µA typical)
 * - Built-in temperature compensated crystal oscillator
 * - Battery backup with automatic switchover
 * - Alarm and periodic timer functionality
 * - External event timestamping
 * - Programmable CLKOUT output
 * - EEPROM for persistent configuration
 * - Temperature sensor for crystal compensation
 * 
 * @par Thread Safety
 * Not thread-safe. External synchronization required for multi-threaded access.
 * 
 * @par Usage Example
 * @code
 * #include "RV3032/RV3032.h"
 * 
 * RV3032::RV3032 rtc;
 * 
 * void setup() {
 *   Wire.begin();
 *   
 *   RV3032::Config cfg;
 *   cfg.wire = &Wire;
 *   
 *   RV3032::Status st = rtc.begin(cfg);
 *   if (!st.ok()) {
 *     Serial.printf("RTC init failed: %s\n", st.msg);
 *     return;
 *   }
 *   
 *   // Set time from build timestamp
 *   RV3032::DateTime now;
 *   if (RV3032::RV3032::parseBuildTime(now)) {
 *     rtc.setTime(now);
 *   }
 * }
 * 
 * void loop() {
 *   rtc.tick(millis());  // Non-blocking update
 *   
 *   RV3032::DateTime dt;
 *   if (rtc.readTime(dt).ok()) {
 *     Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
 *                   dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
 *   }
 *   delay(1000);
 * }
 * @endcode
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "Status.h"
#include "Config.h"

namespace RV3032 {

/**
 * @struct DateTime
 * @brief Date and time representation for RTC operations
 * 
 * All values are in decimal (not BCD). Year is full 4-digit value (e.g., 2026).
 */
struct DateTime {
  uint16_t year = 0;     ///< Year (full value, e.g., 2026, range: 2000-2099)
  uint8_t month = 0;     ///< Month (1-12, 1=January)
  uint8_t day = 0;       ///< Day of month (1-31)
  uint8_t hour = 0;      ///< Hour (0-23, 24-hour format)
  uint8_t minute = 0;    ///< Minute (0-59)
  uint8_t second = 0;    ///< Second (0-59)
  uint8_t weekday = 0;   ///< Day of week (0-6, 0=Sunday, auto-calculated)
};

/**
 * @struct AlarmConfig
 * @brief Configuration for RTC alarm functionality
 * 
 * Each time component can be individually enabled for matching.
 */
struct AlarmConfig {
  uint8_t minute = 0;       ///< Alarm minute (0-59)
  uint8_t hour = 0;         ///< Alarm hour (0-23)
  uint8_t date = 0;         ///< Alarm date (day of month, 1-31)
  bool matchMinute = false; ///< Enable minute matching
  bool matchHour = false;   ///< Enable hour matching
  bool matchDate = false;   ///< Enable date matching (day of month)
};

/**
 * @enum ClkoutFrequency
 * @brief Clock output frequencies available on CLKOUT pin
 */
enum class ClkoutFrequency : uint8_t {
  Hz32768 = 0,  ///< 32.768 kHz (crystal frequency)
  Hz1024 = 1,   ///< 1024 Hz
  Hz64 = 2,     ///< 64 Hz
  Hz1 = 3       ///< 1 Hz (one pulse per second)
};

/**
 * @enum TimerFrequency
 * @brief Periodic timer countdown frequencies
 */
enum class TimerFrequency : uint8_t {
  Hz4096 = 0,   ///< 4096 Hz (clock source for timer)
  Hz64 = 1,     ///< 64 Hz
  Hz1 = 2,      ///< 1 Hz (one tick per second)
  Hz1_60 = 3    ///< 1/60 Hz (one tick per minute)
};

/**
 * @enum EviDebounce
 * @brief External event input (EVI) debounce filter settings
 */
enum class EviDebounce : uint8_t {
  None = 0,   ///< No debouncing (immediate capture)
  Hz256 = 1,  ///< Debounce at 256 Hz sampling rate
  Hz64 = 2,   ///< Debounce at 64 Hz sampling rate
  Hz8 = 3     ///< Debounce at 8 Hz sampling rate (for slow mechanical switches)
};

/**
 * @struct EviConfig
 * @brief Configuration for external event input functionality
 */
struct EviConfig {
  bool rising = false;              ///< Trigger on rising edge (vs falling)
  EviDebounce debounce = EviDebounce::None;  ///< Debounce filter setting
  bool overwrite = false;           ///< Allow overwriting previous event timestamp
};

/**
 * @class RV3032
 * @brief Comprehensive driver for RV-3032-C7 real-time clock module
 * 
 * This class provides complete control over the RV-3032-C7 RTC following
 * the begin/tick/end lifecycle pattern.
 * 
 * @par Threading Model
 * Single-threaded by default. No FreeRTOS tasks created.
 * 
 * @par Timing
 * tick() completes in bounded time. When EEPROM persistence is enabled,
 * tick() advances a non-blocking EEPROM state machine (one I2C op per call).
 * 
 * @par Resource Ownership
 * I2C interface passed via Config. No hardcoded pins or resources.
 * 
 * @par Memory
 * All allocation in begin(). Zero allocation in tick() and normal operations.
 * 
 * @par Error Handling
 * All errors returned as Status. No silent failures.
 */
class RV3032 {
 public:
  /**
   * @brief Initialize RTC with configuration
   * 
   * @param config Hardware and behavior configuration
   * @return OK on success (library is ready to use), error otherwise
   * @note config.wire must be initialized (Wire.begin() called) before this.
   *       If EEPROM writes are enabled, call tick() to complete EEPROM work.
   *       Use getEepromStatus() to check if EEPROM persistence is active.
   */
  Status begin(const Config& config);

  /**
   * @brief Cooperative update (non-blocking)
   * 
   * @param now_ms Current time in milliseconds (from millis())
   * @note Advances EEPROM persistence state machine when enabled.
   */
  void tick(uint32_t now_ms);

  /**
   * @brief Stop and release resources
   * 
   * @note Currently no resources to release. Provided for API consistency.
   */
  void end();

  /**
   * @brief Check if library is initialized
   * 
   * @return true if begin() succeeded
   */
  bool isInitialized() const { return _initialized; }

  /**
   * @brief Get current configuration
   * 
   * @return Reference to active configuration
   */
  const Config& getConfig() const { return _config; }

  /**
   * @brief Check if an EEPROM persistence operation is in progress
   * 
   * @return true if EEPROM update is active
   */
  bool isEepromBusy() const;

  /**
   * @brief Get status of the last EEPROM persistence operation
   * 
   * @return IN_PROGRESS if active, OK if last op succeeded, or error status
   */
  Status getEepromStatus() const;

  // ===== Time/Date Operations =====

  /**
   * @brief Read current time and date from RTC
   * 
   * @param[out] out Structure to receive current date/time
   * @return Status::Ok() on success, error on I2C failure
   */
  Status readTime(DateTime& out);

  /**
   * @brief Set RTC time and date
   * 
   * @param time Date/time structure with values to set
   * @return Status::Ok() on success, error on validation or I2C failure
   * @note Weekday is auto-calculated from year/month/day. The time.weekday field is ignored.
   *       Validates date/time values before writing.
   */
  Status setTime(const DateTime& time);

  /**
   * @brief Read current time as Unix timestamp
   * 
   * @param[out] out Unix timestamp (seconds since Jan 1, 1970 00:00:00 UTC)
   * @return Status::Ok() on success, error otherwise
   * @note RTC does not store timezone information. Interprets time as UTC.
   */
  Status readUnix(uint32_t& out);

  /**
   * @brief Set RTC time from Unix timestamp
   * 
   * @param ts Unix timestamp (seconds since epoch)
   * @return Status::Ok() on success, error otherwise
   */
  Status setUnix(uint32_t ts);

  // ===== Alarm Operations =====

  /**
   * @brief Set alarm time values
   * 
   * @param minute Alarm minute (0-59)
   * @param hour Alarm hour (0-23)
   * @param date Alarm date (day of month, 1-31)
   * @return Status::Ok() on success, error on invalid values or I2C failure
   * @note Does not enable matching or interrupt. Use setAlarmMatch() and enableAlarmInterrupt().
   */
  Status setAlarmTime(uint8_t minute, uint8_t hour, uint8_t date);

  /**
   * @brief Configure which alarm components are matched
   * 
   * @param matchMinute Enable minute matching
   * @param matchHour Enable hour matching
   * @param matchDate Enable date matching
   * @return Status::Ok() on success, error otherwise
   */
  Status setAlarmMatch(bool matchMinute, bool matchHour, bool matchDate);

  /**
   * @brief Read complete alarm configuration
   * 
   * @param[out] out Structure to receive alarm configuration
   * @return Status::Ok() on success, error otherwise
   */
  Status getAlarmConfig(AlarmConfig& out);

  /**
   * @brief Check if alarm has triggered
   * 
   * @param[out] triggered true if alarm triggered since last clear
   * @return Status::Ok() on success, error otherwise
   * @note Flag remains set until cleared with clearAlarmFlag()
   */
  Status getAlarmFlag(bool& triggered);

  /**
   * @brief Clear alarm triggered flag
   * 
   * @return Status::Ok() on success, error otherwise
   */
  Status clearAlarmFlag();

  /**
   * @brief Enable or disable alarm interrupt output
   * 
   * @param enable true to enable INT pin output, false to disable
   * @return Status::Ok() on success, error otherwise
   */
  Status enableAlarmInterrupt(bool enable);

  /**
   * @brief Check if alarm interrupt is enabled
   * 
   * @param[out] enabled true if alarm interrupt enabled
   * @return Status::Ok() on success, error otherwise
   */
  Status getAlarmInterruptEnabled(bool& enabled);

  // ===== Timer Operations =====

  /**
   * @brief Configure periodic countdown timer
   * 
   * @param ticks Number of timer ticks before triggering (0-4095)
   * @param freq Timer clock frequency
   * @param enable Start timer immediately if true
   * @return Status::Ok() on success, error otherwise
   * @note Timer period = ticks / freq. Example: 60 ticks at Hz1 = 60 seconds
   */
  Status setTimer(uint16_t ticks, TimerFrequency freq, bool enable);

  /**
   * @brief Read current timer configuration and value
   * 
   * @param[out] ticks Current/remaining tick count
   * @param[out] freq Timer frequency setting
   * @param[out] enabled Timer enable status
   * @return Status::Ok() on success, error otherwise
   */
  Status getTimer(uint16_t& ticks, TimerFrequency& freq, bool& enabled);

  // ===== Clock Output Operations =====

  /**
   * @brief Enable or disable clock output
   * 
   * @param enabled true to enable CLKOUT pin, false to disable
   * @return OK on success, IN_PROGRESS if EEPROM persistence is queued, error otherwise
   * @note Persistent if Config::enableEepromWrites is true
   */
  Status setClkoutEnabled(bool enabled);

  /**
   * @brief Check if clock output is enabled
   * 
   * @param[out] enabled true if clock output enabled
   * @return Status::Ok() on success, error otherwise
   */
  Status getClkoutEnabled(bool& enabled);

  /**
   * @brief Set clock output frequency
   * 
   * @param freq Desired output frequency
   * @return OK on success, IN_PROGRESS if EEPROM persistence is queued, error otherwise
   * @note Persistent if Config::enableEepromWrites is true
   */
  Status setClkoutFrequency(ClkoutFrequency freq);

  /**
   * @brief Read current clock output frequency
   * 
   * @param[out] freq Current output frequency setting
   * @return Status::Ok() on success, error otherwise
   */
  Status getClkoutFrequency(ClkoutFrequency& freq);

  // ===== Calibration Operations =====

  /**
   * @brief Set frequency offset in parts-per-million
   * 
   * @param ppm Frequency offset in ppm (typical range: ±200 ppm)
   * @return OK on success, IN_PROGRESS if EEPROM persistence is queued, error otherwise
   * @note Positive values increase frequency, negative decrease it.
   *       Persistent if Config::enableEepromWrites is true.
   */
  Status setOffsetPpm(float ppm);

  /**
   * @brief Read frequency offset in parts-per-million
   * 
   * @param[out] ppm Current offset in ppm
   * @return Status::Ok() on success, error otherwise
   */
  Status getOffsetPpm(float& ppm);

  // ===== Temperature Sensor =====

  /**
   * @brief Read die temperature
   * 
   * @param[out] celsius Temperature in degrees Celsius
   * @return Status::Ok() on success, error otherwise
   * @note Typical accuracy: ±3°C. Not intended as precision thermometer.
   */
  Status readTemperatureC(float& celsius);

  // ===== External Event Input =====

  /**
   * @brief Set external event input edge sensitivity
   * 
   * @param rising true for rising edge, false for falling edge
   * @return Status::Ok() on success, error otherwise
   */
  Status setEviEdge(bool rising);

  /**
   * @brief Set external event input debounce filter
   * 
   * @param debounce Debounce filter setting
   * @return Status::Ok() on success, error otherwise
   */
  Status setEviDebounce(EviDebounce debounce);

  /**
   * @brief Enable overwriting of event timestamps
   * 
   * @param enable true to allow overwrite, false to preserve first event
   * @return Status::Ok() on success, error otherwise
   */
  Status setEviOverwrite(bool enable);

  /**
   * @brief Read complete EVI configuration
   * 
   * @param[out] out Structure to receive EVI configuration
   * @return Status::Ok() on success, error otherwise
   */
  Status getEviConfig(EviConfig& out);

  // ===== Status Operations =====

  /**
   * @brief Read RTC status register
   * 
   * @param[out] status Status register value
   * @return Status::Ok() on success, error otherwise
   */
  Status readStatus(uint8_t& status);

  /**
   * @brief Clear status register flags
   * 
   * @param mask Bit mask of flags to clear (1=clear, 0=leave unchanged)
   * @return Status::Ok() on success, error otherwise
   */
  Status clearStatus(uint8_t mask);

  // ===== Low-Level Operations =====

  /**
   * @brief Read single RTC register
   * 
   * @param reg Register address
   * @param[out] value Register value read
   * @return Status::Ok() on success, error otherwise
   * @warning Direct register access can disrupt RTC operation if misused
   */
  Status readRegister(uint8_t reg, uint8_t& value);

  /**
   * @brief Write single RTC register
   * 
   * @param reg Register address
   * @param value Value to write
   * @return Status::Ok() on success, error otherwise
   * @warning Direct register access can disrupt RTC operation if misused
   */
  Status writeRegister(uint8_t reg, uint8_t value);

  // ===== Static Utility Functions =====

  /**
   * @brief Validate date/time structure
   * 
   * @param time Date/time structure to validate
   * @return true if all values valid, false otherwise
   */
  static bool isValidDateTime(const DateTime& time);

  /**
   * @brief Compute day of week from date
   * 
   * @param year Full year (e.g., 2026)
   * @param month Month (1-12)
   * @param day Day of month (1-31)
   * @return Weekday (0-6, where 0=Sunday)
   */
  static uint8_t computeWeekday(uint16_t year, uint8_t month, uint8_t day);

  /**
   * @brief Parse compiler build date/time into DateTime
   * 
   * @param[out] out Structure to receive parsed date/time
   * @return true if parsing successful, false otherwise
   * @warning Time is in UTC of build machine. May need timezone adjustment.
   */
  static bool parseBuildTime(DateTime& out);

 private:
  Config _config;
  bool _initialized = false;

  // I2C operations
  Status readRegs(uint8_t reg, uint8_t* buf, size_t len);
  Status writeRegs(uint8_t reg, const uint8_t* buf, size_t len);
  Status writeEepromRegister(uint8_t reg, uint8_t value);
  void processEeprom(uint32_t now_ms);
  Status queueEepromUpdate(uint8_t reg, uint8_t value, uint32_t now_ms);
  Status startEepromUpdate(uint8_t reg, uint8_t value, uint32_t now_ms);
  Status readEepromBusy(bool& busy);

  // Conversion helpers
  static uint8_t bcdToBin(uint8_t v);
  static uint8_t binToBcd(uint8_t v);
  static bool isLeapYear(uint16_t year);
  static uint8_t daysInMonth(uint16_t year, uint8_t month);
  static uint32_t dateToDays(uint16_t year, uint8_t month, uint8_t day);
  static bool unixToDate(uint32_t ts, DateTime& out);

  enum class EepromState : uint8_t {
    Idle,
    ReadControl1,
    EnableEerd,
    WriteAddr,
    WriteData,
    WaitReadyPreCmd,
    WriteCmd,
    WaitReadyPostCmd,
    RestoreControl1
  };

  struct EepromWrite {
    uint8_t reg = 0;
    uint8_t value = 0;
  };

  static constexpr size_t kEepromQueueSize = 8;  // Fixed-size queue (no heap allocation)

  struct EepromOp {
    EepromState state = EepromState::Idle;
    uint8_t reg = 0;
    uint8_t value = 0;
    uint8_t control1 = 0;
    bool control1Valid = false;
    uint32_t deadlineMs = 0;
    
    // Circular buffer queue
    EepromWrite queue[kEepromQueueSize];
    uint8_t queueHead = 0;  // Next write position
    uint8_t queueTail = 0;  // Next read position
    uint8_t queueCount = 0; // Number of items in queue
  };

  EepromOp _eeprom;
  Status _eepromLastStatus = Status::Ok();
  
  bool eepromQueuePush(uint8_t reg, uint8_t value);
  bool eepromQueuePop(uint8_t& reg, uint8_t& value);
};

}  // namespace RV3032
