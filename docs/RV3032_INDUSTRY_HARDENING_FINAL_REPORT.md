# RV3032-C7 Industry Hardening Final Report

Branch: `hardening/rv3032-industry-readiness`

Date: 2026-05-30

Commit range reviewed: `443b5e4..HEAD` (`main` / `v1.5.0` through the Prompt 06 final report commit)

## Summary Of Chunks

- Prompt 00/01 established the chunked workflow and baseline readiness matrix.
- Prompt 02 tightened core contracts: framework-neutral core, injected transport,
  precise probe/recover errors, deleted copy/move, direct register safety, and
  documentation for thread/ISR/timebase contracts.
- Prompt 03 added coherent RTC time semantics: burst reads, STOP-controlled
  `setTime()`, explicit validity/fault flag APIs, and clock/calendar uncertainty
  diagnostics for partial writes.
- Prompt 04 added RV3032-specific user EEPROM command APIs, bounded busy/EEF
  handling, timer/alarm/event/interrupt semantics, timestamp helpers, and related
  fake-transport tests.
- Prompt 05 added the native ESP-IDF component path, ESP-IDF diagnostic example,
  ESP-IDF example guard, CI job configuration, production bus-manager guidance,
  and ESP-IDF documentation.
- Prompt 06 reviewed the full branch, ran local validation, added the hardware
  validation matrix, updated migration notes, removed stale package artifacts,
  and finalized this readiness report.

## Public API Changes And Migration Notes

- `RV3032` copy and move construction/assignment are deleted. Applications must
  keep a stable driver instance and pass references or pointers.
- `Config::nowMs` no longer has an Arduino `millis()` fallback. If omitted,
  driver-owned timestamps report `0`.
- `probe()` now maps only definite address NACK to `DEVICE_NOT_FOUND`; data NACK,
  timeout, bus, and generic I2C errors remain distinguishable when transport
  callbacks provide them.
- Direct register access rejects the user EEPROM data range `0xCB..0xEA`; use
  offset-based `readUserEeprom*` / `writeUserEeprom*` APIs.
- `setTime()` uses STOP-controlled clock/calendar writes and records uncertainty
  if a partial hardware update may have occurred.
- `setTimer()` writes timer value/frequency while disabled and enables last.
- New public APIs cover user EEPROM, decoded validity/fault flags, explicit flag
  clears, alarm/timer/event interrupt routing, and timestamp read/reset.
- Shared-bus or multi-task applications must serialize access externally. Public
  driver APIs are task-context APIs and are not ISR-safe.

SemVer note: `library.json` still reports `1.5.0` on this branch. Before release,
maintainers should choose a version bump deliberately. Because copy/move deletion
can break code that copied driver objects, a major release is the conservative
SemVer choice unless that usage is classified as unsupported misuse.

## Implemented

- Framework-neutral core under `include/` and `src/`, enforced by
  `tools/check_core_timing_guard.py`.
- Injected I2C transport and optional injected timebase only.
- Manual health/recovery with precise probe/recover status handling.
- STOP-controlled time setting and coherent time read contracts.
- Explicit PORF/VLF/BSF/EEF/EEbusy/CLKF validity/fault handling.
- User EEPROM APIs using `EEADDR`, `EEDATA`, and `EECMD`.
- Alarm, timer, event interrupt, and timestamp helpers with native tests.
- Native ESP-IDF component metadata and a diagnostic ESP-IDF 5.4 example.
- CI configuration for PlatformIO builds, native tests, package validation,
  guard scripts, and ESP-IDF example builds on `main`, `hardening/**`, and PRs
  to `main`.
- Hardware validation matrix and production bus-manager documentation.
- Changelog migration notes under `Unreleased`.

## Intentionally Not Implemented

- High-level ESYN arm/cancel API.
- Password/write-protection APIs for RV3032 EEPROM.
- Production shared-bus ESP-IDF example with multiple devices and recovery loop.
- Internal locking; the driver remains externally serialized by contract.
- Claims of hardware, field, or industry-grade proof without bench records.

## Tests And Checks Run

| Command | Result |
| --- | --- |
| `git diff --stat main...HEAD` | Reviewed; 30 files changed before Prompt 06 final docs |
| `git diff main...HEAD` | Reviewed; no unrelated build outputs from the branch identified |
| `git diff --check main...HEAD` | Initially failed on whitespace in earlier reports; Prompt 06 fixes were applied and final check passed |
| `python tools/check_core_timing_guard.py` | Passed |
| `python tools/check_cli_contract.py` | Passed |
| `python tools/check_idf_example_contract.py` | Passed |
| `python scripts/generate_version.py check` | Passed; `Version.h` and `idf_component.yml` up to date |
| `python -m platformio test -e native` | Passed; 67/67 native tests |
| `python -m platformio run -e esp32s3dev` | Passed |
| `python -m platformio run -e esp32s2dev` | Passed |
| `python -m platformio pkg pack` | Passed; generated `RV3032-C7-1.5.0.tar.gz` |
| Package artifact inspection | Passed after Prompt 06 cleanup; no `.venv`, `.pio`, `build_output.txt`, `build/`, `managed_components/`, `sdkconfig`, `dependencies.lock`, or nested `.tar.gz` entries found |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Not run locally; `idf.py` is not on PATH in this shell |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Not run locally; `idf.py` is not on PATH in this shell |

The generated package tarball was removed after inspection.

## CI Status

CI coverage is configured for guard scripts, native tests, PlatformIO Arduino
builds, PlatformIO package validation, and ESP-IDF 5.4 example builds for
`esp32s3` and `esp32s2`.

No GitHub Actions run was verified in this local audit. Do not state that CI
passed until a real workflow run is observed.

## Hardware Tests Run

No physical RV3032-C7 hardware tests were run in this session.

## Hardware Tests Not Run And Why

No serial-connected ESP32-S2/ESP32-S3 plus RV3032-C7 hardware was available in
this local coding session. The full required bench plan is documented in
`docs/RV3032_HARDWARE_VALIDATION_MATRIX.md`.

Not-run categories include:

- Device scan/probe on real I2C hardware.
- Real RTC rollover, STOP behavior, PORF/VLF power events, backup switchover,
  BSF, and VBACKUP behavior.
- Alarm all-`AE_x=1` behavior, timer boundary values, EVI/EVOW/timestamp reset
  behavior, THF/TLF STATUS-write side effects, and POR startup with `EEbusy`.
- INT electrical behavior with pull-up rail/value and multiple interrupt sources.
- Hardware EEPROM persistence, EEbusy timing, and inducible EEF failures.
- Async configuration EEPROM persistence through `tick()`.
- Alarm, timer, update, and EVI event behavior on wired pins.
- Multi-hour/multi-day soak.

## Remaining Risks

- Native tests are fake-transport tests; they do not prove physical counter
  freezing, clock rollover, INT electrical behavior, backup switchover, or EEPROM
  timing on real silicon.
- The ESP-IDF diagnostic example maps `ESP_FAIL` to generic `I2C_ERROR`; it does
  not prove adapter-level address/data NACK precision for ESP-IDF hardware.
- `idf.py` local builds were not run because the ESP-IDF environment is not on
  PATH here. CI is configured to build them, but a workflow run was not observed.
- Package version remains `1.5.0` until maintainers decide the release version.
- Hardware validation remains mandatory before release and before stronger
  readiness claims.

## Merge Verdict

Merge recommendation: conditionally merge after reviewing this final Prompt 06
commit and running the configured GitHub Actions workflow. The branch is suitable
for merge as a production-oriented hardening branch if maintainers accept that
hardware validation is documented but not yet executed.

## Release Verdict

Release recommendation: do not cut a production release until:

- GitHub Actions has passed on the branch or PR.
- ESP-IDF example builds have passed in CI or a local ESP-IDF shell.
- The SemVer bump is chosen and `library.json` / changelog are updated for the
  release.
- Required-before-release hardware validation in
  `docs/RV3032_HARDWARE_VALIDATION_MATRIX.md` is executed and recorded.

## Allowed Claims

Allowed after the local checks in this report and a clean CI run:

```text
RV3032-C7 library hardened for production-oriented Arduino and ESP-IDF integration, with framework-neutral core, injected I2C transport, explicit RTC/time/flag/EEPROM contracts, native tests, and documented hardware validation matrix.
```

## Prohibited Claims

Do not claim any of the following until proven with recorded hardware and CI
evidence:

- Fully field-proven industry-grade RV3032-C7 RTC library.
- Hardware-validated backup switchover, INT behavior, EEPROM persistence/failure
  handling, or long-soak stability.
- ESP-IDF CI passed, unless a real GitHub Actions run is observed.
- Adapter-level ESP-IDF address-NACK versus data-NACK precision.
- ESYN or password/write-protection support.
