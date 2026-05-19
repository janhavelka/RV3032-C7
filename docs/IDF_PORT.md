# RV3032-C7 ESP-IDF v6.0.1 Port Audit

Last audited: 2026-05-17

Status update: the first ESP-IDF port implementation was added on
2026-05-19. Keep this audit as historical context and see
`docs/IDF_PORT_IMPLEMENTATION.md` for resolved items, validation, and remaining
hardware checks.

Scope: documentation-only readiness audit for a future ESP-IDF port. The current code remains an Arduino/PlatformIO library.

Official ESP-IDF references:
- I2C master driver: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html
- Build system and components: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html
- ESP-IDF v6 peripheral migration notes: https://docs.espressif.com/projects/esp-idf/en/release-v6.0/esp32c3/migration-guides/release-6.x/6.0/peripherals.html

## Current State

- Public API is in `include/RV3032/`; implementation is in `src/RV3032.cpp`.
- `library.json` and `platformio.ini` are Arduino/PlatformIO-only today.
- The driver already uses injected I2C callbacks from `Config`; library code does not call `Wire` directly.
- The RV3032-C7 I2C address is fixed at `0x51`.
- The driver supports BCD time/date, Unix conversion, status/flag handling,
  clock integrity checks, alarm, countdown timer, CLKOUT, offset calibration,
  backup switch configuration, timestamp/EVI registers, user RAM, direct
  register access, and health tracking.
- EEPROM write/update/fill/commit machinery exists as internal managed behavior
  and cached status/queue diagnostics. Do not document new public EEPROM
  read/write helpers in the IDF port unless the public API actually exposes
  them.
- The driver preserves reserved bits during register updates where required by the command table and helpers.

## Arduino Dependencies

- `src/RV3032.cpp` includes `<Arduino.h>` only for the fallback `_nowMs()` implementation that calls `millis()`.
- `include/RV3032/Config.h` exposes framework-neutral callbacks:
  - `i2cWrite`
  - `i2cWriteRead`
  - `nowMs`
  - `timeUser`
- `examples/common/I2cTransport.h`, `I2cScanner.h`, `BoardConfig.h`, `CommandHandler.h`, and `Log.h` are Arduino example glue. They use `Wire`, `Serial`, `String`, GPIO helpers, and Arduino timing.
- `platformio.ini` builds Arduino examples and native tests. It is not an IDF project file.
- `library.json` declares only the Arduino framework and must remain the PlatformIO package manifest.
- `include/RV3032/Version.h` is generated from `library.json`; do not edit it by hand.

## Portability Blockers

1. `src/RV3032.cpp` cannot compile as a pure ESP-IDF component while it includes `<Arduino.h>`.
2. The default time fallback is Arduino `millis()`. ESP-IDF must either require `Config.nowMs` or provide a private `esp_timer_get_time()` fallback.
3. There is no `CMakeLists.txt` for `idf_component_register`.
4. There is no ESP-IDF I2C adapter using the v6 new master driver.
5. RTC GPIO concerns are not part of the library yet. INT, CLKOUT, and EVI pin configuration must remain application/example code.
6. The Arduino examples cannot be compiled by ESP-IDF and should not be pulled into the component target.

## Exact Files and APIs to Change

- `src/RV3032.cpp`
  - Replace direct `<Arduino.h>` usage with a private platform/timing shim.
  - Keep EEPROM wait and commit paths deadline-driven through existing `tick()` behavior.
  - Keep all register access through existing I2C `readRegs()` and `writeRegs()` helpers.
  - Do not add ESP-IDF I2C bus handles, pins, GPIO interrupts, or board policy to the driver class.
- `include/RV3032/Config.h`
  - No API break is required for the first IDF port.
  - Keep `i2cWrite`, `i2cWriteRead`, `i2cUser`, `nowMs`, and `timeUser` as the public portability boundary.
  - Update comments during the actual port if they still imply Arduino-only timing fallback.
- New private shim, for example `src/PlatformTime.h`
  - Under Arduino, call `millis()`.
  - Under ESP-IDF, call `esp_timer_get_time() / 1000`.
  - Keep this file private; public headers must stay framework-neutral.
- New ESP-IDF example files, for example `examples/idf/basic/main/main.cpp`
  - Own the I2C bus, device handle, optional INT/EVI GPIOs, and task timing.
  - Fill `Config` with IDF adapter callbacks and time callback.
  - Call `tick()` until EEPROM operations complete.
- New CMake files
  - Add a component-level `CMakeLists.txt`.
  - Add example project CMake files only under an IDF example directory.

## Dual Arduino and ESP-IDF Architecture

- Keep the RV3032 core as a framework-neutral C++17 component.
- Keep Arduino-only adapters in `examples/common/`; they are not part of the library.
- Add IDF-only adapters under an IDF example or an optional `extras/idf/` style directory.
- The library must never own the ESP-IDF I2C bus, pins, pullups, clock speed, INT pin, EVI pin, or CLKOUT input routing.
- The driver configures RV3032 registers only. ESP32 GPIO configuration for INT, EVI, or CLKOUT belongs in the IDF application or example.
- EEPROM operations remain manual and deadline-driven. Applications must call `tick()` until the driver reports the EEPROM operation is no longer busy.
- The application owns bus lifetime:
  - create an `i2c_master_bus_handle_t`;
  - add an `i2c_master_dev_handle_t` for `0x51`;
  - pass an adapter context through `Config.i2cUser`;
  - destroy handles after `driver.end()` and after all driver calls stop.
- Do not call public driver APIs from an ISR. If INT or EVI is wired to a GPIO, notify a task and call the driver there.

## IDF Transport Adapter Contract

Use ESP-IDF v6.0.1's new I2C master driver only:

```cpp
#include <driver/i2c_master.h>
```

The adapter should provide the existing callback shape:

```cpp
Status idfWrite(uint8_t addr,
                const uint8_t* data,
                size_t len,
                uint32_t timeoutMs,
                void* user);

Status idfWriteRead(uint8_t addr,
                    const uint8_t* txData,
                    size_t txLen,
                    uint8_t* rxData,
                    size_t rxLen,
                    uint32_t timeoutMs,
                    void* user);
```

Expected behavior:

- `addr` is a 7-bit address and should be `0x51`.
- `idfWrite()` calls `i2c_master_transmit()`.
- `idfWriteRead()` calls:
  - `i2c_master_receive()` when `txLen == 0`;
  - `i2c_master_transmit()` when `rxLen == 0`;
  - `i2c_master_transmit_receive()` for register reads.
- `timeoutMs` is passed as the ESP-IDF transfer timeout in milliseconds.
- Clamp or reject `timeoutMs` before passing it to ESP-IDF's signed
  `xfer_timeout_ms`; never allow overflow to become `-1` because `-1` waits
  forever.
- Map `ESP_OK` to `Err::OK`.
- Map `ESP_ERR_TIMEOUT` to `Err::I2C_TIMEOUT`.
- Map `ESP_ERR_INVALID_ARG` to `Err::INVALID_PARAM`.
- Map `ESP_ERR_INVALID_RESPONSE` to an I2C NACK-related status. The simple
  ESP-IDF master APIs do not expose address-vs-data phase, so use
  `Err::I2C_ERROR` with `Status.detail = ESP_ERR_INVALID_RESPONSE` unless a
  custom adapter can prove the NACK phase.
- Map address/data NACKs to `Err::I2C_NACK_ADDR` or `Err::I2C_NACK_DATA` only when the adapter can distinguish them. Otherwise use `Err::I2C_ERROR` and place the raw `esp_err_t` value in `Status.detail`.
- Do not register `i2c_master_register_event_callbacks()` on the handle used by
  these callbacks unless the adapter waits for transfer completion before
  returning. The driver expects callbacks to be complete and blocking.
- Adapter setup failures such as bus creation or device-add failure occur before `begin()` and should be reported by the example/application, not hidden inside the driver.

## CMake and Component Plan

Minimal core component:

```cmake
idf_component_register(
  SRCS "src/RV3032.cpp"
  INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
```

If the private timing shim uses ESP-IDF fallback APIs, add private requirements:

```cmake
idf_component_register(
  SRCS "src/RV3032.cpp"
  INCLUDE_DIRS "include"
  PRIV_REQUIRES esp_timer
)
```

If an IDF adapter is built into an example component, that example should declare:

```cmake
idf_component_register(
  SRCS "main.cpp" "IdfI2cTransport.cpp"
  INCLUDE_DIRS "."
  REQUIRES RV3032-C7 esp_driver_i2c esp_timer esp_driver_gpio
)
```

The component name above assumes the repository directory remains
`RV3032-C7`. If the port renames or copies the component directory to `RV3032`,
update the `REQUIRES` value accordingly. `esp_driver_gpio` is only for optional
INT/EVI/CLKOUT GPIO setup in examples; do not require GPIO from the core
component unless the core starts using ESP-IDF GPIO APIs, which it should not.

Do not compile `examples/common/*.h` into ESP-IDF targets.

## IDF and Arduino Example Plan

Arduino examples:

- Keep existing examples using `examples/common/BoardConfig.h` and `I2cTransport.h`.
- Do not replace Arduino `Wire` examples with IDF code.

ESP-IDF examples:

- Add `examples/idf/basic` with a normal ESP-IDF `main` component.
- Configure SDA, SCL, pullups, and bus frequency in the example only.
- Use `i2c_new_master_bus()`, `i2c_master_bus_add_device()`, and the callback adapter.
- Build an `RV3032::Config`, set callbacks, timeout, and `nowMs`.
- Call `begin(config)`, read current time, set time if explicitly requested, and check validity flags.
- Demonstrate alarm or countdown timer with GPIO notification only in the example task.
- If public EEPROM operations are added or exposed, demonstrate them by starting
  the operation and calling `tick()` until it completes. Otherwise show only the
  existing public EEPROM status/queue diagnostics.
- Print results with ESP-IDF logging from the example, not from the library.

## Test and Validation Plan

- Compile the existing Arduino PlatformIO environments.
- Compile native tests to preserve framework-neutral behavior.
- Add an ESP-IDF example build for ESP32-S2 and ESP32-S3.
- Hardware smoke test fixed address `0x51`.
- Verify BCD time/date round trips and invalid-date rejection.
- Verify Unix conversion across the supported 2000-2099 range.
- Verify status flags are explicit and are not silently cleared.
- Verify alarm and timer register configuration preserves reserved bits.
- Verify CLKOUT register configuration without configuring ESP GPIO inside the library.
- Verify timestamp/EVI reads if the board exposes the pin.
- Verify any public EEPROM operations that exist after the port. If no public
  EEPROM write/update/fill/commit API is exposed, verify only status/queue
  diagnostics and internal deadline behavior through targeted tests.
- Verify EEPROM wear-sensitive operations are not run by default examples.
- Verify health transitions on injected timeout/NACK and recovery on later success.

## ESP-IDF v6.0.1 Hazards

- Do not include `<driver/i2c.h>` or use legacy APIs such as `i2c_param_config()`, `i2c_driver_install()`, `i2c_cmd_link_create()`, or command-link transactions.
- Use `<driver/i2c_master.h>` and the `esp_driver_i2c` component dependency.
- IDF builds commonly surface warnings that Arduino builds hide. Keep casts explicit for enum, integer-width, `size_t`, and signed/unsigned conversions.
- Avoid global constructors that create ESP-IDF bus handles before the scheduler/runtime is initialized.
- `esp_timer_get_time()` returns microseconds as `int64_t`; down-convert deliberately for existing wrap-safe `uint32_t` logic.
- Do not add blocking `vTaskDelay()` calls inside the core driver for EEPROM wait paths.
- GPIO ISR handlers must not perform I2C transactions; use task notification or a queue.
- EEPROM write enable timing and timeout requirements must stay explicit. Do not hide EEPROM writes behind normal read APIs.

## Ordered Checklist

1. Add a private timing shim and remove direct `<Arduino.h>` use from `src/RV3032.cpp`.
2. Add a minimal component `CMakeLists.txt` for the core library.
3. Add an IDF I2C adapter using `<driver/i2c_master.h>` outside the core driver.
4. Add optional IDF INT/EVI/CLKOUT GPIO examples outside the library.
5. Add `examples/idf/basic` with bus setup, adapter callbacks, timing callback, and a polling task.
6. Build with ESP-IDF v6.0.1 for ESP32-S2 and ESP32-S3.
7. Run Arduino and native builds to confirm existing users are unaffected.
8. Run hardware tests for probe, time/date, flags, alarm, timer, CLKOUT, timestamp, EEPROM, and fault injection.
9. Update README and changelog only when the actual IDF port is implemented, not during this audit.
