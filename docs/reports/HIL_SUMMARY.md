# RV3032-C7 HIL summary

This file retains concise hardware-in-the-loop evidence. Raw runner JSON,
generated step tables, stdout/stderr captures, PID files, and serial
transcripts are intentionally not committed.

## Latest retained campaign

The latest physical campaign ran on 2026-07-31 against the v3.0.0 integration
surface. The final persistence-return correction used in that campaign was
subsequently captured by commit `ae08889`; the campaign did not run against a
clean future release tag. Release-preparation and device-free validation must
therefore be reported separately from this physical evidence.

### Fixture

| Item | Value |
|---|---|
| Target | ESP32-S3 revision 0.2, `esp32-s3-devkitc1-n16r8`, 16 MB flash, 8 MB PSRAM |
| Runtime | PIOArduino `55.03.311`, Arduino-ESP32 `3.3.11`, ESP-IDF `5.5.5` |
| RTC bus | Address `0x51`, SDA GPIO8, SCL GPIO9, 400 kHz, 50 ms callback timeout |
| Power scope | Stable 3.3 V main rail; operator-confirmed non-rechargeable lithium backup cell |
| Library surface | `3.0.0`, including the later-committed persistence-return harness correction |

Host-local serial-port names, MCU identifiers, and unrelated devices on the
shared bus are omitted because they add no reproducible evidence.

### Results

| Run | Result | Evidence |
|---|---:|---|
| Autonomous exhaustive HIL | 157 PASS, 0 FAIL, 1 SKIP | 783 read callbacks, 436 write callbacks, four intentional `WRITE_ONE` commands, and zero health failures. The skip was the separately authorized primary-cell safety case. |
| Intensive CLI soak | 24,491 PASS, 0 FAIL, 0 UNKNOWN | 3,572.594 s; maximum consecutive failures 0, worst command latency 0.625 s, and no RTC/I2C error token. |
| Primary-cell campaign | 51 PASS, 0 FAIL | Persistent and active targets plus cleanup were verified. The persistent byte was already correct, so no `WRITE_ONE` was issued; a second same-lifecycle call was rejected. |
| Maximum read stress | 100,000 PASS, 0 FAIL | 33.685 s; driver health remained clean. |
| Maximum mixed stress | 100,000 PASS, 0 FAIL | 24.061 s across all seven read paths; driver health remained clean. |
| Battery retention | PASS | After main-VDD removal with the backup cell retained, PORF/VLF remained clear, BSF set, time advanced by 49 s, the RAM sentinel survived, and the post-return self-test passed 16/16. |
| Two-cycle configuration persistence | PASS | Alternate C0-C5 values survived the first power cycle; the exact originals were restored and survived the second. Each six-byte change used exactly six `WRITE_ONE` commands. Startup primary-cell reconciliation restored active C0 semantics with zero additional persistent writes. |
| Final cleanup | PASS | User RAM was cleared and read back; PORF/VLF/BSF were clear; time was valid; backup mode was level with charging disabled; timer was zero/4096 Hz/disabled; self-test passed 16/16; short read/mixed stress passed; driver health was READY with zero failures. |

The exact original persistent and active C0-C5 bytes were
`20 00 00 00 38 0D`; the alternate test bytes were
`60 05 24 60 23 01`. Final direct reads proved the originals restored.

### Findings retained in the repository

- A disabled timer may legitimately read preset zero. The API accepts zero
  only when `enable=false`; enabling a zero preset remains a pre-I/O error.
- Arduino-ESP32 `delay(ms)` is tick-relative. Example and HIL wait adapters add
  one guard tick so the driver's vendor settle intervals cannot be shortened.
- After a backup-powered return, product startup must explicitly reconcile the
  active primary-cell C0 state before exact active-configuration comparison.
  The dedicated ensure operation proved that reconciliation without another
  persistent write when EEPROM was already correct.
- PIOArduino may generate root ESP-IDF component manifests while repairing its
  framework package. This Arduino-only library ignores, excludes, and rejects
  those transient files from published packages.

### Limits

- The operator accepted the existing fixture GPIO/output risk. VBACKUP rail
  decay, switchover timing, and VDD-off backfeed current were not measured with
  an oscilloscope or independent meter.
- No deliberate bus short/disconnect, external temperature-limit stimulus, or
  EVI stimulus was applied. Native fault injection covers bounded software
  paths without electrically disturbing the shared bus.
- The campaign provides short functional power-cycle evidence, not EEPROM
  endurance, long-outage retention, oscillator-accuracy, qualification, or
  long-term field-stability evidence.

## Runner ownership

The host runner is [`tools/hil_cli_runner.py`](../../tools/hil_cli_runner.py).
Physical execution requires an explicit serial port. Destructive setup also
requires fresh scope fields naming the port/module, primary-cell chemistry,
power conditions, possible C0 write, and VDD-off/backfeed authorization. The
parser self-test and dry-run are device-free and open no serial port.
