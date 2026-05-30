# RV3032-C7 Production Bus Manager Guidance

Last updated: 2026-05-30

This driver is transport-injected and non-owning. A production application needs a
bus manager around it when the RTC shares an I2C controller, when multiple tasks
can call the driver, or when bus recovery policy matters.

## Ownership Boundary

The application or bus manager owns:

- I2C controller creation, deletion, pin routing, pull-ups, and clock speed.
- Device handle creation and removal.
- Locking for shared bus and shared driver access.
- Timeout policy used by the injected callbacks.
- Bus clear/recovery and retry scheduling.
- Logging and telemetry.
- Task scheduling and interrupt dispatch.

The RV3032 core owns only its internal state machine, cached settings, health
counters, and register-level protocol. It receives I2C access through
`Config::i2cWrite`, `Config::i2cWriteRead`, and `Config::i2cUser`.

## Locking Contract

Driver instances are not internally thread-safe. If multiple tasks can touch the
same `RV3032` object or the same I2C bus, the application must serialize access
outside the driver. The injected transport callbacks must not recursively call
back into the same driver instance.

Use one lock scope that covers the physical transaction in each callback. If a
larger workflow needs to be atomic across several public driver calls, take an
application-level workflow lock before calling the driver.

## Timeout And Recovery Contract

`Config::i2cTimeoutMs` is passed to the transport callbacks. The callback decides
how that value maps to the platform I2C timeout and lock timeout.

Do not hide repeated failures with unbounded retries inside the callback. Return
precise `Status` values instead:

- Definite address NACK: `DEVICE_NOT_FOUND` or `I2C_NACK_ADDR`
- Data NACK: `I2C_NACK_DATA`
- Transaction timeout: `I2C_TIMEOUT`
- Arbitration/stuck-bus/controller fault: `I2C_BUS`
- Unknown platform I2C failure: `I2C_ERROR`

When the driver reaches `OFFLINE`, recover the physical bus in application code,
then call `recover()`. `probe()` is diagnostic-only and does not update health.

## Scheduling Contract

Call `tick(nowMs)` regularly from task context. EEPROM persistence is advanced by
`tick()` with bounded work per call. If `Config::nowMs` is omitted, driver-owned
timestamps report `0`; elapsed EEPROM work still uses the `nowMs` value supplied
to `tick()`.

## Interrupt Contract

Public driver APIs are not ISR-safe. The RV3032 INT pin should be handled by a
minimal ISR that notifies a task. The task should call `readStatusFlags()` or
`readValidity()`, handle each asserted source, and explicitly clear only the
flags it has handled.

INT is open-drain active-low. Production hardware must provide a pull-up to the
intended rail and must account for multiple RV3032 sources sharing the same INT
line.

## Partial State Contract

Multi-step operations can leave hardware state uncertain if a later I2C
transaction fails after an earlier transaction changed the chip. The current
driver exposes diagnostics for clock/calendar uncertainty and EEPROM command
status. Later hardening chunks may add more detailed dirty-state diagnostics for
Control register read-modify-write, timer configuration, alarm configuration,
and high-level ESYN workflows.

## Hardware Validation Record

Do not claim production hardware validation without recording:

- Commit hash and date.
- ESP-IDF or PlatformIO environment version.
- Board and RV3032 module details.
- SDA/SCL pins, pull-up values, bus speed, and bus sharing topology.
- Main supply and backup supply setup.
- Test commands or firmware image used.
- Observed PORF/VLF/BSF/EEF/EEbusy/INT behavior.
- Pass/fail result and any anomalies.
