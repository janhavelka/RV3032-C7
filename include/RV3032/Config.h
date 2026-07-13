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
 */
enum class BackupSwitchMode : uint8_t {
  Off = 0,     ///< Backup switching disabled (no battery backup)
  Level = 1,   ///< Level switching mode (threshold-based, recommended)
  Direct = 2   ///< Direct switching mode (instant switchover)
};

/// @brief I2C write callback signature.
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                              uint32_t timeoutMs, void* user);

/// @brief I2C write-read callback signature.
using I2cWriteReadFn = Status (*)(uint8_t addr, const uint8_t* tx, size_t txLen,
                                  uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                                  void* user);

/// Millisecond timestamp callback.
/// @param user User context pointer passed through from Config
/// @return Current monotonic milliseconds
using NowMsFn = uint32_t (*)(void* user);

/// Yielding millisecond wait callback used only by the explicit synchronous
/// primary-cell ensure operation.
using WaitMsFn = void (*)(uint32_t delayMs, void* user);

/**
 * @struct Config
 * @brief RTC configuration parameters
 *
 * All hardware resources are application-provided. Library does not
 * define any pin defaults - board-specific values must be passed by user.
 */
struct Config {
  /// @brief I2C write callback (required).
  I2cWriteFn i2cWrite = nullptr;

  /// @brief I2C write-read callback (required).
  I2cWriteReadFn i2cWriteRead = nullptr;

  /// @brief Opaque application transport-owner context.
  void* i2cUser = nullptr;

  /// @brief Monotonic millisecond source callback (optional).
  /// @note If null, health timestamps remain 0. EEPROM deadlines are driven by tick(nowMs).
  NowMsFn nowMs = nullptr;

  /// @brief Yielding wait source (optional for cooperative use).
  /// @note Required by ensurePrimaryCellConfiguration().
  WaitMsFn waitMs = nullptr;

  /// @brief User context passed to timing callbacks.
  void* timeUser = nullptr;

  /// @brief I2C address of RV3032-C7 (fixed at 0x51 on hardware)
  /// @note begin() validates this is exactly 0x51.
  uint8_t i2cAddress = 0x51;

  /// @brief I2C transaction timeout in milliseconds (default: 50ms)
  /// @note Passed to the transport callback. The library never configures the bus.
  ///       Valid range is 1..100 ms.
  uint32_t i2cTimeoutMs = 50;

  /// @brief Enable explicit generic EEPROM persistence (default: false)
  /// @note When true, EEPROM-backed config changes (backup PMU, clock output,
  ///       offset) may queue persistence after an explicit typed setter.
  ///       begin() never reads or mutates the device and never queues work.
  ///       When false, config is RAM-only (faster, saves EEPROM wear).
  ///       Typed setters that need read-modify-write return IN_PROGRESS and must
  ///       first be advanced through pollJob(). Their resulting active bytes are
  ///       then queued for tick()/pollEeprom() persistence.
  ///       Configuration EEPROM endurance is 10,000 writes at 3.0 V/25 C and
  ///       100 writes at 5.5 V/85 C. Compare before writing.
  bool enableEepromWrites = false;

  /// @brief EEPROM operation timeout in milliseconds (default: 100ms)
  /// @note Valid range is 10..250 ms when generic persistence is enabled.
  uint32_t eepromTimeoutMs = 100;

  /// @brief Consecutive failure threshold before transitioning to OFFLINE
  /// @note Default: 5. DEGRADED = [1, offlineThreshold-1], OFFLINE >= offlineThreshold.
  ///       Values below 1 are rejected by begin().
  uint8_t offlineThreshold = 5;
};

}  // namespace RV3032
