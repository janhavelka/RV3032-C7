# RV3032-C7

Production-oriented RV-3032-C7 RTC driver for ESP32-S2/S3, Arduino, and
PlatformIO. The core owns no I2C peripheral, pins, task, lock, logging sink, or
heap allocation. Applications inject bounded transport and timing callbacks.

[![CI](https://github.com/janhavelka/RV3032-C7/actions/workflows/ci.yml/badge.svg)](https://github.com/janhavelka/RV3032-C7/actions/workflows/ci.yml)

The maintained integration is Arduino on ESP32-S2/S3. Start with the
[integration sketch](#integration-sketch), then choose the
[calendar](#calendar-apis), [persistent storage](#persistent-apis), or
[configuration](#cooperative-configuration-evidence) contract your owner loop
needs. The [architecture](docs/ARCHITECTURE.md) and
[device reference](docs/DEVICE_REFERENCE.md) explain the underlying protocol.

## Design contract

- `begin()` validates and binds callbacks with exactly zero device I/O.
- `probe()` is one raw Status-register communication check at address `0x51`.
  It proves only that transaction's address response, not chip identity.
- `end()` always abandons local queued/active work with zero I/O. It cannot
  undo an already-issued silicon write. Abandoned persistent cleanup retains
  `persistentAccessStateUnproven` across rebinding; explicitly run
  `startPersistentAccessStateRecoveryJob()` with the application's intended C0
  and inspect its restoration evidence before further persistence.
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
| `0x00..0x2D` | Direct calendar, alarm, timer, status, control, temperature, and timestamp registers | Typed operations or restricted raw access |
| `0x2E..0x38` | Reserved/unimplemented direct-register gap | Denied |
| `0x39..0x3C` | Write-only password registers | Unsupported and denied |
| `0x3D..0x3F` | EEPROM address/data/command staging registers | Driver-internal protocol only |
| `0x40..0x4F` | 16-byte volatile user RAM | Typed RAM operations |
| `0xC0..0xC5` | Supported active configuration mirrors and EEPROM | Typed active operations; indirect durable proof |
| `0xC6..0xCA` | Vendor password mirrors and EEPROM | Silicon reference only; library access is unsupported and denied |
| `0xCB..0xEA` | 32-byte user EEPROM | Indirect access only |

There is no FRAM. Time is maintained by the RTC counter while the backup supply
is valid; the calendar is not stored in EEPROM.

Configuration EEPROM endurance is finite: **10,000 writes at 3.0 V/25 C** and
**100 writes at 5.5 V/85 C**. Persistent mutation therefore compares first,
issues at most one write-one command per byte, and directly verifies durability.

## Integration sketch

```cpp
#include <Wire.h>
#include "RV3032/RV3032.h"

RV3032::RV3032 rtc;

static uint32_t nowMs(void*) { return millis(); }
static void waitMs(uint32_t delayMs, void*) {
  // Arduino-ESP32 delay() is relative to scheduler ticks. One guard tick
  // prevents a near-boundary call from returning before delayMs has elapsed.
  delay(delayMs + 1U);
}

void setup() {
  RV3032::Config cfg{};
  cfg.i2cWrite = mySingleAttemptWrite;
  cfg.i2cWriteRead = myBoundedRead;
  cfg.i2cUser = &Wire;
  cfg.nowMs = nowMs;
  cfg.waitMs = waitMs;
  cfg.timeUser = nullptr;
  cfg.i2cTimeoutMs = 50;
  cfg.enableEepromWrites = false;

  RV3032::Status st = rtc.begin(cfg);   // validates/binds; zero I2C
  if (!st.ok()) return;
  st = rtc.probe();                     // address communication, not identity
  if (!st.ok()) return;
}

void loop() {
  const uint32_t now = nowMs(nullptr);
  if (rtc.isOrdinaryJobBusy()) {
    uint8_t used = 0;
    RV3032::Status job = rtc.pollJob(now, 1, used);
    handleRtcJobProgress(job);          // at most one library callback
  } else if (rtc.isEepromPollable()) {
    RV3032::Status persistence = rtc.tick(now); // one EEPROM instruction
    if (!persistence.ok() && !persistence.inProgress()) {
      handleRtcPersistenceFailure(persistence);
    }
  }
}
```

This is a lifecycle sketch: the transport callbacks and application error
handlers are intentionally project-owned. See
`examples/01_basic_bringup_cli/main.cpp` and `examples/common/I2cTransport.h`
for a complete Arduino-ESP32 integration.

Use `isOrdinaryJobBusy()` and `isEepromPollable()` to select one polling
surface per owner pass. `isJobBusy()` combines active work; `isEepromBusy()`
also includes queued persistence that may be waiting for an ordinary job.
Either poller's `BUSY` result means advance the other surface. While generic
work owns the device, `getJobStatus()` reports `BUSY`, but completed typed
`get*JobResult()` accessors retain the ordinary job's terminal status and report.

`waitMs` is optional for ordinary cooperative use. It is required only by the
explicit synchronous primary-cell ensure operation. It must sleep/yield for at
least the requested monotonic duration rather than spin or perform I2C.

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

Applications needing stronger evidence choose one cooperative job and poll it
to a terminal result before admitting another. For a status-first snapshot:

```cpp
const uint32_t now = nowMs(nullptr);
RV3032::Status st = rtc.startReadTimeSnapshotJob(now);
if (!st.inProgress()) handleRtcJobAdmissionFailure(st);

// One owner-loop step; repeat with a freshly sampled time while IN_PROGRESS.
const uint32_t pollNow = nowMs(nullptr);
uint8_t used = 0;
st = rtc.pollJob(pollNow, 1, used);
```

For a verified set that deliberately clears PORF/VLF, use this instead after
the previous job is terminal:

```cpp
const uint32_t now = nowMs(nullptr);
RV3032::Status st =
    rtc.startSetTimeAndClearInvalidFlagsVerifiedJob(value, now);
if (!st.inProgress()) handleRtcJobAdmissionFailure(st);

// One owner-loop step; repeat with a freshly sampled time while IN_PROGRESS.
const uint32_t pollNow = nowMs(nullptr);
uint8_t used = 0;
st = rtc.pollJob(pollNow, 1, used);
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
The 200 ms snapshot and 700 ms verified-set defaults are executable across the
accepted `i2cTimeoutMs` range. A custom verified-set timeout must also retain
the public `MIN_SET_TIME_OPERATION_BUDGET_MS` post-mutation proof interval.

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
RV3032::Status st = rtc.startReadConfigurationEepromJob(
    RV3032::ConfigurationEepromRegister::PMU, now);
if (!st.inProgress()) handleRtcJobAdmissionFailure(st);
```

After that job is terminal, a separate user-EEPROM read may be admitted:

```cpp
const uint32_t now = nowMs(nullptr);
RV3032::Status st = rtc.startReadUserEepromJob(offset, length, now);
if (!st.inProgress()) handleRtcJobAdmissionFailure(st);
```

User EEPROM writes require `Config::enableEepromWrites=true`:

```cpp
const uint32_t now = nowMs(nullptr);
RV3032::Status st = rtc.startWriteUserEepromJob(offset, data, length, now);
if (!st.inProgress()) handleRtcJobAdmissionFailure(st);
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

Direct EEPROM read/write budgets include the two READ_ONE waits before the
cleanup cutoff. Without `nowMs`, admission also budgets all first-byte
callbacks conservatively. For example, `i2cTimeoutMs=5` and
`eepromTimeoutMs=100` require at least 298/518 ms for a read/write with a clock
hook, or 398/648 ms without one. Allow extra time for slow callbacks, additional
bytes, initial EEbusy, and scheduling gaps; see the public API for the formulas.
The direct-read default is 4000 ms and the direct-write default is 6000 ms.
Queued items use at least 4000 ms, extended when necessary to cover the
configured first-byte callback bound, initial ready wait, and post-write
proof/cleanup reserve. At the maximum supported timeout settings, that bound
is 5323 ms per queued item. Caller scheduling gaps still consume these budgets.

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
not start another EEPROM command. `persistentAccessStateUnproven` then remains
set in `SettingsSnapshot`, and new persistence is rejected until the
application runs and proves `startPersistentAccessStateRecoveryJob()` with its
intended C0. The synchronous primary-cell ensure operation obeys the same latch:
it is rejected with zero I/O until recovery, and an ensure that cannot prove
its own C0/Control 1 cleanup sets the latch.
The latch survives `end()`/`begin()` on the same driver object, including
abandoned active cleanup and callback rebinding. Recovery remains available
with generic writes disabled; `begin()` always validates its EEPROM timeout.
An EEbusy read failure or timeout is retained as the recovery operation error
while bounded direct C0/EERD restoration still proceeds.

The generic queue status/count/depth surfaces do not describe explicit typed
persistent jobs. Those jobs use `isOrdinaryJobBusy()`, `pollJob()`, and their
typed result getters. Legacy `isJobBusy()` remains a combined active-work
predicate, while `getJobStatus()` reports `BUSY` when active generic EEPROM
work owns the separate polling surface. The generic queue has separate fixed
state, so advancing it never erases or replaces the last ordinary job status
or result.

`getEepromHardwareFlags()` reads the chip's EEbusy and sticky EEF bits; these
are distinct from the library queue state returned by `isEepromBusy()`.
`clearEepromErrorFlag()` explicitly acknowledges stale EEF through a guarded
cooperative W0C operation without claiming that an earlier write was durable.

### CLKOUT factory default and persistence

The factory-delivery configuration bytes are C0=`0x00`, C2=`0x00`, and
C3=`0x00`. That selects direct output (`NCLKE=0`), XTAL mode (`OS=0`), and
FD=`00`, so CLKOUT is enabled at 32.768 kHz. HFD is also stored as zero, which
would select 8.192 kHz if HF mode were later enabled. Control 2, Clock
Interrupt Mask, and EVI Control reset to zero, so interrupt-controlled CLKOUT
and its delays are initially disabled. In VBACKUP power state the pin is LOW.

This is a delivery default, not an immutable reset policy. At power-up the chip
copies its stored configuration EEPROM into the C0..CA RAM mirrors after the
approximately 66 ms POR refresh. It also refreshes those mirrors automatically
at date increment when EERD=0. Therefore an active-only CLKOUT change can be
replaced by the next POR, automatic, or software refresh. Passive `begin()` and
one-read `probe()` do not wait for this refresh; applications that depend on a
custom persisted power-on configuration should observe EEbusy through
`getEepromHardwareFlags()` before treating the mirrors as loaded.

With `Config::enableEepromWrites=false`, all three CLKOUT setters change only
the active RAM mirrors:

- `setClkoutEnabled()` changes C0.NCLKE;
- `setClkoutFrequency()` selects XTAL mode/FD while retaining the stored HFD;
- `setClkoutConfig()` sets direct enable, OS, FD, and HFD.

With persistence enabled, `setClkoutEnabled()` queues exact C0, while
`setClkoutFrequency()` and `setClkoutConfig()` queue exact C0, C2, and C3 after
active readback proof. C1 is preserved during the active four-byte burst but
is not a CLKOUT persistence target. The ordinary job's terminal success proves
the active mirrors only; the application must then drive `tick()` or
`pollEeprom()` and require terminal `getEepromStatus()` success for durable
readback proof. Compare-before-write avoids wearing an already-equal byte.

CLKIE, CLKF, Clock Interrupt Mask (`0x14`), and CLKDE (`0x15`) are active-only
controls/evidence and are not part of C0/C2/C3 persistence. Before changing
frequency or mode, disable CLKIE and clear CLKF; the staged job otherwise
returns `BUSY` without mutation.

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

Timer preset zero is the vendor-defined non-running state. Calling
`setTimer(0, freq, false)` can restore that state exactly;
`setTimer(0, freq, true)` returns `INVALID_PARAM` before any I2C callback.

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
The safe default rejects a change to Direct/Level if the observed TCM field is
nonzero. Re-requesting the current mode succeeds without an active PMU write.
Boards with a compatible rechargeable source must state that intent explicitly
with `startSetBackupSwitchModeJobWithChargePolicy(...,
BackupChargePolicy::ALLOW_BACKUP_CHARGING)`; this may energize charging.
The same safe default applies to `setTrickleChargeMode(nonzero)` when BSM is
already Direct/Level. Use `setTrickleChargeModeWithChargePolicy()` for the
corresponding explicit rechargeable-source assertion, so charging cannot be
enabled accidentally through the inverse update.
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
attempt, and rejects an unproven persistent-access latch until explicit
recovery succeeds. Its callbacks use the separate
`primaryCellI2cTimeoutMs` (1..5 ms),
not `i2cTimeoutMs`. It directly reads persistent C0 before deciding and uses
the exact target:

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

### Support boundaries and known gaps

The following capabilities are deliberately outside the current library:

| Capability | Current boundary |
|---|---|
| Password management | Password ranges are denied. An already protected device requires out-of-band service. |
| Native ESP-IDF package | No native component, maintained example, or ESP-IDF-only CI build is shipped. [Adapter notes](docs/IDF_PORT.md) describe the callback integration. |
| Dates and civil-time policy | Calendar and Unix conversions cover 2000..2099. The application owns timezone, DST, and clock synchronization policy. |
| Atomic multi-byte EEPROM updates | Jobs process at most 16 bytes and report verified partial progress. Applications needing an atomic record must supply their own storage format and commit policy. |
| Bus and interrupt ownership | The application owns serialization, GPIO interrupt handling, scheduling, bus recovery, and permitted read retry. The library supplies typed operations and observable results. |

For release publication status, see [versioning](#versioning) for the current development and tagged
versions.

## Status and health

All fallible APIs return:

```cpp
struct Status {
  Err code;
  int32_t detail;
  const char* msg; // static storage only
};
```

No exceptions are used. Transport success/failure is recorded per tracked
transport callback inside the tracked wrappers. Validation, precondition
failures, and raw `probe()` do not change health counters. `READY` after
`begin()` means callbacks are bound; it
is observational health, not address response or presence evidence. A
successful raw `probe()` proves only communication for that one address-`0x51`
Status read. It does not prove RV3032 identity. `recover()` and `lastError()`
map the same address NACK to `DEVICE_NOT_FOUND`. Success/failure counters reset
on each successful `begin()`/`end()` lifecycle, are ordinary `uint32_t` values,
and wrap from `UINT32_MAX` to zero.

## Wire example adapter

`examples/common/I2cTransport.h` is application glue, not library code. Its
Wire adapter, validated with Arduino-ESP32 3.3.11, applies the callback's
supplied timeout as one hard, exclusive bound across the complete callback.
Each blocking Wire phase gets only the remaining interval, and RAII restores
the application's prior Wire timeout on every exit. A short staging write is
not retried: the adapter emits one bounded final STOP before returning an
I2C-domain error. The application must serialize the shared bus and keep the
Wire mutex uncontended during the synchronous callback; the adapter does not
add a second lock or scheduler.
Zero/partial reads return `I2C_ERROR` with the received-byte count because
Wire discards the backend error. They do not prove an address NACK; an
explicit address-NACK result still maps to `DEVICE_NOT_FOUND` in the core.

The example's `initWire()` is startup-only, before Wire owns the pins. It uses
open-drain bus-clear pulses and one bounded SCL-wait budget, and fails if either
line remains LOW. Runtime recovery must first detach the existing bus owner;
the supplied CLI never calls this startup helper from a runtime command.

## CLI ownership

The bring-up CLI uses one overflow-discarding line reader and strict numeric
tokens with exact argument counts. Invalid input starts no RTC/I2C work. One
fixed `PendingOperation` owns accepted asynchronous work, advances at most one
callback per loop iteration, and retains the ordinary terminal status through
optional EEPROM persistence. It prints completion only after terminal ordinary
and persistence evidence is available; there is no parallel `tick()` path. An
item-level persistence failure does not release ownership while later queue
entries remain, so no queued work is orphaned.

After 15 seconds it prints one pending-work diagnostic and keeps ownership
until the operation terminates. Each loop discards at most 256 bytes of busy
serial input; queued lines and partial tails remain marked for discard after
completion. Input arriving during the terminal callback is also discarded, so
an old command cannot execute as a new command after the completion prompt.

## Repository and examples

- `include/RV3032/` — public Doxygen headers only
- `src/` — platform-neutral implementation
- `examples/common/` — example-only board/transport glue, not library code
- `examples/01_basic_bringup_cli/` — interactive product-neutral bring-up CLI
- [`docs/`](https://github.com/janhavelka/RV3032-C7/tree/main/docs) — architecture,
  device reference, ESP-IDF adapter notes, verification guidance, and repository-only
  vendor PDFs
- `test/test_native/` — host unit/integration tests against the bounded fake
- `test/test_hil/`, `test/test_hil_persistence/` — on-device harnesses
  (`esp32s3hil`, `esp32s3hil_persistence`)
- `test/stubs/` — Arduino/Wire stubs for the native build
- `tools/` — repository check scripts and the device-free HIL runner
- `scripts/` — version generator and the PlatformIO wrapper

The maintained example glue is intentionally small: `BoardConfig.h`,
`CliShell.h`, `CliStyle.h`, `CommandHandler.h`, `I2cScanner.h`,
`I2cTransport.h`, and `Log.h`. The CLI composes those owners directly; there is
no parallel transport, bus-diagnostic, or health facade. The scanner applies a
temporary bounded Wire timeout and restores the application's previous value;
it does not perform bus recovery.

## Verification

The embedded environments pin PIOArduino `55.03.311` (Arduino-ESP32 `3.3.11`,
ESP-IDF `5.5.5`). The ESP32-S3 environments use the built-in
`esp32-s3-devkitc1-n16r8` definition for 16 MB QIO flash and 8 MB octal PSRAM.
Select upload and monitor ports on the command line; machine-local COM ports
are intentionally not committed.

On Windows hosts with legacy path limits enabled, the first 3.3.11 framework
installation can exceed `MAX_PATH` while unpacking bundled headers. Enable
Windows long-path support or temporarily set `PLATFORMIO_CACHE_DIR` to a short,
writable path for the PlatformIO install/build command.

Run the complete [software verification gate](docs/VERIFICATION.md), which
covers native tests, all four embedded builds, host tooling, version/ABI and
portability contracts, Doxygen, and the release package. These checks require
no attached RTC and do not flash a device.

The same guide explains the interactive CLI runner, autonomous HIL firmware,
EEPROM write counts, power-cycle restoration, and evidence limits. Physical
execution requires authorization for the fixture and mutations involved.
Build success and a runner exit code alone do not establish complete hardware
coverage; inspect individual results and untested conditions.

## Versioning

The latest published release is **v3.0.1**. Current development is
**Unreleased**, with **3.1.0** intended as the next release. The
[changelog](CHANGELOG.md) collects all changes since the published release.
Existing build metadata is a development identifier and will be aligned when
the next release is prepared; it does not establish a published version.

`library.json` is the single version source. `include/RV3032/Version.h` is
generated and must not be edited manually. `scripts/generate_version.py`
generates only this library's version header and build defines. Dependency-pin
or application-version metadata belongs to the consuming project.

## API documentation

Run `doxygen Doxyfile` with Doxygen 1.9.7 or newer from the repository root and open
`docs/doxygen/html/index.html`. The generated reference uses this README as its
entry page and includes the changelog, contributor guide, architecture, device
reference, ESP-IDF notes, and verification guide. The driver reference groups
methods by feature; each operation documents admission, polling, result, and
device side effects.
`Config.h` defines transport and timing contracts, `Status.h` defines result
semantics, and `CommandTable.h` records silicon addresses without granting raw
access to restricted ranges. Documentation warnings fail the CI build.

## License

MIT. See `LICENSE`.
