# RV3032-C7 driver architecture

## Ownership and lifecycle

The application owns the I2C controller, pins, locking, timeout policy,
recovery, and scheduling. The driver receives `i2cWrite` and `i2cWriteRead`
callbacks and never touches `Wire` or reconfigures a bus.

`begin(const Config&)` validates and stores callbacks only. It performs no I2C,
does not prove device presence, does not apply a PMU policy, and does not start
persistence. A successful passive bind enters `READY`. Calling `begin()` again
without `end()` returns `BUSY` and preserves the live configuration.

`probe()` is the explicit diagnostic presence check. It performs one raw
register read and does not affect health counters. `recover()` is a single
tracked register read that can restore the observational health state; it does
not recover the bus or reapply device configuration. `end()` uses no I2C and is
idempotent while uninitialized. If work is active or queued it leaves the
lifecycle intact; once work is idle it releases all driver state.

The four states are `UNINIT`, `READY`, `DEGRADED`, and `OFFLINE`. `OFFLINE` is a
health label, not an admission policy: a valid caller-requested operation still
reaches the transport. Only tracked transport wrappers update health.

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
and at most one read retry; mutating callbacks must never be replayed.

Every callback receives a finite timeout. `i2cTimeoutMs` is constrained to
1..100 ms. `nowMs` provides wrap-safe health and operation time. `waitMs` is a
yielding application wait used only by the explicit primary-cell ensure
operation; it must sleep or yield and must not spin.

## Execution forms

Single-transfer reads and writes can complete synchronously. Work requiring
more than one transfer uses the one fixed-capacity job engine:

- `start...Job()` validates and admits work without I2C.
- `pollJob(nowMs, budget, used)` advances at most `budget` callbacks.
- `pollEeprom(nowMs, budget, used)` advances optional queued persistence with
  the same instruction accounting.
- `tick(nowMs)` is the compatibility scheduler for persistence with a budget of
  one. It does not poll ordinary jobs.

A budget of zero performs no callback. A resource owner can pass one so that
one owner poll performs at most one transport callback. Wait states consume no
instruction. Jobs have fixed storage, fixed callback/check caps, and either a
derived transfer bound or wrap-safe phase and whole-operation deadlines.

At a hard persistence deadline, no new callback is started. If cleanup remains
unverified, the terminal result is `EEPROM_CLEANUP_FAILED`; queued persistence
items are cancelled rather than executed against uncertain C0/C1 or password
authorization state. The application owns any later recovery and re-admission.
No callback starts at or after a hard deadline. If a persistent job reaches
that boundary before cleanup proof, it terminates with
`EEPROM_CLEANUP_FAILED`; the application must treat the hardware access state
as unknown and apply its own later admission/recovery policy.

Control register read-modify-write operations, flag clears, timer programming,
calendar composites, large RAM writes, and all indirect persistence use this
engine. There is no second asynchronous scheduler.

## Calendar contracts

`readTime()` is a strict single burst read. It rejects reserved bits, malformed
BCD, invalid Gregorian dates, years outside 2000..2099, and weekday mismatch.

`startReadTimeSnapshotJob()` reads Status first. If PORF or VLF is set, the
typed result records invalid time and the job does not read the calendar.

`startSetTimeAndClearInvalidFlagsVerifiedJob()` writes and reads back the
calendar, reads Status immediately before clearing PORF/VLF, writes Status, then
verifies Status and calendar again. Its public name exposes the mutation. Every
Status-register write also clears THF and TLF in RV3032 silicon; the result
records whether those flags were set before the write. Possibly committed
calendar or Status writes are reconciled by readback and are never replayed.

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
6. waits for vendor settle time, checks busy/EEF, and directly proves the
   persistent byte again;
7. restores and verifies active C0 and the exact original Control 1 value, then
   observes the BSM settle interval.

Callback failure after a command write is ambiguous, so the engine continues
with direct proof and never retries the wear-limited command. Generic paths do
not dispatch UPDATE_ALL (`0x11`) or REFRESH_ALL (`0x12`). User EEPROM writes are
bounded to 16 bytes per job and stop at the first failed byte with a partial
typed report.

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
be trusted. The report separates operation and cleanup evidence.

## Status and raw access

All fallible APIs return `Status`; errors are never logged or hidden. Status
flags are explicit. The guarded Status clear refuses a write that would
silently erase an active THF/TLF flag not named by the caller.

Raw access is intentionally narrow. Reads cover ordinary active registers,
user RAM, and active configuration mirrors. Writes cover temperature
thresholds and user RAM only. Calendar, alarm, timer, control, Status, password,
EEADDR/EEDATA/EECMD, and persistent storage require typed APIs.

## Memory and concurrency

The driver allocates no heap memory in steady operation. Jobs, credentials,
reports, and queues use fixed arrays. Password bytes are copied only into the
active job, zeroed at terminal completion, and never returned in settings,
status, logs, or raw reads.

The application serializes all calls. Job admission rejects overlapping work
with `BUSY`. Queued-only persistence permits the narrow setter admission above,
but `pollEeprom()` cannot execute while that setter job is active; all unrelated
jobs and primary ensure reject pending queue entries.
