# RV3032-C7 I2C Uniformization Prompt

Repository: `RV3032-C7`

Absolute path: `C:\Users\Honza\Documents\Projects\RV3032-C7`

## Execution Rules

You are working inside this single repository. Implement this prompt directly;
do not repeat the cross-repository audit.

You may spawn subagents for read-only inspection of APIs, tests, I2C
transactions, docs, and diagnostics. Keep final judgment, edits, and
verification in the main agent.

Prefer simple, robust, readable code. Before adding code, inspect whether
existing code can be simplified, reused, tightened, or deleted.

Preserve dirty user changes. Do not commit unless explicitly asked.

## Common Uniformization Target

Apply this shared I2C library contract: injected non-owning transport, `Status` returns, cache-only `getSettings(SettingsSnapshot&) const`, active `probe()`/diagnostics named explicitly, `DriverState` with `state()` and `driverState()`, `isOnline()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`, `consecutiveFailures()`, `totalFailures()`, and `totalSuccess()`.

Keep the common `Err` vocabulary append-only where missing: `OK`, `NOT_INITIALIZED`, `INVALID_CONFIG`, `INVALID_PARAM`, `I2C_ERROR`, `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, `I2C_BUS`, `DEVICE_NOT_FOUND`, `TIMEOUT`, `BUSY`, and `IN_PROGRESS`. Preserve RV3032-specific datetime, EEPROM, register, queue, and RTC behavior.

Uniformization is not a new base class or framework. Make only local, source-compatible additions and tests.

## Current State

- Public health snapshot is in `include\RV3032\RV3032.h:210-232`; it includes `lastOkMs`, `lastErrorMs`, `consecutiveFailures`, `totalFailures`, and `totalSuccess`.
- Public lifecycle and status accessors include `probe()` at `include\RV3032\RV3032.h:314`, `recover()` at line 325, `driverState()` at line 337, `getSettings(SettingsSnapshot&)` at line 353, and health accessors at lines 369-399.
- Register helpers are exposed as `readRegister()`, `writeRegister()`, `readRegisters()`, and `writeRegisters()` at `include\RV3032\RV3032.h:881-919`.
- Raw I2C/register access is documented as diagnostic/no-health in `include\RV3032\RV3032.h:1072-1086`.
- Native tests passed 48 tests.
- No explicit HIL runner was found.
- The repository currently has a dirty worktree; preserve it.

## Best Sources To Adapt

- Use BME280/SHT3x for read-only vs active diagnostics naming: `BME280\include\BME280\BME280.h:277-344`, `SHT3x-main\include\SHT3x\SHT3x.h:340-358`.
- Use SCD41's destructive maintenance command warnings for any EEPROM-writing RTC commands: `SCD41\include\SCD41\SCD41.h:403-411`.
- Use BME280 HIL runner style only if the RTC example CLI already supports safe status, time read, alarm/status, EEPROM status, and health commands.

## Implementation Tasks

1. Preserve existing health API names; RV3032 already has `driverState()` and the common health counters.
2. Audit EEPROM-writing and register-writing APIs. Ensure each write has a bounded timeout and visible error status. EEPROM writes must not look like ordinary fast register writes in docs.
3. Confirm `getSettings()` remains cache-only. Any live status refresh must be named as an active read/probe.
4. Add explicit docs around register helpers: raw reads/writes are diagnostics, not the preferred application API for time/alarm flows.
5. Add a HIL runner only if a maintained CLI exists and the default sequence is non-destructive. If present, cover the common minimum contract: `version`, `scan`, `probe`, `settings`, `health`, failure-token classification, and dry-run/parser test support. EEPROM writes must be opt-in with an explicit confirmation or scratch address.

## API Changes Required

- None expected.

## Simplifications Before Adding Code

- Do not add a generic dirty-state API unless a concrete write path can leave cached public state divergent.
- Prefer documenting EEPROM busy/timeout behavior over adding a new manager class.

## Tests To Add Or Update

- Native tests for any EEPROM timeout or destructive-command guard changed.
- Native bus-silence test for `getSettings()`.
- HIL parser tests only if a real runner is added.

## Commands To Run

- `pio test -e native`
- `pio run -e esp32s3dev`

## Constraints And Non-Goals

- Do not add RTC ownership of shared I2C bus recovery.
- Do not run destructive EEPROM HIL by default.
- Injected transport only: no global `Wire`, new bus manager, pin ownership, or shared bus reset from the device driver.
- Preserve distinct timeout, address NACK/device-not-found, data NACK, bus, datetime, EEPROM, register, and queue statuses. Do not collapse them into generic `I2C_ERROR` or use `DEVICE_NOT_FOUND` for timeout/data/bus failures.

## Risks And Open Questions

- Open: whether RV3032 should add automated HIL now, and if so which RTC features are safe for default non-destructive validation.
