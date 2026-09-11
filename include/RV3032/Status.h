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
  NOT_INITIALIZED = 1,       ///< Library not initialized (call begin() first)
  INVALID_CONFIG = 2,        ///< Invalid configuration parameter
  I2C_ERROR = 3,             ///< I2C communication failure
  TIMEOUT = 4,               ///< Operation timed out (EEPROM write, etc.)
  INVALID_PARAM = 5,         ///< Invalid parameter value
  INVALID_DATETIME = 6,      ///< Invalid date/time value
  DEVICE_NOT_FOUND = 7,      ///< RTC device not responding on I2C bus
  EEPROM_WRITE_FAILED = 8,   ///< EEPROM update operation failed
  REGISTER_READ_FAILED = 9,  ///< Reserved legacy register-read failure category
  REGISTER_WRITE_FAILED = 10, ///< Register configuration/readback verification failed
  QUEUE_FULL = 11,           ///< EEPROM write queue is full (too many pending writes)
  BUSY = 12,                 ///< Operation not admitted or advanced because driver, guard, or hardware is busy
  IN_PROGRESS = 13,          ///< Work admitted; call pollJob(), pollEeprom(), or tick() as documented
  I2C_NACK_ADDR = 14,        ///< I2C address not acknowledged
  I2C_NACK_DATA = 15,        ///< I2C data byte not acknowledged
  I2C_TIMEOUT = 16,          ///< I2C transaction timed out
  I2C_BUS = 17,              ///< I2C bus error (arbitration lost, etc.)
  EEPROM_VERIFY_FAILED = 18,   ///< Readback or semantic verification did not match
  EEPROM_CLEANUP_FAILED = 19,  ///< Safe EEPROM access-state cleanup failed
  PRIMARY_CELL_ALREADY_ATTEMPTED = 20, ///< Ensure already called this lifecycle
  JOB_RESULT_UNAVAILABLE = 21, ///< Requested typed job result is unavailable
  INCOHERENT_DATA = 22,        ///< Repeated hardware samples did not agree
  CONFIGURATION_CLEANUP_FAILED = 23, ///< Staged configuration cleanup could not be proven
  TRANSPORT_CONTRACT_VIOLATION = 24, ///< Transport callback returned an illegal status code
  INTERNAL_STATE_ERROR = 25    ///< An impossible internal state was reached
};

/**
 * @struct Status
 * @brief Status result from library operations
 * 
 * All fallible operations return Status. ok() reports only Err::OK;
 * IN_PROGRESS means admitted or continuing work and must be advanced through
 * the operation's documented polling surface. BUSY is not an admission token.
 * A terminal job error may accompany a typed report containing partial progress
 * or durable-write evidence; inspect that report before deciding product policy.
 * No Status owns its message: msg must point to static storage.
 */
struct Status {
  Err code = Err::OK;      ///< Error category
  int32_t detail = 0;      ///< Context-specific detail; interpret with code and calling API
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
   * @brief Explicit boolean conversion for terminal success checks.
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
