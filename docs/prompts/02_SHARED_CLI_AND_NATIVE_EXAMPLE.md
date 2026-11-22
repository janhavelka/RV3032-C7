# 02: Shared framework-neutral CLI engine and native ESP-IDF example

**Status: DONE.** Prerequisite: 01 is DONE, and its component, manifest
check, adapter suite and S2/S3 images pass. Next:
[03](03_CI_PACKAGE_AND_CLOSEOUT.md).

## Outcome and scope

- **Phase A.** Move the CLI from `examples/01_basic_bringup_cli/main.cpp` into
  one framework-neutral engine. The Arduino example becomes a thin shell with
  byte-identical behavior. Commit and push once the Arduino gate passes.
- **Phase B.** `examples/esp_idf/basic/main/main.cpp` becomes a native shell on
  the same engine, with full command parity, build identity, final sdkconfig
  and `tools/check_idf_example_contract.py`.

Authorized:

- `examples/01_basic_bringup_cli/main.cpp`.
- `examples/common/*`: make files neutral, and delete a helper only once
  nothing uses it. `I2cTransport.h` stays; `test/test_hil/main.cpp:9` and
  `test/test_hil_persistence/main.cpp:9` use it.
- One engine pair, for example `examples/common/Rv3032Cli.{h,cpp}`.
- `examples/esp_idf/basic/**`; `test/test_native/test_datetime.cpp`;
  `test/test_native_idf/**`.
- `platformio.ini:32-37,109-111`, only to compile the engine.
- `tools/check_portability.py:17-25,254-259` and its tests (strengthen only).
- Comments only: `tools/hil_cli_runner.py:433` (docstring) and
  `tools/test_hil_health_snapshot.py:9-10`.
- New `tools/check_idf_example_contract.py` and
  `tools/test_check_idf_example_contract.py`, named for the CI discovery at
  `ci.yml:101`.
- `tools/check_package.py:27-39`, only to track moved or new example files.
- The AGENTS repository model, `README.md:545-603`, `docs/IDF_PORT.md` and
  CHANGELOG.

Forbidden: `src/`, `include/RV3032/`, `test/test_hil*/`, `.github/`, and any
Arduino behavior change.

## Audit: Arduino-bound versus shareable [verify]

- **Bound to Arduino.**
  - `main.cpp`: includes `:16-17`; 159 `Serial.` calls; 27 `F()`; `String` in
    every handler and in the token helpers (`:359-387`).
  - `millis()` at `:58,81,309,333,338,340,1485,1644,1807,1841,1923,2004,2305,2355`;
    `micros()` at `:1817,1819`; `delay()` at `:87,341,2303,2306,2356,2368`;
    `yield()` at `:1838,2001`.
  - `ESP.*`/`psramFound()` at `:520-529`; `Wire` at `:2217,2323`;
    `setup()`/`loop()` at `:2302-2369`.
  - `CliShell.h:3,16,22-88`; `CliStyle.h:11,37-56`; `Log.h:11,22-35`;
    `I2cScanner.h:10-64`; `BoardConfig.h:17,42-44`; `CommandHandler.h:11`
    (`String` only in signatures). `I2cTransport.h` stays Arduino-only
    (`:14-16`).
- **Shareable.**
  - The pending-operation owner (`main.cpp:33-62,194-278`).
  - Tables and printers (`:64-77,93-192,283-322,407-444,987-1005`), help
    (`:449-515`), every `cmd_*` body, and dispatch (`:2190-2300`).
  - `CommandHandler.h:23-137`, `CliStyle.h:19-36`, and pins
    `BoardConfig.h:29-39`.
- **Pinned output.**
  - CLI tests in `test_datetime.cpp:8-18,10025-10948`, registered at
    `:11098-11110`; the scanner test at `:10697-10714`.
  - The HIL runner's prompt, steps and parsers: `tools/hil_cli_runner.py:22`,
    `:79-292`, `:375-484`.
- **Hazard.** ESP-IDF 6.0.1 `components/log/include/esp_log_color.h:89-99`
  defines its own `LOG_COLOR_*`. Shared code must not define those macros; use
  `constexpr` (`AGENTS.md:141`).

## Phase A: extract with no behavior change

1. **Characterization first.** Before moving code, add a host test against
   `FakeRv3032` with fixed clocks. It runs every help form, every rejection in
   `test_phase3_cli_invalid_mutating_commands_are_zero_io`, and the pending,
   EEPROM and discard paths, and records exact output and callback counts. Mask
   only build date and time. It must pass unchanged before and after.
2. **The engine owns:**
   - dispatch, help, every handler and the boot lines;
   - the single `PendingOperation` owner: one callback per pass and the 15 s
     warning;
   - the bounded line editor and discard rules (`CliShell.h:9-88`,
     `README.md:567-580`);
   - strict `const char*` token parsing with exact counts and no `String`, heap,
     `sscanf`, `atoi`, `strtol` or `sprintf` (`check_portability.py:11-25`);
   - one bounded output hook (`vsnprintf` into a fixed buffer) for all text.
   It borrows the driver. The shell keeps the bus, `Config`, `begin()` and
   `probe()`.
3. **Platform hooks**, only these:
   - write; read a byte; count received bytes;
   - `nowMs`, `nowUs`, and a yielding `sleepMs`, which the 1000 ms helper at
     `main.cpp:324-357` must still use. `sleepMs` loops until the clock
     confirms the full time, with a bounded attempt count;
   - probe a scan address, keeping `I2cScanner.h:23-64` and its timeout
     restore;
   - the platform lines of `version` (`main.cpp:517-534`).
4. **Keep identical:** names, aliases, usage and error text, every format and
   colour, the `> ` prompt, the boot sequence, the stress and selftest bounds,
   and zero I/O on rejected input. The Arduino shell keeps
   `transport::wireWrite`/`wireWriteRead`, `board::initI2c` and its 10 ms
   cadence.
5. **Tests.** Change only call sites (for example `String` to `const char*`).
   Keep every assertion, input, expected fragment, callback bound and
   `RUN_TEST`. Compile the engine in the native and both Arduino environments.
6. **Portability rules.** Make `check_portability.py:22-23` (the parallel
   tick owner and inferred job owner) independent of the receiver name, and
   add mutation probes.
7. **Gate.** Run 01's Verify list without the IDF builds, then commit, push
   and sync.

## Phase B: native shell, identity and contract checker

1. **`main/main.cpp`.** `app_main` alone owns the console, the 01 adapter and
   bus, the driver and the engine.
   - **Console.** The OPT4001 recipe
     (`..\OPT4001\examples\esp_idf\basic\main\main.cpp:1419-1450`):
     unbuffered stdio, `O_NONBLOCK`, `getchar()`, yield on EOF.
   - **Received-byte count.** Make it truthful, for example with a bounded
     non-blocking pre-read into a fixed buffer, so the discard rules match.
   - **Cadence.** The Arduino 10 ms cadence, and the loop yields at least one
     tick (`vTaskDelay(1)`) on every iteration.
   - **Hooks.**
     - `esp_timer` time and `vTaskDelay` sleep;
     - scan with `i2c_master_probe`: 50 ms, `ESP_ERR_NOT_FOUND` absent,
       `ESP_ERR_TIMEOUT` shown as `TO`;
     - chip model and revision, flash size, PSRAM when enabled,
       `esp_get_idf_version()`.
2. **Identity.** Write `main/GenerateBuildIdentity.cmake`, following INA228
   `main/CMakeLists.txt:9-31` and its `GenerateGitProvenance.cmake`. It writes
   a build-directory header with:
   - the commit from plain `git rev-parse --short HEAD`, exactly as the
     Arduino identity does (`scripts/generate_version.py:128`), so the CLI and
     HIL-runner output look the same from both shells. The cross-set
     `--short=12` yields to this repo's own Arduino format here;
   - a `clean`/`dirty`/`unknown` status computed as in
     `scripts/generate_version.py:125-148`;
   - the build time.
   Both values are `unknown` unless `git -C <repo root> rev-parse
   --show-toplevel` equals the repository root. No Python. Library lines keep
   the `Version.h` values; do not fabricate `RV3032_GIT_*`.
3. **CMake.** Add `../../../common/<engine>.cpp` and the include path its
   include spelling needs. Add console components only as the build proves
   them necessary, compared with TMP1x2
   `examples/esp_idf/basic/main/CMakeLists.txt:5-9` **[verify]**. Set
   `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192`, the Arduino loop-task size
   (`CONFIG_ARDUINO_LOOP_STACK_SIZE=8192` in the pinned framework's
   `esp32s3/sdkconfig`; the IDF default is 3584, `esp_system/Kconfig:232-234`).
   The high-water mark is NOT RUN.
4. **Example README.** Include:
   - the wiring table (`BoardConfig.h:29-39`) and external pull-ups;
   - build, flash and monitor with `C:\esp\idf.cmd 6.0.1`, and the console for
     each target;
   - first commands, plus safety for mutating commands, disabled EEPROM writes
     and the primary-cell confirmation;
   - "no Arduino sources", and that a build or static contract check is not
     hardware validation.
5. **Contract checker** (like INA228's, smaller). It checks:
   - the required files;
   - wrapper SRCS equal to the root's under the name `RV3032`;
   - no `override_path` and no `main/idf_component.yml`;
   - REQUIRES;
   - no `Arduino.h`, `Wire`, `TwoWire`, `String`, `Serial`, `millis`,
     `delay(` or `F("` in the IDF example or the engine;
   - parity: both shells compile one engine, neither dispatches commands
     itself, and every command the HIL runner sends is in the engine's
     dispatch;
   - the sdkconfig values, including `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192`,
     the manifest's `idf: ">=6.0.1"`, and the README tokens.
   Add `tools/test_check_idf_example_contract.py` with a mutation case for
   each rule.
6. **Host tests.** Use the fake SDK and `FakeRv3032`, with scripted `getchar()`
   input. Cover discard cases mirroring
   `test_cli_terminal_callback_input_is_discarded_on_both_surfaces` and
   `test_cli_busy_backlog_is_discarded_after_terminal_poll`, scan mapping, the
   identity fallback, and ensure through the one-attempt adapter.
7. **Docs.**
   - `README.md:565-603`: CLI ownership and the glue list at `:598-603`.
   - AGENTS repository model; `docs/IDF_PORT.md` (example and console).
   - `hil_cli_runner.py:433`: one shared engine; only the Arduino CLI was ever
     run on hardware.
   - CHANGELOG.

## Acceptance

- The characterization test and all existing native assertions pass with no
  expectation edits.
- Both Arduino images build, the HIL-runner device-free checks pass, and the
  engine holds no Arduino token.
- Both IDF images link the shared engine.
- The checker and its probes pass, and each probe fails on its own mutation.
- Console I/O and HIL on a target stay NOT RUN.

## Verify

Run 01's Verify list, plus `python tools/check_idf_example_contract.py`. Build
IDF into clean `C:\esp\build\rv3032-<target>-6.0.1` folders. Pack, run
`python tools/check_package.py package RV3032-C7.tar.gz`, and finish with
`git diff --check`.

## Close out

Report each evidence layer and both checkpoint SHAs; remote CI and hardware
are NOT RUN. Set row 02 to DONE, commit only owned files, push and sync.
