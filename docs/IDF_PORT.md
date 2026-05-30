# RV3032-C7 ESP-IDF Portability Status

Last audited: 2026-05-30

## Current Reality

- The core library is framework-neutral and lives in `include/` and `src/`.
- The repository root now contains an ESP-IDF component registration for the core:
  `CMakeLists.txt` builds `src/RV3032.cpp` and exposes `include/`.
- `idf_component.yml` is generated from `library.json` by `scripts/generate_version.py`
  and currently requires ESP-IDF `>=5.4`.
- PlatformIO board builds remain Arduino-based for the existing diagnostic CLI.
- The native ESP-IDF diagnostic example is `examples/esp_idf/basic`.

## ESP-IDF Adapter Requirements

A pure ESP-IDF application must provide:

1. `Config::i2cWrite`
2. `Config::i2cWriteRead`
3. `Config::i2cUser`
4. Optional `Config::nowMs` / `Config::timeUser` for health timestamps

The adapter owns bus handles, GPIO routing, pull-up decisions, mutexes, timeout
policy, recovery policy, logging, and task scheduling. The driver must not own or
initialize those resources.

## Basic Build Commands

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

If `idf.py` is not on PATH, run these from an ESP-IDF shell or source the local
ESP-IDF export script first.

## Minimal Adapter Pattern

```cpp
static uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

RV3032::Config cfg{};
cfg.i2cWrite = myI2cWrite;
cfg.i2cWriteRead = myI2cWriteRead;
cfg.i2cUser = &myBusContext;
cfg.nowMs = idfNowMs;
```

## Production Notes

- `tick(nowMs)` should be called regularly from task context.
- Public driver APIs are not ISR-safe. Interrupt handlers should notify a task,
  and that task should read and clear RV3032 flags.
- Timeout checks in the core use wrap-safe unsigned arithmetic.
- EEPROM queue behavior and READY/DEGRADED/OFFLINE transitions must remain
  unchanged in adapter code.
- Shared I2C buses must be serialized outside the driver. See
  `docs/PRODUCTION_BUS_MANAGER.md`.

## Verification Checklist

- `python tools/check_core_timing_guard.py`
- `python tools/check_cli_contract.py`
- `python tools/check_idf_example_contract.py`
- `python scripts/generate_version.py check`
- `python -m platformio test -e native`
- `python -m platformio run -e esp32s3dev`
- `python -m platformio run -e esp32s2dev`
- `idf.py -C examples/esp_idf/basic set-target esp32s3 build`
- `idf.py -C examples/esp_idf/basic set-target esp32s2 build`

Do not record the native ESP-IDF example as hardware validation unless it has
been built and run against real hardware with board, bus speed, power setup,
commit, date, and observed results documented.
