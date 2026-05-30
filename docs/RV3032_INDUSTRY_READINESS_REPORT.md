# RV3032-C7 Industry Readiness Audit

Branch: `hardening/rv3032-industry-readiness`

Historical note: this was the baseline audit before later hardening chunks.
Current core-contract, time/flag, EEPROM/interrupt, and ESP-IDF status is tracked
in the chunk reports added after this document, including
`docs/RV3032_CORE_CONTRACTS_REPORT.md` and
`docs/RV3032_ESPIDF_CI_REPORT.md`.

## Executive Summary

The RV3032-C7 library has a useful managed synchronous architecture, injected I2C callbacks, health tracking, native tests, and good date/time validation coverage. It is not yet industry-grade for ESP32-S2/S3 Arduino and ESP-IDF consumers because the core still has an Arduino dependency, the repository does not contain a pure ESP-IDF example/build path, and several RTC-specific hardware semantics need tighter contracts.

Readiness classification: closer to production-grade Arduino diagnostic use than production-grade dual Arduino/ESP-IDF library. Pure ESP-IDF readiness is pending.

## Baseline Checks

- `python tools/check_core_timing_guard.py`: pass
- `python tools/check_cli_contract.py`: pass
- `python scripts/generate_version.py check`: pass
- `python -m platformio test -e native`: pass, 30 test cases
- ESP-IDF build: not run; no pure ESP-IDF example/component was present in this repository layout.
- Hardware validation: not run in this audit.

## Scorecard

| Area | Status | Notes |
| --- | --- | --- |
| Core framework neutrality | Not ready | `src/RV3032.cpp` includes Arduino and falls back to `millis()`. |
| I2C ownership | Mostly ready | Core uses injected transport callbacks and does not own pins/bus. |
| Timing contracts | Partial | EEPROM is deadline-driven, but missing timebase behavior was framework-dependent. |
| Status/error precision | Partial | Probe/recover collapsed distinguishable I2C failures to `DEVICE_NOT_FOUND`. |
| Health/recovery | Mostly ready | Tracked wrappers centralize health; probe is diagnostic-only. |
| Partial hardware state | Partial | EEPROM-backed settings can be left pending/failed without a general dirty diagnostic. |
| Device correctness | Needs fixes | Local register reference indicates Control2 interrupt bit mapping is wrong. |
| Tests | Partial | Native tests are useful; missing ESP-IDF build and some register/partial-write cases. |
| Docs/examples | Partial | Arduino examples are diagnostic; pure ESP-IDF example is absent. |

## Strengths

- Transport is injected through `Config::i2cWrite` and `Config::i2cWriteRead`.
- Public headers avoid direct `Wire` ownership.
- `readTime()` uses a burst read and validates BCD/date ranges.
- Leap year and Unix conversion tests exist.
- EEPROM persistence is tick-driven and bounded by deadlines.
- Health state is centralized through tracked transport wrappers.

## High Findings

1. Core framework neutrality is broken by `<Arduino.h>` and `millis()` fallback in `src/RV3032.cpp`.
2. The local register reference documents Control2 bit 3 as `AIE`, bit 4 as `TIE`, and bit 2 as `EIE`, but `CommandTable.h` maps alarm interrupt enable to bit 2 and timer interrupt enable to bit 3.
3. `setTime()` writes clock/calendar fields directly without STOP/control sequencing called out by local operational notes.
4. `setTimer()` enables the timer before writing timer value registers, which can expose an intermediate timer value.
5. Direct register access accepts the user EEPROM address range even though local docs say user EEPROM is accessed through EEADDR/EEDATA/EECMD.

## Medium Findings

- Probe/recover error mapping hides timeout, bus, and generic I2C failures.
- Copy/move construction is not explicitly disabled for the driver object.
- Validity/fault APIs expose PORF/VLF/BSF but not all local fault bits such as `EEF` and `CLKF`.
- EEPROM endurance documentation should be reconciled before making a numeric production claim.
- CI does not build a pure ESP-IDF example.

## Recommended Remediation Plan

- Remove Arduino from core and make missing `nowMs` behavior explicit.
- Correct Control2 interrupt bit constants and add native tests for exact bit writes.
- Preserve distinguishable I2C probe errors; map only address NACK to `DEVICE_NOT_FOUND`.
- Disable copy/move for `RV3032`.
- Reorder timer programming to write the timer value while disabled and enable last.
- Document remaining STOP-sequencing, EEPROM dirty-state, and pure ESP-IDF gaps honestly.

## Exit Criteria For Industry Grade

- Core builds without Arduino/ESP-IDF framework headers.
- Native tests cover register bit maps, probe error mapping, copy/move prevention, and partial hardware state.
- Arduino S2/S3 builds pass.
- A real pure ESP-IDF example/component builds in CI.
- Hardware validation covers RTC time setting across rollover, flags, alarm/timer interrupts, backup/power-loss behavior, and EEPROM persistence/failure paths.
