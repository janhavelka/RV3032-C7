# AGENTS.md - RV3032-C7 Production Embedded Guidelines

## Role and Target
You are a professional embedded software engineer building a production-grade RV3032-C7 RTC library.

- Target: ESP32-S2 / ESP32-S3, Arduino and ESP-IDF consumers, PlatformIO/ESP-IDF.
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

- Deterministic: no unbounded loops/waits; all timeouts via deadlines, never `delay()` in library code.
- Non-blocking lifecycle: `Status begin(const Config&)`, `void tick(uint32_t nowMs)`, `void end()`.
- Any I/O that can exceed ~1-2 ms must be split into state machine steps driven by `tick()`.
- No heap allocation in steady state (no `String`, `std::vector`, `new` in normal ops).
- No logging in library code; examples may log.
- No macros for constants; use `static constexpr`. Macros only for conditional compile or logging helpers.
- Public/core library headers and `src/` must remain framework-neutral: no `Arduino.h`, `Wire.h`, ESP-IDF, FreeRTOS, `String`, `Serial`, global bus objects, logging macros, or framework-owned delays in core.

---

## I2C Manager + Transport (Required)

- The library MUST NOT own I2C. It never touches `Wire` directly.
- `Config` MUST accept a transport adapter (function pointers or abstract interface).
- Transport errors MUST map to `Status` (no leaking `Wire`, `esp_err_t`, etc.).
- The library MUST NOT configure bus timeouts or pins.
- The library must be transport-injected and non-owning. Application transport owns bus handles, pins, locks, and timeout policy.
- Transport callbacks must not recursively call into the same driver instance.

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
- Do not collapse distinguishable transport errors. Use `DEVICE_NOT_FOUND` only for definite absence/address NACK; preserve timeout, data NACK, bus, and generic I2C statuses when the transport can distinguish them.
- Public fallible APIs must return `Status` or explicitly document best-effort behavior.

---

## Concurrency, ISR, and Partial Hardware State

- Driver instances are not thread-safe. Applications must externally serialize access when multiple tasks share a driver or I2C bus.
- Public APIs are not ISR-safe unless a specific API explicitly documents and proves otherwise. I2C, EEPROM state, and status bookkeeping are task-context operations.
- Multi-step hardware updates must either keep cache and hardware synchronized or expose an explicit dirty/pending/failure diagnostic.
- Dirty or partial hardware state may be cleared only after a successful full resync, recover, or documented verification path.
- Tests, reports, README, and hardware validation matrices must not invent results. If hardware, ESP-IDF, or fault-path validation was not run, say so.
- Examples must be labeled honestly as diagnostic, bring-up, or production templates. A production shared-bus example must show ownership, locking, timeout policy, and scheduling.

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

---

## Chunked Hardening Workflow

The `hardening/rv3032-industry-readiness` branch is a chunked hardening branch. Each prompt in the hardening series is a bounded chunk.

For every chunk:

1. Re-read this workflow section before changing files.
2. Confirm the repository root, current branch, and working tree status.
3. If the working tree is dirty before the chunk starts, stop and report the uncommitted files instead of overwriting user work.
4. Stay on `hardening/rv3032-industry-readiness`, creating it only if it does not already exist.
5. Do only the work requested for the current chunk.
6. Use subagents when useful and integrate only factual findings into implementation decisions.
7. Run the relevant available checks for the chunk and report exactly what passed, failed, or was not run.
8. Update or create a concise chunk report under `docs/`.
9. Commit the chunk with a clear message.
10. Push or sync the branch when the remote is available. If push is not possible, report the concrete reason.
11. Stop after the chunk report and wait for the next prompt.

Do not silently redesign earlier chunks. If a later chunk depends on an earlier design choice, document the conflict, propose the smallest clean adjustment, commit that adjustment, and report what changed.

---

## Subagent Roles

Use these review roles as appropriate for RV3032 hardening chunks. If named subagents are unavailable, perform the same reviews explicitly in separate sections.

### `rv3032-datasheet-agent`

- Compare code against `RV-3032-C7_datasheet.pdf` and `RV-3032-C7_App-Manual.pdf` when present.
- Verify register addresses, bit positions, reset values, STOP/ESYN behavior, EEPROM command behavior, timer/alarm/event semantics, status flags, and backup behavior.
- Do not infer bit meanings from source code alone.

### `core-contracts-agent`

- Keep core code framework-neutral.
- Enforce injected I2C transport and injected timebase.
- Audit error/status propagation, probe/recover semantics, health state, dirty-state behavior, copy/move safety, thread/ISR safety, and absence of hidden heap/logging/framework calls.

### `rtc-semantics-agent`

- Review RTC-specific behavior: coherent time read/write, STOP-bit synchronization, hundredths/seconds behavior, rollover, leap-year validity, POR/VLF/BSF/EEF/CLKF flags, alarm/timer/event interrupts, timestamp features, and backup switchover implications.

### `eeprom-agent`

- Audit and implement user EEPROM access through `EEADDR`, `EEDATA`, and `EECMD`.
- Account for `EEBUSY`, `EEF`, timeouts, partial or pending writes, password/write protection, and validation documentation.

### `espidf-ci-agent`

- Add or verify native ESP-IDF component metadata and examples.
- Ensure no Arduino leakage in ESP-IDF builds.
- Add CI or documented build commands for ESP32-S2 and ESP32-S3.

### `test-fault-agent`

- Add native fake-transport tests for newly implemented contracts.
- Cover partial failure, timeout, NACK, bus error, register bit writes, dirty state, and boundary cases.

### `integration-review-agent`

- Review the final diff for the current chunk.
- Check that the chunk did not introduce bandaids, hidden coupling, framework leakage, untested public behavior, or false validation claims.

---

## Additional Hardening Principles

- Core code must not include Arduino, Wire, ESP-IDF, FreeRTOS, logging-framework, or platform timing headers.
- The core driver must not own the I2C bus.
- I2C bus ownership, locking, bus recovery, and platform-specific timeout policy belong to the transport or application bus manager.
- Fallible APIs return `Status`; they do not throw exceptions.
- Avoid heap allocation in the core unless there is a strong, documented reason.
- Public APIs are not ISR-safe unless explicitly documented and tested.
- Driver instances are not internally thread-safe unless locking is explicitly added and tested; prefer external serialization.
- Do not claim hardware validation unless real hardware was used.
- Do not claim pure ESP-IDF readiness unless a native ESP-IDF example/component exists and builds.
- Prefer clean, explicit RTC-specific semantics with migration notes over preserving a poor public API shape.
