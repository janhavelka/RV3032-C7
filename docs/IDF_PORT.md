# ESP-IDF Port

The core library is framework-neutral. Public headers and `src/` do not include Arduino or ESP-IDF framework headers, and all hardware access is supplied through `Config` callbacks.

The ESP-IDF example in `examples/espidf_basic` is a native IDF application:

- entry point is `app_main()`
- I2C uses `driver/i2c_master.h`
- timestamps use an injected `Config::nowMs` callback backed by `esp_timer_get_time()`
- delays use `vTaskDelay()`
- command input uses fixed C buffers and `fgets()`
- the RV3032 device handle is kept for normal transfers instead of creating a
  transient handle per transaction

The IDF example must not include Arduino sources or use Arduino compatibility facades. `tools/check_idf_example_contract.py` enforces the native-IDF boundary and command coverage.

## Command Confirmation Policy

Read-only commands execute directly. Mutating commands require a final
`confirm` token. This includes RTC time setters, alarm changes, timer changes,
CLKOUT changes, offset calibration, backup PMU changes, status flag clears,
timestamp resets, user RAM writes, direct register writes, and EEPROM-backed
configuration changes.

Without `confirm`, the CLI prints:

- what would change
- whether the change is volatile or persistent
- why confirmation is required
- the exact confirmed command form

Examples:

```text
set 2026 01 10 15 30 00
set 2026 01 10 15 30 00 confirm

backup usual
backup usual confirm
```

Persistent commands can consume EEPROM endurance or change retained hardware
state across power cycles. Volatile commands can still affect RTC time,
interrupts, flags, timestamps, or scratch RAM until reset or a later write.

ESP-IDF hardware validation remains pending on physical ESP32-S2/S3 targets
with an RV3032-C7 attached. Treat the example as source-level and contract-level
validated until that board validation is completed.
