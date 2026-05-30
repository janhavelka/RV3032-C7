# RV3032-C7 Hardware Validation Matrix

Branch: `hardening/rv3032-industry-readiness`

Last updated: 2026-05-30

No physical RV3032-C7 hardware validation was run in this Prompt 06 pass. This
matrix defines the bench evidence required before merge, before release, and
before any field/industry-grade readiness claim.

## Validation Tiers

| Tier | Meaning | Current Status |
| --- | --- | --- |
| Required before merge | Must be satisfied before merging the hardening branch unless explicitly waived by maintainers. | Local fake-transport tests and builds pass; physical hardware tests are not run. |
| Required before release | Must pass on real hardware before publishing a production-oriented release. | Not run. |
| Required before field/industry-grade claim | Must pass across representative hardware, power, fault, and soak conditions before claiming field-proven readiness. | Not run. |

## Required Test Record

Every hardware run must record:

- Commit hash and date.
- Firmware/example used.
- Board/module, ESP32 target, RTC breakout/module, and RV3032-C7 marking if visible.
- SDA/SCL pins, pull-up values, bus speed, and other devices on the bus.
- VDD voltage, VBACKUP source, INT pull-up rail/value, EVI wiring, and CLKOUT wiring if used.
- Toolchain versions: PlatformIO or ESP-IDF, Arduino core or IDF release, host OS.
- Exact CLI or serial commands run and observed output.
- Pass/fail result and anomalies.

## Basic Bus And Device

| Test | Merge | Release | Field Claim | Status |
| --- | --- | --- | --- | --- |
| I2C scan sees RV3032 at `0x51`. | Recommended | Required | Required on each target board | Not run |
| `probe()` succeeds with the device present. | Recommended | Required | Required on each target board | Not run |
| Device absent/address NACK maps to `DEVICE_NOT_FOUND` or address-NACK status. | Native fake test covers mapping | Required if bench setup allows disconnect | Required | Not run on hardware |
| Data NACK, timeout, and bus/arbitration errors remain distinguishable if the transport can expose them. | Native fake tests cover mapping | Required with fault injection when practical | Required with fault injection | Not run on hardware |
| Safe unplug/replug or bus recovery returns the driver to `READY` through application recovery plus `recover()`. | Recommended | Required if connector/test setup supports it | Required | Not run |

## Time And Date

| Test | Merge | Release | Field Claim | Status |
| --- | --- | --- | --- | --- |
| Set and read current time. | Recommended | Required | Required | Not run |
| Coherent seconds/minutes/hours/date rollover. | Recommended | Required | Required with logged timestamps | Not run |
| Month/year rollover. | Optional before merge | Required | Required | Not run |
| Leap-year behavior within the supported range. | Native fake tests cover conversion/validation | Required on hardware if practical | Required | Not run on hardware |
| Invalid BCD/date rejection from mocked/faulted registers. | Native fake tests pass | Optional on hardware | Required with fault injection if claiming robustness | Native only |
| PORF/VLF behavior after power event and trusted-time reinitialization flow. | Optional before merge | Required | Required across VDD/VBACKUP scenarios | Not run |

## STOP And ESYN

| Test | Merge | Release | Field Claim | Status |
| --- | --- | --- | --- | --- |
| STOP-controlled `setTime()` does not produce torn calendar values. | Native fake tests cover sequence | Required | Required over repeated sets | Not run on hardware |
| Seconds/hundredths reset behavior during STOP/time set is observed or explicitly documented as not measured. | Optional before merge | Required | Required | Not run |
| ESYN behavior. | Not required because high-level ESYN API is deferred | Not required until implemented | Required before claiming ESYN support | Deferred |

## Flags

| Test | Merge | Release | Field Claim | Status |
| --- | --- | --- | --- | --- |
| Read STATUS and fault/validity flags: THF, TLF, UF, TF, AF, EVF, PORF, VLF, EEF, EEbusy, CLKF, BSF. | Native fake tests pass | Required | Required | Not run on hardware |
| Clear selected flags only. | Native fake tests pass | Required for user-clearable flags | Required | Not run on hardware |
| Confirm unrelated flags are preserved across clears where the chip supports preservation. | Native fake tests pass | Required | Required | Not run on hardware |
| Confirm `begin()`, `readTime()`, and `setTime()` do not silently clear power-loss or voltage-low flags. | Native fake tests pass | Required | Required | Not run on hardware |
| Confirm any STATUS write side effect on THF/TLF is understood and documented for the exact clear operation used. | Native fake tests cover driver mask behavior | Required | Required | Not run on hardware |

## Backup And Power

| Test | Merge | Release | Field Claim | Status |
| --- | --- | --- | --- | --- |
| VDD power-cycle preserves time when backup supply is valid. | Optional before merge | Required | Required across target boards | Not run |
| VBACKUP switchover behavior and BSF flag. | Optional before merge | Required | Required | Not run |
| I2C unavailable or limited behavior in VBACKUP state if testable. | Optional | Required if hardware permits safe test | Required if claimed | Not run |
| INT behavior in VBACKUP for supported sources if testable. | Optional | Required if claimed | Required if claimed | Not run |
| Primary-cell backup defaults disable trickle charger and use level switching. | Native fake tests cover PMU bits | Required on hardware before recommending wiring | Required | Not run on hardware |
| POR startup while `EEbusy` may reflect EEPROM refresh does not race begin-time PMU configuration or queued persistence. | Optional before merge | Required | Required | Not run |

## EEPROM

| Test | Merge | Release | Field Claim | Status |
| --- | --- | --- | --- | --- |
| User EEPROM read/write one byte through public API. | Native fake tests pass | Required | Required | Not run on hardware |
| Multi-byte read/write bounds and out-of-range rejection. | Native fake tests pass | Required | Required | Not run on hardware |
| Persistence after VDD power cycle. | Optional before merge | Required | Required | Not run |
| `EEbusy` handling and timeout behavior. | Native fake tests pass | Required if inducible | Required with fault injection if claiming robustness | Native only |
| `EEF` write-failure behavior if inducible. | Native fake tests pass | Required if inducible | Required with fault injection if claiming robustness | Native only |
| Async configuration EEPROM persistence via `tick()` for CLKOUT, offset, and backup mode completes or reports failure without hidden retries. | Native fake tests cover queue mechanics | Required | Required | Not run on hardware |
| Password/write-protection behavior. | Not required; API deferred | Required before claiming support | Required before claiming support | Deferred |

## Alarm, Timer, Event, And INT

| Test | Merge | Release | Field Claim | Status |
| --- | --- | --- | --- | --- |
| Alarm match produces AF flag. | Recommended | Required | Required | Not run |
| Alarm disabled behavior and all-`AE_x=1` every-minute hazard are verified before using alarm APIs in production. | Optional before merge | Required | Required | Not run |
| Alarm interrupt routing asserts INT and clear sequence releases INT. | Recommended | Required | Required | Not run |
| Timer countdown produces TF flag and optional INT. | Recommended | Required | Required | Not run |
| Timer boundaries `0`, `1`, and `4095` are verified across supported frequencies; `0` must not start through public API. | Native fake tests cover invalid `0` and sequencing | Required | Required | Not run on hardware |
| Update flag/interrupt UF behavior if enabled through existing APIs. | Optional before merge | Required before claiming update interrupt behavior | Required if claimed | Not run |
| External event input EVF and timestamp behavior with EVI held at a defined level. | Optional before merge | Required | Required | Not run |
| EVI edge selection, debounce/filter behavior, EVOW first/last timestamp policy, and EVI timestamp reset. | Optional before merge | Required | Required | Not run |
| TLow/THigh timestamp reset side effects and no-capture behavior. | Optional before merge | Required | Required | Not run |
| INT open-drain electrical behavior with actual pull-up rail/value. | Optional before merge | Required | Required | Not run |
| Multiple enabled sources sharing INT are serviced and cleared without losing unrelated flags. | Optional before merge | Required | Required | Not run |

## Soak

| Test | Merge | Release | Field Claim | Status |
| --- | --- | --- | --- | --- |
| Repeated `readTime()` loop for at least 1 hour with no health degradation. | Optional before merge | Required | Required with longer duration | Not run |
| Repeated safe set/read cycle with bounded count and no EEPROM wear-heavy settings. | Optional | Required if set-time reliability is claimed | Required | Not run |
| Periodic flag polling with no silent clears. | Optional | Required | Required | Not run |
| Multi-hour or multi-day shared-bus soak on representative hardware. | Not required before merge | Recommended | Required | Not run |

## CLI And Hardware Command Plan

The current Arduino diagnostic CLI provides these relevant commands:

```text
version
scan
probe
drv
validity
statusf
time
set YYYY MM DD HH MM SS
selftest
stress N
```

The prompt-suggested command names `state`, `flags`, `settime`, and
`stress_time` are not current CLI commands. Current equivalents are `drv`,
`statusf`/`validity`, `set`, and `stress`.

No hardware CLI commands were run in Prompt 06 because no serial-connected
RV3032-C7 hardware was available in this session.
