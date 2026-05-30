# RV3032 Core Contracts Report

Date: 2026-05-30

Branch: `hardening/rv3032-industry-readiness`

## Scope

Prompt 02 focused on core driver contracts only: framework neutrality, injected
transport ownership, probe/recover error precision, copy/move prevention,
low-level register safety, and documentation of known dirty-state deferrals.

Subagents used:
- `core-contracts-agent`
- `test-fault-agent`
- `integration-review-agent`

`rv3032-datasheet-agent` was not needed because the only register-boundary fact
used here was already present in the local command table and register reference:
user EEPROM data range `0xCB..0xEA`.

## What Changed

- Strengthened `tools/check_core_timing_guard.py` from a narrow timing guard into
  a core timing/framework guard for `include/` and `src/`.
- Removed framework-specific tokens from public core comments and Doxygen
  examples.
- Documented the core framework-neutral contract, injected transport ownership,
  timebase behavior, thread/ISR safety, probe/recover error mapping, and direct
  register warnings in README/Doxygen/docs.
- Changed `recover()` to return tracked transport errors directly instead of
  remapping address NACK after health tracking. This keeps `recover()` return
  status and `lastError()` consistent.
- Expanded native fake-transport tests for probe/recover error preservation and
  direct register rejection of the user EEPROM data range.
- Verified copy/move prevention remains present and statically tested.

## Public API Changes

- No signature changes.
- Behavioral clarification/change: `probe()` still maps definite address NACK to
  `DEVICE_NOT_FOUND` and preserves non-address transport errors. `recover()` now
  preserves tracked transport errors, including `I2C_NACK_ADDR`, so recovery
  diagnostics are not split between returned status and `lastError()`.
- Direct low-level register APIs remain available as diagnostic APIs, but docs now
  explicitly warn that password, EEADDR, EEDATA, EECMD, protected control, alarm,
  timer, and clock/calendar writes are dangerous.

## Tests Added or Updated

- Added probe coverage for `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`, and
  `I2C_ERROR`, asserting no health tracking.
- Added recover coverage for `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`,
  `I2C_BUS`, and `I2C_ERROR`, asserting returned status, `lastError()`, and
  health counters agree.
- Updated existing recover/offline tests for the preserved `I2C_NACK_ADDR`
  behavior.
- Expanded direct register tests to reject single and block read/write attempts
  into user EEPROM `0xCB..0xEA`, including a crossing block from `0xCA`.
- Existing static assertions continue to verify that `RV3032` is not copyable or
  movable.

## Checks Run

- `python tools/check_core_timing_guard.py` - passed.
- `python tools/check_cli_contract.py` - passed.
- `python scripts/generate_version.py check` - passed; `Version.h` was up to
  date.
- `python -m platformio test -e native` - passed; 36/36 test cases succeeded.
- `python -m platformio run -e esp32s3dev` - passed.
- `python -m platformio run -e esp32s2dev` - passed.

## Deferred to Later Chunks

- STOP/ESYN-controlled coherent time setting and related clock/calendar
  dirty-state diagnostics.
- Full fault/validity flag semantics for PORF, VLF, BSF, EEF, and timestamp
  status flags.
- Dedicated user EEPROM APIs using EEADDR/EEDATA/EECMD command sequencing.
- Full dirty-state model for Control register read-modify-write, timer
  configuration, alarm configuration, and EEPROM command failures.
- ESP-IDF native component/example/CI coverage.
- Hardware validation matrix and release-readiness report.

