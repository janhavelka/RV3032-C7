/**
 * @file RV3032.h
 * @brief Driver for Micro Crystal RV-3032-C7 real-time clock (RTC) module
 * 
 * This library provides a comprehensive interface for the RV-3032-C7, a high-precision
 * real-time clock IC with I2C interface. The RV-3032-C7 features:
 * - Ultra-low power consumption (<1 uA typical)
 * - Built-in temperature compensated crystal oscillator
 * - Battery backup with automatic switchover
 * - Alarm and periodic timer functionality
 * - External event input configuration (edge/debounce/overwrite)
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
 *   RV3032::Config cfg;
 *   cfg.i2cWrite = myI2cWrite;
 *   cfg.i2cWriteRead = myI2cWriteRead;
 *   cfg.i2cUser = myI2cContext;
 *   cfg.nowMs = myNowMs;
 *   cfg.enableEepromWrites = false;
 *   
 *   RV3032::Status st = rtc.begin(cfg);
 *   if (!st.ok()) return;
 *   st = rtc.probe();  // Explicit presence check; begin() performed no I2C.
 *   if (!st.ok()) return;
 *
 *   RV3032::DateTime observed{};
 *   st = rtc.readTime(observed);  // One strict calendar burst read.
 * }
 * @endcode
 */

#pragma once

#include <stddef.h>
#include "Status.h"
#include "Config.h"
#include "Version.h"

namespace RV3032 {

/**
 * @enum DriverState
 * @brief Driver lifecycle and health state
 * 
 * Reflects current operational health of the device:
 * - UNINIT: begin() never called or end() called
 * - READY: callbacks bound and no tracked failure is outstanding
 * - DEGRADED: one or more consecutive tracked failures below the threshold
 * - OFFLINE: consecutive tracked failures reached the threshold; calls remain allowed
 */
enum class DriverState : uint8_t {
  UNINIT,    ///< Never initialized, begin() not called or end() called
  READY,     ///< Callbacks bound; no current tracked failure
  DEGRADED,  ///< Frequent errors (1 to offlineThreshold-1 consecutive failures)
  OFFLINE    ///< Observational failure threshold reached; operations remain allowed
};

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
  uint8_t weekday = 0;   ///< User-assigned weekday value (0-6)
};

/**
 * @enum TimestampSource
 * @brief Hardware timestamp source blocks.
 */
enum class TimestampSource : uint8_t {
  TLow = 0,   ///< Temperature-low event timestamp
  THigh = 1,  ///< Temperature-high event timestamp
  Evi = 2     ///< External event input timestamp
};

/**
 * @struct Timestamp
 * @brief Decoded hardware timestamp block.
 */
struct Timestamp {
  uint8_t count = 0;          ///< Raw timestamp event count
  bool timeValid = false;     ///< False for a reset/empty all-zero block.
  bool hasHundredths = false; ///< True only for EVI timestamps
  uint8_t hundredths = 0;     ///< Hundredths of a second for EVI timestamps
  DateTime time;              ///< Captured date/time, weekday computed from date
};

/**
 * @struct StatusFlags
 * @brief Decoded status register flags
 */
struct StatusFlags {
  bool tempHigh = false;     ///< THF: Temperature high flag
  bool tempLow = false;      ///< TLF: Temperature low flag
  bool update = false;       ///< UF: Periodic time update flag
  bool timer = false;        ///< TF: Periodic countdown timer flag
  bool alarm = false;        ///< AF: Alarm flag
  bool event = false;        ///< EVF: External event flag
  bool powerOnReset = false; ///< PORF: Power-on reset flag
  bool voltageLow = false;   ///< VLF: Voltage low flag
};

/**
 * @struct ValidityFlags
 * @brief Power and validity-related flags
 */
struct ValidityFlags {
  bool powerOnReset = false;   ///< PORF: Power-on reset flag
  bool voltageLow = false;     ///< VLF: Voltage low flag
  bool backupSwitched = false; ///< BSF: Backup switchover flag
  bool timeInvalid = false;    ///< True when PORF or VLF indicates invalid time
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
  uint8_t date = 0;         ///< Alarm date (1-31), or 0 for the documented inactive POR state
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
 * @brief External event input edge/level qualification settings (EVI ET bits)
 *
 * The vendor ET field always detects the selected edge. Nonzero values also
 * sample the selected high/low level at the stated frequency, providing the
 * documented debounce/level qualification.
 */
enum class EviDebounce : uint8_t {
  None = 0,   ///< Selected edge only; no level detection
  Hz256 = 1,  ///< Edge plus selected-level detection sampled at 256 Hz
  Hz64 = 2,   ///< Edge plus selected-level detection sampled at 64 Hz
  Hz8 = 3     ///< Edge plus selected-level detection sampled at 8 Hz
};

/**
 * @struct EviConfig
 * @brief Configuration for external event input functionality
 */
struct EviConfig {
  bool rising = false;              ///< Rising edge/high level vs falling edge/low level
  EviDebounce debounce = EviDebounce::None;  ///< ET edge/level qualification
  bool overwrite = false;           ///< Allow overwriting previous event timestamp
  bool synchronized = false;        ///< ESYN resets subsecond prescalers on the next event, then self-clears.
  bool clkoutStopDelay = false;      ///< CLKDE delays CLKOUT switch-off after I2C STOP
};

/** @brief Periodic time-update interrupt cadence selected by Control 1 USEL. */
enum class PeriodicUpdateFrequency : uint8_t {
  SECOND = 0, ///< Generate the periodic update event every second.
  MINUTE = 1, ///< Generate the periodic update event every minute.
};

/** @brief Trickle charger voltage mode selected by PMU TCM. */
enum class TrickleChargeMode : uint8_t {
  CHARGER_DISABLED = 0, ///< Charger disabled.
  V1_75 = 1,    ///< 1.75 V regulated mode in level switching; direct VDD charging in direct switching.
  V3_0 = 2,     ///< 3.0 V regulated mode under the vendor's level-mode/VDD conditions.
  V4_5 = 3,     ///< 4.5 V regulated mode under the vendor's level-mode/VDD conditions.
};

/** @brief Trickle charger series resistance selected by PMU TCR. */
enum class TrickleChargeResistance : uint8_t {
  OHM_600 = 0, ///< 600 ohms.
  KOHM_2 = 1,  ///< 2 kohms.
  KOHM_7 = 2,  ///< 7 kohms.
  KOHM_12 = 3, ///< 12 kohms.
};

/** @brief Complete active CLKOUT oscillator/frequency configuration. */
struct ClkoutConfig {
  bool enabled = true; ///< Direct NCLKE enable; interrupt-selected CLKOUT may still activate when false.
  bool highFrequencyMode = false; ///< True for programmable high-frequency mode.
  ClkoutFrequency xtalFrequency = ClkoutFrequency::Hz32768; ///< Crystal-derived selection.
  uint16_t highFrequencyDivider = 1; ///< Stored range 1..8192; output = value * 8192 Hz.
};

/** @brief CLKOUT delay and interrupt-controlled source selection. */
struct ClockInterruptMaskConfig {
  bool longStopDelay = false; ///< 75 ms rather than 1.4 ms after I2C STOP.
  bool interruptDelayEnabled = false; ///< Add the documented 3.9..5.9 ms delay.
  bool eventSource = false; ///< Select external-event interrupt EI.
  bool alarmSource = false; ///< Select alarm interrupt AI.
  bool timerSource = false; ///< Select countdown-timer interrupt TI.
  bool updateSource = false; ///< Select periodic-update interrupt UI.
  bool tempHighSource = false; ///< Select high-temperature interrupt THI.
  bool tempLowSource = false; ///< Select low-temperature interrupt TLI.
};

/** @brief Temperature threshold, event latch, and interrupt configuration. */
struct TemperatureEventConfig {
  int8_t lowThresholdC = 0; ///< Signed low threshold in whole degrees C.
  int8_t highThresholdC = 0; ///< Signed high threshold in whole degrees C.
  bool lowEventEnabled = false; ///< Enable low-temperature event capture.
  bool highEventEnabled = false; ///< Enable high-temperature event capture.
  bool lowInterruptEnabled = false; ///< Enable low-temperature interrupt.
  bool highInterruptEnabled = false; ///< Enable high-temperature interrupt.
  bool lowTimestampOverwrite = false; ///< Allow low timestamp overwrite.
  bool highTimestampOverwrite = false; ///< Allow high timestamp overwrite.
};

/** @brief Result from two matching calibrated temperature samples. */
struct CoherentTemperatureResult {
  int16_t raw = 0; ///< Signed 12-bit TEMP value in 1/16 degree C units.
  float celsius = 0.0f; ///< Calibrated temperature in degrees C.
};

/** @brief Result of a status-first cooperative calendar snapshot. */
struct TimeSnapshot {
  DateTime time{}; ///< Decoded time when timeValid is true.
  StatusFlags statusFlags{}; ///< Typed flags decoded from the same Status callback.
  uint8_t statusRaw = 0; ///< Status byte observed before calendar access.
  bool statusValid = false; ///< True after the Status read succeeded.
  bool timeValid = false; ///< False when PORF/VLF prevented calendar access.
};

/** @brief Evidence from the verified calendar-set and invalid-flag-clear job. */
struct VerifiedTimeSetReport {
  DateTime requested{}; ///< Requested calendar, including its user-assigned weekday.
  DateTime verified{}; ///< Last accepted calendar readback.
  Status calendarWriteStatus = Status::Ok(); ///< Exact callback result; meaningful only when calendarWriteAttempted is true.
  Status statusWriteStatus = Status::Ok(); ///< Exact callback result; meaningful only when statusWriteAttempted is true.
  uint8_t statusBefore = 0; ///< Status observed before any mutation.
  uint8_t statusBeforeClear = 0; ///< Fresh pre-clear Status retained as side-effect evidence.
  uint8_t statusAfter = 0; ///< Status read back after the W0C write.
  bool calendarWriteAttempted = false; ///< True after the sole calendar write callback was invoked.
  bool statusWriteAttempted = false; ///< True after the sole Status write callback was invoked.
  bool calendarWriteAmbiguous = false; ///< Calendar write callback failed and may have committed.
  bool statusWriteAmbiguous = false; ///< Status write callback failed and may have committed.
  bool verifiedAfterAmbiguousWrite = false; ///< Readback proved at least one ambiguous write committed as intended.
  bool verifiedValid = false; ///< `verified` contains an accepted calendar readback.
  bool statusBeforeValid = false; ///< `statusBefore` was read successfully.
  bool statusBeforeClearValid = false; ///< `statusBeforeClear` was read successfully.
  bool statusAfterValid = false; ///< `statusAfter` was read successfully.
  bool temperatureHighWasSetBeforeClear = false; ///< THF was set immediately before its unavoidable clear.
  bool temperatureLowWasSetBeforeClear = false; ///< TLF was set immediately before its unavoidable clear.
};

/** @brief Typed address selector for persistent configuration inspection. */
enum class ConfigurationEepromRegister : uint8_t {
  PMU = 0xC0, ///< Persistent PMU byte.
  OFFSET = 0xC1, ///< Persistent frequency-offset byte.
  CLKOUT1 = 0xC2, ///< Persistent CLKOUT divider low byte.
  CLKOUT2 = 0xC3, ///< Persistent CLKOUT mode/divider high byte.
  TEMPERATURE_REFERENCE0 = 0xC4, ///< Persistent temperature reference low byte.
  TEMPERATURE_REFERENCE1 = 0xC5, ///< Persistent temperature reference high byte.
};

static constexpr uint8_t USER_EEPROM_SIZE = 32; ///< Persistent user EEPROM size.
static constexpr uint8_t USER_EEPROM_JOB_MAX_BYTES = 16; ///< Per-job byte bound.
static constexpr uint32_t READ_TIME_OPERATION_TIMEOUT_MS = 100; ///< Default snapshot deadline.
static constexpr uint32_t SET_TIME_OPERATION_TIMEOUT_MS = 250; ///< Default verified-set deadline.
static constexpr uint32_t MIN_SET_TIME_OPERATION_BUDGET_MS = 125; ///< Minimum accepted set deadline.

/** @brief Typed result from persistent configuration or user EEPROM reads. */
struct PersistentReadResult {
  uint8_t eepromAddress = 0; ///< First indirect EEPROM address read.
  uint8_t data[USER_EEPROM_JOB_MAX_BYTES] = {}; ///< Durably proven bytes.
  uint8_t length = 0; ///< Number of result bytes.
  bool persistentVerified = false; ///< True when every requested byte was proven.
  bool cleanupVerified = false; ///< True when access-state cleanup was proven.
};

/** @brief Partial-progress and cleanup evidence for a user EEPROM write job. */
struct UserEepromWriteReport {
  uint8_t offset = 0; ///< User EEPROM offset requested.
  uint8_t requestedLength = 0; ///< Number of bytes requested.
  uint8_t completedBytes = 0; ///< Bytes compared or written successfully.
  uint8_t durablyVerifiedBytes = 0; ///< Bytes proven through READ_ONE.
  bool cleanupVerified = false; ///< True when access-state cleanup was proven.
};

/** @brief Four-byte credential accepted only by typed password operations. */
struct PasswordCredential {
  uint8_t bytes[4] = {}; ///< Sensitive credential bytes; never returned by status APIs.
};

/** @brief Redacted evidence level for password-protection state. */
enum class PasswordProtectionEvidence : uint8_t {
  UNKNOWN = 0, ///< No durable or authentication evidence is available.
  VERIFIED_DISABLED = 1, ///< Persistent disabled byte was proven.
  VERIFIED_ENABLED = 2, ///< Persistent password and enabled byte were proven.
  AUTHENTICATED = 3, ///< A protected operation proved credential acceptance.
};

/** @brief Credential-free password-protection evidence. */
struct PasswordProtectionStatus {
  PasswordProtectionEvidence evidence = PasswordProtectionEvidence::UNKNOWN; ///< Evidence level.
  bool credentialAccepted = false; ///< True only when acceptance was proven.
  bool persistentVerified = false; ///< True after direct persistent proof.
  bool cleanupVerified = false; ///< True after access-state cleanup proof.
};

/** @brief Terminal semantic outcome of primary-cell provisioning. */
enum class PrimaryCellConfigurationOutcome : uint8_t {
  NOT_ATTEMPTED = 0,
  ALREADY_CONFIGURED = 1,
  EEPROM_UPDATED = 2,
  FAILED = 3,
};

/** @brief Operation phase responsible for a primary-cell ensure failure. */
enum class PrimaryCellFailureStage : uint8_t {
  NONE = 0,
  PRECONDITION = 1,
  PREPARE_ACCESS = 2,
  READ_PERSISTENT = 3,
  WRITE_PERSISTENT = 4,
  VERIFY_PERSISTENT = 5,
  CLEANUP = 6,
  SETTLE = 7,
};

/** @brief Detailed operation and cleanup evidence from primary-cell ensure. */
struct PrimaryCellConfigurationReport {
  PrimaryCellConfigurationOutcome outcome =
      PrimaryCellConfigurationOutcome::NOT_ATTEMPTED;
  PrimaryCellFailureStage failureStage = PrimaryCellFailureStage::NONE;
  Status operationStatus = Status::Ok();
  Status cleanupStatus = Status::Ok();
  uint8_t persistentBefore = 0;
  uint8_t persistentTarget = 0;
  uint8_t persistentAfter = 0;
  uint8_t activeAfter = 0;
  uint8_t control1After = 0;
  bool persistentBeforeValid = false;
  bool persistentAfterValid = false;
  bool persistentTargetVerified = false; ///< Direct EEPROM read proved the exact derived primary-cell target.
  bool activeTargetVerified = false; ///< Active-C0 readback proved the exact derived persistent target.
  bool writeCommandAttempted = false;
  bool writeDurablyVerified = false;
  bool cleanupVerified = false;
  bool autoRefreshHeldDisabledForSafety = false;
};

/**
 * @struct SettingsSnapshot
 * @brief Cached configuration and runtime state that can be read without I2C.
 */
struct SettingsSnapshot {
  bool initialized = false;                   ///< True after begin() succeeds
  DriverState state = DriverState::UNINIT;     ///< Current driver state
  uint8_t i2cAddress = 0x51;                   ///< Active 7-bit address
  uint32_t i2cTimeoutMs = 0;                   ///< Active I2C timeout
  uint8_t offlineThreshold = 0;                ///< Failure threshold for OFFLINE
  bool hasNowMsHook = false;                   ///< True when Config::nowMs is set
  bool hasWaitMsHook = false;                  ///< True when Config::waitMs is set

  bool beginInProgress = false;                ///< Reserved; passive begin is never in progress
  bool enableEepromWrites = false;             ///< Persistent EEPROM writes enabled
  uint32_t eepromTimeoutMs = 0;                ///< EEPROM write timeout
  bool eepromBusy = false;                     ///< True while EEPROM state machine is active
  Status eepromLastStatus = Status::Ok();      ///< Last EEPROM state-machine status
  uint32_t eepromWriteCount = 0;               ///< Successful generic queue items since begin()
  uint32_t eepromWriteFailures = 0;            ///< Failed generic queue items since begin()
  uint8_t eepromQueueDepth = 0;                ///< Pending EEPROM queue depth
  bool jobBusy = false;                        ///< Cooperative job currently active
  bool primaryCellEnsureAttempted = false;     ///< Ensure latch for this lifecycle
  uint32_t lastOkMs = 0;                       ///< Timestamp of last successful operation
  uint32_t lastErrorMs = 0;                    ///< Timestamp of last failed operation
  Status lastError = Status::Ok();             ///< Most recent operation failure
  uint8_t consecutiveFailures = 0;             ///< Consecutive I2C failures
  uint32_t totalFailures = 0;                  ///< Lifetime failure count
  uint32_t totalSuccess = 0;                   ///< Lifetime success count
};

/** @brief Hardware EEPROM support flags read from TEMP_LSB. */
struct EepromHardwareFlags {
  bool busy = false;         ///< EEbusy: a command is still executing.
  bool writeFailed = false;  ///< Sticky EEF: a prior EEPROM write failed.
};

/**
 * @class RV3032
 * @brief Comprehensive driver for RV-3032-C7 real-time clock module
 * 
 * This class provides complete control over the RV-3032-C7 RTC following
 * the begin/tick/end lifecycle pattern.
 * 
 * @par Threading Model
 * Not thread-safe. The application must serialize both this instance and the
 * shared device. The driver creates no task or lock.
 *
 * @par Persistence-producing setter admission
 * When generic persistence is enabled and only queued entries are pending, a
 * supported active-configuration setter may admit its cooperative job. Exact
 * queue capacity is checked after read/compute and before active mutation, so
 * pollJob() can return QUEUE_FULL without writing the active mirror. Exact
 * duplicates coalesce only against the newest pending value for that address.
 * The EEPROM engine never advances concurrently; unrelated jobs return BUSY.
 * 
 * @par Timing
 * tick() advances at most one bounded transport instruction in the generic
 * EEPROM state machine. Its wall time is therefore bounded by the configured
 * per-transfer timeout and the application transport contract.
 * 
 * @par Resource Ownership
 * I2C transport passed via Config. No hardcoded pins or resources.
 * 
 * @par Memory
 * The driver uses only fixed-capacity storage and performs no heap allocation.
 * 
 * @par Error Handling
 * All errors returned as Status. No silent failures.
 */
class RV3032 {
 public:
  /**
   * @brief Passively bind and validate configuration
   * 
   * @param config Hardware and behavior configuration
   * @return OK when callbacks are bound; BUSY on a second lifecycle begin
   * @note Performs zero transport/wait callbacks and does not prove presence,
   *       apply PMU policy, clear flags, or queue persistence. Call probe()
   *       explicitly when presence evidence is required.
   */
  Status begin(const Config& config);

  /**
   * @brief Advance generic EEPROM persistence by at most one instruction
   * 
   * @param now_ms Current monotonic time in milliseconds
   * @note Performs no work before begin(). It never advances an ordinary job,
   *       probes or recovers the device, or invokes primary-cell provisioning.
   */
  void tick(uint32_t now_ms);

  /**
   * @brief End an idle lifecycle with zero callbacks
   * 
   * @note Idempotent while uninitialized. If queued/active EEPROM or budgeted
   *       job work exists, this call preserves the initialized lifecycle and
   *       operation state. Once idle, it clears callbacks, health, fixed work
   *       state, and the primary-cell attempt latch.
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

  // ===== Driver State and Health =====

  /**
   * @brief Probe RTC device presence and identity
   * 
   * Performs exactly one raw Status-register read.
   * Requires begin() so I2C callbacks are configured.
   * Does NOT modify configuration or initialize the driver.
   * Does NOT update health tracking or driver state.
   * Can be called anytime after begin().
   * 
   * @return OK if device present, DEVICE_NOT_FOUND on address NACK,
   *         or the original timeout/data-NACK/bus error on other failures.
   */
  Status probe();

  /**
   * @brief Perform one tracked read-only health re-probe
   * @note Does not recover the physical bus, reapply configuration, provision
   *       primary-cell policy, start persistence, or reset the ensure latch.
   */
  Status recover();

  /**
   * @brief Get current driver state
   * @return UNINIT, READY, DEGRADED, or OFFLINE
   */
  DriverState state() const { return _driverState; }

  /**
   * @brief Alias for state() for cross-driver diagnostics
   * @return UNINIT, READY, DEGRADED, or OFFLINE
   */
  DriverState driverState() const { return state(); }

  /**
   * @brief Check if device is operational
   * @return true if READY or DEGRADED, false if UNINIT or OFFLINE
   */
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }

  /**
   * @brief Get cached configuration and runtime state without performing I2C.
   * @param[out] out Snapshot of the current cached state.
   * @return Status::Ok() always.
   */
  Status getSettings(SettingsSnapshot& out) const;

  /**
   * @brief Get cached configuration and runtime state without performing I2C.
   * @return Snapshot of the current cached state.
   */
  SettingsSnapshot getSettings() const {
    SettingsSnapshot out;
    (void)getSettings(out);
    return out;
  }

  /**
   * @brief Get timestamp of last successful operation
   * @return Milliseconds timestamp from driver timebase
   */
  uint32_t lastOkMs() const { return _lastOkMs; }

  /**
   * @brief Get most recent error status
   * @return Status with error code, detail, and message
   */
  Status lastError() const { return _lastError; }

  /**
   * @brief Get consecutive failure count
   * @return Number of consecutive failures since last success
   */
  uint8_t consecutiveFailures() const { return _consecutiveFailures; }

  /**
   * @brief Get total failure count
   * @return Total failures since begin() (wraps at UINT32_MAX)
   */
  uint32_t totalFailures() const { return _totalFailures; }

  /**
   * @brief Get total success count
   * @return Total successes since begin() (wraps at UINT32_MAX)
   */
  uint32_t totalSuccess() const { return _totalSuccess; }

  /**
   * @brief Get timestamp of last error
   * @return Milliseconds timestamp from driver timebase, 0 if no error yet
   */
  uint32_t lastErrorMs() const { return _lastErrorMs; }

  // ===== EEPROM Status =====

  /**
   * @brief Check whether generic configuration persistence is active or queued.
   * 
   * @return true if the generic EEPROM engine is active or queue entries are pending
   * @note Explicit configuration/user EEPROM jobs use isJobBusy() and pollJob().
   */
  bool isEepromBusy() const;

  /**
   * @brief Get status of the current or last generic persistence batch.
   * 
   * @return IN_PROGRESS while generic work is active; otherwise the first
   *         item error retained for the batch, or OK when all items succeeded.
   * @note An ordinary item failure is returned at that item boundary and later
   *       queued items remain available for a subsequent poll. Starting a new
   *       batch after the queue becomes idle clears the prior batch status.
   */
  Status getEepromStatus() const;

  /**
   * @brief Get count of successful generic configuration-persistence items.
   * @return Completed queue items, including compare-equal no-write outcomes.
   * @note Does not count explicit user-EEPROM/password jobs or primary ensure.
   */
  uint32_t eepromWriteCount() const { return _eepromWriteCount; }

  /**
   * @brief Get count of failed generic configuration-persistence items.
   * @return Failed generic queue items.
   * @note Does not count explicit user-EEPROM/password jobs or primary ensure.
   */
  uint32_t eepromWriteFailures() const { return _eepromWriteFailures; }

  /**
   * @brief Get current generic configuration-persistence queue depth.
   * @return Number of pending queue items, excluding an active item
   */
  uint8_t eepromQueueDepth() const { return _eeprom.queueCount; }

  /**
   * @brief Advance generic configuration persistence with an instruction budget.
   *
   * @param now_ms Current monotonic time in milliseconds
   * @param maxInstructions Maximum backend I2C instructions to execute
   * @param[out] instructionsUsed Number of I2C instructions attempted
   * @return OK when idle or the active update completed, IN_PROGRESS when work remains,
   *         or an error from the EEPROM state machine.
   * @note The method may use the full supplied budget. tick(now_ms) delegates
   *       with a budget of one instruction. A zero budget performs no I2C and
   *       returns the current progress or terminal status.
   * @note An ordinary item failure is returned immediately, retains the
   *       remaining queue, and remains the batch status while later items are
   *       advanced by subsequent calls.
   * @warning A terminal EEPROM_CLEANUP_FAILED cancels remaining queued items;
   *          no later item is allowed to run against unverified C0/C1 or
   *          password-authorization state. Re-admission is an application
   *          recovery-policy decision.
   */
  Status pollEeprom(uint32_t now_ms, uint8_t maxInstructions, uint8_t& instructionsUsed);

  // ===== Budgeted Job Operations =====

  /**
   * @brief Check whether the shared budgeted job storage is active.
   *
   * @return true while an ordinary job or EEPROM-owned persistent job is active
   * @note Continue using the polling surface that admitted the work:
   *       pollJob() for ordinary jobs and pollEeprom()/tick() for queued
   *       generic persistence.
   */
  bool isJobBusy() const;

  /**
   * @brief Get the current or last shared job status.
   *
   * @return IN_PROGRESS while a job is active, otherwise the last terminal job status.
   */
  Status getJobStatus() const;

  /**
   * @brief Start a budgeted periodic countdown timer configuration job.
   *
   * @param ticks Timer preset (1-4095). The vendor defines 0 as a non-running
   *              value rather than a countdown preset.
   * @param freq Timer clock frequency
   * @param enable Start timer immediately if true
   * @return IN_PROGRESS if the job was accepted, BUSY if another job is active,
   *         INVALID_PARAM for invalid values, or NOT_INITIALIZED before begin().
   * @note Timer High writes bits 3:0 and writes reserved bits 7:4 as zero.
   *       One register read or write is one instruction.
   */
  Status startSetTimerJob(uint16_t ticks, TimerFrequency freq, bool enable);

  /**
   * @brief Start a budgeted volatile user-RAM read-modify-write job.
   *
   * @param reg Direct user-RAM address in `0x40..0x4F`
   * @param clearMask Bits to clear in the read value
   * @param setMask Bits to set after clearMask is applied
   * @return IN_PROGRESS if the job was accepted, BUSY if another job is active,
   *         INVALID_PARAM outside user RAM, or NOT_INITIALIZED before begin().
   * @note Computes `(current & ~clearMask) | setMask`, then writes the result through
   *       pollJob(). Status, control, calendar, password, EEPROM-command, and
   *       reserved registers are deliberately not accepted by this compatibility API.
   */
  Status startRegisterUpdateJob(uint8_t reg, uint8_t clearMask, uint8_t setMask);

  /**
   * @brief Start a budgeted volatile user RAM write job.
   *
   * @param offset Offset inside user RAM (0-15)
   * @param buf Source buffer copied into the driver's fixed job buffer
   * @param len Number of bytes to write
   * @return IN_PROGRESS if the job was accepted, BUSY if another job is active,
   *         INVALID_PARAM for invalid ranges, or NOT_INITIALIZED before begin().
   * @note Writes larger than the single-transfer limit are chunked by pollJob().
   */
  Status startWriteUserRamJob(uint8_t offset, const uint8_t* buf, size_t len);

  /**
   * @brief Advance the active explicit job with an instruction budget.
   *
   * @param now_ms Current monotonic time in milliseconds
   * @param maxInstructions Maximum backend I2C instructions to execute
   * @param[out] instructionsUsed Number of I2C instructions attempted
   * @return OK when no job remains, IN_PROGRESS when the budget was exhausted before
   *         completion, or the terminal job error. A hard deadline never permits
   *         another callback; if persistent cleanup was still required, the terminal
   *         status is EEPROM_CLEANUP_FAILED rather than an unqualified timeout.
   * @note A zero budget performs no I2C and returns the current progress or
   *       terminal status.
   */
  Status pollJob(uint32_t now_ms, uint8_t maxInstructions, uint8_t& instructionsUsed);

  /**
   * @brief Start a status-first calendar snapshot job.
   *
   * Admission performs zero I2C and stores a wrap-safe operation deadline.
   * Each pollJob() instruction invokes at most one transport callback. Typed
   * PORF or VLF evidence from that Status read short-circuits the job with
   * `timeValid=false`.
   *
   * @param nowMs Current application monotonic time.
   * @param operationTimeoutMs Whole-operation timeout in `1..1000` ms.
   * @return IN_PROGRESS when admitted, or a zero-I/O validation/admission error.
   */
  Status startReadTimeSnapshotJob(
      uint32_t nowMs,
      uint32_t operationTimeoutMs = READ_TIME_OPERATION_TIMEOUT_MS);
  /**
   * @brief Copy the completed status-first calendar snapshot result.
   * @return IN_PROGRESS while this job is active, JOB_RESULT_UNAVAILABLE before
   *         a matching completion or after another job, otherwise the exact
   *         terminal status of the matching job.
   * @note On IN_PROGRESS or JOB_RESULT_UNAVAILABLE, `out` is unchanged. Results
   *       remain available until another job is admitted.
   */
  Status getReadTimeSnapshotJobResult(TimeSnapshot& out) const;

  /**
   * @brief Start a verified calendar set followed by the named PORF/VLF clear.
   *
   * The caller's user-assigned weekday is range-validated and written exactly.
   * Admission performs zero I2C. Calendar and Status writes are each attempted
   * at most once and ambiguous callback failures are reconciled by later
   * readback. The Status payload is the fixed value 0xFC, preserving
   * UF/TF/AF/EVF even if they become set between the pre-clear read and write.
   * Clearing PORF/VLF necessarily also clears THF/TLF; the completed report
   * records their fresh pre-clear state.
   *
   * @param value Requested calendar value; `weekday` must be in 0..6.
   * @param nowMs Current application monotonic time.
   * @param operationTimeoutMs Whole-operation timeout in `126..1000` ms.
   * @return IN_PROGRESS when admitted, or a zero-I/O validation/admission error.
   */
  Status startSetTimeAndClearInvalidFlagsVerifiedJob(
      const DateTime& value,
      uint32_t nowMs,
      uint32_t operationTimeoutMs = SET_TIME_OPERATION_TIMEOUT_MS);
  /**
   * @brief Copy the completed verified calendar-set result and mutation evidence.
   * @return IN_PROGRESS while this job is active, JOB_RESULT_UNAVAILABLE before
   *         a matching completion or after another job, otherwise the exact
   *         terminal status of the matching job.
   * @note On IN_PROGRESS or JOB_RESULT_UNAVAILABLE, `out` is unchanged. A
   *       terminal failure after mutation still copies the complete report.
   */
  Status getSetTimeAndClearInvalidFlagsVerifiedJobResult(
      VerifiedTimeSetReport& out) const;

  /**
   * @brief Start an explicit, directly verified configuration EEPROM read.
   * @param reg Typed C0..C5 configuration EEPROM selector. Password bytes are
   *            deliberately excluded and require the typed protection flow.
   * @param nowMs Current monotonic time.
   * @param operationTimeoutMs Whole-operation timeout in `50..10000` ms.
   * @return IN_PROGRESS when admitted, or a zero-I/O validation/admission error.
   * @note The start call performs zero I2C. Reads remain available when generic
   *       writes are disabled, but direct access temporarily changes EERD and
   *       active C0 and succeeds only after both are restored and verified.
   */
  Status startReadConfigurationEepromJob(
      ConfigurationEepromRegister reg,
      uint32_t nowMs,
      uint32_t operationTimeoutMs = 250);
  /**
   * @brief Start an indirect, directly verified user EEPROM read job.
   * @param offset Public user EEPROM offset in `0..31`.
   * @param length Byte count in `1..16`; the range must not cross offset 31.
   * @param nowMs Current monotonic time.
   * @param operationTimeoutMs Whole-operation timeout in `50..10000` ms.
   * @return IN_PROGRESS when admitted, or a zero-I/O validation/admission error.
   * @note The start call performs zero I2C. Each reported byte is proven by an
   *       adaptive two-READ_ONE sequence; cleanup is also verified.
   */
  Status startReadUserEepromJob(
      uint8_t offset,
      uint8_t length,
      uint32_t nowMs,
      uint32_t operationTimeoutMs = 1000);
  /**
   * @brief Start a compare-before-write, directly verified user EEPROM write.
   * @param offset Public user EEPROM offset in `0..31`.
   * @param data Source bytes; copied into fixed driver storage on admission.
   * @param length Byte count in `1..16`; the range must not cross offset 31.
   * @param nowMs Current monotonic time.
   * @param operationTimeoutMs Whole-operation timeout in `301..10000` ms.
   * @return IN_PROGRESS when admitted, or a zero-I/O validation/admission error.
   * @note Requires Config::enableEepromWrites. Every changed byte permits at
   *       most one WRITE_ONE command and requires direct persistent readback.
   *       A failure stops before later bytes and the report retains verified
   *       partial progress.
   */
  Status startWriteUserEepromJob(
      uint8_t offset,
      const uint8_t* data,
      uint8_t length,
      uint32_t nowMs,
      uint32_t operationTimeoutMs = 4000);
  /**
   * @brief Copy the completed persistent-read result without consuming it.
   * @return IN_PROGRESS while the matching job is active,
   *         JOB_RESULT_UNAVAILABLE before a matching completion or after a
   *         different job, otherwise the exact terminal job status.
   * @note Performs zero I2C. On IN_PROGRESS or JOB_RESULT_UNAVAILABLE, `out`
   *       is unchanged. `length` counts only bytes positively proven.
   */
  Status getPersistentReadJobResult(PersistentReadResult& out) const;
  /**
   * @brief Copy the completed user EEPROM write report without consuming it.
   * @return IN_PROGRESS while the matching job is active,
   *         JOB_RESULT_UNAVAILABLE before a matching completion or after a
   *         different job, otherwise the exact terminal job status.
   * @note Performs zero I2C and leaves `out` unchanged when unavailable.
   */
  Status getUserEepromWriteJobResult(UserEepromWriteReport& out) const;

  // ===== Time/Date Operations =====

  /**
   * @brief Read current time and date in one strict calendar burst.
   * 
   * @param[out] out Structure to receive current date/time
   * @return OK on success, INVALID_DATETIME on invalid BCD, error on I2C failure
   * @note This helper does not read Status or clear flags. Use
   *       startReadTimeSnapshotJob() when validity flags must be checked first.
   */
  Status readTime(DateTime& out);

  /**
   * @brief Read the current hundredths-of-a-second counter.
   * @param[out] hundredths Strictly decoded BCD value in the range 0..99.
   * @return OK on success, INVALID_DATETIME on malformed BCD, or transport error.
   * @note This is one independent read. Use it only when the application can
   *       tolerate rollover relative to a separate readTime() call.
   */
  Status readHundredths(uint8_t& hundredths);

  /**
   * @brief Set RTC time and date in one calendar burst.
   * 
   * @param time Date/time structure with values to set
   * @return Status::Ok() on success, error on validation or I2C failure
   * @note Weekday is user-assigned: values 0..6 are accepted without requiring
   *       agreement with the date and are written unchanged. Writing Seconds resets the
   *       chip's hundredths counter and the 4096 Hz through 1 Hz prescalers.
   *       This helper does not verify readback or clear Status flags. Use
   *       startSetTimeAndClearInvalidFlagsVerifiedJob() for that contract.
   *       The calendar mutation is active-only and does not use EEPROM.
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
   * @note Active-only calendar mutation; no EEPROM is involved.
   */
  Status setUnix(uint32_t ts);

  // ===== Alarm Operations =====

  /**
   * @brief Set alarm time values
   * 
   * @param minute Alarm minute (0-59)
   * @param hour Alarm hour (0-23)
   * @param date Alarm date (day of month, 1-31), or 0 to restore the vendor
   *             deactivated comparator state (Date Alarm 00 with AE_D=0).
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Active-only; no EEPROM is involved. Does not enable matching or
   *       interrupt. Use setAlarmMatch() and enableAlarmInterrupt().
   */
  Status setAlarmTime(uint8_t minute, uint8_t hour, uint8_t date);

  /**
   * @brief Configure which alarm components are matched
   * 
   * @param matchMinute Enable minute matching
   * @param matchHour Enable hour matching
   * @param matchDate Enable date matching
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Active-only; no EEPROM is involved.
   * @warning Passing false for all three fields sets every AE bit. Per the
   *          vendor truth table this generates an alarm event every minute; it
   *          does not deactivate the comparator. Use setAlarmTime(..., ..., 0)
   *          for the deactivated AF-never-sets state.
   */
  Status setAlarmMatch(bool matchMinute, bool matchHour, bool matchDate);

  /**
   * @brief Read complete alarm configuration
   * 
   * @param[out] out Structure to receive alarm configuration
   * @return Status::Ok() on success, error otherwise.
   */
  Status getAlarmConfig(AlarmConfig& out);

  /**
   * @brief Check if alarm has triggered
   * 
   * @param[out] triggered true if alarm triggered since last clear
   * @return Status::Ok() on success, error otherwise.
   * @note Flag remains set until cleared with clearAlarmFlag()
   */
  Status getAlarmFlag(bool& triggered);

  /**
   * @brief Clear alarm triggered flag
   * 
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Active-only Status W0C operation.
   */
  Status clearAlarmFlag();

  /**
   * @brief Enable or disable alarm interrupt output
   * 
   * @param enable true to enable INT pin output, false to disable
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Active-only Control 2 mutation.
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
   * @param ticks Timer preset (1-4095). The vendor defines 0 as a non-running
   *              value rather than a countdown preset.
   * @param freq Timer clock frequency
   * @param enable Start timer immediately if true
   * @return IN_PROGRESS when admitted; INVALID_PARAM for invalid input.
   *         pollJob() returns the terminal transport result.
   * @note Timer period = ticks / freq. Example: 60 ticks at Hz1 = 60 seconds
   *       Timer configuration is active-only.
   */
  Status setTimer(uint16_t ticks, TimerFrequency freq, bool enable);

  /**
   * @brief Read the configured timer preset, frequency, and enable state
   * 
   * @param[out] ticks Configured timer preset; hardware does not expose the
   *                   remaining countdown value
   * @param[out] freq Timer frequency setting
   * @param[out] enabled Timer enable status
   * @return Status::Ok() on success, error otherwise
   */
  Status getTimer(uint16_t& ticks, TimerFrequency& freq, bool& enabled);

  /** @brief Cooperatively set the active-only timer-interrupt enable bit. */
  Status setTimerInterruptEnabled(bool enabled);
  /** @brief Read the timer-interrupt enable bit in one transfer. */
  Status getTimerInterruptEnabled(bool& enabled);
  /** @brief Read the timer flag without clearing it. */
  Status getTimerFlag(bool& triggered);
  /** @brief Cooperatively clear TF, subject to Status side-effect guards; active-only. */
  Status clearTimerFlag();

  /** @brief Cooperatively configure active-only periodic update cadence and interrupt. */
  Status setPeriodicUpdate(PeriodicUpdateFrequency frequency, bool interruptEnabled);
  /** @brief Read periodic update cadence and interrupt state in one transfer. */
  Status getPeriodicUpdate(PeriodicUpdateFrequency& frequency, bool& interruptEnabled);
  /** @brief Read UF without clearing it. */
  Status getPeriodicUpdateFlag(bool& triggered);
  /** @brief Cooperatively clear UF, subject to Status side-effect guards; active-only. */
  Status clearPeriodicUpdateFlag();

  // ===== Power Management / Backup Operations =====

  /**
   * @brief Set backup switchover mode.
   *
   * @param mode Off, Level, or Direct switchover mode
   * @return IN_PROGRESS when the active update is admitted. pollJob() returns
   *         its terminal result and may then queue generic persistence.
   * @note Persistent if Config::enableEepromWrites is true. This preserves
   *       existing trickle-charger and CLKOUT PMU bits.
   * @warning When EEPROM persistence is enabled, this schedules a wear-limited,
   *          deadline-driven EEPROM commit. Call tick() or pollEeprom() until
   *          getEepromStatus() reports a terminal status.
   */
  Status setBackupSwitchMode(BackupSwitchMode mode);

  /**
   * @brief Read backup switchover mode from the PMU register.
   *
   * @param[out] mode Decoded backup mode. Both disabled PMU encodings map to Off.
   * @return Status::Ok() on success, error otherwise
   */
  Status getBackupSwitchMode(BackupSwitchMode& mode);

  /**
   * @brief Cooperatively set PMU trickle-charge voltage mode.
   * @note The electrical meaning depends on BSM and supply topology. When
   *       Config::enableEepromWrites is true, this queues a wear-limited C0
   *       update that must be advanced through tick()/pollEeprom().
   */
  Status setTrickleChargeMode(TrickleChargeMode mode);
  /** @brief Read active PMU trickle-charge voltage mode. */
  Status getTrickleChargeMode(TrickleChargeMode& mode);
  /**
   * @brief Cooperatively set PMU trickle-charge resistance.
   * @note TCR is stored independently even while charging is disabled. When
   *       Config::enableEepromWrites is true, this queues a wear-limited C0
   *       update that must be advanced through tick()/pollEeprom().
   */
  Status setTrickleChargeResistance(TrickleChargeResistance resistance);
  /** @brief Read active PMU trickle-charge resistance. */
  Status getTrickleChargeResistance(TrickleChargeResistance& resistance);
  /** @brief Cooperatively set active-only backup-switch interrupt enable. */
  Status setBackupSwitchInterruptEnabled(bool enabled);
  /** @brief Read backup-switch interrupt enable. */
  Status getBackupSwitchInterruptEnabled(bool& enabled);

  /// Sole synchronous multi-callback API. Requires nowMs/waitMs and may be
  /// invoked at most once per successful begin/end lifecycle. It is independent
  /// of enableEepromWrites, returns a terminal status (never IN_PROGRESS), and
  /// can dispatch at most one EEPROM WRITE_ONE command.
  /// @note Rejected preconditions perform zero callbacks, leave report unchanged,
  ///       and do not consume the lifecycle attempt. operationStatus preserves
  ///       the forward-path result while cleanupStatus reports safety cleanup.
  ///       persistentTargetVerified and activeTargetVerified are set at their
  ///       direct-readback proof points and remain set if a later cleanup or
  ///       settle step fails. A safe BSM00/TCM00 hold is not active-target proof.
  /// @warning The application transport owner must place both i2cWrite and
  ///          i2cWriteRead in scoped single-physical-attempt mode for the whole
  ///          call. No ensure read or write may recover or retry internally.
  /// @warning The caller must prove stable VDD >=1.6 V for EEPROM writing,
  ///          VDD >=2.0 V when using 400 kHz, and VDD safely above the maximum
  ///          2.2 V LSM threshold through activation and the measured 10 ms
  ///          settle, plus the board's backup/backfeed conditions. A failed
  ///          safety hold is volatile and does not survive POR; the module is
  ///          not deployment-ready until an explicit ensure in a later complete
  ///          lifecycle succeeds.
  Status ensurePrimaryCellConfiguration(
      PrimaryCellConfigurationReport& report);

  // ===== Clock Output Operations =====

  /**
   * @brief Enable or disable direct clock output through PMU.NCLKE
   * 
   * @param enabled true to directly enable CLKOUT, false to disable direct
   *                output. Interrupt-selected CLKOUT can still activate while
   *                direct output is disabled.
   * @return IN_PROGRESS when the active update is admitted. pollJob() returns
   *         its terminal result and may then queue generic persistence.
   * @note Persistent if Config::enableEepromWrites is true
   * @warning When EEPROM persistence is enabled, this schedules a wear-limited,
   *          deadline-driven EEPROM commit. Call tick() or pollEeprom() until
   *          getEepromStatus() reports a terminal status.
   */
  Status setClkoutEnabled(bool enabled);

  /**
   * @brief Check whether direct CLKOUT is enabled through PMU.NCLKE
   * 
   * @param[out] enabled true if direct CLKOUT is enabled. This does not report
   *                     interrupt-selected output activity.
   * @return Status::Ok() on success, error otherwise
   */
  Status getClkoutEnabled(bool& enabled);

  /**
   * @brief Set clock output frequency
   * 
   * @param freq Desired output frequency
   * @return IN_PROGRESS when the active update is admitted. pollJob() returns
   *         its terminal result and may then queue generic persistence.
   * @note Persistent if Config::enableEepromWrites is true
   * @note This legacy helper selects crystal-derived mode (OS=0). Use
   *       setClkoutConfig() for high-frequency OS/HFD configuration.
   * @note Returns BUSY without mutation while interrupt-controlled CLKOUT is
   *       enabled or active; disable CLKIE and clear CLKF explicitly first.
   * @warning When EEPROM persistence is enabled, this schedules a wear-limited,
   *          deadline-driven EEPROM commit. Call tick() or pollEeprom() until
   *          getEepromStatus() reports a terminal status.
   */
  Status setClkoutFrequency(ClkoutFrequency freq);

  /**
   * @brief Read the crystal-derived clock output frequency selection
   * 
   * @param[out] freq Crystal-derived output frequency setting
   * @return Status::Ok() on success, INVALID_PARAM when high-frequency mode is
   *         active, or a transport/precondition error otherwise.
   * @note Use getClkoutConfig() to inspect high-frequency OS/HFD configuration.
   */
  Status getClkoutFrequency(ClkoutFrequency& freq);

  /**
   * @brief Cooperatively configure the complete active CLKOUT state.
   * @note Queues wear-limited generic persistence when
   *       Config::enableEepromWrites is true; finish with tick()/pollEeprom().
   * @note Both crystal FD and high-frequency HFD fields are stored regardless
   *       of which OS mode is active, so later mode changes retain both values.
   * @note The cooperative sequence first proves interrupt-controlled CLKOUT is
   *       disabled/inactive, stops direct output, changes C2/C3, then restores
   *       the requested direct NCLKE state. Otherwise it returns BUSY before mutation.
   * @warning The register encodes dividers through 8192 (67.108864 MHz), but
   *          the vendor guarantees CLKOUT electrical characteristics only
   *          through 52 MHz. Board/application design must enforce that limit.
   */
  Status setClkoutConfig(const ClkoutConfig& config);
  /** @brief Read complete active CLKOUT state in one burst. */
  Status getClkoutConfig(ClkoutConfig& config);
  /** @brief Cooperatively set active-only clock-output interrupt enable. */
  Status setClockInterruptEnabled(bool enabled);
  /** @brief Read clock-output interrupt enable. */
  Status getClockInterruptEnabled(bool& enabled);
  /** @brief Cooperatively configure active-only CLKOUT delay and interrupt sources. */
  Status setClockInterruptMaskConfig(const ClockInterruptMaskConfig& config);
  /** @brief Read CLKOUT delay and interrupt source selection. */
  Status getClockInterruptMaskConfig(ClockInterruptMaskConfig& config);
  /** @brief Read the latched clock-output interrupt flag (CLKF). */
  Status getClockOutputFlag(bool& triggered);
  /**
   * @brief Cooperatively clear active-only CLKF while preserving EEF and BSF.
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Returns BUSY without writing if EEPROM is currently busy.
   */
  Status clearClockOutputFlag();

  // ===== Calibration Operations =====

  /**
   * @brief Set frequency offset in parts-per-million
   * 
   * @param ppm Signed measured frequency error in ppm, quantized to the nearest
   *            0.238418579 ppm step (range -7.6293945 to +7.3909760 ppm).
   * @return IN_PROGRESS when the active update is admitted. pollJob() returns
   *         its terminal result and may then queue generic persistence.
   * @note Follow the vendor calibration convention: pass the signed measured
   *       frequency error (for example, a measured +1.2 ppm error is encoded as
   *       approximately +5 steps). Persistent if Config::enableEepromWrites is true.
   * @warning When EEPROM persistence is enabled, this schedules a wear-limited,
   *          deadline-driven EEPROM commit. Call tick() or pollEeprom() until
   *          getEepromStatus() reports a terminal status.
   */
  Status setOffsetPpm(float ppm);

  /**
   * @brief Read frequency offset in parts-per-million
   * 
   * @param[out] ppm Current offset in ppm
   * @return Status::Ok() on success, error otherwise.
   */
  Status getOffsetPpm(float& ppm);

  /**
   * @brief Cooperatively set the Power-On-Reset interrupt enable (PORIE).
   * @note When Config::enableEepromWrites is true, this queues a wear-limited
   *       EEPROM item that the caller must advance through tick()/pollEeprom().
   * @warning An active-only change cannot control the next power-on reset. PORIE
   *          must be durably provisioned beforehand when that behavior is required.
   */
  Status setPowerOnResetInterruptEnabled(bool enabled);
  /** @brief Read the active Power-On-Reset interrupt enable (PORIE). */
  Status getPowerOnResetInterruptEnabled(bool& enabled);
  /**
   * @brief Cooperatively set the Voltage-Low interrupt enable (VLIE).
   * @note When Config::enableEepromWrites is true, this queues a wear-limited
   *       EEPROM item that the caller must advance through tick()/pollEeprom().
   * @warning Persist this setting beforehand when it must survive a refresh or
   *          reset; an active-only change is volatile.
   */
  Status setVoltageLowInterruptEnabled(bool enabled);
  /** @brief Read the active Voltage-Low interrupt enable (VLIE). */
  Status getVoltageLowInterruptEnabled(bool& enabled);

  // ===== Temperature Sensor =====

  /**
   * @brief Read die temperature
   * 
   * @param[out] celsius Temperature in degrees Celsius
   * @return Status::Ok() on success, error otherwise.
   * @note Typical accuracy: +/-3 C. Not intended as precision thermometer.
   *       This one-transfer convenience read is not coherent across a 1 Hz
   *       temperature update because the device has no shadow latch. Use
   *       startReadCoherentTemperatureJob() when coherence matters.
   */
  Status readTemperatureC(float& celsius);

  /**
   * @brief Cooperatively set temperature thresholds/events/interrupts.
   * @note TLT and THT are independent signed-byte comparators; either ordering
   *       is vendor-valid and is preserved exactly.
   * @note Active-only. The job disables temperature detection/interrupts before
   *       changing overwrite/threshold fields, then enables interrupts before
   *       detection. It does not implicitly clear flags or reset timestamps.
   */
  Status setTemperatureEventConfig(const TemperatureEventConfig& config);
  /** @brief Read temperature thresholds/events/interrupts in one burst. */
  Status getTemperatureEventConfig(TemperatureEventConfig& config);
  /**
   * @brief Cooperatively set the signed 16-bit raw TREF correction value.
   * @note One raw step is 1/128 degree C (0.0078125 C). This adjusts the
   *       temperature reference used by compensation and is not a replacement
   *       for the device's factory calibration procedure.
   * @note Queues wear-limited generic persistence when
   *       Config::enableEepromWrites is true; finish with tick()/pollEeprom().
   */
  Status setTemperatureReference(int16_t referenceRaw);
  /** @brief Read the signed 16-bit raw TREF correction value in one burst. */
  Status getTemperatureReference(int16_t& referenceRaw);
  /** @brief Read the low/high temperature event flags without clearing them. */
  Status getTemperatureFlags(bool& lowTriggered, bool& highTriggered);
  /** @brief Cooperatively clear both active-only THF and TLF together. */
  Status clearTemperatureFlags();
  /** @brief Start a two-sample coherent temperature-read job. */
  Status startReadCoherentTemperatureJob(
      uint32_t nowMs, uint32_t operationTimeoutMs = 100);
  /** @brief Copy the completed coherent temperature result. */
  Status getReadCoherentTemperatureJobResult(
      CoherentTemperatureResult& result) const;

  // ===== External Event Input =====

  /**
   * @brief Set external event input edge sensitivity
   * 
   * @param rising true for rising edge, false for falling edge
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Active-only; no EEPROM is involved.
   */
  Status setEviEdge(bool rising);

  /**
   * @brief Set external event input debounce filter
   * 
   * @param debounce Debounce filter setting
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Active-only; no EEPROM is involved.
   */
  Status setEviDebounce(EviDebounce debounce);

  /**
   * @brief Enable overwriting of event timestamps
   * 
   * @param enable true to allow overwrite, false to preserve first event
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Active-only; no EEPROM is involved.
   */
  Status setEviOverwrite(bool enable);

  /**
   * @brief Read complete EVI configuration
   * 
   * @param[out] out Structure to receive EVI configuration
   * @return Status::Ok() on success, error otherwise.
   */
  Status getEviConfig(EviConfig& out);

  /** @brief Cooperatively set the active-only external-event interrupt enable. */
  Status setEventInterruptEnabled(bool enabled);
  /** @brief Read the external-event interrupt enable. */
  Status getEventInterruptEnabled(bool& enabled);
  /** @brief Read EVF without clearing it. */
  Status getEventFlag(bool& triggered);
  /** @brief Cooperatively clear EVF, subject to Status side-effect guards. */
  Status clearEventFlag(); ///< Active-only Status W0C operation.
  /** @brief Cooperatively set event-input synchronization. */
  Status setEventSynchronizationEnabled(bool enabled); ///< Active-only ESYN arm/disarm.
  /** @brief Read event-input synchronization enable (ESYN). */
  Status getEventSynchronizationEnabled(bool& enabled);
  /** @brief Cooperatively enable CLKOUT switch-off delay after I2C STOP. */
  Status setClkoutStopDelayEnabled(bool enabled); ///< Active-only EVI CLKDE update.
  /** @brief Read CLKOUT switch-off delay enable (CLKDE). */
  Status getClkoutStopDelayEnabled(bool& enabled);

  /**
   * @brief Cooperatively set the RTC STOP bit.
   * @note STOP=1 resets the hundredths counter and 4096 Hz through 1 Hz
   *       prescalers, stops the calendar, and disables CLKOUT, timer, periodic
   *       update, EVI filtering, and temperature measurement until restarted.
   */
  Status setStopEnabled(bool enabled); ///< Active-only STOP update.
  /** @brief Read the RTC STOP bit. */
  Status getStopEnabled(bool& enabled);
  /** @brief Cooperatively set Control 1 GP0 and Control 2 GP1. */
  Status setGeneralPurposeBits(bool gp0, bool gp1); ///< Active-only GP0/GP1 update.
  /** @brief Read Control 1 GP0 and Control 2 GP1 in one burst. */
  Status getGeneralPurposeBits(bool& gp0, bool& gp1);

  /// Load exactly one caller-supplied credential into the write-only active
  /// password input registers. No guessing, retry, credential readback, or
  /// retained credential storage is provided by the library.
  Status unlockPasswordProtection(const PasswordCredential& credential);
  /** @brief Start a durable, redacted password-protection configuration job. */
  Status startConfigurePasswordProtectionJob(
      const PasswordCredential& currentCredential,
      const PasswordCredential& newCredential,
      bool enable,
      uint32_t nowMs,
      uint32_t operationTimeoutMs = 4000);
  /** @brief Return credential-free password-protection evidence. */
  Status getPasswordProtectionStatus(PasswordProtectionStatus& status) const;

  /**
   * @brief Read and decode a hardware timestamp block.
   *
   * @param source Timestamp source to read
   * @param[out] out Decoded timestamp data
   * @return Status::Ok() on success, error otherwise
   * @note A reset all-zero block returns OK with out.timeValid=false. Non-empty
   *       blocks are decoded strictly, including reserved-bit validation.
   */
  Status readTimestamp(TimestampSource source, Timestamp& out);

  /**
   * @brief Reset one hardware timestamp block.
   *
   * @param source Timestamp source to reset
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @warning Clears the selected captured timestamp/count. Call only after the
   *          application has consumed the event data. Resetting TLow also
   *          clears TLF; resetting THigh also clears THF. Resetting EVI does
   *          not clear EVF. Because EVR may remain set and another written 1
   *          may not reset again, the cooperative EVI path emits a preserved
   *          0-to-1 pulse when necessary.
   */
  Status resetTimestamp(TimestampSource source); ///< Active-only timestamp reset.

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
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Writing STATUS clears THF/TLF regardless of mask (datasheet behavior).
   *       If THF/TLF are currently set but omitted from mask, this returns
   *       INVALID_PARAM instead of clearing them implicitly.
   *       This is an active-only Status W0C operation.
   * @warning Clears latched status flags. Call only after the application has
   *          handled the corresponding fault/event state.
   */
  Status clearStatus(uint8_t mask);

  /**
   * @brief Read decoded status register flags
   * 
   * @param[out] out Decoded status flags
   * @return Status::Ok() on success, error otherwise
   */
  Status readStatusFlags(StatusFlags& out);

  /**
   * @brief Read validity-related flags (PORF, VLF, BSF)
   * 
   * @param[out] out Validity flags
   * @return Status::Ok() on success, error otherwise
   * @note timeInvalid is true when PORF or VLF indicates invalid time.
   */
  Status readValidity(ValidityFlags& out);

  /**
   * @brief Clear power-on reset flag (PORF)
   * 
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note PORF indicates a power-up event occurred. Once confirmed the time is correct,
   *       clear this active-only flag to indicate time validity.
   */
  Status clearPowerOnResetFlag();

  /**
   * @brief Clear voltage low flag (VLF)
   * 
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note VLF is set when a low-voltage event occurs. Once verified, clear this
   *       active-only flag.
   */
  Status clearVoltageLowFlag();

  /**
   * @brief Clear backup switchover flag (BSF)
   * 
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Must be cleared while VDD is present (per datasheet). Returns BUSY
   *       without writing when EEF, EEbusy, or CLKF are set in the same register.
   *       This is an active-only TEMP_LSB W0C operation.
   */
  Status clearBackupSwitchFlag();

  /**
   * @brief Read the chip's EEbusy and sticky EEF hardware flags.
   * @note This is distinct from isEepromBusy(), which reports library-owned
   *       cooperative work rather than the chip's EEbusy bit.
   */
  Status getEepromHardwareFlags(EepromHardwareFlags& out);

  /**
   * @brief Cooperatively clear the chip's sticky EEPROM failure flag (EEF).
   * @return IN_PROGRESS when admitted; pollJob() returns the terminal result.
   * @note Returns BUSY without writing while hardware EEbusy is set and
   *       preserves CLKF, BSF, and the read-only temperature fraction.
   *       This is an active-only TEMP_LSB W0C operation.
   * @warning Clearing EEF acknowledges diagnostic evidence; it does not make a
   *          failed or unverified EEPROM write durable.
   */
  Status clearEepromErrorFlag();

  // ===== User RAM Operations =====

  /**
   * @brief Read the 16-byte volatile user RAM area.
   *
   * @param offset Offset inside user RAM (0-15)
   * @param[out] buf Destination buffer
   * @param len Number of bytes to read
   * @return Status::Ok() on success, error otherwise
   */
  Status readUserRam(uint8_t offset, uint8_t* buf, size_t len);

  /**
   * @brief Write the 16-byte volatile user RAM area.
   *
   * @param offset Offset inside user RAM (0-15)
   * @param buf Source buffer
   * @param len Number of bytes to write
   * @return OK after one burst for lengths up to 15; IN_PROGRESS for the full
   *         16-byte cooperative job, whose terminal result is returned by
   *         pollJob(); or a validation/transport error.
   * @note User RAM is volatile; this operation never uses EEPROM.
   */
  Status writeUserRam(uint8_t offset, const uint8_t* buf, size_t len);

  // ===== Low-Level Diagnostic Register Operations =====

  /**
   * @brief Read single RTC register
   * 
   * @param reg Register address
   * @param[out] value Register value read
   * @return Status::Ok() on success, error otherwise
   * @note Validates that reg is in a reviewed direct read range. Password,
   *       EEPROM staging/command, indirect EEPROM, and reserved gaps are rejected.
   *       This is a diagnostic/control helper using tracked I2C bounded by
   *       Config::i2cTimeoutMs; prefer typed APIs for normal time, alarm,
   *       timer, status, and EEPROM flows.
   * @warning Direct register access can disrupt RTC operation if misused
   */
  Status readRegister(uint8_t reg, uint8_t& value);

  /**
   * @brief Write single RTC register
   * 
   * @param reg Register address
   * @param value Value to write
   * @return Status::Ok() on success, error otherwise
   * @note Uses a reviewed per-register direct-write allowlist. Status,
   *       control, password, EEPROM staging/command, indirect EEPROM,
   *       read-only, and reserved registers are rejected before I2C.
   *       This is a diagnostic/control helper using one tracked I2C write
   *       bounded by Config::i2cTimeoutMs; it is not the managed EEPROM
   *       persistence path.
   * @warning Direct writes are limited to temperature thresholds and volatile
   *          user RAM; prefer typed APIs where available.
   */
  Status writeRegister(uint8_t reg, uint8_t value);

  /**
   * @brief Read a contiguous register block using tracked I2C.
   *
   * @param reg Start register address
   * @param[out] buf Destination buffer
   * @param len Number of bytes to read
   * @return Status::Ok() on success, error otherwise
   * @note Rejects zero length, oversized reads, wraparound, and blocks that
   *       cross undocumented address gaps. This is a diagnostic/control helper
   *       using tracked I2C bounded by Config::i2cTimeoutMs; prefer typed APIs
   *       for normal application flows.
   * @warning Direct block access can disrupt RTC operation if misused.
   */
  Status readRegisters(uint8_t reg, uint8_t* buf, size_t len);

  /**
   * @brief Write a contiguous register block using tracked I2C.
   *
   * @param reg Start register address
   * @param buf Source buffer
   * @param len Number of bytes to write
   * @return Status::Ok() on success, error otherwise
   * @note Rejects zero length, writes larger than the fixed stack buffer,
   *       wraparound, and blocks that cross undocumented address gaps. This is
   *       a diagnostic/control helper using tracked I2C bounded by
   *       Config::i2cTimeoutMs; it is not the managed EEPROM persistence path.
   * @warning Every byte must pass the reviewed direct-write allowlist. Only
   *          temperature thresholds and volatile user RAM are writable.
   */
  Status writeRegisters(uint8_t reg, const uint8_t* buf, size_t len);

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

  /**
   * @brief Convert BCD value to binary
   * 
   * @param bcd BCD-encoded value
   * @return Binary value
   */
  static uint8_t bcdToBinary(uint8_t bcd);

  /**
   * @brief Convert binary value to BCD
   * 
   * @param bin Binary value
   * @return BCD-encoded value
   */
  static uint8_t binaryToBcd(uint8_t bin);

  /**
   * @brief Convert Unix timestamp to DateTime
   * 
   * @param ts Unix timestamp (seconds since epoch)
   * @param[out] out DateTime result
   * @return true on success, false if out of range
   */
  static bool unixToDateTime(uint32_t ts, DateTime& out);

  /**
   * @brief Convert DateTime to Unix timestamp
   * 
   * @param time DateTime to convert
   * @param[out] out Unix timestamp result
   * @return true on success, false if DateTime invalid
   */
  static bool dateTimeToUnix(const DateTime& time, uint32_t& out);

 private:
  enum class EepromState : uint8_t {
    Idle,
    ReadControl1,
    EnableEerd,
    VerifyEerd,
    WaitReady,
    ReadActiveC0,
    WriteSafeC0,
    VerifySafeC0,
    WriteAddr,
    VerifyAddr,
    WriteSentinel1,
    VerifySentinel1,
    PreReadBusy1,
    ReadCmd1,
    WaitRead1,
    PollRead1,
    ReadData1,
    WriteSentinel2,
    VerifySentinel2,
    PreReadBusy2,
    ReadCmd2,
    WaitRead2,
    PollRead2,
    ReadData2,
    ComparePersistent,
    ClearEef,
    VerifyEef,
    WriteData,
    VerifyData,
    WaitReadyPreCmd,
    WriteCmd,
    WaitWriteSettle,
    WaitReadyPostCmd,
    VerifyWriteFlags,
    BeginVerifyPersistent,
    CleanupWaitReady,
    RestoreSelectedActive,
    VerifySelectedActive,
    RestoreActive,
    VerifyActive,
    RestoreControl1,
    VerifyControl1,
    Settle,
    WritePassword,
    ApplyPassword,
    ApplyPasswordBytes,
    ApplyPasswordCredential,
    FinalizePasswordEnable,
    CleanupLockPassword
  };

  enum class JobKind : uint8_t {
    None,
    SetTimer,
    RegisterUpdate,
    WriteUserRam,
    ReadCoherentTemperature,
    ReadTimeSnapshot,
    SetTimeVerified,
    PersistentRead,
    UserEepromWrite,
    PasswordProtection,
  };

  enum class JobState : uint8_t {
    Idle,
    SetTimerReadControl1,
    SetTimerReadTimerHigh,
    SetTimerWriteControl1,
    SetTimerWriteLow,
    SetTimerWriteHigh,
    SetTimerWriteFinalControl1,
    RegisterUpdateRead,
    RegisterUpdateWrite,
    EviResetRead,
    EviResetWriteClear,
    EviResetWriteSet,
    RegisterBlockUpdateRead,
    RegisterBlockUpdateWrite,
    ClkoutReadActive,
    ClkoutReadGuards,
    ClkoutWriteStoppedConfig,
    ClkoutWriteFinalPmu,
    TemperatureConfigRead,
    TemperatureConfigWriteDisabled,
    TemperatureConfigWriteTimestampControl,
    TemperatureConfigWriteThresholds,
    TemperatureConfigWriteInterrupts,
    TemperatureConfigWriteDetection,
    WriteUserRamChunk,
    ReadTemperatureFirst,
    ReadTemperatureSecond,
    ReadTimeStatus,
    ReadTimeCalendar,
    SetTimeReadStatusBefore,
    SetTimeWriteCalendar,
    SetTimeVerifyCalendar,
    SetTimeReadStatusBeforeClear,
    SetTimeWriteStatus,
    SetTimeReadStatusAfter,
    SetTimeReadFinalCalendar,
    Persistent
  };

  struct EepromWrite {
    uint8_t reg = 0;
    uint8_t value = 0;
  };

  static constexpr size_t kEepromQueueSize = 8;  // Fixed-size queue (no heap allocation)
  static constexpr size_t kJobUserRamBufferSize = 16;

  struct EepromOp {
    EepromState state = EepromState::Idle;
    uint8_t reg = 0;
    uint8_t value = 0;
    
    // Circular buffer queue
    EepromWrite queue[kEepromQueueSize];
    uint8_t queueHead = 0;  // Next write position
    uint8_t queueTail = 0;  // Next read position
    uint8_t queueCount = 0; // Number of items in queue
  };

  struct JobOp {
    JobState state = JobState::Idle;
    JobKind activeKind = JobKind::None;
    JobKind completedKind = JobKind::None;
    Status lastStatus = Status::Ok();
    Status completedStatus = Status::Ok();
    Status persistentCleanupStatus = Status::Ok();
    uint32_t deadlineMs = 0;
    uint32_t mutationCutoffMs = 0;
    bool deadlineActive = false;
    bool mutationCutoffActive = false;
    uint16_t timerTicks = 0;
    TimerFrequency timerFreq = TimerFrequency::Hz4096;
    bool timerEnable = false;
    uint8_t timerControl1 = 0;
    uint8_t timerFinalControl1 = 0;
    uint8_t timerHigh = 0;
    uint8_t registerUpdateReg = 0;
    uint8_t registerUpdateImplementedMask = 0xFF;
    uint8_t registerUpdateClearMask = 0;
    uint8_t registerUpdateSetMask = 0;
    uint8_t registerUpdateValue = 0;
    uint8_t registerUpdateRequiredMask = 0;
    uint8_t registerUpdateForbiddenMask = 0;
    bool registerUpdateGuardedClear = false;
    bool registerUpdateGuardFailureIsBusy = false;
    bool persistRegisterUpdate = false;
    uint8_t registerBlockReg = 0;
    uint8_t registerBlockLen = 0;
    uint8_t registerBlockValues[8] = {0};
    uint8_t registerBlockClear[8] = {0};
    uint8_t registerBlockSet[8] = {0};
    uint8_t registerBlockImplemented[8] = {0};
    ClkoutConfig clkoutConfig{};
    uint8_t clkoutGuard[4] = {0};
    bool clkoutLegacyFrequency = false;
    uint8_t userRamOffset = 0;
    uint8_t userRamLen = 0;
    uint8_t userRamWritten = 0;
    uint8_t userRamBuf[kJobUserRamBufferSize] = {0};
    uint8_t firstTemperature[2] = {0};
    CoherentTemperatureResult coherentTemperature{};
    TimeSnapshot timeSnapshot{};
    VerifiedTimeSetReport verifiedSet{};
    uint8_t calendarBuf[7] = {0};
    DateTime firstVerified{};
    bool firstVerifiedValid = false;
    EepromState persistentState = EepromState::Idle;
    uint8_t persistentAddress = 0;
    uint8_t persistentLength = 0;
    uint8_t persistentIndex = 0;
    uint8_t persistentSentinel = 0;
    uint8_t persistentFirstRead = 0;
    uint8_t persistentControl1 = 0;
    uint8_t persistentActiveC0 = 0;
    uint8_t persistentSafeC0 = 0;
    uint8_t persistentDesired = 0;
    uint16_t persistentReadyChecks = 0;
    uint32_t persistentNotBeforeMs = 0;
    uint32_t persistentPhaseDeadlineMs = 0;
    bool persistentControl1Valid = false;
    bool persistentActiveC0Valid = false;
    bool persistentSafeC0Verified = false;
    bool persistentWriteMode = false;
    bool persistentWriteAttempted = false;
    bool persistentCleanupRequired = false;
    PersistentReadResult persistentRead{};
    UserEepromWriteReport userEepromWrite{};
    bool persistentUseAddressList = false;
    uint8_t persistentAddresses[6] = {0};
    PasswordCredential currentCredential{};
    PasswordCredential newCredential{};
    bool passwordEnable = false;
    bool passwordAuthorizationMayBeActive = false;
  };

  Config _config;
  bool _initialized = false;
  bool _beginInProgress = false;
  EepromOp _eeprom;
  JobOp _job;
  Status _eepromLastStatus = Status::Ok();
  uint32_t _eepromWriteCount = 0;
  uint32_t _eepromWriteFailures = 0;
  bool _primaryCellEnsureAttempted = false;
  PasswordProtectionStatus _passwordStatus{};

  // Driver state and health tracking
  DriverState _driverState = DriverState::UNINIT;
  uint32_t _lastOkMs = 0;              ///< Timestamp of last successful operation
  Status _lastError = Status::Ok();    ///< Most recent error status
  uint32_t _lastErrorMs = 0;           ///< Timestamp of last error
  uint8_t _consecutiveFailures = 0;    ///< Consecutive failures since last success
  uint32_t _totalFailures = 0;         ///< Total failures since begin()
  uint32_t _totalSuccess = 0;          ///< Total successes since begin()
  
  // Raw I2C transport (no health tracking). Probe uses it directly; tracked
  // wrappers delegate through it before updating health.
  Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen, uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteRaw(const uint8_t* buf, size_t len);
  Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                          uint8_t* rxBuf, size_t rxLen,
                          uint32_t timeoutMs);
  Status _i2cWriteRaw(const uint8_t* buf, size_t len, uint32_t timeoutMs);

  // Tracked I2C transport (with health tracking) - for normal operations
  Status _i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen, uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteTracked(const uint8_t* buf, size_t len);
  Status _i2cWriteReadTrackedTimeout(const uint8_t* txBuf, size_t txLen,
                                     uint8_t* rxBuf, size_t rxLen,
                                     uint32_t timeoutMs);
  Status _i2cWriteTrackedTimeout(const uint8_t* buf, size_t len,
                                 uint32_t timeoutMs);

  // Register-level I2C helpers (use tracked transport internally)
  Status readRegs(uint8_t reg, uint8_t* buf, size_t len);
  Status writeRegs(uint8_t reg, const uint8_t* buf, size_t len);
  
  // Raw register access (no health tracking) - for diagnostics
  Status _readRegisterRaw(uint8_t reg, uint8_t& value);

  // EEPROM operations
  Status processEeprom(uint32_t now_ms, uint8_t maxInstructions, uint8_t& instructionsUsed);
  Status startEepromUpdate(uint8_t reg, uint8_t value);
  Status readEepromFlags(bool& busy, bool& failed);
  bool eepromQueueContains(uint8_t reg, uint8_t value) const;
  bool eepromQueuePush(uint8_t reg, uint8_t value);
  bool eepromQueuePop(uint8_t& reg, uint8_t& value);
  Status processPersistentJob(uint32_t nowMs, bool& callbackUsed);
  Status startPersistentReadJob(uint8_t address, uint8_t length,
                                uint32_t nowMs, uint32_t timeoutMs);
  void finishJob(const Status& status);
  bool workIdle() const;

  // Health tracking (called only by tracked transport wrappers)
  Status _updateHealth(const Status& st);
  uint32_t _nowMs() const;
  void _resetRuntimeState();

  Status updateRegisterSingle(uint8_t reg, uint8_t implementedMask,
                              uint8_t clearMask, uint8_t setMask);
  Status updateRegisterBlock(uint8_t reg, uint8_t length,
                             const uint8_t* implementedMasks,
                             const uint8_t* clearMasks,
                             const uint8_t* setMasks);
  Status readPersistentC0ForEnsure(uint32_t operationStart,
                                   uint8_t& value,
                                   PrimaryCellConfigurationReport& report,
                                   bool& callbackReturnedLate);
  Status cleanupPrimaryCellEnsure(uint32_t operationStart,
                                  uint8_t control1Before,
                                  bool control1Valid,
                                  bool control1WriteAttempted,
                                  bool accessStateVerified,
                                  uint8_t activeBefore,
                                  bool activeValid,
                                  uint8_t safeActive,
                                  bool safeValid,
                                  bool safeActiveWriteAmbiguous,
                                  bool persistenceTrusted,
                                  uint8_t target,
                                  PrimaryCellConfigurationReport& report,
                                  bool& callbackReturnedLate);
  Status ensureRead(uint8_t reg, uint8_t* data, size_t len,
                    uint32_t operationStart, uint32_t phaseDeadlineMs,
                    bool requireCleanupReserve = false,
                    bool* callbackReturnedLate = nullptr);
  Status ensureWrite(uint8_t reg, const uint8_t* data, size_t len,
                     uint32_t operationStart, uint32_t phaseDeadlineMs,
                     bool requireCleanupReserve = false,
                     bool* callbackAttempted = nullptr,
                     bool* callbackReturnedLate = nullptr);
  Status ensureWait(uint32_t delayMs, uint32_t operationStart);
  Status ensureWaitReady(uint32_t operationStart, uint32_t phaseTimeoutMs,
                         uint16_t checkCap, uint8_t& tempLsb,
                         bool requireCleanupReserve = false,
                         bool* callbackReturnedLate = nullptr);
  static StatusFlags decodeStatusFlags(uint8_t raw);
  static bool decodeCalendar(const uint8_t* data, DateTime& out);
  static bool acceptedVerifiedTime(const DateTime& requested,
                                   const DateTime& observed);

  // Conversion helpers
  static bool isValidBcd(uint8_t v);
  static uint8_t bcdToBin(uint8_t v);
  static uint8_t binToBcd(uint8_t v);
  static bool isLeapYear(uint16_t year);
  static uint8_t daysInMonth(uint16_t year, uint8_t month);
  static uint32_t dateToDays(uint16_t year, uint8_t month, uint8_t day);
  static bool unixToDate(uint32_t ts, DateTime& out);
};

}  // namespace RV3032
