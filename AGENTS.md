# AGENTS.md - RV3032-C7 Production Embedded Guidelines

## Role and Target
You are a professional embedded software engineer building a production-grade RV3032-C7 RTC library.

- Target: ESP32-S2 / ESP32-S3, Arduino framework, PlatformIO.
- Goals: deterministic behavior, long-term stability, clean API contracts, portability, no surprises in the field.
- These rules are binding.

---

## Repository Model (Single Library)

```
include/RV3032/        - Public API headers only (Doxygen)
  CommandTable.h       - a full list from register reference
  Status.h
  Config.h
  RV3032.h
  Version.h            - Auto-generated (do not edit)
src/                   - Implementation (.cpp)
examples/
  00_*/
  01_*/
  common/              - Example-only helpers (Log.h, BoardConfig.h, I2cTransport.h,
                         I2cScanner.h, CommandHandler.h)
platformio.ini
library.json
README.md
CHANGELOG.md
AGENTS.md
```

Rules:
- `examples/common/` is NOT part of the library. It simulates project glue and keeps examples self-contained.
- No board-specific pins/bus in library code; only in `Config`.
- Public headers only in `include/RV3032/`.
- Examples demonstrate usage and may use `examples/common/BoardConfig.h`.
- Keep the layout boring and predictable.
---

## Core Engineering Rules (Mandatory)

- Prefer simplicity, clarity, correctness, robustness, safety, and readability over clever abstractions or speculative flexibility.
- Before coding, inspect whether existing code can be simplified, reused, or deleted.
- Prefer deleting unnecessary code over adding code.
- Keep changes tightly scoped to the user request; preserve dirty user changes and never revert unrelated work.
- Prefer extending existing owners/modules/contracts over creating parallel abstractions.
- Before adding a new service/class/file/interface/abstraction, confirm a concrete current need plus a clear caller or test.
- Do not add placeholder classes, future stubs, empty managers, broad frameworks, plugin systems, registries, generic layers, or speculative extension points unless the current task explicitly requires them.
- Deterministic: no unbounded loops/waits; all timeouts via deadlines, never `delay()` in library code.
- No unbounded retries, allocations, queues, or buffers in steady paths.
- Non-blocking lifecycle: `Status begin(const Config&)`, `void tick(uint32_t nowMs)`, `void end()`.
- Any I/O that can exceed ~1-2 ms must be split into state machine steps driven by `tick()`.
- Any hardware operation that can block must have a timeout and an observable failure path.
- Recovery logic must be bounded, deterministic, and testable.
- Prefer explicit state, explicit ownership, and small local helpers over hidden global state.
- Do not hide hardware failures behind silent retries or fake success.
- No heap allocation in steady state (no `String`, `std::vector`, `new` in normal ops).
- Avoid dynamic allocation in steady embedded paths unless it is already an accepted local pattern and the bound is clear.
- No logging in library code; examples may log.
- No macros for constants; use `static constexpr`. Macros only for conditional compile or logging helpers.

---

## I2C Manager + Transport (Required)

- The I2C bus must have one clear owner.
- The library MUST NOT own I2C. It never touches `Wire` directly.
- Device drivers MUST NOT directly own or reconfigure a shared bus unless this repository's architecture explicitly says so.
- `Config` MUST accept a transport adapter (function pointers or abstract interface).
- Transport errors MUST map to `Status` (no leaking `Wire`, `esp_err_t`, etc.).
- The library MUST NOT configure bus timeouts or pins.
- I2C transactions MUST be timeout-bounded and report errors clearly.
- Do not implement chip protocols manually if an existing hardened project library already provides the needed timeout, recovery, and testability behavior.
- Keep chip-level protocol code inside the driver/wrapper. Keep application policy outside the chip driver.
- Do not add fake devices, simulated buses, or test doubles to production paths.

---

## Status / Error Handling (Mandatory)

All fallible APIs return `Status`:

```
struct Status {
  Err code;
  int32_t detail;
  const char* msg;  // static string only
};
```

- Silent failure is unacceptable.
- No exceptions.

---

## RV3032-C7 Driver Requirements

- I2C address 0x51; check device presence in `begin()`.
- Burst read time/date; strict BCD encode/decode with range checks.
- Explicit flag read/clear; never clear implicitly.
- EEPROM writes are deadline-driven; bounded retries only.
- Never write reserved bits; preserve register masks.

---

## Driver Architecture: Managed Synchronous Driver

The driver follows a **managed synchronous** model with health tracking:

- All public I2C operations are **blocking** (no async beyond EEPROM persistence).
- `tick()` only advances the EEPROM state machine; no automatic recovery.
- Health is tracked via **tracked transport wrappers** -- public API never calls `_updateHealth()` directly.
- Recovery is **manual** via `recover()` - the application controls retry strategy.

### DriverState (4 states only)

```cpp
enum class DriverState : uint8_t {
  UNINIT,    // begin() not called or end() called
  READY,     // Operational, consecutiveFailures == 0
  DEGRADED,  // 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    // consecutiveFailures >= offlineThreshold
};
```

State transitions:
- `begin()` success -> READY
- Any I2C failure in READY -> DEGRADED
- Success in DEGRADED/OFFLINE -> READY
- Failures reach `offlineThreshold` -> OFFLINE
- `end()` -> UNINIT

### Transport Wrapper Architecture

All I2C goes through layered wrappers:

```
Public API (readTime, setTime, etc.)
    ↓
Register helpers (readRegs, writeRegs)
    ↓
TRACKED wrappers (_i2cWriteReadTracked, _i2cWriteTracked)
    ↓  <- _updateHealth() called here ONLY
RAW wrappers (_i2cWriteReadRaw, _i2cWriteRaw)
    ↓
Transport callbacks (Config::i2cWrite, i2cWriteRead)
```

**Rules:**
- Public API methods NEVER call `_updateHealth()` directly
- `readRegs()`/`writeRegs()` use TRACKED wrappers -> health updated automatically
- `probe()` uses RAW wrappers -> no health tracking (diagnostic only)
- `recover()` tracks probe failures (driver is initialized, so failures count)

### Health Tracking Rules

- `_updateHealth()` called ONLY inside tracked transport wrappers.
- State transitions guarded by `_initialized` (no DEGRADED/OFFLINE before `begin()` succeeds).
- NOT called for config/param validation errors (INVALID_CONFIG, INVALID_PARAM).
- NOT called for precondition errors (NOT_INITIALIZED).
- `Err::IN_PROGRESS` treated as success (EEPROM queuing is not a failure).
- `probe()` uses raw I2C and does NOT update health (diagnostic only).

### Health Tracking Fields

- `_lastOkMs` - timestamp of last successful I2C operation
- `_lastErrorMs` - timestamp of last failed I2C operation
- `_lastError` - most recent error Status
- `_consecutiveFailures` - failures since last success (resets on success)
- `_totalFailures` / `_totalSuccess` - lifetime counters (wrap at max)

---

## Versioning and Releases

Single source of truth: `library.json`. `Version.h` is auto-generated and must never be edited.

SemVer:
- MAJOR: breaking API/Config/enum changes.
- MINOR: new backward-compatible features or error codes (append only).
- PATCH: bug fixes, refactors, docs.

Release steps:
1. Update `library.json`.
2. Update `CHANGELOG.md` (Added/Changed/Fixed/Removed).
3. Update `README.md` if API or examples changed.
4. Commit and tag: `Release vX.Y.Z`.
---

## Naming Conventions

- Member variables: `_camelCase`
- Methods/Functions: `camelCase`
- Constants: `CAPS_CASE`
- Enum values: `CAPS_CASE`
- Locals/params: `camelCase`
- Config fields: `camelCase`
