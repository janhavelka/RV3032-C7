#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "RV3032/CommandTable.h"
#include "RV3032/Config.h"

namespace test_rv3032 {

struct Transfer {
  bool write = false;
  uint8_t reg = 0;
  uint8_t data[16] = {};
  uint8_t length = 0;
  uint32_t timeoutMs = 0;
  RV3032::Err statusCode = RV3032::Err::OK;
  int32_t statusDetail = 0;
  uint8_t physicalAttempts = 1;
  bool recoveryUsed = false;
  bool mayHaveCommitted = false;
};

struct FakeRv3032 {
  static constexpr size_t LOG_CAPACITY = 512;
  static constexpr size_t WAIT_LOG_CAPACITY = 512;

  uint8_t direct[0x50] = {};
  uint8_t activeConfig[11] = {};
  uint8_t persistent[0x2B] = {};
  uint32_t nowMs = 0;
  uint32_t callbackCount = 0;
  uint32_t nowCallCount = 0;
  uint32_t physicalAttemptCount = 0;
  uint32_t waitCount = 0;
  uint32_t waitedMs = 0;
  uint32_t waitRequests[WAIT_LOG_CAPACITY] = {};
  size_t waitLogCount = 0;
  bool waitLogOverflow = false;
  uint32_t failOrdinal = 0;
  uint32_t ignoreWriteOrdinal = 0;
  RV3032::Err failError = RV3032::Err::I2C_BUS;
  bool failCommandAfterCommit = false;
  bool failWriteAfterCommit = false;
  bool ignoreNextCommandAcked = false;
  bool earlyWait = false;
  uint32_t lateWaitExtraMs = 0;
  uint32_t lateWaitOrdinal = 0;
  uint32_t callbackDurationMs = 0;
  uint32_t lateCallbackOrdinal = 0;
  uint32_t lateCallbackExtraMs = 0;
  uint32_t advanceNowOnCall = 0;
  uint32_t advanceNowByMs = 0;
  uint32_t readOneDispatchNowCall = 0;
  uint32_t writeOneDispatchNowCall = 0;
  uint32_t readRecoveryRetryOrdinal = 0;
  uint32_t readRecoveryDelayMs = 0;
  uint32_t forceBusyAfterCallbackOrdinal = 0;
  uint32_t forcedBusyDurationMs = 0;
  bool lowVdd = false;
  bool logOverflow = false;
  bool protocolViolation = false;
  bool unsafeAccessStateAtCommand = false;
  uint16_t writeOneAttempts = 0;
  uint16_t readOneAttempts = 0;
  uint16_t updateAllAttempts = 0;
  uint16_t refreshAllAttempts = 0;
  uint16_t rejectedProtectedWrites = 0;
  bool passwordSequenceActive = false;
  bool passwordReferenceChanged = false;
  bool passwordNewCredentialEstablished = false;
  bool passwordCleanupObserved = false;
  bool passwordLockObserved = false;
  bool passwordReferenceWriteWhileEnabled = false;
  bool passwordCleanupBeforeNewCredential = false;
  bool passwordProtectionEnabledBeforeCleanup = false;
  bool passwordLockBeforeCleanup = false;
  Transfer log[LOG_CAPACITY] = {};
  size_t logCount = 0;

  uint32_t busyUntil = 0;
  bool initialBusy = false;
  uint8_t pendingCommand = 0;
  uint8_t pendingAddress = 0;
  uint8_t pendingData = 0;
  uint8_t passwordInput[4] = {};
  bool passwordAuthenticated = true;
  bool pendingWriteAuthorized = true;

  FakeRv3032() {
    persistent[0] = RV3032::cmd::PMU_BSM_LEVEL;
    resetFromPersistent();
  }

  void resetFromPersistent() {
    memset(direct, 0, sizeof(direct));
    memcpy(activeConfig, persistent, sizeof(activeConfig));
    direct[RV3032::cmd::REG_STATUS] = 0x03;
    direct[RV3032::cmd::REG_CONTROL1] = 0;
    direct[RV3032::cmd::REG_TEMP_LSB] = 0;
    memset(passwordInput, 0, sizeof(passwordInput));
    updatePasswordAuthentication();
    pendingWriteAuthorized = true;
    busyUntil = 0;
    initialBusy = false;
    pendingCommand = 0;
  }

  void dailyRefresh() {
    completeCommand();
    if ((direct[RV3032::cmd::REG_CONTROL1] &
         RV3032::cmd::CONTROL1_EERD_MASK) == 0) {
      memcpy(activeConfig, persistent, sizeof(activeConfig));
      updatePasswordAuthentication();
    }
  }

  bool passwordProtectionEnabled() const {
    return activeConfig[RV3032::cmd::REG_EEPROM_PW_ENABLE -
                        RV3032::cmd::CONFIG_EEPROM_START] == 0xFF;
  }

  bool passwordInputMatchesActiveReference() const {
    return memcmp(passwordInput,
                  &activeConfig[RV3032::cmd::REG_EEPROM_PASSWORD0 -
                                RV3032::cmd::CONFIG_EEPROM_START],
                  sizeof(passwordInput)) == 0;
  }

  void updatePasswordAuthentication() {
    passwordAuthenticated = !passwordProtectionEnabled() ||
        passwordInputMatchesActiveReference();
  }

  void setInitialBusy(uint32_t durationMs) {
    initialBusy = durationMs != 0;
    busyUntil = nowMs + durationMs;
    if (initialBusy) {
      direct[RV3032::cmd::REG_TEMP_LSB] |= RV3032::cmd::EEPROM_BUSY_MASK;
    }
  }

  void applyPostCallbackFault() {
    if (forceBusyAfterCallbackOrdinal != 0 &&
        callbackCount == forceBusyAfterCallbackOrdinal) {
      setInitialBusy(forcedBusyDurationMs);
    }
  }

  void setCalendar(uint16_t year, uint8_t month, uint8_t day,
                   uint8_t hour, uint8_t minute, uint8_t second,
                   uint8_t weekday) {
    direct[RV3032::cmd::REG_SECONDS] = bcd(second);
    direct[RV3032::cmd::REG_MINUTES] = bcd(minute);
    direct[RV3032::cmd::REG_HOURS] = bcd(hour);
    direct[RV3032::cmd::REG_WEEKDAY] = bcd(weekday);
    direct[RV3032::cmd::REG_DATE] = bcd(day);
    direct[RV3032::cmd::REG_MONTH] = bcd(month);
    direct[RV3032::cmd::REG_YEAR] = bcd(static_cast<uint8_t>(year - 2000));
  }

  static uint8_t bcd(uint8_t value) {
    return static_cast<uint8_t>(((value / 10U) << 4) | (value % 10U));
  }

  void completeCommand() {
    if (static_cast<int32_t>(nowMs - busyUntil) < 0) {
      return;
    }
    if (initialBusy) {
      initialBusy = false;
      direct[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(
          direct[RV3032::cmd::REG_TEMP_LSB] & ~RV3032::cmd::EEPROM_BUSY_MASK);
    }
    if (pendingCommand == 0) return;
    direct[RV3032::cmd::REG_TEMP_LSB] = static_cast<uint8_t>(
        direct[RV3032::cmd::REG_TEMP_LSB] & ~RV3032::cmd::EEPROM_BUSY_MASK);
    if (pendingAddress < RV3032::cmd::CONFIG_EEPROM_START ||
        pendingAddress > RV3032::cmd::USER_EEPROM_END) {
      direct[RV3032::cmd::REG_TEMP_LSB] |= RV3032::cmd::EEPROM_EEF_MASK;
    } else if (pendingCommand == RV3032::cmd::EEPROM_CMD_READ_ONE) {
      direct[RV3032::cmd::REG_EE_DATA] =
          persistent[pendingAddress - RV3032::cmd::CONFIG_EEPROM_START];
    } else if (pendingCommand == RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
      if (lowVdd || !pendingWriteAuthorized) {
        direct[RV3032::cmd::REG_TEMP_LSB] |= RV3032::cmd::EEPROM_EEF_MASK;
      } else {
        persistent[pendingAddress - RV3032::cmd::CONFIG_EEPROM_START] =
            pendingData;
      }
    }
    pendingCommand = 0;
  }

  uint8_t readByte(uint8_t reg) {
    completeCommand();
    if (reg == RV3032::cmd::REG_EE_COMMAND ||
        (reg >= RV3032::cmd::REG_PASSWORD0 &&
         reg <= RV3032::cmd::REG_PASSWORD3) ||
        (reg >= RV3032::cmd::REG_EEPROM_PASSWORD0 &&
         reg <= RV3032::cmd::REG_EEPROM_PW_ENABLE)) {
      return 0;
    }
    if (reg < sizeof(direct)) return direct[reg];
    if (reg >= RV3032::cmd::CONFIG_EEPROM_START &&
        reg <= RV3032::cmd::CONFIG_EEPROM_END) {
      return activeConfig[reg - RV3032::cmd::CONFIG_EEPROM_START];
    }
    return 0;
  }

  void writeByte(uint8_t reg, uint8_t value) {
    completeCommand();
    if (reg >= RV3032::cmd::REG_PASSWORD0 &&
        reg <= RV3032::cmd::REG_PASSWORD3) {
      const uint8_t index = static_cast<uint8_t>(
          reg - RV3032::cmd::REG_PASSWORD0);
      passwordInput[index] = value;
      if (reg == RV3032::cmd::REG_PASSWORD3) {
        updatePasswordAuthentication();
        if (passwordSequenceActive) {
          if (passwordInputMatchesActiveReference()) {
            passwordNewCredentialEstablished = true;
          } else {
            if (!passwordCleanupObserved) {
              passwordLockBeforeCleanup = true;
            }
            passwordLockObserved = true;
            passwordSequenceActive = false;
          }
        }
      }
      return;
    }
    if (passwordProtectionEnabled() && !passwordAuthenticated) {
      ++rejectedProtectedWrites;
      return;
    }
    if (reg == RV3032::cmd::REG_STATUS) {
      direct[reg] = static_cast<uint8_t>(direct[reg] & value & 0x3Fu);
      return;
    }
    if (reg == RV3032::cmd::REG_TEMP_LSB) {
      const uint8_t preserveTemperature = static_cast<uint8_t>(
          direct[reg] & 0xF0u);
      const uint8_t preserveBusy = static_cast<uint8_t>(
          direct[reg] & RV3032::cmd::EEPROM_BUSY_MASK);
      uint8_t flags = static_cast<uint8_t>(direct[reg] & 0x0Bu);
      flags = static_cast<uint8_t>(flags & value);
      direct[reg] = static_cast<uint8_t>(
          preserveTemperature | preserveBusy | flags);
      return;
    }
    if (reg == RV3032::cmd::REG_CONTROL1) {
      if (passwordSequenceActive) {
        if (passwordReferenceChanged &&
            !passwordNewCredentialEstablished) {
          passwordCleanupBeforeNewCredential = true;
        }
        passwordCleanupObserved = true;
      }
      direct[reg] = static_cast<uint8_t>(value &
          RV3032::cmd::CONTROL1_IMPLEMENTED_MASK);
      return;
    }
    if (reg == RV3032::cmd::REG_CONTROL2) {
      direct[reg] = static_cast<uint8_t>(value &
          RV3032::cmd::CONTROL2_IMPLEMENTED_MASK);
      if ((direct[reg] & (1u << RV3032::cmd::CTRL2_STOP_BIT)) != 0) {
        direct[RV3032::cmd::REG_100TH_SECONDS] = 0;
      }
      return;
    }
    if (reg == RV3032::cmd::REG_CONTROL3) {
      direct[reg] = static_cast<uint8_t>(value &
          RV3032::cmd::CONTROL3_IMPLEMENTED_MASK);
      return;
    }
    if (reg == RV3032::cmd::REG_TS_CONTROL) {
      const bool eviResetWasSet =
          (direct[reg] &
           (1u << RV3032::cmd::TS_EVI_RESET_BIT)) != 0;
      if ((value & (1u << RV3032::cmd::TS_TLOW_RESET_BIT)) != 0) {
        memset(&direct[RV3032::cmd::REG_TS_TLOW_COUNT], 0, 7);
        direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
            direct[RV3032::cmd::REG_STATUS] &
            ~(1u << RV3032::cmd::STATUS_TLF_BIT));
      }
      if ((value & (1u << RV3032::cmd::TS_THIGH_RESET_BIT)) != 0) {
        memset(&direct[RV3032::cmd::REG_TS_THIGH_COUNT], 0, 7);
        direct[RV3032::cmd::REG_STATUS] = static_cast<uint8_t>(
            direct[RV3032::cmd::REG_STATUS] &
            ~(1u << RV3032::cmd::STATUS_THF_BIT));
      }
      if ((value & (1u << RV3032::cmd::TS_EVI_RESET_BIT)) != 0 &&
          !eviResetWasSet) {
        memset(&direct[RV3032::cmd::REG_TS_EVI_COUNT], 0, 8);
      }
      direct[reg] = static_cast<uint8_t>(
          value & RV3032::cmd::TS_CONTROL_READBACK_MASK);
      return;
    }
    if (reg == RV3032::cmd::REG_EVI_CONTROL) {
      direct[reg] = static_cast<uint8_t>(value &
          RV3032::cmd::EVI_IMPLEMENTED_MASK);
      return;
    }
    if (reg == RV3032::cmd::REG_TIMER_HIGH) {
      direct[reg] = static_cast<uint8_t>(value & 0x0Fu);
      return;
    }
    if (reg == RV3032::cmd::REG_EE_COMMAND) {
      const uint8_t bsm = static_cast<uint8_t>(
          activeConfig[0] & RV3032::cmd::PMU_BSM_MASK);
      if (bsm != RV3032::cmd::PMU_BSM_DISABLED ||
          (activeConfig[0] & RV3032::cmd::PMU_TCM_MASK) != 0) {
        unsafeAccessStateAtCommand = true;
      }
      if ((direct[RV3032::cmd::REG_CONTROL1] &
           RV3032::cmd::CONTROL1_EERD_MASK) == 0 ||
          (bsm != 0 && bsm != RV3032::cmd::PMU_BSM_DISABLED_ALT)) {
        protocolViolation = true;
        return;
      }
      if ((direct[RV3032::cmd::REG_TEMP_LSB] &
           RV3032::cmd::EEPROM_BUSY_MASK) != 0) {
        // The vendor defines commands presented while EEbusy as ignored.
        return;
      }
      if (ignoreNextCommandAcked) {
        ignoreNextCommandAcked = false;
        protocolViolation = true;
        return;
      }
      if (value != RV3032::cmd::EEPROM_CMD_READ_ONE &&
          value != RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
        if (value == RV3032::cmd::EEPROM_CMD_UPDATE_ALL) {
          ++updateAllAttempts;
        } else if (value == RV3032::cmd::EEPROM_CMD_REFRESH_ALL) {
          ++refreshAllAttempts;
        }
        protocolViolation = true;
        return;
      }
      if (value == RV3032::cmd::EEPROM_CMD_WRITE_ONE &&
          direct[RV3032::cmd::REG_EE_ADDRESS] >=
              RV3032::cmd::REG_EEPROM_PASSWORD0 &&
          direct[RV3032::cmd::REG_EE_ADDRESS] <=
              RV3032::cmd::REG_EEPROM_PASSWORD3 &&
          passwordProtectionEnabled()) {
        passwordReferenceWriteWhileEnabled = true;
      }
      pendingAddress = direct[RV3032::cmd::REG_EE_ADDRESS];
      pendingData = direct[RV3032::cmd::REG_EE_DATA];
      pendingWriteAuthorized = passwordAuthenticated ||
          !passwordProtectionEnabled();
      pendingCommand = value;
      if (value == RV3032::cmd::EEPROM_CMD_READ_ONE) {
        ++readOneAttempts;
        busyUntil = nowMs + 1;
      } else if (value == RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
        ++writeOneAttempts;
        busyUntil = nowMs + 10;
      }
      direct[RV3032::cmd::REG_TEMP_LSB] |= RV3032::cmd::EEPROM_BUSY_MASK;
      return;
    }
    if (reg < sizeof(direct)) {
      if (reg == RV3032::cmd::REG_SECONDS) {
        direct[RV3032::cmd::REG_100TH_SECONDS] = 0;
      }
      direct[reg] = value;
      return;
    }
    if (reg >= RV3032::cmd::CONFIG_EEPROM_START &&
        reg <= RV3032::cmd::CONFIG_EEPROM_END) {
      if (reg == RV3032::cmd::REG_ACTIVE_PMU && passwordSequenceActive) {
        if (passwordReferenceChanged &&
            !passwordNewCredentialEstablished) {
          passwordCleanupBeforeNewCredential = true;
        }
        passwordCleanupObserved = true;
      }
      if (reg >= RV3032::cmd::REG_EEPROM_PASSWORD0 &&
          reg <= RV3032::cmd::REG_EEPROM_PASSWORD3 &&
          passwordProtectionEnabled()) {
        passwordReferenceWriteWhileEnabled = true;
      }
      if (reg == RV3032::cmd::REG_EEPROM_PW_ENABLE) {
        if (value != 0xFF) {
          passwordSequenceActive = true;
          passwordReferenceChanged = false;
          passwordNewCredentialEstablished = false;
          passwordCleanupObserved = false;
        } else if (passwordSequenceActive && !passwordCleanupObserved) {
          passwordProtectionEnabledBeforeCleanup = true;
        }
      }
      const uint8_t index = static_cast<uint8_t>(
          reg - RV3032::cmd::CONFIG_EEPROM_START);
      activeConfig[index] = index == 0
          ? static_cast<uint8_t>(value & RV3032::cmd::PMU_IMPLEMENTED_MASK)
          : value;
      if (reg >= RV3032::cmd::REG_EEPROM_PASSWORD0 &&
          reg <= RV3032::cmd::REG_EEPROM_PW_ENABLE) {
        if (reg >= RV3032::cmd::REG_EEPROM_PASSWORD0 &&
            reg <= RV3032::cmd::REG_EEPROM_PASSWORD3) {
          passwordReferenceChanged = true;
        }
        updatePasswordAuthentication();
      }
    }
  }

  Transfer* record(bool write, uint8_t reg, const uint8_t* data, size_t len,
                   uint32_t timeoutMs) {
    if (logCount >= LOG_CAPACITY) {
      logOverflow = true;
      return nullptr;
    }
    Transfer& item = log[logCount++];
    item.write = write;
    item.reg = reg;
    item.length = static_cast<uint8_t>(len > sizeof(item.data)
                                          ? sizeof(item.data) : len);
    item.timeoutMs = timeoutMs;
    if (data != nullptr && item.length > 0) {
      memcpy(item.data, data, item.length);
    }
    return &item;
  }

  void advanceCallbackClock(uint32_t timeoutMs) {
    uint32_t elapsed = callbackDurationMs;
    if (elapsed > timeoutMs) elapsed = timeoutMs;
    if (lateCallbackOrdinal != 0 && callbackCount == lateCallbackOrdinal) {
      elapsed += lateCallbackExtraMs;
    }
    nowMs += elapsed;
    completeCommand();
  }

  static RV3032::Status writeCallback(uint8_t address, const uint8_t* data,
                                      size_t len, uint32_t timeoutMs,
                                      void* user) {
    FakeRv3032& fake = *static_cast<FakeRv3032*>(user);
    ++fake.callbackCount;
    ++fake.physicalAttemptCount;
    if (address != RV3032::cmd::I2C_ADDR_7BIT || data == nullptr || len < 2) {
      return RV3032::Status::Error(RV3032::Err::I2C_ERROR, "bad fake write");
    }
    Transfer* transfer = fake.record(true, data[0], &data[1], len - 1,
                                     timeoutMs);
    const bool fail = fake.failOrdinal != 0 &&
                      fake.callbackCount == fake.failOrdinal;
    const bool ignore = fake.ignoreWriteOrdinal != 0 &&
                        fake.callbackCount == fake.ignoreWriteOrdinal;
    const bool command = data[0] == RV3032::cmd::REG_EE_COMMAND;
    if (command && len == 2) {
      if (data[1] == RV3032::cmd::EEPROM_CMD_READ_ONE) {
        fake.readOneDispatchNowCall = fake.nowCallCount;
      } else if (data[1] == RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
        fake.writeOneDispatchNowCall = fake.nowCallCount;
      }
    }
    const bool commitAfterFailure = command
        ? fake.failCommandAfterCommit
        : fake.failWriteAfterCommit;
    const bool applyWrite = !ignore && (!fail || commitAfterFailure);
    if (applyWrite && !command) {
      for (size_t i = 1; i < len; ++i) {
        fake.writeByte(static_cast<uint8_t>(data[0] + i - 1U), data[i]);
      }
    }
    fake.advanceCallbackClock(timeoutMs);
    // EECMD is presented at the end of the transport callback. This makes the
    // fake enforce vendor waits from command completion, not callback entry.
    if (applyWrite && command) {
      for (size_t i = 1; i < len; ++i) {
        fake.writeByte(static_cast<uint8_t>(data[0] + i - 1U), data[i]);
      }
    }
    fake.applyPostCallbackFault();
    if (fail) {
      if (transfer != nullptr) {
        transfer->statusCode = fake.failError;
        transfer->statusDetail = static_cast<int32_t>(fake.callbackCount);
        transfer->mayHaveCommitted = commitAfterFailure;
      }
      return RV3032::Status::Error(fake.failError,
                                   "injected callback failure",
                                   static_cast<int32_t>(fake.callbackCount));
    }
    return RV3032::Status::Ok();
  }

  static RV3032::Status readCallback(uint8_t address, const uint8_t* tx,
                                     size_t txLen, uint8_t* rx, size_t rxLen,
                                     uint32_t timeoutMs, void* user) {
    FakeRv3032& fake = *static_cast<FakeRv3032*>(user);
    ++fake.callbackCount;
    ++fake.physicalAttemptCount;
    if (address != RV3032::cmd::I2C_ADDR_7BIT || tx == nullptr || txLen != 1 ||
        rx == nullptr || rxLen == 0) {
      return RV3032::Status::Error(RV3032::Err::I2C_ERROR, "bad fake read");
    }
    Transfer* transfer = fake.record(false, tx[0], nullptr, rxLen, timeoutMs);
    if (fake.readRecoveryRetryOrdinal != 0 &&
        fake.callbackCount == fake.readRecoveryRetryOrdinal) {
      ++fake.physicalAttemptCount;
      fake.nowMs += fake.readRecoveryDelayMs;
      if (transfer != nullptr) {
        transfer->physicalAttempts = 2;
        transfer->recoveryUsed = true;
      }
    }
    fake.advanceCallbackClock(timeoutMs);
    if (fake.failOrdinal != 0 && fake.callbackCount == fake.failOrdinal) {
      fake.applyPostCallbackFault();
      if (transfer != nullptr) {
        transfer->statusCode = fake.failError;
        transfer->statusDetail = static_cast<int32_t>(fake.callbackCount);
      }
      return RV3032::Status::Error(fake.failError,
                                   "injected callback failure",
                                   static_cast<int32_t>(fake.callbackCount));
    }
    for (size_t i = 0; i < rxLen; ++i) {
      rx[i] = fake.readByte(static_cast<uint8_t>(tx[0] + i));
    }
    if (transfer != nullptr) {
      transfer->length = static_cast<uint8_t>(
          rxLen > sizeof(transfer->data) ? sizeof(transfer->data) : rxLen);
      memcpy(transfer->data, rx, transfer->length);
    }
    fake.applyPostCallbackFault();
    return RV3032::Status::Ok();
  }

  static uint32_t nowCallback(void* user) {
    FakeRv3032& fake = *static_cast<FakeRv3032*>(user);
    ++fake.nowCallCount;
    if (fake.advanceNowOnCall != 0 &&
        fake.nowCallCount == fake.advanceNowOnCall) {
      fake.nowMs += fake.advanceNowByMs;
    }
    return fake.nowMs;
  }

  static void waitCallback(uint32_t delayMs, void* user) {
    FakeRv3032& fake = *static_cast<FakeRv3032*>(user);
    ++fake.waitCount;
    if (fake.waitLogCount < WAIT_LOG_CAPACITY) {
      fake.waitRequests[fake.waitLogCount++] = delayMs;
    } else {
      fake.waitLogOverflow = true;
      fake.logOverflow = true;
    }
    const bool lateThisWait = fake.lateWaitExtraMs != 0 &&
        (fake.lateWaitOrdinal == 0 ||
         fake.waitCount == fake.lateWaitOrdinal);
    const uint32_t actual = fake.earlyWait && delayMs > 0
        ? delayMs - 1U
        : delayMs + (lateThisWait ? fake.lateWaitExtraMs : 0U);
    fake.waitedMs += actual;
    fake.nowMs += actual;
    fake.completeCommand();
  }

  RV3032::Config config(bool writes = false, bool withWait = true) {
    RV3032::Config cfg{};
    cfg.i2cWrite = writeCallback;
    cfg.i2cWriteRead = readCallback;
    cfg.i2cUser = this;
    cfg.nowMs = nowCallback;
    cfg.waitMs = withWait ? waitCallback : nullptr;
    cfg.timeUser = this;
    cfg.i2cTimeoutMs = 5;
    cfg.enableEepromWrites = writes;
    cfg.eepromTimeoutMs = 100;
    cfg.offlineThreshold = 3;
    return cfg;
  }
};

}  // namespace test_rv3032
