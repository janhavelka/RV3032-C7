# RV3032 ESP-IDF Component, CI, And Integration Report

Branch: `hardening/rv3032-industry-readiness`

Date: 2026-05-30

## What Changed

- Added a root ESP-IDF component registration in `CMakeLists.txt` that builds
  only `src/RV3032.cpp` and exposes `include/`.
- Added generated `idf_component.yml` metadata, synchronized from
  `library.json` by `scripts/generate_version.py`.
- Added `examples/esp_idf/basic`, a native ESP-IDF diagnostic bring-up example
  using application-owned I2C bus/device handles, a mutex, `esp_timer_get_time()`,
  injected RV3032 transport callbacks, and task-context `tick()` scheduling.
- Added `tools/check_idf_example_contract.py` to keep the ESP-IDF example free of
  Arduino/Wire/example-helper dependencies and to verify component metadata.
- Strengthened CI to run generated metadata checks, core guard checks, CLI and
  ESP-IDF example contract checks, PlatformIO builds, native tests, and an
  ESP-IDF 5.4 build matrix for `esp32s3` and `esp32s2`.
- Updated README and `docs/IDF_PORT.md` with native ESP-IDF usage and validation
  commands.
- Added `docs/PRODUCTION_BUS_MANAGER.md` for production ownership, locking,
  recovery, scheduling, and ISR guidance.
- Updated `.gitignore` for ESP-IDF local build products such as `build/`,
  `managed_components/`, and `sdkconfig`.
- Marked the original industry-readiness report as a historical baseline so it
  does not contradict later chunk reports.

## Public API Changes

No RV3032 public C++ API was changed in this chunk.

New repository/build surface:

- `CMakeLists.txt`
- `idf_component.yml`
- `examples/esp_idf/basic`
- `tools/check_idf_example_contract.py`
- `docs/PRODUCTION_BUS_MANAGER.md`

## Tests And Guards Added

- `tools/check_idf_example_contract.py` verifies:
  - Native ESP-IDF example project files exist.
  - The example contains `app_main`, ESP-IDF I2C master setup, injected
    `RV3032::Config` callbacks, `tick()`, and `end()`.
  - The example does not depend on Arduino, Wire, Serial, `examples/common`, or
    the existing CLI example.
  - The root ESP-IDF component CMake builds only the core source and does not
    pull in platform driver dependencies.
  - `idf_component.yml` version matches `library.json` and requires ESP-IDF
    `>=5.4`.

## Checks Run

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | Passed |
| `python tools/check_cli_contract.py` | Passed |
| `python tools/check_idf_example_contract.py` | Passed |
| `python scripts/generate_version.py check` | Passed; `Version.h` and `idf_component.yml` up to date |
| `python -m platformio test -e native` | Passed; 67/67 native tests |
| `python -m platformio run -e esp32s3dev` | Passed |
| `python -m platformio run -e esp32s2dev` | Passed |
| `idf.py -C examples/esp_idf/basic set-target esp32s3 build` | Not run locally; `idf.py` is not on PATH in this shell |
| `idf.py -C examples/esp_idf/basic set-target esp32s2 build` | Not run locally; `idf.py` is not on PATH in this shell |

## CI Coverage

`.github/workflows/ci.yml` now keeps the existing PlatformIO coverage and adds an
ESP-IDF example build job using `espressif/idf:release-v5.4` for:

- `esp32s3`
- `esp32s2`

This CI job is the intended build proof for the native ESP-IDF example when the
workflow runs on GitHub. Local `idf.py` validation still requires an ESP-IDF shell
or PATH export.

## Deferred To Later Chunks

- Hardware validation on real ESP32-S2/ESP32-S3 boards with documented pull-ups,
  supply setup, bus speed, backup supply, INT behavior, and fault flags.
- Production shared-bus example with multiple devices and an application recovery
  loop.
- High-level ESYN arm/cancel APIs and validation.
- Additional dirty-state diagnostics for timer/alarm/control-register workflows
  beyond the diagnostics already added in earlier chunks.
