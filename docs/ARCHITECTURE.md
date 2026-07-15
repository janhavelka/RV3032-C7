# RV3032-C7 driver architecture

## Ownership and lifecycle

The application owns the I2C controller, pins, locking, timeout policy,
recovery, and scheduling. The driver receives `i2cWrite` and `i2cWriteRead`
callbacks and never touches `Wire` or reconfigures a bus.

`begin(const Config&)` validates and stores callbacks only. It performs no I2C,
does not prove device presence, does not apply a PMU policy, and does not start
persistence. A successful passive bind enters `READY`. Calling `begin()` again
without `end()` returns `BUSY` and preserves the live configuration.

`probe()` is the explicit address-communication check. It performs one raw
Status-register read at `0x51` and does not affect health counters. Success
proves only that transaction's address response; it cannot prove RV3032
identity because another device could implement the same address/register.
`recover()` is a single
tracked register read that can restore the observational health state; it does
not recover the bus or reapply device configuration. `end()` is unconditional,
uses no I2C, and releases all local driver state even when work is active or
queued. Already-issued silicon writes cannot be undone. Abandoning persistent
cleanup discards the saved C0/Control 1 restoration snapshot, so the application
must explicitly reinitialize affected product policy after rebinding. The
driver object is noncopyable and nonmovable because duplicating live state could
duplicate an in-flight wear-limited mutation.

The four states are `UNINIT`, `READY`, `DEGRADED`, and `OFFLINE`. `OFFLINE` is a
health label, not an admission policy: a valid caller-requested operation still
reaches the transport. Only tracked transport wrappers update health.
Lifetime success/failure counters are ordinary `uint32_t` values and wrap from
`UINT32_MAX` to zero. Address NACK is mapped consistently to
`DEVICE_NOT_FOUND` in both tracked `recover()` evidence and `lastError()`.

## Transport layers

```text
typed public API
    -> register helpers
    -> tracked transport wrappers       (health updated here only)
    -> raw transport wrappers
    -> application callbacks
```

Each transport callback is one physical attempt from the driver's point of
view. The driver never retries, resets, or recovers the bus. A read-only
application adapter may implement its separately documented bounded recovery
and at most one read retry. Both physical attempts together must fit the one
supplied callback timeout and remain one driver instruction. Mutating callbacks
must never be replayed.
For `ensurePrimaryCellConfiguration()`, the owner must temporarily use scoped
single-attempt mode for both read and write callbacks; the ordinary read-only
recovery exception is disabled for the complete synchronous call.

Callbacks are synchronous and borrow buffers only for the call duration.
Their status domain is closed to `OK`, `I2C_ERROR`, `I2C_NACK_ADDR`,
`I2C_NACK_DATA`, `I2C_TIMEOUT`, and `I2C_BUS`; any other code becomes
`TRANSPORT_CONTRACT_VIOLATION` at the raw boundary. Tracked health is updated
exactly once only when application code was actually invoked.

Every callback receives a finite timeout that is a hard, exclusive bound for
the complete application callback, including serialization, permitted
read-only recovery, and physical phases. The application must arrange an
uncontended shared-bus owner before dispatch. `i2cTimeoutMs` is constrained to
1..100 ms. `nowMs` provides wrap-safe health and operation time. `waitMs` is a
yielding application wait used only by the explicit primary-cell ensure
operation; it must sleep or yield and must not spin.

The example Arduino-ESP32 3.2.0 Wire adapter applies only the remaining part of
one callback deadline before each potentially blocking phase and restores the
previous Wire timeout by RAII. `endTransmission(false)` only stages the
repeated-start transfer; the combined operation executes in the final-stop
`requestFrom(..., true)`. A short staging write is never retried and is closed
by exactly one bounded final STOP so it cannot strand Wire ownership.

## Execution forms

Single-transfer reads and writes can complete synchronously. Work requiring
more than one transfer uses the one fixed-capacity job engine:

- `start...Job()` validates and admits work without I2C.
- `pollJob(nowMs, budget, used)` advances at most `budget` callbacks.
- `pollEeprom(nowMs, budget, used)` advances optional queued persistence with
  the same instruction accounting.
- `Status tick(nowMs)` delegates exactly to the persistence poller with a
  budget of one. It returns precondition, progress, or cached terminal status
  and does not poll ordinary jobs.

A budget of zero performs no callback. A resource owner can pass one so that
one owner poll performs at most one transport callback. Wait states consume no
instruction. Jobs have fixed storage, fixed callback/check caps, and either a
derived transfer bound or wrap-safe phase and whole-operation deadlines.
When a larger budget is used, the engine refreshes the optional monotonic clock
between instructions; without that hook it conservatively charges each callback
its exact supplied timeout before deciding whether another may start. Deadlines
are exclusive. The timed transport owner clips each callback timeout to the
remaining interval minus one millisecond and checks completion immediately.
A callback exceeding its supplied timeout becomes `I2C_TIMEOUT`; a late result
cannot advance normal success. When a mutating callback was dispatched, the
state machine may advance only into its existing readback reconciliation or
bounded cleanup path, never into a replay of that mutation.

Admission rejects a whole-operation budget that cannot execute the fixed
callback sequence under these rules. With a live clock hook, a two-transfer
job needs at least 2 ms; without one, its minimum also includes the conservative
per-callback timeout charges. The verified calendar job similarly accounts for
all seven callbacks and its shared mutation cutoff. Rejection performs zero I/O.

## Staged configuration and reconciliation

Timer, periodic update, backup, CLKOUT, and temperature events are distinct
fixed-state owners. Each publishes one `ConfigurationJobReport` with separate
first forward-operation and first cleanup/reconciliation status, a Boolean set
only by actual requested-write dispatch, and one verified terminal state:
`UNCHANGED`, `REQUESTED_VERIFIED`, `SAFE_DISABLED_VERIFIED`, or `UNKNOWN`.
Requested writes are one physical attempt and are never replayed. Generic
configuration persistence is admitted only after exact requested-state
readback and an `OK` forward operation.

Timer, periodic-update, CLKOUT, and temperature-event failures enter their
bounded safe-disable owner after mutation. The safe gates are TE=0, UIE=0,
exact preserved PMU with NCLKE=1, and THE/TLE/THIE/TLIE=0. Cleanup first reads
the gate, writes only when it is proven unsafe, writes at most once, and
verifies once. An unreadable gate is not guessed. The fixed success/worst-case
callback caps are 6/9, 5/8, 5/8, and 7/10 respectively.

Backup uses readback-only reconciliation and a 4/4 callback cap. Public modes
are explicitly encoded as Off=`00`, Direct=`01`, and Level=`10`; observed raw
`11` is also disabled. The job requires BSIE=0, reserves optional C0 queue
capacity before mutation, preserves non-BSM PMU bits, issues at most one PMU
write, and proves requested/original/unknown state by exact implemented-bit
readback. Disabled-to-Direct cannot complete before 2 ms after write callback
completion; disabled-to-Level cannot complete before 10 ms. Admission requires
the exclusive interval `4 * i2cTimeoutMs + activationMs + 1`, through 1000 ms.
The wait state performs no callback. Software register proof is not physical
retention, voltage/topology safety, or measured board timing evidence.

Live register reconfiguration uses explicit read-only quiescence guards. Timer
requires TIE=0, alarm updates AIE=0, EVI updates and EVI timestamp reset EIE=0,
and backup BSIE=0. A set guard returns `BUSY` before mutation. Applications use
`disable interrupt -> consume/clear flag if appropriate -> configure ->
enable`; the driver does not silently toggle or restore interrupt policy. EIE=0
suppresses signaling but does not stop timestamp capture, so this is
vendor-recommended hardening rather than proof against a concurrent physical
edge. CLKDE is not EIE-guarded because it controls CLKOUT delay after I2C STOP.

UIE represents update-event enable. UIE=0 produces neither new UF nor the INT
event and therefore provides no UF-polling-only mode. Periodic reconfiguration
does not implicitly clear an already latched UF.

At a hard persistence deadline, no new callback is started. If cleanup remains
unverified, the terminal result is `EEPROM_CLEANUP_FAILED`; queued persistence
items are cancelled rather than executed against uncertain C0/Control 1 state.
The application owns any later recovery and re-admission.

Control register read-modify-write operations, flag clears, timer programming,
calendar composites, staged CLKOUT/temperature configuration, large RAM writes,
and all indirect persistence use this engine. CLKOUT reconfiguration first
proves interrupt-controlled output inactive and stops direct output before
changing FD/HFD/OS. Temperature configuration disables detection while changing
thresholds. There is no second asynchronous scheduler.

The example CLI likewise has one owner. Its sole line reader discards an
overlength line through its terminator, numeric tokens are strict and
range-checked, and invalid input dispatches no I2C. A fixed
`PendingOperation` advances either the ordinary job or EEPROM surface with a
budget of one. Ordinary terminal status is retained across optional
persistence, and the loop has no unconditional `tick()` or inferred second
polling path. An item-level persistence failure does not release the owner
while later queue entries remain; the batch is reported only after the queue
truly terminates.

## Calendar contracts

`readTime()` is a strict single burst read. It rejects reserved bits, malformed
BCD, invalid Gregorian dates, years outside 2000..2099, and weekday values
outside `0..6`. Weekday is user-assigned and is not required to match the date;
setters write the caller's validated value unchanged.

`startReadTimeSnapshotJob()` reads Status first. If PORF or VLF is set, the
typed result exposes decoded `StatusFlags` from that same callback, records
invalid time, and does not read the calendar.

`startSetTimeAndClearInvalidFlagsVerifiedJob()` writes and reads back the
calendar, reads Status immediately before clearing PORF/VLF, writes Status, then
verifies Status and calendar again. Its public name exposes the mutation. The
Status write is the fixed value `0xFC`: PORF/VLF are cleared while
UF/TF/AF/EVF are protected if they assert between the budgeted read and write.
Every Status-register write also clears THF and TLF in RV3032 silicon; the
result records whether those flags were set before the write. Possibly
committed calendar or Status writes are reconciled by readback and are never
replayed.

The simple AF, TF, UF, EVF, PORF, and VLF clearers use a different guarded
contract: any Status-register write clears THF and TLF in silicon. If either
omitted flag is already set at the guard read, the operation returns
`INVALID_PARAM` without writing. An assertion after the guard read cannot be
preserved. The verified calendar-set job does not take that exit; its public
name promises PORF/VLF clearing and its `VerifiedTimeSetReport` captures the
pre-clear THF/TLF evidence.

The temperature registers have no shadow latch. `readTemperatureC()` remains a
single-transfer convenience API, while `startReadCoherentTemperatureJob()`
uses two separately budgeted samples and returns `INCOHERENT_DATA` if their
TEMP fields differ. THF and TLF can be inspected together and are cleared
together because every Status write clears both flags in silicon.

## Active configuration and persistent EEPROM

Addresses `0xC0..0xC5` are active configuration mirrors. Persistent
configuration and the 32-byte user EEPROM are accessed indirectly through
EEADDR, EEDATA, and EECMD. The part does not contain FRAM.

Generic writes are disabled by default. When enabled, an active configuration
job can hand its resulting bytes to the fixed queue. If the EEPROM state
machine is idle, another persistence-producing setter may be admitted while
entries are pending. It checks exact capacity after reading/computing the new
active value but before writing it. Duplicate coalescing compares only with the
newest pending value for the same address, so FIFO final-state intent is not
lost. Persistent inspection is available even while writes are disabled.

The shared persistence protocol:

1. saves Control 1 and enables EERD with readback verification;
2. proves EEPROM idle, reads active C0, and installs a safe temporary C0;
3. performs an adaptive two-command READ_ONE proof of the persistent byte;
4. skips wear if the byte already matches;
5. clears stale EEF, stages and verifies EEADDR/EEDATA, and issues at most one
   WRITE_ONE (`0x21`);
6. waits from command-callback completion, checks busy/EEF, and directly proves
   the persistent byte again even when EEF reports a write failure;
7. restores and verifies the queued intended active C0..C5 mirror, active C0
   access state, and the exact original Control 1 value, then observes the BSM
   settle interval.

Callback failure after a command write is ambiguous, so the engine continues
with direct proof and never retries the wear-limited command. Generic paths do
not dispatch UPDATE_ALL (`0x11`) or REFRESH_ALL (`0x12`). User EEPROM writes are
bounded to 16 bytes per job and stop at the first failed byte with a partial
typed report. A persistent-read result length likewise counts only positively
proven bytes.

Typed persistent reports keep `operationStatus`, `cleanupStatus`, durable
content proof, and partial byte counts separate. Directly established proof is
not erased by a later C0/Control 1 cleanup failure. Generic queue batches latch
the first forward and cleanup statuses into durable settings fields before
clearing each item. Cleanup failure has terminal semantic precedence, cancels
remaining entries, and leaves both exact causes observable until a new batch,
`begin()`, or `end()`.

Every budgeted queue loop refreshes elapsed time between callbacks (or charges
the configured callback bound when no clock hook exists), so a larger budget
cannot cross a mutation cutoff and start WRITE_ONE. The configured EEPROM
timeout is the busy-poll window after the mandatory 10 ms write settle. An
ordinary item failure pauses at its boundary and remains the first batch
operation error
while later queued items can be advanced; cleanup failure instead cancels the
remaining queue because safe access state is unproven.

The generic cleanup reserve is derived, not guessed:
`250 ms + 6 * i2cTimeoutMs + 10 ms`. Typed starts require that reserve plus one
forward callback timeout and one millisecond. The six callbacks restore and
verify selected active state, active C0, and Control 1.

## Primary-cell provisioning boundary

`ensurePrimaryCellConfiguration()` is the sole synchronous multi-callback
exception. It exists for a serialized application I2C owner during startup and
is never called by `begin()`, `recover()`, `tick()`, or another library API.

Admission requires `nowMs`, `waitMs`, and no active/pending job or EEPROM work.
The same successful begin/end lifecycle permits only one admitted attempt.
Rejected preconditions do not consume the latch.

The operation directly reads persistent C0 before deciding and computes:

```text
target = (persistentC0 & 0x4C) | 0x20
```

This selects level backup switching and disables the charger mode while
preserving NCLKE and TCR. If already correct, no persistent write is issued. If
wrong, exactly one WRITE_ONE is permitted, followed by direct durable proof.
Bounded cleanup either restores a trusted active target and clears EERD, or
holds a safe active C0 with automatic refresh disabled when persistence cannot
be trusted. The report separates operation and cleanup evidence and exposes
semantic `persistentTargetVerified` and `activeTargetVerified` fields at their
direct proof points. Proven target evidence survives later cleanup/settle
failure; the safe BSM00/TCM00 hold is not active-target proof.

Every normal callback is admitted only when its supplied timeout plus the
300 ms cleanup reserve fits. Attempt evidence is set immediately before the
physical callback, late returns stop normal forward progress, and ambiguous
non-command mutations are read back before any safety cleanup write. The final
10 ms level-switch settle is measured from exact target-C0 readback, not from a
later Control 1 callback. Electrical qualification remains application-owned:
stable write-capable VDD (at least 1.6 V), the 2.0 V minimum for 400 kHz, VDD
safely above the maximum 2.2 V LSM threshold through activation/settle, and
board backup/backfeed evidence cannot be inferred by the driver.

## Status and raw access

All fallible APIs return `Status`; errors are never logged or hidden. Status
flags are explicit. The guarded Status clear refuses a write that would
silently erase a THF/TLF flag observed by its guard read and writes ones for
every unnamed lower-six W0C flag, including flags that assert between the read
and write callbacks. Silicon unconditionally clears THF/TLF on every Status
write, so a THF/TLF assertion in that same cooperative race window remains an
unavoidable hardware limitation.

TEMP_LSB EEF/CLKF/BSF clears use one dedicated two-callback owner. It reads only
to prove the target is set and EEbusy is clear, then writes the fixed W0C
preservation payload (`0x03`, `0x09`, or `0x0A`). The payload never copies a
stale neighboring flag bit from the earlier read.

Raw access is intentionally narrow. Reads cover ordinary active registers,
user RAM, and supported active configuration mirrors. Writes cover temperature
thresholds and user RAM only. Calendar, alarm, timer, control, Status,
EEADDR/EEDATA/EECMD, and persistent storage require typed APIs. Password ranges
`0x39..0x3C` and `0xC6..0xCA` are unsupported and rejected before transport;
password protection must be disabled for supported operation. A protected part
requires out-of-band service.

## Memory and concurrency

The driver allocates no heap memory in steady operation. Jobs, reports, and
queues use fixed arrays. Password credentials and protection-management state
are not present in the library.

The application serializes all calls. Job admission rejects overlapping work
with `BUSY`. Queued-only persistence permits the narrow setter admission above,
but `pollEeprom()` cannot execute while that setter job is active; all unrelated
jobs and primary ensure reject pending queue entries.
