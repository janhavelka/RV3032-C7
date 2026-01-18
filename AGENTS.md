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
  Status.h
  Config.h
  RV3032.h
  Version.h            - Auto-generated (do not edit)
src/                   - Implementation (.cpp)
examples/
  01_basic_bringup_cli/  - Example app
  common/                - Example-only helpers (Log.h, BoardPins.h)
platformio.ini
library.json
README.md
CHANGELOG.md
AGENTS.md
```

Rules:
- Public headers only in `include/RV3032/`.
- No board-specific pins in library code; use `Config` only.
- Examples demonstrate usage and may use `examples/common/BoardPins.h`.
- Keep the layout boring and predictable.

---

## Core Engineering Rules (Mandatory)

- Deterministic: no unbounded loops/waits; all timeouts via deadlines, never `delay()` in library code.
- Non-blocking lifecycle: `Status begin(const Config&)`, `void tick(uint32_t nowMs)`, `void end()`.
- Any I/O that can exceed ~1-2 ms must be split into state machine steps driven by `tick()`.
- No heap allocation in steady state (no `String`, `std::vector`, `new` in normal ops).
- No logging in library code; examples may log.
- No macros for constants; use `static constexpr`. Macros only for conditional compile or logging helpers.

---

## I2C Manager + Transport (Required)

The bus is owned by a manager. The driver never touches `Wire` directly.

- The library MUST accept a transport adapter in `Config` (function pointers or an abstract interface).
- The transport MUST translate errors to `Status` (no leaking `Wire`, `esp_err_t`, etc.).
- The driver MUST NOT call `Wire.setTimeOut()` or configure the bus; manager owns timeouts and recovery.
- The driver MUST be fully non-blocking: public APIs queue I2C work and return `IN_PROGRESS` until `tick()` completes it.
- No blocking mode; remove or ignore any "blocking" config flags.

Example requirement:
- Implement an I2C manager in the example.
- The example must use the manager and never call `Wire` directly.
- Manager responsibilities: per-transaction timeout, bounded retries with backoff, bus recovery, and device health tracking.

---

## RV3032-C7 Specific Requirements

- I2C address is 0x51; check device presence in `begin()`.
- Time reads are burst reads; use strict BCD encode/decode with range checks.
- Compute weekday; handle 12/24h and leap year correctly.
- Read flags explicitly; never clear flags implicitly.
- EEPROM writes are non-blocking and deadline-driven; use busy/error flags and bounded timeouts.
- Never write reserved bits; preserve register masks.
- Any retry policy must be bounded and backoff-based.

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
