# ESP-IDF Port Implementation

The driver core remains portable by requiring applications to inject transport and timing callbacks. When `Config::nowMs` is null, health timestamps are `0`; framework time sources belong in examples or application glue.

The native ESP-IDF example owns only example-local resources:

- `i2c_new_master_bus`, one persistent RV3032 device handle,
  `i2c_master_transmit`, `i2c_master_transmit_receive`
- `esp_timer_get_time()` through `Config::nowMs`
- `vTaskDelay()` for the CLI loop
- fixed command buffers for console input

The Arduino example and ESP-IDF example share a command contract, not
implementation source.

The native IDF dispatcher implements the same key command surface locally. It
does not include Arduino sources and does not use Arduino console, I2C, or
compatibility APIs.

Mutating commands are guarded by explicit confirmation. A command without a
final `confirm` token performs a dry run only and prints the planned change, its
volatile or persistent scope, the reason for the guard, and the exact confirmed
form. This guard applies to time, alarm, timer, CLKOUT, offset, backup, status
clear, timestamp reset, user RAM write, and direct register write commands.

Persistent changes are hardware-risky because they can queue EEPROM commits,
consume EEPROM endurance, alter backup retention behavior, drive CLKOUT, or
change calibration across power cycles. Volatile changes are also guarded when
they alter RTC state, flags, interrupts, timestamps, or scratch RAM.

Hardware validation for `examples/espidf_basic` is pending on real ESP32-S2/S3
hardware with an RV3032-C7 device. Current validation is static/source-level:
native boundary checks, command-surface checks, and repository contract checks.
