# RV3032-C7 HIL Summary

This file keeps the durable hardware-in-the-loop evidence. Raw runner JSON,
step tables, stdout/stderr captures, and full serial transcripts are not kept in
the repository because they are large generated artifacts.

## Fixture

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

Fixture limits:

- No external temperature-low, temperature-high, or EVI timestamp stimulus was
  attached.
- No deliberate disconnect, wrong-address, bus-short, power-cycle, NACK, or
  bus-timeout fault injection was performed.
- The 48-hour run allowed destructive CLI operations because this was a test
  fixture, not a preserved deployment.

## Runs

| Date | Run | Duration | PASS | FAIL | UNKNOWN | Notes |
|---|---|---:|---:|---:|---:|---|
| 2026-06-26 to 2026-06-27 | Functional + 8-hour soak | 28800.0 s soak | 28140 | 5 | 3 | Five isolated host serial framing failures. Timestamp reads were fixture-limited. |
| 2026-06-27 to 2026-06-29 | Intensive 48-hour soak | 172794.234 s soak | 168772 | 22 | 2 | Full duration completed. Max consecutive failures was 1. Fail rows matched host serial framing/timeout artifacts; post-run health passed. |
| 2026-06-29 | Post-48-hour health | single pass | 27 | 0 | 2 | Confirms device and driver were still responsive after the 48-hour soak. |
| 2026-06-29 | Final runner regression | 109.265 s soak | 148 | 0 | 2 | Verifies the final bounded serial resync change in the runner. |

## Intensive 48-Hour Coverage

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
