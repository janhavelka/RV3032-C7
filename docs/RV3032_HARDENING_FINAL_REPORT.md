# RV3032-C7 Hardening Final Report

Branch: `hardening/rv3032-industry-readiness`

Historical note: this report captured an earlier hardening checkpoint before
the later EEPROM/interrupt and ESP-IDF chunks. The current final readiness
verdict is tracked in `docs/RV3032_INDUSTRY_HARDENING_FINAL_REPORT.md`.

## Summary

This pass moved RV3032-C7 closer to industry-grade Arduino/ESP32 use and removed a core framework dependency. It is still not merge-complete for pure ESP-IDF production claims because this repository has no native ESP-IDF example/component build path.

## Public API / Behavior Changes

- `RV3032` copy and move construction/assignment are now deleted.
- `Config::nowMs` no longer falls back to Arduino `millis()`; if omitted, driver-owned timestamps report `0`.
- `probe()` maps only definite address NACK to `DEVICE_NOT_FOUND`; timeout, data NACK, bus, and generic I2C errors are preserved.
- Low-level direct register block access now rejects the user EEPROM range `0xCB..0xEA`.
- `setTimer()` writes timer value registers while the timer is disabled, then enables the timer last.
- Control2 interrupt bit constants were corrected to match the local register reference: `TIE=4`, `AIE=3`, `EIE=2`.

## Code Changes

- Removed `Arduino.h` from `src/RV3032.cpp`.
- Updated `tools/check_core_timing_guard.py` to enforce zero core Arduino timing calls/includes.
- Added native tests for copy/move prevention, Control2 bit mapping, probe error mapping, timer final state, alarm interrupt bit writes, and direct user EEPROM rejection.
- Updated `AGENTS.md` and `README.md` with framework-neutral core, transport ownership, thread/ISR safety, partial-state, and validation-honesty rules.

## Tests And Checks Run

- `python tools/check_core_timing_guard.py`: pass
- `python tools/check_cli_contract.py`: pass
- `python scripts/generate_version.py check`: pass
- `python -m platformio test -e native`: pass, 34/34 test cases
- `python -m platformio run -e esp32s3dev`: pass
- `python -m platformio run -e esp32s2dev`: pass
- `python -m platformio pkg pack`: pass; generated tarball was removed after validation

## Not Run

- Pure ESP-IDF build: not run because the repository currently has no pure ESP-IDF example/component path.
- Hardware validation: not run.

## Remaining Work

- Add a native ESP-IDF component/example before claiming pure ESP-IDF production readiness.
- Implement STOP/control sequencing for atomic clock/calendar set if required by the hardware validation plan.
- Add explicit user EEPROM APIs using EEADDR/EEDATA/EECMD.
- Add dirty/persistence diagnostics for asynchronous EEPROM failures.
- Validate alarm/timer/event interrupt behavior, power-loss flags, backup switchover, and EEPROM endurance/timing on hardware.

## Readiness Assessment

Closer to industry-grade for Arduino diagnostic/bring-up use. Not ready for production ESP-IDF claims until native IDF build coverage and hardware validation exist.
