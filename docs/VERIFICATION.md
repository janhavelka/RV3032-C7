# RV3032-C7 verification guide

Use this guide to reproduce software checks and plan hardware validation.
API contracts live in the public headers; this document describes how to test
them and interpret the evidence. Completed run reports belong in Git history
or private test artifacts, not in the maintained documentation.

## Software verification

Run from the repository root. On Windows, use the existing current-user
PlatformIO installation through `scripts/pio.cmd`. On other hosts, replace
`.\scripts\pio.cmd` with `pio`. Doxygen 1.9.7 or newer is required.

```powershell
.\scripts\pio.cmd test -e native
.\scripts\pio.cmd run -e esp32s3dev
.\scripts\pio.cmd run -e esp32s2dev
.\scripts\pio.cmd run -e esp32s3hil
.\scripts\pio.cmd run -e esp32s3hil_persistence
python scripts/generate_version.py check
python tools/check_portability.py
python tools/check_abi.py
python -m unittest discover -s tools -p 'test_check_*.py'
python -S tools/hil_cli_runner.py --parser-self-test
python -S tools/test_hil_cli_runner.py
python -S tools/test_hil_health_snapshot.py
python -S tools/hil_cli_runner.py --dry-run
python tools/check_package.py source
doxygen Doxyfile
C:\esp\idf.cmd 6.0.1 --version
C:\esp\idf.cmd 6.0.1 -C examples\esp_idf\basic -B C:\esp\build\rv3032-esp32s3-6.0.1 -D SDKCONFIG=C:\esp\build\rv3032-esp32s3-6.0.1.sdkconfig set-target esp32s3 build
C:\esp\idf.cmd 6.0.1 -C examples\esp_idf\basic -B C:\esp\build\rv3032-esp32s2-6.0.1 -D SDKCONFIG=C:\esp\build\rv3032-esp32s2-6.0.1.sdkconfig set-target esp32s2 build
# Remove generated dependencies.lock and managed_components/ from the example before packing.
.\scripts\pio.cmd pkg pack -o RV3032-C7.tar.gz .
python tools/check_package.py package RV3032-C7.tar.gz
```

CI runs the Arduino, host, static and package checks. Run the native ESP-IDF
builds locally. To verify the exported example, unpack the validated archive
in a fresh directory, point `-C` at that copy's example and repeat both IDF
builds with fresh external build and sdkconfig paths. A linked image proves
compilation and linking for that target. Physical HIL and console operation
require a board.
Open `docs/doxygen/html/index.html` to inspect the generated API reference.
The native suite covers callback budgets, deadlines and wraparound, ambiguous
writes, cleanup, status/result retention, register masks, and example transport
and CLI behavior. Host Python tests exercise contract checkers and serial
framing, payload, and health validation without opening a serial port.

The package includes maintained guides and public headers. Vendor PDFs remain
in the repository for offline reference. Generated docs, reports, prompts,
tests, and build products are excluded. Both source and archive checks require
the quoted version, supported targets and ESP-IDF constraint in the committed
root `idf_component.yml`. After each PlatformIO build, check
`git status --porcelain --ignored` for a changed manifest or its generated
`.orig` backup. If pioarduino rewrites the manifest, stop and restore the
committed file with `git restore -- idf_component.yml`. Remove the generated
backup before packing. A missing Arduino framework manifest can trigger this
fallback because the project uses `src_dir = .`.

## Hardware test surfaces

| Environment or tool | Purpose and execution |
|---|---|
| `esp32s3dev`, `esp32s2dev` | Interactive bring-up CLI; the host runner targets this command protocol. |
| `esp32s3hil` | Autonomous exhaustive harness that runs on boot, changes RTC state, and tests EEPROM wear behavior. A successful full wear sequence intentionally issues four WRITE_ONE commands: changed/restored user byte 31 and changed/restored configuration C1. |
| `esp32s3hil_persistence` | Dedicated interactive two-power-cycle harness that stages alternate C0..C5, verifies retention, restores originals, and verifies the second return. Follow its serial phase prompts. |
| `tools/hil_cli_runner.py` | Host CLI validation and optional stress/soak. `--parser-self-test` and `--dry-run` are device-free; real execution requires an explicit `--port`. |

The pinned platform is PIOArduino `55.03.311`, Arduino-ESP32 `3.3.11`, and
ESP-IDF `5.5.5`. The S3 board definition is `esp32-s3-devkitc1-n16r8` (16 MB
flash, 8 MB octal PSRAM). Example defaults are SDA GPIO8, SCL GPIO9, and 400 kHz;
verify the actual board connections and select serial ports on the command
line. Board configuration belongs to the application, not the library.

The two-cycle harness accepts `stage` and `status`, not the bring-up CLI
command set. It stores its phase record and original configuration in user
RAM bytes 8..15. After `stage`, it resumes verification/restoration on boot
when the saved phase and BSF indicate a power-cycle return; interrupted staging
also enters restoration on boot. Preserve that record through both cycles and
include those automatic mutations in the test scope.

`stage` invokes primary-cell ensure before saving its original C0..C5. It can
therefore persist C0, and its restoration baseline is the durable configuration
after that ensure. The RAM record is not a backup of arbitrary pre-test active
and persistent settings; save those separately outside the device when exact
pre-test restoration is required.

Physical execution, flashing, EEPROM writes, power cycling, and electrical
tests require explicit authorization for the actual fixture. The autonomous
firmware's boot-time mutations are part of that scope. For the CLI runner,
`--destructive-setup` additionally requires `--authorization-port`,
`--authorization-module`, `--authorization-primary-cell-chemistry`,
`--authorization-power-conditions`,
`--authorization-c0-write CONFIRM-POSSIBLE-C0-WRITE`, and
`--authorization-vdd-off-backfeed-scope`. Missing or mismatched fields are
rejected before the serial port opens. Use `--help` for optional stress/soak
and output arguments.

## Interpreting CLI runner results

Start from a freshly reset CLI with its startup prompt available. The runner
requires complete response framing, memory payloads, stress counters, and
every driver health row; a prompt alone does not prove a complete result.
Normal health checks require READY and zero consecutive, total, and EEPROM
write failures. Expected fixture errors cannot conceal unrelated failures.

The runner stops at its first failure. A reboot, missing terminal prompt, or
short serial command write forbids subsequent commands and automatic replay.
With intact framing, `drv` snapshots bracket stress and capture observational
health after a failure without replacing the original error. The retained
`--idle-timeout-s` option is for command-line compatibility; only the hard
command deadline ends a response without its prompt.

Exit status zero means no FAIL was recorded; review `UNKNOWN`, `SKIP`, and
`NOT RUN` outcomes as coverage gaps, not passed tests. Absent temperature or
event stimulus cannot establish interrupt/timestamp behavior.

Serial bytes are flushed to a live transcript under `.pio/hil-runs/`, or a
new path supplied through `--transcript-out`; an existing explicit file is
not overwritten. Keep transcripts and JSON/Markdown outputs in private test
artifacts. They may contain original RAM or EEPROM values. Record commit,
firmware, fixture, authorization, and untested conditions with each run, and
share only reviewed excerpts without those values.

## Persistence and retention validation

Save active mirrors and persistent C0..C5 independently before mutation; they
can legitimately differ. Verify restoration through direct active reads and
indirect persistent readback. The exhaustive harness preserves active and
durable C1 separately, including PORIE/VLIE. Its offset test uses one changed
write and one restoration; preparation, equal-value verification, and final
active restoration add no configuration EEPROM writes.

After controlled main-VDD removal with backup retained, compare elapsed
calendar time, PORF/VLF, BSF, and a known RAM sentinel. BSF is cleared by POR
and reads zero with backup switching disabled, so it cannot independently
prove retention. If the product deliberately invokes primary-cell ensure
after return, finish that operation before comparing active C0. Correct stored
C0 must require no additional WRITE_ONE command.

Functional power-cycle checks do not measure VBACKUP decay, VDD-off backfeed,
switchover timing, oscillator accuracy, EEPROM endurance, long-outage retention,
or field stability. Those require separate fixtures, instruments, and test
scope. Native fault injection and successful embedded builds likewise provide
software evidence only; tie physical claims to the tested revision and setup.
