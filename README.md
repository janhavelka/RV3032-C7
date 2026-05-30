# RV3032-C7

Robust **ESP32 (S2/S3)** driver for **Micro Crystal RV-3032-C7** real-time clock module. The core driver is framework-neutral; the current examples and PlatformIO build targets use Arduino adapters.

[![CI](https://github.com/janhavelka/RV3032-C7/actions/workflows/ci.yml/badge.svg)](https://github.com/janhavelka/RV3032-C7/actions/workflows/ci.yml)

## Features

- **Ultra-low power RTC** with battery backup (<1 uA typical)
- **Temperature compensated** crystal oscillator (TCXO)
- **Alarm functionality** with INT pin output
- **Periodic countdown timer** with programmable frequency
- **External event input configuration** (EVI edge/debounce/overwrite)
- **Programmable CLKOUT** output (32.768 kHz to 1 Hz)
- **Frequency offset calibration** (+/-200 ppm range)
- **Built-in temperature sensor** (+/-3 C accuracy)
- **EEPROM** for persistent configuration
- **Deterministic lifecycle** with begin/tick/end and non-blocking EEPROM handling
- **Unix timestamp** support
- **No heap allocation** in steady state

## Hardware

**RV-3032-C7** specifications:
- I2C interface (address: 0x51)
- Supply voltage: 1.1V - 5.5V
- Timekeeping current: <200 nA (typical)
- Backup current: <45 nA (typical)
- Temperature range: -40 C to +85 C
- Accuracy: +/-5 ppm (with calibration)

**Typical wiring (ESP32):**
```
RV-3032   ESP32
------    -----
SDA   ->  GPIO8 (or custom)
SCL   ->  GPIO9 (or custom)
VDD   ->  3.3V
VSS   ->  GND
VBAT  ->  CR2032 battery +
```

## Quickstart

```cpp
#include <Wire.h>
#include "RV3032/RV3032.h"
#include "examples/common/BoardConfig.h"  // Example-only defaults + Wire adapter

RV3032::RV3032 rtc;

void setup() {
  Serial.begin(115200);
  board::initI2c();  // Initialize I2C (example defaults)

  // Configure RTC
  RV3032::Config cfg;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = &Wire;
  cfg.backupMode = RV3032::BackupSwitchMode::Level;

  // Initialize RTC
  RV3032::Status st = rtc.begin(cfg);
  if (!st.ok()) {
    Serial.printf("RTC init failed: %s\n", st.msg);
    return;
  }

  // Set time from build timestamp (first boot only)
  RV3032::DateTime now;
  if (RV3032::RV3032::parseBuildTime(now)) {
    rtc.setTime(now);
  }
}

void loop() {
  rtc.tick(millis());  // Cooperative update (non-blocking)

  // Read time every second
  RV3032::DateTime dt;
  if (rtc.readTime(dt).ok()) {
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                  dt.year, dt.month, dt.day,
                  dt.hour, dt.minute, dt.second);
  }
  delay(1000);
}
```

## Versioning

The library version is defined in [library.json](library.json). A pre-build script automatically generates `include/RV3032/Version.h` with version constants.

**Print version in your code:**
```cpp
#include "RV3032/Version.h"

Serial.println(RV3032::VERSION);           // "<semver>"
Serial.println(RV3032::VERSION_FULL);      // "<semver> (<git>, <timestamp>)"
Serial.println(RV3032::BUILD_TIMESTAMP);   // "<YYYY-MM-DD hh:mm:ss>"
Serial.println(RV3032::GIT_COMMIT);        // "<git>"
```

**Update version:** Edit `library.json` only. `Version.h` is auto-generated on every build.

## API

The library follows a **begin/tick/end** lifecycle with **Status** error handling:

### Core Methods

| Method | Description |
|--------|-------------|
| `Status begin(const Config&)` | Initialize RTC with configuration |
| `void tick(uint32_t now_ms)` | Cooperative update, call from `loop()` |
| `void end()` | Stop and release resources |
| `bool isInitialized() const` | Check if RTC initialized |
| `const Config& getConfig() const` | Get current configuration |
| `Status getSettings(SettingsSnapshot& out)` | Populate a snapshot of cached config, EEPROM state, and health counters (no I2C) |
| `Status recover()` | Re-probe the RTC and return the driver to READY on success |
| `bool isEepromBusy() const` | Check if EEPROM persistence is active |
| `Status getEepromStatus() const` | Get last EEPROM persistence status |
| `uint32_t eepromWriteCount() const` | Get successful EEPROM commit count |
| `uint32_t eepromWriteFailures() const` | Get failed EEPROM commit count |
| `uint8_t eepromQueueDepth() const` | Get EEPROM queue depth |

### User EEPROM Operations

| Method | Description |
|--------|-------------|
| `Status readUserEepromByte(uint8_t offset, uint8_t& out)` | Read one user EEPROM byte at offset `0..31` |
| `Status writeUserEepromByte(uint8_t offset, uint8_t value)` | Write one user EEPROM byte at offset `0..31` |
| `Status readUserEeprom(uint8_t offset, uint8_t* dst, size_t len)` | Read a user EEPROM block |
| `Status writeUserEeprom(uint8_t offset, const uint8_t* src, size_t len)` | Write a user EEPROM block as one-byte commands |

User EEPROM offsets map to physical EEPROM addresses `0xCB..0xEA`. The driver
uses `EEADDR`, `EEDATA`, and `EECMD` internally, waits for `EEbusy` with bounded
polling, treats `EEF` as `EEPROM_WRITE_FAILED` on writes, and never clears `EEF`
implicitly.

```cpp
uint8_t bytes[4] = {0x52, 0x56, 0x33, 0x32};
RV3032::Status st = rtc.writeUserEeprom(0, bytes, sizeof(bytes));
if (st.ok()) {
  uint8_t readback[4] = {};
  st = rtc.readUserEeprom(0, readback, sizeof(readback));
}
```

### Health / Recovery

| Method | Description |
|--------|-------------|
| `DriverState state() const` | Current driver health state |
| `bool isOnline() const` | `true` in READY or DEGRADED |
| `Status lastError() const` | Most recent tracked transport error |
| `uint32_t lastOkMs() const` / `lastErrorMs() const` | Last success/error timestamps from the driver timebase |
| `uint8_t consecutiveFailures() const` | Consecutive tracked failures since last success |
| `uint32_t totalSuccess() const` / `totalFailures() const` | Lifetime tracked I2C counters |

### Time/Date Operations

| Method | Description |
|--------|-------------|
| `Status readTime(DateTime& out)` | Read current time/date with one coherent seconds-through-year burst |
| `Status setTime(const DateTime& time)` | Set RTC time/date using the STOP-controlled synchronization sequence |
| `Status readUnix(uint32_t& out)` | Read time as Unix timestamp |
| `Status setUnix(uint32_t ts)` | Set time from Unix timestamp |

### Alarm Operations

| Method | Description |
|--------|-------------|
| `Status setAlarmTime(uint8_t minute, uint8_t hour, uint8_t date)` | Set alarm time values |
| `Status setAlarmMatch(bool minute, bool hour, bool date)` | Configure which components match |
| `Status getAlarmConfig(AlarmConfig& out)` | Read alarm configuration |
| `Status getAlarmFlag(bool& triggered)` | Check if alarm triggered |
| `Status clearAlarmFlag()` | Clear alarm flag |
| `Status enableAlarmInterrupt(bool enable)` | Enable/disable INT pin output |
| `Status getAlarmInterruptEnabled(bool& enabled)` | Read alarm INT routing state |

`getAlarmConfig()` reports `AlarmConfig::date == 0` for the RV3032-C7 reset/default alarm state where the date alarm register is `00h` and the alarm function is inactive.

### Timer Operations

| Method | Description |
|--------|-------------|
| `Status setTimer(uint16_t ticks, TimerFrequency freq, bool enable)` | Configure countdown timer |
| `Status getTimer(uint16_t& ticks, TimerFrequency& freq, bool& enabled)` | Read timer configuration |
| `Status enableTimerInterrupt(bool enable)` | Enable/disable TF routing to INT |
| `Status getTimerInterruptEnabled(bool& enabled)` | Read timer INT routing state |

### Clock Output

| Method | Description |
|--------|-------------|
| `Status setClkoutEnabled(bool enabled)` | Enable/disable CLKOUT pin |
| `Status setClkoutFrequency(ClkoutFrequency freq)` | Set output frequency (32.768 kHz to 1 Hz) |
| `Status getClkoutEnabled(bool& enabled)` | Check if CLKOUT enabled |
| `Status getClkoutFrequency(ClkoutFrequency& freq)` | Read output frequency |

### Calibration

| Method | Description |
|--------|-------------|
| `Status setOffsetPpm(float ppm)` | Set frequency offset in parts-per-million |
| `Status getOffsetPpm(float& ppm)` | Read frequency offset |

### Temperature Sensor

| Method | Description |
|--------|-------------|
| `Status readTemperatureC(float& celsius)` | Read die temperature (+/-3 C accuracy) |

### External Event Input

| Method | Description |
|--------|-------------|
| `Status setEviEdge(bool rising)` | Set edge sensitivity (rising/falling) |
| `Status setEviDebounce(EviDebounce debounce)` | Set debounce filter |
| `Status setEviOverwrite(bool enable)` | Allow overwriting timestamps |
| `Status enableEventInterrupt(bool enable)` | Enable/disable EVF routing to INT |
| `Status getEventInterruptEnabled(bool& enabled)` | Read EVI INT routing state |
| `Status getEviConfig(EviConfig& out)` | Read EVI configuration |
| `Status readTimestamp(TimestampSource source, Timestamp& out)` | Read decoded hardware timestamp block |
| `Status resetTimestamp(TimestampSource source)` | Reset a hardware timestamp block |

ESYN external-event synchronization is not enabled implicitly by ordinary `setTime()`.
A dedicated high-level ESYN arm/cancel API remains deferred; direct register access to
`REG_EVI_CONTROL` is diagnostic-only.
EVI is disabled while the RV3032 is in VBACKUP; production hardware must not let
the EVI pin float and must not tie EVI to VBACKUP.

### Status & Low-Level

| Method | Description |
|--------|-------------|
| `Status setBackupSwitchMode(BackupSwitchMode mode)` | Set battery-backup switchover mode |
| `Status setPrimaryBatteryBackupDefaults()` | Apply primary-cell backup defaults: level switching, trickle charger off |
| `Status getBackupSwitchMode(BackupSwitchMode& mode)` | Read backup switchover mode from PMU |
| `Status readStatus(uint8_t& status)` | Read status register |
| `Status clearStatus(uint8_t mask)` | Clear STATUS flags explicitly by mask |
| `Status readStatusFlags(StatusFlags& out)` | Read decoded STATUS and 0x0E fault flags |
| `Status readValidity(ValidityFlags& out)` | Read PORF/VLF/BSF/EEF/EEbusy/CLKF validity flags |
| `Status clearFaultFlags(uint8_t mask)` | Clear EEF/CLKF/BSF fault flags explicitly by mask |
| `Status clearTimerFlag()` | Clear TF |
| `Status clearUpdateFlag()` | Clear UF |
| `Status clearEventFlag()` | Clear EVF |
| `Status clearBackupSwitchFlag()` | Clear backup switchover flag (BSF) |
| `Status clearEepromErrorFlag()` | Clear EEPROM write-access failure flag (EEF) |
| `Status clearClockFlag()` | Clear interrupt-controlled clock flag (CLKF) |
| `Status clearPowerOnResetFlag()` | Clear the PORF validity flag after handling a reset event |
| `Status clearVoltageLowFlag()` | Clear the VLF validity flag after voltage monitoring |
| `bool isEepromBusy() const` | Check if EEPROM commit is in progress |
| `Status getEepromStatus() const` | Get last EEPROM commit status |
| `uint32_t eepromWriteCount() const` | Get successful EEPROM commit count |
| `uint32_t eepromWriteFailures() const` | Get failed EEPROM commit count |
| `uint8_t eepromQueueDepth() const` | Get EEPROM queue depth |
| `Status readRegister(uint8_t reg, uint8_t& value)` | Read single register |
| `Status writeRegister(uint8_t reg, uint8_t value)` | Write single register |
| `Status readRegisters(uint8_t reg, uint8_t* buf, size_t len)` | Read contiguous register block |
| `Status writeRegisters(uint8_t reg, const uint8_t* buf, size_t len)` | Write contiguous register block |

### Static Utility Functions

| Method | Description |
|--------|-------------|
| `static bool isValidDateTime(const DateTime& time)` | Validate date/time structure |
| `static uint8_t computeWeekday(uint16_t year, uint8_t month, uint8_t day)` | Calculate day of week |
| `static bool parseBuildTime(DateTime& out)` | Parse __DATE__ and __TIME__ macros |
| `static uint8_t bcdToBinary(uint8_t bcd)` | Convert BCD to binary |
| `static uint8_t binaryToBcd(uint8_t bin)` | Convert binary to BCD |
| `static bool unixToDateTime(uint32_t ts, DateTime& out)` | Convert Unix timestamp to DateTime |
| `static bool dateTimeToUnix(const DateTime& time, uint32_t& out)` | Convert DateTime to Unix timestamp |

## Configuration

Configuration is injected via `Config` struct. The library **never hardcodes pins** and never owns the I2C bus.

```cpp
namespace RV3032 {
  struct Config {
    I2cWriteFn i2cWrite = nullptr;               // I2C write callback (required)
    I2cWriteReadFn i2cWriteRead = nullptr;       // I2C write-read callback (required)
    void* i2cUser = nullptr;                     // Transport-owned context
    NowMsFn nowMs = nullptr;                     // Optional monotonic clock callback
    void* timeUser = nullptr;                    // User context for timing callback
    uint8_t i2cAddress = 0x51;                   // RV3032 I2C address
    uint32_t i2cTimeoutMs = 50;                  // I2C transaction timeout
    BackupSwitchMode backupMode = BackupSwitchMode::Level;  // Battery backup mode
    bool enableEepromWrites = true;              // Persistent config (EEPROM)
    uint32_t eepromTimeoutMs = 100;              // EEPROM write timeout
    uint8_t offlineThreshold = 5;                // Failures before OFFLINE
  };
}
```

**Configuration rules:**
- `i2cWrite` and `i2cWriteRead` must be provided before `begin()`
- The application or transport adapter owns pins, bus initialization/deinitialization, locks, timeout policy, and bus recovery.
- Transport callbacks must not recursively call into the same `RV3032` instance.
- `i2cAddress`: Fixed at `0x51` on RV3032-C7 hardware
- `backupMode`: Off=no backup, Level=threshold (default), Direct=immediate
- Invalid `backupMode` enum values are rejected by `begin()` before any I2C access.
- `i2cTimeoutMs`: Passed to the transport callback (default 50ms); must be >= 50ms when EEPROM writes are enabled.
- `enableEepromWrites`: When `false`, config changes are RAM-only (faster, saves EEPROM wear). When `true` (default), changes persist across power loss and complete asynchronously.
- `eepromTimeoutMs`: Maximum time for EEPROM writes to complete (default 100ms)
- `nowMs` / `timeUser`: Optional injected timebase used for health timestamps and non-blocking EEPROM deadlines
- `offlineThreshold`: Consecutive tracked I2C failures required before transitioning to `OFFLINE`

## Error Handling

All library functions return `Status` struct:

```cpp
struct Status {
  Err code;           // Error category (OK, I2C_ERROR, TIMEOUT, ...)
  int32_t detail;     // I2C error code or vendor detail
  const char* msg;    // Static error message (never heap-allocated)

  bool ok() const;    // Returns true if code == Err::OK
};
```

**Error codes:**
- `OK` - Operation successful
- `NOT_INITIALIZED` - Call `begin()` first
- `INVALID_CONFIG` - Invalid configuration parameter
- `I2C_ERROR` - I2C communication failure
- `TIMEOUT` - Operation timed out (EEPROM write)
- `INVALID_PARAM` - Invalid parameter value
- `INVALID_DATETIME` - Invalid date/time value
- `DEVICE_NOT_FOUND` - RTC not responding on I2C bus
- `EEPROM_WRITE_FAILED` - EEPROM update failed
- `REGISTER_READ_FAILED` - Register read failed
- `REGISTER_WRITE_FAILED` - Register write failed
- `IN_PROGRESS` - EEPROM persistence queued or active
- `QUEUE_FULL` - EEPROM persistence queue full
- `I2C_NACK_ADDR` - I2C address not acknowledged
- `I2C_NACK_DATA` - I2C data byte not acknowledged
- `I2C_TIMEOUT` - I2C transaction timed out
- `I2C_BUS` - I2C bus error (arbitration lost, stuck bus, etc.)

**Example error handling:**
```cpp
RV3032::Status st = rtc.setTime(dt);
if (!st.ok()) {
  Serial.printf("Error: %s (code=%d, detail=%d)\n",
                st.msg, static_cast<int>(st.code), st.detail);
}
```

## Behavioral Contracts

**Framework-Neutral Core:** Files under `include/` and `src/` do not include or call Arduino, ESP-IDF, FreeRTOS, logging, scheduler, or platform timing APIs. Framework-specific code belongs in examples or application transport adapters.

**Injected Transport:** I2C access is supplied by `Config::i2cWrite` and `Config::i2cWriteRead`. The driver does not own pins, bus handles, bus init/deinit, locks, recovery, or platform timeout configuration; it only passes `i2cTimeoutMs` to the injected callbacks.

**Threading Model:** Single-threaded by default. The driver is not internally synchronized and public APIs are not ISR-safe unless an API explicitly says otherwise. Applications must serialize shared driver or shared-bus access externally.

**Timing:** `tick()` completes in <1ms. When EEPROM persistence is enabled, each call performs at most one I2C operation and uses deadline checks (no delay). Other API calls perform synchronous I2C transactions bounded by the transport timeout. `Config::nowMs` is optional; if omitted, driver-owned timestamps are `0`, and elapsed EEPROM work is driven by the `now_ms` argument passed to `tick()`.

**Clock/Calendar Coherence:** `readTime()` reads registers `0x01..0x07` in a single burst and validates reserved bits, BCD encoding, decoded ranges, dates, and leap years. The RV3032 blocks clock/calendar counters during RTC register transactions shorter than the device's 950ms I2C timeout, so the returned seconds-through-year fields are coherent when the transport completes normally. `readTime()` does not read `100th Seconds` and does not clear fault flags.

**STOP-Controlled Time Set:** `setTime()` reads Control 2, sets STOP, writes seconds through year in one burst, then releases STOP while preserving unrelated Control 2 bits. Setting STOP and writing Seconds reset the 100th-seconds/prescaler state per the RV3032 manual. If a partial failure occurs after hardware may have changed, `SettingsSnapshot::clockCalendarUncertain` is set with the failure status and is cleared only by a later successful STOP-controlled `setTime()`.

**ESYN:** ESYN is EVI Control `0x15` bit 0 and is not used as an implicit side effect of ordinary `setTime()`. External-event synchronization arm/cancel semantics remain deferred to a dedicated API.

**INT Output:** INT is an open-drain, active-low output and needs an application-provided pull-up to the intended rail. Multiple enabled sources can assert INT; service code should read flags, handle each source, then explicitly clear only the handled flags. Enabling alarm/timer/EVI interrupts routes AF/TF/EVF to INT but does not clear flags.

**Fault Flags:** Reading flags never clears them. `StatusFlags` exposes THF, TLF, UF, TF, AF, EVF, PORF, VLF, EEF, EEbusy, CLKF, and BSF. `ValidityFlags::timeInvalid` is true when PORF or VLF is set; PORF/VLF mean the time/configuration must be reinitialized or verified from a trusted external source before clearing. `clearStatus(0)` and `clearFaultFlags(0)` perform no register write. Nonzero clears write `0` only for requested flags and `1` for non-target flags to avoid stale read-modify-write clears. Any nonzero STATUS write clears THF/TLF on the RV3032 regardless of mask.

**RTC Fault Recovery:** After PORF/VLF/BSF/EEF, recover the bus if needed, call `probe()`, read validity/fault flags, reinitialize configuration and set time from a trusted source when PORF or VLF is set, verify operation, then explicitly clear only the handled flags.

**Resource Ownership:** I2C transport passed via `Config`. No hardcoded pins or resources.

**Memory:** All allocation in `begin()`. Zero allocation in `tick()` and normal operations.

**Error Handling:** All errors returned as `Status`. No silent failures.

**Probe / Recovery Errors:** `probe()` is diagnostic-only and does not update health. It maps a definite address NACK to `DEVICE_NOT_FOUND`, while preserving data NACK, timeout, bus, and generic I2C errors. `recover()` uses tracked I2C and returns the tracked transport/configuration error so `lastError()` and the returned status agree.

**Health / Recovery:** `OFFLINE` is latched. Normal public I2C operations and EEPROM `tick()` work do not touch the bus while OFFLINE; call `recover()` after application-level bus recovery to return to `READY`.

**Low-level Register Access:** `readRegister()`, `writeRegister()`, `readRegisters()`, and `writeRegisters()` are diagnostic-only. They validate documented RV3032 address windows, reject wraparound and invalid buffers, reject the user EEPROM data range `0xCB..0xEA`, and do not count local validation failures against driver health. Direct writes to password, EEADDR, EEDATA, EECMD, protected control, alarm, timer, or clock/calendar registers can alter hardware state and should not be used as production configuration APIs.

**User EEPROM:** User EEPROM APIs use offsets `0..31` and the manual's `EEADDR`/`EEDATA`/`EECMD` one-byte command flow. They are synchronous and bounded by `EEbusy` polling; they do not use the asynchronous config-EEPROM queue. `SettingsSnapshot` reports the last user EEPROM command status plus timeout, EEF, and dirty diagnostics when a write may be partial or unverified. Password/write-protection APIs are not implemented; if password protection is enabled on hardware, the application must unlock the device before using these APIs.

**Timer/Alarm/Event Validation Checklist:** On hardware, verify user EEPROM read/write/readback and EEF handling, timer disabled-write-enabled ordering and TF clearing, alarm AF/AIE separation, EVI edge/debounce/EVF behavior with EVI held at a defined level, INT pull-up rail/value and active-low release, multiple interrupt sources sharing INT, and VBACKUP behavior for EVI and INT.

**Known Deferrals:** Full dirty-state diagnostics for Control register read-modify-write outside `setTime()`, timer configuration, alarm configuration, password/write-protection APIs, high-level ESYN arm/cancel APIs, and full interrupt-controlled CLKOUT configuration helpers are deferred to later hardening chunks. Current config EEPROM persistence diagnostics cover the existing asynchronous configuration write path through `isEepromBusy()`, `getEepromStatus()`, `eepromWriteCount()`, `eepromWriteFailures()`, and `eepromQueueDepth()`.

**EEPROM Usage:** When `Config::enableEepromWrites` is `true`, the following operations write to EEPROM:
- `setClkoutEnabled()` / `setClkoutFrequency()`
- `setOffsetPpm()`
- Backup mode changes in `begin()`, `setBackupSwitchMode()`, and `setPrimaryBatteryBackupDefaults()`

EEPROM persistence is asynchronous. Methods that trigger persistence return `IN_PROGRESS` when queued; call `tick()` until `getEepromStatus().ok()` or an error is reported. If the queue is full, calls return `QUEUE_FULL`.

**Time Retention:** The current time is maintained by the RTC counter while VBACKUP is present; it is not copied into EEPROM. For a normal non-rechargeable backup cell, use `BackupSwitchMode::Level` with the trickle charger disabled. The CLI command `backup usual` applies those PMU settings, then use `set ...` to set the time and clear PORF/VLF/BSF after verifying validity.

EEPROM has finite write endurance. Use `enableEepromWrites = false` in applications with frequent config changes. Enable only when persistent configuration is required across power cycles. Use `isEepromBusy()` to check progress and `getEepromStatus()` for the last config commit result.

## Supported Targets

| Board | Environment | Notes |
|-------|-------------|-------|
| ESP32-S3-MINI-1U-N4R2 | `esp32s3dev` | PSRAM enabled |
| ESP32-S2-MINI-2-N4 | `esp32s2dev` | No PSRAM |

## Examples

### 01_basic_bringup_cli

Interactive CLI demonstrating all RTC features:
- Time reading and setting
- Alarm configuration
- Timer operations
- User EEPROM dump through public EEPROM APIs
- Clock output control
- Calibration (offset adjustment)
- Temperature monitoring

```bash
# Build and upload
pio run -e esp32s3dev -t upload && pio device monitor -e esp32s3dev
```

### Example Helpers (`examples/common/`)

These helpers are example-only glue and are not part of the public library API.

| File | Purpose |
|------|---------|
| `BoardConfig.h` | Board-specific pin defaults and `Wire` setup |
| `BuildConfig.h` | Compile-time log-level configuration |
| `Log.h` | Serial logging helpers |
| `I2cTransport.h` | Wire-backed transport adapter |
| `I2cScanner.h` | Bus scan helper |
| `BusDiag.h` | Bus diagnostics wrapper |
| `CliShell.h` | Simple serial shell helper |
| `CommandHandler.h` | Example command parsing helpers |
| `HealthView.h` | Compact health display helper |
| `HealthDiag.h` | Verbose health diagnostics helper |
| `TransportAdapter.h` | Transport alias helper |

## Running Tests

```bash
pio test -e native
python tools/check_cli_contract.py
python tools/check_core_timing_guard.py
pio run -e esp32s3dev
pio run -e esp32s2dev
```

## Project Structure

```
include/RV3032/         - Public API headers (Doxygen documented)
  - Status.h            - Error types
  - Config.h            - Configuration struct
  - RV3032.h            - Main library class
  - Version.h           - Auto-generated version constants
src/
  - RV3032.cpp          - Implementation
examples/
  - 01_basic_bringup_cli/  - Interactive CLI example
  - common/                - Example-only helpers (shared CLI/log/transport glue)
platformio.ini          - Build environments
library.json            - PlatformIO metadata
README.md               - This file
CHANGELOG.md            - Version history
AGENTS.md               - Coding guidelines
```

## Documentation

- `CHANGELOG.md` - full release history
- `docs/MANAGED_SYNC_DRIVER_PATTERN.md` - managed synchronous driver pattern
- `docs/IDF_PORT.md` - ESP-IDF portability guidance
- `docs/RV3032_Register_Reference.md` - register reference guide
- `docs/RV-3032-C7_datasheet.pdf` - device datasheet
- `docs/RV-3032-C7_App-Manual.pdf` - vendor application manual

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

**Coding standards:**
- Follow [AGENTS.md](AGENTS.md) guidelines
- Non-blocking by default (begin/tick/end pattern)
- Status error model (no exceptions)
- No heap allocation in steady state
- Doxygen documentation for public API

## Production Readiness Notes

- Core code is framework-neutral and uses injected I2C callbacks; Arduino `Wire` and ESP-IDF handles belong in examples/adapters only.
- `Config::nowMs` is optional for initialization, but applications should inject a monotonic clock when using EEPROM persistence or health timestamps. If omitted, driver-owned timestamps report `0`.
- `probe()` is diagnostic-only and preserves timeout, bus, data-NACK, and generic I2C errors. `DEVICE_NOT_FOUND` is reserved for definite address NACK. `recover()` preserves tracked transport errors.
- Low-level direct register access rejects the user EEPROM data range `0xCB..0xEA`; use the offset-based user EEPROM APIs for that storage.
- Driver instances are not thread-safe and public APIs are not ISR-safe. Shared-bus users must serialize access externally.
- No hardware validation was run as part of this hardening report; see `docs/RV3032_HARDENING_FINAL_REPORT.md` for exact checks.

## License

MIT License - see [LICENSE](LICENSE) file.

## References

- [RV-3032-C7 Datasheet](https://www.microcrystal.com/en/products/real-time-clock-rtc-modules/rv-3032-c7/)
- [Application Manual](https://www.microcrystal.com/fileadmin/Media/Products/RTC/App.Manual/RV-3032-C7_App-Manual.pdf)

## Credits

Based on original RtcManager implementation. Refactored to follow ESP32 embedded engineering best practices from [AGENTS.md](AGENTS.md).
