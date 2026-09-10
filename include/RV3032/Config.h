/**
 * @file Config.h
 * @brief Configuration for RV3032-C7 RTC library
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Status.h"

namespace RV3032 {

/**
 * @enum BackupSwitchMode
 * @brief Battery backup switching modes
 * 
 * Controls how the RTC switches between main power and backup battery.
 * These are logical API values, not the raw PMU BSM bit encodings. The driver
 * encodes them explicitly; do not cast an enum value into a register field.
 */
enum class BackupSwitchMode : uint8_t {
  Off = 0,     ///< Backup switching disabled (no battery backup)
  Level = 1,   ///< Level mode (raw BSM=10); use only for documented topology.
  Direct = 2   ///< Direct mode (raw BSM=01); requires VDD to remain above VBACKUP.
};

/** @brief Explicit permission for a PMU change to energize backup charging. */
enum class BackupChargePolicy : uint8_t {
  REQUIRE_CHARGER_OFF = 0, ///< Reject a resulting enabled-BSM/nonzero-TCM pair.
  ALLOW_BACKUP_CHARGING = 1, ///< Caller confirms charging is intended.
};

/// @brief I2C write callback signature.
/// @note Invocation is synchronous. The buffer is borrowed only for the
///       callback duration and Status::msg must have static storage. Legal
///       return codes are OK, I2C_ERROR, I2C_NACK_ADDR, I2C_NACK_DATA,
///       I2C_TIMEOUT, and I2C_BUS.
/// @note timeoutMs is a hard, exclusive bound for the complete callback,
///       including application serialization and every physical bus phase.
///       The application owner must ensure its shared-bus mutex is uncontended
///       before dispatch.
/// @warning One invocation is exactly one physical write attempt. The callback
///          must not retry, recover the bus, or replay a possibly mutating
///          transfer. Ambiguous outcomes are reconciled by later driver reads.
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                              uint32_t timeoutMs, void* user);

/// @brief I2C write-read callback signature.
/// @note Invocation is synchronous. All buffers are borrowed only for the
///       callback duration and Status::msg must have static storage. Legal
///       return codes are OK, I2C_ERROR, I2C_NACK_ADDR, I2C_NACK_DATA,
///       I2C_TIMEOUT, and I2C_BUS.
/// @note timeoutMs is a hard, exclusive bound for the complete callback,
///       including application serialization, recovery when permitted, and
///       every physical bus phase. The application owner must ensure its
///       shared-bus mutex is uncontended before dispatch.
/// @note For ordinary read-only operations, an application owner may document
///       bounded recovery plus at most one read retry inside this callback.
///       Both physical attempts together must finish within the supplied
///       timeout; the driver counts them as one callback and one instruction.
/// @warning While ensurePrimaryCellConfiguration() is active, the application
///          must switch this callback to a scoped single-attempt mode: every
///          read is one physical attempt with no recovery or retry.
using I2cWriteReadFn = Status (*)(uint8_t addr, const uint8_t* tx, size_t txLen,
                                  uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                                  void* user);

/// Millisecond timestamp callback.
/// @param user User context pointer passed through from Config
/// @return Current monotonic milliseconds
using NowMsFn = uint32_t (*)(void* user);

/// Sleeping/yielding millisecond wait callback used only by the explicit
/// synchronous primary-cell ensure operation.
/// @note The callback must not return before delayMs monotonic milliseconds
///       have elapsed. A scheduler-based delay may therefore need one guard
///       tick because entering a relative tick delay just before the next tick
///       can otherwise sleep for less than requested.
/// @warning The callback must not busy-spin or perform I2C work.
using WaitMsFn = void (*)(uint32_t delayMs, void* user);

/**
 * @struct Config
 * @brief RTC configuration parameters
 *
 * All hardware resources are application-provided. Library does not
 * define any pin defaults - board-specific values must be passed by user.
 */
struct Config {
  /// @brief I2C write callback (required, always single-attempt).
  I2cWriteFn i2cWrite = nullptr;

  /// @brief I2C write-read callback (required; see scoped ensure contract).
  I2cWriteReadFn i2cWriteRead = nullptr;

  /// @brief Opaque application transport-owner context.
  void* i2cUser = nullptr;

  /// @brief Monotonic millisecond source callback (optional).
  /// @note If null, health timestamps remain 0 and caller-supplied poll/tick
  ///       times drive cooperative deadlines. Each invoked cooperative
  ///       transport callback is then conservatively charged its exact
  ///       supplied timeout before another instruction can be dispatched.
  NowMsFn nowMs = nullptr;

  /// @brief Sleeping/yielding wait source (optional for cooperative use).
  /// @note Required by ensurePrimaryCellConfiguration(); must honor the full
  ///       requested monotonic duration and must not spin or use I2C.
  WaitMsFn waitMs = nullptr;

  /// @brief User context passed to timing callbacks.
  void* timeUser = nullptr;

  /// @brief I2C address of RV3032-C7 (fixed at 0x51 on hardware)
  /// @note begin() validates this is exactly 0x51.
  uint8_t i2cAddress = 0x51;

  /// @brief I2C transaction timeout in milliseconds (default: 50ms)
  /// @note The library never configures the bus. Cooperative transfers clip
  ///       this value to the earliest exclusive deadline minus one
  ///       millisecond. The complete application callback must finish within
  ///       the supplied clipped timeout; exceeding it reports I2C_TIMEOUT.
  ///       Valid range is 1..100 ms.
  uint32_t i2cTimeoutMs = 50;

  /// @brief Enable explicit generic EEPROM persistence (default: false)
  /// @note This authorizes only configuration EEPROM C0..C5 and typed user
  ///       EEPROM CB..EA. It never grants password-register authority. When
  ///       true, supported EEPROM-backed config changes may queue persistence
  ///       after an explicit typed setter.
  ///       begin() never reads or mutates the device and never queues work.
  ///       When false, config is RAM-only (faster, saves EEPROM wear) and may
  ///       be replaced by the next POR, automatic date-increment, or software
  ///       refresh from stored EEPROM.
  ///       Typed setters that need read-modify-write return IN_PROGRESS and must
  ///       first be advanced through pollJob(). Their resulting active bytes are
  ///       then queued for tick()/pollEeprom() persistence.
  ///       Configuration EEPROM endurance is 10,000 writes at 3.0 V/25 C and
  ///       100 writes at 5.5 V/85 C. Compare before writing.
  bool enableEepromWrites = false;

  /// @brief EEPROM busy-poll window after the mandatory write settle (default: 100ms)
  /// @note begin() always requires 10..250 ms, including when generic writes
  ///       are disabled, so access-state recovery remains available. The 10 ms
  ///       post-WRITE_ONE wait is additional and is measured from
  ///       transport-callback completion.
  uint32_t eepromTimeoutMs = 100;

  /// @brief Consecutive failure threshold before transitioning to OFFLINE
  /// @note Default: 5. DEGRADED = [1, offlineThreshold-1], OFFLINE >= offlineThreshold.
  ///       Values below 1 are rejected by begin().
  uint8_t offlineThreshold = 5;

  /// @brief Per-callback timeout used only by primary-cell ensure (default: 5ms)
  /// @note Valid range is 1..5 ms. Kept as a trailing field so existing
  ///       positional aggregate initializers retain their meaning. The
  ///       dedicated synchronous primary-cell operation has a fixed one-second
  ///       whole-operation bound and sizes its cleanup reserve for this
  ///       independent transfer bound.
  uint32_t primaryCellI2cTimeoutMs = 5;
};

}  // namespace RV3032
