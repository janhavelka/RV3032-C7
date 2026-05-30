# RV3032 EEPROM and Interrupt Contracts Report

Date: 2026-05-30

Branch: `hardening/rv3032-industry-readiness`

## Scope

Prompt 04 focused on RV3032 EEPROM command sequencing and interrupt-producing
RTC features: user EEPROM, EEPROM busy/failure diagnostics, timer programming,
alarm/event interrupt routing, explicit flag clearing, and documentation of
hardware validation requirements. ESP-IDF component work remains deferred.

Subagents used:
- `rv3032-datasheet-agent`
- `eeprom-agent`
- `rtc-semantics-agent`
- `test-fault-agent`
- `integration-review-agent`

## EEPROM API and Command Design

Added public offset-based user EEPROM APIs:

- `readUserEepromByte(uint8_t offset, uint8_t& out)`
- `writeUserEepromByte(uint8_t offset, uint8_t value)`
- `readUserEeprom(uint8_t offset, uint8_t* dst, size_t len)`
- `writeUserEeprom(uint8_t offset, const uint8_t* src, size_t len)`

Offsets `0..31` map internally to physical EEPROM addresses `0xCB..0xEA`.
Direct register access to `0xCB..0xEA` remains rejected.

The implementation uses the manual's control path:

1. Read Control1.
2. Set `EERD=1`.
3. Poll `EEbusy=0` with bounded polling before issuing a command.
4. Write `EEADDR`.
5. For writes, write `EEDATA`, then `EECMD=0x21`.
6. For reads, write `EECMD=0x22`, then read `EEDATA`.
7. Poll `EEbusy=0` after the command.
8. Restore the original Control1 byte exactly.

Writes treat `EEF` as `EEPROM_WRITE_FAILED` and do not clear `EEF`
implicitly. Reads preserve existing `EEF` because the manual says `EEF` reports
write-access failure, not read failure. Password/write-protection APIs are
deferred; the local documentation notes that applications must unlock protected
devices before using user EEPROM APIs.

## Dirty and Pending Diagnostics

`SettingsSnapshot` now includes synchronous user EEPROM diagnostics:

- `eepromCommandLastStatus`
- `eepromBusyTimeout`
- `eepromErrorObserved`
- `eepromWritePendingDirty`
- `eepromDirtyAddress`
- `eepromDirtyValue`
- `eepromRecoveryRecommendation`

Dirty state is set when a user EEPROM write may have partially completed or
cannot be verified because of timeout, `EEF`, transport failure, or Control1
restore failure. It is cleared by a later fully successful user EEPROM write.
The existing async config-EEPROM queue/counters remain separate from the new
synchronous user EEPROM command path.

The existing async config EEPROM state machine was also corrected to restore the
original Control1 byte instead of forcing `EERD=0`.

## Timer, Alarm, Event, and Interrupt Design

- `setTimer()` now rejects `ticks == 0` when enabling the timer, writes Timer
  Value 1 high bits as zero, disables `TE` before timer value/frequency updates,
  and enables `TE` last when requested.
- Added `enableTimerInterrupt()` / `getTimerInterruptEnabled()` for explicit
  `TIE` routing of `TF` to INT.
- Added `enableEventInterrupt()` / `getEventInterruptEnabled()` for explicit
  `EIE` routing of `EVF` to INT.
- Alarm time/match APIs remain value/mask setters. `enableAlarmInterrupt()`
  remains the separate `AIE` routing API.
- Added typed clear helpers for `TF`, `UF`, and `EVF`.
- `clearStatus()` and `clearFaultFlags()` now avoid stale read-modify-write
  clears by writing `0` only for requested flags and `1` for non-target bits.
  The STATUS register THF/TLF side effect remains documented.
- EVI setters now mask reserved EVI Control bits `3:1` instead of preserving
  stale read values.
- README/Doxygen document that INT is open-drain active-low, needs a pull-up,
  can be asserted by multiple sources, and requires explicit flag servicing.

## Tests Added or Updated

Native fake-transport coverage now includes:

- Ordered transaction logging for read/write sequencing.
- Fake user EEPROM backing storage and `EECMD=0x21/0x22` side effects.
- Scripted `REG_TEMP_LSB` reads for `EEbusy`/`EEF` polling paths.
- User EEPROM byte read/write command sequences.
- User EEPROM offset/length/null-buffer rejection with no I2C.
- Busy polling before command, pre-command timeout, and post-command timeout.
- `EEF` write failure handling without implicit flag clear.
- Read behavior with preexisting `EEF`.
- Partial I2C failure diagnostics and Control1 restore failure diagnostics.
- Timer disable/write/enable order and exact timer value bit mapping.
- Alarm BCD payload and `AE_x` bit mapping.
- Timer/EVI interrupt enable bits separated from flag clearing.
- Clear-mask behavior for STATUS and 0x0E fault flags.
- Direct-register rejection for the entire user EEPROM range.

## Checks Run

- `python tools/check_core_timing_guard.py` - passed.
- `python tools/check_cli_contract.py` - passed.
- `python scripts/generate_version.py check` - passed; `Version.h` was up to
  date.
- `python -m platformio test -e native` - passed; 67/67 test cases succeeded.
- `python -m platformio run -e esp32s3dev` - passed.
- `python -m platformio run -e esp32s2dev` - passed.

## Deferred

- Password programming/unlock/lock APIs.
- Full dirty-state modeling for general alarm/timer/event read-modify-write
  sequences beyond the specific behavior covered here.
- High-level combined "configure and arm" APIs for alarm, timer, EVI, ESYN, and
  interrupt-controlled CLKOUT.
- Timestamp no-capture modeling and broader timestamp reset side-effect APIs.
- Native ESP-IDF component/example and CI integration.
- Hardware validation matrix and release-readiness report.

## Hardware Validation Still Required

No physical RV3032-C7 hardware validation was run in this chunk. Bench work
still needs to verify:

- User EEPROM write/read/readback, `EEbusy` timing, and `EEF` behavior.
- Password-protected EEPROM behavior and failure visibility.
- Timer first-period behavior, TF clearing, and TIE/INT assertion/release.
- Alarm AF/AIE separation and INT release after explicit AF clear.
- EVI edge/debounce/EVF behavior with a defined input level, including VBACKUP
  behavior where EVI is disabled.
- INT open-drain pull-up value/rail, active-low assertion, release, and multiple
  interrupt-source ORing.
- CLKOUT interrupt-controlled clock behavior if used by production designs.
