/**
 * @file Config.h
 * @brief Configuration for RV3032-C7 RTC library
 */

#pragma once

#include <stdint.h>

// Forward declaration for TwoWire
class TwoWire;

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

/**
 * @struct Config
 * @brief RTC configuration parameters
 * 
 * All hardware resources are application-provided. Library does not
 * define any pin defaults - board-specific values must be passed by user.
 */
struct Config {
  /// @brief I2C interface pointer. Application must initialize Wire before calling begin().
  /// @note Application-provided. Library does not initialize I2C bus.
  TwoWire* wire = nullptr;

  /// @brief I2C address of RV3032-C7 (default: 0x51, not user-configurable on hardware)
  uint8_t i2cAddress = 0x51;

  /// @brief I2C transaction timeout in milliseconds (default: 50ms)
  /// @note Applied in begin() via TwoWire::setTimeOut(). Affects the shared I2C bus.
  ///       Set to a value > 0 to bound bus stalls.
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
};

}  // namespace RV3032
