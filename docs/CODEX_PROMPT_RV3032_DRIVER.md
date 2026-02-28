# CODEX PROMPT -- Build a production-grade RV-3032-C7 RTC driver library (ESP32 / Arduino / PlatformIO)

You are implementing a **new reusable library** for the Micro Crystal **RV-3032-C7** (DTCXO RTC module).

## Hard constraints (must follow)
- Target: ESP32-S2 / ESP32-S3, Arduino framework, PlatformIO.
- Library template rules: follow `AGENTS.md` in this repo.
- Non-blocking by default:
  - `Status begin(const Config&)`
  - `void tick(uint32_t nowMs)`
  - `void end()`
- No heap allocations in steady state (no `String`, no `new`, no `std::vector` in tick/normal ops).
- Library MUST NOT own the I2C bus (no global `Wire`). Bus access is provided via a transport adapter in `Config`.
- Deterministic: bounded work in `tick()`; all waits via deadlines (never `delay()` loops).

## Scope
- **I2C only**
- Device address fixed to **0x51** (7-bit).
- Provide full RTC functionality typically needed in products:
  - read/set time & date
  - alarms / periodic interrupts (as supported)
  - status/flag read + clear
  - temperature readout (device has thermometer)
  - optional timestamp/event features if exposed in registers
- Provide safe access to EEPROM-backed configuration:
  - password/unlock handling
  - commit/write sequencing
  - non-blocking busy waits

## Inputs you must use
- Use the register reference file: `RV3032_Register_Reference.md` (in this repo).
- Do not rely on external RTC libraries.

## Public API design (suggested)

Create a main class `Rv3032` with:
- `Status begin(const Config& cfg);`
- `void tick(uint32_t nowMs);`
- `void end();`

### Time API
- `Status getDateTime(DateTime& out);`
- `Status setDateTime(const DateTime& dt);`
Where `DateTime` uses validated ranges and stores:
- year (e.g., 2000-2099), month 1-12, day 1-31
- hour 0-23, minute 0-59, second 0-59
- weekday (1-7 or 0-6; document mapping)

Implementation requirements:
- Use **burst read** of the whole time block to get a coherent snapshot.
- Handle BCD encode/decode strictly; reject invalid BCD nibbles.

### Alarm / interrupt API
Provide explicit, boring structs:
- `struct AlarmConfig { ... }`
- `Status configureAlarm1(const AlarmConfig& cfg);` (if supported)
- `Status configurePeriodicTimer(const PeriodicTimerConfig& cfg);` (if supported)
- `Status readFlags(Flags& out);`
- `Status clearFlags(uint32_t mask);` (clear only requested flags)

Rules:
- Never clear flags implicitly.
- In examples, demonstrate "read flags -> handle -> clear".

### Temperature API
- `Status readTemperatureC(float& outC);` (or fixed-point int)
- If thresholds exist (TLow/THigh), expose:
  - `Status setTempThresholds(const TempThresholds& t);`
  - `Status getTempThresholds(TempThresholds& t);`

### EEPROM / protected writes
Provide an opt-in API. Default behavior: do not touch EEPROM unless requested.
- `Status unlockEeprom(const Password& pw);`
- `Status lockEeprom();`
- `Status writeEepromConfig(const EepromConfig& cfg);` (non-blocking commit)
- `bool isEepromBusy(uint32_t nowMs) const;`
- `Status serviceEeprom(uint32_t nowMs);` (called from tick as part of a state machine)

Rules:
- Respect register conventions (`R/WP`, `WP`, `*WP`) from the reference.
- Any EEPROM commit or long operation must be split across `tick()` states.
- Enforce a write-rate limiter (configurable) to protect endurance.

## Transport abstraction (mandatory)

In `Config`, include a bus adapter:
- `Status (*i2cWrite)(uint8_t addr7, const uint8_t* data, size_t len);`
- `Status (*i2cWriteRead)(uint8_t addr7, const uint8_t* w, size_t wLen, uint8_t* r, size_t rLen);`

No dynamic allocation; no virtual calls needed; keep it simple and explicit.

## Driver behavior requirements (must implement)
1) **Presence check**
- Read at least one stable register (e.g., status or seconds) and validate BCD format.
- If the manual provides an ID register, use it. If not, use robust heuristics and return INVALID_DEVICE on nonsense.

2) **Coherent time reads**
- Use a single burst read of all time/date registers.
- If the device supports freeze/latch, use it; otherwise burst read is mandatory.

3) **Non-blocking EEPROM handling**
- Any sequence that requires waiting (busy flags, internal copy, etc.) must be deadline-based and progressed in `tick()`.

4) **Write protection**
- If a write fails due to protection, return a clear error (e.g., `WRITE_PROTECTED`).
- Do not silently unlock or auto-write passwords unless explicitly enabled by config.

5) **Error mapping**
- Translate I2C errors to stable `Err` codes:
  - `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `BUS_ERROR`
- Keep `lastError()`.

## Examples to include
- `examples/00_read_time/` -- print time every second, show burst reads
- `examples/01_set_time_alarm/` -- set time + configure an alarm + handle interrupt flags
- Optional: `examples/02_eeprom_config/` -- unlock, write a config, commit, poll busy via tick()

## Deliverables (files to create)
- `include/Rv3032/Status.h`, `Config.h`, `Rv3032.h`, `Types.h`
- `src/` implementation
- README.md with behavioral contracts (timing, memory, ownership)
- CHANGELOG.md skeleton
- library.json, platformio.ini skeleton

Now implement the library.
