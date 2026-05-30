# RV3032 Time and Fault Flag Synchronization Report

Date: 2026-05-30

Branch: `hardening/rv3032-industry-readiness`

## Scope

Prompt 03 focused on RTC-specific time/calendar coherence, STOP-controlled time
setting, explicit fault flag handling, and documentation of ESYN/timestamp
deferrals. EEPROM command APIs, alarm/timer/event configuration semantics, and
ESP-IDF integration remain out of scope for this chunk.

Subagents used:
- `rv3032-datasheet-agent`
- `rtc-semantics-agent`
- `test-fault-agent`
- `integration-review-agent`

## Datasheet Facts Applied

- Clock/calendar registers are decoded from the seconds through year registers
  in one contiguous transaction. The 100ths register is not part of the public
  `DateTime` object in this driver.
- I2C register access blocks clock/calendar counter updates for the addressed
  clock/calendar registers when the transaction stays within the RV3032 timing
  requirement. The driver therefore uses a single burst read for `readTime()`.
- Writing the seconds register resets the subsecond prescalers and 100ths state.
- Control2 bit 0 is STOP. While STOP is set, clock/calendar counters are stopped
  and can be updated as a coherent set.
- EVI Control bit 0 is ESYN. ESYN synchronizes on an EVI event and auto-clears;
  no implicit ESYN behavior was added in this chunk.
- Status register `0x0D` contains THF, TLF, UF, TF, AF, EVF, PORF, and VLF.
  Writing the status register has the documented side effect of clearing THF and
  TLF.
- Temp LSB register `0x0E` contains EEF, EEbusy, CLKF, and BSF in its low bits.
  EEbusy is read-only; EEF, CLKF, and BSF are clearable fault/status flags.

## What Changed

- `readTime()` now decodes into a local `DateTime` object and only updates the
  caller output after the full burst has passed reserved-bit, BCD, range, and
  calendar validation.
- `setTime()` now performs a STOP-controlled sequence: read Control2, set STOP,
  write seconds through year, then clear STOP while preserving unrelated Control2
  bits.
- A clock/calendar uncertainty diagnostic was added for partial time-setting
  failures. It is visible through `SettingsSnapshot::clockCalendarUncertain` and
  `SettingsSnapshot::clockCalendarLastStatus`.
- `StatusFlags` and `ValidityFlags` now expose EEPROM error, EEPROM busy, clock
  flag, and backup switch state from `0x0E` where applicable.
- New explicit fault clear APIs were added for the non-status-register flags:
  `clearFaultFlags()`, `clearEepromErrorFlag()`, and `clearClockFlag()`.
- Command table constants were corrected/extended for Control2 STOP and EVI
  Control ESYN while retaining compatibility aliases for prior names.
- README and Doxygen comments now describe coherent reads, STOP-controlled time
  setting, ESYN deferral, explicit fault clearing, and PORF/VLF trust handling.
- The diagnostic CLI now prints the extended status/validity bits and exposes
  explicit EEF/CLKF clear commands.

## Public API Changes

- No existing function signature was removed or changed.
- `setTime()` behavior is now STOP-controlled. The method may leave
  `clockCalendarUncertain` set if STOP was asserted or a partial write occurred
  and the full sequence did not complete successfully.
- `StatusFlags` gained:
  - `eepromError`
  - `eepromBusy`
  - `clockFlag`
  - `backupSwitched`
- `ValidityFlags` gained:
  - `eepromError`
  - `eepromBusy`
  - `clockFlag`
- `SettingsSnapshot` gained:
  - `clockCalendarUncertain`
  - `clockCalendarLastStatus`
- New APIs:
  - `Status clearFaultFlags(uint8_t mask)`
  - `Status clearEepromErrorFlag()`
  - `Status clearClockFlag()`

## STOP and ESYN Design

`setTime()` uses STOP as the synchronization primitive for normal clock/calendar
updates. It preserves existing Control2 bits, sets only STOP before writing the
time/date registers, and clears STOP after the burst write succeeds. If a
failure happens after STOP may have changed hardware state, the driver records
the uncertainty and attempts a best-effort STOP release without hiding the
original error.

ESYN was intentionally kept explicit and deferred. The driver exposes the EVI
configuration bits but does not automatically arm ESYN during `setTime()` or any
read path. Higher-level event/timestamp/ESYN APIs are deferred to the later
timer/alarm/event chunk where EVI semantics can be handled as one unit.

## Fault Flag Semantics

- `readStatusFlags()` reads both `0x0D` and `0x0E` and reports all currently
  represented status/fault bits without clearing them.
- `readValidity()` treats PORF or VLF as time-invalid and also surfaces BSF, EEF,
  EEbusy, and CLKF.
- `clearStatus(0)` is a no-op. Nonzero status clears are explicit, but any write
  to the status register can clear THF/TLF on hardware as documented.
- `clearFaultFlags()` only clears clearable low bits from `0x0E`; EEbusy is
  read-only and is not cleared by software.
- PORF/VLF clearing remains explicit. Production code should reinitialize or
  verify trusted time/configuration before clearing those flags.

## Tests Added or Updated

- Coherent `readTime()` happy-path burst decode and invalid BCD/range rejection.
- Leap-day acceptance for valid leap years and rejection for non-leap Feb 29.
- Defensive `readTime()` behavior that does not partially update caller output
  on decode failure.
- STOP-controlled `setTime()` sequencing, Control2 bit preservation, and invalid
  input behavior without I2C traffic.
- Partial `setTime()` failure diagnostics, including STOP-release failure and
  successful clearing of the uncertainty diagnostic after a later complete set.
- Full status and validity decoding for `0x0D` and `0x0E` bits.
- Explicit flag clearing behavior for status flags, BSF, EEF, and CLKF.
- Regression coverage that `begin()`, `readTime()`, and `setTime()` do not
  implicitly clear fault flags.

## Checks Run

- `python tools/check_core_timing_guard.py` - passed.
- `python tools/check_cli_contract.py` - passed.
- `python scripts/generate_version.py check` - passed; `Version.h` was up to
  date.
- `python -m platformio test -e native` - passed; 51/51 test cases succeeded.
- `python -m platformio run -e esp32s3dev` - passed.
- `python -m platformio run -e esp32s2dev` - passed.

## Hardware Validation

No physical RV3032-C7 hardware validation was run in this chunk. The current
evidence is native fake-transport coverage plus ESP32-S2/S3 compile coverage.
STOP timing, PORF/VLF recovery flows, EEF/CLKF/BSF clearing, and ESYN/event
behavior still require bench validation in the final hardware validation chunk.

## Deferred to Later Chunks

- EEPROM command APIs, EEPROM dirty state, command sequencing, and EEbusy
  deadline handling.
- Alarm, timer, event input, timestamp, interrupt flag, and ESYN high-level APIs.
- Full dirty-state modeling for alarm/timer/event configuration and EEPROM
  command failures.
- Native ESP-IDF component/example and CI integration coverage.
- Hardware validation matrix and release-readiness report.
