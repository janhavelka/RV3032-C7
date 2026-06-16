# RV3032-C7 TunnelMonitor Fit Report

This audit keeps the library compatible with a caller-owned I2C task model:
the application owns the bus, supplies typed transport callbacks, supplies time
when health timestamps matter, and opts into EEPROM persistence only when a
configuration write must survive power loss.

## Contract Changes

- `Config::enableEepromWrites` now defaults to `false`. Configuration helpers
  update the RAM mirror by default and only queue EEPROM persistence when the
  caller explicitly enables it.
- The core no longer calls Arduino `millis()` when `Config::nowMs` is unset.
  Health timestamps remain `0` without a hook; EEPROM deadlines are driven by
  the `tick(now_ms)` or poll API argument.
- EEPROM queue-full is checked before changing the RAM mirror when persistence
  is required and no queue slot is available.
- `setTimer(..., enable=true)` writes the timer count before enabling the timer.

## Steady-Path Atomic Operations

These operations are suitable for normal owner-poll use when their documented
single public call cost is acceptable:

- `readTime(DateTime&)`: one burst read of the RTC date/time block, followed by
  strict BCD and calendar validation.
- `setTime(const DateTime&)`: input validation plus one burst write of the RTC
  date/time block.
- Status and snapshot reads with no I2C: `state()`, `driverState()`,
  `isOnline()`, health counters/timestamps, `getSettings(...)`,
  `isEepromBusy()`, `getEepromStatus()`, EEPROM counters/queue depth.
- Single-read decode helpers: `readStatus()`, `readStatusFlags()`,
  `getAlarmConfig()`, `getAlarmFlag()`, `getAlarmInterruptEnabled()`,
  `getBackupSwitchMode()`, `getClkoutEnabled()`, `getClkoutFrequency()`,
  `getOffsetPpm()`, `readTemperatureC()`, and `readTimestamp(...)`.
- Low-level direct helpers are atomic only at the requested transfer level:
  `readRegister()`, `writeRegister()`, `readRegisters()`, `writeRegisters()`,
  and `readUserRam()`.

## Setup Or CLI Only

These APIs perform setup, control-plane, diagnostic, or multi-transfer work and
should stay out of TunnelMonitor's steady RTC read path:

- Lifecycle and recovery: `begin()`, `end()`, `probe()`, `recover()`.
- Alarm/timer/EVI setup: `setAlarmTime()`, `setAlarmMatch()`,
  `enableAlarmInterrupt()`, `setTimer()`, `getTimer()`, `setEviEdge()`,
  `setEviDebounce()`, `setEviOverwrite()`, `getEviConfig()`.
- EEPROM-backed control: `setBackupSwitchMode()`,
  `setPrimaryBatteryBackupDefaults()`, `setClkoutEnabled()`,
  `setClkoutFrequency()`, `setOffsetPpm()`.
- Flag and validity control: `clearStatus()`, `clearAlarmFlag()`,
  `clearPowerOnResetFlag()`, `clearVoltageLowFlag()`,
  `clearBackupSwitchFlag()`, `readValidity()`, `resetTimestamp()`.
- Multi-transfer or staged helper surfaces: `writeUserRam()` and the budgeted
  job/poll APIs. The companion poll-chunking audit owns exact sequencing and
  transaction-budget behavior for those APIs.

## Convenience Only

- `readUnix()` and `setUnix()` are conversion wrappers over `readTime()` and
  `setTime()`.
- Value-returning `getSettings()` is a convenience wrapper over
  `getSettings(SettingsSnapshot&)`.
- Static helpers such as BCD conversion, Unix/date conversion, weekday
  calculation, and `parseBuildTime()` are local conversion utilities.

## Raw And Integer Helpers

No new raw or integer offset/temperature helpers were added in this audit. The
current float helpers remain setup/diagnostic conveniences; TunnelMonitor does
not need them on the steady path unless this driver becomes a hard dependency
there.
