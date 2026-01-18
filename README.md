# RV3032-C7

Robust **ESP32 (S2/S3)** driver for **Micro Crystal RV-3032-C7** real-time clock module using **Arduino framework** with **PlatformIO**.

[![CI](https://github.com/janhavelka/RV3032-C7/actions/workflows/ci.yml/badge.svg)](https://github.com/janhavelka/RV3032-C7/actions/workflows/ci.yml)

## Features

- **Ultra-low power RTC** with battery backup (<1 uA typical)
- **Temperature compensated** crystal oscillator (TCXO)
- **Alarm functionality** with INT pin output
- **Periodic countdown timer** with programmable frequency
- **External event timestamping** (EVI pin)
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

Serial.println(RV3032::VERSION);           // "1.0.0"
Serial.println(RV3032::VERSION_FULL);      // "1.0.0 (a1b2c3d, 2026-01-10 15:30:00)"
Serial.println(RV3032::BUILD_TIMESTAMP);   // "2026-01-10 15:30:00"
Serial.println(RV3032::GIT_COMMIT);        // "a1b2c3d"
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
| `bool isEepromBusy() const` | Check if EEPROM persistence is active |
| `Status getEepromStatus() const` | Get last EEPROM persistence status |

### Time/Date Operations

| Method | Description |
|--------|-------------|
| `Status readTime(DateTime& out)` | Read current time and date |
| `Status setTime(const DateTime& time)` | Set RTC time and date |
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

### Timer Operations

| Method | Description |
|--------|-------------|
| `Status setTimer(uint16_t ticks, TimerFrequency freq, bool enable)` | Configure countdown timer |
| `Status getTimer(uint16_t& ticks, TimerFrequency& freq, bool& enabled)` | Read timer configuration |

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
| `Status getEviConfig(EviConfig& out)` | Read EVI configuration |

### Status & Low-Level

| Method | Description |
|--------|-------------|
| `Status readStatus(uint8_t& status)` | Read status register |
| `Status clearStatus(uint8_t mask)` | Clear status flags |
| `Status readStatusFlags(StatusFlags& out)` | Read decoded status flags |
| `Status readValidity(ValidityFlags& out)` | Read PORF/VLF/BSF validity flags |
| `Status clearBackupSwitchFlag()` | Clear backup switchover flag (BSF) |
| `bool isEepromBusy() const` | Check if EEPROM commit is in progress |
| `Status getEepromStatus() const` | Get last EEPROM commit status |
| `Status readRegister(uint8_t reg, uint8_t& value)` | Read single register |
| `Status writeRegister(uint8_t reg, uint8_t value)` | Write single register |

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

Configuration is injected via `Config` struct. The library **never hardcodes pins**.

```cpp
namespace RV3032 {
  struct Config {
    I2cWriteFn i2cWrite = nullptr;               // I2C write callback (required)
    I2cWriteReadFn i2cWriteRead = nullptr;       // I2C write-read callback (required)
    void* i2cUser = nullptr;                     // User context (e.g., TwoWire*)
    uint8_t i2cAddress = 0x51;                   // RV3032 I2C address
    uint32_t i2cTimeoutMs = 50;                  // I2C transaction timeout
    BackupSwitchMode backupMode = BackupSwitchMode::Level;  // Battery backup mode
    bool enableEepromWrites = false;             // Persistent config (EEPROM)
    uint32_t eepromTimeoutMs = 200;              // EEPROM write timeout
  };
}
```

**Configuration rules:**
- `i2cWrite` and `i2cWriteRead` must be provided before `begin()`
- `i2cAddress`: Valid 7-bit range 0x08-0x77
- `backupMode`: Off=no backup, Level=threshold (default), Direct=immediate
- `i2cTimeoutMs`: Passed to the transport callback (default 50ms); must be >= 50ms when EEPROM writes are enabled.
- `enableEepromWrites`: When `false` (default), config changes are RAM-only (faster, saves EEPROM wear). When `true`, changes persist across power loss and complete asynchronously.
- `eepromTimeoutMs`: Maximum time for EEPROM writes to complete (default 200ms)

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

**Example error handling:**
```cpp
RV3032::Status st = rtc.setTime(dt);
if (!st.ok()) {
  Serial.printf("Error: %s (code=%d, detail=%d)\n",
                st.msg, static_cast<int>(st.code), st.detail);
}
```

## Behavioral Contracts

**Threading Model:** Single-threaded by default. No FreeRTOS tasks created.

**Timing:** `tick()` completes in <1ms. When EEPROM persistence is enabled, each call performs at most one I2C operation and uses deadline checks (no delay). Other API calls perform synchronous I2C transactions bounded by the transport timeout.

**Resource Ownership:** I2C transport passed via `Config`. No hardcoded pins or resources.

**Memory:** All allocation in `begin()`. Zero allocation in `tick()` and normal operations.

**Error Handling:** All errors returned as `Status`. No silent failures.

**EEPROM Usage:** When `Config::enableEepromWrites` is `true`, the following operations write to EEPROM:
- `setClkoutEnabled()` / `setClkoutFrequency()`
- `setOffsetPpm()`
- Backup mode changes in `begin()`

EEPROM persistence is asynchronous. Methods that trigger persistence return `IN_PROGRESS` when queued; call `tick()` until `getEepromStatus().ok()` or an error is reported. If the queue is full, calls return `QUEUE_FULL`.

EEPROM has ~100k write endurance. Use `enableEepromWrites = false` (default) in applications with frequent config changes. Enable only when persistent configuration is required across power cycles. Use `isEepromBusy()` to check progress and `getEepromStatus()` for the last commit result.

## Supported Targets

| Board | Environment | Notes |
|-------|-------------|-------|
| ESP32-S3-MINI-1U-N4R2 | `ex_bringup_s3` | PSRAM enabled |
| ESP32-S2-MINI-2-N4 | `ex_bringup_s2` | No PSRAM |

## Examples

### 01_basic_bringup_cli

Interactive CLI demonstrating all RTC features:
- Time reading and setting
- Alarm configuration
- Timer operations
- Clock output control
- Calibration (offset adjustment)
- Temperature monitoring

```bash
# Build and upload
pio run -e ex_bringup_s3 -t upload && pio device monitor -e ex_bringup_s3
```

## Build

```bash
# Clone repository
git clone https://github.com/janhavelka/RV3032-C7.git
cd RV3032-C7

# Build for ESP32-S3
pio run -e ex_bringup_s3

# Upload and monitor
pio run -e ex_bringup_s3 -t upload
pio device monitor -e ex_bringup_s3
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
  - common/                - Example-only helpers (Log.h, BoardConfig.h, I2cTransport.h, I2cScanner.h)
platformio.ini          - Build environments
library.json            - PlatformIO metadata
README.md               - This file
CHANGELOG.md            - Version history
AGENTS.md               - Coding guidelines
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

**Coding standards:**
- Follow [AGENTS.md](AGENTS.md) guidelines
- Non-blocking by default (begin/tick/end pattern)
- Status error model (no exceptions)
- No heap allocation in steady state
- Doxygen documentation for public API

## License

MIT License - see [LICENSE](LICENSE) file.

## References

- [RV-3032-C7 Datasheet](https://www.microcrystal.com/en/products/real-time-clock-rtc-modules/rv-3032-c7/)
- [Application Manual](https://www.microcrystal.com/fileadmin/Media/Products/RTC/App.Manual/RV-3032-C7_App-Manual.pdf)

## Credits

Based on original RtcManager implementation. Refactored to follow ESP32 embedded engineering best practices from [AGENTS.md](AGENTS.md).
