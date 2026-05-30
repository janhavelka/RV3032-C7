# RV3032-C7 Industry-Grade Implementation Matrix

Branch: `hardening/rv3032-industry-readiness`  
Prompt: 01 - Deep baseline audit and implementation matrix  
Date: 2026-05-30

## Executive Summary

The repository is closer to a robust Arduino/PlatformIO RTC library than a complete industry-grade dual Arduino/ESP-IDF component. Several items from the older readiness audit are already fixed: core implementation no longer includes Arduino, the transport is injected, copy/move is disabled, probe error mapping preserves distinguishable errors, Control2 AIE/TIE/EIE was partly corrected, native tests exist, and PlatformIO Arduino S2/S3 CI exists.

The main remaining risks are RTC-specific rather than generic driver shape. The command table still has stale or wrong control bit names for STOP, ESYN, Control 3, and temperature/clock interrupt control. `setTime()` does not use the STOP-bit sequence from the application manual. Public fault APIs do not expose EEF/CLKF. EEPROM sequencing is incomplete against the manual's busy, EEF, backup-switchover, and user EEPROM requirements. Alarm/timer/interrupt semantics need tighter API contracts and tests. Pure ESP-IDF component/example support is absent.

No functional driver changes were made in this chunk.

## Subagent Audit Summary

- `rv3032-datasheet-agent`: spawned. Reported stale/wrong register bit names, EEPROM command naming and sequencing gaps, default backup-mode behavior, timer zero semantics, alarm all-disabled hazard, temperature coherence concern, and CLKOUT mode limitations.
- `core-contracts-agent`: spawned. Reported framework-neutral compiled core, stale Arduino examples in public Doxygen, injected I2C model, recover/lastError mismatch, direct register write policy risks, stale IDF docs, and narrow timing guard coverage.
- `rtc-semantics-agent`: spawned. Reported coherent burst `readTime()`, non-STOP `setTime()`, missing STOP/ESYN constants, status-clear hazards, missing fault flags, missing interrupt APIs, and stale README timestamp statement.
- `eeprom-agent`: spawned. Reported no public user EEPROM API, `EEPROM_CMD_READ` unused, partial dirty diagnostics, EEbusy/EEF private-only handling, user EEPROM doc mismatch, and missing password/write-protection APIs.
- `espidf-ci-agent`: spawned. Reported no `CMakeLists.txt`, `idf_component.yml`, pure ESP-IDF example, or ESP-IDF CI; PlatformIO builds are Arduino-only.
- `test-fault-agent`: spawned. Reported shallow fake transport, missing bus/error/failure schedule tests, missing EEPROM state-machine fault tests, and partial alarm/timer/event coverage.
- `integration-review-agent`: spawned for final diff review after the first six reviews completed. Confirmed the diff is documentation-only, contains the required sections and 28 matrix areas, and makes no false hardware or ESP-IDF validation claims.

## Already Fixed Since The Previous Audit

- Core implementation includes only project and C/C++ headers in `src/RV3032.cpp:6-11`; no `Arduino.h` fallback remains.
- `Config::nowMs` is injected and optional; `_nowMs()` returns injected time or `0` in `src/RV3032.cpp:404-409`.
- I2C transport is injected through callbacks in `include/RV3032/Config.h:27-34` and passed through raw wrappers in `src/RV3032.cpp:294-313`.
- Copy/move is disabled in `include/RV3032/RV3032.h:260-263` and tested in `test/test_native/test_datetime.cpp:129-137`.
- `probe()` maps only address NACK to `DEVICE_NOT_FOUND` and otherwise preserves transport status in `src/RV3032.cpp:50-55` and `src/RV3032.cpp:246-261`.
- Direct register block access rejects undocumented gaps and user EEPROM direct range in `src/RV3032.cpp:69-80`, with coverage at `test/test_native/test_datetime.cpp:846-884`.
- Native tests cover 34 cases at the current baseline, including health state, probe mapping, Control2 AIE/TIE/EIE constants, timer high-nibble preservation, timestamp decoding, user RAM, and direct-register boundaries.
- PlatformIO CI builds `esp32s3dev` and `esp32s2dev` Arduino environments in `.github/workflows/ci.yml:14-42`, and native tests in `.github/workflows/ci.yml:44-72`.

## What Remains Risky

- `include/RV3032/CommandTable.h:340-371` still contains stale/wrong bit names relative to the application manual: STOP is Control2 bit 0, ESYN is EVI Control bit 0, Control3 is active, and current names include obsolete `OUT_A`/`OUT_B` and temperature mask names.
- `setTime()` writes seconds through year directly in `src/RV3032.cpp:503-530`; it does not expose or use the manual STOP-bit sequence described in `docs/pdf-extracted-md/RV-3032-C7_App-Manual.md:6401-6449`.
- `clearStatus()` read-modify-writes the status register in `src/RV3032.cpp:1199-1211`; any status write clears THF/TLF per `include/RV3032/RV3032.h:727` and the app manual.
- `StatusFlags` and `ValidityFlags` expose PORF/VLF/BSF but not EEF or CLKF in `include/RV3032/RV3032.h:124-143`; `readEepromFlags()` is private in `src/RV3032.cpp:1571-1580`.
- EEPROM persistence writes RAM mirror first in `src/RV3032.cpp:1363-1374` and then queues EEPROM work; failure diagnostics do not expose the failed address/value.
- EEPROM state machine does not clear EEF before write or temporarily disable backup switchover, both called out by the local app manual extracts.
- `setAlarmMatch(false,false,false)` can create the manual's every-minute alarm encoding rather than a clear "disabled" semantic.
- `setTimer()` accepts `ticks == 0` in `src/RV3032.cpp:718-724`, while the manual describes 1..4095 as useful countdown values and 0 as not starting countdown.
- `readTemperatureC()` reads a single two-byte sample in `src/RV3032.cpp:992-1013`; the manual says temperature registers are not shadowed and should be read with timing care or repeated comparison.
- No pure ESP-IDF component metadata, example, or CI exists; `library.json:34-35` advertises Arduino only.
- `docs/IDF_PORT.md` and `docs/RV3032_INDUSTRY_READINESS_REPORT.md` contain stale statements that conflict with the current code and final report.

## Implementation Matrix

| # | Area | Current status | Target status | Evidence and next action |
| --- | --- | --- | --- | --- |
| 1 | Core framework neutrality | Mostly implemented, but public Doxygen example still uses Arduino idioms. | Core source and public docs are framework-neutral; Arduino usage only in examples/adapters. | `src/RV3032.cpp:6-11`; public docs in `include/RV3032/RV3032.h:33-52` need cleanup. |
| 2 | Injected I2C ownership model | Implemented in core; examples own Wire. | Keep non-owning callbacks; document locking/timeout ownership for Arduino and ESP-IDF transports. | `include/RV3032/Config.h:27-56`, `src/RV3032.cpp:294-313`, `examples/common/I2cTransport.h:21-35`. |
| 3 | Monotonic timebase contract | Partially implemented. `nowMs` is optional; diagnostics report 0 without it. | Explicit contract for required monotonic clock when EEPROM, health timestamps, or diagnostics are used. | `include/RV3032/Config.h:58-64`, `src/RV3032.cpp:404-409`, README says inject for EEPROM/health at `README.md:435`. |
| 4 | Status/error precision and `probe()` mapping | Mostly implemented. `probe()` preserves transport status; `recover()` return vs `lastError()` can differ after address NACK. | Returned status and health diagnostic status should be intentionally consistent or documented. | `src/RV3032.cpp:50-55`, `src/RV3032.cpp:246-289`, `src/RV3032.cpp:373-374`. |
| 5 | Health/recover/offline semantics | Mostly implemented. Offline latch blocks normal I2C and recover is manual. | Preserve manual recovery model; add tests for mapped `lastError()` and EEPROM tick behavior while offline. | `src/RV3032.cpp:316-339`, `src/RV3032.cpp:350-402`, tests at `test/test_native/test_datetime.cpp:394-476`. |
| 6 | Copy/move prevention | Implemented. | Keep deleted copy/move and compile-time tests. | `include/RV3032/RV3032.h:260-263`, `test/test_native/test_datetime.cpp:129-137`. |
| 7 | Direct register access boundaries | Partial. Address windows are guarded, but docs are stale and access-mode/reserved-bit rules are not enforced. | Reject unsafe direct writes or provide documented expert-only API with masks/access-mode checks. | `src/RV3032.cpp:69-80`, `src/RV3032.cpp:1325-1347`, stale docs at `include/RV3032/RV3032.h:803-816`. |
| 8 | `setTime()` coherent clock/calendar write semantics | Partial. Writes all clock/calendar fields in one burst. | STOP-controlled set-time API or documented mode; avoid accidental sync side effects. | `src/RV3032.cpp:503-530`, STOP manual extract `docs/pdf-extracted-md/RV-3032-C7_App-Manual.md:6401-6449`. |
| 9 | `readTime()` coherent read semantics and I2C timeout implications | Mostly implemented for seconds-year burst. I2C timeout policy is transport-owned. | Document that read is one transaction and must complete within RV3032 950 ms bus timeout; add read decode/failure tests. | `src/RV3032.cpp:462-500`, manual timeout extract `docs/pdf-extracted-md/RV-3032-C7_App-Manual.md:3802-3888`. |
| 10 | BCD/date/leap-year validation | Partial. Utility tests exist; `readTime()` register decode tests are missing. | Test all public decode/encode boundaries, RTC range 2000..2099, invalid BCD, invalid dates, and weekday policy. | `src/RV3032.cpp:481-500`, tests at `test/test_native/test_datetime.cpp:107-221`. |
| 11 | STOP bit and ESYN synchronization semantics | Gap. Constants and public APIs are missing or misnamed. | Correct command table, add STOP/ESYN typed APIs or explicit set-time options, and tests. | `include/RV3032/CommandTable.h:347-371`, app manual `docs/pdf-extracted-md/RV-3032-C7_App-Manual.md:1503-1518` and `6579-6636`. |
| 12 | PORF/VLF/BSF/EEF/CLKF and interrupt flag exposure | Partial. PORF/VLF/BSF exposed; EEF/CLKF missing from public typed snapshots. | Add fault snapshot and explicit clear APIs for 0Dh and 0Eh flags with THF/TLF side-effect warnings. | `include/RV3032/RV3032.h:124-143`, `src/RV3032.cpp:1693-1765`, register ref `docs/rv-3032-c7_register_reference.md:109-122`. |
| 13 | Alarm interrupt semantics | Partial and risky. AIE exists; alarm disabled semantics are not explicit. | Add explicit alarm disable/clear semantics and tests for all AE combinations and AF/AIE behavior. | `src/RV3032.cpp:559-713`, manual alarm combinations `docs/pdf-extracted-md/RV-3032-C7_App-Manual.md:4963-4978`. |
| 14 | Periodic countdown timer semantics | Partial. Timer value/frequency and TE supported; TIE interrupt API missing; ticks 0 accepted. | Enforce/document 1..4095 active countdown semantics, add TIE API and exact register tests. | `src/RV3032.cpp:718-783`, `include/RV3032/CommandTable.h:347-354`. |
| 15 | External event/time-stamp semantics | Partial. EVI edge/debounce/overwrite and timestamp read/reset exist; ESYN/EIE missing. | Add event interrupt enable, ESYN control/cancel semantics, and full timestamp reset tests. | `src/RV3032.cpp:1018-1165`, `include/RV3032/RV3032.h:696-710`, README stale at `README.md:199-200`. |
| 16 | Temperature registers and threshold interrupt semantics | Partial. Temperature read exists; threshold/THE/TLE/THIE/TLIE APIs missing. | Add coherent temperature read contract, threshold configuration, temperature interrupt APIs, and THF/TLF clear tests. | `src/RV3032.cpp:992-1013`, `include/RV3032/CommandTable.h:128-134`, Control3 manual lines `docs/pdf-extracted-md/RV-3032-C7_App-Manual.md:1530-1583`. |
| 17 | Backup switchover behavior and flags | Partial. PMU backup helpers and BSF clear exist; default `begin()` changes backup mode to Level. | Make default behavior explicit, avoid surprising PMU writes, expose BSIE if supporting interrupts, and test backup flag handling. | `include/RV3032/Config.h:75-77`, `src/RV3032.cpp:429-455`, `src/RV3032.cpp:788-865`, `src/RV3032.cpp:1750-1765`. |
| 18 | User EEPROM APIs through `EEADDR`, `EEDATA`, `EECMD` | Gap. No public user EEPROM API. | Add bounded read/write APIs for 0xCB..0xEA using command flow and status diagnostics. | Constants at `include/RV3032/CommandTable.h:248-259` and `317-326`; no public API near `include/RV3032/RV3032.h:773-845`. |
| 19 | EEPROM busy/failure/dirty diagnostics | Partial. Busy/status/count/queue diagnostics exist, but no failed address/value or dirty state. | Expose pending/failed EEPROM diagnostic snapshot and only clear dirty state after verified success/resync. | `include/RV3032/RV3032.h:407-439`, `src/RV3032.cpp:1395-1513`. |
| 20 | Password/write protection behavior | Gap. Constants exist; high-level APIs and tests do not. | Add explicit unlock/lock/write-protection APIs or document unsupported behavior and status propagation. | `include/RV3032/CommandTable.h:232-246`, `include/RV3032/CommandTable.h:297-315`, app manual `docs/pdf-extracted-md/RV-3032-C7_App-Manual.md:6694`. |
| 21 | Clock output configuration and safety notes | Partial. Only NCLKE and FD are handled. | Model XTAL vs HF mode, FD/HFD/OS/CLKIE/CLKF semantics, and warn about current and measurement constraints. | `src/RV3032.cpp:869-940`, `include/RV3032/CommandTable.h:373-382`, manual `docs/pdf-extracted-md/RV-3032-C7_App-Manual.md:2772-2805`. |
| 22 | Pure ESP-IDF component/example/build path | Gap. No CMake/idf component files or pure IDF example. | Add native ESP-IDF component metadata, example, and CI using `idf.py` with app-owned I2C, locks, timebase, and error mapping. | `platformio.ini:7`, `library.json:34-35`; `CMakeLists.txt` and `idf_component.yml` absent. |
| 23 | Arduino examples and diagnostic-only labeling | Partial. CLI is clearly Arduino/Wire and diagnostic, but no production shared-bus template. | Keep diagnostic CLI; add production shared-bus example or docs with ownership, locking, timeout, and scheduling. | `examples/01_basic_bringup_cli/main.cpp:16-18`, commands at `examples/01_basic_bringup_cli/main.cpp:340-368`, README `README.md:349-380`. |
| 24 | CLI/selftest/stress diagnostics | Partial. CLI stress/selftest exists; not hardware-validation evidence. | Keep diagnostics labeled as bring-up tools; add explicit hardware validation report format. | `examples/01_basic_bringup_cli/main.cpp:1481-1871`, `tools/check_cli_contract.py:10-68`. |
| 25 | Native tests and fault-injection coverage | Partial. Useful baseline but fake bus is shallow and many fault paths are untested. | Expand fake transport with scheduled failures, short reads, sticky busy/EEF, partial writes, and register event hooks. | Fake bus `test/test_native/test_datetime.cpp:13-90`, test list `test/test_native/test_datetime.cpp:886-922`. |
| 26 | Hardware validation matrix | Gap. No hardware validation was run in this audit. | Add board/power/test matrix covering rollover, backup, interrupts, EEPROM, disconnect/reconnect, and soak. | Current reports explicitly say hardware not run: `docs/RV3032_HARDENING_FINAL_REPORT.md:37-38`. |
| 27 | CI/build/package validation | Partial. Arduino S2/S3, native tests, guards, and package pack exist; no ESP-IDF CI. | Keep current CI and add pure ESP-IDF build plus package/version guards. | `.github/workflows/ci.yml:14-92`, `platformio.ini:43-90`, `scripts/generate_version.py`. |
| 28 | Release readiness and allowed/prohibited claims | Partial. Some docs are stale and readiness claims need tightening. | Allow Arduino/PlatformIO claims only after current checks; prohibit ESP-IDF production and hardware claims until validated. | README production notes `README.md:432-439`; stale older report `docs/RV3032_INDUSTRY_READINESS_REPORT.md:24-32`. |

## Recommended Chunk-By-Chunk Sequence

1. Chunk 02 - Core contracts and command-table safety:
   Correct stale public docs, fix CommandTable naming for STOP/ESYN/Control3/EEPROM commands, decide direct register access policy, align `recover()` diagnostics, update IDF portability docs, and expand guard tooling.

2. Chunk 03 - Time, flags, and synchronization:
   Add STOP-controlled set-time semantics, document read coherence and 950 ms I2C timeout implication, add fault snapshots for PORF/VLF/BSF/EEF/CLKF, and make clear APIs side-effect explicit.

3. Chunk 04 - EEPROM, alarms, timers, events, interrupts:
   Add user EEPROM APIs, fix EEPROM sequencing and dirty diagnostics, implement password/write-protection policy, make alarm/timer/event/temperature interrupt APIs explicit, and test all bit mappings.

4. Chunk 05 - ESP-IDF, CI, examples, docs:
   Add pure ESP-IDF component/example/build path, app-owned bus locking/timeouts, `esp_timer` timebase, CI build, and production shared-bus documentation.

5. Chunk 06 - Hardware validation and release readiness:
   Execute and record hardware validation on ESP32-S2/S3 with RV3032-C7, update the hardware matrix, reconcile all docs, and prepare release notes.

## Public API Changes Likely Needed

- Add explicit `setTime()` mode/options or a dedicated STOP-controlled set-time API.
- Add STOP and ESYN control APIs if public synchronization is supported.
- Add typed `FaultFlags` or extend validity/status snapshots to include EEF, EEbusy, CLKF, BSF, PORF, VLF, THF, TLF, AF, TF, EVF, UF.
- Add explicit clear APIs for 0Dh and 0Eh flags, with THF/TLF side-effect documented in each affected function.
- Add user EEPROM read/write APIs with offset/length bounds and command-flow status.
- Add EEPROM diagnostic snapshot containing active/pending/failed address, value, queue depth, and dirty state.
- Add password/write-protection APIs or explicit unsupported status for protected devices.
- Add timer interrupt enable/disable and reject or document `ticks == 0`.
- Add alarm disable semantics that do not encode unintended every-minute alarms.
- Add event interrupt enable, ESYN arm/cancel, and timestamp reset APIs for all sources.
- Add temperature threshold/interrupt configuration and a coherent temperature read policy.
- Add CLKOUT mode API that models XTAL/HF selection and FD/HFD fields.
- Consider changing `Config::backupMode` default or adding an explicit "do not touch PMU" mode. This would be a breaking API/config change and needs migration notes.

## Tests To Add Per Chunk

| Chunk | Tests to add |
| --- | --- |
| 02 | Compile-time CommandTable bit assertions against local register reference; guard tests for no public-header Arduino examples; direct register access-mode/reserved-mask tests; recover `lastError()` mapping test; guard tool coverage for `Wire`, `Serial`, `String`, `delay`, heap, and logging in core. |
| 03 | `readTime()` decode and invalid BCD/date tests; `setTime()` exact write sequence tests; STOP set/release sequence tests; status clear side-effect/race tests; EEF/CLKF/BSF/PORF/VLF read and clear tests. |
| 04 | EEPROM command flow tests for EERD, EEbusy, EEF pre-clear, EECMD 0x21/0x22, timeouts, queue full, restore failure, dirty diagnostics; alarm disable/every-minute tests; timer zero/TIE tests; EVI/ESYN/EIE tests; temperature threshold tests. |
| 05 | Pure ESP-IDF example compile test; Arduino examples still build; production shared-bus example contract test; docs checks for no false ESP-IDF/hardware claims. |
| 06 | Hardware validation evidence tests/checklists, not synthetic pass claims; package/version/changelog/readme release checks. |

## Hardware Validation Checklist

Do not mark these passed until executed on real hardware and recorded with board, bus speed, power setup, firmware commit, and date.

- ESP32-S2 and ESP32-S3 boot, probe, recover, and repeated readTime at 100 kHz and 400 kHz.
- Clock/calendar set using STOP-controlled path and verify rollover across minute/hour/day/month/year boundaries.
- Leap day read/write and 2099 upper-bound behavior.
- PORF/VLF/BSF/EEF/CLKF read and explicit clear behavior.
- Backup switchover with VDD removal/reapply; verify BSF and time retention.
- Alarm AF/AIE behavior on INT pin, including disabled/no-unintended-every-minute case.
- Timer TF/TIE behavior for each frequency and boundary values 1, 4095, and invalid 0.
- EVI edge/debounce/overwrite, EIE, ESYN, EVI timestamp hundredths, reset behavior.
- TLow/THigh threshold interrupt flags, timestamps, and THF/TLF clear side effects.
- User EEPROM read/write persistence, EEbusy timeout, EEF failure handling if fault-injection hardware allows.
- Password/write-protection lock/unlock and protected-write failure behavior.
- CLKOUT frequency/mode selection, current impact, and 1 Hz accuracy measurement guidance.
- I2C fault recovery: address NACK, data NACK, bus stuck/recovery, unplug/replug, and timeout.
- Soak test: 24-72 hour periodic reads with health counters and no heap growth.

## Allowed And Prohibited Release Claims

Allowed now, if the listed checks pass in the release run:

- Framework-neutral compiled core implementation for the tracked `include/` and `src/` files.
- Arduino/PlatformIO ESP32-S2/S3 build support.
- Native fake-transport unit test coverage for currently tested contracts.
- Diagnostic Arduino CLI example.
- Injected, non-owning I2C transport model.

Prohibited until implemented and validated:

- Pure ESP-IDF production readiness.
- Hardware-validated production readiness.
- Complete RV3032-C7 interrupt coverage.
- Complete user EEPROM/password/write-protection support.
- Fully coherent STOP/ESYN time synchronization support.
- Complete power-loss/backup/fault handling.
- Any claim that diagnostics, selftest, or native fake tests prove hardware behavior.

## Prompt 01 Validation

- `python tools/check_core_timing_guard.py`: passed.
- `python tools/check_cli_contract.py`: passed.
- `python scripts/generate_version.py check`: passed; `include/RV3032/Version.h` was up to date.
- `python -m platformio test -e native`: passed; 34/34 test cases succeeded.
- Hardware validation: not run.
- Pure ESP-IDF build validation: not run; no pure ESP-IDF component/example path exists in this chunk.
