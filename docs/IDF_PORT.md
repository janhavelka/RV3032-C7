# RV3032-C7 ESP-IDF Port Notes

Date: 2026-05-19.
Branch: `feature/rv3032-idf-port`.

## Current Status

- ESP-IDF support is implemented for the current port branch.
- The driver core is framework-neutral at the I2C boundary:
  all bus access still goes through `Config::i2cWrite` and
  `Config::i2cWriteRead`.
- Root `CMakeLists.txt` and `idf_component.yml` expose the driver as an
  ESP-IDF component.
- `examples/espidf_basic` reuses the same colored bring-up CLI source as the
  Arduino example, so command structure, help depth, diagnostics, and output
  style stay aligned across frameworks.

## Architecture

- The library does not own the ESP-IDF I2C bus, SDA/SCL pins, pull-ups, bus
  clock, INT pin, EVI pin, or CLKOUT routing. Those remain application/example
  responsibilities.
- The core driver configures only RV3032 registers.
- Timing uses the existing `Config::nowMs` hook or guarded platform fallback.
- Arduino-only helpers remain under `examples/common/`; ESP-IDF glue remains
  example-local.
- EEPROM operations remain deadline-driven. Applications must keep calling
  `tick()` until pending operations complete.

## ESP-IDF Example

- Entry point: `examples/espidf_basic/main/main.cpp`
- Shared CLI source: `examples/01_basic_bringup_cli/main.cpp`
- Example compatibility layer: `examples/common/IdfArduinoCompat.h`

The shared CLI covers the same flows in both frameworks:

- version/help/settings output;
- I2C scan/probe/recover/health diagnostics;
- time/date read and set flows;
- Unix conversion checks;
- alarm/countdown/CLKOUT/register/user-RAM flows exposed by the example;
- self-test, stress, and read/write style validation helpers.

## ESP-IDF v6 Notes

- Use `<driver/i2c_master.h>` and the `esp_driver_i2c` component dependency in
  ESP-IDF applications/examples.
- Do not include `<driver/i2c.h>` or use legacy command-link APIs.
- Do not call public driver APIs from GPIO ISR handlers; notify a task and call
  the driver there.
- Keep INT/EVI/CLKOUT GPIO setup in the application or example, not the core.

## Validation

Completed during the implementation pass:

- Arduino PlatformIO example builds.
- Native/unit tests where present.
- Static shared-CLI contract checks.

Still required before production hardware release:

- `idf.py build` for `examples/espidf_basic` on the target ESP-IDF version.
- Hardware smoke test at fixed RV3032 I2C address `0x51`.
- BCD time/date, alarm/timer, CLKOUT, EVI/timestamp, and EEPROM timing checks
  on real hardware.
