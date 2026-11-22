# 01: ESP-IDF component, committed manifest and example-owned I2C adapter

**Status: DONE.** Prerequisite: none. Next:
[02](02_SHARED_CLI_AND_NATIVE_EXAMPLE.md). Planning anchor: `e0d3f1f`.
Fetched execution base: `a1338bb13933c6ac08a721998492df52362705b3`.

## Outcome and scope

The owner elected native ESP-IDF support on 2026-11-16. This prompt records
that election; do not ask for another scope decision. It delivers:

- a root ESP-IDF component;
- a committed `idf_component.yml` protected by a content check;
- an example-owned `i2c_master` adapter with host tests through `FakeRv3032`;
- a small native bring-up app that links for esp32s3 and esp32s2.

The CLI belongs to 02. Keep every committed statement true at this checkpoint.

Authorized edits:

- `AGENTS.md:39-44`: the elected target. Keep a pointer to this set until 03.
- `AGENTS.md:58-90`: repository model. `README.md:514`: support row.
- `docs/IDF_PORT.md:3-5`, plus new sections.
- `docs/VERIFICATION.md:14-32,41-45`; `docs/README.md:15`.
- `.gitignore:54`: delete the `idf_component.yml` line only. Keep `:55`
  (`idf_component.yml.orig`) and `:57-64`.
- `library.json:65`: delete the `idf_component.yml` exclude only, and keep
  `:66` (`.orig`). Add excludes for IDF output under `examples/esp_idf/basic/`:
  `build*/**`, `managed_components/**`, `dependencies.lock`, `sdkconfig`,
  `sdkconfig.old` (README rule 3).
- `tools/check_package.py:44,170`: drop only the `idf_component.yml` entries,
  keeping `idf_component.yml.orig` excluded and package-forbidden. Add the
  content check, and update `tools/test_check_contracts.py:179-259` to match.
  Keep the permanent promptset exception (`:70-82`) and its test.
- `platformio.ini:94-111`: the new host suite and fake-SDK include only.
- `CHANGELOG.md` `[Unreleased]`.

New paths: `CMakeLists.txt`, `idf_component.yml`,
`examples/esp_idf/basic/{CMakeLists.txt, sdkconfig.defaults*,
components/RV3032/CMakeLists.txt, main/CMakeLists.txt, main/main.cpp,
main/Rv3032IdfI2c.{h,cpp}}`, `test/fake_idf/**`, `test/test_native_idf/**`.

Forbidden:

- `src/` and `include/RV3032/`, except a minimal fix for a warning an IDF
  build proves fatal. That fix needs native tests and a CHANGELOG Fixed entry.
  Never hand-edit `Version.h`.
- `examples/01_basic_bringup_cli/`, `examples/common/`, `test/test_hil*`.
- `.github/` (03).

## Audit before editing

- **Contract.**
  - `AGENTS.md`; `..\AGENTS.md:3-38`.
  - `include/RV3032/Config.h:35-168`. `:63-65`: scoped single-attempt reads
    during `ensurePrimaryCellConfiguration()`. `i2cTimeoutMs` is 1..100
    (`:123-131`), the primary-cell timeout 1..5 (`:161-167`).
  - `src/RV3032.cpp:2534-2549`: a status outside the domain becomes
    `TRANSPORT_CONTRACT_VIOLATION`. `:2565-2569,2589-2592`: zero timeouts are
    never dispatched.
  - `docs/IDF_PORT.md:7-67,119-148`.
- **References here.**
  - `examples/common/I2cTransport.h:59-288`, the validated Wire adapter: one
    hard exclusive deadline per callback, the closed status domain, and a short
    read reported as `I2C_ERROR`.
  - `test/test_native/FakeRv3032.h:381-476,508-521`.
  - The owner read-retry timing test, `test_datetime.cpp:7396-7557`. It
    covers the contract's optional retry, which this adapter does not use.
- **pioarduino 55.03.311** (`platformio.ini:6`) **[verify]**. The PlatformIO core
  on this PC is `C:\pio` (`PLATFORMIO_CORE_DIR`):
  `C:\pio\platforms\espressif32@src-f8b6dfa6ef8ac4595c2269bd049911b5\builder\frameworks\component_manager.py:284-311,313-326,368-383`,
  called from `arduino.py:489-495`. Confirm the platform directory with
  `.\scripts\pio.cmd run -e esp32s3dev -v`. When the framework manifest is missing, it
  backs up `$PROJECT_SRC_DIR/idf_component.yml` to `.orig` and re-dumps it with
  `yaml.dump`. With `src_dir = .` (`platformio.ini:2`) that file is our root
  manifest.
- **ESP-IDF 6.0.1** `components/esp_driver_i2c/` **[verify]**. Cite each point
  in `docs/IDF_PORT.md`.
  - `xfer_timeout_ms` is an `int`; `-1` waits forever
    (`include/driver/i2c_master.h:167,208`). `pdMS_TO_TICKS`
    (`i2c_master.c:675,1003`) turns a value below one tick into zero ticks.
  - The timeout applies to each wait: the bus lock (`:1004`), then each
    command batch (`:535`).
  - Every non-DONE end returns `ESP_ERR_INVALID_RESPONSE` (`:720-723`): NACK
    and timeouts alike, with no phase or byte count. `ESP_ERR_TIMEOUT` comes
    from the lock.
  - After an error the SDK resets the FSM (`:1028`). The next start resets it
    and clears the bus after a timeout or while the bus is busy (`:678-679`).
    This is SDK behavior before the next transaction, not a replayed write;
    document it.
  - `i2c_master_probe()` maps an address NACK to `ESP_ERR_NOT_FOUND`
    (`:1392`). The adapter never calls `i2c_master_bus_reset()`
    (`:1241-1247`).
- **Siblings, read-only.** Record path, revision, and what you adopted or
  rejected.
  - INA228 `examples/esp_idf/basic/main/Ina228IdfI2cTransport.cpp:107-171`:
    adopt its lifecycle and cleanup-failure retention. Reject its
    out-of-domain `INVALID_PARAM` and `NACK_UNKNOWN_PHASE`.
  - SCD41 `examples/idf/basic/components/SCD41/CMakeLists.txt:1-10`: the
    wrapper.
  - TMP1x2 and OPT4001 `examples/esp_idf/basic/sdkconfig.defaults*`.

## Implement

1. **Component.** `CMakeLists.txt` calls
   `idf_component_register(SRCS "src/RV3032.cpp" INCLUDE_DIRS "include")` and
   sets `cxx_std_17`. No REQUIRES: the core uses only standard headers
   (`src/RV3032.cpp:6-9`).
2. **Manifest.** `idf_component.yml` holds:
   - `version` equal to `library.json`, a description, `license: "MIT"`;
   - maintainer `"Jan Havelka (THYMOS Solution s.r.o.) <info@thymos.cz>"`;
   - GitHub `url`/`repository`; targets esp32s2 and esp32s3;
   - `dependencies: idf: ">=6.0.1"`;
   - `files.exclude` mirroring `library.json` plus IDF output.
3. **Content check.** Use the standard library only; `validate-library`
   installs only PlatformIO (`ci.yml:91-92`). Run it in `source` and
   `package` modes. Require:
   - the exact quoted version, idf and target lines, with the version from
     the same tree's `library.json`;
   - no `idf_component.yml.orig`. A `.orig` means pioarduino rewrote the file,
     so report how to restore it from Git.
   Add mutation tests: a missing or edited line, an unquoted re-dumped copy, a
   version mismatch and a `.orig`. Packages still reject `docs/prompts/**`.
4. **Example project.**
   - Top `CMakeLists.txt` as in OPT4001: project `rv3032_basic`, no
     `EXTRA_COMPONENT_DIRS`.
   - `components/RV3032/CMakeLists.txt`: the root's SRCS and include under the
     fixed name `RV3032`, with SCD41's explanatory comment.
   - `main/CMakeLists.txt`: `REQUIRES RV3032 esp_driver_i2c esp_driver_gpio
     esp_timer freertos`, plus only what the build proves, and `cxx_std_17`.
   - `sdkconfig.defaults`: `CONFIG_FREERTOS_HZ=1000`. Per target:
     `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` (S3) or
     `CONFIG_ESP_CONSOLE_USB_CDC=y` (S2), each with
     `CONFIG_ESP_CONSOLE_SECONDARY_NONE=y`.
5. **Adapter `main/Rv3032IdfI2c.{h,cpp}`.**
   - **Ownership.** One application-owned context holds the bus and device
     handles: created at init, removed in reverse order at deinit, kept when
     cleanup fails.
   - **Bus.** A dedicated port: SDA 8, SCL 9, 400 kHz
     (`examples/common/BoardConfig.h:29-36`), 7-bit `0x51`. Document internal
     pull-ups as insufficient. Set `trans_queue_depth = 0` and register no event
     callbacks.
   - **Startup.** Match `initWire`'s bus clear (`I2cTransport.h:290-336`). A
     line still held low fails init.
   - **Validation.** Reject a null or unbound context, an address other than
     0x51, null or empty buffers, and a zero or over-limit timeout. Each
     rejection is `I2C_ERROR` with zero SDK calls.
   - **Timeout.** Read the start from `esp_timer_get_time()`. If at most 1 ms
     remains, return `I2C_TIMEOUT` with no SDK call. Otherwise pass
     `remaining - 1` ms, never 0 or -1, as `I2cTransport.h:143-153` does.
     Require a 1000 Hz tick at compile time or init. When the elapsed time is
     past the budget, return `I2C_TIMEOUT` even after an SDK OK.
   - **Mapping.** `ESP_OK` is OK. `ESP_ERR_TIMEOUT` is `I2C_TIMEOUT`.
     `ESP_ERR_INVALID_RESPONSE` is `I2C_ERROR`, because the phase is unknown
     (`docs/IDF_PORT.md:35-39`); never invent `I2C_NACK_ADDR`. Map other codes
     to `I2C_ERROR` or `I2C_BUS` as documented. Use static messages and put the
     `esp_err_t` in the detail.
   - **`i2cWrite`.** Exactly one `i2c_master_transmit`. Never retry, reset or
     replay.
   - **`i2cWriteRead`.** Exactly one `i2c_master_transmit_receive`, with no
     bus reset, retry or replay inside the callback, matching
     `examples/common/I2cTransport.h:185-288`. Because no read is ever
     retried, the scoped single-attempt rule (`Config.h:63-65`) holds
     unconditionally; document that.
   - **SDK behavior.** Document that after an error the driver resets its FSM
     (`i2c_master.c:1028`), and the next transaction start resets it and
     clears the bus after a timeout or while busy (`:678-679`). This is SDK
     behavior, not a replayed write.
   - **Time.** `nowMs` follows `docs/IDF_PORT.md:41-44`. `waitMs` follows
     `:46-52`: round up, add one guard tick, then loop until `nowMs` confirms
     the full time, with a bounded attempt count. No logging in the adapter.
6. **Bring-up `main/main.cpp`.** Run the startup from
   `examples/01_basic_bringup_cli/main.cpp:2311-2347` with the same printed
   lines: bus init, `Config` with `enableEepromWrites = false`, `begin`,
   `probe`. Then one owner loop prints `readTime()` once a second and yields
   at least one tick (`vTaskDelay(1)`) on every iteration. 02 replaces it.
7. **Host suite.**
   - `test/fake_idf/` scripts `driver/i2c_master.h`, `esp_err.h`,
     `esp_timer.h`, `freertos/FreeRTOS.h` and `freertos/task.h`, with a call
     log and a controllable clock.
   - `test/test_native_idf/` compiles the adapter against them and routes
     transfers into `FakeRv3032`, so the real driver runs.
   - `.\scripts\pio.cmd test -e native` must run both suites
     (`platformio.ini:98-99`) **[verify]**.
8. **Docs.**
   - `docs/IDF_PORT.md`: the component and adapter, the cited SDK facts and
     SDK-internal FSM reset, the mappings, the one-attempt policy and the
     1000 Hz tick.
   - The residual risk: on a stall, one SDK call can wait once per command
     batch, past the budget. The adapter then reports `I2C_TIMEOUT` but does
     not cancel the transfer.
   - Update `README.md:514`, `docs/VERIFICATION.md` and AGENTS.
   - CHANGELOG: Added (the component, the `>=6.0.1` manifest, the adapter);
     Changed (the manifest is no longer ignored or rejected by name).

## Tests and acceptance

- **Write.** Each outcome is exactly one transmit, with no recovery and no
  second transfer. Results stay in the closed domain.
- **Read.** The mapping holds for each `esp_err_t`, with exactly one
  transmit-receive per callback and no recovery or retry.
- **Timing.** The fake clock proves each callback finishes within its budget.
  With at most 1 ms left, no SDK call is made. A late OK is `I2C_TIMEOUT`. No
  0 or -1 reaches the SDK.
- **Validation and lifecycle.** Validation failures make zero SDK calls. Each
  init failure unwinds, and a cleanup failure keeps its handle.
- **End to end.** Through `FakeRv3032`: `readTime`, `setTime`, a cooperative
  job polled with budget 1, and ensure through the one-attempt adapter.
  Existing native tests pass unchanged.
- **Builds.** All four PlatformIO environments build. After each,
  `git status --porcelain --ignored` shows the manifest unchanged and no
  `.orig`. If a PlatformIO build creates `idf_component.yml.orig` or changes
  `idf_component.yml`, stop: restore the file from Git, record the build and
  the `component_manager.py` evidence, and leave 01 NOT DONE. The S3 and S2
  IDF images link.

## Verify

```powershell
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e esp32s3dev; .\scripts\pio.cmd run -e esp32s2dev
.\scripts\pio.cmd run -e esp32s3hil; .\scripts\pio.cmd run -e esp32s3hil_persistence
python scripts/generate_version.py check; python tools/check_portability.py; python tools/check_abi.py
python -m unittest discover -s tools -p 'test_check_*.py'
python -S tools/hil_cli_runner.py --parser-self-test; python -S tools/test_hil_cli_runner.py
python -S tools/test_hil_health_snapshot.py; python -S tools/hil_cli_runner.py --dry-run
python tools/check_package.py source; doxygen Doxyfile
C:\esp\idf.cmd 6.0.1 -C examples\esp_idf\basic -B C:\esp\build\rv3032-esp32s3-6.0.1 -D SDKCONFIG=C:\esp\build\rv3032-esp32s3-6.0.1.sdkconfig set-target esp32s3 build
C:\esp\idf.cmd 6.0.1 -C examples\esp_idf\basic -B C:\esp\build\rv3032-esp32s2-6.0.1 -D SDKCONFIG=C:\esp\build\rv3032-esp32s2-6.0.1.sdkconfig set-target esp32s2 build
.\scripts\pio.cmd pkg pack -o RV3032-C7.tar.gz .; python tools/check_package.py package RV3032-C7.tar.gz
git status --porcelain --ignored; git diff --check
```

Before packing, delete the IDF lock and managed components (README rule 3).
Record the image sizes and the output of `idf.py --version`.

## Close out

Report each evidence layer; remote CI and hardware are NOT RUN. Set row 01 to
DONE, commit only owned files, push and sync. The handoff lists the SDK facts
confirmed or corrected and any core warning fix.
