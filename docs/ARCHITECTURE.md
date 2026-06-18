# RV3032-C7 Driver Architecture

This document records the maintained driver contract. Keep it concise; detailed
API comments live in the public headers.

## Model

The driver is a managed synchronous I2C driver:

- The application owns I2C and supplies callbacks through `Config`.
- The library never touches `Wire`, pins, bus locks, or bus timeout registers.
- Public I2C APIs are synchronous and bounded by the caller's transport timeout.
- EEPROM persistence is asynchronous and advanced by `tick()` or
  `pollEeprom()`.
- Recovery is manual through `recover()`; `tick()` does not auto-recover.

## Lifecycle

```cpp
Status begin(const Config& cfg);
void tick(uint32_t nowMs);
void end();
```

`begin()` validates config, probes address `0x51`, applies the requested RAM
mirror configuration, and enters `READY` on success.

`tick(nowMs)` advances EEPROM persistence only. It performs at most one I2C
instruction per call and returns immediately when uninitialized or offline.

`end()` clears runtime state and returns to `UNINIT`.

## Driver State

| State | Meaning |
|---|---|
| `UNINIT` | `begin()` has not succeeded or `end()` was called. |
| `READY` | Operational; no consecutive tracked failures. |
| `DEGRADED` | One or more consecutive tracked failures below `offlineThreshold`. |
| `OFFLINE` | Consecutive tracked failures reached `offlineThreshold`; normal public I2C is blocked until `recover()`. |

Transitions:

- `begin()` success -> `READY`
- `end()` -> `UNINIT`
- first tracked I2C failure in `READY` -> `DEGRADED`
- tracked success in `DEGRADED` or `OFFLINE` -> `READY`
- failures reach `offlineThreshold` -> `OFFLINE`

## Transport Layers

All normal I2C goes through these layers:

```text
Public API
  -> register helpers: readRegs/writeRegs
  -> tracked wrappers: _i2cWriteReadTracked/_i2cWriteTracked
  -> raw wrappers: _i2cWriteReadRaw/_i2cWriteRaw
  -> Config transport callbacks
```

Health is updated only inside tracked wrappers. Public methods and register
helpers do not call health update directly.

Raw wrappers are used for diagnostics such as `probe()`. `probe()` does not
update health.

## Health Tracking

Tracked I2C results update:

- `lastOkMs`
- `lastErrorMs`
- `lastError`
- `consecutiveFailures`
- `totalFailures`
- `totalSuccess`

Config errors, parameter validation errors, invalid date/time decode results,
and `NOT_INITIALIZED` precondition failures do not update health.

If `Config::nowMs` is unset, health timestamps remain `0`. EEPROM deadlines use
the `nowMs` argument passed to `tick()` or `pollEeprom()`.

## Instruction Budget

An I2C instruction is one backend transport operation:

- one register read
- one register write
- one contiguous block read or write
- one time/calendar burst read or write
- one `EECMD` command write

Repeated-start register reads are atomic and are never split across polls.

## Atomic Steady Operations

These operations are intended to remain compact:

- `readTime()` - one burst read of `0x01..0x07`
- `setTime()` - one burst write of `0x01..0x07`
- `readTimestamp()` - one timestamp block read
- `readRegister()` / `writeRegister()`
- bounded `readRegisters()` / `writeRegisters()`
- `readUserRam()`
- snapshot and health getters with no I2C

## Budgeted Operations

Use budgeted APIs when the caller needs explicit instruction accounting:

- EEPROM persistence: `tick()` or `pollEeprom(nowMs, maxInstructions, used)`
- timer setup: `startSetTimerJob()` then `pollJob()`
- simple masked control RMW: `startRegisterUpdateJob()` then `pollJob()`
- user RAM writes over the low-level write limit: `startWriteUserRamJob()` then
  `pollJob()`

EEPROM persistence always caps itself at one I2C instruction per poll call even
when `maxInstructions` is larger than one.

## Setup And Control Helpers

The following public APIs are setup/control-plane convenience helpers and may
perform multiple synchronous I2C instructions:

- alarm setup and alarm interrupt helpers
- timer get/set convenience helpers
- EVI setup helpers
- status and validity clear helpers
- EEPROM-backed config setters when persistence is enabled
- `recover()`
- large `writeUserRam()` calls

They are safe and bounded, but not the preferred steady RTC polling surface when
an owner task needs strict instruction budgets.

## EEPROM Policy

`Config::enableEepromWrites` defaults to `false`.

With EEPROM writes disabled, configuration helpers update the RAM mirror only.
When enabled, changed EEPROM-backed configuration values queue persistence and
complete through the EEPROM state machine. Queue-full is reported before the RAM
mirror is changed.

The state machine:

1. reads and saves `Control 1`
2. sets `EERD`
3. writes `EEADDR`
4. writes `EEDATA`
5. waits for pre-command `EEbusy=0`
6. writes one `EECMD`
7. waits for post-command `EEbusy=0`
8. checks `EEF`
9. restores `Control 1`

All waits are deadline-based; the library does not use delay loops.
