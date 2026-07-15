# RV3032-C7

Production-oriented RV-3032-C7 RTC driver for ESP32-S2/S3, Arduino, and
PlatformIO. The core owns no I2C peripheral, pins, task, lock, logging sink, or
heap allocation. Applications inject bounded transport and timing callbacks.

[![CI](https://github.com/janhavelka/RV3032-C7/actions/workflows/ci.yml/badge.svg)](https://github.com/janhavelka/RV3032-C7/actions/workflows/ci.yml)

## Design contract

- `begin()` validates and binds callbacks with exactly zero device I/O.
- `probe()` is one raw Status-register communication check at address `0x51`.
  It proves only that transaction's address response, not chip identity.
- `end()` always abandons local queued/active work with zero I/O. It cannot
  undo an already-issued silicon write; abandoned persistent cleanup requires
  explicit product-policy reinitialization after rebinding.
- A driver object cannot be copied or moved because it owns live cooperative
  state and a borrowed transport binding.
- `recover()` is one tracked read-only health re-probe; physical bus recovery
  and retry policy remain application-owned.
- `OFFLINE` is an observational health label and never suppresses the next
  valid requested operation.
- Single-transfer calendar helpers remain available. Composite calendar,
  timer, control, RAM, and persistent work is cooperative through
  `start...Job()` plus `pollJob()` or `pollEeprom()`.
- One counted instruction is one library transport callback. Budgets `0`, `1`,
  and larger bounded values are honored.
- `ensurePrimaryCellConfiguration()` is the sole synchronous multi-callback
  exception. It is explicit, once per begin/end lifecycle, deadline-bounded,
  and uses an injected yielding wait callback.
- No operation performs application bus recovery or blindly retries a possibly
  mutating transfer.
- Transport callbacks are synchronous borrowed-buffer calls with a closed
  I2C-only status domain. Cooperative deadlines are exclusive and are checked
  again at callback completion. A callback that exceeds its supplied timeout
  is reported as `I2C_TIMEOUT`.

## Memory model

The chip has several different storage classes:

| Range | Storage | Access |
| --- | --- | --- |
| `0x00..0x4F` | Direct volatile registers and 16-byte user RAM | Direct register transfers, subject to public allowlists |
| `0xC0..0xC5` | Supported active configuration mirrors and EEPROM | Typed active operations; indirect durable proof |
| `0xC6..0xCA` | Vendor password mirrors and EEPROM | Silicon reference only; library access is unsupported and denied |
| `0xCB..0xEA` | 32-byte user EEPROM | Indirect access only |

There is no FRAM. Time is maintained by the RTC counter while the backup supply
is valid; the calendar is not stored in EEPROM.

Configuration EEPROM endurance is finite: **10,000 writes at 3.0 V/25 C** and
**100 writes at 5.5 V/85 C**. Persistent mutation therefore compares first,
issues at most one write-one command per byte, and directly verifies durability.

## Quick start

```cpp
#include <Wire.h>
#include "RV3032/RV3032.h"

RV3032::RV3032 rtc;

static uint32_t nowMs(void*) { return millis(); }
static void waitMs(uint32_t delayMs, void*) { delay(delayMs); }

void setup() {
  RV3032::Config cfg{};
  cfg.i2cWrite = mySingleAttemptWrite;
  cfg.i2cWriteRead = myBoundedRead;
  cfg.i2cUser = &Wire;
  cfg.nowMs = nowMs;
  cfg.waitMs = waitMs;
  cfg.timeUser = nullptr;
  cfg.i2cTimeoutMs = 5;
  cfg.enableEepromWrites = false;

  RV3032::Status st = rtc.begin(cfg);   // validates/binds; zero I2C
  if (!st.ok()) return;
  st = rtc.probe();                     // address communication, not identity
  if (!st.ok()) return;
}

void loop() {
  const uint32_t now = nowMs(nullptr);
  RV3032::Status persistence = rtc.tick(now); // one EEPROM instruction
  if (!persistence.ok() && !persistence.inProgress() &&
      persistence.code != RV3032::Err::BUSY) {
    handleRtcPersistenceFailure(persistence);
  }
  if (rtc.isJobBusy()) {
    uint8_t used = 0;
    RV3032::Status job = rtc.pollJob(now, 1, used);
    handleRtcJobProgress(job);          // at most one library callback
  }
}
```

`waitMs` is optional for ordinary cooperative use. It is required only by the
explicit synchronous primary-cell ensure operation and must sleep/yield rather
than spin or perform I2C.

## Calendar APIs

`readTime()` performs one calendar burst. `setTime()` performs one synchronous
calendar write without readback verification or Status clearing. `readHundredths()`
provides a separate strict BCD read of register `0x00`; applications must allow
for rollover relative to a separate calendar burst. `readTime()`
strictly rejects reserved bits, invalid BCD/calendar fields, and weekday values
outside `0..6`. The RV3032 weekday is user-assigned: every value `0..6` is
accepted without requiring agreement with the date, and setters preserve the
caller's valid value. `computeWeekday()` remains available for applications
that want Gregorian weekday policy. It and the Unix/build-time conversions
return `Status`, validate the complete `2000..2099` domain, and preserve output
arguments on failure. Writing Seconds resets the hundredths
counter and the 4096 Hz through 1 Hz prescalers. These helpers do not inspect
or clear Status flags.

Applications needing stronger evidence use:

```cpp
const uint32_t now = nowMs(nullptr);
rtc.startReadTimeSnapshotJob(now, 100);
rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(value, now, 250);

uint8_t used = 0;
RV3032::Status st = rtc.pollJob(now, 1, used);
```

The snapshot reads Status before the calendar, returns typed `StatusFlags` from
that same first callback, and short-circuits when typed PORF or VLF is set. The
verified setter writes once, reconciles an ambiguous callback by readback,
accepts the requested value or exactly one second later, reads fresh Status,
and writes the named fixed payload `0xFC`. That payload clears PORF/VLF while
preserving UF/TF/AF/EVF that may assert between the cooperative read and write.
The report records the unavoidable THF/TLF clearing, and the job verifies final
Status/calendar state. Larger polling budgets refresh elapsed time between
callbacks so no later mutation starts after its cutoff.

Every simple Status clearer for AF, TF, UF, EVF, PORF, and VLF applies this
silicon rule: any Status-register write clears THF and TLF. If either omitted
flag is already set at the guard read, the operation returns `INVALID_PARAM`
without writing. An assertion after the guard read cannot be preserved. The
verified calendar-set job is a separate contract: its public name promises
PORF/VLF clearing, its `VerifiedTimeSetReport` captures pre-clear THF/TLF
evidence, and its Status write still has the unavoidable THF/TLF side effect.

## Persistent APIs

Explicit reads work even when generic writes are disabled:

```cpp
const uint32_t now = nowMs(nullptr);
rtc.startReadConfigurationEepromJob(
    RV3032::ConfigurationEepromRegister::PMU, now);
rtc.startReadUserEepromJob(offset, length, now);
```

User EEPROM writes require `Config::enableEepromWrites=true`:

```cpp
const uint32_t now = nowMs(nullptr);
rtc.startWriteUserEepromJob(offset, data, length, now, 4000);
```

Public user EEPROM offsets are `0..31`; each job is limited to 16 bytes. Write
authority is deliberately limited to configuration C0..C5 and typed user
EEPROM CB..EA; it never authorizes password registers. Password protection must
be disabled for supported operation. A module protected by other firmware is
unsupported and requires out-of-band service. Callback success alone cannot
prove a protected write changed silicon; typed multi-transfer operations rely
on readback, and ordinary single writes exist only under this product
precondition. The library copies write input into fixed storage at admission.
Reads use an adaptive two-read staging proof, and a failed multi-byte read
reports only the number of bytes positively proven. Each changed byte is
written with at most one `0x21` write-one attempt, reconciled after ambiguous
callback failure, and directly read back. Generic persistence never uses
update-all `0x11` or
refresh-all `0x12`.

Configuration setters can update active mirrors while persistence is disabled.
When enabled, supported setters queue fixed-capacity durable updates. While the
EEPROM engine is idle, another persistence-producing setter may run even when
entries are queued; unrelated jobs remain blocked. Capacity is checked before
active mutation, and an exact request coalesces only with the newest pending
value for that address, preserving FIFO final-state intent. The application
advances durable work with `Status tick(uint32_t nowMs)` or `pollEeprom()`.
`tick()` delegates exactly one EEPROM instruction and returns
`NOT_INITIALIZED`, ordinary-job `BUSY`, progress, or the cached terminal EEPROM
status; it does not discard failures. READ_ONE/WRITE_ONE wait
intervals start after the command transport callback completes; the generic
`eepromTimeoutMs` window begins after the mandatory 10 ms WRITE_ONE settle.
Successful cleanup restores and verifies the queued intended active C0..C5
mirror as well as the saved safe-access state.

Forward-operation and access-state-cleanup evidence are separate. Typed read
and write reports retain `operationStatus`, `cleanupStatus`, durable proof, and
exact partial byte counts independently. A cleanup failure cannot erase proof
already established by direct persistent readback. Generic batch diagnostics
expose the same two first-cause statuses in `SettingsSnapshot`; a new batch,
`begin()`, or `end()` resets them.

An ordinary queue-item failure is returned at that item boundary. Later items
remain queued for a subsequent poll, while the first operation failure remains
the batch status so a later success cannot hide it. Once a hard operation
deadline is reached, no further callback is
started. If access-state cleanup was still required, the terminal error is
`EEPROM_CLEANUP_FAILED` so an unproven safe state is never reported as a plain
timeout. That terminal cleanup failure cancels remaining queued items and does
not start another EEPROM command; the application must decide when it is safe
to re-admit persistence work.

The generic queue status/count/depth surfaces do not describe explicit typed
persistent jobs. Those jobs use `isJobBusy()`, `pollJob()`, and their typed
result getters.

`getEepromHardwareFlags()` reads the chip's EEbusy and sticky EEF bits; these
are distinct from the library queue state returned by `isEepromBusy()`.
`clearEepromErrorFlag()` explicitly acknowledges stale EEF through a guarded
cooperative W0C operation without claiming that an earlier write was durable.

## Cooperative configuration evidence

Timer, periodic-update, backup, CLKOUT, and temperature-event jobs publish a
`ConfigurationJobReport`. Its terminal state is exactly one of
`UNCHANGED`, `REQUESTED_VERIFIED`, `SAFE_DISABLED_VERIFIED`, or `UNKNOWN`.
`operationStatus` retains the first forward failure; `cleanupStatus` retains
the first safe-state or reconciliation failure. `mutationAttempted` is set
only when a requested mutating callback was dispatched. A failed, timed-out,
or otherwise ambiguous requested write is never replayed. Persistence is
queued only after the requested active state was read back and
`operationStatus` remained `OK`.

Timer, periodic-update, CLKOUT, and temperature-event failures use their own
bounded safe gates: TE=0, UIE=0, preserved PMU with NCLKE=1, and
THE/TLE/THIE/TLIE=0 respectively. Their success/worst-case callback caps are
6/9, 5/8, 5/8, and 7/10. Backup is reconciliation-only and has a 4/4 cap; it
never issues a cleanup PMU write or replays its one requested write.

Backup mode configuration is cooperative:

```cpp
const uint32_t now = nowMs(nullptr);
RV3032::Status st = rtc.startSetBackupSwitchModeJob(
    RV3032::BackupSwitchMode::Level, now);
```

The public enum is encoded explicitly: Off=`00`, Direct=`01`, Level=`10`;
observed raw `11` is also disabled. Admission requires BSIE=0 and an exclusive
timeout satisfying `4 * i2cTimeoutMs + activationMs + 1`, up to 1000 ms, where
activation is 2 ms for disabled-to-Direct and 10 ms for disabled-to-Level.
The job preserves non-BSM PMU bits, checks optional C0 queue capacity before
mutation, writes at most once, reconciles by exact implemented-bit readback,
and cannot report terminal success before the activation not-before boundary.
Register proof does not prove physical retention, backup voltage/topology
safety, or electrical timing on a real board.

Periodic UIE is update-event enable, not merely interrupt enable. UIE=0
suppresses both new UF generation and the INT event; there is no UF-polling-only
mode. Configuration does not implicitly clear a pre-existing UF.

Live reconfiguration uses explicit quiescence guards. The caller sequence is:

```text
disable interrupt -> consume/clear flag if appropriate -> configure -> enable
```

Timer requires TIE=0, alarm updates require AIE=0, EVI updates/reset require
EIE=0, and backup requires BSIE=0; a busy guard performs no write. EIE=0
suppresses interrupt signaling but does not stop timestamp capture, so an edge
can still occur during EVI configuration. This is vendor-recommended
hardening, not proof that a concurrent physical event was reproduced safely.
`setClkoutStopDelayEnabled()` is intentionally not EIE-guarded because CLKDE
controls CLKOUT delay after I2C STOP rather than EVI capture.

## Primary-cell configuration

Battery chemistry is application policy. The library never provisions from
`begin()`, `probe()`, `recover()`, `tick()`, a setter, or a calendar job.

An authorized application may explicitly call:

```cpp
RV3032::PrimaryCellConfigurationReport report{};
RV3032::Status st = rtc.ensurePrimaryCellConfiguration(report);
```

This operation requires single-attempt transport callbacks plus `nowMs` and
`waitMs`. It rejects active cooperative work without consuming its lifecycle
attempt. It directly reads persistent C0 before deciding and uses the exact
target:

```text
(persistentC0 & 0x4C) | 0x20
```

This preserves NCLKE and TCR, clears reserved bit 7 and TCM, and selects level
switching. Correct state causes zero write-one commands. Incorrect state causes
at most one C0 write-one, followed by busy/EEF checks and a second direct
persistent read. Trusted cleanup restores active target state, clears EERD, and
honors the 10 ms level-switch settle. Untrusted communicable failure holds
verified BSM00/TCM00 and EERD=1 where possible; that volatile hold is not
deployment-ready and does not survive POR.

The terminal report exposes semantic evidence without requiring raw C0
interpretation. `persistentTargetVerified` is set at the direct persistent
read proving the derived target; `activeTargetVerified` is set only after the
active C0 target is read back. Both remain true if a later Control 1 cleanup or
settle step fails. A verified safe BSM00/TCM00 failure hold is deliberately not
reported as active-target proof. `cleanupVerified` remains the separate
terminal cleanup result.

The caller must prove the electrical preconditions; the library cannot measure
them. Keep VDD stable and at least 1.6 V through any EEPROM write and busy-clear
phase. A 400 kHz bus requires VDD at least 2.0 V. Before enabling level
switching, keep VDD safely above the vendor's maximum 2.2 V LSM threshold
through activation and the complete measured 10 ms settle, and verify the
board's backup/backfeed topology. Do not deploy a module after a failed ensure
until an explicit ensure in a later complete lifecycle succeeds.

Application-manual page 141 shows `TCR=00` in black as the unchanged delivery
value and `TCM=00` in red as the primary-cell safety setting. Charging is
disabled by TCM, so this helper preserves an existing inactive TCR selection
instead of resetting it.

The call is allowed once per successful begin/end lifecycle. A second call
returns `PRIMARY_CELL_ALREADY_ATTEMPTED` with zero callbacks. A later explicit
attempt requires `end()` followed by a new successful `begin()` and always
re-reads actual chip state.

The generic CLI example does not provision at startup. It requires the exact
operator command:

```text
primary-cell ensure CONFIRM-PRIMARY-CELL
```

## Capability coverage

Typed APIs cover calendar/hundredths/Unix conversion, alarm/AIE/AF, countdown timer/TIE/TF,
periodic update/UIE/UF, event input/EIE/EVF/timestamps, temperature thresholds
and events, coherent two-sample temperature reads and THF/TLF handling,
complete XTAL/HF CLKOUT configuration and typed CLKF read/clear, signed offset
calibration plus PORIE/VLIE, independent BSM/TCM/TCR, BSIE/BSF,
STOP/ESYN/CLKDE/GP bits, hardware EEbusy/EEF, Status/reset/validity,
user RAM, supported configuration/user EEPROM, and passive
health diagnostics.

Alarm date 0 restores the vendor's AF-never-sets reset state; setting every AE
bit instead means an event every minute. CLKOUT configuration is staged through
the cooperative engine: interrupt-controlled output must first be disabled and
inactive, then direct output is stopped before FD/HFD/OS change. Temperature
event configuration likewise disables detection while replacing independent
signed thresholds. The offset API uses the exact 0.238418579 ppm nominal step,
and CLKOUT register values above 52 MHz are exposed but are outside the vendor's
guaranteed electrical-characteristic range.

Unless an API explicitly says it queues C0..C5 generic persistence, mutations
of calendar/alarm/timer/control/Status/EVI/timestamps/thresholds/GP/user RAM are
active-only. The application decides whether and when persistent configuration
is written and must advance that opt-in work through `tick()`/`pollEeprom()`.

Raw diagnostics are intentionally narrow. Reads are limited to documented
direct ranges. Writes are limited to temperature thresholds and volatile user
RAM; they cannot reach calendar, alarm, timer, Status/control, unsupported password, EEPROM
staging/command, indirect EEPROM, read-only, or reserved registers. Use typed
methods for side-effecting features.

## Status and health

All fallible APIs return:

```cpp
struct Status {
  Err code;
  int32_t detail;
  const char* msg; // static storage only
};
```

No exceptions are used. Transport success/failure is recorded only inside the
tracked wrappers. Validation, precondition failures, and raw `probe()` do not
change health counters. `READY` after `begin()` means callbacks are bound; it
is observational health, not address response or presence evidence. A
successful raw `probe()` proves only communication for that one address-`0x51`
Status read. It does not prove RV3032 identity. `recover()` and `lastError()`
map the same address NACK to `DEVICE_NOT_FOUND`. Lifetime success/failure
counters are ordinary `uint32_t` counters and wrap from `UINT32_MAX` to zero.

## Wire example adapter

`examples/common/I2cTransport.h` is application glue, not library code. Its
Arduino-ESP32 3.2.0 adapter applies the callback's supplied timeout as one hard,
exclusive bound across the complete callback. Each blocking Wire phase gets
only the remaining interval, and RAII restores the application's prior Wire
timeout on every exit. A short staging write is not retried: the adapter emits
one bounded final STOP before returning an I2C-domain error. The application
must serialize the shared bus and keep the Wire mutex uncontended during the
synchronous callback; the adapter does not add a second lock or scheduler.

## CLI ownership

The bring-up CLI uses one overflow-discarding line reader and strict numeric
tokens with exact argument counts. Invalid input starts no RTC/I2C work. One
fixed `PendingOperation` owns accepted asynchronous work, advances at most one
callback per loop iteration, and retains the ordinary terminal status through
optional EEPROM persistence. It prints completion only after terminal ordinary
and persistence evidence is available; there is no parallel `tick()` path. An
item-level persistence failure does not release ownership while later queue
entries remain, so no queued work is orphaned.

## Repository and examples

- `include/RV3032/` — public Doxygen headers only
- `src/` — platform-neutral implementation
- `examples/common/` — example-only board/transport glue, not library code
- `examples/01_basic_bringup_cli/` — interactive product-neutral bring-up CLI
- `docs/` — architecture, device reference, adapter notes, reports, local PDFs
- `test/test_native/` — native fake and unit/integration tests

## Verification

```powershell
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python scripts/generate_version.py check
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_docs_contract.py source
python tools/hil_cli_runner.py --parser-self-test
python tools/hil_cli_runner.py --dry-run
```

Parser self-test and dry-run are device-free. Physical HIL, flashing, EEPROM
execution, voltage/backfeed, power-cycle, and retention work require separate
authorization. Current compatibility evidence is recorded in
`docs/reports/2026-07-14-tunnelmonitor-integration-readiness.md`; the broad
2.0.0 implementation report and historical HIL evidence are kept separately.
The library is a dependency candidate only: TunnelMonitor integration and an
immutable commit pin remain external work, and old 1.5.0 HIL is not a 2.0.0
primary-cell or retention claim.

After such fresh authorization, `--destructive-setup` additionally requires
explicit `--authorization-port`, `--authorization-module`,
`--authorization-primary-cell-chemistry`, `--authorization-power-conditions`,
`--authorization-c0-write CONFIRM-POSSIBLE-C0-WRITE`, and
`--authorization-vdd-off-backfeed-scope` values. The runner rejects missing or
mismatched scope before opening the serial port and records it in HIL results.

## Versioning

`library.json` is the single version source. `include/RV3032/Version.h` is
generated and must not be edited manually.

## License

MIT. See `LICENSE`.
