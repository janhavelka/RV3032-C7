#include <Arduino.h>
#include <Wire.h>

#include <cmath>
#include <cstring>

#include "RV3032/CommandTable.h"
#include "RV3032/RV3032.h"
#include "examples/common/I2cTransport.h"

namespace {

constexpr uint8_t CONFIG_BYTES = 6U;
constexpr uint8_t RECORD_OFFSET = 8U;
constexpr uint8_t RECORD_BYTES = 8U;
constexpr uint8_t PHASE_NONE = 0x00U;
constexpr uint8_t PHASE_STAGING = 0xA0U;
constexpr uint8_t PHASE_ALT_WAIT = 0xA1U;
constexpr uint8_t PHASE_RESTORE_WAIT = 0xA2U;
constexpr uint8_t PHASE_COMPLETE = 0xA3U;
constexpr uint8_t PHASE_ABORTED = 0xAFU;
constexpr float OFFSET_STEP_PPM = 0.238418579F;

struct TransportOwner {
  TwoWire* wire = nullptr;
  uint32_t reads = 0;
  uint32_t writes = 0;
  uint8_t writeOneCommands = 0;
};

struct PhaseRecord {
  uint8_t original[CONFIG_BYTES] = {};
  uint8_t phase = PHASE_NONE;
  uint8_t writeOneCommands = 0;
};

struct Settings {
  RV3032::ClkoutConfig clkout{};
  int8_t offsetSteps = 0;
  int16_t temperatureReference = 0;
};

RV3032::RV3032 gRtc;
TransportOwner gOwner{&Wire};
PhaseRecord gRecord{};
char gLine[16] = {};
uint8_t gLineLength = 0;

bool before(uint32_t deadlineMs) {
  return static_cast<int32_t>(deadlineMs - millis()) > 0;
}

RV3032::Status ownerWrite(uint8_t address, const uint8_t* data, size_t length,
                          uint32_t timeoutMs, void* user) {
  auto* owner = static_cast<TransportOwner*>(user);
  ++owner->writes;
  if (data != nullptr && length >= 2U &&
      data[0] == RV3032::cmd::REG_EE_COMMAND &&
      data[1] == RV3032::cmd::EEPROM_CMD_WRITE_ONE) {
    ++owner->writeOneCommands;
  }
  return transport::wireWrite(address, data, length, timeoutMs, owner->wire);
}

RV3032::Status ownerWriteRead(uint8_t address, const uint8_t* tx,
                              size_t txLength, uint8_t* rx, size_t rxLength,
                              uint32_t timeoutMs, void* user) {
  auto* owner = static_cast<TransportOwner*>(user);
  ++owner->reads;
  return transport::wireWriteRead(address, tx, txLength, rx, rxLength,
                                  timeoutMs, owner->wire);
}

uint32_t nowMs(void*) { return millis(); }

void waitMs(uint32_t durationMs, void*) { delay(durationMs + 1U); }

RV3032::Status completeJob(RV3032::Status status,
                           uint32_t timeoutMs = 6000U) {
  if (!status.inProgress()) {
    return status;
  }
  const uint32_t deadlineMs = millis() + timeoutMs;
  while (status.inProgress() && before(deadlineMs)) {
    uint8_t used = 0;
    status = gRtc.pollJob(millis(), 1U, used);
    if (status.inProgress()) {
      delay(1U);
    }
  }
  if (status.inProgress()) {
    uint8_t used = 0;
    status = gRtc.pollJob(deadlineMs, 1U, used);
  }
  return status;
}

RV3032::Status drainEeprom(uint32_t timeoutMs = 12000U) {
  RV3032::Status status = gRtc.getEepromStatus();
  const uint32_t deadlineMs = millis() + timeoutMs;
  while ((status.inProgress() || gRtc.isEepromBusy()) && before(deadlineMs)) {
    uint8_t used = 0;
    status = gRtc.pollEeprom(millis(), 1U, used);
    if (status.inProgress() || gRtc.isEepromBusy()) {
      delay(1U);
    }
  }
  if (status.inProgress() || gRtc.isEepromBusy()) {
    uint8_t used = 0;
    status = gRtc.pollEeprom(deadlineMs, 1U, used);
  }
  return status;
}

void printStatus(const char* operation, const RV3032::Status& status) {
  Serial.printf("[FAIL] %s code=%u detail=%ld msg=%s\n", operation,
                static_cast<unsigned>(status.code),
                static_cast<long>(status.detail),
                status.msg == nullptr ? "" : status.msg);
}

void printBytes(const char* label, const uint8_t* values) {
  Serial.printf("%s", label);
  for (uint8_t i = 0; i < CONFIG_BYTES; ++i) {
    Serial.printf(" %02X", values[i]);
  }
  Serial.println();
}

RV3032::Status readPersistentConfiguration(uint8_t* out) {
  for (uint8_t i = 0; i < CONFIG_BYTES; ++i) {
    RV3032::Status status = gRtc.startReadConfigurationEepromJob(
        static_cast<RV3032::ConfigurationEepromRegister>(0xC0U + i),
        millis(), 3000U);
    status = completeJob(status, 4000U);
    if (!status.ok()) {
      return status;
    }
    RV3032::PersistentReadResult result{};
    status = gRtc.getPersistentReadJobResult(result);
    if (!status.ok()) {
      return status;
    }
    if (!result.persistentVerified || !result.cleanupVerified ||
        result.length != 1U) {
      return RV3032::Status::Error(RV3032::Err::INCOHERENT_DATA,
                                   "Persistent configuration proof incomplete");
    }
    out[i] = result.data[0];
  }
  return RV3032::Status::Ok();
}

RV3032::Status readActiveConfiguration(uint8_t* out) {
  for (uint8_t i = 0; i < CONFIG_BYTES; ++i) {
    const RV3032::Status status = gRtc.readRegister(
        static_cast<uint8_t>(RV3032::cmd::REG_ACTIVE_PMU + i), out[i]);
    if (!status.ok()) {
      return status;
    }
  }
  return RV3032::Status::Ok();
}

RV3032::Status readRecord(PhaseRecord& record) {
  uint8_t bytes[RECORD_BYTES] = {};
  RV3032::Status status = completeJob(
      gRtc.readUserRam(RECORD_OFFSET, bytes, sizeof(bytes)), 2000U);
  if (!status.ok()) {
    return status;
  }
  memcpy(record.original, bytes, CONFIG_BYTES);
  record.phase = bytes[6];
  record.writeOneCommands = bytes[7];
  return RV3032::Status::Ok();
}

RV3032::Status writeRecord(const uint8_t* original, uint8_t phase,
                           uint8_t writeOneCommands) {
  uint8_t bytes[RECORD_BYTES] = {};
  memcpy(bytes, original, CONFIG_BYTES);
  bytes[6] = phase;
  bytes[7] = writeOneCommands;
  RV3032::Status status = completeJob(
      gRtc.writeUserRam(RECORD_OFFSET, bytes, sizeof(bytes)), 2000U);
  if (status.ok()) {
    memcpy(gRecord.original, original, CONFIG_BYTES);
    gRecord.phase = phase;
    gRecord.writeOneCommands = writeOneCommands;
  }
  return status;
}

bool isKnownPhase(uint8_t phase) {
  return phase == PHASE_STAGING || phase == PHASE_ALT_WAIT ||
         phase == PHASE_RESTORE_WAIT || phase == PHASE_COMPLETE ||
         phase == PHASE_ABORTED;
}

int8_t decodeOffsetSteps(uint8_t value) {
  const uint8_t raw = static_cast<uint8_t>(
      value & RV3032::cmd::OFFSET_VALUE_MASK);
  return (raw & 0x20U) != 0U
             ? static_cast<int8_t>(raw | 0xC0U)
             : static_cast<int8_t>(raw);
}

Settings decodeSettings(const uint8_t* raw) {
  Settings settings{};
  settings.clkout.enabled =
      (raw[0] & RV3032::cmd::PMU_NCLKE_MASK) == 0U;
  settings.clkout.highFrequencyMode =
      (raw[3] & RV3032::cmd::CLKOUT_OS_MASK) != 0U;
  settings.clkout.xtalFrequency = static_cast<RV3032::ClkoutFrequency>(
      (raw[3] & RV3032::cmd::CLKOUT_FREQ_MASK) >>
      RV3032::cmd::CLKOUT_FREQ_SHIFT);
  const uint16_t dividerMinusOne = static_cast<uint16_t>(raw[2]) |
      (static_cast<uint16_t>(raw[3] &
                             RV3032::cmd::CLKOUT_HFD_HIGH_MASK) << 8);
  settings.clkout.highFrequencyDivider =
      static_cast<uint16_t>(dividerMinusOne + 1U);
  settings.offsetSteps = decodeOffsetSteps(raw[1]);
  const uint16_t reference = static_cast<uint16_t>(raw[4]) |
      (static_cast<uint16_t>(raw[5]) << 8);
  settings.temperatureReference = static_cast<int16_t>(reference);
  return settings;
}

Settings makeAlternateSettings(const uint8_t* original) {
  const Settings saved = decodeSettings(original);
  Settings alternate{};
  alternate.clkout.enabled = !saved.clkout.enabled;
  alternate.clkout.highFrequencyMode = false;
  alternate.clkout.xtalFrequency =
      saved.clkout.xtalFrequency == RV3032::ClkoutFrequency::Hz1
          ? RV3032::ClkoutFrequency::Hz64
          : RV3032::ClkoutFrequency::Hz1;
  alternate.clkout.highFrequencyDivider =
      saved.clkout.highFrequencyDivider == 37U ? 53U : 37U;
  alternate.offsetSteps = saved.offsetSteps == 5 ? -5 : 5;
  alternate.temperatureReference =
      saved.temperatureReference == 0x0123 ? static_cast<int16_t>(-0x0124)
                                           : static_cast<int16_t>(0x0123);
  return alternate;
}

void encodeExpected(const uint8_t* original, const Settings& settings,
                    uint8_t* expected) {
  memcpy(expected, original, CONFIG_BYTES);
  expected[0] = static_cast<uint8_t>(
      (original[0] & static_cast<uint8_t>(~RV3032::cmd::PMU_NCLKE_MASK)) |
      (settings.clkout.enabled ? 0U : RV3032::cmd::PMU_NCLKE_MASK));
  expected[1] = static_cast<uint8_t>(
      (original[1] & static_cast<uint8_t>(~RV3032::cmd::OFFSET_VALUE_MASK)) |
      (static_cast<uint8_t>(settings.offsetSteps) &
       RV3032::cmd::OFFSET_VALUE_MASK));
  const uint16_t dividerMinusOne = static_cast<uint16_t>(
      settings.clkout.highFrequencyDivider - 1U);
  expected[2] = static_cast<uint8_t>(dividerMinusOne & 0xFFU);
  expected[3] = static_cast<uint8_t>(
      (settings.clkout.highFrequencyMode ? RV3032::cmd::CLKOUT_OS_MASK : 0U) |
      ((static_cast<uint8_t>(settings.clkout.xtalFrequency)
        << RV3032::cmd::CLKOUT_FREQ_SHIFT) &
       RV3032::cmd::CLKOUT_FREQ_MASK) |
      ((dividerMinusOne >> 8) & RV3032::cmd::CLKOUT_HFD_HIGH_MASK));
  const uint16_t reference =
      static_cast<uint16_t>(settings.temperatureReference);
  expected[4] = static_cast<uint8_t>(reference & 0xFFU);
  expected[5] = static_cast<uint8_t>(reference >> 8);
}

RV3032::Status applySettings(const Settings& settings) {
  bool interruptEnabled = false;
  RV3032::Status status =
      gRtc.getClockInterruptEnabled(interruptEnabled);
  if (!status.ok()) {
    return status;
  }
  bool clockFlag = false;
  status = gRtc.getClockOutputFlag(clockFlag);
  if (!status.ok()) {
    return status;
  }
  if (interruptEnabled || clockFlag) {
    return RV3032::Status::Error(
        RV3032::Err::BUSY,
        "Disable CLKOUT interrupt and clear CLKF before persistence HIL");
  }

  status = completeJob(gRtc.setClkoutConfig(settings.clkout));
  if (!status.ok()) {
    return status;
  }
  status = drainEeprom();
  if (!status.ok()) {
    return status;
  }

  status = completeJob(gRtc.setOffsetPpm(
      static_cast<float>(settings.offsetSteps) * OFFSET_STEP_PPM));
  if (!status.ok()) {
    return status;
  }
  status = drainEeprom();
  if (!status.ok()) {
    return status;
  }

  status = completeJob(
      gRtc.setTemperatureReference(settings.temperatureReference));
  if (!status.ok()) {
    return status;
  }
  return drainEeprom();
}

RV3032::Status verifyExact(const uint8_t* expected) {
  uint8_t persistent[CONFIG_BYTES] = {};
  uint8_t active[CONFIG_BYTES] = {};
  RV3032::Status status = readPersistentConfiguration(persistent);
  if (!status.ok()) {
    return status;
  }
  status = readActiveConfiguration(active);
  if (!status.ok()) {
    return status;
  }
  printBytes("PERSISTENT_C0_C5", persistent);
  printBytes("ACTIVE_C0_C5", active);
  if (memcmp(persistent, expected, CONFIG_BYTES) != 0 ||
      memcmp(active, expected, CONFIG_BYTES) != 0) {
    printBytes("EXPECTED_C0_C5", expected);
    return RV3032::Status::Error(RV3032::Err::EEPROM_VERIFY_FAILED,
                                 "Configuration bytes did not match");
  }
  return RV3032::Status::Ok();
}

RV3032::Status clearBackupFlag() {
  return completeJob(gRtc.clearBackupSwitchFlag(), 2000U);
}

bool readBackupSwitched(bool& switched) {
  RV3032::ValidityFlags validity{};
  const RV3032::Status status = gRtc.readValidity(validity);
  if (!status.ok()) {
    printStatus("read validity", status);
    return false;
  }
  switched = validity.backupSwitched;
  Serial.printf("VALIDITY PORF=%u VLF=%u BSF=%u TIME_VALID=%u\n",
                validity.powerOnReset ? 1U : 0U,
                validity.voltageLow ? 1U : 0U,
                validity.backupSwitched ? 1U : 0U,
                validity.timeInvalid ? 0U : 1U);
  return !validity.powerOnReset && !validity.voltageLow &&
         !validity.timeInvalid;
}

void markAborted(const char* reason) {
  const RV3032::Status marker = writeRecord(
      gRecord.original, PHASE_ABORTED, gOwner.writeOneCommands);
  if (!marker.ok()) {
    printStatus("write aborted marker", marker);
  }
  Serial.printf("PERSIST_HIL_ABORTED reason=%s\n", reason);
}

void runStage() {
  if (isKnownPhase(gRecord.phase)) {
    Serial.printf("[FAIL] stage rejected phase=0x%02X\n> ", gRecord.phase);
    return;
  }

  RV3032::PrimaryCellConfigurationReport primary{};
  RV3032::Status status = gRtc.ensurePrimaryCellConfiguration(primary);
  if (!status.ok() || !primary.persistentTargetVerified ||
      !primary.activeTargetVerified || !primary.cleanupVerified) {
    printStatus("primary-cell ensure", status);
    Serial.print("> ");
    return;
  }

  uint8_t original[CONFIG_BYTES] = {};
  status = readPersistentConfiguration(original);
  if (!status.ok()) {
    printStatus("read original persistent configuration", status);
    Serial.print("> ");
    return;
  }
  if ((original[0] & RV3032::cmd::PMU_BSM_MASK) !=
          RV3032::cmd::PMU_BSM_LEVEL ||
      (original[0] & RV3032::cmd::PMU_TCM_MASK) != 0U) {
    Serial.println("[FAIL] original C0 is not primary-cell level/charger-off");
    Serial.print("> ");
    return;
  }
  printBytes("ORIGINAL_C0_C5", original);

  status = clearBackupFlag();
  if (!status.ok()) {
    printStatus("clear BSF before staging", status);
    Serial.print("> ");
    return;
  }
  status = writeRecord(original, PHASE_STAGING, 0U);
  if (!status.ok()) {
    printStatus("write staging record", status);
    Serial.print("> ");
    return;
  }

  const Settings alternate = makeAlternateSettings(original);
  uint8_t expected[CONFIG_BYTES] = {};
  encodeExpected(original, alternate, expected);
  printBytes("ALTERNATE_C0_C5", expected);
  gOwner.writeOneCommands = 0U;
  status = applySettings(alternate);
  if (status.ok()) {
    status = verifyExact(expected);
  }
  if (!status.ok()) {
    printStatus("stage alternate configuration", status);
    const Settings saved = decodeSettings(original);
    const RV3032::Status restore = applySettings(saved);
    if (!restore.ok()) {
      printStatus("emergency restore original configuration", restore);
    }
    markAborted("stage failed");
    Serial.print("> ");
    return;
  }

  const uint8_t stageWrites = gOwner.writeOneCommands;
  status = writeRecord(original, PHASE_ALT_WAIT, stageWrites);
  if (!status.ok()) {
    printStatus("write alternate-wait marker", status);
    Serial.print("> ");
    return;
  }
  Serial.printf("PERSIST_STAGE_PASS write_one=%u\n", stageWrites);
  Serial.println("POWER_CYCLE_1_REQUIRED");
  Serial.print("> ");
}

void handleAlternateReturn() {
  const uint8_t stageWrites = gRecord.writeOneCommands;
  const Settings alternate = makeAlternateSettings(gRecord.original);
  uint8_t expected[CONFIG_BYTES] = {};
  encodeExpected(gRecord.original, alternate, expected);
  RV3032::Status status = verifyExact(expected);
  if (!status.ok()) {
    printStatus("verify alternate after power cycle", status);
  }

  gOwner.writeOneCommands = 0U;
  const Settings saved = decodeSettings(gRecord.original);
  const RV3032::Status restoreStatus = applySettings(saved);
  if (!restoreStatus.ok()) {
    printStatus("restore original configuration", restoreStatus);
    markAborted("restore failed");
    return;
  }
  const RV3032::Status verifyRestore = verifyExact(gRecord.original);
  if (!verifyRestore.ok()) {
    printStatus("verify restored configuration", verifyRestore);
    markAborted("restore verification failed");
    return;
  }
  if (!status.ok()) {
    markAborted("alternate power-cycle verification failed");
    return;
  }

  const uint8_t restoreWrites = gOwner.writeOneCommands;
  RV3032::Status marker = writeRecord(
      gRecord.original, PHASE_RESTORE_WAIT, restoreWrites);
  if (!marker.ok()) {
    printStatus("write restore-wait marker", marker);
    return;
  }
  marker = clearBackupFlag();
  if (!marker.ok()) {
    printStatus("clear BSF after first cycle", marker);
    return;
  }
  Serial.printf("ALT_POWER_CYCLE_PASS stage_write_one=%u\n",
                stageWrites);
  Serial.printf("ORIGINAL_RESTORE_PASS restore_write_one=%u\n",
                restoreWrites);
  Serial.println("POWER_CYCLE_2_REQUIRED");
}

void handleRestoredReturn() {
  const RV3032::Status status = verifyExact(gRecord.original);
  if (!status.ok()) {
    printStatus("verify original after second power cycle", status);
    markAborted("second power-cycle verification failed");
    return;
  }
  RV3032::Status marker = writeRecord(gRecord.original, PHASE_COMPLETE, 0U);
  if (!marker.ok()) {
    printStatus("write complete marker", marker);
    return;
  }
  marker = clearBackupFlag();
  if (!marker.ok()) {
    printStatus("clear BSF after second cycle", marker);
    return;
  }
  Serial.println("PERSIST_POWER_CYCLE_COMPLETE");
  printBytes("RESTORED_C0_C5", gRecord.original);
}

void recoverInterruptedStage() {
  gOwner.writeOneCommands = 0U;
  const Settings saved = decodeSettings(gRecord.original);
  RV3032::Status status = applySettings(saved);
  if (status.ok()) {
    status = verifyExact(gRecord.original);
  }
  if (!status.ok()) {
    printStatus("recover interrupted stage", status);
    return;
  }
  status = writeRecord(gRecord.original, PHASE_ABORTED,
                       gOwner.writeOneCommands);
  if (!status.ok()) {
    printStatus("write interrupted-stage recovery marker", status);
    return;
  }
  status = clearBackupFlag();
  if (!status.ok()) {
    printStatus("clear BSF after interrupted-stage recovery", status);
    return;
  }
  Serial.printf("INTERRUPTED_STAGE_RESTORED write_one=%u\n",
                gOwner.writeOneCommands);
}

void reportWaitingPhase() {
  if (gRecord.phase == PHASE_ALT_WAIT) {
    Serial.printf("PERSIST_STAGE_RECORDED stage_write_one=%u\n",
                  gRecord.writeOneCommands);
    Serial.println("WAITING_FOR_POWER_CYCLE_1");
  } else if (gRecord.phase == PHASE_RESTORE_WAIT) {
    Serial.printf("ALT_AND_RESTORE_RECORDED restore_write_one=%u\n",
                  gRecord.writeOneCommands);
    Serial.println("WAITING_FOR_POWER_CYCLE_2");
  } else if (gRecord.phase == PHASE_COMPLETE) {
    Serial.println("PERSIST_POWER_CYCLE_COMPLETE");
    printBytes("RESTORED_C0_C5", gRecord.original);
  } else if (gRecord.phase == PHASE_ABORTED) {
    Serial.println("PERSIST_HIL_ABORTED");
    printBytes("ORIGINAL_C0_C5", gRecord.original);
  } else if (gRecord.phase == PHASE_STAGING) {
    Serial.println("PERSIST_HIL_INTERRUPTED_DURING_STAGE");
    printBytes("ORIGINAL_C0_C5", gRecord.original);
  } else {
    Serial.println("PERSIST_HIL_READY command=stage");
  }
  Serial.print("> ");
}

void processLine() {
  gLine[gLineLength] = '\0';
  if (strcmp(gLine, "stage") == 0) {
    runStage();
  } else if (strcmp(gLine, "status") == 0) {
    reportWaitingPhase();
  } else if (gLineLength != 0U) {
    Serial.println("[FAIL] command must be stage or status");
    Serial.print("> ");
  } else {
    Serial.print("> ");
  }
  gLineLength = 0U;
}

void runSetup() {
  delay(1000U);
  Serial.begin(115200);
  const uint32_t serialDeadline = millis() + 30000U;
  while (!Serial && before(serialDeadline)) {
    delay(10U);
  }
  Serial.println("PERSIST_HIL_BEGIN RV3032-C7 COM20 v1");

  if (!transport::initWire(8, 9, 400000U, 50U)) {
    Serial.println("[FAIL] application-owned I2C initialization");
    return;
  }
  RV3032::Config config{};
  config.i2cWrite = ownerWrite;
  config.i2cWriteRead = ownerWriteRead;
  config.i2cUser = &gOwner;
  config.nowMs = nowMs;
  config.waitMs = waitMs;
  config.timeUser = nullptr;
  config.enableEepromWrites = true;
  config.eepromTimeoutMs = 100U;
  RV3032::Status status = gRtc.begin(config);
  if (!status.ok()) {
    printStatus("begin", status);
    return;
  }
  status = gRtc.probe();
  if (!status.ok()) {
    printStatus("probe", status);
    return;
  }
  status = readRecord(gRecord);
  if (!status.ok()) {
    printStatus("read phase record", status);
    return;
  }
  if (!isKnownPhase(gRecord.phase)) {
    gRecord = PhaseRecord{};
  }

  bool switched = false;
  if (!readBackupSwitched(switched)) {
    return;
  }
  if (gRecord.phase == PHASE_STAGING) {
    recoverInterruptedStage();
  } else if (gRecord.phase == PHASE_ALT_WAIT && switched) {
    handleAlternateReturn();
  } else if (gRecord.phase == PHASE_RESTORE_WAIT && switched) {
    handleRestoredReturn();
  }
  reportWaitingPhase();
}

}  // namespace

void setup() { runSetup(); }

void loop() {
  while (Serial.available() > 0) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r') {
      continue;
    }
    if (value == '\n') {
      processLine();
      continue;
    }
    if (gLineLength + 1U < sizeof(gLine)) {
      gLine[gLineLength++] = value;
    } else {
      gLineLength = 0U;
      Serial.println("[FAIL] input line too long");
      Serial.print("> ");
    }
  }
  delay(10U);
}
