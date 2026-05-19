# RV3032-C7 ESP-IDF Port Implementation Notes

Date: 2026-05-19.
Branch: `feature/rv3032-idf-port`.

## Scope

- Kept `include/RV3032/` and `src/RV3032.cpp` as a framework-neutral driver
  core with application-owned I2C callbacks.
- Removed the pure-IDF blocker by replacing unconditional Arduino timing with
  explicit Arduino/native-test and ESP-IDF fallbacks.
- Added ESP-IDF component metadata and a native IDF entry point for the full
  bring-up CLI.
- Added an example-only ESP-IDF compatibility layer so the Arduino and ESP-IDF
  examples share one command implementation.

## Files Added

- `CMakeLists.txt`
- `idf_component.yml`
- `examples/common/IdfArduinoCompat.h`
- `examples/common/Arduino.h`
- `examples/common/Wire.h`
- `examples/espidf_basic/CMakeLists.txt`
- `examples/espidf_basic/main/CMakeLists.txt`
- `examples/espidf_basic/main/main.cpp`

## Core Platform Resolution

- All RV3032 I2C access still goes through `Config::i2cWrite` and
  `Config::i2cWriteRead`.
- `_nowMs()` uses `Config::nowMs` when supplied, Arduino `millis()` in
  Arduino/native-test builds, and `esp_timer_get_time() / 1000` in ESP-IDF
  builds.
- The core component declares `esp_timer` only for the private fallback. It does
  not own the I2C bus or configure GPIO.

## Example Strategy

The Arduino CLI remains the source of truth. The ESP-IDF example sets
`RV3032_EXAMPLE_PLATFORM_IDF=1`, includes `examples/common/IdfArduinoCompat.h`,
defines `Serial` and `Wire`, then includes
`examples/01_basic_bringup_cli/main.cpp`.

The compatibility shim provides:

- `millis`, `micros`, `delay`, `delayMicroseconds`, and `yield`
- GPIO helpers used by bus recovery helpers
- a fixed-capacity `String` subset used by the CLI parser
- a nonblocking stdin/stdout `Serial` replacement
- a `TwoWire`-shaped adapter backed by ESP-IDF v6 `driver/i2c_master.h`

The shim is example-only and is not part of the public driver API.

## Remaining Hardware Checks

- Build the IDF example for `esp32s3` and `esp32s2` with a real ESP-IDF v6.0.1
  toolchain.
- Run scan/probe, disconnected-device timeout, time/date, validity flags,
  alarm/timer, CLKOUT, offset, user RAM, EEPROM status, backup PMU, recovery,
  selftest, stress, and mixed-stress flows on target hardware.
- Verify INT/EVI/CLKOUT GPIO handling in application code; the core driver
  intentionally does not configure those ESP32 pins.

## Verification

Completed locally:

- `python -m platformio test -e native`: passed, 30 tests.
- `python -m platformio run -e esp32s3dev`: passed.
- `python -m platformio run -e esp32s2dev`: passed.
- `python tools/check_cli_contract.py`: passed.
- `python tools/check_core_timing_guard.py`: passed.
- `python scripts/generate_version.py check`: passed.
- `doxygen Doxyfile`: completed.
- `git diff --check`: passed before the final verification pass.

Pending:

- `idf.py build` for `examples/espidf_basic`. `idf.py` was not available on
  PATH in this shell.
