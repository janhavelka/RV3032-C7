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
- Deterministic: no unbounded loops/waits; all timeouts use wrap-safe deadlines.
  Core library code never calls a platform delay directly. A bounded operation
  that must honor vendor wait time may call an injected application wait
  callback; its contract requires sleep/yield rather than spin.
- No unbounded retries, allocations, queues, or buffers in steady paths.
- Passive lifecycle: `Status begin(const Config&)` validates/stores callbacks
  and performs zero I/O; `Status probe()` is the explicit presence check;
  `end()` performs zero I/O.
- Preserve the fixed-capacity cooperative `tick()`/`pollEeprom()` and
  `start...Job()`/`pollJob()` architecture. Ordinary multi-transfer chip work
  advances only within the caller's explicit instruction budget. A resource-
  owner integration may pass a budget of one so one owner poll performs at most
  one transport callback.
- Every multi-transfer operation must have fixed callback/check bounds and a
  whole-operation bound. A no-wait operation with a fixed transfer count may
  use the derived bound of count times the configured per-transfer timeout; any
  operation with vendor waits/polling requires wrap-safe phase and operation
  deadlines.
- The dedicated primary-cell ensure operation is the sole synchronous multi-
  callback exception. It may execute its complete bounded sequence in the
  application's I2C owner during startup, using an injected yielding wait
  callback. Do not generalize this exception to ordinary calendar, timer, RAM,
  or generic persistence work.
- Any hardware operation that can block must have a timeout and an observable failure path.
- Bus recovery and transport retry policy belong to the application owner. The
  library returns typed errors and performs only protocol-required readback
  reconciliation; it never resets or recovers a bus.
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
- I2C transactions and complete multi-transfer operations MUST be timeout-
  bounded and report errors clearly.
- The library invokes each transport callback at most once per counted
  instruction and never retries or recovers internally. An injected
  `i2cWrite` callback is exactly one physical write attempt with no retry. An
  injected read-only `i2cWriteRead` callback may implement the application's
  documented bounded recovery plus at most one read retry; this is an explicit
  owner exception and must be included in operation timing tests. No recovery
  path may replay a possibly mutating callback or high-level operation.
- Do not implement chip protocols manually if an existing hardened project library already provides the needed timeout, recovery, and testability behavior.
- Keep chip-level protocol code inside the driver/wrapper. Application policy
  decides whether and when to call the dedicated semantic primary-cell ensure
  operation; the library owns its safe C0 encoding/protocol but never invokes
  it implicitly.
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

- I2C address 0x51; `begin()` is passive and `probe()` explicitly checks device
  presence.
- Burst read time/date; strict BCD encode/decode with range checks.
- Explicit flag read/clear; never clear implicitly. The one deliberate calendar
  operation that clears PORF/VLF must say so in its public name and documentation
  and must expose the RV3032 rule that every Status-register write also clears
  THF/TLF. Audit every Status-register writer for that side effect.
- Existing opt-in generic EEPROM persistence may remain fixed-capacity and
  cooperative through `tick()`/`pollEeprom()`, but its protocol must be vendor-
  correct, durably verified, bounded, mutually exclusive with other chip jobs,
  and free of blind write retry. It is never started implicitly by `begin()` or
  recovery.
- The dedicated primary-cell ensure operation is separate from the generic
  persistence queue: direct persistent read before deciding, zero writes when
  correct, at most one write-one command when wrong, busy/EEF handling, direct
  persistent verification, and bounded cleanup. Never enqueue it or retry its
  wear-limited write command inside one invocation.
- Never write reserved bits; preserve register masks.

---

## Driver Architecture: Passive Cooperative Driver

The driver follows a passive cooperative model with one narrow synchronous
startup operation:

- `begin()` binds validated callbacks with zero transport calls and no chip
  mutation. It never probes, applies PMU policy, starts persistence, or recovers.
- Single-transfer APIs may complete synchronously. Existing bounded multi-
  transfer work remains explicit `start...Job()` plus `pollJob()`, and opt-in
  generic persistence remains `tick()`/`pollEeprom()` driven. No polling method
  performs more callbacks than its caller-supplied instruction budget.
- `probe()` is an explicit diagnostic read.
- The dedicated `ensurePrimaryCellConfiguration()` operation is explicit at the
  library boundary. A product such as TunnelMonitor may deliberately call it
  once during application startup; the library lifecycle never calls it. It
  rejects active/pending job or EEPROM work and is the only API allowed to
  perform multiple transport callbacks before returning.
- Health counters are observational only. They never suppress a caller-
  requested transport operation or create a second bus-policy owner.
- The application owns serialization, admission, bus recovery, transport retry,
  and scheduling.

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
- `begin()` success -> READY (callbacks bound; device presence not yet proved)
- Any I2C failure in READY -> DEGRADED
- Success in DEGRADED/OFFLINE -> READY
- Failures reach `offlineThreshold` -> OFFLINE
- `end()` -> UNINIT

`OFFLINE` is an observational label only. It must not block the next valid
requested operation.

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
- Public `recover()` is only a bounded driver-health re-probe. It never resets
  the application bus, reapplies configuration, starts EEPROM persistence,
  invokes primary-cell ensure, or clears the same-lifecycle ensure-attempt
  latch. Physical bus recovery remains application-owned.

### Health Tracking Rules

- `_updateHealth()` called ONLY inside tracked transport wrappers.
- State transitions guarded by `_initialized` (no DEGRADED/OFFLINE before `begin()` succeeds).
- NOT called for config/param validation errors (INVALID_CONFIG, INVALID_PARAM).
- NOT called for precondition errors (NOT_INITIALIZED).
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
4. Only with explicit user authorization, commit and tag: `Release vX.Y.Z`.
   Otherwise stop after the requested edits and verification with the worktree
   uncommitted and untagged.
---

## Naming Conventions

- Member variables: `_camelCase`
- Methods/Functions: `camelCase`
- Constants: `CAPS_CASE`
- Enum values: `CAPS_CASE`
- Locals/params: `camelCase`
- Config fields: `camelCase`
