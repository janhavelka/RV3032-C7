# RV3032-C7 HIL Summary

This file keeps the durable hardware-in-the-loop evidence. Raw runner JSON,
step tables, stdout/stderr captures, and full serial transcripts are not kept in
the repository because they are large generated artifacts.

## Current v3.0.0 COM21 / PIOArduino 55.03.311 Evidence

### Fixture

| Item | Value |
|---|---|
| Date | `2026-07-31` |
| Port | `COM21`, USB VID:PID `303A:1001` |
| Board/target | ESP32-S3 revision 0.2, built-in PlatformIO board `esp32-s3-devkitc1-n16r8`, 16 MB flash, 8 MB PSRAM |
| MCU MAC | `44:1b:f6:8d:1f:90` |
| Runtime | PIOArduino platform `55.03.311` (PlatformIO displays `55.3.311`), Arduino-ESP32 `3.3.11`, ESP-IDF `v5.5.5` |
| RTC address | `0x51` |
| Other detected I2C addresses | `0x3C`, `0x41`, `0x50` |
| I2C pins/clock | SDA `GPIO8`, SCL `GPIO9`, `400000` Hz |
| I2C callback timeout | `50` ms |
| Backup cell / VDD | Operator-confirmed non-rechargeable lithium cell installed; stable `3.3` V main rail |
| Firmware/library | `3.0.0`; final CLI printed base commit `5e5c4f7` dirty because the corrected persistence-return harness remained intentionally uncommitted |

### Results

| Run | Result | Evidence |
|---|---:|---|
| Autonomous exhaustive HIL | 157 PASS, 0 FAIL, 1 SKIP | 783 read callbacks, 436 write callbacks, four intentional `WRITE_ONE` commands, and zero health failures. The skip was the separately authorized primary-cell safety case. |
| Intensive one-hour CLI soak | 24491 PASS, 0 FAIL, 0 UNKNOWN | 3572.594 s; maximum consecutive failures `0`, worst command latency 0.625 s, and no RTC/I2C error token. |
| Post-wait-adapter primary-cell campaign | 51 PASS, 0 FAIL | Persistent and active primary-cell targets plus cleanup were verified. The persistent byte was already correct, so `WRITE_ONE` count was zero; a second same-lifecycle call was rejected. |
| Maximum read stress | 100000 PASS, 0 FAIL | 33.685 s, approximately 2968.7 operations/s, driver health clean. |
| Maximum mixed stress | 100000 PASS, 0 FAIL | 24.061 s, approximately 4156.1 operations/s across all seven read paths, driver health clean. |
| Battery retention | PASS | After a main-VDD removal with the lithium cell retained, PORF/VLF remained clear, BSF set, time stayed valid and advanced by 49 s, and the exact RAM sentinel survived. The post-return selftest was 16/16. |
| Corrected two-cycle configuration persistence | PASS | Original persistent/active C0-C5 `20 00 00 00 38 0D` were saved. Alternate `60 05 24 60 23 01` was staged with exactly six `WRITE_ONE` commands and survived the first cycle. Startup primary-cell reconciliation restored the volatile active C0 semantics with zero persistent writes. The originals were then restored with exactly six commands and survived the second cycle; final startup reconciliation again used zero writes. Both persistent and active copies were exact after each return. |
| Final normal-CLI cleanup | PASS | User RAM bytes 8-15 were zeroed and read back; PORF/VLF/BSF were clear and time valid; backup mode was level with charger bits `0x0`; timer was exactly zero/4096 Hz/disabled; selftest was 16/16; read and mixed stress were each 100/100; driver state was READY with zero failures. Runtime reported ESP32-S3 revision 0.2, 16 MB flash, 8 MB PSRAM, Arduino-ESP32 3.3.11, and ESP-IDF v5.5.5. |

The final physical-cycle transcript reported:

```text
VALIDITY PORF=0 VLF=0 BSF=1 TIME_VALID=1
RESTORE_RETURN_PRIMARY_ENSURE_PASS write_one=0
PERSISTENT_C0_C5 20 00 00 00 38 0D
ACTIVE_C0_C5 20 00 00 00 38 0D
PERSIST_POWER_CYCLE_COMPLETE
RESTORED_C0_C5 20 00 00 00 38 0D
```

### Findings and Repository Changes

- Updating from PIOArduino `54.03.20` to `55.03.311` moved the runtime from
  Arduino-ESP32 3.2.0 / ESP-IDF 5.4.1 to Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5.
  The configuration now uses the built-in N16R8 board definition and removes
  obsolete manual 4 MB flash/partition/PSRAM overrides and the old PSRAM cache
  workaround flag.
- A disabled RV3032 timer legitimately reads preset zero. The API, CLI,
  documentation, native tests, and HIL now accept `ticks=0` only when the timer
  is disabled; enabled zero remains a pre-I/O validation error.
- Arduino `delay(ms)` is tick-relative and can return just short of a full
  monotonic millisecond interval. Example and HIL wait adapters add one guard
  tick, preserving the driver's strict vendor-settle proof without weakening
  its early-return detection.
- A backup-powered return preserves persistent C0 but its active backup-switch
  state needs the product's explicit startup primary-cell reconciliation. The
  corrected persistence harness performs that bounded operation before exact
  active-byte verification and requires persistent target proof, active target
  proof, cleanup proof, and zero additional persistent writes.
- PIOArduino can generate a root `idf_component.yml` when `src_dir = .`. That
  transient file and its `.orig` form are ignored, excluded from library
  exports, and rejected by the package contract because this repository does
  not ship a native ESP-IDF component.

### Invalid Attempts, Recovery, and Exclusions

- The first two requested removals occurred before the backup battery had been
  installed. PORF, invalid time, and lost RAM correctly made those attempts
  invalid as retention evidence. The operator then installed the cell, the
  original configuration was recovered, and only the later valid cycles are
  counted above.
- An earlier persistence-harness ordering exposed active C0 `0x40` after a
  backup return and then restored persistent C0 incorrectly. C1-C5 were already
  restored; an explicitly authorized primary-cell ensure recovered C0 to
  `0x20` with one bounded `WRITE_ONE`. The corrected two-cycle campaign was
  restarted from exact original persistent and active bytes and passed. This
  was a harness/startup-lifecycle finding, not a silent driver failure.
- A separate TunnelMonitor upload briefly replaced the HIL firmware during the
  campaign. The intended firmware and exact RTC state were restored and
  reverified; this was external fixture activity, not a library failure.
- The operator accepted the existing GPIO/output fixture risk and declined an
  independent VBACKUP measurement. No oscilloscope measurement of rail decay,
  switchover timing, or backfeed current was made.
- No deliberate bus short/disconnect, external temperature-limit stimulus, or
  EVI stimulus was applied. Native fault injection covers bounded software
  failure paths without electrically disturbing the shared bus.

## Current v3.0.0 COM20 Evidence

### Fixture

| Item | Value |
|---|---|
| Date | `2026-07-16` through `2026-07-17` |
| Port | `COM20` |
| Board/target | ESP32-S3 revision 0.2, PlatformIO `esp32s3dev` / `esp32s3hil` |
| MCU MAC | `1c:db:d4:80:c9:40` |
| RTC address | `0x51` |
| Backup-cell chemistry | Operator-confirmed non-rechargeable primary cells |
| Other detected I2C addresses | `0x50`, `0x3C` |
| I2C pins/clock | SDA `GPIO8`, SCL `GPIO9`, `400000` Hz |
| I2C callback timeout | `50` ms |
| Firmware/library | `3.0.0`; the first power-loss image printed base commit `5122db2` dirty and its tested HIL changes were subsequently committed as `4b653b8`; the persistence harness and final CLI image printed base commit `4b653b8` dirty |
| Power scope | Three operator-performed USB/main-VDD removals while primary cells remained connected; measured COM20 absences were `10.531` s and `10.466` s, while the first persistence-cycle edge was not captured |

### Results

| Run | Result | Evidence |
|---|---:|---|
| Post-fix functional plus short stress | 28 PASS, 0 FAIL | Probe, health, time/Unix/temperature, status, alarm/timer/CLKOUT/EVI, timestamps, RAM/registers, 16-case selftest, and two 50-operation stress blocks. |
| Autonomous exhaustive HIL | 157 PASS, 0 FAIL, 1 SKIP | 783 read callbacks, 436 write callbacks, 1219 total callbacks, and four intentionally bounded `WRITE_ONE` commands in the clean run. |
| Maximum read stress | 100000 PASS, 0 FAIL | 33.106 s, 3020.6 operations/s, 324 us minimum / 543 us maximum / 325 us average. |
| Maximum mixed stress | 100000 PASS, 0 FAIL | 23.818 s, 4198.5 operations/s; seven public read paths split evenly. |
| Intensive soak | 21460 device-command PASS, 34 host-framing rows, 0 UNKNOWN | 3574.125 s, 37-command mix, maximum consecutive classified failures `1`. All 34 incomplete captures were completed in the immediately following row and contained no device error token. |
| Post-soak functional plus short stress | 28 PASS, 0 FAIL | Driver READY, Status `0x00`, PORF/VLF/BSF clear, valid advancing calendar, selftest 16/16, and both 50-operation stress blocks clean. |
| Authorized primary-cell run | 51 PASS, 0 FAIL | Persistent C0 was already the exact `0x20` target, so no `WRITE_ONE` was issued. Persistent and active targets plus cleanup were verified; the second same-lifecycle call was rejected; active level no-op and level/off/level mutation paths were verified with charger-off readback; the profile survived the setup mix; selftest was 16/16 and both 50-operation stress blocks passed. |
| Physical power-loss/reboot | PASS | Before removal, Unix time was `1784308937` with PORF/VLF/BSF clear. After COM20 was absent for `10.531` s, time had advanced to `1784309113` and was within 2 s of the host; PORF/VLF remained clear, BSF was set, time remained valid, the RAM sentinel was retained, the application rebooted READY, selftest was 16/16, and read/mixed stress were both 100/100. |
| Configuration EEPROM power-cycle persistence | PASS | The harness saved original C0-C5 `20 00 00 00 58 09`, used typed APIs to stage `60 05 24 60 23 01`, and issued exactly six `WRITE_ONE` commands. After the first physical cycle it directly verified all six persistent and active alternate bytes, restored the originals with exactly six more `WRITE_ONE` commands, and verified both copies. After the second, measured `10.466` s outage, it again directly verified exact persistent and active originals. |
| Post-persistence cleanup | 10 PASS, 0 FAIL | Normal CLI firmware was restored; the RAM phase record was zeroed and read back; PORF/VLF/BSF were clear and time valid; backup mode was level with charger bits `0x0`; active C0-C5 were `20 00 00 00 58 09`; primary-cell ensure returned already configured with no write; selftest was 16/16; read and mixed stress were both 100/100; driver state was READY with zero failures. |
| Native fault-injection regression | 111 PASS, 0 FAIL | Deadline, wrap, retry/reconciliation, bounded-budget, EEPROM, primary-cell, register-mask, transport-domain, and CLI contracts. |

The intensive soak ran 21,494 commands: 34 commands in the mix ran 581 times
each, while `selftest`, `stress 25`, and `stress_mix 25` ran 580
times each. Its 34 raw runner failures were 13 `selftest`, 12 second RAM-write,
five `clkout_freq 3`, three `probe`, and one `clkout 0` capture. Each response
tail appeared at the start of the next command capture. The transcript contained
no RTC/I2C timeout, NACK, bus error, `DEGRADED`, `OFFLINE`, nonzero stress
failure count, or selftest failure.

The autonomous harness changed and restored user EEPROM byte 31 and persistent
offset C1 once per pass, with compare-equal checks proving zero `WRITE_ONE`
commands. Two harness passes were executed, so the complete campaign issued
eight `WRITE_ONE` commands: four user-byte change/restores and four C1
change/restores. Generic persistence remained disabled throughout the soak.
The later authorized primary-cell runs found persistent C0 already equal to
the exact target and issued no additional `WRITE_ONE` command. The later
configuration power-cycle harness issued 12 more commands: six to stage the
alternate C0-C5 values and six to restore the exact originals. The complete
campaign therefore issued 20 intentional `WRITE_ONE` commands. No write was
blindly retried, and the final direct persistent read proved restoration.

### Exclusions

- The primary-cell operation ran on the continuously USB-powered nominal 3.3 V
  fixture after chemistry confirmation. The rail and VDD-off backfeed current
  were not independently measured. Direct-switch mode was not tested because
  it is unsafe for this primary-cell fixture near VDD.
- Three short main-power-loss cycles were performed. The first persistence
  cycle began before the host monitor captured its removal edge, so its exact
  outage duration is unavailable. No extended outage, repeated-cycle
  endurance beyond these short functional cycles, backup-cell
  depletion/removal, or oscilloscope measurement of switchover timing and rail
  decay was performed.
- No deliberate physical disconnect, bus short, external temperature limit,
  or EVI stimulus was applied. Owner-level NACK injection on target and the
  native fault matrix covered software handling without disturbing the bus.

### Findings and Repository Changes

- The first fresh image found `0x51` on the bus but the CLI stopped before
  `RV3032::begin()`: the example initialized `Wire` and then treated a failed
  redundant `setClock()` as fatal. Passing the frequency to the single
  `Wire.begin(sda, scl, frequency)` call fixed COM20 and retained ESP32-S2/S3
  builds and native coverage.
- The intensive runner expected cooperative 16-byte RAM terminal wording for
  synchronous four-byte writes. The expectation now matches the actual public
  completion surface; a 431-check regression then passed with zero failures.
- The destructive runner previously accepted any primary-cell report header.
  It now requires `status=OK`, persistent-target and active-target proof, and
  verified cleanup, then checks the no-write second-call latch, safe active
  level/off/level paths, and active level/charger state after the remaining
  setup commands.
- The physical power-loss test proved the previously excluded retention path:
  RTC time tracked the host across the outage, BSF alone recorded the switch to
  backup, persistent/active primary-cell configuration remained valid, and the
  post-return driver and stress checks were clean. One one-off host assertion
  used uppercase `PORF` while the CLI prints lowercase `porf`; inspection of the
  captured device output confirmed the expected `porf=0 vlf=0` result.
- The configuration persistence test toggled CLKOUT enable and frequency,
  stored divider, offset, and temperature-reference fields through public typed
  APIs. Direct configuration-EEPROM and active-mirror reads proved the staged
  bytes after one physical cycle and the exact originals after restoration and
  a second physical cycle. C0 always retained level switching with charging
  disabled; Direct mode was never selected.
- The reusable target harness is in `test/test_hil/main.cpp`, selected by the
  `esp32s3hil` PlatformIO environment. The two-cycle harness is in
  `test/test_hil_persistence/main.cpp`, selected by
  `esp32s3hil_persistence`. The native environment is explicitly filtered to
  `test_native` so target firmware is not linked as a Windows test.

## Historical v1.5.0 Evidence

### Fixture

| Item | Value |
|---|---|
| Port | `COM27` |
| Board/target | ESP32-S3, PlatformIO `esp32s3dev` |
| Firmware | `examples/01_basic_bringup_cli`, `115200` baud |
| RTC address | `0x51` |
| I2C pins | SDA `GPIO8`, SCL `GPIO9` |
| I2C clock | `400000` Hz |
| Example timeout | `50` ms |
| Library version printed | `1.5.0` |

This evidence is historical 1.5.0 coverage. It does not validate the current
2.0.0 primary-cell protocol, EEPROM voltage/backfeed preconditions,
power-cycle behavior, or backup retention. Those 2.0.0 physical cases remain
`NOT RUN` until separately authorized and executed on identified hardware.

Fixture limits:

- No external temperature-low, temperature-high, or EVI timestamp stimulus was
  attached.
- No deliberate disconnect, wrong-address, bus-short, power-cycle, NACK, or
  bus-timeout fault injection was performed.
- The 48-hour run allowed destructive CLI operations because this was a test
  fixture, not a preserved deployment.

### Runs

| Date | Run | Duration | PASS | FAIL | UNKNOWN | Notes |
|---|---|---:|---:|---:|---:|---|
| 2026-06-26 to 2026-06-27 | Functional + 8-hour soak | 28800.0 s soak | 28140 | 5 | 3 | Five isolated host serial framing failures. Timestamp reads were fixture-limited. |
| 2026-06-27 to 2026-06-29 | Intensive 48-hour soak | 172794.234 s soak | 168772 | 22 | 2 | Full duration completed. Max consecutive failures was 1. Fail rows matched host serial framing/timeout artifacts; post-run health passed. |
| 2026-06-29 | Post-48-hour health | single pass | 27 | 0 | 2 | Confirms device and driver were still responsive after the 48-hour soak. |
| 2026-06-29 | Final runner regression | 109.265 s soak | 148 | 0 | 2 | Verifies the final bounded serial resync change in the runner. |

### Intensive 48-Hour Coverage

The 48-hour loop repeatedly exercised 37 CLI commands:

- Read paths: `time`, `unix`, `temp`, `status`, `statusf`, `validity`,
  `probe`, `drv`, `backup`, `reg 0x0D`, `eeprom`.
- User RAM: `ram_write 0 ...`, `ram 0 4`, `ram_write 4 ...`, `ram 4 4`.
- Control paths: alarm set/match/interrupt/clear, timer on/read/off, clock
  output on/frequency/off, offset set/read, EVI edge/debounce/overwrite/read,
  and `status_clear 0x0F`.
- Stress paths: `stress 25`, `stress_mix 25`, and `selftest`.

Most commands ran 4561 times; `eeprom`, `stress 25`, `stress_mix 25`, and
`selftest` ran 4560 times.

48-hour failure distribution:

- `selftest`: 10
- `probe`: 2
- `evi`: 2
- `status_clear 0x0F`: 1
- `ram 4 4`: 1
- `stress 25`: 1
- `clkout 0`: 1
- `clkout_freq 3`: 1
- `timer`: 1
- `evi debounce 3`: 1
- `drv`: 1

The failed rows did not show device error tokens such as I2C timeout, NACK, bus
error, `DEGRADED`, `OFFLINE`, nonzero stress failure counts, or selftest
`fail>0`. In each inspected failure, the missing response tail appeared in the
following command capture. This is consistent with USB CDC host/runner framing
artifacts, not a persistent RTC or driver failure.

## Runner Notes

The HIL runner lives at [`tools/hil_cli_runner.py`](../../tools/hil_cli_runner.py).
It is a host-side test tool and is not compiled into production firmware.
Its destructive setup path now rejects execution before opening the port unless
fresh authorization fields record the exact port/module, primary-cell
chemistry, power conditions, possible C0 write, and VDD-off/backfeed scope.

After the 48-hour run, the runner was tightened to send one final bounded blank
line resync before declaring a command hard-timeout. A short intensive
regression with that change produced 148 PASS, 0 FAIL, and 2 fixture-limited
UNKNOWN rows.
