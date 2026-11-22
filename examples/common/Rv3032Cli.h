#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdarg>

#include "RV3032/RV3032.h"

namespace rv3032cli {

inline constexpr size_t MAX_LINE_LENGTH = 127U;
inline constexpr size_t MAX_INPUT_BYTES_PER_POLL = 256U;
inline constexpr uint32_t PENDING_WARNING_MS = 15000U;

enum class ScanResult { PRESENT, ABSENT, TIMEOUT };
enum class PendingSurface { NONE, ORDINARY_JOB, EEPROM };

struct PendingOperation {
  PendingSurface surface = PendingSurface::NONE;
  const char* name = nullptr;
  RV3032::Status ordinaryStatus = RV3032::Status::Ok();
  RV3032::Status lastStatus = RV3032::Status::Ok();
  uint32_t startedMs = 0;
  bool overdueReported = false;
};

class Engine;

struct Hooks {
  void* user;
  // endLine appends the platform console's native line ending.
  void (*write)(void*, const char* text, bool endLine);
  int (*readByte)(void*);
  size_t (*available)(void*);
  uint32_t (*nowMs)(void*);
  uint32_t (*nowUs)(void*);
  // A zero duration yields; positive waits check elapsed monotonic time.
  void (*sleepMs)(void*, uint32_t durationMs);
  ScanResult (*probeAddress)(void*, uint8_t address, uint16_t timeoutMs);
  void (*platformVersion)(void*, Engine&);
};

const char* clkoutFrequencyName(RV3032::ClkoutFrequency frequency);

class Engine {
 public:
  Engine(RV3032::RV3032& rtc, Hooks hooks);

  void startBus(int sda, int scl);
  bool busResult(bool ok);
  bool beginResult(const RV3032::Status& status);
  bool probeResult(const RV3032::Status& status);

  // Run one pass; the application owns the cadence and driver lifecycle.
  void poll();
  void dispatch(const char* line);
  void print(const char* format, ...);
  void scan(uint16_t timeoutMs = 50);

  const PendingOperation& pending() const { return _pending; }
  bool verbose() const { return _verbose; }
  void reset();
  bool operationAccepted(const char* name, const RV3032::Status& status);
  void pollPendingOperation(uint32_t nowMs);
  RV3032::Status readUserEepromChunk(uint8_t offset, uint8_t length, uint8_t* out);

 private:
  struct Arguments;
  RV3032::RV3032& _rtc;
  Hooks _hooks;
  PendingOperation _pending{};
  bool _verbose = false;
  char _line[MAX_LINE_LENGTH + 1U]{};
  size_t _length = 0;
  size_t _discardRemaining = 0;
  bool _discardUntilNewline = false;

  bool readLine(bool discardInput);
  void dispatchLine(char* line, size_t length);
  void println(const char* format = "", ...);
  void writeFormatted(bool endLine, const char* format, va_list args);
  void printHelpSection(const char* title);
  void printHelpItem(const char* command, const char* description);

  void printConfigurationReport(const char* name);
  void reportPendingCompletion(const RV3032::Status* persistenceStatus);
  void reportPendingOverdue(uint32_t nowMs);
  void printVerboseStatus(const char* op, const RV3032::Status& st);
  void printHelp();
  void cmdVersion();
  void printDatetime(const RV3032::DateTime& dt);
  void warnIfTimeInvalid();
  void cmdTime();
  void cmdSet(const Arguments& args);
  void cmdSetbuild();
  void cmdUnix(const Arguments& args);
  void cmdTemp();
  void cmdTs(const char* argument);
  void cmdTsReset(const char* argument);
  void cmdAlarm();
  void cmdAlarmSet(const Arguments& args);
  void cmdAlarmMatch(const Arguments& args);
  void cmdAlarmInt(const Arguments& args);
  void cmdAlarmClear();
  void cmdClkout(const Arguments& args);
  void cmdClkoutFreq(const Arguments& args);
  void cmdOffset(const Arguments& args);
  void cmdTimer(const Arguments& args);
  void cmdEvi(const Arguments& args);
  void cmdStatusf();
  void cmdStatusClear(const Arguments& args);
  void cmdReg(const Arguments& args);
  void cmdRam(const Arguments& args);
  void cmdRamWrite(const Arguments& args);
  void cmdStatus();
  void cmdValidity();
  void cmdEeprom();
  void cmdBackup(const char* argument, bool hasArgument);
  void cmdPrimaryCell(const Arguments& args);
  void cmdClearBsf();
  void cmdClearPorf();
  void cmdClearVlf();
  void cmdDrv();
  void cmdProbe();
  void cmdRecover();
  void cmdVerbose(const Arguments& args);
  void cmdStress(const Arguments& args);
  void cmdStressMix(const Arguments& args);
  void cmdSelftest();
};

}  // namespace rv3032cli
