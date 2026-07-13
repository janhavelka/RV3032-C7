# AI Coder Prompt 04.1: Passive Lifecycle And Cooperative Core

## Phase contract

This is phase 1 of 5 in Prompt Suite 04.

- Required first: read `04-00-shared-contract.md` completely.
- Prerequisites: none beyond the shared contract; audit the baseline before editing.
- Current scope: Implement and verify ownership, transport layering, passive lifecycle, Config changes, observational health, instruction budgets, and mutual exclusion.
- Exit criteria: the core architecture is internally consistent, public lifecycle behavior is covered by focused native tests, and later phases have a safe cooperative foundation.
- Preserve unrelated work and keep the shared dated requirement/evidence report
  current. Do not commit, tag, publish, flash, or run hardware EEPROM work.

## Target library architecture

### Ownership and transport

- The application owns the I2C bus, lock, task/thread, pins, initialization,
  transfer timeout, recovery policy, retry policy, and scheduling.
- The library is single-threaded/not thread-safe. The application serializes an
  instance and the device.
- Core source includes no Arduino, Wire, FreeRTOS, ESP-IDF, or transport driver.
- Core source creates no task, bus, lock, log, or heap allocation in steady
  operation.
- All I2C uses injected `Config` callbacks.
- The library invokes a transport callback at most once for each counted job or
  EEPROM instruction. It never retries or recovers internally.
- A mutating `i2cWrite` callback is one physical attempt with no automatic
  retry. A may-have-committed error is reconciled by later readback in the same
  logical operation and is never blindly resent.
- A resource-owning application may map normal read-only `i2cWriteRead`
  callbacks to a documented bounded recovery plus at most one read retry. This
  is owner behavior, not library retry, and must be included in that
  integration's timing tests. Applications that do not want retry provide a
  single-attempt callback.
- The primary-cell ensure callback contract requires single-attempt transport
  for every provisioning callback, including reads. TunnelMonitor satisfies it
  with a scoped adapter mode; other callers must provide equivalent behavior.
  After terminal failure the owner may recover the bus for later work but never
  replay ensure in that begin/end lifecycle.

### Lifecycle

Keep these public lifecycle names:

```cpp
Status begin(const Config& config);
void tick(uint32_t nowMs);
void end();
Status probe();
Status recover();
```

`begin()`:

- validates and stores configuration;
- performs exactly zero transport/wait callbacks;
- does not probe, read a register, change PMU, clear flags, queue persistence,
  invoke ensure, or apply backup policy;
- returns BUSY with no state change for a second begin while initialized;
- sets READY to mean callbacks are bound, not device presence;
- resets the primary-cell attempt latch only on a new successful lifecycle.

`end()`:

- performs zero callbacks;
- is idempotent while uninitialized;
- does not silently destroy an active job or EEPROM operation;
- after all work is idle, clears callbacks, fixed operation state, health, and
  the primary-cell attempt latch.

`probe()` performs one raw Status-register read and no mutation, retry, PMU, or
EEPROM action.

`recover()` remains for compatibility but means a bounded driver-health
re-probe only. It must not recover the physical bus, call `_applyConfig()`,
start generic persistence, invoke ensure, reset the ensure latch, or perform
more than the documented read-only recovery attempt.

Remove automatic `_applyConfig()` from lifecycle and recovery. Remove the
unsafe public `setPrimaryBatteryBackupDefaults()`; the dedicated semantic ensure
replaces it. Keep typed active backup-mode setters for legitimate live control.

### Config

Retain current transport/health/persistence fields and add only the wait hook:

```cpp
using WaitMsFn = void (*)(uint32_t delayMs, void* user);

struct Config {
  I2cWriteFn i2cWrite = nullptr;
  I2cWriteReadFn i2cWriteRead = nullptr;
  void* i2cUser = nullptr;
  NowMsFn nowMs = nullptr;
  WaitMsFn waitMs = nullptr;
  void* timeUser = nullptr;
  uint8_t i2cAddress = 0x51;
  uint32_t i2cTimeoutMs = 50;
  bool enableEepromWrites = false;
  uint32_t eepromTimeoutMs = 100;
  uint8_t offlineThreshold = 5;
};
```

Remove `Config::backupMode`; passive begin no longer applies an implicit PMU
mode, so retaining an unused lifecycle policy field would be misleading. Keep
`BackupSwitchMode` and explicit typed setter/getter APIs.

Validation:

- both I2C callbacks required;
- address exactly `0x51`;
- `i2cTimeoutMs` in `1..100`;
- `eepromTimeoutMs` in `10..250` when generic persistence is enabled;
- `offlineThreshold >= 1`; reject rather than clamp;
- `nowMs` and `waitMs` remain optional for ordinary chunked use;
- `ensurePrimaryCellConfiguration()` requires both hooks and returns
  INVALID_CONFIG with zero I2C if either is absent.

Remove the current incorrect requirement that EEPROM use needs a 50 ms I2C
transaction timeout. Keep 50 ms as a conservative general-purpose default, but
permit callers to select `1..100` ms. The TunnelMonitor compatibility profile
uses 5 ms callbacks. EEPROM completion is controlled by operation deadlines,
not one long transfer.

### Cooperative work and mutual exclusion

Retain the current fixed `_job`, `_eeprom`, `JobState`, `EepromState`, queue,
`pollJob()`, `pollEeprom()`, and `tick()` structure. Refactor their fields and
states as necessary, but do not add parallel operation engines.

Rules:

- `pollJob(nowMs, maxInstructions, used)` performs at most
  `maxInstructions` transport callbacks.
- `pollEeprom(nowMs, maxInstructions, used)` does the same.
- `maxInstructions=0` performs zero I2C and returns current progress/status.
- One instruction means one library callback invocation. Owner-side read-only
  recovery is separately measured as an allowed physical-attempt exception.
- A caller may choose any bounded nonzero instruction budget; the
  TunnelMonitor compatibility test passes `maxInstructions=1` for normal RTC
  work.
- No busy loop: every loop iteration consumes a callback budget, completes a
  pure bounded state transition, or returns.
- Job and EEPROM execution are mutually exclusive. A job start returns BUSY
  while the EEPROM state machine is active. While only fixed queue entries are
  pending, a persistence-producing setter may admit its active-mirror job so
  the queue can reach its documented capacity; it reserves exact capacity
  before mutation, and `pollEeprom()` cannot advance concurrently. Every other
  job start returns BUSY while the EEPROM state/queue is active. A persistence-
  producing setter returns BUSY before active mutation while a job is active.
- Ensure returns BUSY with zero I2C and leaves both machines untouched if any
  job, EEPROM operation, or queued EEPROM item exists.
- `tick()` advances only generic EEPROM persistence. It never advances a normal
  job, calls ensure, probes, or recovers.
- No lifecycle/recovery/periodic method starts provisioning.

Health may remain, but OFFLINE is observational for a resource-owner
integration: it must not silently suppress a valid explicitly requested
operation. Preserve exact typed errors and counters; local validation errors do
not count as transport failures.
