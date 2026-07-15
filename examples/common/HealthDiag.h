/**
 * @file HealthDiag.h
 * @brief Verbose health diagnostic helpers for RV3032 examples.
 *
 * Provides detailed logging of driver health state, counters, and
 * state transitions for debugging and monitoring.
 *
 * NOT part of the library API. Example-only.
 */

#pragma once

#include <Arduino.h>

#include "RV3032/RV3032.h"
#include "RV3032/Status.h"
#include "examples/common/Log.h"

namespace diag {

/**
 * @brief Convert DriverState enum to human-readable string.
 */
inline const char* stateToString(RV3032::DriverState state) {
  switch (state) {
    case RV3032::DriverState::UNINIT:   return "UNINIT";
    case RV3032::DriverState::READY:    return "READY";
    case RV3032::DriverState::DEGRADED: return "DEGRADED";
    case RV3032::DriverState::OFFLINE:  return "OFFLINE";
    default:                             return "UNKNOWN";
  }
}

/**
 * @brief Convert Err enum to human-readable string.
 */
inline const char* errToString(RV3032::Err err) {
  switch (err) {
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
    case RV3032::Err::I2C_NACK_ADDR:        return "I2C_NACK_ADDR";
    case RV3032::Err::I2C_NACK_DATA:        return "I2C_NACK_DATA";
    case RV3032::Err::I2C_TIMEOUT:          return "I2C_TIMEOUT";
    case RV3032::Err::I2C_BUS:              return "I2C_BUS";
    case RV3032::Err::EEPROM_VERIFY_FAILED: return "EEPROM_VERIFY_FAILED";
    case RV3032::Err::EEPROM_CLEANUP_FAILED:return "EEPROM_CLEANUP_FAILED";
    case RV3032::Err::PRIMARY_CELL_ALREADY_ATTEMPTED:
      return "PRIMARY_CELL_ALREADY_ATTEMPTED";
    case RV3032::Err::JOB_RESULT_UNAVAILABLE:return "JOB_RESULT_UNAVAILABLE";
    case RV3032::Err::INCOHERENT_DATA:      return "INCOHERENT_DATA";
    case RV3032::Err::CONFIGURATION_CLEANUP_FAILED:
      return "CONFIGURATION_CLEANUP_FAILED";
    case RV3032::Err::TRANSPORT_CONTRACT_VIOLATION:
      return "TRANSPORT_CONTRACT_VIOLATION";
    case RV3032::Err::INTERNAL_STATE_ERROR: return "INTERNAL_STATE_ERROR";
    default:                                 return "UNKNOWN";
  }
}

/**
 * @brief Get state color indicator for terminal output.
 */
inline const char* stateColor(RV3032::DriverState state) {
  switch (state) {
    case RV3032::DriverState::READY:    return LOG_COLOR_GREEN;
    case RV3032::DriverState::DEGRADED: return LOG_COLOR_YELLOW;
    case RV3032::DriverState::OFFLINE:  return LOG_COLOR_RED;
    case RV3032::DriverState::UNINIT:   return LOG_COLOR_GRAY;
    default:                             return LOG_COLOR_RESET;
  }
}

inline const char* colorReset() { return LOG_COLOR_RESET; }

inline const char* successRateColor(float pct) {
  if (pct >= 99.9f) return LOG_COLOR_GREEN;
  if (pct >= 80.0f) return LOG_COLOR_YELLOW;
  return LOG_COLOR_RED;
}

inline const char* failureCountColor(uint32_t failures) {
  if (failures == 0U) return LOG_COLOR_GREEN;
  if (failures < 3U) return LOG_COLOR_YELLOW;
  return LOG_COLOR_RED;
}

inline const char* successCountColor(uint32_t successes) {
  return (successes > 0U) ? LOG_COLOR_GREEN : LOG_COLOR_GRAY;
}

inline const char* totalFailureColor(uint32_t failures) {
  return (failures == 0U) ? LOG_COLOR_GREEN : LOG_COLOR_RED;
}

/**
 * @brief Print a compact one-line health summary.
 */
inline void printHealthOneLine(RV3032::RV3032& driver) {
  RV3032::DriverState st = driver.state();
  LOGI("Health: state=%s%s%s consecFail=%s%u%s ok=%s%lu%s fail=%s%lu%s",
       stateColor(st), stateToString(st), colorReset(),
       failureCountColor(driver.consecutiveFailures()), driver.consecutiveFailures(), colorReset(),
       successCountColor(driver.totalSuccess()), (unsigned long)driver.totalSuccess(), colorReset(),
       totalFailureColor(driver.totalFailures()), (unsigned long)driver.totalFailures(), colorReset());
}

/**
 * @brief Print detailed verbose health diagnostics.
 */
inline void printHealthVerbose(RV3032::RV3032& driver) {
  RV3032::DriverState st = driver.state();
  RV3032::Status lastErr = driver.lastError();
  uint32_t now = millis();

  const uint32_t totalSuccess = driver.totalSuccess();
  const uint32_t totalFailures = driver.totalFailures();
  uint32_t total = totalSuccess + totalFailures;
  float successRate = (total > 0) ? (100.0f * totalSuccess / total) : 0.0f;

  LOG_SERIAL.println();
  LOGI("=== Driver Health ===");
  LOGI("  State: %s%s%s", stateColor(st), stateToString(st), colorReset());
  LOGI("  Consecutive failures: %s%u%s",
       failureCountColor(driver.consecutiveFailures()),
       driver.consecutiveFailures(),
       colorReset());
  LOGI("  Total success: %s%lu%s",
       successCountColor(totalSuccess),
       (unsigned long)totalSuccess,
       colorReset());
  LOGI("  Total failures: %s%lu%s",
       totalFailureColor(totalFailures),
       (unsigned long)totalFailures,
       colorReset());
  LOGI("  Success rate: %s%.1f%%%s", successRateColor(successRate), successRate, colorReset());

  LOGI("=== Timestamps ===");

  uint32_t lastOk = driver.lastOkMs();
  uint32_t lastFail = driver.lastErrorMs();

  if (lastOk > 0) {
    LOGI("  Last success: %lu ms ago (at %lu ms)",
         (unsigned long)(now - lastOk), (unsigned long)lastOk);
  } else {
    LOGI("  Last success: never");
  }

  if (lastFail > 0) {
    LOGI("  Last failure: %lu ms ago (at %lu ms)",
         (unsigned long)(now - lastFail), (unsigned long)lastFail);
  } else {
    LOGI("  Last failure: never");
  }

  LOGI("=== Last Error ===");

  if (lastErr.ok()) {
    LOGI("  Last error: %snone%s", LOG_COLOR_GREEN, colorReset());
  } else {
    LOGI("  Code: %s%s%s", LOG_COLOR_RED, errToString(lastErr.code), colorReset());
    LOGI("  Detail: %ld", (long)lastErr.detail);
    LOGI("  Message: %s", lastErr.msg ? lastErr.msg : "(null)");
  }
  LOG_SERIAL.println();
}

/**
 * @brief Health snapshot for before/after comparison.
 */
struct HealthSnapshot {
  RV3032::DriverState state;
  uint8_t consecutiveFailures;
  uint32_t totalSuccess;
  uint32_t totalFailures;
  uint32_t timestamp;

  void capture(RV3032::RV3032& driver) {
    state = driver.state();
    consecutiveFailures = driver.consecutiveFailures();
    totalSuccess = driver.totalSuccess();
    totalFailures = driver.totalFailures();
    timestamp = millis();
  }
};

/**
 * @brief Compare two snapshots and print differences.
 */
inline void printHealthDiff(const HealthSnapshot& before, const HealthSnapshot& after) {
  bool changed = false;

  if (before.state != after.state) {
    LOGI("  State: %s%s%s -> %s%s%s",
         stateColor(before.state), stateToString(before.state), colorReset(),
         stateColor(after.state), stateToString(after.state), colorReset());
    changed = true;
  }

  if (before.consecutiveFailures != after.consecutiveFailures) {
    const bool improved = after.consecutiveFailures < before.consecutiveFailures;
    const char* color = improved ? LOG_COLOR_GREEN : LOG_COLOR_RED;
    LOGI("  ConsecFail: %s%u -> %u%s",
         color,
         before.consecutiveFailures,
         after.consecutiveFailures,
         colorReset());
    changed = true;
  }

  if (before.totalSuccess != after.totalSuccess) {
    LOGI("  TotalOK: %lu -> %s%lu (+%lu)%s",
         (unsigned long)before.totalSuccess,
         LOG_COLOR_GREEN,
         (unsigned long)after.totalSuccess,
         (unsigned long)(after.totalSuccess - before.totalSuccess),
         colorReset());
    changed = true;
  }

  if (before.totalFailures != after.totalFailures) {
    LOGI("  TotalFail: %lu -> %s%lu (+%lu)%s",
         (unsigned long)before.totalFailures,
         LOG_COLOR_RED,
         (unsigned long)after.totalFailures,
         (unsigned long)(after.totalFailures - before.totalFailures),
         colorReset());
    changed = true;
  }

  if (!changed) {
    LOGI("  (no changes)");
  }
}

}  // namespace diag
