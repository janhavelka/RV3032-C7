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

  /// @brief User context passed to I2C callbacks (e.g., TwoWire*).
  void* i2cUser = nullptr;

  /// @brief I2C address of RV3032-C7 (default: 0x51, not user-configurable on hardware)
  /// @note Valid 7-bit range: 0x08-0x77.
  uint8_t i2cAddress = 0x51;

  /// @brief I2C transaction timeout in milliseconds (default: 50ms)
  /// @note Passed to the transport callback. The library never configures the bus.
  ///       Must be >= 50ms when enableEepromWrites is true.
  uint32_t i2cTimeoutMs = 50;

  /// @brief Battery backup switching mode (default: Level)
  /// @note Applied during begin(). Off=no backup, Level=threshold, Direct=immediate
  BackupSwitchMode backupMode = BackupSwitchMode::Level;

  /// @brief Enable automatic EEPROM write for persistent config changes (default: false)
  /// @note When true, config changes (clock output, offset) persist across power loss.
  ///       When false, config is RAM-only (faster, saves EEPROM wear).
  ///       When true, persistence is asynchronous; methods may return IN_PROGRESS
  ///       until tick() completes the EEPROM update.
  ///       EEPROM has ~100k write endurance - use sparingly in production.
  bool enableEepromWrites = false;

  /// @brief EEPROM write timeout in milliseconds (default: 200ms)
  /// @note RV3032 EEPROM writes take several milliseconds. This is the max wait time.
  ///       Must be > 0 when enableEepromWrites is true.
  uint32_t eepromTimeoutMs = 200;

  /// @brief Consecutive failure threshold before transitioning to OFFLINE
  /// @note Default: 5. DEGRADED = [1, offlineThreshold-1], OFFLINE >= offlineThreshold.
  ///       Values < 1 are clamped to 1 during begin().
  uint8_t offlineThreshold = 5;
};

}  // namespace RV3032
