/**
 * @file Status.h
 * @brief Error status codes for RV3032-C7 RTC library
 */

#pragma once

#include <stdint.h>

namespace RV3032 {

/**
 * @enum Err
 * @brief Error codes returned by library operations
 */
enum class Err : uint8_t {
  OK = 0,                ///< Operation successful
  NOT_INITIALIZED,       ///< Library not initialized (call begin() first)
  INVALID_CONFIG,        ///< Invalid configuration parameter
  I2C_ERROR,             ///< I2C communication failure
  TIMEOUT,               ///< Operation timed out (EEPROM write, etc.)
  INVALID_PARAM,         ///< Invalid parameter value
  INVALID_DATETIME,      ///< Invalid date/time value
  DEVICE_NOT_FOUND,      ///< RTC device not responding on I2C bus
  EEPROM_WRITE_FAILED,   ///< EEPROM update operation failed
  REGISTER_READ_FAILED,  ///< Register read operation failed
  REGISTER_WRITE_FAILED, ///< Register write operation failed
  QUEUE_FULL,            ///< EEPROM write queue is full (too many pending writes)
  BUSY,                  ///< Operation deferred because device is busy
  IN_PROGRESS,           ///< Operation scheduled; call tick() to complete
  I2C_NACK_ADDR,         ///< I2C address not acknowledged
  I2C_NACK_DATA,         ///< I2C data byte not acknowledged
  I2C_TIMEOUT,           ///< I2C transaction timed out
  I2C_BUS                ///< I2C bus error (arbitration lost, etc.)
};

/**
 * @struct Status
 * @brief Status result from library operations
 * 
 * All library functions return Status to indicate success or failure.
 * Check status.ok() to determine if operation succeeded.
 */
struct Status {
  Err code = Err::OK;      ///< Error category
  int32_t detail = 0;      ///< I2C error code or vendor-specific detail
  const char* msg = "";    ///< Static error message (never heap-allocated)

  /**
   * @brief Default constructor
   */
  constexpr Status() = default;

  /**
   * @brief Constructor with all fields
   */
  constexpr Status(Err c, int32_t d, const char* m) : code(c), detail(d), msg(m) {}

  /**
   * @brief Check if operation succeeded
   * @return true if code == Err::OK
   */
  constexpr bool ok() const { return code == Err::OK; }

  /**
   * @brief Check if status matches the requested error code.
   * @param expected Error code to compare against.
   * @return true if code == expected
   */
  constexpr bool is(Err expected) const { return code == expected; }

  /**
   * @brief Check if operation is in progress (not a failure)
   * @return true if code == Err::IN_PROGRESS
   */
  constexpr bool inProgress() const { return code == Err::IN_PROGRESS; }

  /**
   * @brief Implicit truthiness for success checks.
   * @return true if operation succeeded
   */
  explicit constexpr operator bool() const { return ok(); }

  /**
   * @brief Create successful status
   */
  static constexpr Status Ok() { return Status{Err::OK, 0, "OK"}; }

  /**
   * @brief Create error status
   * @param err Error code
   * @param message Static error message
   * @param detailCode Optional detail code
   */
  static constexpr Status Error(Err err, const char* message, int32_t detailCode = 0) {
    return Status{err, detailCode, message};
  }
};

}  // namespace RV3032
