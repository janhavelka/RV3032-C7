# AGENTS.md - Production Embedded Engineering Guidelines (Library Template)

## Role
You are a professional embedded software engineer building **production-grade reusable libraries** for ESP32 systems.

**Primary goals:**
- Robustness and stability
- Deterministic, predictable behavior
- Portability across projects and boards
- Clean API contracts and long-term maintainability

**Target:** ESP32-S2 / ESP32-S3, Arduino framework, PlatformIO.

**These rules are binding.**

---

## Repository Model (Single Library Template)

This repository is a SINGLE reusable library template designed to scale across multiple embedded projects.

### Folder Structure (Mandatory)

```
include/<libname>/   - Public API headers ONLY (Doxygen documented)
  ├── Status.h       - Error types
  ├── Config.h       - Configuration struct
  └── <Lib>.h        - Main library class
src/                 - Implementation (.cpp files)
examples/
  ├── 00_name/       - Example applications
  ├── 01_name/
  └── common/        - Example-only helpers (Log.h, BoardPins.h)
platformio.ini       - Build environments (uses build_src_filter)
library.json         - PlatformIO metadata
README.md            - Full documentation
CHANGELOG.md         - Keep a Changelog format
AGENTS.md            - This file
```

**Rules:**
- Public headers go in `include/<libname>/` - these define the API contract
- Board-specific values (pins, etc.) NEVER in library code - only in `Config`
- Examples demonstrate usage - they may use `examples/common/BoardPins.h`
- Keep structure boring and predictable - no clever layouts

---

## Core Architecture Principles (Non-Negotiable)

### 1) Deterministic Behavior Over Convenience
- Predictable execution time
- No unbounded loops or waits
- All timeouts implemented via deadline checking (**not** `delay()`)
- State machines preferred over “clever” event-driven code

### 2) Non-Blocking by Default

All libraries MUST expose:

```cpp
Status begin(const Config& config);  // Initialize
void tick(uint32_t nowMs);           // Cooperative update (non-blocking)
void end();                          // Cleanup
```

- `tick()` returns immediately after bounded work
- Long operations split into state machine steps
- Example: 120-second timeout → check `nowMs >= deadlineMs` each tick

> **Rule:** any I/O operation that could exceed ~1–2 ms must be chunked and progressed across `tick()` calls.

### 3) Explicit Configuration (No Hidden Globals)
- Hardware resources passed via `Config`
- No hardcoded pins or interfaces in library code
- Libraries are board-agnostic by design
# Config Struct Design
```cpp
struct Config {
  // Hardware
  int pin_tx = -1;           // -1 = disabled/not used
  int pin_rx = -1;
  uint32_t baud = 115200;

  // Behavior
  uint32_t timeout_ms = 5000;
  bool enable_feature = false;

  // Optional: task mode
  bool use_task = false;
  uint16_t task_stack = 4096;
};
```

**Rules:**
- All pins default to -1 (disabled)
- All timeouts in milliseconds (uint32_t)
- Boolean flags for optional features
- Validate in `begin()`, return `INVALID_CONFIG` on error
- Document valid ranges in Doxygen

- Don’t use macros for constants. Use static constexpr instead.
- Macros are OK for conditional compilation (#ifdef) and logging helpers (LOGD(...)).

### 4) No Repeated Heap Allocations in Steady State
- Allocate resources in `begin()` if needed
- **Zero** allocations in `tick()` and normal operation (no `String`, no `std::vector`, no `new`)
- Use fixed-size buffers, ring buffers, or user-supplied buffers

### 5) Boring, Predictable Code
- Prefer verbose over clever
- Explicit state machines over callback chains
- Simple control flow over complex abstractions
- If uncertain, choose the simplest deterministic solution

---

## Shared-Bus / Transport Abstraction (Mandatory)

For libraries that talk to a shared bus (I2C/SPI/UART):

- The library MUST NOT own the bus.
- The library MUST accept a transport adapter via `Config` (function pointers or an abstract interface).
- The library MUST NOT call `delay()` to “wait for the bus”.
- The library MUST translate transport errors into `Status` (no leaking `Wire`, `esp_err_t`, etc.).

### I2C Transaction Rules (Driver Quality)
- Bounded work per `tick()` (byte budget if needed).
- Explicit timeouts via deadlines (software) plus the platform’s hardware timeout if available.
- Retries are allowed but MUST be bounded and use backoff (e.g., 1ms, 2ms, 4ms capped).
- Never assume I2C writes are atomic; handle partial progress in a state machine.
- Always support “bus busy” / “NACK” failures as normal operational errors (not asserts).

---

## Error Handling

### Status/Err Type (Mandatory)
All library APIs must return `Status` for fallible operations:

```cpp
struct Status {
  Err code;           // OK, INVALID_CONFIG, TIMEOUT, I2C_NACK, ...
  int32_t detail;     // Vendor/third-party error code (if any)
  const char* msg;    // STATIC STRING ONLY (never heap-allocated)
};
```

Rules:
- Silent failure is unacceptable.
- Errors are checkable: `if (!st.ok()) { /* handle */ }`
- Library code: no logging. Examples may log.

---

## Register-Mapped Sensor Driver Checklist (Generic)

### Initialization
- Verify bus address and **chip ID** (if available) early in `begin()`.
- Support an explicit **soft reset** sequence if the device has one.
- After reset, wait using deadlines (no delay loops) and re-read ID/calibration.

### Calibration / trimming
- Read calibration registers once at init.
- Store calibration in a fixed struct; validate ranges if datasheet allows.
- Expose a way to re-read calibration (optional) without reboot.

### Configuration rules
- If a datasheet requires “only writable in sleep/standby”, enforce it:
  - switch to standby
  - write config
  - restore previous mode
- When multiple registers have ordering constraints, wrap them in a single “applyConfig()” step.

### Data readout
- Prefer **burst reads** for multi-byte data to avoid incoherent samples.
- If status bits exist, use them to avoid reading mid-conversion.

### Timing model
- Derive conversion time from settings (oversampling/filter/etc.).
- Provide request→wait→read workflows for single-shot measurements.
- All waits must be deadline-based and driven by `tick(nowMs)`.

### Error policy
### Status/Err Type (Mandatory)
- Library APIs return `Status` struct:
  ```cpp
  struct Status {
    Err code;           // Category (OK, INVALID_CONFIG, TIMEOUT, ...)
    int32_t detail;     // Vendor/third-party error code
    const char* msg;    // STATIC STRING ONLY (never heap-allocated)
  };
  ```
- Never auto-retry forever. Bounded retries + backoff only.

---

## RTC / Timekeeping Driver Checklist (Generic)

### Core timekeeping
- Treat time/date registers as a coherent snapshot:
  - Prefer burst-read of the full time block.
  - If the device supports a “freeze/latch” bit, use it.
- If the device uses **BCD**, implement strict encode/decode with range checks.
- Handle 12/24h mode, weekday encoding, leap-year behavior.

### Alarms / timers / interrupts
- Provide explicit structs for Alarm/Timer configs.
- Expose `readFlags()` and `clearFlags(mask)`; never clear flags implicitly.
- Avoid missed events: read flags first, clear only what you handled.

### Oscillator health
- Expose oscillator status / clock-integrity bits.
- Provide a “clock valid” check in `begin()`.

### Calibration / offset / aging
- Wrap calibration in explicit getters/setters and validate step sizes.
- Never write reserved ranges.

### EEPROM / NVM backed configuration
- Enforce required unlock/password/commit/busy sequences.
- Commit must be non-blocking (deadline-based).
- Rate limit NVM writes; document endurance assumptions.

### Backup domain / power switching
- Default any charger/trickle features OFF; require explicit opt-in.
- Validate thresholds and document behavior when VDD drops.

---

## Versioning and Releases
Follow Semantic Versioning (MAJOR.MINOR.PATCH) and Keep a Changelog.

Single source of truth for version: `library.json`.
### When to Update Version

Follow [Semantic Versioning](https://semver.org/) (MAJOR.MINOR.PATCH):

- **MAJOR** (1.0.0 → 2.0.0): Breaking API changes
  - Changed function signatures
  - Removed public methods
  - Changed Config struct fields (name or type)
  - Changed enum values or error codes

- **MINOR** (1.0.0 → 1.1.0): New features, backward compatible
  - New public methods
  - New Config fields with defaults
  - New error codes (append only)
  - New optional features

- **PATCH** (1.0.0 → 1.0.1): Bug fixes, no API changes
  - Fixed bugs
  - Performance improvements
  - Documentation updates
  - Internal refactoring

### Release Checklist (MANDATORY)

**Before ANY version change, complete ALL steps:**

#### 1. Update [library.json](library.json)
```json
{
  "version": "X.Y.Z"  // ← Change this first
}
```

#### 2. Update [CHANGELOG.md](CHANGELOG.md)
Add new version section at the top:
```markdown
## [X.Y.Z] - YYYY-MM-DD

### Added
- List new features

### Changed
- List API changes (breaking or non-breaking)

### Fixed
- List bug fixes

### Removed
- List removed features (breaking)
```

**Ask:** "What changed in this release? List all Added/Changed/Fixed/Removed items."

#### 3. Update [README.md](README.md) (if needed)
Check if any of these sections need updates:
- **API table** - new methods? changed signatures?
- **Config struct** - new fields? changed defaults?
- **Examples** - does example code reflect new API?
- **Error codes** - new Err enum values?
- **Pin mappings** - changed default pins?

**Ask:** "Do any README sections need updates based on the changes?"

#### 4. Commit and Tag
```bash
git add library.json CHANGELOG.md README.md
git commit -m "Release vX.Y.Z"
git tag vX.Y.Z
git push origin main --tags
```

### Version.h Auto-Generation

**Single source of truth:** [library.json](library.json)

**Automatic generation:** [scripts/generate_version.py](scripts/generate_version.py) creates `include/YourLibrary/Version.h` before each build.

**Generated constants:**
- `VERSION_MAJOR`, `VERSION_MINOR`, `VERSION_PATCH` (uint16_t)
- `VERSION` (string) - e.g., "1.2.3"
- `VERSION_CODE` (uint32_t) - encoded for comparison (e.g., 10203)
- `BUILD_DATE`, `BUILD_TIME`, `BUILD_TIMESTAMP` (strings)
- `GIT_COMMIT` (string) - short hash (e.g., "a1b2c3d")
- `GIT_STATUS` (string) - "clean" or "dirty"
- `VERSION_FULL` (string) - complete version with build info

**Never edit Version.h manually** - it's regenerated on every build.

---

## Naming Conventions (Mandatory)

Arduino/PlatformIO/ESP-IDF style:

| Item                | Convention   | Example                   |
| ------------------- | ------------ | ------------------------- |
| Member variables    | `_camelCase` | `_config`, `_initialized` |
| Methods/Functions   | `camelCase`  | `isReady()`, `getData()`  |
| Constants           | `CAPS_CASE`  | `LED_PIN`, `MAX_RETRIES`  |
| Enum values         | `CAPS_CASE`  | `OK`, `TIMEOUT`           |
| Local vars/params   | `camelCase`  | `startTime`, `timeoutMs`  |
| Config fields       | `camelCase`  | `ledPin`, `timeoutMs`     |