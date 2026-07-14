/**
 * @file RV3032.cpp
 * @brief Implementation of RV-3032-C7 RTC driver
 */

#include "RV3032/RV3032.h"
#include "RV3032/CommandTable.h"
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace RV3032 {

// Implementation-only constants (not part of public API)
namespace {
constexpr uint32_t EEPROM_READ_SETTLE_MS = 1;
constexpr uint32_t EEPROM_WRITE_SETTLE_MS = 10;
constexpr uint32_t EEPROM_BUSY_POLL_INTERVAL_MS = 1;
constexpr uint32_t EEPROM_READY_TIMEOUT_MS = 250;
constexpr uint32_t EEPROM_READ_TIMEOUT_MS = 25;
constexpr uint32_t EEPROM_WRITE_TIMEOUT_MS = 100;
constexpr uint32_t EEPROM_CLEANUP_READY_TIMEOUT_MS = 250;
constexpr uint32_t PRIMARY_CELL_OPERATION_TIMEOUT_MS = 1000;
constexpr uint32_t PRIMARY_CELL_WRITE_START_CUTOFF_MS = 500;
constexpr uint32_t PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS = 300;
constexpr uint32_t PRIMARY_CELL_TRANSFER_TIMEOUT_MS = 5;
constexpr uint8_t PRIMARY_CELL_WRITE_COMMAND_CAP = 1;
constexpr uint16_t EEPROM_READY_CHECK_CAP = 256;
constexpr uint16_t EEPROM_READ_CHECK_CAP = 32;
constexpr uint16_t EEPROM_WRITE_CHECK_CAP = 101;
constexpr uint16_t EEPROM_CLEANUP_CHECK_CAP = 256;
constexpr uint8_t kMaxBackupSwitchMode = static_cast<uint8_t>(BackupSwitchMode::Direct);
constexpr uint8_t kMaxTimerFrequency = static_cast<uint8_t>(TimerFrequency::Hz1_60);
constexpr uint8_t kMaxClkoutFrequency = static_cast<uint8_t>(ClkoutFrequency::Hz1);
constexpr uint8_t kMaxEviDebounce = static_cast<uint8_t>(EviDebounce::Hz8);
constexpr uint32_t kEpoch2000 = 946684800UL;  // 2000-01-01 00:00:00 UTC
constexpr float kOffsetPpmPerStep =
    1000000.0f / (32768.0f * 128.0f);
constexpr uint8_t kUserRamSize =
    static_cast<uint8_t>(cmd::REG_USER_RAM_END - cmd::REG_USER_RAM_START + 1U);

int16_t decodeTemperatureRaw(const uint8_t* bytes) {
  const uint16_t raw = static_cast<uint16_t>(
      (static_cast<uint16_t>(bytes[1]) << 4) | (bytes[0] >> 4));
  return (raw & 0x0800u) != 0
      ? static_cast<int16_t>(raw | 0xF000u)
      : static_cast<int16_t>(raw);
}

/// @brief Check if deadline has passed, with wraparound-safe comparison.
/// Uses signed arithmetic to handle 32-bit millisecond wraparound (~49 days).
bool hasDeadlinePassed(uint32_t now_ms, uint32_t deadline_ms) {
  return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

Status mapPresenceError(const Status& st) {
  if (st.code == Err::I2C_NACK_ADDR) {
    return Status::Error(Err::DEVICE_NOT_FOUND, "RTC not responding", st.detail);
  }
  return st;
}

bool shouldTrackHealth(const Status& st) {
  switch (st.code) {
    case Err::INVALID_CONFIG:
    case Err::INVALID_PARAM:
    case Err::NOT_INITIALIZED:
    case Err::INVALID_DATETIME:
      return false;
    default:
      return true;
  }
}

bool isKnownRegisterAddress(uint8_t reg) {
  return reg <= cmd::REG_USER_RAM_END ||
         (reg >= cmd::CONFIG_EEPROM_START && reg <= cmd::CONFIG_EEPROM_END);
}

bool isPublicReadableAddress(uint8_t reg) {
  return reg <= cmd::REG_TS_EVI_YEAR ||
         (reg >= cmd::REG_USER_RAM_START && reg <= cmd::REG_USER_RAM_END) ||
         (reg >= cmd::REG_ACTIVE_PMU && reg <= cmd::REG_ACTIVE_TREFERENCE1);
}

bool isRawWritableAddress(uint8_t reg) {
  return reg == cmd::REG_TLOW_THRESHOLD || reg == cmd::REG_THIGH_THRESHOLD ||
         (reg >= cmd::REG_USER_RAM_START && reg <= cmd::REG_USER_RAM_END);
}

bool isKnownRegisterBlock(uint8_t reg, size_t len) {
  if (len == 0 || len > 256) {
    return false;
  }

  const uint16_t start = reg;
  const uint16_t end = static_cast<uint16_t>(start + len - 1);
  if (end > 0xFFu) {
    return false;
  }

  for (uint16_t addr = start; addr <= end; ++addr) {
    if (!isKnownRegisterAddress(static_cast<uint8_t>(addr))) {
      return false;
    }
  }
  return true;
}

bool timestampBlock(TimestampSource source, uint8_t& startReg, size_t& len) {
  switch (source) {
    case TimestampSource::TLow:
      startReg = cmd::REG_TS_TLOW_COUNT;
      len = 7;
      return true;
    case TimestampSource::THigh:
      startReg = cmd::REG_TS_THIGH_COUNT;
      len = 7;
      return true;
    case TimestampSource::Evi:
      startReg = cmd::REG_TS_EVI_COUNT;
      len = 8;
      return true;
    default:
      return false;
  }
}

bool timestampResetBit(TimestampSource source, uint8_t& bit) {
  switch (source) {
    case TimestampSource::TLow:
      bit = cmd::TS_TLOW_RESET_BIT;
      return true;
    case TimestampSource::THigh:
      bit = cmd::TS_THIGH_RESET_BIT;
      return true;
    case TimestampSource::Evi:
      bit = cmd::TS_EVI_RESET_BIT;
      return true;
    default:
      return false;
  }
}
}  // namespace

// ===== Lifecycle Functions =====

Status RV3032::begin(const Config& config) {
  if (_initialized) {
    return Status::Error(Err::BUSY, "Driver already initialized");
  }
  if (!config.i2cWrite || !config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C transport callbacks are null");
  }
  if (config.i2cAddress != cmd::I2C_ADDR_7BIT) {
    return Status::Error(Err::INVALID_CONFIG, "RV3032 I2C address must be 0x51");
  }
  if (config.i2cTimeoutMs == 0 || config.i2cTimeoutMs > 100) {
    return Status::Error(Err::INVALID_CONFIG, "I2C timeout must be 1..100 ms");
  }
  if (config.enableEepromWrites &&
      (config.eepromTimeoutMs < 10 || config.eepromTimeoutMs > 250)) {
    return Status::Error(Err::INVALID_CONFIG, "EEPROM timeout must be 10..250 ms");
  }
  if (config.offlineThreshold < 1) {
    return Status::Error(Err::INVALID_CONFIG, "Offline threshold must be at least 1");
  }

  _resetRuntimeState();
  _config = config;
  _initialized = true;
  _driverState = DriverState::READY;
  _primaryCellEnsureAttempted = false;
  return Status::Ok();
}

void RV3032::tick(uint32_t now_ms) {
  if (!_initialized) {
    return;
  }
  uint8_t instructionsUsed = 0;
  (void)pollEeprom(now_ms, 1, instructionsUsed);
}

void RV3032::end() {
  if (isEepromBusy() || isJobBusy()) {
    return;
  }
  _resetRuntimeState();
}

bool RV3032::isEepromBusy() const {
  return _eeprom.state != EepromState::Idle || _eeprom.queueCount > 0;
}

Status RV3032::getEepromStatus() const {
  if (isEepromBusy()) {
    return Status::Error(Err::IN_PROGRESS, "EEPROM update in progress");
  }
  return _eepromLastStatus;
}

Status RV3032::pollEeprom(uint32_t now_ms, uint8_t maxInstructions, uint8_t& instructionsUsed) {
  instructionsUsed = 0;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (isJobBusy() && _job.activeKind != JobKind::None) {
    return Status::Error(Err::BUSY, "Cooperative job owns the device");
  }
  return processEeprom(now_ms, maxInstructions, instructionsUsed);
}

bool RV3032::isJobBusy() const {
  return _job.state != JobState::Idle;
}

Status RV3032::getJobStatus() const {
  if (isJobBusy()) {
    return Status::Error(Err::IN_PROGRESS, "Job in progress");
  }
  return _job.lastStatus;
}

Status RV3032::startSetTimerJob(uint16_t ticks, TimerFrequency freq, bool enable) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (isJobBusy() || isEepromBusy()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (ticks == 0 || ticks > 0x0FFF) {
    return Status::Error(Err::INVALID_PARAM, "Timer ticks out of range");
  }
  const uint8_t freqRaw = static_cast<uint8_t>(freq);
  if (freqRaw > kMaxTimerFrequency) {
    return Status::Error(Err::INVALID_PARAM, "Timer frequency out of range");
  }

  _job = JobOp{};
  _job.timerTicks = ticks;
  _job.timerFreq = freq;
  _job.timerEnable = enable;
  _job.activeKind = JobKind::SetTimer;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS, "Job in progress");
  _job.state = JobState::SetTimerReadControl1;
  return _job.lastStatus;
}

Status RV3032::startRegisterUpdateJob(uint8_t reg, uint8_t clearMask, uint8_t setMask) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (isJobBusy() || isEepromBusy()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (reg < cmd::REG_USER_RAM_START || reg > cmd::REG_USER_RAM_END) {
    return Status::Error(Err::INVALID_PARAM, "Register update is not allowlisted");
  }

  _job = JobOp{};
  _job.registerUpdateReg = reg;
  _job.registerUpdateImplementedMask = 0xFF;
  _job.registerUpdateClearMask = clearMask;
  _job.registerUpdateSetMask = setMask;
  _job.activeKind = JobKind::RegisterUpdate;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS, "Job in progress");
  _job.state = JobState::RegisterUpdateRead;
  return _job.lastStatus;
}

Status RV3032::startWriteUserRamJob(uint8_t offset, const uint8_t* buf, size_t len) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (isJobBusy() || isEepromBusy()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (buf == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid user RAM write buffer");
  }
  if (offset >= kUserRamSize || len > static_cast<size_t>(kUserRamSize - offset) ||
      len > kJobUserRamBufferSize) {
    return Status::Error(Err::INVALID_PARAM, "User RAM write out of range");
  }

  _job = JobOp{};
  _job.userRamOffset = offset;
  _job.userRamLen = static_cast<uint8_t>(len);
  std::memcpy(_job.userRamBuf, buf, len);
  _job.activeKind = JobKind::WriteUserRam;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS, "Job in progress");
  _job.state = JobState::WriteUserRamChunk;
  return _job.lastStatus;
}

Status RV3032::pollJob(uint32_t now_ms, uint8_t maxInstructions, uint8_t& instructionsUsed) {
  instructionsUsed = 0;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (_eeprom.state != EepromState::Idle &&
      _job.activeKind == JobKind::None && _job.state != JobState::Idle) {
    return Status::Error(Err::BUSY, "EEPROM queue owns the shared engine");
  }
  if (!isJobBusy()) {
    return _job.lastStatus;
  }
  if (maxInstructions == 0) {
    return Status::Error(Err::IN_PROGRESS, "Job in progress");
  }

  while (isJobBusy() && instructionsUsed < maxInstructions) {
    uint32_t currentNowMs = now_ms;
    if (_config.nowMs != nullptr) {
      const uint32_t observedNowMs = _nowMs();
      if (static_cast<int32_t>(observedNowMs - currentNowMs) > 0) {
        currentNowMs = observedNowMs;
      }
    } else if (instructionsUsed != 0) {
      // Without a clock hook, conservatively account for each callback's full
      // configured bound before allowing another instruction in this poll.
      currentNowMs += static_cast<uint32_t>(instructionsUsed) *
                      _config.i2cTimeoutMs;
    }
    if (_job.deadlineActive &&
        hasDeadlinePassed(currentNowMs, _job.deadlineMs)) {
      Status terminal = Status::Error(Err::TIMEOUT, "Job deadline expired");
      if (_job.state == JobState::Persistent &&
          (_job.persistentCleanupRequired ||
           _job.passwordAuthorizationMayBeActive)) {
        terminal = Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "Persistent hard deadline expired before cleanup proof");
        _job.persistentCleanupStatus = terminal;
        _job.persistentRead.cleanupVerified = false;
        _job.userEepromWrite.cleanupVerified = false;
        if (_job.activeKind == JobKind::PasswordProtection) {
          _passwordStatus = PasswordProtectionStatus{};
        }
      }
      finishJob(terminal);
      return terminal;
    }

    Status st = Status::Ok();
    switch (_job.state) {
      case JobState::SetTimerReadControl1: {
        st = readRegs(cmd::REG_CONTROL1, &_job.timerControl1, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        const uint8_t freqRaw = static_cast<uint8_t>(_job.timerFreq);
        const uint8_t timerEnableMask = static_cast<uint8_t>(1u << cmd::CTRL1_TE_BIT);
        _job.timerControl1 = static_cast<uint8_t>(
            (_job.timerControl1 & cmd::CONTROL1_IMPLEMENTED_MASK) &
            ~(cmd::CTRL1_TD_MASK | timerEnableMask));
        _job.timerControl1 = static_cast<uint8_t>(_job.timerControl1 | (freqRaw & cmd::CTRL1_TD_MASK));
        _job.timerFinalControl1 = _job.timerControl1;
        if (_job.timerEnable) {
          _job.timerFinalControl1 = static_cast<uint8_t>(_job.timerFinalControl1 | timerEnableMask);
        }
        _job.timerHigh = static_cast<uint8_t>((_job.timerTicks >> 8) & 0x0Fu);
        _job.state = JobState::SetTimerWriteControl1;
        break;
      }
      case JobState::SetTimerReadTimerHigh:
        _job.state = JobState::SetTimerWriteControl1;
        break;
      case JobState::SetTimerWriteControl1:
        st = writeRegs(cmd::REG_CONTROL1, &_job.timerControl1, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.state = JobState::SetTimerWriteLow;
        break;
      case JobState::SetTimerWriteLow:
      {
        const uint8_t value = static_cast<uint8_t>(_job.timerTicks & 0xFFu);
        st = writeRegs(cmd::REG_TIMER_LOW, &value, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.state = JobState::SetTimerWriteHigh;
        break;
      }
      case JobState::SetTimerWriteHigh:
        st = writeRegs(cmd::REG_TIMER_HIGH, &_job.timerHigh, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        if (_job.timerFinalControl1 != _job.timerControl1) {
          _job.state = JobState::SetTimerWriteFinalControl1;
          break;
        }
        finishJob(Status::Ok());
        return Status::Ok();
      case JobState::SetTimerWriteFinalControl1:
        st = writeRegs(cmd::REG_CONTROL1, &_job.timerFinalControl1, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        finishJob(Status::Ok());
        return Status::Ok();
      case JobState::RegisterUpdateRead:
        st = readRegs(_job.registerUpdateReg, &_job.registerUpdateValue, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        if (_job.registerUpdateGuardedClear) {
          if ((_job.registerUpdateValue & _job.registerUpdateRequiredMask) == 0) {
            finishJob(Status::Ok());
            return Status::Ok();
          }
          if ((_job.registerUpdateValue & _job.registerUpdateForbiddenMask) != 0) {
            st = _job.registerUpdateGuardFailureIsBusy
                ? Status::Error(Err::BUSY,
                                "TEMP_LSB support flags active")
                : Status::Error(
                      Err::INVALID_PARAM,
                      "Flag clear would have collateral side effects");
            finishJob(st);
            return st;
          }
          if (_job.registerUpdateReg == cmd::REG_STATUS) {
            // Preserve every unnamed lower-six W0C flag even if it asserts
            // after this guard read and before the later write callback.
            _job.registerUpdateValue = static_cast<uint8_t>(
                _job.registerUpdateValue | cmd::STATUS_W0C_PRESERVE_MASK);
          }
        }
        _job.registerUpdateValue = static_cast<uint8_t>(
            ((_job.registerUpdateValue & _job.registerUpdateImplementedMask) &
             ~_job.registerUpdateClearMask) |
            _job.registerUpdateSetMask);
        if (_job.persistRegisterUpdate &&
            !eepromQueueContains(_job.registerUpdateReg,
                                 _job.registerUpdateValue) &&
            _eeprom.queueCount >= kEepromQueueSize) {
          st = Status::Error(Err::QUEUE_FULL,
                             "Configuration persistence queue full");
          finishJob(st);
          return st;
        }
        _job.state = JobState::RegisterUpdateWrite;
        break;
      case JobState::RegisterUpdateWrite:
        st = writeRegs(_job.registerUpdateReg, &_job.registerUpdateValue, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        if (_job.persistRegisterUpdate &&
            !eepromQueuePush(_job.registerUpdateReg,
                             _job.registerUpdateValue)) {
          st = Status::Error(Err::QUEUE_FULL,
                             "Configuration persistence queue full");
          finishJob(st);
          return st;
        }
        finishJob(Status::Ok());
        return Status::Ok();
      case JobState::EviResetRead: {
        st = readRegs(cmd::REG_TS_CONTROL, &_job.registerUpdateValue, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        const uint8_t resetMask = static_cast<uint8_t>(
            1u << cmd::TS_EVI_RESET_BIT);
        const bool resetAlreadySet =
            (_job.registerUpdateValue & resetMask) != 0;
        _job.registerUpdateValue = static_cast<uint8_t>(
            _job.registerUpdateValue & cmd::TS_CONTROL_OVERWRITE_MASK);
        if (resetAlreadySet) {
          _job.state = JobState::EviResetWriteClear;
        } else {
          _job.registerUpdateValue = static_cast<uint8_t>(
              _job.registerUpdateValue | resetMask);
          _job.state = JobState::EviResetWriteSet;
        }
        break;
      }
      case JobState::EviResetWriteClear:
        st = writeRegs(cmd::REG_TS_CONTROL, &_job.registerUpdateValue, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.registerUpdateValue = static_cast<uint8_t>(
            _job.registerUpdateValue |
            (1u << cmd::TS_EVI_RESET_BIT));
        _job.state = JobState::EviResetWriteSet;
        break;
      case JobState::EviResetWriteSet:
        st = writeRegs(cmd::REG_TS_CONTROL, &_job.registerUpdateValue, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        finishJob(Status::Ok());
        return Status::Ok();
      case JobState::RegisterBlockUpdateRead:
        st = readRegs(_job.registerBlockReg, _job.registerBlockValues,
                      _job.registerBlockLen);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        for (uint8_t i = 0; i < _job.registerBlockLen; ++i) {
          _job.registerBlockValues[i] = static_cast<uint8_t>(
              ((_job.registerBlockValues[i] &
                _job.registerBlockImplemented[i]) &
               ~_job.registerBlockClear[i]) |
              _job.registerBlockSet[i]);
        }
        if (_job.persistRegisterUpdate) {
          uint8_t requiredCapacity = 0;
          for (uint8_t i = 0; i < _job.registerBlockLen; ++i) {
            if (!eepromQueueContains(
                    static_cast<uint8_t>(_job.registerBlockReg + i),
                    _job.registerBlockValues[i])) {
              ++requiredCapacity;
            }
          }
          if (requiredCapacity >
              static_cast<uint8_t>(kEepromQueueSize - _eeprom.queueCount)) {
            st = Status::Error(
                Err::QUEUE_FULL,
                "Configuration persistence queue lacks capacity");
            finishJob(st);
            return st;
          }
        }
        _job.state = JobState::RegisterBlockUpdateWrite;
        break;
      case JobState::RegisterBlockUpdateWrite:
        st = writeRegs(_job.registerBlockReg, _job.registerBlockValues,
                       _job.registerBlockLen);
        ++instructionsUsed;
        if (st.ok() && _job.persistRegisterUpdate) {
          for (uint8_t i = 0; i < _job.registerBlockLen; ++i) {
            if (!eepromQueuePush(
                    static_cast<uint8_t>(_job.registerBlockReg + i),
                    _job.registerBlockValues[i])) {
              st = Status::Error(Err::QUEUE_FULL,
                                 "Configuration persistence queue full");
              break;
            }
          }
        }
        finishJob(st);
        return st;
      case JobState::ClkoutReadActive: {
        st = readRegs(cmd::REG_ACTIVE_PMU, _job.registerBlockValues, 4);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        if (_job.clkoutLegacyFrequency) {
          _job.clkoutConfig.enabled =
              (_job.registerBlockValues[0] & cmd::PMU_NCLKE_MASK) == 0;
          _job.clkoutConfig.highFrequencyMode = false;
          const uint16_t existingHfd = static_cast<uint16_t>(
              _job.registerBlockValues[2]) |
              (static_cast<uint16_t>(_job.registerBlockValues[3] &
                                     cmd::CLKOUT_HFD_HIGH_MASK) << 8);
          _job.clkoutConfig.highFrequencyDivider =
              static_cast<uint16_t>(existingHfd + 1U);
        }
        const uint16_t hfd = static_cast<uint16_t>(
            _job.clkoutConfig.highFrequencyDivider - 1U);
        _job.registerBlockValues[0] = static_cast<uint8_t>(
            (_job.registerBlockValues[0] & cmd::PMU_IMPLEMENTED_MASK &
             static_cast<uint8_t>(~cmd::PMU_NCLKE_MASK)) |
            (_job.clkoutConfig.enabled ? 0 : cmd::PMU_NCLKE_MASK));
        _job.registerBlockValues[1] = static_cast<uint8_t>(
            _job.registerBlockValues[1] &
            cmd::OFFSET_REGISTER_IMPLEMENTED_MASK);
        _job.registerBlockValues[2] = static_cast<uint8_t>(hfd & 0xFFu);
        _job.registerBlockValues[3] = static_cast<uint8_t>(
            (_job.clkoutConfig.highFrequencyMode ? cmd::CLKOUT_OS_MASK : 0) |
            ((static_cast<uint8_t>(_job.clkoutConfig.xtalFrequency)
              << cmd::CLKOUT_FREQ_SHIFT) & cmd::CLKOUT_FREQ_MASK) |
            ((hfd >> 8) & cmd::CLKOUT_HFD_HIGH_MASK));
        if (_job.persistRegisterUpdate) {
          const uint8_t indexes[3] = {0, 2, 3};
          uint8_t requiredCapacity = 0;
          for (uint8_t index : indexes) {
            if (!eepromQueueContains(
                    static_cast<uint8_t>(cmd::REG_ACTIVE_PMU + index),
                    _job.registerBlockValues[index])) {
              ++requiredCapacity;
            }
          }
          if (requiredCapacity >
              static_cast<uint8_t>(kEepromQueueSize - _eeprom.queueCount)) {
            st = Status::Error(
                Err::QUEUE_FULL,
                "CLKOUT persistence queue lacks capacity");
            finishJob(st);
            return st;
          }
        }
        _job.state = JobState::ClkoutReadGuards;
        break;
      }
      case JobState::ClkoutReadGuards:
        st = readRegs(cmd::REG_TEMP_LSB, _job.clkoutGuard,
                      sizeof(_job.clkoutGuard));
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        if ((_job.clkoutGuard[0] & cmd::TEMP_CLKF_MASK) != 0 ||
            (_job.clkoutGuard[3] & (1u << cmd::CTRL2_CLKIE_BIT)) != 0) {
          st = Status::Error(
              Err::BUSY,
              "Disable interrupt-controlled CLKOUT before reconfiguration");
          finishJob(st);
          return st;
        }
        _job.state = JobState::ClkoutWriteStoppedConfig;
        break;
      case JobState::ClkoutWriteStoppedConfig: {
        uint8_t stopped[4] = {
            static_cast<uint8_t>(_job.registerBlockValues[0] |
                                 cmd::PMU_NCLKE_MASK),
            _job.registerBlockValues[1],
            _job.registerBlockValues[2],
            _job.registerBlockValues[3]};
        st = writeRegs(cmd::REG_ACTIVE_PMU, stopped, sizeof(stopped));
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.state = JobState::ClkoutWriteFinalPmu;
        break;
      }
      case JobState::ClkoutWriteFinalPmu: {
        if ((_job.registerBlockValues[0] & cmd::PMU_NCLKE_MASK) == 0) {
          st = writeRegs(cmd::REG_ACTIVE_PMU,
                         &_job.registerBlockValues[0], 1);
          ++instructionsUsed;
          if (!st.ok()) {
            finishJob(st);
            return st;
          }
        }
        if (_job.persistRegisterUpdate) {
          const uint8_t indexes[3] = {0, 2, 3};
          for (uint8_t index : indexes) {
            if (!eepromQueuePush(
                    static_cast<uint8_t>(cmd::REG_ACTIVE_PMU + index),
                    _job.registerBlockValues[index])) {
              st = Status::Error(Err::QUEUE_FULL,
                                 "CLKOUT persistence queue full");
              finishJob(st);
              return st;
            }
          }
        }
        finishJob(Status::Ok());
        return Status::Ok();
      }
      case JobState::TemperatureConfigRead:
        st = readRegs(cmd::REG_CONTROL3, _job.registerBlockValues, 6);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        for (uint8_t i = 0; i < 6; ++i) {
          _job.registerBlockValues[i] = static_cast<uint8_t>(
              ((_job.registerBlockValues[i] &
                _job.registerBlockImplemented[i]) &
               ~_job.registerBlockClear[i]) |
              _job.registerBlockSet[i]);
        }
        _job.state = JobState::TemperatureConfigWriteDisabled;
        break;
      case JobState::TemperatureConfigWriteDisabled: {
        const uint8_t controlMask = static_cast<uint8_t>(
            (1u << cmd::CTRL3_THE_BIT) | (1u << cmd::CTRL3_TLE_BIT) |
            (1u << cmd::CTRL3_THIE_BIT) | (1u << cmd::CTRL3_TLIE_BIT));
        const uint8_t disabled = static_cast<uint8_t>(
            _job.registerBlockValues[0] & ~controlMask);
        st = writeRegs(cmd::REG_CONTROL3, &disabled, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.state = JobState::TemperatureConfigWriteTimestampControl;
        break;
      }
      case JobState::TemperatureConfigWriteTimestampControl:
        st = writeRegs(cmd::REG_TS_CONTROL,
                       &_job.registerBlockValues[1], 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.state = JobState::TemperatureConfigWriteThresholds;
        break;
      case JobState::TemperatureConfigWriteThresholds:
        st = writeRegs(cmd::REG_TLOW_THRESHOLD,
                       &_job.registerBlockValues[4], 2);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.state = JobState::TemperatureConfigWriteInterrupts;
        break;
      case JobState::TemperatureConfigWriteInterrupts: {
        const uint8_t detectionMask = static_cast<uint8_t>(
            (1u << cmd::CTRL3_THE_BIT) | (1u << cmd::CTRL3_TLE_BIT));
        const uint8_t interruptsOnly = static_cast<uint8_t>(
            _job.registerBlockValues[0] & ~detectionMask);
        st = writeRegs(cmd::REG_CONTROL3, &interruptsOnly, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.state = JobState::TemperatureConfigWriteDetection;
        break;
      }
      case JobState::TemperatureConfigWriteDetection:
        st = writeRegs(cmd::REG_CONTROL3,
                       &_job.registerBlockValues[0], 1);
        ++instructionsUsed;
        finishJob(st);
        return st;
      case JobState::WriteUserRamChunk: {
        const uint8_t remaining = static_cast<uint8_t>(_job.userRamLen - _job.userRamWritten);
        const uint8_t chunk = (remaining > 15U) ? 15U : remaining;
        st = writeRegs(static_cast<uint8_t>(cmd::REG_USER_RAM_START +
                                            _job.userRamOffset +
                                            _job.userRamWritten),
                       &_job.userRamBuf[_job.userRamWritten],
                       chunk);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.userRamWritten = static_cast<uint8_t>(_job.userRamWritten + chunk);
        if (_job.userRamWritten >= _job.userRamLen) {
          finishJob(Status::Ok());
          return Status::Ok();
        }
        break;
      }
      case JobState::ReadTemperatureFirst:
        st = readRegs(cmd::REG_TEMP_LSB, _job.firstTemperature,
                      sizeof(_job.firstTemperature));
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.state = JobState::ReadTemperatureSecond;
        break;
      case JobState::ReadTemperatureSecond: {
        uint8_t second[2] = {};
        st = readRegs(cmd::REG_TEMP_LSB, second, sizeof(second));
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        if ((_job.firstTemperature[0] & 0xF0u) != (second[0] & 0xF0u) ||
            _job.firstTemperature[1] != second[1]) {
          st = Status::Error(Err::INCOHERENT_DATA,
                             "Temperature samples did not agree");
          finishJob(st);
          return st;
        }
        _job.coherentTemperature.raw = decodeTemperatureRaw(second);
        _job.coherentTemperature.celsius =
            static_cast<float>(_job.coherentTemperature.raw) / 16.0f;
        finishJob(Status::Ok());
        return Status::Ok();
      }
      case JobState::ReadTimeStatus:
        st = readRegs(cmd::REG_STATUS, &_job.timeSnapshot.statusRaw, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.timeSnapshot.statusValid = true;
        if ((_job.timeSnapshot.statusRaw & 0x03u) != 0) {
          finishJob(Status::Ok());
          return Status::Ok();
        }
        _job.state = JobState::ReadTimeCalendar;
        break;
      case JobState::ReadTimeCalendar:
        st = readRegs(cmd::REG_SECONDS, _job.calendarBuf, sizeof(_job.calendarBuf));
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        if (!decodeCalendar(_job.calendarBuf, _job.timeSnapshot.time, true)) {
          st = Status::Error(Err::INVALID_DATETIME, "Invalid calendar encoding");
          finishJob(st);
          return st;
        }
        _job.timeSnapshot.timeValid = true;
        finishJob(Status::Ok());
        return Status::Ok();
      case JobState::SetTimeReadStatusBefore:
        st = readRegs(cmd::REG_STATUS, &_job.verifiedSet.statusBefore, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.verifiedSet.statusBeforeValid = true;
        _job.state = JobState::SetTimeWriteCalendar;
        break;
      case JobState::SetTimeWriteCalendar:
        if (hasDeadlinePassed(currentNowMs, _job.mutationCutoffMs)) {
          st = Status::Error(Err::TIMEOUT, "Calendar mutation cutoff expired");
          finishJob(st);
          return st;
        }
        _job.verifiedSet.calendarWriteAttempted = true;
        st = writeRegs(cmd::REG_SECONDS, _job.calendarBuf, sizeof(_job.calendarBuf));
        ++instructionsUsed;
        _job.verifiedSet.calendarWriteStatus = st;
        _job.verifiedSet.calendarWriteAmbiguous = !st.ok();
        _job.state = JobState::SetTimeVerifyCalendar;
        break;
      case JobState::SetTimeVerifyCalendar: {
        uint8_t observed[7] = {};
        st = readRegs(cmd::REG_SECONDS, observed, sizeof(observed));
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        DateTime decoded{};
        if (!decodeCalendar(observed, decoded, true) ||
            !acceptedVerifiedTime(_job.verifiedSet.requested, decoded)) {
          st = Status::Error(Err::EEPROM_VERIFY_FAILED, "Calendar readback mismatch");
          finishJob(st);
          return st;
        }
        _job.verifiedSet.verified = decoded;
        _job.verifiedSet.verifiedValid = true;
        _job.firstVerified = decoded;
        _job.firstVerifiedValid = true;
        if (_job.verifiedSet.calendarWriteAmbiguous) {
          _job.verifiedSet.verifiedAfterAmbiguousWrite = true;
        }
        _job.state = JobState::SetTimeReadStatusBeforeClear;
        break;
      }
      case JobState::SetTimeReadStatusBeforeClear:
        st = readRegs(cmd::REG_STATUS, &_job.verifiedSet.statusBeforeClear, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.verifiedSet.statusBeforeClearValid = true;
        _job.verifiedSet.temperatureHighWasSetBeforeClear =
            (_job.verifiedSet.statusBeforeClear & 0x80u) != 0;
        _job.verifiedSet.temperatureLowWasSetBeforeClear =
            (_job.verifiedSet.statusBeforeClear & 0x40u) != 0;
        _job.state = JobState::SetTimeWriteStatus;
        break;
      case JobState::SetTimeWriteStatus: {
        if (hasDeadlinePassed(currentNowMs, _job.mutationCutoffMs)) {
          st = Status::Error(Err::TIMEOUT, "Status mutation cutoff expired");
          finishJob(st);
          return st;
        }
        const uint8_t value = static_cast<uint8_t>(_job.verifiedSet.statusBeforeClear & 0x3Cu);
        _job.verifiedSet.statusWriteAttempted = true;
        st = writeRegs(cmd::REG_STATUS, &value, 1);
        ++instructionsUsed;
        _job.verifiedSet.statusWriteStatus = st;
        _job.verifiedSet.statusWriteAmbiguous = !st.ok();
        _job.state = JobState::SetTimeReadStatusAfter;
        break;
      }
      case JobState::SetTimeReadStatusAfter:
        st = readRegs(cmd::REG_STATUS, &_job.verifiedSet.statusAfter, 1);
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        _job.verifiedSet.statusAfterValid = true;
        if ((_job.verifiedSet.statusAfter & 0x03u) != 0) {
          st = Status::Error(Err::EEPROM_VERIFY_FAILED, "Invalid-time flags remain set");
          finishJob(st);
          return st;
        }
        if (_job.verifiedSet.statusWriteAmbiguous) {
          _job.verifiedSet.verifiedAfterAmbiguousWrite = true;
        }
        _job.state = JobState::SetTimeReadFinalCalendar;
        break;
      case JobState::SetTimeReadFinalCalendar: {
        uint8_t observed[7] = {};
        st = readRegs(cmd::REG_SECONDS, observed, sizeof(observed));
        ++instructionsUsed;
        if (!st.ok()) {
          finishJob(st);
          return st;
        }
        DateTime decoded{};
        if (!decodeCalendar(observed, decoded, true) ||
            !acceptedVerifiedTime(_job.verifiedSet.requested, decoded)) {
          st = Status::Error(Err::EEPROM_VERIFY_FAILED, "Final calendar mismatch");
          finishJob(st);
          return st;
        }
        uint32_t firstUnix = 0;
        uint32_t finalUnix = 0;
        if (!dateTimeToUnix(_job.firstVerified, firstUnix) ||
            !dateTimeToUnix(decoded, finalUnix) || finalUnix < firstUnix) {
          st = Status::Error(Err::EEPROM_VERIFY_FAILED, "Final calendar moved backward");
          finishJob(st);
          return st;
        }
        _job.verifiedSet.verified = decoded;
        if (_job.verifiedSet.statusWriteAmbiguous) {
          _job.verifiedSet.verifiedAfterAmbiguousWrite = true;
        }
        finishJob(Status::Ok());
        return Status::Ok();
      }
      case JobState::Persistent: {
        bool callbackUsed = false;
        st = processPersistentJob(currentNowMs, callbackUsed);
        if (callbackUsed) {
          ++instructionsUsed;
        }
        if (st.inProgress()) {
          if (!callbackUsed) {
            return st;
          }
          break;
        }
        finishJob(st);
        return st;
      }
      case JobState::Idle:
      default:
        finishJob(Status::Ok());
        return Status::Ok();
    }
  }

  return isJobBusy() ? Status::Error(Err::IN_PROGRESS, "Job in progress") : _job.lastStatus;
}

void RV3032::finishJob(const Status& status) {
  _job.lastStatus = status;
  _job.completedStatus = status;
  _job.completedKind = _job.activeKind;
  _job.activeKind = JobKind::None;
  _job.state = JobState::Idle;
  if (_job.completedKind == JobKind::PasswordProtection) {
    std::memset(_job.currentCredential.bytes, 0,
                sizeof(_job.currentCredential.bytes));
    std::memset(_job.newCredential.bytes, 0,
                sizeof(_job.newCredential.bytes));
    std::memset(_job.userRamBuf, 0, sizeof(_job.userRamBuf));
  }
}

bool RV3032::workIdle() const {
  return !isJobBusy() && !isEepromBusy();
}

Status RV3032::updateRegisterSingle(uint8_t reg, uint8_t implementedMask,
                                    uint8_t clearMask, uint8_t setMask) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if ((clearMask & ~implementedMask) != 0 ||
      (setMask & ~implementedMask) != 0) {
    return Status::Error(Err::INVALID_PARAM, "Register mutation touches reserved bits");
  }
  const bool persist =
      _config.enableEepromWrites && reg >= cmd::REG_ACTIVE_PMU &&
      reg <= cmd::REG_ACTIVE_TREFERENCE1;
  if (isJobBusy() || _eeprom.state != EepromState::Idle ||
      (!persist && _eeprom.queueCount > 0)) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::RegisterUpdate;
  _job.registerUpdateReg = reg;
  _job.registerUpdateImplementedMask = implementedMask;
  _job.registerUpdateClearMask = clearMask;
  _job.registerUpdateSetMask = setMask;
  _job.persistRegisterUpdate = persist;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS, "Control update in progress");
  _job.state = JobState::RegisterUpdateRead;
  return _job.lastStatus;
}

Status RV3032::updateRegisterBlock(uint8_t reg, uint8_t length,
                                   const uint8_t* implementedMasks,
                                   const uint8_t* clearMasks,
                                   const uint8_t* setMasks) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (length == 0 || length > 8 || implementedMasks == nullptr ||
      clearMasks == nullptr || setMasks == nullptr) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register-block mutation");
  }
  for (uint8_t i = 0; i < length; ++i) {
    if ((clearMasks[i] & ~implementedMasks[i]) != 0 ||
        (setMasks[i] & ~implementedMasks[i]) != 0) {
      return Status::Error(Err::INVALID_PARAM,
                           "Register-block mutation touches reserved bits");
    }
  }
  const bool persist =
      _config.enableEepromWrites && reg >= cmd::REG_ACTIVE_PMU &&
      static_cast<uint16_t>(reg) + length - 1U <=
          cmd::REG_ACTIVE_TREFERENCE1;
  if (isJobBusy() || _eeprom.state != EepromState::Idle ||
      (!persist && _eeprom.queueCount > 0)) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::RegisterUpdate;
  _job.registerBlockReg = reg;
  _job.registerBlockLen = length;
  std::memcpy(_job.registerBlockImplemented, implementedMasks, length);
  std::memcpy(_job.registerBlockClear, clearMasks, length);
  std::memcpy(_job.registerBlockSet, setMasks, length);
  _job.persistRegisterUpdate = persist;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                  "Control block update in progress");
  _job.state = JobState::RegisterBlockUpdateRead;
  return _job.lastStatus;
}

Status RV3032::startReadTimeSnapshotJob(uint32_t nowMs,
                                        uint32_t operationTimeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (operationTimeoutMs == 0 || operationTimeoutMs > 1000) {
    return Status::Error(Err::INVALID_PARAM, "Read-time timeout must be 1..1000 ms");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::ReadTimeSnapshot;
  _job.deadlineMs = nowMs + operationTimeoutMs;
  _job.deadlineActive = true;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS, "Job in progress");
  _job.state = JobState::ReadTimeStatus;
  return _job.lastStatus;
}

Status RV3032::getReadTimeSnapshotJobResult(TimeSnapshot& out) const {
  if (_job.activeKind == JobKind::ReadTimeSnapshot) {
    return Status::Error(Err::IN_PROGRESS, "Read-time job in progress");
  }
  if (_job.completedKind != JobKind::ReadTimeSnapshot) {
    return Status::Error(Err::JOB_RESULT_UNAVAILABLE, "Read-time result unavailable");
  }
  out = _job.timeSnapshot;
  return _job.completedStatus;
}

Status RV3032::startSetTimeAndClearInvalidFlagsVerifiedJob(
    const DateTime& value, uint32_t nowMs, uint32_t operationTimeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (value.year < 2000 || value.year > 2099 ||
      value.month < 1 || value.month > 12 ||
      value.day < 1 || value.day > daysInMonth(value.year, value.month) ||
      value.hour > 23 || value.minute > 59 || value.second > 59) {
    return Status::Error(Err::INVALID_DATETIME, "Invalid date/time");
  }
  if (operationTimeoutMs <= MIN_SET_TIME_OPERATION_BUDGET_MS ||
      operationTimeoutMs > 1000) {
    return Status::Error(Err::INVALID_PARAM, "Verified-set timeout must be 126..1000 ms");
  }

  DateTime normalized = value;
  normalized.weekday = computeWeekday(value.year, value.month, value.day);
  _job = JobOp{};
  _job.activeKind = JobKind::SetTimeVerified;
  _job.deadlineMs = nowMs + operationTimeoutMs;
  _job.mutationCutoffMs = nowMs +
      (operationTimeoutMs - MIN_SET_TIME_OPERATION_BUDGET_MS);
  _job.deadlineActive = true;
  _job.mutationCutoffActive = true;
  _job.verifiedSet.requested = normalized;
  _job.calendarBuf[0] = binToBcd(normalized.second);
  _job.calendarBuf[1] = binToBcd(normalized.minute);
  _job.calendarBuf[2] = binToBcd(normalized.hour);
  _job.calendarBuf[3] = binToBcd(normalized.weekday);
  _job.calendarBuf[4] = binToBcd(normalized.day);
  _job.calendarBuf[5] = binToBcd(normalized.month);
  _job.calendarBuf[6] = binToBcd(static_cast<uint8_t>(normalized.year - 2000));
  _job.lastStatus = Status::Error(Err::IN_PROGRESS, "Job in progress");
  _job.state = JobState::SetTimeReadStatusBefore;
  return _job.lastStatus;
}

Status RV3032::getSetTimeAndClearInvalidFlagsVerifiedJobResult(
    VerifiedTimeSetReport& out) const {
  if (_job.activeKind == JobKind::SetTimeVerified) {
    return Status::Error(Err::IN_PROGRESS, "Verified-set job in progress");
  }
  if (_job.completedKind != JobKind::SetTimeVerified) {
    return Status::Error(Err::JOB_RESULT_UNAVAILABLE, "Verified-set result unavailable");
  }
  out = _job.verifiedSet;
  return _job.completedStatus;
}

Status RV3032::startPersistentReadJob(uint8_t address, uint8_t length,
                                      uint32_t nowMs, uint32_t timeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (length == 0 || length > USER_EEPROM_JOB_MAX_BYTES ||
      timeoutMs < 50 || timeoutMs > 10000) {
    return Status::Error(Err::INVALID_PARAM, "Invalid persistent-read bounds");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::PersistentRead;
  _job.state = JobState::Persistent;
  _job.persistentState = EepromState::ReadControl1;
  _job.persistentAddress = address;
  _job.persistentLength = length;
  _job.deadlineMs = nowMs + timeoutMs;
  _job.mutationCutoffMs = nowMs + (timeoutMs -
      (timeoutMs > 600 ? 300 : timeoutMs / 2));
  _job.deadlineActive = true;
  _job.mutationCutoffActive = true;
  _job.persistentRead.eepromAddress = address;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS, "Persistent read in progress");
  return _job.lastStatus;
}

Status RV3032::startReadConfigurationEepromJob(
    ConfigurationEepromRegister reg, uint32_t nowMs, uint32_t operationTimeoutMs) {
  const uint8_t address = static_cast<uint8_t>(reg);
  if (address < cmd::CONFIG_EEPROM_START ||
      address > cmd::REG_ACTIVE_TREFERENCE1) {
    return Status::Error(Err::INVALID_PARAM, "Unsupported configuration EEPROM address");
  }
  return startPersistentReadJob(address, 1, nowMs, operationTimeoutMs);
}

Status RV3032::startReadUserEepromJob(uint8_t offset, uint8_t length,
                                      uint32_t nowMs,
                                      uint32_t operationTimeoutMs) {
  if (offset >= USER_EEPROM_SIZE || length == 0 ||
      length > USER_EEPROM_JOB_MAX_BYTES ||
      length > static_cast<uint8_t>(USER_EEPROM_SIZE - offset)) {
    return Status::Error(Err::INVALID_PARAM, "User EEPROM read out of range");
  }
  return startPersistentReadJob(static_cast<uint8_t>(cmd::USER_EEPROM_START + offset),
                                length, nowMs, operationTimeoutMs);
}

Status RV3032::startWriteUserEepromJob(uint8_t offset, const uint8_t* data,
                                       uint8_t length, uint32_t nowMs,
                                       uint32_t operationTimeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (!_config.enableEepromWrites) {
    return Status::Error(Err::INVALID_CONFIG, "Generic EEPROM writes are disabled");
  }
  if (data == nullptr || offset >= USER_EEPROM_SIZE || length == 0 ||
      length > USER_EEPROM_JOB_MAX_BYTES ||
      length > static_cast<uint8_t>(USER_EEPROM_SIZE - offset) ||
      operationTimeoutMs <= 300 || operationTimeoutMs > 10000) {
    return Status::Error(Err::INVALID_PARAM, "User EEPROM write out of range");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::UserEepromWrite;
  _job.state = JobState::Persistent;
  _job.persistentState = EepromState::ReadControl1;
  _job.persistentAddress = static_cast<uint8_t>(cmd::USER_EEPROM_START + offset);
  _job.persistentLength = length;
  _job.persistentWriteMode = true;
  _job.deadlineMs = nowMs + operationTimeoutMs;
  _job.mutationCutoffMs = nowMs + (operationTimeoutMs - 300);
  _job.deadlineActive = true;
  _job.mutationCutoffActive = true;
  std::memcpy(_job.userRamBuf, data, length);
  _job.userEepromWrite.offset = offset;
  _job.userEepromWrite.requestedLength = length;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS, "User EEPROM write in progress");
  return _job.lastStatus;
}

Status RV3032::getPersistentReadJobResult(PersistentReadResult& out) const {
  if (_job.activeKind == JobKind::PersistentRead) {
    return Status::Error(Err::IN_PROGRESS, "Persistent-read job in progress");
  }
  if (_job.completedKind != JobKind::PersistentRead) {
    return Status::Error(Err::JOB_RESULT_UNAVAILABLE, "Persistent-read result unavailable");
  }
  out = _job.persistentRead;
  return _job.completedStatus;
}

Status RV3032::getUserEepromWriteJobResult(UserEepromWriteReport& out) const {
  if (_job.activeKind == JobKind::UserEepromWrite) {
    return Status::Error(Err::IN_PROGRESS, "User EEPROM write in progress");
  }
  if (_job.completedKind != JobKind::UserEepromWrite) {
    return Status::Error(Err::JOB_RESULT_UNAVAILABLE, "User EEPROM write result unavailable");
  }
  out = _job.userEepromWrite;
  return _job.completedStatus;
}

Status RV3032::getSettings(SettingsSnapshot& out) const {
  out.initialized = _initialized;
  out.state = _driverState;
  out.i2cAddress = _config.i2cAddress;
  out.i2cTimeoutMs = _config.i2cTimeoutMs;
  out.offlineThreshold = _config.offlineThreshold;
  out.hasNowMsHook = (_config.nowMs != nullptr);
  out.hasWaitMsHook = (_config.waitMs != nullptr);

  out.beginInProgress = _beginInProgress;
  out.enableEepromWrites = _config.enableEepromWrites;
  out.eepromTimeoutMs = _config.eepromTimeoutMs;
  out.eepromBusy = isEepromBusy();
  out.eepromLastStatus = _eepromLastStatus;
  out.eepromWriteCount = _eepromWriteCount;
  out.eepromWriteFailures = _eepromWriteFailures;
  out.eepromQueueDepth = _eeprom.queueCount;
  out.jobBusy = isJobBusy();
  out.primaryCellEnsureAttempted = _primaryCellEnsureAttempted;
  out.lastOkMs = _lastOkMs;
  out.lastErrorMs = _lastErrorMs;
  out.lastError = _lastError;
  out.consecutiveFailures = _consecutiveFailures;
  out.totalFailures = _totalFailures;
  out.totalSuccess = _totalSuccess;
  return Status::Ok();
}

// ===== Driver State and Health =====

Status RV3032::probe() {
  if (!_initialized && !_beginInProgress) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks not configured");
  }
  // Read a known register to verify device presence.
  // Uses raw I2C (no health tracking) - this is a diagnostic-only function.
  // The status register value is not used; we only check if I2C succeeds.
  uint8_t statusReg = 0;
  Status st = _readRegisterRaw(cmd::REG_STATUS, statusReg);
  (void)statusReg;  // Value unused - we only verify I2C communication works

  // Do NOT call _updateHealth() - probe is diagnostic only
  return mapPresenceError(st);
}

Status RV3032::recover() {
  // Precondition: must be initialized to recover
  if (!_initialized) {
    // No health tracking for precondition errors
    return Status::Error(Err::NOT_INITIALIZED, "begin() not called");
  }

  uint8_t statusReg = 0;
  const uint8_t reg = cmd::REG_STATUS;
  Status st = _i2cWriteReadTracked(&reg, 1, &statusReg, 1);
  (void)statusReg;
  return mapPresenceError(st);
}

// ===== I2C Transport Wrappers =====

Status RV3032::_i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen, uint8_t* rxBuf, size_t rxLen) {
  return _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen,
                          _config.i2cTimeoutMs);
}

Status RV3032::_i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                                uint8_t* rxBuf, size_t rxLen,
                                uint32_t timeoutMs) {
  if (!_config.i2cWriteRead) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback null");
  }
  if ((txLen > 0 && txBuf == nullptr) || rxBuf == nullptr || rxLen == 0 ||
      timeoutMs == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C read buffers");
  }
  return _config.i2cWriteRead(_config.i2cAddress, txBuf, txLen, rxBuf, rxLen,
                              timeoutMs, _config.i2cUser);
}

Status RV3032::_i2cWriteRaw(const uint8_t* buf, size_t len) {
  return _i2cWriteRaw(buf, len, _config.i2cTimeoutMs);
}

Status RV3032::_i2cWriteRaw(const uint8_t* buf, size_t len,
                            uint32_t timeoutMs) {
  if (!_config.i2cWrite) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback null");
  }
  if (!buf || len == 0 || timeoutMs == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C write buffer");
  }
  return _config.i2cWrite(_config.i2cAddress, buf, len,
                          timeoutMs, _config.i2cUser);
}

Status RV3032::_i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen, uint8_t* rxBuf, size_t rxLen) {
  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen);
  if (!shouldTrackHealth(st)) {
    return st;
  }
  return _updateHealth(st);
}

Status RV3032::_i2cWriteTracked(const uint8_t* buf, size_t len) {
  Status st = _i2cWriteRaw(buf, len);
  if (!shouldTrackHealth(st)) {
    return st;
  }
  return _updateHealth(st);
}

Status RV3032::_i2cWriteReadTrackedTimeout(const uint8_t* txBuf, size_t txLen,
                                            uint8_t* rxBuf, size_t rxLen,
                                            uint32_t timeoutMs) {
  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen, timeoutMs);
  return shouldTrackHealth(st) ? _updateHealth(st) : st;
}

Status RV3032::_i2cWriteTrackedTimeout(const uint8_t* buf, size_t len,
                                       uint32_t timeoutMs) {
  Status st = _i2cWriteRaw(buf, len, timeoutMs);
  return shouldTrackHealth(st) ? _updateHealth(st) : st;
}

Status RV3032::_readRegisterRaw(uint8_t reg, uint8_t& value) {
  if (!isKnownRegisterAddress(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Register address out of range");
  }
  uint8_t tx = reg;
  return _i2cWriteReadRaw(&tx, 1, &value, 1);
}

Status RV3032::_updateHealth(const Status& st) {
  if (!_initialized || st.inProgress()) {
    return st;
  }

  bool isSuccess = st.ok();

  if (isSuccess) {
    // Success path
    _lastOkMs = _nowMs();
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      ++_totalSuccess;
    }

    // Transition to READY on first success after init or recovery.
    if (_driverState == DriverState::UNINIT ||
        _driverState == DriverState::DEGRADED ||
        _driverState == DriverState::OFFLINE) {
      _driverState = DriverState::READY;
    }
  } else {
    // Failure path
    _lastError = st;
    _lastErrorMs = _nowMs();
    if (_consecutiveFailures < 0xFFu) {
      ++_consecutiveFailures;
    }
    if (_totalFailures < UINT32_MAX) {
      ++_totalFailures;
    }

    // Transition READY -> DEGRADED on first failure.
    if (_consecutiveFailures == 1 && _driverState == DriverState::READY) {
      _driverState = DriverState::DEGRADED;
    }

    // Transition DEGRADED -> OFFLINE when threshold reached.
    if (_consecutiveFailures >= _config.offlineThreshold) {
      _driverState = DriverState::OFFLINE;
    }
  }

  return st;
}

uint32_t RV3032::_nowMs() const {
  if (_config.nowMs != nullptr) {
    return _config.nowMs(_config.timeUser);
  }
  return 0;
}

void RV3032::_resetRuntimeState() {
  _config = Config{};
  _initialized = false;
  _beginInProgress = false;
  _driverState = DriverState::UNINIT;
  _eeprom = EepromOp{};
  _job = JobOp{};
  _eepromLastStatus = Status::Ok();
  _eepromWriteCount = 0;
  _eepromWriteFailures = 0;
  _primaryCellEnsureAttempted = false;
  _passwordStatus = PasswordProtectionStatus{};
  _lastOkMs = 0;
  _lastError = Status::Ok();
  _lastErrorMs = 0;
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
}

// ===== Time/Date Operations =====

Status RV3032::readTime(DateTime& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t buf[7] = {0};
  Status st = readRegs(cmd::REG_SECONDS, buf, sizeof(buf));
  if (!st.ok()) {
    return st;  // readRegs() uses tracked I2C, so health state is already updated
  }

  if (!decodeCalendar(buf, out, true)) {
    return Status::Error(Err::INVALID_DATETIME, "RTC returned invalid calendar encoding");
  }

  return Status::Ok();
}

Status RV3032::readHundredths(uint8_t& hundredths) {
  uint8_t raw = 0;
  Status st = readRegs(cmd::REG_100TH_SECONDS, &raw, 1);
  if (!st.ok()) {
    return st;
  }
  if (!isValidBcd(raw)) {
    return Status::Error(Err::INVALID_DATETIME,
                         "RTC returned invalid hundredths encoding");
  }
  hundredths = bcdToBin(raw);
  return Status::Ok();
}

Status RV3032::setTime(const DateTime& time) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  // Validate input fields BEFORE computing weekday to avoid calling
  // dateToDays() with out-of-range month/day values.
  if (time.year < 2000 || time.year > 2099 ||
      time.month < 1 || time.month > 12 ||
      time.day < 1 || time.day > daysInMonth(time.year, time.month) ||
      time.hour > 23 || time.minute > 59 || time.second > 59) {
    return Status::Error(Err::INVALID_DATETIME, "Invalid date/time values");
  }

  uint8_t weekday = computeWeekday(time.year, time.month, time.day);

  uint8_t buf[7] = {
    binToBcd(time.second),
    binToBcd(time.minute),
    binToBcd(time.hour),
    static_cast<uint8_t>(weekday & 0x07),
    binToBcd(time.day),
    binToBcd(time.month),
    binToBcd(static_cast<uint8_t>(time.year % 100))
  };

  // Health updated automatically by tracked wrapper
  return writeRegs(cmd::REG_SECONDS, buf, sizeof(buf));
}

Status RV3032::readUnix(uint32_t& out) {
  DateTime dt;
  Status st = readTime(dt);
  if (!st.ok()) {
    return st;
  }

  uint32_t days = dateToDays(dt.year, dt.month, dt.day);
  out = days * 86400UL
      + static_cast<uint32_t>(dt.hour) * 3600UL
      + static_cast<uint32_t>(dt.minute) * 60UL
      + static_cast<uint32_t>(dt.second);

  return Status::Ok();
}

Status RV3032::setUnix(uint32_t ts) {
  DateTime dt;
  if (!unixToDate(ts, dt)) {
    return Status::Error(Err::INVALID_DATETIME, "Unix timestamp out of range");
  }
  return setTime(dt);
}

// ===== Alarm Operations =====

Status RV3032::setAlarmTime(uint8_t minute, uint8_t hour, uint8_t date) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  if (minute > 59 || hour > 23 || date > 31) {
    return Status::Error(Err::INVALID_PARAM, "Invalid alarm time values");
  }

  const uint8_t implemented[3] = {0xFF, 0xBF, 0xBF};
  const uint8_t clear[3] = {
      0x7F, 0x3F, static_cast<uint8_t>(date == 0 ? 0xBF : 0x3F)};
  const uint8_t set[3] = {binToBcd(minute), binToBcd(hour), binToBcd(date)};
  return updateRegisterBlock(cmd::REG_ALARM_MINUTE, 3, implemented, clear, set);
}

Status RV3032::setAlarmMatch(bool matchMinute, bool matchHour, bool matchDate) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  const uint8_t implemented[3] = {0xFF, 0xBF, 0xBF};
  const uint8_t clear[3] = {0x80, 0x80, 0x80};
  const uint8_t set[3] = {
      static_cast<uint8_t>(matchMinute ? 0 : 0x80),
      static_cast<uint8_t>(matchHour ? 0 : 0x80),
      static_cast<uint8_t>(matchDate ? 0 : 0x80)};
  return updateRegisterBlock(cmd::REG_ALARM_MINUTE, 3, implemented, clear, set);
}

Status RV3032::getAlarmConfig(AlarmConfig& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  // Burst-read all 3 alarm registers (0x08-0x0A)
  uint8_t buf[3] = {0};
  Status st = readRegs(cmd::REG_ALARM_MINUTE, buf, sizeof(buf));
  if (!st.ok()) return st;

  const uint8_t minReg = buf[0];
  const uint8_t hourReg = buf[1];
  const uint8_t dateReg = buf[2];
  if ((hourReg & 0x40u) != 0 || (dateReg & 0x40u) != 0) {
    return Status::Error(Err::INVALID_PARAM, "Alarm registers contain reserved bits");
  }

  out.matchMinute = ((minReg & 0x80) == 0);
  out.matchHour = ((hourReg & 0x80) == 0);
  out.matchDate = ((dateReg & 0x80) == 0);

  const uint8_t minRaw = static_cast<uint8_t>(minReg & 0x7F);
  const uint8_t hourRaw = static_cast<uint8_t>(hourReg & 0x7F);
  const uint8_t dateRaw = static_cast<uint8_t>(dateReg & 0x7F);

  auto decodeAlarmField = [&](uint8_t rawBcd, bool fieldEnabled,
                              uint8_t minValue, uint8_t maxValue,
                              uint8_t disabledFallback,
                              bool allowEnabledZero,
                              uint8_t& outValue) -> Status {
    if (!isValidBcd(rawBcd)) {
      if (fieldEnabled) {
        return Status::Error(Err::INVALID_PARAM, "Alarm registers contain invalid BCD");
      }
      outValue = disabledFallback;
      return Status::Ok();
    }
    const uint8_t value = bcdToBin(rawBcd);
    if (value < minValue || value > maxValue) {
      if (fieldEnabled && allowEnabledZero && value == 0) {
        outValue = 0;
        return Status::Ok();
      }
      if (fieldEnabled) {
        return Status::Error(Err::INVALID_PARAM, "Alarm registers out of range");
      }
      outValue = disabledFallback;
      return Status::Ok();
    }
    outValue = value;
    return Status::Ok();
  };

  Status decode = decodeAlarmField(minRaw, out.matchMinute, 0, 59, 0, false, out.minute);
  if (!decode.ok()) {
    return decode;
  }
  decode = decodeAlarmField(hourRaw, out.matchHour, 0, 23, 0, false, out.hour);
  if (!decode.ok()) {
    return decode;
  }
  // RV-3032-C7 reset state uses AE_D=0 with Date Alarm=00h to keep the alarm
  // function inactive. Report that documented hardware state instead of failing.
  decode = decodeAlarmField(dateRaw, out.matchDate, 1, 31, 1, true, out.date);
  if (!decode.ok()) {
    return decode;
  }

  return Status::Ok();
}

Status RV3032::getAlarmFlag(bool& triggered) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(cmd::REG_STATUS, status);
  if (!st.ok()) {
    return st;
  }

  triggered = ((status & (1u << cmd::STATUS_AF_BIT)) != 0);
  return Status::Ok();
}

Status RV3032::clearAlarmFlag() {
  return clearStatus(static_cast<uint8_t>(1u << cmd::STATUS_AF_BIT));
}

Status RV3032::enableAlarmInterrupt(bool enable) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::CTRL2_AIE_BIT);
  return updateRegisterSingle(cmd::REG_CONTROL2, cmd::CONTROL2_IMPLEMENTED_MASK,
                              bit, enable ? bit : 0);
}

Status RV3032::getAlarmInterruptEnabled(bool& enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t control2 = 0;
  Status st = readRegister(cmd::REG_CONTROL2, control2);
  if (!st.ok()) {
    return st;
  }

  enabled = ((control2 & (1u << cmd::CTRL2_AIE_BIT)) != 0);
  return Status::Ok();
}

// ===== Timer Operations =====

Status RV3032::setTimer(uint16_t ticks, TimerFrequency freq, bool enable) {
  return startSetTimerJob(ticks, freq, enable);
}

Status RV3032::getTimer(uint16_t& ticks, TimerFrequency& freq, bool& enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t values[6] = {};
  Status st = readRegs(cmd::REG_TIMER_LOW, values, sizeof(values));
  if (!st.ok()) return st;

  const uint8_t low = values[0];
  const uint8_t high = values[1];
  const uint8_t control1 = values[5];
  if ((high & 0xF0u) != 0) {
    return Status::Error(Err::INVALID_PARAM,
                         "Timer high register contains reserved bits");
  }

  ticks = static_cast<uint16_t>((static_cast<uint16_t>(high & 0x0F) << 8) | low);
  freq = static_cast<TimerFrequency>(control1 & cmd::CTRL1_TD_MASK);
  enabled = ((control1 & (1u << cmd::CTRL1_TE_BIT)) != 0);

  return Status::Ok();
}

Status RV3032::setTimerInterruptEnabled(bool enabled) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::CTRL2_TIE_BIT);
  return updateRegisterSingle(cmd::REG_CONTROL2, cmd::CONTROL2_IMPLEMENTED_MASK,
                              bit, enabled ? bit : 0);
}

Status RV3032::getTimerInterruptEnabled(bool& enabled) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_CONTROL2, &value, 1);
  if (st.ok()) enabled = (value & (1u << cmd::CTRL2_TIE_BIT)) != 0;
  return st;
}

Status RV3032::getTimerFlag(bool& triggered) {
  uint8_t value = 0;
  Status st = readStatus(value);
  if (st.ok()) triggered = (value & (1u << cmd::STATUS_TF_BIT)) != 0;
  return st;
}

Status RV3032::clearTimerFlag() {
  return clearStatus(static_cast<uint8_t>(1u << cmd::STATUS_TF_BIT));
}

Status RV3032::setPeriodicUpdate(PeriodicUpdateFrequency frequency,
                                  bool interruptEnabled) {
  const uint8_t raw = static_cast<uint8_t>(frequency);
  if (raw > 1) {
    return Status::Error(Err::INVALID_PARAM, "Periodic update frequency out of range");
  }
  const uint8_t implemented[2] = {cmd::CONTROL1_IMPLEMENTED_MASK,
                                  cmd::CONTROL2_IMPLEMENTED_MASK};
  const uint8_t clear[2] = {
      static_cast<uint8_t>(1u << cmd::CTRL1_USEL_BIT),
      static_cast<uint8_t>(1u << cmd::CTRL2_UIE_BIT)};
  const uint8_t set[2] = {
      static_cast<uint8_t>(raw ? (1u << cmd::CTRL1_USEL_BIT) : 0),
      static_cast<uint8_t>(interruptEnabled ?
          (1u << cmd::CTRL2_UIE_BIT) : 0)};
  return updateRegisterBlock(cmd::REG_CONTROL1, 2, implemented, clear, set);
}

Status RV3032::getPeriodicUpdate(PeriodicUpdateFrequency& frequency,
                                  bool& interruptEnabled) {
  uint8_t values[2] = {};
  Status st = readRegs(cmd::REG_CONTROL1, values, sizeof(values));
  if (st.ok()) {
    frequency = (values[0] & (1u << cmd::CTRL1_USEL_BIT)) != 0
        ? PeriodicUpdateFrequency::MINUTE
        : PeriodicUpdateFrequency::SECOND;
    interruptEnabled = (values[1] & (1u << cmd::CTRL2_UIE_BIT)) != 0;
  }
  return st;
}

Status RV3032::getPeriodicUpdateFlag(bool& triggered) {
  uint8_t value = 0;
  Status st = readStatus(value);
  if (st.ok()) triggered = (value & (1u << cmd::STATUS_UF_BIT)) != 0;
  return st;
}

Status RV3032::clearPeriodicUpdateFlag() {
  return clearStatus(static_cast<uint8_t>(1u << cmd::STATUS_UF_BIT));
}

// ===== Power Management / Backup Operations =====

Status RV3032::setBackupSwitchMode(BackupSwitchMode mode) {
  const uint8_t modeRaw = static_cast<uint8_t>(mode);
  if (modeRaw > kMaxBackupSwitchMode) {
    return Status::Error(Err::INVALID_PARAM, "Backup switch mode out of range");
  }
  uint8_t setMask = 0;
  switch (mode) {
    case BackupSwitchMode::Off:
      break;
    case BackupSwitchMode::Level:
      setMask = cmd::PMU_BSM_LEVEL;
      break;
    case BackupSwitchMode::Direct:
      setMask = cmd::PMU_BSM_DIRECT;
      break;
  }
  return updateRegisterSingle(cmd::REG_ACTIVE_PMU,
                              cmd::PMU_IMPLEMENTED_MASK,
                              cmd::PMU_BSM_MASK, setMask);
}

Status RV3032::getBackupSwitchMode(BackupSwitchMode& mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t pmu = 0;
  Status st = readRegister(cmd::REG_ACTIVE_PMU, pmu);
  if (!st.ok()) {
    return st;
  }

  switch (pmu & cmd::PMU_BSM_MASK) {
    case cmd::PMU_BSM_DIRECT:
      mode = BackupSwitchMode::Direct;
      break;
    case cmd::PMU_BSM_LEVEL:
      mode = BackupSwitchMode::Level;
      break;
    default:
      mode = BackupSwitchMode::Off;
      break;
  }
  return Status::Ok();
}

Status RV3032::setTrickleChargeMode(TrickleChargeMode mode) {
  const uint8_t raw = static_cast<uint8_t>(mode);
  if (raw > 3) {
    return Status::Error(Err::INVALID_PARAM, "Trickle charge mode out of range");
  }
  return updateRegisterSingle(cmd::REG_ACTIVE_PMU,
                              cmd::PMU_IMPLEMENTED_MASK,
                              cmd::PMU_TCM_MASK, raw);
}

Status RV3032::getTrickleChargeMode(TrickleChargeMode& mode) {
  uint8_t pmu = 0;
  Status st = readRegs(cmd::REG_ACTIVE_PMU, &pmu, 1);
  if (st.ok()) mode = static_cast<TrickleChargeMode>(pmu & cmd::PMU_TCM_MASK);
  return st;
}

Status RV3032::setTrickleChargeResistance(
    TrickleChargeResistance resistance) {
  const uint8_t raw = static_cast<uint8_t>(resistance);
  if (raw > 3) {
    return Status::Error(Err::INVALID_PARAM, "Trickle resistance out of range");
  }
  return updateRegisterSingle(cmd::REG_ACTIVE_PMU,
                              cmd::PMU_IMPLEMENTED_MASK,
                              cmd::PMU_TCR_MASK,
                              static_cast<uint8_t>(raw << 2));
}

Status RV3032::getTrickleChargeResistance(
    TrickleChargeResistance& resistance) {
  uint8_t pmu = 0;
  Status st = readRegs(cmd::REG_ACTIVE_PMU, &pmu, 1);
  if (st.ok()) {
    resistance = static_cast<TrickleChargeResistance>(
        (pmu & cmd::PMU_TCR_MASK) >> 2);
  }
  return st;
}

Status RV3032::setBackupSwitchInterruptEnabled(bool enabled) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::CTRL3_BSIE_BIT);
  return updateRegisterSingle(cmd::REG_CONTROL3, cmd::CONTROL3_IMPLEMENTED_MASK,
                              bit, enabled ? bit : 0);
}

Status RV3032::getBackupSwitchInterruptEnabled(bool& enabled) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_CONTROL3, &value, 1);
  if (st.ok()) enabled = (value & (1u << cmd::CTRL3_BSIE_BIT)) != 0;
  return st;
}

Status RV3032::ensurePrimaryCellConfiguration(
    PrimaryCellConfigurationReport& report) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (_config.nowMs == nullptr || _config.waitMs == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "Primary ensure requires nowMs and waitMs");
  }
  if (_primaryCellEnsureAttempted) {
    return Status::Error(Err::PRIMARY_CELL_ALREADY_ATTEMPTED,
                         "Primary-cell ensure already attempted");
  }

  PrimaryCellConfigurationReport local{};
  _primaryCellEnsureAttempted = true;
  const uint32_t operationStart = _nowMs();
  uint8_t control1Before = 0;
  uint8_t activeBefore = 0;
  uint8_t safeActive = 0;
  bool control1Valid = false;
  bool control1WriteAttempted = false;
  bool accessStateVerified = false;
  bool activeValid = false;
  bool safeValid = false;
  bool safeActiveWriteAmbiguous = false;
  bool persistenceTrusted = false;
  bool callbackReturnedLate = false;
  uint8_t writeCommandAttempts = 0;
  uint8_t target = 0;

  Status operation = ensureRead(cmd::REG_CONTROL1, &control1Before, 1,
                                operationStart,
                                PRIMARY_CELL_OPERATION_TIMEOUT_MS, true,
                                &callbackReturnedLate);
  if (!operation.ok()) {
    local.failureStage = PrimaryCellFailureStage::PREPARE_ACCESS;
    local.operationStatus = operation;
    // No device mutation was attempted, so there is no access state to undo.
    local.cleanupVerified = true;
    local.outcome = PrimaryCellConfigurationOutcome::FAILED;
    report = local;
    return operation;
  }
  control1Valid = true;

  const uint8_t eerdOn = static_cast<uint8_t>(
      (control1Before & cmd::CONTROL1_IMPLEMENTED_MASK) |
      cmd::CONTROL1_EERD_MASK);
  if ((control1Before & cmd::CONTROL1_IMPLEMENTED_MASK) != eerdOn) {
    control1WriteAttempted = true;
    operation = ensureWrite(cmd::REG_CONTROL1, &eerdOn, 1, operationStart,
                            PRIMARY_CELL_OPERATION_TIMEOUT_MS, true, nullptr,
                            &callbackReturnedLate);
  }
  uint8_t control1Check = 0;
  if (operation.ok()) {
    operation = ensureRead(cmd::REG_CONTROL1, &control1Check, 1, operationStart,
                           PRIMARY_CELL_OPERATION_TIMEOUT_MS, true,
                           &callbackReturnedLate);
  }
  if (operation.ok() &&
      (control1Check & cmd::CONTROL1_IMPLEMENTED_MASK) != eerdOn) {
    operation = Status::Error(Err::EEPROM_VERIFY_FAILED,
                              "Primary ensure could not verify EERD");
  }
  accessStateVerified = operation.ok();

  uint8_t tempLsb = 0;
  if (operation.ok()) {
    operation = ensureWaitReady(operationStart, EEPROM_READY_TIMEOUT_MS,
                                EEPROM_READY_CHECK_CAP, tempLsb, true,
                                &callbackReturnedLate);
  }
  if (operation.ok()) {
    operation = ensureRead(cmd::REG_ACTIVE_PMU, &activeBefore, 1,
                           operationStart, PRIMARY_CELL_OPERATION_TIMEOUT_MS,
                           true, &callbackReturnedLate);
    activeValid = operation.ok();
  }
  if (operation.ok()) {
    safeActive = static_cast<uint8_t>(
        activeBefore & cmd::PMU_PRIMARY_PRESERVE_MASK);
    if ((activeBefore & cmd::PMU_IMPLEMENTED_MASK) != safeActive) {
      bool safeWriteAttempted = false;
      operation = ensureWrite(cmd::REG_ACTIVE_PMU, &safeActive, 1,
                              operationStart,
                              PRIMARY_CELL_OPERATION_TIMEOUT_MS, true,
                              &safeWriteAttempted, &callbackReturnedLate);
      safeActiveWriteAmbiguous = safeWriteAttempted && !operation.ok();
    }
  }
  uint8_t activeCheck = 0;
  if (operation.ok()) {
    operation = ensureRead(cmd::REG_ACTIVE_PMU, &activeCheck, 1,
                           operationStart, PRIMARY_CELL_OPERATION_TIMEOUT_MS,
                           true, &callbackReturnedLate);
  }
  if (operation.ok() &&
      (activeCheck & cmd::PMU_IMPLEMENTED_MASK) != safeActive) {
    operation = Status::Error(Err::EEPROM_VERIFY_FAILED,
                              "Primary ensure safe PMU verification failed");
  }
  safeValid = operation.ok();

  uint8_t persistentBefore = 0;
  if (operation.ok()) {
    operation = readPersistentC0ForEnsure(operationStart, persistentBefore,
                                          local, callbackReturnedLate);
    if (!operation.ok()) {
      local.failureStage = PrimaryCellFailureStage::READ_PERSISTENT;
    }
  } else {
    local.failureStage = PrimaryCellFailureStage::PREPARE_ACCESS;
  }
  if (operation.ok()) {
    local.persistentBefore = persistentBefore;
    local.persistentBeforeValid = true;
    target = static_cast<uint8_t>(
        (persistentBefore & cmd::PMU_PRIMARY_PRESERVE_MASK) |
        cmd::PMU_BSM_LEVEL);
    local.persistentTarget = target;
    if (persistentBefore == target) {
      persistenceTrusted = true;
      local.persistentAfter = persistentBefore;
      local.persistentAfterValid = true;
    }
  }

  if (operation.ok() && !persistenceTrusted) {
    uint32_t elapsed = _nowMs() - operationStart;
    if (elapsed >= PRIMARY_CELL_WRITE_START_CUTOFF_MS ||
        elapsed + PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS >=
            PRIMARY_CELL_OPERATION_TIMEOUT_MS) {
      operation = Status::Error(Err::TIMEOUT,
                                "Primary-cell write cutoff or cleanup reserve reached");
    }
    const uint8_t clearEef = cmd::EEPROM_CLEAR_EEF_VALUE;
    if (operation.ok()) {
      operation = ensureWrite(cmd::REG_TEMP_LSB, &clearEef, 1, operationStart,
                              PRIMARY_CELL_WRITE_START_CUTOFF_MS, true,
                              nullptr, &callbackReturnedLate);
    }
    if (operation.ok()) {
      operation = ensureRead(cmd::REG_TEMP_LSB, &tempLsb, 1, operationStart,
                             PRIMARY_CELL_WRITE_START_CUTOFF_MS, true,
                             &callbackReturnedLate);
      if (operation.ok() &&
          (tempLsb & (cmd::EEPROM_BUSY_MASK | cmd::EEPROM_EEF_MASK)) != 0) {
        operation = Status::Error(Err::EEPROM_WRITE_FAILED,
                                  "EEPROM not ready for primary-cell write");
      }
    }
    const uint8_t address = cmd::REG_ACTIVE_PMU;
    if (operation.ok()) {
      operation = ensureWrite(cmd::REG_EE_ADDRESS, &address, 1, operationStart,
                              PRIMARY_CELL_WRITE_START_CUTOFF_MS, true,
                              nullptr, &callbackReturnedLate);
    }
    uint8_t staged = 0;
    if (operation.ok()) {
      operation = ensureRead(cmd::REG_EE_ADDRESS, &staged, 1, operationStart,
                             PRIMARY_CELL_WRITE_START_CUTOFF_MS, true,
                             &callbackReturnedLate);
      if (operation.ok() && staged != address) {
        operation = Status::Error(Err::EEPROM_VERIFY_FAILED,
                                  "Primary-cell address staging mismatch");
      }
    }
    if (operation.ok()) {
      operation = ensureWrite(cmd::REG_EE_DATA, &target, 1, operationStart,
                              PRIMARY_CELL_WRITE_START_CUTOFF_MS, true,
                              nullptr, &callbackReturnedLate);
    }
    if (operation.ok()) {
      operation = ensureRead(cmd::REG_EE_DATA, &staged, 1, operationStart,
                             PRIMARY_CELL_WRITE_START_CUTOFF_MS, true,
                             &callbackReturnedLate);
      if (operation.ok() && staged != target) {
        operation = Status::Error(Err::EEPROM_VERIFY_FAILED,
                                  "Primary-cell data staging mismatch");
      }
    }
    if (operation.ok()) {
      operation = ensureRead(cmd::REG_TEMP_LSB, &tempLsb, 1, operationStart,
                             PRIMARY_CELL_WRITE_START_CUTOFF_MS, true,
                             &callbackReturnedLate);
      if (operation.ok() && (tempLsb & cmd::EEPROM_BUSY_MASK) != 0) {
        operation = Status::Error(Err::BUSY,
                                  "EEPROM busy immediately before write-one");
      }
    }

    Status writeCommandStatus = Status::Ok();
    uint32_t writeCommandStart = 0;
    bool writeCommandAttempted = false;
    bool writeCommandReturnedLate = false;
    if (operation.ok()) {
      elapsed = _nowMs() - operationStart;
      if (elapsed >= PRIMARY_CELL_WRITE_START_CUTOFF_MS ||
          elapsed + PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS >=
              PRIMARY_CELL_OPERATION_TIMEOUT_MS) {
        operation = Status::Error(Err::TIMEOUT,
                                  "Primary-cell write dispatch cutoff reached");
      } else if (writeCommandAttempts >= PRIMARY_CELL_WRITE_COMMAND_CAP) {
        operation = Status::Error(Err::EEPROM_WRITE_FAILED,
                                  "Primary-cell write command cap reached");
      } else {
        const uint8_t command = cmd::EEPROM_CMD_WRITE_ONE;
        writeCommandStart = _nowMs();
        writeCommandStatus = ensureWrite(cmd::REG_EE_COMMAND, &command, 1,
                                         operationStart,
                                         PRIMARY_CELL_WRITE_START_CUTOFF_MS,
                                         true, &writeCommandAttempted,
                                         &writeCommandReturnedLate);
        callbackReturnedLate = callbackReturnedLate ||
                               writeCommandReturnedLate;
        if (writeCommandAttempted) {
          ++writeCommandAttempts;
          local.writeCommandAttempted = true;
        } else {
          operation = writeCommandStatus;
        }
        if (writeCommandReturnedLate) {
          operation = writeCommandStatus;
        }
      }
    }
    if (operation.ok()) {
      operation = ensureWait(EEPROM_WRITE_SETTLE_MS, operationStart);
    }
    if (operation.ok()) {
      const uint32_t commandElapsed = _nowMs() - writeCommandStart;
      operation = commandElapsed >= EEPROM_WRITE_TIMEOUT_MS
          ? Status::Error(Err::TIMEOUT,
                          "Primary write-one completion phase expired")
          : ensureWaitReady(operationStart,
                            EEPROM_WRITE_TIMEOUT_MS - commandElapsed,
                            EEPROM_WRITE_CHECK_CAP, tempLsb, true,
                            &callbackReturnedLate);
    }
    bool eefClear = false;
    if (operation.ok()) {
      operation = ensureRead(cmd::REG_TEMP_LSB, &tempLsb, 1, operationStart,
                             PRIMARY_CELL_OPERATION_TIMEOUT_MS, true,
                             &callbackReturnedLate);
      eefClear = operation.ok() && (tempLsb & cmd::EEPROM_EEF_MASK) == 0;
    }
    uint8_t persistentAfter = 0;
    if (operation.ok()) {
      operation = readPersistentC0ForEnsure(operationStart, persistentAfter,
                                             local, callbackReturnedLate);
      local.persistentAfter = persistentAfter;
      local.persistentAfterValid = operation.ok();
    }
    if (operation.ok() && !eefClear) {
      operation = Status::Error(Err::EEPROM_WRITE_FAILED,
                                "Primary-cell EEPROM reported write failure");
    } else if (operation.ok() && persistentAfter != target) {
      operation = Status::Error(Err::EEPROM_VERIFY_FAILED,
                                "Primary-cell durable verification failed");
    }
    if (operation.ok()) {
      persistenceTrusted = true;
      local.writeDurablyVerified = true;
      // A possibly committed command error is reconciled only by this direct proof.
    } else if (!writeCommandStatus.ok()) {
      // Durable proof did not reconcile the ambiguous mutating callback.
      // Preserve the first transport failure as the primary operation error;
      // cleanup evidence remains separate in the public report.
      operation = writeCommandStatus;
    }
  }

  if (!operation.ok() && local.failureStage == PrimaryCellFailureStage::NONE) {
    local.failureStage = local.writeCommandAttempted
        ? PrimaryCellFailureStage::VERIFY_PERSISTENT
        : PrimaryCellFailureStage::WRITE_PERSISTENT;
  }
  local.operationStatus = operation;
  Status cleanup = cleanupPrimaryCellEnsure(operationStart, control1Before,
                                             control1Valid,
                                             control1WriteAttempted,
                                             accessStateVerified, activeBefore,
                                             activeValid, safeActive, safeValid,
                                             safeActiveWriteAmbiguous,
                                             persistenceTrusted, target, local,
                                             callbackReturnedLate);
  local.cleanupStatus = cleanup;

  if (operation.ok() && cleanup.ok()) {
    local.outcome = local.writeCommandAttempted
        ? PrimaryCellConfigurationOutcome::EEPROM_UPDATED
        : PrimaryCellConfigurationOutcome::ALREADY_CONFIGURED;
    local.failureStage = PrimaryCellFailureStage::NONE;
    local.operationStatus = Status::Ok();
    report = local;
    return Status::Ok();
  }

  local.outcome = PrimaryCellConfigurationOutcome::FAILED;
  if (!cleanup.ok() &&
      local.failureStage != PrimaryCellFailureStage::SETTLE) {
    local.failureStage = PrimaryCellFailureStage::CLEANUP;
  }
  report = local;
  if (callbackReturnedLate) {
    return Status::Error(Err::TIMEOUT,
                         "Primary ensure callback returned after its bound");
  }
  return !cleanup.ok() ? cleanup : operation;
}

// ===== Clock Output Operations =====

Status RV3032::setClkoutEnabled(bool enabled) {
  return updateRegisterSingle(
      cmd::REG_ACTIVE_PMU, cmd::PMU_IMPLEMENTED_MASK,
      cmd::PMU_NCLKE_MASK, enabled ? 0 : cmd::PMU_NCLKE_MASK);
}

Status RV3032::getClkoutEnabled(bool& enabled) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t coe = 0;
  Status st = readRegister(cmd::REG_ACTIVE_PMU, coe);
  if (!st.ok()) {
    return st;
  }

  enabled = ((coe & cmd::PMU_CLKOUT_DISABLE) == 0);
  return Status::Ok();
}

Status RV3032::setClkoutFrequency(ClkoutFrequency freq) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  const uint8_t freqRaw = static_cast<uint8_t>(freq);
  if (freqRaw > kMaxClkoutFrequency) {
    return Status::Error(Err::INVALID_PARAM, "CLKOUT frequency out of range");
  }

  const bool persist = _config.enableEepromWrites;
  if (isJobBusy() || _eeprom.state != EepromState::Idle ||
      (!persist && _eeprom.queueCount > 0)) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::RegisterUpdate;
  _job.clkoutConfig.xtalFrequency = freq;
  _job.clkoutLegacyFrequency = true;
  _job.persistRegisterUpdate = persist;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                  "CLKOUT configuration in progress");
  _job.state = JobState::ClkoutReadActive;
  return _job.lastStatus;
}

Status RV3032::getClkoutFrequency(ClkoutFrequency& freq) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t clkout = 0;
  Status st = readRegister(cmd::REG_ACTIVE_CLKOUT2, clkout);
  if (!st.ok()) {
    return st;
  }
  if ((clkout & cmd::CLKOUT_OS_MASK) != 0) {
    return Status::Error(
        Err::INVALID_PARAM,
        "High-frequency CLKOUT requires getClkoutConfig()");
  }

  uint8_t value = static_cast<uint8_t>((clkout & cmd::CLKOUT_FREQ_MASK) >> cmd::CLKOUT_FREQ_SHIFT);
  if (value > 3) {
    value = 0;
  }
  freq = static_cast<ClkoutFrequency>(value);

  return Status::Ok();
}

Status RV3032::setClkoutConfig(const ClkoutConfig& config) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (config.highFrequencyDivider < 1 ||
      config.highFrequencyDivider > 8192) {
    return Status::Error(Err::INVALID_PARAM,
                         "High-frequency divider must be 1..8192");
  }
  if (static_cast<uint8_t>(config.xtalFrequency) > kMaxClkoutFrequency) {
    return Status::Error(Err::INVALID_PARAM, "CLKOUT frequency out of range");
  }
  const bool persist = _config.enableEepromWrites;
  if (isJobBusy() || _eeprom.state != EepromState::Idle ||
      (!persist && _eeprom.queueCount > 0)) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::RegisterUpdate;
  _job.clkoutConfig = config;
  _job.persistRegisterUpdate = persist;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                  "CLKOUT configuration in progress");
  _job.state = JobState::ClkoutReadActive;
  return _job.lastStatus;
}

Status RV3032::getClkoutConfig(ClkoutConfig& config) {
  uint8_t values[4] = {};
  Status st = readRegs(cmd::REG_ACTIVE_PMU, values, sizeof(values));
  if (!st.ok()) return st;
  config.enabled = (values[0] & cmd::PMU_NCLKE_MASK) == 0;
  config.highFrequencyMode = (values[3] & cmd::CLKOUT_OS_MASK) != 0;
  config.xtalFrequency = static_cast<ClkoutFrequency>(
      (values[3] & cmd::CLKOUT_FREQ_MASK) >> cmd::CLKOUT_FREQ_SHIFT);
  const uint16_t hfd = static_cast<uint16_t>(values[2]) |
      (static_cast<uint16_t>(values[3] & cmd::CLKOUT_HFD_HIGH_MASK) << 8);
  config.highFrequencyDivider = static_cast<uint16_t>(hfd + 1U);
  return Status::Ok();
}

Status RV3032::setClockInterruptEnabled(bool enabled) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::CTRL2_CLKIE_BIT);
  return updateRegisterSingle(cmd::REG_CONTROL2, cmd::CONTROL2_IMPLEMENTED_MASK,
                              bit, enabled ? bit : 0);
}

Status RV3032::getClockInterruptEnabled(bool& enabled) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_CONTROL2, &value, 1);
  if (st.ok()) enabled = (value & (1u << cmd::CTRL2_CLKIE_BIT)) != 0;
  return st;
}

Status RV3032::setClockInterruptMaskConfig(
    const ClockInterruptMaskConfig& config) {
  const uint8_t value = static_cast<uint8_t>(
      (config.longStopDelay ? (1u << cmd::CLOCK_INT_CLKD_BIT) : 0) |
      (config.interruptDelayEnabled ? (1u << cmd::CLOCK_INT_INTDE_BIT) : 0) |
      (config.eventSource ? (1u << cmd::CLOCK_INT_CEIE_BIT) : 0) |
      (config.alarmSource ? (1u << cmd::CLOCK_INT_CAIE_BIT) : 0) |
      (config.timerSource ? (1u << cmd::CLOCK_INT_CTIE_BIT) : 0) |
      (config.updateSource ? (1u << cmd::CLOCK_INT_CUIE_BIT) : 0) |
      (config.tempHighSource ? (1u << cmd::CLOCK_INT_CTHIE_BIT) : 0) |
      (config.tempLowSource ? (1u << cmd::CLOCK_INT_CTLIE_BIT) : 0));
  return updateRegisterSingle(
      cmd::REG_CLOCK_INT_MASK, cmd::CLOCK_INT_MASK_IMPLEMENTED_MASK,
      cmd::CLOCK_INT_MASK_IMPLEMENTED_MASK, value);
}

Status RV3032::getClockInterruptMaskConfig(
    ClockInterruptMaskConfig& config) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_CLOCK_INT_MASK, &value, 1);
  if (!st.ok()) return st;
  config.longStopDelay = (value & (1u << cmd::CLOCK_INT_CLKD_BIT)) != 0;
  config.interruptDelayEnabled =
      (value & (1u << cmd::CLOCK_INT_INTDE_BIT)) != 0;
  config.eventSource = (value & (1u << cmd::CLOCK_INT_CEIE_BIT)) != 0;
  config.alarmSource = (value & (1u << cmd::CLOCK_INT_CAIE_BIT)) != 0;
  config.timerSource = (value & (1u << cmd::CLOCK_INT_CTIE_BIT)) != 0;
  config.updateSource = (value & (1u << cmd::CLOCK_INT_CUIE_BIT)) != 0;
  config.tempHighSource = (value & (1u << cmd::CLOCK_INT_CTHIE_BIT)) != 0;
  config.tempLowSource = (value & (1u << cmd::CLOCK_INT_CTLIE_BIT)) != 0;
  return Status::Ok();
}

Status RV3032::getClockOutputFlag(bool& triggered) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_TEMP_LSB, &value, 1);
  if (st.ok()) {
    triggered = (value & cmd::TEMP_CLKF_MASK) != 0;
  }
  return st;
}

Status RV3032::clearClockOutputFlag() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::RegisterUpdate;
  _job.registerUpdateReg = cmd::REG_TEMP_LSB;
  _job.registerUpdateImplementedMask = static_cast<uint8_t>(
      cmd::EEPROM_EEF_MASK | cmd::TEMP_CLKF_MASK | cmd::TEMP_BSF_MASK);
  _job.registerUpdateClearMask = cmd::TEMP_CLKF_MASK;
  _job.registerUpdateRequiredMask = cmd::TEMP_CLKF_MASK;
  _job.registerUpdateForbiddenMask = cmd::EEPROM_BUSY_MASK;
  _job.registerUpdateGuardedClear = true;
  _job.registerUpdateGuardFailureIsBusy = true;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                  "Clock-output flag clear in progress");
  _job.state = JobState::RegisterUpdateRead;
  return _job.lastStatus;
}

// ===== Calibration Operations =====

Status RV3032::setOffsetPpm(float ppm) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!std::isfinite(ppm)) {
    return Status::Error(Err::INVALID_PARAM, "Offset must be finite");
  }

  const float steps = ppm / kOffsetPpmPerStep;
  int value = lrintf(steps);

  if (value < -32 || value > 31 ||
      ppm < (-32.0f * kOffsetPpmPerStep) ||
      ppm > (31.0f * kOffsetPpmPerStep)) {
    return Status::Error(Err::INVALID_PARAM, "Offset is outside vendor range");
  }

  const uint8_t raw = static_cast<uint8_t>(value & cmd::OFFSET_VALUE_MASK);
  return updateRegisterSingle(cmd::REG_ACTIVE_OFFSET,
                              cmd::OFFSET_REGISTER_IMPLEMENTED_MASK,
                              cmd::OFFSET_VALUE_MASK, raw);
}

Status RV3032::getOffsetPpm(float& ppm) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t raw = 0;
  Status st = readRegister(cmd::REG_ACTIVE_OFFSET, raw);
  if (!st.ok()) {
    return st;
  }

  raw = static_cast<uint8_t>(raw & cmd::OFFSET_VALUE_MASK);
  int8_t signedRaw = (raw & 0x20) ? static_cast<int8_t>(raw | 0xC0) : static_cast<int8_t>(raw);
  ppm = static_cast<float>(signedRaw) * kOffsetPpmPerStep;

  return Status::Ok();
}

Status RV3032::setPowerOnResetInterruptEnabled(bool enabled) {
  return updateRegisterSingle(
      cmd::REG_ACTIVE_OFFSET, cmd::OFFSET_REGISTER_IMPLEMENTED_MASK,
      cmd::OFFSET_PORIE_MASK, enabled ? cmd::OFFSET_PORIE_MASK : 0);
}

Status RV3032::getPowerOnResetInterruptEnabled(bool& enabled) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_ACTIVE_OFFSET, &value, 1);
  if (st.ok()) enabled = (value & cmd::OFFSET_PORIE_MASK) != 0;
  return st;
}

Status RV3032::setVoltageLowInterruptEnabled(bool enabled) {
  return updateRegisterSingle(
      cmd::REG_ACTIVE_OFFSET, cmd::OFFSET_REGISTER_IMPLEMENTED_MASK,
      cmd::OFFSET_VLIE_MASK, enabled ? cmd::OFFSET_VLIE_MASK : 0);
}

Status RV3032::getVoltageLowInterruptEnabled(bool& enabled) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_ACTIVE_OFFSET, &value, 1);
  if (st.ok()) enabled = (value & cmd::OFFSET_VLIE_MASK) != 0;
  return st;
}

// ===== Temperature Sensor =====

Status RV3032::readTemperatureC(float& celsius) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t buf[2] = {0};
  Status st = readRegs(cmd::REG_TEMP_LSB, buf, sizeof(buf));
  if (!st.ok()) {
    return st;
  }

  celsius = static_cast<float>(decodeTemperatureRaw(buf)) / 16.0f;

  return Status::Ok();
}

Status RV3032::startReadCoherentTemperatureJob(
    uint32_t nowMs, uint32_t operationTimeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (operationTimeoutMs == 0 || operationTimeoutMs > 1000) {
    return Status::Error(Err::INVALID_PARAM,
                         "Temperature timeout must be 1..1000 ms");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::ReadCoherentTemperature;
  _job.state = JobState::ReadTemperatureFirst;
  _job.deadlineMs = nowMs + operationTimeoutMs;
  _job.deadlineActive = true;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                  "Temperature job in progress");
  return _job.lastStatus;
}

Status RV3032::getReadCoherentTemperatureJobResult(
    CoherentTemperatureResult& result) const {
  if (_job.activeKind == JobKind::ReadCoherentTemperature) {
    return Status::Error(Err::IN_PROGRESS,
                         "Temperature job in progress");
  }
  if (_job.completedKind != JobKind::ReadCoherentTemperature) {
    return Status::Error(Err::JOB_RESULT_UNAVAILABLE,
                         "Temperature result unavailable");
  }
  result = _job.coherentTemperature;
  return _job.completedStatus;
}

Status RV3032::setTemperatureEventConfig(
    const TemperatureEventConfig& config) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  const uint8_t implemented[6] = {
      cmd::CONTROL3_IMPLEMENTED_MASK, cmd::TS_CONTROL_IMPLEMENTED_MASK,
      0xFF, cmd::EVI_IMPLEMENTED_MASK, 0xFF, 0xFF};
  const uint8_t control3Mask = static_cast<uint8_t>(
      (1u << cmd::CTRL3_THE_BIT) | (1u << cmd::CTRL3_TLE_BIT) |
      (1u << cmd::CTRL3_THIE_BIT) | (1u << cmd::CTRL3_TLIE_BIT));
  const uint8_t overwriteMask = static_cast<uint8_t>(
      (1u << cmd::TS_TLOW_OVERWRITE_BIT) |
      (1u << cmd::TS_THIGH_OVERWRITE_BIT));
  const uint8_t clear[6] = {control3Mask, overwriteMask, 0, 0, 0xFF, 0xFF};
  const uint8_t control3 = static_cast<uint8_t>(
      (config.highEventEnabled ? (1u << cmd::CTRL3_THE_BIT) : 0) |
      (config.lowEventEnabled ? (1u << cmd::CTRL3_TLE_BIT) : 0) |
      (config.highInterruptEnabled ? (1u << cmd::CTRL3_THIE_BIT) : 0) |
      (config.lowInterruptEnabled ? (1u << cmd::CTRL3_TLIE_BIT) : 0));
  const uint8_t overwrite = static_cast<uint8_t>(
      (config.lowTimestampOverwrite ? (1u << cmd::TS_TLOW_OVERWRITE_BIT) : 0) |
      (config.highTimestampOverwrite ? (1u << cmd::TS_THIGH_OVERWRITE_BIT) : 0));
  const uint8_t set[6] = {
      control3, overwrite, 0, 0,
      static_cast<uint8_t>(config.lowThresholdC),
      static_cast<uint8_t>(config.highThresholdC)};
  _job = JobOp{};
  _job.activeKind = JobKind::RegisterUpdate;
  _job.registerBlockReg = cmd::REG_CONTROL3;
  _job.registerBlockLen = 6;
  std::memcpy(_job.registerBlockImplemented, implemented, 6);
  std::memcpy(_job.registerBlockClear, clear, 6);
  std::memcpy(_job.registerBlockSet, set, 6);
  _job.lastStatus = Status::Error(
      Err::IN_PROGRESS, "Temperature configuration in progress");
  _job.state = JobState::TemperatureConfigRead;
  return _job.lastStatus;
}

Status RV3032::getTemperatureEventConfig(TemperatureEventConfig& config) {
  uint8_t values[6] = {};
  Status st = readRegs(cmd::REG_CONTROL3, values, sizeof(values));
  if (!st.ok()) return st;
  config.highEventEnabled = (values[0] & (1u << cmd::CTRL3_THE_BIT)) != 0;
  config.lowEventEnabled = (values[0] & (1u << cmd::CTRL3_TLE_BIT)) != 0;
  config.highInterruptEnabled = (values[0] & (1u << cmd::CTRL3_THIE_BIT)) != 0;
  config.lowInterruptEnabled = (values[0] & (1u << cmd::CTRL3_TLIE_BIT)) != 0;
  config.lowTimestampOverwrite =
      (values[1] & (1u << cmd::TS_TLOW_OVERWRITE_BIT)) != 0;
  config.highTimestampOverwrite =
      (values[1] & (1u << cmd::TS_THIGH_OVERWRITE_BIT)) != 0;
  config.lowThresholdC = static_cast<int8_t>(values[4]);
  config.highThresholdC = static_cast<int8_t>(values[5]);
  return Status::Ok();
}

Status RV3032::setTemperatureReference(int16_t referenceRaw) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  const uint16_t raw = static_cast<uint16_t>(referenceRaw);
  const uint8_t values[2] = {static_cast<uint8_t>(raw & 0xFFu),
                             static_cast<uint8_t>(raw >> 8)};
  const uint8_t implemented[2] = {0xFF, 0xFF};
  const uint8_t clear[2] = {0xFF, 0xFF};
  return updateRegisterBlock(cmd::REG_ACTIVE_TREFERENCE0, 2,
                             implemented, clear, values);
}

Status RV3032::getTemperatureReference(int16_t& referenceRaw) {
  uint8_t values[2] = {};
  Status st = readRegs(cmd::REG_ACTIVE_TREFERENCE0, values, sizeof(values));
  if (st.ok()) {
    const uint16_t raw = static_cast<uint16_t>(values[0]) |
        (static_cast<uint16_t>(values[1]) << 8);
    referenceRaw = static_cast<int16_t>(raw);
  }
  return st;
}

Status RV3032::getTemperatureFlags(bool& lowTriggered,
                                    bool& highTriggered) {
  uint8_t status = 0;
  Status st = readStatus(status);
  if (st.ok()) {
    lowTriggered = (status & (1u << cmd::STATUS_TLF_BIT)) != 0;
    highTriggered = (status & (1u << cmd::STATUS_THF_BIT)) != 0;
  }
  return st;
}

Status RV3032::clearTemperatureFlags() {
  return clearStatus(static_cast<uint8_t>(
      (1u << cmd::STATUS_TLF_BIT) | (1u << cmd::STATUS_THF_BIT)));
}

// ===== External Event Input =====

Status RV3032::setEviEdge(bool rising) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::EVI_EB_BIT);
  return updateRegisterSingle(cmd::REG_EVI_CONTROL, cmd::EVI_IMPLEMENTED_MASK,
                              bit, rising ? bit : 0);
}

Status RV3032::setEviDebounce(EviDebounce debounce) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  const uint8_t debounceRaw = static_cast<uint8_t>(debounce);
  if (debounceRaw > kMaxEviDebounce) {
    return Status::Error(Err::INVALID_PARAM, "EVI debounce out of range");
  }

  return updateRegisterSingle(
      cmd::REG_EVI_CONTROL, cmd::EVI_IMPLEMENTED_MASK, cmd::EVI_DB_MASK,
      static_cast<uint8_t>((debounceRaw << cmd::EVI_DB_SHIFT) & cmd::EVI_DB_MASK));
}

Status RV3032::setEviOverwrite(bool enable) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::TS_EVI_OVERWRITE_BIT);
  return updateRegisterSingle(cmd::REG_TS_CONTROL,
                              cmd::TS_CONTROL_IMPLEMENTED_MASK,
                              bit, enable ? bit : 0);
}

Status RV3032::getEviConfig(EviConfig& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t values[3] = {};
  Status st = readRegs(cmd::REG_TS_CONTROL, values, sizeof(values));
  if (!st.ok()) return st;
  const uint8_t ts = values[0];
  const uint8_t evi = values[2];

  out.rising = ((evi & (1u << cmd::EVI_EB_BIT)) != 0);
  out.debounce = static_cast<EviDebounce>((evi & cmd::EVI_DB_MASK) >> cmd::EVI_DB_SHIFT);
  out.overwrite = ((ts & (1u << cmd::TS_OVERWRITE_BIT)) != 0);
  out.synchronized = (evi & (1u << cmd::EVI_ESYN_BIT)) != 0;
  out.clkoutStopDelay = (evi & (1u << cmd::EVI_CLKDE_BIT)) != 0;

  return Status::Ok();
}

Status RV3032::setEventInterruptEnabled(bool enabled) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::CTRL2_EIE_BIT);
  return updateRegisterSingle(cmd::REG_CONTROL2, cmd::CONTROL2_IMPLEMENTED_MASK,
                              bit, enabled ? bit : 0);
}

Status RV3032::getEventInterruptEnabled(bool& enabled) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_CONTROL2, &value, 1);
  if (st.ok()) enabled = (value & (1u << cmd::CTRL2_EIE_BIT)) != 0;
  return st;
}

Status RV3032::getEventFlag(bool& triggered) {
  uint8_t value = 0;
  Status st = readStatus(value);
  if (st.ok()) triggered = (value & (1u << cmd::STATUS_EVF_BIT)) != 0;
  return st;
}

Status RV3032::clearEventFlag() {
  return clearStatus(static_cast<uint8_t>(1u << cmd::STATUS_EVF_BIT));
}

Status RV3032::setEventSynchronizationEnabled(bool enabled) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::EVI_ESYN_BIT);
  return updateRegisterSingle(cmd::REG_EVI_CONTROL, cmd::EVI_IMPLEMENTED_MASK,
                              bit, enabled ? bit : 0);
}

Status RV3032::getEventSynchronizationEnabled(bool& enabled) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_EVI_CONTROL, &value, 1);
  if (st.ok()) enabled = (value & (1u << cmd::EVI_ESYN_BIT)) != 0;
  return st;
}

Status RV3032::setClkoutStopDelayEnabled(bool enabled) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::EVI_CLKDE_BIT);
  return updateRegisterSingle(cmd::REG_EVI_CONTROL, cmd::EVI_IMPLEMENTED_MASK,
                              bit, enabled ? bit : 0);
}

Status RV3032::getClkoutStopDelayEnabled(bool& enabled) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_EVI_CONTROL, &value, 1);
  if (st.ok()) enabled = (value & (1u << cmd::EVI_CLKDE_BIT)) != 0;
  return st;
}

Status RV3032::setStopEnabled(bool enabled) {
  const uint8_t bit = static_cast<uint8_t>(1u << cmd::CTRL2_STOP_BIT);
  return updateRegisterSingle(cmd::REG_CONTROL2, cmd::CONTROL2_IMPLEMENTED_MASK,
                              bit, enabled ? bit : 0);
}

Status RV3032::getStopEnabled(bool& enabled) {
  uint8_t value = 0;
  Status st = readRegs(cmd::REG_CONTROL2, &value, 1);
  if (st.ok()) enabled = (value & (1u << cmd::CTRL2_STOP_BIT)) != 0;
  return st;
}

Status RV3032::setGeneralPurposeBits(bool gp0, bool gp1) {
  const uint8_t implemented[2] = {cmd::CONTROL1_IMPLEMENTED_MASK,
                                  cmd::CONTROL2_IMPLEMENTED_MASK};
  const uint8_t clear[2] = {
      static_cast<uint8_t>(1u << cmd::CTRL1_GP0_BIT),
      static_cast<uint8_t>(1u << cmd::CTRL2_GP1_BIT)};
  const uint8_t set[2] = {
      static_cast<uint8_t>(gp0 ? (1u << cmd::CTRL1_GP0_BIT) : 0),
      static_cast<uint8_t>(gp1 ? (1u << cmd::CTRL2_GP1_BIT) : 0)};
  return updateRegisterBlock(cmd::REG_CONTROL1, 2, implemented, clear, set);
}

Status RV3032::getGeneralPurposeBits(bool& gp0, bool& gp1) {
  uint8_t values[2] = {};
  Status st = readRegs(cmd::REG_CONTROL1, values, sizeof(values));
  if (st.ok()) {
    gp0 = (values[0] & (1u << cmd::CTRL1_GP0_BIT)) != 0;
    gp1 = (values[1] & (1u << cmd::CTRL2_GP1_BIT)) != 0;
  }
  return st;
}

Status RV3032::unlockPasswordProtection(
    const PasswordCredential& credential) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  Status st = writeRegs(cmd::REG_PASSWORD0, credential.bytes,
                        sizeof(credential.bytes));
  if (st.ok()) {
    // The active password inputs are write-only/read-zero. Callback success is
    // not authentication evidence; a later protected operation must prove it.
    _passwordStatus = PasswordProtectionStatus{};
  }
  return st;
}

Status RV3032::startConfigurePasswordProtectionJob(
    const PasswordCredential& currentCredential,
    const PasswordCredential& newCredential,
    bool enable, uint32_t nowMs, uint32_t operationTimeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  if (!_config.enableEepromWrites) {
    return Status::Error(Err::INVALID_CONFIG,
                         "Password persistence requires generic EEPROM authority");
  }
  if (operationTimeoutMs <= 300 || operationTimeoutMs > 10000) {
    return Status::Error(Err::INVALID_PARAM,
                         "Password operation timeout out of range");
  }
  if (enable && newCredential.bytes[0] == 0 && newCredential.bytes[1] == 0 &&
      newCredential.bytes[2] == 0 && newCredential.bytes[3] == 0) {
    return Status::Error(Err::INVALID_PARAM,
                         "All-zero persistent password is not permitted");
  }

  _job = JobOp{};
  _job.activeKind = JobKind::PasswordProtection;
  _job.state = JobState::Persistent;
  _job.persistentState = EepromState::WritePassword;
  _job.persistentWriteMode = true;
  _job.persistentUseAddressList = true;
  _job.currentCredential = currentCredential;
  _job.newCredential = newCredential;
  _job.passwordEnable = enable;
  _job.deadlineMs = nowMs + operationTimeoutMs;
  _job.mutationCutoffMs = nowMs + (operationTimeoutMs - 300);
  _job.deadlineActive = true;
  _job.mutationCutoffActive = true;
  if (enable) {
    _job.persistentLength = 6;
    _job.persistentAddresses[0] = cmd::REG_EEPROM_PW_ENABLE;
    _job.persistentAddresses[1] = cmd::REG_EEPROM_PASSWORD0;
    _job.persistentAddresses[2] = cmd::REG_EEPROM_PASSWORD1;
    _job.persistentAddresses[3] = cmd::REG_EEPROM_PASSWORD2;
    _job.persistentAddresses[4] = cmd::REG_EEPROM_PASSWORD3;
    _job.persistentAddresses[5] = cmd::REG_EEPROM_PW_ENABLE;
    _job.userRamBuf[0] = 0;
    std::memcpy(&_job.userRamBuf[1], newCredential.bytes, 4);
    _job.userRamBuf[5] = 0xFF;
  } else {
    _job.persistentLength = 1;
    _job.persistentAddresses[0] = cmd::REG_EEPROM_PW_ENABLE;
    _job.userRamBuf[0] = 0;
  }
  _passwordStatus = PasswordProtectionStatus{};
  _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                  "Password protection job in progress");
  return _job.lastStatus;
}

Status RV3032::getPasswordProtectionStatus(
    PasswordProtectionStatus& status) const {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (_job.activeKind == JobKind::PasswordProtection) {
    return Status::Error(Err::IN_PROGRESS,
                         "Password protection job in progress");
  }
  status = _passwordStatus;
  return Status::Ok();
}

Status RV3032::readTimestamp(TimestampSource source, Timestamp& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t startReg = 0;
  size_t len = 0;
  if (!timestampBlock(source, startReg, len)) {
    return Status::Error(Err::INVALID_PARAM, "Timestamp source out of range");
  }

  uint8_t buf[8] = {0};
  Status st = readRegs(startReg, buf, len);
  if (!st.ok()) {
    return st;
  }

  const bool hasHundredths = (source == TimestampSource::Evi);
  const size_t timeOffset = hasHundredths ? 2U : 1U;
  bool allZero = buf[0] == 0;
  for (size_t i = 1; i < len; ++i) {
    allZero = allZero && buf[i] == 0;
  }
  if (allZero) {
    out = Timestamp{};
    out.hasHundredths = hasHundredths;
    return Status::Ok();
  }
  if ((buf[timeOffset] & 0x80u) != 0 ||
      (buf[timeOffset + 1U] & 0x80u) != 0 ||
      (buf[timeOffset + 2U] & 0xC0u) != 0 ||
      (buf[timeOffset + 3U] & 0xC0u) != 0 ||
      (buf[timeOffset + 4U] & 0xE0u) != 0) {
    return Status::Error(Err::INVALID_DATETIME,
                         "Timestamp block contains reserved bits");
  }
  const uint8_t hundredthsReg = hasHundredths ? buf[1] : 0;
  const uint8_t secReg = static_cast<uint8_t>(buf[timeOffset] & 0x7F);
  const uint8_t minReg = static_cast<uint8_t>(buf[timeOffset + 1U] & 0x7F);
  const uint8_t hourReg = static_cast<uint8_t>(buf[timeOffset + 2U] & 0x3F);
  const uint8_t dayReg = static_cast<uint8_t>(buf[timeOffset + 3U] & 0x3F);
  const uint8_t monthReg = static_cast<uint8_t>(buf[timeOffset + 4U] & 0x1F);
  const uint8_t yearReg = buf[timeOffset + 5U];

  if (!isValidBcd(secReg) || !isValidBcd(minReg) || !isValidBcd(hourReg) ||
      !isValidBcd(dayReg) || !isValidBcd(monthReg) || !isValidBcd(yearReg) ||
      (hasHundredths && !isValidBcd(hundredthsReg))) {
    return Status::Error(Err::INVALID_DATETIME, "Timestamp block contains invalid BCD");
  }

  const uint8_t hundredths = hasHundredths ? bcdToBin(hundredthsReg) : 0;
  if (hasHundredths && hundredths > 99U) {
    return Status::Error(Err::INVALID_DATETIME, "Timestamp hundredths out of range");
  }

  DateTime dt;
  dt.second = bcdToBin(secReg);
  dt.minute = bcdToBin(minReg);
  dt.hour = bcdToBin(hourReg);
  dt.day = bcdToBin(dayReg);
  dt.month = bcdToBin(monthReg);
  dt.year = static_cast<uint16_t>(2000 + bcdToBin(yearReg));
  dt.weekday = computeWeekday(dt.year, dt.month, dt.day);
  if (!isValidDateTime(dt)) {
    return Status::Error(Err::INVALID_DATETIME, "Timestamp block contains invalid date/time");
  }

  out.count = buf[0];
  out.timeValid = true;
  out.hasHundredths = hasHundredths;
  out.hundredths = hundredths;
  out.time = dt;
  return Status::Ok();
}

Status RV3032::resetTimestamp(TimestampSource source) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t bit = 0;
  if (!timestampResetBit(source, bit)) {
    return Status::Error(Err::INVALID_PARAM, "Timestamp source out of range");
  }

  if (source == TimestampSource::Evi) {
    if (!workIdle()) {
      return Status::Error(Err::BUSY, "Driver work already in progress");
    }
    _job = JobOp{};
    _job.activeKind = JobKind::RegisterUpdate;
    _job.registerUpdateReg = cmd::REG_TS_CONTROL;
    _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                    "EVI timestamp reset in progress");
    _job.state = JobState::EviResetRead;
    return _job.lastStatus;
  }

  const uint8_t mask = static_cast<uint8_t>(1u << bit);
  return updateRegisterSingle(cmd::REG_TS_CONTROL,
                              cmd::TS_CONTROL_IMPLEMENTED_MASK,
                              mask, mask);
}

// ===== Status Operations =====

Status RV3032::readStatus(uint8_t& status) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  return readRegister(cmd::REG_STATUS, status);
}

Status RV3032::readStatusFlags(StatusFlags& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t status = 0;
  Status st = readRegister(cmd::REG_STATUS, status);
  if (!st.ok()) {
    return st;
  }

  out.tempHigh = (status & (1u << cmd::STATUS_THF_BIT)) != 0;
  out.tempLow = (status & (1u << cmd::STATUS_TLF_BIT)) != 0;
  out.update = (status & (1u << cmd::STATUS_UF_BIT)) != 0;
  out.timer = (status & (1u << cmd::STATUS_TF_BIT)) != 0;
  out.alarm = (status & (1u << cmd::STATUS_AF_BIT)) != 0;
  out.event = (status & (1u << cmd::STATUS_EVF_BIT)) != 0;
  out.powerOnReset = (status & (1u << cmd::STATUS_PORF_BIT)) != 0;
  out.voltageLow = (status & (1u << cmd::STATUS_VLF_BIT)) != 0;

  return Status::Ok();
}

Status RV3032::clearStatus(uint8_t mask) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (mask == 0) {
    return Status::Ok();
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  const uint8_t temperatureFlagMask = static_cast<uint8_t>(
      (1u << cmd::STATUS_THF_BIT) | (1u << cmd::STATUS_TLF_BIT));
  _job = JobOp{};
  _job.activeKind = JobKind::RegisterUpdate;
  _job.registerUpdateReg = cmd::REG_STATUS;
  _job.registerUpdateImplementedMask = 0xFF;
  _job.registerUpdateClearMask = mask;
  _job.registerUpdateRequiredMask = mask;
  _job.registerUpdateForbiddenMask = static_cast<uint8_t>(
      temperatureFlagMask & static_cast<uint8_t>(~mask));
  _job.registerUpdateGuardedClear = true;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                  "Status clear in progress");
  _job.state = JobState::RegisterUpdateRead;
  return _job.lastStatus;
}

Status RV3032::getEepromHardwareFlags(EepromHardwareFlags& out) {
  bool busy = false;
  bool failed = false;
  Status st = readEepromFlags(busy, failed);
  if (st.ok()) {
    out.busy = busy;
    out.writeFailed = failed;
  }
  return st;
}

Status RV3032::clearEepromErrorFlag() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::RegisterUpdate;
  _job.registerUpdateReg = cmd::REG_TEMP_LSB;
  _job.registerUpdateImplementedMask = static_cast<uint8_t>(
      cmd::EEPROM_EEF_MASK | cmd::TEMP_CLKF_MASK | cmd::TEMP_BSF_MASK);
  _job.registerUpdateClearMask = cmd::EEPROM_EEF_MASK;
  _job.registerUpdateRequiredMask = cmd::EEPROM_EEF_MASK;
  _job.registerUpdateForbiddenMask = cmd::EEPROM_BUSY_MASK;
  _job.registerUpdateGuardedClear = true;
  _job.registerUpdateGuardFailureIsBusy = true;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                  "EEPROM failure flag clear in progress");
  _job.state = JobState::RegisterUpdateRead;
  return _job.lastStatus;
}

// ===== Low-Level Operations =====

Status RV3032::readRegister(uint8_t reg, uint8_t& value) {
  if (!isPublicReadableAddress(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Register is not publicly readable");
  }
  uint8_t buf = 0;
  Status st = readRegs(reg, &buf, 1);
  if (st.ok()) {
    value = buf;
  }
  return st;
}

Status RV3032::writeRegister(uint8_t reg, uint8_t value) {
  if (!isRawWritableAddress(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Register is not publicly writable");
  }
  return writeRegs(reg, &value, 1);
}

Status RV3032::readRegisters(uint8_t reg, uint8_t* buf, size_t len) {
  if (len == 0 || static_cast<uint16_t>(reg) + len - 1U > 0xFFu) {
    return Status::Error(Err::INVALID_PARAM, "Register block out of range");
  }
  for (size_t i = 0; i < len; ++i) {
    if (!isPublicReadableAddress(static_cast<uint8_t>(reg + i))) {
      return Status::Error(Err::INVALID_PARAM, "Register block is not publicly readable");
    }
  }
  return readRegs(reg, buf, len);
}

Status RV3032::writeRegisters(uint8_t reg, const uint8_t* buf, size_t len) {
  if (len == 0 || static_cast<uint16_t>(reg) + len - 1U > 0xFFu) {
    return Status::Error(Err::INVALID_PARAM, "Register block out of range");
  }
  for (size_t i = 0; i < len; ++i) {
    if (!isRawWritableAddress(static_cast<uint8_t>(reg + i))) {
      return Status::Error(Err::INVALID_PARAM, "Register block is not publicly writable");
    }
  }
  return writeRegs(reg, buf, len);
}

// ===== Static Utility Functions =====

bool RV3032::isValidDateTime(const DateTime& time) {
  if (time.year < 2000 || time.year > 2099) {
    return false;
  }
  if (time.month < 1 || time.month > 12) {
    return false;
  }
  if (time.day < 1 || time.day > daysInMonth(time.year, time.month)) {
    return false;
  }
  if (time.hour > 23 || time.minute > 59 || time.second > 59) {
    return false;
  }
  if (time.weekday > 6) {
    return false;
  }
  return true;
}

uint8_t RV3032::computeWeekday(uint16_t year, uint8_t month, uint8_t day) {
  uint32_t days = dateToDays(year, month, day);
  return static_cast<uint8_t>((days + 4) % 7);
}

bool RV3032::parseBuildTime(DateTime& out) {
  const char* dateStr = __DATE__;
  const char* timeStr = __TIME__;
  if (!dateStr || !timeStr) {
    return false;
  }

  char monthStr[4] = {0};
  int day = 0;
  int year = 0;
  if (sscanf(dateStr, "%3s %d %d", monthStr, &day, &year) != 3) {
    return false;
  }

  int hour = 0;
  int minute = 0;
  int second = 0;
  if (sscanf(timeStr, "%d:%d:%d", &hour, &minute, &second) != 3) {
    return false;
  }

  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char* pos = strstr(months, monthStr);
  if (!pos) {
    return false;
  }

  uint8_t month = static_cast<uint8_t>((pos - months) / 3 + 1);

  out.year = static_cast<uint16_t>(year);
  out.month = month;
  out.day = static_cast<uint8_t>(day);
  out.hour = static_cast<uint8_t>(hour);
  out.minute = static_cast<uint8_t>(minute);
  out.second = static_cast<uint8_t>(second);
  out.weekday = computeWeekday(out.year, out.month, out.day);

  return isValidDateTime(out);
}

// ===== Private Helper Functions =====

Status RV3032::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
  if (!_initialized && !_beginInProgress) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!buf || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C read parameters");
  }
  // Limit reads to avoid oversized transfers.
  if (len > 255) {
    return Status::Error(Err::INVALID_PARAM, "I2C read length too large");
  }
  if (!isKnownRegisterBlock(reg, len)) {
    return Status::Error(Err::INVALID_PARAM, "Register block out of range");
  }

  uint8_t tx = reg;
  // Use tracked wrapper - health is updated automatically
  return _i2cWriteReadTracked(&tx, 1, buf, len);
}

Status RV3032::writeRegs(uint8_t reg, const uint8_t* buf, size_t len) {
  if (!_initialized && !_beginInProgress) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (!buf || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C write parameters");
  }

  // Max write buffer: 1 byte for register address + 15 bytes data = 16 bytes total.
  // This covers all RV3032 multi-byte writes (time registers = 7 bytes, etc.)
  static constexpr size_t kMaxWriteLen = 16;
  if (len > (kMaxWriteLen - 1)) {  // len is data only, we add 1 for register address
    return Status::Error(Err::INVALID_PARAM, "I2C write length exceeds 15 bytes");
  }
  if (!isKnownRegisterBlock(reg, len)) {
    return Status::Error(Err::INVALID_PARAM, "Register block out of range");
  }

  uint8_t tx[kMaxWriteLen] = {0};
  tx[0] = reg;
  std::memcpy(&tx[1], buf, len);
  // Use tracked wrapper - health is updated automatically
  return _i2cWriteTracked(tx, len + 1);
}

Status RV3032::ensureRead(uint8_t reg, uint8_t* data, size_t len,
                          uint32_t operationStart,
                          uint32_t phaseDeadlineMs,
                          bool requireCleanupReserve,
                          bool* callbackReturnedLate) {
  const uint32_t callbackStart = _nowMs();
  const uint32_t elapsed = callbackStart - operationStart;
  if (elapsed >= PRIMARY_CELL_OPERATION_TIMEOUT_MS || elapsed >= phaseDeadlineMs) {
    return Status::Error(Err::TIMEOUT, "Primary ensure read deadline expired");
  }
  uint32_t timeout = _config.i2cTimeoutMs;
  if (timeout > PRIMARY_CELL_TRANSFER_TIMEOUT_MS) timeout = PRIMARY_CELL_TRANSFER_TIMEOUT_MS;
  const uint32_t overallRemaining = PRIMARY_CELL_OPERATION_TIMEOUT_MS - elapsed;
  const uint32_t phaseRemaining = phaseDeadlineMs - elapsed;
  if (requireCleanupReserve) {
    if (overallRemaining <= PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS) {
      return Status::Error(Err::TIMEOUT,
                           "Primary ensure cleanup reserve reached");
    }
    const uint32_t normalRemaining =
        overallRemaining - PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS;
    if (timeout > normalRemaining) timeout = normalRemaining;
  }
  if (timeout > overallRemaining) timeout = overallRemaining;
  if (timeout > phaseRemaining) timeout = phaseRemaining;
  if (timeout == 0) {
    return Status::Error(Err::TIMEOUT, "Primary ensure read has no callback budget");
  }
  const Status result =
      _i2cWriteReadTrackedTimeout(&reg, 1, data, len, timeout);
  const uint32_t after = _nowMs();
  const uint32_t afterElapsed = after - operationStart;
  const bool returnedLate = after - callbackStart > timeout;
  const bool deadlineExpired =
      afterElapsed >= PRIMARY_CELL_OPERATION_TIMEOUT_MS ||
      afterElapsed >= phaseDeadlineMs ||
      (requireCleanupReserve &&
       PRIMARY_CELL_OPERATION_TIMEOUT_MS - afterElapsed <
           PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS);
  if (returnedLate && callbackReturnedLate != nullptr) {
    *callbackReturnedLate = true;
  }
  if (returnedLate || deadlineExpired) {
    return Status::Error(Err::TIMEOUT,
                         returnedLate
                             ? "Primary ensure read callback returned late"
                             : "Primary ensure read callback crossed deadline");
  }
  return result;
}

Status RV3032::ensureWrite(uint8_t reg, const uint8_t* data, size_t len,
                           uint32_t operationStart,
                           uint32_t phaseDeadlineMs,
                           bool requireCleanupReserve,
                           bool* callbackAttempted,
                           bool* callbackReturnedLate) {
  if (callbackAttempted != nullptr) *callbackAttempted = false;
  if (data == nullptr || len == 0 || len > 15) {
    return Status::Error(Err::INVALID_PARAM, "Invalid primary ensure write");
  }
  const uint32_t callbackStart = _nowMs();
  const uint32_t elapsed = callbackStart - operationStart;
  if (elapsed >= PRIMARY_CELL_OPERATION_TIMEOUT_MS || elapsed >= phaseDeadlineMs) {
    return Status::Error(Err::TIMEOUT, "Primary ensure write deadline expired");
  }
  uint32_t timeout = _config.i2cTimeoutMs;
  if (timeout > PRIMARY_CELL_TRANSFER_TIMEOUT_MS) timeout = PRIMARY_CELL_TRANSFER_TIMEOUT_MS;
  const uint32_t overallRemaining = PRIMARY_CELL_OPERATION_TIMEOUT_MS - elapsed;
  const uint32_t phaseRemaining = phaseDeadlineMs - elapsed;
  if (requireCleanupReserve) {
    if (overallRemaining <= PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS) {
      return Status::Error(Err::TIMEOUT,
                           "Primary ensure cleanup reserve reached");
    }
    const uint32_t normalRemaining =
        overallRemaining - PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS;
    if (timeout > normalRemaining) timeout = normalRemaining;
  }
  if (timeout > overallRemaining) timeout = overallRemaining;
  if (timeout > phaseRemaining) timeout = phaseRemaining;
  if (timeout == 0) {
    return Status::Error(Err::TIMEOUT, "Primary ensure write has no callback budget");
  }
  uint8_t payload[16] = {};
  payload[0] = reg;
  std::memcpy(&payload[1], data, len);
  if (callbackAttempted != nullptr) *callbackAttempted = true;
  const Status result = _i2cWriteTrackedTimeout(payload, len + 1, timeout);
  const uint32_t after = _nowMs();
  const uint32_t afterElapsed = after - operationStart;
  const bool returnedLate = after - callbackStart > timeout;
  const bool deadlineExpired =
      afterElapsed >= PRIMARY_CELL_OPERATION_TIMEOUT_MS ||
      afterElapsed >= phaseDeadlineMs ||
      (requireCleanupReserve &&
       PRIMARY_CELL_OPERATION_TIMEOUT_MS - afterElapsed <
           PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS);
  if (returnedLate && callbackReturnedLate != nullptr) {
    *callbackReturnedLate = true;
  }
  if (returnedLate || deadlineExpired) {
    return Status::Error(Err::TIMEOUT,
                         returnedLate
                             ? "Primary ensure write callback returned late"
                             : "Primary ensure write callback crossed deadline");
  }
  return result;
}

Status RV3032::ensureWait(uint32_t delayMs, uint32_t operationStart) {
  const uint32_t before = _nowMs();
  const uint32_t elapsed = before - operationStart;
  if (delayMs == 0 || elapsed >= PRIMARY_CELL_OPERATION_TIMEOUT_MS) {
    return Status::Error(Err::TIMEOUT, "Primary ensure wait exceeds deadline");
  }
  const uint32_t remaining = PRIMARY_CELL_OPERATION_TIMEOUT_MS - elapsed;
  const uint32_t requested = delayMs < remaining ? delayMs : remaining;
  _config.waitMs(requested, _config.timeUser);
  const uint32_t after = _nowMs();
  if (after - before < requested || requested < delayMs ||
      after - operationStart >= PRIMARY_CELL_OPERATION_TIMEOUT_MS) {
    return Status::Error(Err::TIMEOUT, "Primary ensure wait returned early or late");
  }
  return Status::Ok();
}

Status RV3032::ensureWaitReady(uint32_t operationStart,
                               uint32_t phaseTimeoutMs,
                               uint16_t checkCap,
                               uint8_t& tempLsb,
                               bool requireCleanupReserve,
                               bool* callbackReturnedLate) {
  const uint32_t phaseStart = _nowMs();
  const uint32_t phaseStartElapsed = phaseStart - operationStart;
  uint32_t phaseDeadlineElapsed = phaseStartElapsed + phaseTimeoutMs;
  if (phaseDeadlineElapsed > PRIMARY_CELL_OPERATION_TIMEOUT_MS) {
    phaseDeadlineElapsed = PRIMARY_CELL_OPERATION_TIMEOUT_MS;
  }
  for (uint16_t check = 0; check < checkCap; ++check) {
    if (_nowMs() - phaseStart >= phaseTimeoutMs) {
      return Status::Error(Err::TIMEOUT, "EEPROM ready phase expired");
    }
    Status st = ensureRead(cmd::REG_TEMP_LSB, &tempLsb, 1, operationStart,
                           phaseDeadlineElapsed, requireCleanupReserve,
                           callbackReturnedLate);
    if (!st.ok()) return st;
    if ((tempLsb & cmd::EEPROM_BUSY_MASK) == 0) return Status::Ok();
    if (check + 1U >= checkCap) {
      return Status::Error(Err::TIMEOUT, "EEPROM ready check cap reached");
    }
    if (_nowMs() - phaseStart + EEPROM_BUSY_POLL_INTERVAL_MS >
        phaseTimeoutMs) {
      return Status::Error(Err::TIMEOUT, "EEPROM ready phase expired");
    }
    st = ensureWait(EEPROM_BUSY_POLL_INTERVAL_MS, operationStart);
    if (!st.ok()) return st;
  }
  return Status::Error(Err::TIMEOUT, "EEPROM ready check cap reached");
}

Status RV3032::readPersistentC0ForEnsure(
    uint32_t operationStart, uint8_t& value,
    PrimaryCellConfigurationReport& report,
    bool& callbackReturnedLate) {
  const uint8_t address = cmd::REG_ACTIVE_PMU;
  Status st = ensureWrite(cmd::REG_EE_ADDRESS, &address, 1, operationStart,
                          PRIMARY_CELL_OPERATION_TIMEOUT_MS, true, nullptr,
                          &callbackReturnedLate);
  uint8_t check = 0;
  if (st.ok()) {
    st = ensureRead(cmd::REG_EE_ADDRESS, &check, 1, operationStart,
                    PRIMARY_CELL_OPERATION_TIMEOUT_MS, true,
                    &callbackReturnedLate);
    if (st.ok() && check != address) {
      st = Status::Error(Err::EEPROM_VERIFY_FAILED,
                         "Persistent C0 address verification failed");
    }
  }
  const uint8_t sentinel = cmd::PERSISTENT_READ_SENTINEL;
  if (st.ok()) {
    st = ensureWrite(cmd::REG_EE_DATA, &sentinel, 1, operationStart,
                     PRIMARY_CELL_OPERATION_TIMEOUT_MS, true, nullptr,
                     &callbackReturnedLate);
  }
  if (st.ok()) {
    st = ensureRead(cmd::REG_EE_DATA, &check, 1, operationStart,
                    PRIMARY_CELL_OPERATION_TIMEOUT_MS, true,
                    &callbackReturnedLate);
    if (st.ok() && check != sentinel) {
      st = Status::Error(Err::EEPROM_VERIFY_FAILED,
                         "Persistent C0 sentinel verification failed");
    }
  }
  uint8_t temp = 0;
  if (st.ok()) {
    st = ensureRead(cmd::REG_TEMP_LSB, &temp, 1, operationStart,
                    PRIMARY_CELL_OPERATION_TIMEOUT_MS, true,
                    &callbackReturnedLate);
    if (st.ok() && (temp & cmd::EEPROM_BUSY_MASK) != 0) {
      st = Status::Error(Err::BUSY, "EEPROM busy before read-one");
    }
  }
  Status commandStatus = Status::Ok();
  uint32_t commandStart = 0;
  bool commandAttempted = false;
  bool commandReturnedLate = false;
  if (st.ok()) {
    const uint8_t command = cmd::EEPROM_CMD_READ_ONE;
    commandStart = _nowMs();
    commandStatus = ensureWrite(cmd::REG_EE_COMMAND, &command, 1,
                                operationStart,
                                PRIMARY_CELL_OPERATION_TIMEOUT_MS, true,
                                &commandAttempted, &commandReturnedLate);
    callbackReturnedLate = callbackReturnedLate || commandReturnedLate;
    if (!commandAttempted || commandReturnedLate) {
      st = commandStatus;
    } else {
      st = ensureWait(EEPROM_READ_SETTLE_MS, operationStart);
    }
  }
  if (st.ok()) {
    const uint32_t commandElapsed = _nowMs() - commandStart;
    st = commandElapsed >= EEPROM_READ_TIMEOUT_MS
        ? Status::Error(Err::TIMEOUT,
                        "Primary read-one completion phase expired")
        : ensureWaitReady(operationStart,
                          EEPROM_READ_TIMEOUT_MS - commandElapsed,
                          EEPROM_READ_CHECK_CAP, temp, true,
                          &callbackReturnedLate);
  }
  if (st.ok()) {
    st = ensureRead(cmd::REG_EE_DATA, &value, 1, operationStart,
                    PRIMARY_CELL_OPERATION_TIMEOUT_MS, true,
                    &callbackReturnedLate);
    if (st.ok() && ((value & 0x80u) != 0 || value == sentinel)) {
      st = Status::Error(Err::EEPROM_VERIFY_FAILED,
                         "Persistent C0 sentinel was not replaced");
    }
  }
  if (!st.ok()) return !commandStatus.ok() ? commandStatus : st;
  // A read-one callback error is reconciled only by complete sentinel proof.
  (void)report;
  return Status::Ok();
}

Status RV3032::cleanupPrimaryCellEnsure(
    uint32_t operationStart, uint8_t control1Before, bool control1Valid,
    bool control1WriteAttempted, bool accessStateVerified,
    uint8_t activeBefore, bool activeValid, uint8_t safeActive, bool safeValid,
    bool safeActiveWriteAmbiguous, bool persistenceTrusted, uint8_t target,
    PrimaryCellConfigurationReport& report,
    bool& callbackReturnedLate) {
  if (!accessStateVerified) {
    if (!control1Valid) {
      return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                           "Cleanup has no verified access-state basis");
    }
    if (!control1WriteAttempted) {
      report.control1After = static_cast<uint8_t>(
          control1Before & cmd::CONTROL1_IMPLEMENTED_MASK);
      report.cleanupVerified = true;
      return Status::Ok();
    }
    const uint8_t originalControl1 = static_cast<uint8_t>(
        control1Before & cmd::CONTROL1_IMPLEMENTED_MASK);
    Status restore = ensureWrite(cmd::REG_CONTROL1, &originalControl1, 1,
                                 operationStart,
                                 PRIMARY_CELL_OPERATION_TIMEOUT_MS, false,
                                 nullptr, &callbackReturnedLate);
    if (!restore.ok()) {
      return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                           "Access-state Control 1 restore failed",
                           restore.detail);
    }
    uint8_t controlCheck = 0;
    restore = ensureRead(cmd::REG_CONTROL1, &controlCheck, 1, operationStart,
                         PRIMARY_CELL_OPERATION_TIMEOUT_MS, false,
                         &callbackReturnedLate);
    if (!restore.ok() ||
        (controlCheck & cmd::CONTROL1_IMPLEMENTED_MASK) != originalControl1) {
      return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                           "Access-state Control 1 proof failed",
                           restore.detail);
    }
    report.control1After = controlCheck;
    report.cleanupVerified = true;
    return Status::Ok();
  }
  uint8_t temp = 0;
  Status st = ensureWaitReady(operationStart, EEPROM_CLEANUP_READY_TIMEOUT_MS,
                              EEPROM_CLEANUP_CHECK_CAP, temp, false,
                              &callbackReturnedLate);
  if (!st.ok()) {
    report.autoRefreshHeldDisabledForSafety =
        !persistenceTrusted && accessStateVerified && safeValid;
    report.cleanupStatus = Status::Error(Err::EEPROM_CLEANUP_FAILED,
                                         "Cleanup could not prove EEPROM idle",
                                         st.detail);
    return report.cleanupStatus;
  }

  uint8_t activeTarget = 0;
  bool activeTargetVerified = false;
  if (persistenceTrusted) {
    activeTarget = target;
  } else if (safeValid) {
    activeTarget = safeActive;
    activeTargetVerified = true;
    report.activeAfter = safeActive;
  } else if (safeActiveWriteAmbiguous) {
    uint8_t observedActive = 0;
    st = ensureRead(cmd::REG_ACTIVE_PMU, &observedActive, 1, operationStart,
                    PRIMARY_CELL_OPERATION_TIMEOUT_MS, false,
                    &callbackReturnedLate);
    if (!st.ok()) {
      return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                           "Cleanup could not classify ambiguous PMU write",
                           st.detail);
    }
    activeTarget = static_cast<uint8_t>(
        observedActive & cmd::PMU_PRIMARY_PRESERVE_MASK);
    activeTargetVerified =
        (observedActive & cmd::PMU_IMPLEMENTED_MASK) == activeTarget;
    if (activeTargetVerified) report.activeAfter = observedActive;
  } else if (activeValid) {
    activeTarget = static_cast<uint8_t>(
        activeBefore & cmd::PMU_PRIMARY_PRESERVE_MASK);
  } else {
    st = ensureRead(cmd::REG_ACTIVE_PMU, &activeTarget, 1, operationStart,
                    PRIMARY_CELL_OPERATION_TIMEOUT_MS, false,
                    &callbackReturnedLate);
    if (!st.ok()) {
      return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                           "Cleanup has no valid PMU value", st.detail);
    }
    activeTarget = static_cast<uint8_t>(
        activeTarget & cmd::PMU_PRIMARY_PRESERVE_MASK);
  }

  uint32_t activeVerifiedAt = 0;
  uint8_t activeCheck = 0;
  if (!activeTargetVerified) {
    st = ensureWrite(cmd::REG_ACTIVE_PMU, &activeTarget, 1, operationStart,
                     PRIMARY_CELL_OPERATION_TIMEOUT_MS, false, nullptr,
                     &callbackReturnedLate);
    if (!st.ok()) {
      return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                           "Cleanup PMU write failed", st.detail);
    }
    st = ensureRead(cmd::REG_ACTIVE_PMU, &activeCheck, 1, operationStart,
                    PRIMARY_CELL_OPERATION_TIMEOUT_MS, false,
                    &callbackReturnedLate);
    if (!st.ok() ||
        (activeCheck & cmd::PMU_IMPLEMENTED_MASK) != activeTarget) {
      return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                           "Cleanup PMU verification failed", st.detail);
    }
    report.activeAfter = activeCheck;
  } else {
    activeCheck = report.activeAfter;
  }
  if (persistenceTrusted) activeVerifiedAt = _nowMs();

  if (!control1Valid) {
    return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                         "Cleanup has no valid Control 1 value");
  }
  uint8_t controlTarget = static_cast<uint8_t>(
      control1Before & cmd::CONTROL1_IMPLEMENTED_MASK);
  if (persistenceTrusted) {
    controlTarget = static_cast<uint8_t>(controlTarget &
                                         ~cmd::CONTROL1_EERD_MASK);
  } else {
    controlTarget = static_cast<uint8_t>(controlTarget |
                                         cmd::CONTROL1_EERD_MASK);
  }
  st = ensureWrite(cmd::REG_CONTROL1, &controlTarget, 1, operationStart,
                   PRIMARY_CELL_OPERATION_TIMEOUT_MS, false, nullptr,
                   &callbackReturnedLate);
  if (!st.ok()) {
    return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                         "Cleanup Control 1 write failed", st.detail);
  }
  uint8_t controlCheck = 0;
  st = ensureRead(cmd::REG_CONTROL1, &controlCheck, 1, operationStart,
                  PRIMARY_CELL_OPERATION_TIMEOUT_MS, false,
                  &callbackReturnedLate);
  if (!st.ok() ||
      (controlCheck & cmd::CONTROL1_IMPLEMENTED_MASK) != controlTarget) {
    return Status::Error(Err::EEPROM_CLEANUP_FAILED,
                         "Cleanup Control 1 verification failed", st.detail);
  }
  report.control1After = controlCheck;
  report.autoRefreshHeldDisabledForSafety =
      !persistenceTrusted &&
      (controlCheck & cmd::CONTROL1_EERD_MASK) != 0;
  if (!persistenceTrusted) {
    report.cleanupVerified = true;
    return Status::Ok();
  }

  const uint32_t settledFor = _nowMs() - activeVerifiedAt;
  if (settledFor < EEPROM_WRITE_SETTLE_MS) {
    st = ensureWait(EEPROM_WRITE_SETTLE_MS - settledFor, operationStart);
    if (!st.ok()) {
      report.failureStage = PrimaryCellFailureStage::SETTLE;
      return st.code == Err::TIMEOUT
          ? st
          : Status::Error(Err::EEPROM_CLEANUP_FAILED,
                          "Primary level-switch settle failed", st.detail);
    }
  }
  report.cleanupVerified = true;
  return Status::Ok();
}

Status RV3032::processPersistentJob(uint32_t nowMs, bool& callbackUsed) {
  callbackUsed = false;
  const Status inProgress = Status::Error(Err::IN_PROGRESS, "Persistent job in progress");
  auto currentAfterCallback = [&]() -> uint32_t {
    if (_config.nowMs != nullptr) {
      const uint32_t observedNowMs = _nowMs();
      if (static_cast<int32_t>(observedNowMs - nowMs) > 0) {
        return observedNowMs;
      }
      return nowMs;
    }
    // With no observable clock, charge the callback's full configured bound
    // before starting a vendor-mandated post-command wait.
    return nowMs + _config.i2cTimeoutMs;
  };
  auto shouldRestoreSelectedActive = [&]() -> bool {
    return _job.activeKind == JobKind::None &&
        _job.persistentSafeC0Verified &&
        _job.persistentAddress > cmd::REG_ACTIVE_PMU &&
        _job.persistentAddress <= cmd::REG_ACTIVE_TREFERENCE1;
  };
  auto beginActiveRestore = [&]() {
    _job.persistentState = shouldRestoreSelectedActive()
        ? EepromState::RestoreSelectedActive
        : EepromState::RestoreActive;
  };
  auto rememberFailure = [&](const Status& st) {
    if (_job.completedStatus.ok()) {
      _job.completedStatus = st;
    }
    _job.persistentReadyChecks = 0;
    _job.persistentNotBeforeMs = 0;
    _job.persistentPhaseDeadlineMs =
        nowMs + EEPROM_CLEANUP_READY_TIMEOUT_MS;
    _job.persistentState = _job.persistentControl1Valid
        ? EepromState::CleanupWaitReady
        : (_job.passwordAuthorizationMayBeActive
               ? EepromState::CleanupLockPassword
               : EepromState::RestoreActive);
  };
  auto finishCleanupFailure = [&](const Status& st) -> Status {
    if (_job.persistentCleanupStatus.ok()) {
      _job.persistentCleanupStatus = st;
    }
    if (_job.activeKind == JobKind::PasswordProtection &&
        _job.passwordAuthorizationMayBeActive) {
      _job.persistentState = EepromState::CleanupLockPassword;
      return inProgress;
    }
    return _job.persistentCleanupStatus;
  };
  auto currentAddress = [&]() -> uint8_t {
    return _job.persistentUseAddressList
        ? _job.persistentAddresses[_job.persistentIndex]
        : static_cast<uint8_t>(_job.persistentAddress +
                               _job.persistentIndex);
  };
  auto nextByteOrCleanup = [&]() {
    ++_job.persistentIndex;
    _job.persistentWriteAttempted = false;
    if (_job.activeKind == JobKind::PasswordProtection &&
        _job.persistentIndex == 1) {
      // CA is persisted as disabled first.  Disable the active protection
      // mirror before changing any persistent or active password byte.
      _job.persistentState = EepromState::ApplyPassword;
    } else if (_job.persistentIndex >= _job.persistentLength) {
      if (_job.activeKind == JobKind::PasswordProtection) {
        _job.persistentState = _job.passwordEnable
            ? EepromState::ApplyPasswordBytes
            : EepromState::RestoreActive;
      } else {
        beginActiveRestore();
      }
    } else {
      _job.persistentState = EepromState::WriteAddr;
    }
  };

  const bool cleanupState =
      _job.persistentState == EepromState::RestoreSelectedActive ||
      _job.persistentState == EepromState::VerifySelectedActive ||
      _job.persistentState == EepromState::RestoreActive ||
      _job.persistentState == EepromState::CleanupWaitReady ||
      _job.persistentState == EepromState::VerifyActive ||
      _job.persistentState == EepromState::RestoreControl1 ||
      _job.persistentState == EepromState::VerifyControl1 ||
      _job.persistentState == EepromState::Settle ||
      _job.persistentState == EepromState::FinalizePasswordEnable ||
      _job.persistentState == EepromState::CleanupLockPassword;
  if (!cleanupState && !_job.persistentWriteAttempted &&
      _job.mutationCutoffActive &&
      hasDeadlinePassed(nowMs, _job.mutationCutoffMs)) {
    const Status cutoff = Status::Error(
        Err::TIMEOUT, "Persistent operation cleanup reserve reached");
    if (_job.persistentCleanupRequired ||
        _job.passwordAuthorizationMayBeActive) {
      rememberFailure(cutoff);
      return inProgress;
    }
    // No device state has changed, so there is nothing to clean up.  In
    // particular, do not start password authentication or invent C0/C1
    // restore writes after the reserved cleanup interval begins.
    return cutoff;
  }

  switch (_job.persistentState) {
    case EepromState::WritePassword: {
      _job.passwordAuthorizationMayBeActive = true;
      callbackUsed = true;
      Status st = writeRegs(cmd::REG_PASSWORD0,
                            _job.currentCredential.bytes,
                            sizeof(_job.currentCredential.bytes));
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      _job.persistentState = EepromState::ReadControl1;
      return inProgress;
    }
    case EepromState::ReadControl1: {
      callbackUsed = true;
      Status st = readRegs(cmd::REG_CONTROL1, &_job.persistentControl1, 1);
      if (!st.ok()) {
        if (_job.persistentCleanupRequired ||
            _job.passwordAuthorizationMayBeActive) {
          rememberFailure(st);
          return inProgress;
        }
        return st;
      }
      _job.persistentControl1Valid = true;
      _job.persistentState = EepromState::EnableEerd;
      return inProgress;
    }
    case EepromState::EnableEerd: {
      const uint8_t desired = static_cast<uint8_t>(
          (_job.persistentControl1 & cmd::CONTROL1_IMPLEMENTED_MASK) |
          cmd::CONTROL1_EERD_MASK);
      if ((_job.persistentControl1 & cmd::CONTROL1_IMPLEMENTED_MASK) == desired) {
        _job.persistentState = EepromState::VerifyEerd;
        return inProgress;
      }
      const uint8_t payload[2] = {cmd::REG_CONTROL1, desired};
      _job.persistentCleanupRequired = true;
      callbackUsed = true;
      Status st = _i2cWriteTracked(payload, sizeof(payload));
      if (!st.ok()) {
        // A failed mutating callback may still have reached the device.  Once
        // Control 1 is known, every such ambiguity must take the bounded
        // restore-and-verify path instead of returning with EERD possibly set.
        rememberFailure(st);
        return inProgress;
      }
      _job.persistentState = EepromState::VerifyEerd;
      return inProgress;
    }
    case EepromState::VerifyEerd: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_CONTROL1, &value, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      const uint8_t desired = static_cast<uint8_t>(
          (_job.persistentControl1 & cmd::CONTROL1_IMPLEMENTED_MASK) |
          cmd::CONTROL1_EERD_MASK);
      if ((value & cmd::CONTROL1_IMPLEMENTED_MASK) != desired) {
        rememberFailure(Status::Error(Err::EEPROM_VERIFY_FAILED,
                                      "EERD enable verification failed"));
        return inProgress;
      }
      _job.persistentState = EepromState::WaitReady;
      _job.persistentReadyChecks = 0;
      _job.persistentPhaseDeadlineMs = nowMs + EEPROM_READY_TIMEOUT_MS;
      return inProgress;
    }
    case EepromState::WaitReady: {
      if (_job.persistentReadyChecks != 0 &&
          !hasDeadlinePassed(nowMs, _job.persistentNotBeforeMs)) {
        return inProgress;
      }
      if (hasDeadlinePassed(nowMs, _job.persistentPhaseDeadlineMs)) {
        rememberFailure(Status::Error(Err::TIMEOUT,
                                      "EEPROM ready phase deadline reached"));
        return inProgress;
      }
      uint8_t temp = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_TEMP_LSB, &temp, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      if ((temp & cmd::EEPROM_BUSY_MASK) != 0) {
        if (++_job.persistentReadyChecks >= EEPROM_READY_CHECK_CAP) {
          rememberFailure(Status::Error(Err::TIMEOUT,
                                        "EEPROM ready check cap reached"));
          return inProgress;
        }
        _job.persistentNotBeforeMs = nowMs + EEPROM_BUSY_POLL_INTERVAL_MS;
        return inProgress;
      }
      _job.persistentState = EepromState::ReadActiveC0;
      return inProgress;
    }
    case EepromState::ReadActiveC0: {
      callbackUsed = true;
      Status st = readRegs(cmd::REG_ACTIVE_PMU, &_job.persistentActiveC0, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      _job.persistentActiveC0Valid = true;
      _job.persistentSafeC0 = static_cast<uint8_t>(
          _job.persistentActiveC0 & cmd::PMU_PRIMARY_PRESERVE_MASK);
      _job.persistentState = EepromState::WriteSafeC0;
      return inProgress;
    }
    case EepromState::WriteSafeC0:
      if ((_job.persistentActiveC0 & cmd::PMU_IMPLEMENTED_MASK) ==
          _job.persistentSafeC0) {
        _job.persistentState = EepromState::VerifySafeC0;
        return inProgress;
      }
      _job.persistentCleanupRequired = true;
      callbackUsed = true;
      {
        Status st = writeRegs(cmd::REG_ACTIVE_PMU, &_job.persistentSafeC0, 1);
        if (!st.ok()) {
          rememberFailure(st);
          return inProgress;
        }
      }
      _job.persistentState = EepromState::VerifySafeC0;
      return inProgress;
    case EepromState::VerifySafeC0: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_ACTIVE_PMU, &value, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      if ((value & cmd::PMU_IMPLEMENTED_MASK) != _job.persistentSafeC0) {
        rememberFailure(Status::Error(Err::EEPROM_VERIFY_FAILED,
                                      "Safe PMU verification failed"));
        return inProgress;
      }
      _job.persistentSafeC0Verified = true;
      _job.persistentState = EepromState::WriteAddr;
      return inProgress;
    }
    case EepromState::WriteAddr: {
      const uint8_t address = currentAddress();
      callbackUsed = true;
      Status st = writeRegs(cmd::REG_EE_ADDRESS, &address, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      _job.persistentState = EepromState::VerifyAddr;
      return inProgress;
    }
    case EepromState::VerifyAddr: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_EE_ADDRESS, &value, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      if (value != currentAddress()) {
        rememberFailure(Status::Error(Err::EEPROM_VERIFY_FAILED,
                                      "EEPROM address staging mismatch"));
        return inProgress;
      }
      _job.persistentState = EepromState::WriteSentinel1;
      return inProgress;
    }
    case EepromState::WriteSentinel1:
      _job.persistentSentinel = cmd::PERSISTENT_READ_SENTINEL;
      callbackUsed = true;
      {
        Status st = writeRegs(cmd::REG_EE_DATA, &_job.persistentSentinel, 1);
        if (!st.ok()) {
          rememberFailure(st);
          return inProgress;
        }
      }
      _job.persistentState = EepromState::VerifySentinel1;
      return inProgress;
    case EepromState::VerifySentinel1: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_EE_DATA, &value, 1);
      if (!st.ok() || value != _job.persistentSentinel) {
        rememberFailure(st.ok() ? Status::Error(Err::EEPROM_VERIFY_FAILED,
                                                 "EEPROM sentinel staging mismatch") : st);
        return inProgress;
      }
      _job.persistentState = EepromState::PreReadBusy1;
      return inProgress;
    }
    case EepromState::PreReadBusy1:
    case EepromState::PreReadBusy2: {
      uint8_t temp = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_TEMP_LSB, &temp, 1);
      if (!st.ok() || (temp & cmd::EEPROM_BUSY_MASK) != 0) {
        rememberFailure(st.ok() ? Status::Error(Err::BUSY,
                                                 "EEPROM busy before read-one") : st);
        return inProgress;
      }
      _job.persistentState = (_job.persistentState == EepromState::PreReadBusy1)
          ? EepromState::ReadCmd1 : EepromState::ReadCmd2;
      return inProgress;
    }
    case EepromState::ReadCmd1:
    case EepromState::ReadCmd2: {
      const bool second = _job.persistentState == EepromState::ReadCmd2;
      const uint8_t command = cmd::EEPROM_CMD_READ_ONE;
      _job.persistentCleanupRequired = true;
      callbackUsed = true;
      Status st = writeRegs(cmd::REG_EE_COMMAND, &command, 1);
      (void)st;  // A command error is reconciled by direct persistent proof.
      const uint32_t commandCompletedMs = currentAfterCallback();
      _job.persistentNotBeforeMs =
          commandCompletedMs + EEPROM_READ_SETTLE_MS;
      _job.persistentReadyChecks = 0;
      _job.persistentPhaseDeadlineMs =
          commandCompletedMs + EEPROM_READ_TIMEOUT_MS;
      _job.persistentState = second ? EepromState::WaitRead2
                                    : EepromState::WaitRead1;
      return inProgress;
    }
    case EepromState::WaitRead1:
    case EepromState::WaitRead2:
      if (!hasDeadlinePassed(nowMs, _job.persistentNotBeforeMs)) {
        return inProgress;
      }
      _job.persistentState = (_job.persistentState == EepromState::WaitRead1)
          ? EepromState::PollRead1 : EepromState::PollRead2;
      return inProgress;
    case EepromState::PollRead1:
    case EepromState::PollRead2: {
      const bool second = _job.persistentState == EepromState::PollRead2;
      if (hasDeadlinePassed(nowMs, _job.persistentPhaseDeadlineMs)) {
        rememberFailure(Status::Error(Err::TIMEOUT,
                                      "EEPROM read-one phase deadline reached"));
        return inProgress;
      }
      uint8_t temp = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_TEMP_LSB, &temp, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      if ((temp & cmd::EEPROM_BUSY_MASK) != 0) {
        if (++_job.persistentReadyChecks >= EEPROM_READ_CHECK_CAP) {
          rememberFailure(Status::Error(Err::TIMEOUT,
                                        "EEPROM read-one check cap reached"));
          return inProgress;
        }
        _job.persistentNotBeforeMs = nowMs + EEPROM_BUSY_POLL_INTERVAL_MS;
        _job.persistentState = second ? EepromState::WaitRead2
                                      : EepromState::WaitRead1;
        return inProgress;
      }
      _job.persistentState = second ? EepromState::ReadData2
                                    : EepromState::ReadData1;
      return inProgress;
    }
    case EepromState::ReadData1: {
      callbackUsed = true;
      Status st = readRegs(cmd::REG_EE_DATA, &_job.persistentFirstRead, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      _job.persistentSentinel = static_cast<uint8_t>(
          _job.persistentFirstRead ^ 0xFFu);
      _job.persistentState = EepromState::WriteSentinel2;
      return inProgress;
    }
    case EepromState::WriteSentinel2:
      callbackUsed = true;
      {
        Status st = writeRegs(cmd::REG_EE_DATA, &_job.persistentSentinel, 1);
        if (!st.ok()) {
          rememberFailure(st);
          return inProgress;
        }
      }
      _job.persistentState = EepromState::VerifySentinel2;
      return inProgress;
    case EepromState::VerifySentinel2: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_EE_DATA, &value, 1);
      if (!st.ok() || value != _job.persistentSentinel) {
        rememberFailure(st.ok() ? Status::Error(Err::EEPROM_VERIFY_FAILED,
                                                 "Second sentinel staging mismatch") : st);
        return inProgress;
      }
      _job.persistentState = EepromState::PreReadBusy2;
      return inProgress;
    }
    case EepromState::ReadData2: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_EE_DATA, &value, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      if (value != _job.persistentFirstRead || value == _job.persistentSentinel) {
        rememberFailure(Status::Error(Err::EEPROM_VERIFY_FAILED,
                                      "Persistent two-read proof failed"));
        return inProgress;
      }
      if (!_job.persistentWriteMode) {
        _job.persistentRead.data[_job.persistentIndex] = value;
        ++_job.persistentRead.length;
        nextByteOrCleanup();
        return inProgress;
      }
      const uint8_t desired = _job.userRamBuf[_job.persistentIndex];
      if (_job.persistentWriteAttempted) {
        if (value != desired) {
          rememberFailure(Status::Error(Err::EEPROM_VERIFY_FAILED,
                                        "User EEPROM durable readback mismatch"));
          return inProgress;
        }
        if (!_job.completedStatus.ok()) {
          beginActiveRestore();
          return inProgress;
        }
        ++_job.userEepromWrite.completedBytes;
        ++_job.userEepromWrite.durablyVerifiedBytes;
        nextByteOrCleanup();
        return inProgress;
      }
      if (value == desired) {
        ++_job.userEepromWrite.completedBytes;
        ++_job.userEepromWrite.durablyVerifiedBytes;
        nextByteOrCleanup();
        return inProgress;
      }
      _job.persistentDesired = desired;
      _job.persistentState = EepromState::ClearEef;
      return inProgress;
    }
    case EepromState::ClearEef: {
      const uint8_t value = cmd::EEPROM_CLEAR_EEF_VALUE;
      callbackUsed = true;
      Status st = writeRegs(cmd::REG_TEMP_LSB, &value, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      _job.persistentState = EepromState::VerifyEef;
      return inProgress;
    }
    case EepromState::VerifyEef: {
      uint8_t temp = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_TEMP_LSB, &temp, 1);
      if (!st.ok() || (temp & (cmd::EEPROM_BUSY_MASK | cmd::EEPROM_EEF_MASK)) != 0) {
        rememberFailure(st.ok() ? Status::Error(Err::EEPROM_WRITE_FAILED,
                                                 "EEPROM not ready for write-one") : st);
        return inProgress;
      }
      _job.persistentState = EepromState::WriteData;
      return inProgress;
    }
    case EepromState::WriteData:
      callbackUsed = true;
      {
        Status st = writeRegs(cmd::REG_EE_DATA, &_job.persistentDesired, 1);
        if (!st.ok()) {
          rememberFailure(st);
          return inProgress;
        }
      }
      _job.persistentState = EepromState::VerifyData;
      return inProgress;
    case EepromState::VerifyData: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_EE_DATA, &value, 1);
      if (!st.ok() || value != _job.persistentDesired) {
        rememberFailure(st.ok() ? Status::Error(Err::EEPROM_VERIFY_FAILED,
                                                 "EEPROM data staging mismatch") : st);
        return inProgress;
      }
      _job.persistentState = EepromState::WaitReadyPreCmd;
      return inProgress;
    }
    case EepromState::WaitReadyPreCmd: {
      uint8_t temp = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_TEMP_LSB, &temp, 1);
      if (!st.ok() || (temp & cmd::EEPROM_BUSY_MASK) != 0) {
        rememberFailure(st.ok() ? Status::Error(Err::BUSY,
                                                 "EEPROM busy before write-one") : st);
        return inProgress;
      }
      _job.persistentState = EepromState::WriteCmd;
      return inProgress;
    }
    case EepromState::WriteCmd: {
      const uint8_t command = cmd::EEPROM_CMD_WRITE_ONE;
      _job.persistentWriteAttempted = true;
      _job.persistentCleanupRequired = true;
      callbackUsed = true;
      Status st = writeRegs(cmd::REG_EE_COMMAND, &command, 1);
      (void)st;  // Never resent; later READ_ONE proof determines durability.
      const uint32_t commandCompletedMs = currentAfterCallback();
      _job.persistentNotBeforeMs =
          commandCompletedMs + EEPROM_WRITE_SETTLE_MS;
      _job.persistentReadyChecks = 0;
      _job.persistentPhaseDeadlineMs =
          _job.persistentNotBeforeMs + _config.eepromTimeoutMs;
      _job.persistentState = EepromState::WaitWriteSettle;
      return inProgress;
    }
    case EepromState::WaitWriteSettle:
      if (!hasDeadlinePassed(nowMs, _job.persistentNotBeforeMs)) {
        return inProgress;
      }
      _job.persistentState = EepromState::WaitReadyPostCmd;
      return inProgress;
    case EepromState::WaitReadyPostCmd: {
      if (hasDeadlinePassed(nowMs, _job.persistentPhaseDeadlineMs)) {
        rememberFailure(Status::Error(Err::TIMEOUT,
                                      "EEPROM write-one phase deadline reached"));
        return inProgress;
      }
      uint8_t temp = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_TEMP_LSB, &temp, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      if ((temp & cmd::EEPROM_BUSY_MASK) != 0) {
        if (++_job.persistentReadyChecks >= EEPROM_WRITE_CHECK_CAP) {
          rememberFailure(Status::Error(Err::TIMEOUT,
                                        "EEPROM write-one check cap reached"));
          return inProgress;
        }
        _job.persistentNotBeforeMs = nowMs + EEPROM_BUSY_POLL_INTERVAL_MS;
        _job.persistentState = EepromState::WaitWriteSettle;
        return inProgress;
      }
      if ((temp & cmd::EEPROM_EEF_MASK) != 0) {
        if (_job.completedStatus.ok()) {
          _job.completedStatus = Status::Error(
              Err::EEPROM_WRITE_FAILED,
              "EEPROM write-one reported EEF");
        }
        // EEF is failure evidence, not content evidence. Continue through the
        // adaptive direct READ_ONE proof so a may-have-committed command is
        // reconciled without ever being resent.
        _job.persistentState = EepromState::WriteAddr;
        return inProgress;
      }
      _job.persistentState = EepromState::WriteAddr;
      return inProgress;
    }
    case EepromState::ApplyPassword: {
      _job.persistentCleanupRequired = true;
      callbackUsed = true;
      const uint8_t disabled = 0;
      Status st = writeRegs(cmd::REG_EEPROM_PW_ENABLE, &disabled, 1);
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      _job.persistentState = _job.persistentIndex < _job.persistentLength
          ? EepromState::WriteAddr
          : (_job.passwordEnable ? EepromState::ApplyPasswordBytes
                                 : EepromState::RestoreActive);
      return inProgress;
    }
    case EepromState::ApplyPasswordBytes: {
      callbackUsed = true;
      Status st = writeRegs(cmd::REG_EEPROM_PASSWORD0,
                            _job.newCredential.bytes,
                            sizeof(_job.newCredential.bytes));
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      _job.persistentState = EepromState::ApplyPasswordCredential;
      return inProgress;
    }
    case EepromState::ApplyPasswordCredential: {
      callbackUsed = true;
      Status st = writeRegs(cmd::REG_PASSWORD0,
                            _job.newCredential.bytes,
                            sizeof(_job.newCredential.bytes));
      if (!st.ok()) {
        rememberFailure(st);
        return inProgress;
      }
      _job.passwordAuthorizationMayBeActive = true;
      _job.persistentState = EepromState::RestoreActive;
      return inProgress;
    }
    case EepromState::FinalizePasswordEnable: {
      const uint8_t enabled = 0xFF;
      callbackUsed = true;
      Status st = writeRegs(cmd::REG_EEPROM_PW_ENABLE, &enabled, 1);
      if (!st.ok() && _job.completedStatus.ok()) {
        _job.completedStatus = st;
      }
      _job.persistentState = EepromState::CleanupLockPassword;
      return inProgress;
    }
    case EepromState::CleanupLockPassword: {
      uint8_t invalidCredential[4] = {};
      for (size_t i = 0; i < sizeof(invalidCredential); ++i) {
        invalidCredential[i] = static_cast<uint8_t>(
            _job.newCredential.bytes[i] ^ 0xFFu);
        if (invalidCredential[i] == _job.currentCredential.bytes[i]) {
          invalidCredential[i] ^= 0x01u;
        }
      }
      callbackUsed = true;
      Status st = writeRegs(cmd::REG_PASSWORD0, invalidCredential,
                            sizeof(invalidCredential));
      std::memset(invalidCredential, 0, sizeof(invalidCredential));
      if (!st.ok()) {
        _job.persistentCleanupStatus = Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "Password authorization lock failed", st.detail);
      } else {
        _job.passwordAuthorizationMayBeActive = false;
        _job.persistentCleanupRequired = false;
      }
      if (_job.completedStatus.ok() &&
          _job.persistentCleanupStatus.ok()) {
        _passwordStatus.evidence = _job.passwordEnable
            ? PasswordProtectionEvidence::VERIFIED_ENABLED
            : PasswordProtectionEvidence::VERIFIED_DISABLED;
        _passwordStatus.credentialAccepted = false;
        _passwordStatus.persistentVerified = true;
        _passwordStatus.cleanupVerified = true;
      } else {
        _passwordStatus = PasswordProtectionStatus{};
      }
      return _job.persistentCleanupStatus.ok()
          ? _job.completedStatus
          : _job.persistentCleanupStatus;
    }
    case EepromState::CleanupWaitReady: {
      if (_job.persistentReadyChecks != 0 &&
          !hasDeadlinePassed(nowMs, _job.persistentNotBeforeMs)) {
        return inProgress;
      }
      if (hasDeadlinePassed(nowMs, _job.persistentPhaseDeadlineMs) ||
          _job.persistentReadyChecks >= EEPROM_CLEANUP_CHECK_CAP) {
        return finishCleanupFailure(Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "EEPROM busy during cleanup"));
      }
      uint8_t temp = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_TEMP_LSB, &temp, 1);
      if (!st.ok()) {
        return finishCleanupFailure(Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "EEPROM cleanup readiness read failed", st.detail));
      }
      if ((temp & cmd::EEPROM_BUSY_MASK) != 0) {
        ++_job.persistentReadyChecks;
        _job.persistentNotBeforeMs =
            nowMs + EEPROM_BUSY_POLL_INTERVAL_MS;
        return inProgress;
      }
      beginActiveRestore();
      return inProgress;
    }
    case EepromState::RestoreSelectedActive: {
      const uint8_t desired = _job.userRamBuf[0];
      callbackUsed = true;
      Status st = writeRegs(_job.persistentAddress, &desired, 1);
      if (!st.ok() && _job.persistentCleanupStatus.ok()) {
        _job.persistentCleanupStatus = Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "Selected active mirror restore failed", st.detail);
      }
      _job.persistentState = EepromState::VerifySelectedActive;
      return inProgress;
    }
    case EepromState::VerifySelectedActive: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(_job.persistentAddress, &value, 1);
      if ((!st.ok() || value != _job.userRamBuf[0]) &&
          _job.persistentCleanupStatus.ok()) {
        _job.persistentCleanupStatus = Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "Selected active mirror cleanup verification failed",
            st.detail);
      }
      _job.persistentState = EepromState::RestoreActive;
      return inProgress;
    }
    case EepromState::RestoreActive:
      if (!_job.persistentActiveC0Valid) {
        _job.persistentState = EepromState::RestoreControl1;
        return inProgress;
      }
      callbackUsed = true;
      {
        const bool restoreQueuedC0 =
            _job.activeKind == JobKind::None &&
            _job.persistentSafeC0Verified &&
            _job.persistentAddress == cmd::REG_ACTIVE_PMU;
        const uint8_t value = static_cast<uint8_t>(
            (restoreQueuedC0 ? _job.userRamBuf[0]
                             : _job.persistentActiveC0) &
            cmd::PMU_IMPLEMENTED_MASK);
        Status st = writeRegs(cmd::REG_ACTIVE_PMU, &value, 1);
        if (!st.ok() && _job.persistentCleanupStatus.ok()) {
          _job.persistentCleanupStatus = Status::Error(
              Err::EEPROM_CLEANUP_FAILED, "Active PMU restore failed",
              st.detail);
        }
      }
      _job.persistentState = EepromState::VerifyActive;
      return inProgress;
    case EepromState::VerifyActive: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_ACTIVE_PMU, &value, 1);
      const bool restoreQueuedC0 =
          _job.activeKind == JobKind::None &&
          _job.persistentSafeC0Verified &&
          _job.persistentAddress == cmd::REG_ACTIVE_PMU;
      const uint8_t expected = static_cast<uint8_t>(
          (restoreQueuedC0 ? _job.userRamBuf[0]
                           : _job.persistentActiveC0) &
          cmd::PMU_IMPLEMENTED_MASK);
      if ((!st.ok() || (value & cmd::PMU_IMPLEMENTED_MASK) != expected) &&
          _job.persistentCleanupStatus.ok()) {
        _job.persistentCleanupStatus = Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "Active PMU cleanup verification failed", st.detail);
      }
      _job.persistentState = EepromState::RestoreControl1;
      return inProgress;
    }
    case EepromState::RestoreControl1:
      if (!_job.persistentControl1Valid) {
        return finishCleanupFailure(Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "Control 1 cleanup evidence unavailable"));
      }
      callbackUsed = true;
      {
        const uint8_t value = static_cast<uint8_t>(
            _job.persistentControl1 & cmd::CONTROL1_IMPLEMENTED_MASK);
        Status st = writeRegs(cmd::REG_CONTROL1, &value, 1);
        if (!st.ok() && _job.persistentCleanupStatus.ok()) {
          _job.persistentCleanupStatus = Status::Error(
              Err::EEPROM_CLEANUP_FAILED, "Control 1 restore failed",
              st.detail);
        }
      }
      _job.persistentState = EepromState::VerifyControl1;
      return inProgress;
    case EepromState::VerifyControl1: {
      uint8_t value = 0;
      callbackUsed = true;
      Status st = readRegs(cmd::REG_CONTROL1, &value, 1);
      const uint8_t expected = static_cast<uint8_t>(
          _job.persistentControl1 & cmd::CONTROL1_IMPLEMENTED_MASK);
      if ((!st.ok() || (value & cmd::CONTROL1_IMPLEMENTED_MASK) != expected) &&
          _job.persistentCleanupStatus.ok()) {
        _job.persistentCleanupStatus = Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "Control 1 cleanup verification failed", st.detail);
      }
      _job.persistentNotBeforeMs =
          currentAfterCallback() + EEPROM_WRITE_SETTLE_MS;
      _job.persistentState = EepromState::Settle;
      return inProgress;
    }
    case EepromState::Settle:
      if (!hasDeadlinePassed(nowMs, _job.persistentNotBeforeMs)) {
        return inProgress;
      }
      if (_job.activeKind == JobKind::PasswordProtection) {
        _job.persistentState =
            _job.completedStatus.ok() && _job.passwordEnable
                ? EepromState::FinalizePasswordEnable
                : EepromState::CleanupLockPassword;
        return inProgress;
      }
      if (_job.activeKind == JobKind::PersistentRead) {
        _job.persistentRead.persistentVerified =
            _job.persistentIndex >= _job.persistentLength;
        _job.persistentRead.cleanupVerified =
            _job.persistentCleanupStatus.ok();
      } else {
        _job.userEepromWrite.cleanupVerified =
            _job.persistentCleanupStatus.ok();
      }
      _job.persistentCleanupRequired = false;
      return _job.persistentCleanupStatus.ok()
          ? _job.completedStatus
          : _job.persistentCleanupStatus;
    case EepromState::Idle:
    default:
      return Status::Error(Err::EEPROM_VERIFY_FAILED,
                           "Invalid persistent job state");
  }
}

Status RV3032::processEeprom(uint32_t now_ms, uint8_t maxInstructions, uint8_t& instructionsUsed) {
  instructionsUsed = 0;
  if (!_config.enableEepromWrites) {
    _eeprom.state = EepromState::Idle;
    _eeprom.queueHead = 0;
    _eeprom.queueTail = 0;
    _eeprom.queueCount = 0;
    return _eepromLastStatus;
  }

  if (maxInstructions == 0) {
    return isEepromBusy() ? Status::Error(Err::IN_PROGRESS, "EEPROM update in progress")
                          : _eepromLastStatus;
  }

  while (instructionsUsed < maxInstructions) {
    uint32_t currentNowMs = now_ms;
    if (_config.nowMs != nullptr) {
      const uint32_t observedNowMs = _nowMs();
      if (static_cast<int32_t>(observedNowMs - currentNowMs) > 0) {
        currentNowMs = observedNowMs;
      }
    } else if (instructionsUsed != 0) {
      // A multi-instruction call must not reuse its entry timestamp after a
      // callback. Conservatively charge the configured callback bound when no
      // live clock hook is available.
      currentNowMs += static_cast<uint32_t>(instructionsUsed) *
                      _config.i2cTimeoutMs;
    }
    if (_eeprom.state == EepromState::Idle) {
      uint8_t nextReg = 0;
      uint8_t nextValue = 0;
      if (!eepromQueuePop(nextReg, nextValue)) {
        return _eepromLastStatus;
      }
      (void)startEepromUpdate(nextReg, nextValue);
    }

    if (_job.state == JobState::Idle) {
      _job = JobOp{};
      _job.state = JobState::Persistent;
      _job.activeKind = JobKind::None; // queue owns the shared engine
      _job.persistentState = EepromState::ReadControl1;
      _job.persistentAddress = _eeprom.reg;
      _job.persistentLength = 1;
      _job.persistentWriteMode = true;
      _job.userRamBuf[0] = _eeprom.value;
      _job.deadlineMs = currentNowMs + 4000;
      _job.mutationCutoffMs = currentNowMs + 3700;
      _job.deadlineActive = true;
      _job.mutationCutoffActive = true;
    }

    if (_job.deadlineActive &&
        hasDeadlinePassed(currentNowMs, _job.deadlineMs)) {
      Status terminal = Status::Error(Err::TIMEOUT,
                                      "EEPROM queue deadline expired");
      if (_job.persistentCleanupRequired ||
          _job.passwordAuthorizationMayBeActive) {
        terminal = Status::Error(
            Err::EEPROM_CLEANUP_FAILED,
            "EEPROM queue deadline expired before cleanup proof");
      }
      if (_eepromLastStatus.ok()) {
        _eepromLastStatus = terminal;
      }
      if (_eepromWriteFailures < UINT32_MAX) ++_eepromWriteFailures;
      // No callback is permitted at or after the whole-item deadline.  If the
      // caller starved the reserved cleanup interval, the device access state
      // is unverified; never continue into another admitted queue item.
      _eeprom = EepromOp{};
      _job = JobOp{};
      return terminal;
    }

    bool callbackUsed = false;
    Status st = processPersistentJob(currentNowMs, callbackUsed);
    if (callbackUsed) {
      ++instructionsUsed;
    }
    if (st.inProgress()) {
      if (!callbackUsed) return st;
      continue;
    }

    if (st.ok()) {
      if (_eepromWriteCount < UINT32_MAX) ++_eepromWriteCount;
    } else {
      if (_eepromLastStatus.ok()) {
        _eepromLastStatus = st;
      }
      if (_eepromWriteFailures < UINT32_MAX) ++_eepromWriteFailures;
    }
    if (st.code == Err::EEPROM_CLEANUP_FAILED) {
      // Cleanup failure means C0/C1 (and possibly password authorization) is
      // not proven.  Cancel later queue entries and return the observable
      // terminal error instead of issuing more device commands.
      _eeprom = EepromOp{};
      _job = JobOp{};
      return st;
    }
    _eeprom = [&]() {
      EepromOp next = _eeprom;
      next.state = EepromState::Idle;
      return next;
    }();
    _job = JobOp{};
    if (!st.ok()) {
      // Preserve ordinary remaining items, but expose this exact failure at
      // the item boundary instead of letting a later success hide it.
      return st;
    }
    if (instructionsUsed >= maxInstructions) break;
  }

  return isEepromBusy() ? Status::Error(Err::IN_PROGRESS, "EEPROM update in progress")
                        : _eepromLastStatus;
}

Status RV3032::startEepromUpdate(uint8_t reg, uint8_t value) {
  _eeprom.reg = reg;
  _eeprom.value = value;
  _eeprom.state = EepromState::ReadControl1;
  return Status::Error(Err::IN_PROGRESS, "EEPROM update in progress");
}

bool RV3032::eepromQueueContains(uint8_t reg, uint8_t value) const {
  // Coalesce only against the newest pending value for this address. An older
  // matching entry followed by a different value must not discard a request
  // that changes the final desired state back to the older value.
  for (uint8_t i = _eeprom.queueCount; i > 0; --i) {
    const uint8_t index = static_cast<uint8_t>(
        (_eeprom.queueTail + i - 1U) % kEepromQueueSize);
    if (_eeprom.queue[index].reg == reg) {
      return _eeprom.queue[index].value == value;
    }
  }
  if (_eeprom.state != EepromState::Idle && _eeprom.reg == reg) {
    return _eeprom.value == value;
  }
  return false;
}

bool RV3032::eepromQueuePush(uint8_t reg, uint8_t value) {
  if (eepromQueueContains(reg, value)) {
    return true;
  }
  if (_eeprom.queueCount >= kEepromQueueSize) {
    return false;  // Queue full
  }
  if (_eeprom.state == EepromState::Idle && _eeprom.queueCount == 0) {
    _eepromLastStatus = Status::Ok();
  }
  
  _eeprom.queue[_eeprom.queueHead].reg = reg;
  _eeprom.queue[_eeprom.queueHead].value = value;
  _eeprom.queueHead = (_eeprom.queueHead + 1) % kEepromQueueSize;
  _eeprom.queueCount++;
  return true;
}

bool RV3032::eepromQueuePop(uint8_t& reg, uint8_t& value) {
  if (_eeprom.queueCount == 0) {
    return false;  // Queue empty
  }
  
  reg = _eeprom.queue[_eeprom.queueTail].reg;
  value = _eeprom.queue[_eeprom.queueTail].value;
  _eeprom.queueTail = (_eeprom.queueTail + 1) % kEepromQueueSize;
  _eeprom.queueCount--;
  return true;
}

Status RV3032::readEepromFlags(bool& busy, bool& failed) {
  // EEPROM busy and error flags are in Temperature LSBs register (0x0E).
  uint8_t tempLsb = 0;
  Status st = readRegister(cmd::REG_TEMP_LSB, tempLsb);
  if (!st.ok()) {
    return st;
  }
  busy = ((tempLsb & (1u << cmd::EEPROM_BUSY_BIT)) != 0);
  failed = ((tempLsb & (1u << cmd::EEPROM_ERROR_BIT)) != 0);
  return Status::Ok();
}

// ===== Conversion Helper Functions =====

bool RV3032::decodeCalendar(const uint8_t* data, DateTime& out,
                            bool requireWeekdayMatch) {
  if (data == nullptr || (data[0] & 0x80u) != 0 ||
      (data[1] & 0x80u) != 0 || (data[2] & 0xC0u) != 0 ||
      (data[3] & 0xF8u) != 0 || (data[4] & 0xC0u) != 0 ||
      (data[5] & 0xE0u) != 0) {
    return false;
  }
  for (uint8_t i = 0; i < 7; ++i) {
    if (!isValidBcd(data[i])) {
      return false;
    }
  }
  DateTime decoded{};
  decoded.second = bcdToBin(data[0]);
  decoded.minute = bcdToBin(data[1]);
  decoded.hour = bcdToBin(data[2]);
  decoded.weekday = bcdToBin(data[3]);
  decoded.day = bcdToBin(data[4]);
  decoded.month = bcdToBin(data[5]);
  decoded.year = static_cast<uint16_t>(2000 + bcdToBin(data[6]));
  if (!isValidDateTime(decoded) || decoded.weekday > 6) {
    return false;
  }
  if (requireWeekdayMatch && decoded.weekday !=
      computeWeekday(decoded.year, decoded.month, decoded.day)) {
    return false;
  }
  out = decoded;
  return true;
}

bool RV3032::acceptedVerifiedTime(const DateTime& requested,
                                  const DateTime& observed) {
  uint32_t requestedUnix = 0;
  uint32_t observedUnix = 0;
  if (!dateTimeToUnix(requested, requestedUnix) ||
      !dateTimeToUnix(observed, observedUnix)) {
    return false;
  }
  if (observedUnix == requestedUnix) {
    return true;
  }
  if (requested.year == 2099 && requested.month == 12 && requested.day == 31 &&
      requested.hour == 23 && requested.minute == 59 && requested.second == 59) {
    return false;
  }
  return observedUnix == requestedUnix + 1U;
}

bool RV3032::isValidBcd(uint8_t v) {
  uint8_t low = static_cast<uint8_t>(v & 0x0F);
  uint8_t high = static_cast<uint8_t>((v >> 4) & 0x0F);
  return (low <= 9) && (high <= 9);
}

uint8_t RV3032::bcdToBin(uint8_t v) {
  return static_cast<uint8_t>((v >> 4) * 10 + (v & 0x0F));
}

uint8_t RV3032::binToBcd(uint8_t v) {
  // Precondition: v must be <= 99. All call sites validate before calling.
  // Return 0x99 (max valid BCD) as a safe, detectable sentinel if violated.
  if (v > 99) { return 0x99; }
  return static_cast<uint8_t>(((v / 10) << 4) | (v % 10));
}

bool RV3032::isLeapYear(uint16_t year) {
  return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

uint8_t RV3032::daysInMonth(uint16_t year, uint8_t month) {
  static constexpr uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year)) {
    return 29;
  }
  if (month == 0 || month > 12) {
    return 0;
  }
  return kDays[month - 1];
}

uint32_t RV3032::dateToDays(uint16_t year, uint8_t month, uint8_t day) {
  uint32_t days = 0;

  // Count days from 1970 to current year
  for (uint16_t y = 1970; y < year; ++y) {
    days += isLeapYear(y) ? 366 : 365;
  }

  // Days before current month
  static constexpr uint16_t kDaysBeforeMonth[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
  };
  if (month >= 1 && month <= 12) {
    days += kDaysBeforeMonth[month - 1];
  }

  // Add leap day if after February in leap year
  if (month > 2 && isLeapYear(year)) {
    days += 1;
  }

  // Add day of month
  if (day > 0) {
    days += static_cast<uint32_t>(day - 1);
  }

  return days;
}

bool RV3032::unixToDate(uint32_t ts, DateTime& out) {
  // Reject timestamps before 2000-01-01 (RTC valid range is 2000-2099)
  if (ts < kEpoch2000) {
    return false;
  }

  uint32_t days = ts / 86400UL;
  uint32_t rem = ts % 86400UL;

  // Find year
  uint16_t year = 1970;
  for (; year <= 2099; ++year) {
    uint16_t daysInYear = isLeapYear(year) ? 366 : 365;
    if (days < daysInYear) {
      break;
    }
    days -= daysInYear;
  }
  if (year > 2099 || year < 2000) {
    return false;  // Out of RTC range
  }

  // Find month
  uint8_t month = 1;
  for (; month <= 12; ++month) {
    uint8_t dim = daysInMonth(year, month);
    if (days < dim) {
      break;
    }
    days -= dim;
  }
  if (month > 12) {
    return false;
  }

  out.year = year;
  out.month = month;
  out.day = static_cast<uint8_t>(days + 1);
  out.hour = static_cast<uint8_t>(rem / 3600UL);
  rem %= 3600UL;
  out.minute = static_cast<uint8_t>(rem / 60UL);
  out.second = static_cast<uint8_t>(rem % 60UL);
  out.weekday = computeWeekday(out.year, out.month, out.day);

  return true;
}

Status RV3032::readValidity(ValidityFlags& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  uint8_t values[2] = {};
  Status st = readRegs(cmd::REG_STATUS, values, sizeof(values));
  if (!st.ok()) {
    return st;
  }
  const uint8_t status = values[0];
  const uint8_t tempLsb = values[1];

  out.voltageLow = (status & (1u << cmd::STATUS_VLF_BIT)) != 0;
  out.powerOnReset = (status & (1u << cmd::STATUS_PORF_BIT)) != 0;
  out.backupSwitched = (tempLsb & (1u << cmd::TEMP_BSF_BIT)) != 0;
  out.timeInvalid = out.powerOnReset || out.voltageLow;

  return Status::Ok();
}

Status RV3032::clearPowerOnResetFlag() {
  return clearStatus(static_cast<uint8_t>(1u << cmd::STATUS_PORF_BIT));
}

Status RV3032::clearVoltageLowFlag() {
  return clearStatus(static_cast<uint8_t>(1u << cmd::STATUS_VLF_BIT));
}

Status RV3032::clearBackupSwitchFlag() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }

  const uint8_t bsfMask = static_cast<uint8_t>(1u << cmd::TEMP_BSF_BIT);
  const uint8_t otherFlagMask = static_cast<uint8_t>(
      (1u << cmd::EEPROM_ERROR_BIT) |
      (1u << cmd::EEPROM_BUSY_BIT) |
      (1u << cmd::TEMP_CLKF_BIT));
  if (!workIdle()) {
    return Status::Error(Err::BUSY, "Driver work already in progress");
  }
  _job = JobOp{};
  _job.activeKind = JobKind::RegisterUpdate;
  _job.registerUpdateReg = cmd::REG_TEMP_LSB;
  _job.registerUpdateImplementedMask = static_cast<uint8_t>(
      cmd::EEPROM_EEF_MASK | bsfMask);
  _job.registerUpdateClearMask = bsfMask;
  _job.registerUpdateRequiredMask = bsfMask;
  _job.registerUpdateForbiddenMask = otherFlagMask;
  _job.registerUpdateGuardedClear = true;
  _job.registerUpdateGuardFailureIsBusy = true;
  _job.lastStatus = Status::Error(Err::IN_PROGRESS,
                                  "Backup-switch flag clear in progress");
  _job.state = JobState::RegisterUpdateRead;
  return _job.lastStatus;
}

// ===== User RAM Operations =====

Status RV3032::readUserRam(uint8_t offset, uint8_t* buf, size_t len) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (buf == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid user RAM read buffer");
  }
  if (offset >= kUserRamSize || len > static_cast<size_t>(kUserRamSize - offset)) {
    return Status::Error(Err::INVALID_PARAM, "User RAM read out of range");
  }

  return readRegs(static_cast<uint8_t>(cmd::REG_USER_RAM_START + offset), buf, len);
}

Status RV3032::writeUserRam(uint8_t offset, const uint8_t* buf, size_t len) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Call begin() first");
  }
  if (buf == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid user RAM write buffer");
  }
  if (offset >= kUserRamSize || len > static_cast<size_t>(kUserRamSize - offset)) {
    return Status::Error(Err::INVALID_PARAM, "User RAM write out of range");
  }

  if (len <= 15) {
    return writeRegs(static_cast<uint8_t>(cmd::REG_USER_RAM_START + offset),
                     buf, len);
  }
  return startWriteUserRamJob(offset, buf, len);
}

// ===== Public Static Conversion Functions =====

uint8_t RV3032::bcdToBinary(uint8_t bcd) {
  return bcdToBin(bcd);
}

uint8_t RV3032::binaryToBcd(uint8_t bin) {
  return binToBcd(bin);
}

bool RV3032::unixToDateTime(uint32_t ts, DateTime& out) {
  return unixToDate(ts, out);
}

bool RV3032::dateTimeToUnix(const DateTime& time, uint32_t& out) {
  if (!isValidDateTime(time)) {
    return false;
  }
  uint32_t days = dateToDays(time.year, time.month, time.day);
  out = days * 86400UL
      + static_cast<uint32_t>(time.hour) * 3600UL
      + static_cast<uint32_t>(time.minute) * 60UL
      + static_cast<uint32_t>(time.second);
  return true;
}

}  // namespace RV3032
