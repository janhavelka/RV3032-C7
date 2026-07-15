# AI Coder Prompt 06.2: Cooperative Jobs, Safe Reconciliation, and Persistent Evidence

## Assignment

Implement Phase 2 of the RV3032-C7 functional-hardening work. This prompt is
independently dispatchable, but it assumes the Phase 1 contracts listed below
are already present and correct. Audit those prerequisites before changing
source. Do not paper over a missing or incompatible Phase 1 contract inside a
Phase 2 state machine.

This phase closes these revalidated findings from
`docs/reports/2026-07-14-full-library-functional-audit.md`:

| ID | Severity | Required closure |
| --- | --- | --- |
| H-05 | High | Backup-mode completion includes the vendor 2 ms/10 ms activation interval and verified active state. |
| M-01 | Medium | Timer, periodic-update, CLKOUT, and temperature jobs expose verified requested, verified safe-disabled, unchanged, or unknown terminal state. |
| M-02 | Medium | Persistent operation proof and cleanup proof remain separate and neither overwrites the other. |
| M-04 | Medium | `tick()` returns the fallible EEPROM polling result instead of discarding it. |
| M-05 | Medium | UIE is represented as update-event enable; UIE=0 produces neither UF nor the INT event. |
| M-06 | Medium | Live reconfiguration uses the required TIE/AIE/EIE/BSIE quiescence guards. |

The engineering goal is not to preserve the current implementation shape. The
goal is the smallest clear set of cooperative state owners that satisfies the
vendor protocol, fixed callback bounds, no-retry rules, and public evidence
contracts below. Prefer deleting superseded states, branches, fields, helpers,
and tests over retaining them behind adapters.

## Authority and required reading

Use this authority order:

1. Read repository `AGENTS.md` completely. It is binding.
2. Read the local Micro Crystal documents:
   - `docs/reference-pdfs/RV-3032-C7_App-Manual.pdf`, Rev. 1.3;
   - `docs/reference-pdfs/RV-3032-C7_datasheet.pdf`.
3. Read the complete active audit:
   `docs/reports/2026-07-14-full-library-functional-audit.md`.
4. Read the Phase 1 prompt and its implementation handoff or evidence.
5. Treat maintained source, public headers, tests, examples, and documentation
   as current-implementation evidence, not as authority when they conflict
   with the items above.

Verify the vendor facts used by this phase, including Application Manual pages
23, 59-60, 70-71, 78, 81-82, 84, 87, and 96. Stop and report the exact page,
table, code location, and conflict if the local manual contradicts this prompt.
Do not guess.

The active audit was made against revision `0e2eb0d`, manifest version `2.0.0`.
Phase 1 is expected to have changed the worktree, so do not reset to that
revision. Start with `git status --short`, preserve all user changes, and
inspect the actual diff and HEAD before editing.

## Mandatory multi-agent working method

The primary coder owns all design decisions, edits, integration, and final
verification claims. Subagents provide bounded, read-only independent audits;
they do not invent alternatives to the contracts in this prompt.

Before editing production code, spawn at least these three bounded subagents:

1. **Backup and timing audit**: inspect backup-mode encoding, BSIE admission,
   PMU preservation, persistence admission, write/readback ambiguity, 2 ms/10
   ms activation timing, deadlines, callback caps, fake coverage, and HIL
   topology restrictions.
2. **Staged configuration and quiescence audit**: inspect timer, periodic
   update, CLKOUT, temperature events, alarm/EVI guard ownership, current
   state graphs, all mutation points, failure paths, safe gates, and callback
   caps.
3. **Persistence and integration audit**: inspect typed persistent reports,
   generic EEPROM batch diagnostics, `rememberFailure()`, cleanup precedence,
   result getters, `tick()`, fake behavior, examples, maintained docs, and
   contract checks.

Give every subagent this prompt's exact scope and require file/line evidence.
Have them report contradictions and deletion opportunities, not edit the same
files in parallel. Integrate their findings yourself before implementation.

After the implementation and focused tests, re-task those subagents to audit
the complete Phase 2 diff independently:

- one replays every backup success, early poll, timeout, callback failure, and
  reconciliation path;
- one audits every staged-job state transition, fault ordinal, safe-state
  proof, guard, callback cap, and persistence-admission branch; and
- one audits operation-vs-cleanup evidence, `tick()` propagation, tests,
  documentation, dead-code deletion, and cross-phase compatibility.

Read their evidence yourself. Fix every confirmed defect, rerun the affected
tests, and inspect the final diff again. Do not declare completion merely
because subagents or builds say it looks correct.

## Non-negotiable architecture

- `begin(const Config&)` stays passive and performs zero I2C or wait callbacks.
- `probe()` remains an explicit one-callback diagnostic.
- `end()` performs zero I/O and always ends the lifecycle.
- Ordinary multi-transfer operations remain fixed-capacity cooperative work
  advanced through `start...Job()`/`pollJob()` or
  `tick()`/`pollEeprom()`.
- One polling instruction invokes at most one driver transport callback.
- `ensurePrimaryCellConfiguration()` remains the sole synchronous
  multi-callback exception. Do not route Phase 2 work through it.
- The application owns bus scheduling, serialization, retry, and recovery.
- Do not add an internal retry, hidden recovery, blocking wait, platform
  delay, heap allocation, task, queue, registry, second scheduler, logging, or
  compatibility facade.
- A mutating callback is one physical attempt. Never replay a failed, timed
  out, late, or otherwise ambiguous requested write.
- Wait states consume zero callbacks. All waits and deadlines are wrap-safe and
  use intervals below `2^31` ms.
- Preserve fixed queue and buffer capacity. Do not write reserved bits.
- Do not create a generic configuration framework. The shared report has
  exactly five current users: timer, periodic update, backup, CLKOUT, and
  temperature-event configuration.
- Do not commit, tag, publish, release, flash hardware, issue physical EEPROM
  commands, or claim physical HIL without explicit authorization.

## Phase 1 prerequisite audit

Phase 1 is a hard prerequisite. Confirm every item below in code and tests
before implementing this phase. If a prerequisite is absent, materially
different, or failing, stop with a precise mismatch report. Do not add a local
Phase 2 shim or duplicate owner. Ask for Phase 1 to be corrected or for written
scope expansion.

### Required error and transport contracts

The existing `Err` values are not renumbered, and these values follow
`INCOHERENT_DATA = 22` exactly:

```cpp
CONFIGURATION_CLEANUP_FAILED = 23,
TRANSPORT_CONTRACT_VIOLATION = 24,
INTERNAL_STATE_ERROR = 25
```

There is no password-specific error or password service. Password-management
APIs, workflow states, credentials, and compatibility stubs are deleted.
Private register access fails closed over `0x39..0x3C` and `0xC6..0xCA`, while
managed EEPROM access retains `EEADDR`, `EEDATA`, and `EECMD` at
`0x3D..0x3F`. Persistent addressing is contiguous; no password address-list
branch remains.

Transport callbacks have the closed domain `OK`, `I2C_ERROR`,
`I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, or `I2C_BUS`. Invalid callback
codes normalize to `TRANSPORT_CONTRACT_VIOLATION`. The raw boundary exposes:

```cpp
struct RawTransferResult {
  Status status = Status::Ok();
  bool callbackInvoked = false;
};
```

Tracked health updates only when `callbackInvoked` is true. `probe()` remains
raw/untracked. A driver-control result such as `IN_PROGRESS` cannot escape from
an application transport callback.

### Required timed-transfer contracts

Confirm this exact result and these helpers exist and are used by
deadline-controlled states:

```cpp
struct TimedTransferResult {
  Status status = Status::Ok();
  Status callbackStatus = Status::Ok();
  uint32_t completedAtMs = 0;
  bool callbackInvoked = false;
  bool deadlineCrossed = false;
  bool callbackTimeoutViolated = false;
};

bool remainingBefore(uint32_t nowMs,
                     uint32_t deadlineMs,
                     uint32_t& remainingMs);

uint32_t earlierDeadline(uint32_t nowMs,
                         uint32_t first,
                         uint32_t second);

TimedTransferResult readRegsBefore(uint8_t reg,
                                   uint8_t* buf,
                                   size_t len,
                                   uint32_t& nowMs,
                                   uint32_t deadlineMs);

TimedTransferResult writeRegsBefore(uint8_t reg,
                                    const uint8_t* buf,
                                    size_t len,
                                    uint32_t& nowMs,
                                    uint32_t deadlineMs);
```

Deadlines are exclusive: completion at the deadline is late. Timed helpers
check before dispatch, clip the callback timeout to the remaining interval
minus one millisecond, account from the post-callback time, and reject a
callback that crosses the applicable deadline. They preserve normalized
`callbackStatus` separately from effective `status`. Without a clock hook they
conservatively add the timeout passed to the callback. With a hook, an observed
callback duration above the supplied callback timeout becomes effective
`I2C_TIMEOUT`.

Only a state that calls `writeRegsBefore()` for a forward mutating write copies
`callbackInvoked` into mutation-attempt evidence. Guard and verification reads
never mark mutation. A whole-operation deadline is a hard no-dispatch boundary.
`pollJob()` and persistence carry one mutable `currentNowMs`; waits anchor to
the relevant write's post-callback timestamp.

The persistence step owner has this exact signature so the current timestamp
and callback-instruction evidence cannot be reconstructed or hidden:

```cpp
Status processPersistentJob(uint32_t& nowMs, bool& callbackUsed);
```

Generic persistence uses the Phase 1 derived cleanup reserve, not a literal
300 ms reserve:

```cpp
const uint32_t cleanupReserveMs =
    EEPROM_CLEANUP_READY_TIMEOUT_MS +
    6U * _config.i2cTimeoutMs +
    EEPROM_WRITE_SETTLE_MS;
```

Admission leaves `cleanupReserveMs + one forward callback timeout + 1 ms`.
The one-byte configuration-read default is 1000 ms; user-read and user-write
defaults remain 1000 ms and 4000 ms. The generic configuration queue retains
its 4000 ms whole deadline and uses:

```cpp
mutationCutoffMs = itemStartMs + (4000U - cleanupReserveMs);
```

No deadline-controlled state calls plain `readRegs()` or `writeRegs()`.

### Required state/lifecycle prerequisites

- `TEMP_LSB` W0C flag clearing has one safe owner and cannot erase a neighbor
  asserted between read and write.
- `end()` is unconditional zero-I/O `_resetRuntimeState()`.
- `RV3032` is noncopyable and nonmovable.
- Every retained private `JobKind`, `JobState`, and `EepromState` enumerator is
  `CAPS_CASE`.
- Deleted dead states and `_beginInProgress` have not reappeared.
- An impossible/default state returns `INTERNAL_STATE_ERROR` and enters the
  applicable bounded reconciliation/cleanup owner when mutation or persistent
  access evidence requires it. It never returns `OK`.
- Health/presence mapping is internally consistent, and lifetime counters wrap
  rather than saturate.

Run the Phase 1 focused tests before proceeding. If the Phase 1 implementation
is present but a test exposes a regression caused by Phase 2 work, repair the
owning code; do not weaken the test.

## Phase 2 implementation

### 1. Replace the backup setter with a timed cooperative job

Delete the old public surface:

```cpp
Status setBackupSwitchMode(BackupSwitchMode mode);
```

Do not leave a deprecated wrapper, synchronous compatibility method, feature
flag, or alias. Add exactly:

```cpp
static constexpr uint32_t BACKUP_SWITCH_OPERATION_TIMEOUT_MS = 250;
static constexpr uint32_t BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS = 1000;

Status startSetBackupSwitchModeJob(
    BackupSwitchMode mode,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = BACKUP_SWITCH_OPERATION_TIMEOUT_MS);
```

Keep `getBackupSwitchMode()` as the existing single-transfer getter. Encode the
public enum explicitly; never cast it into silicon bits:

```text
Off    -> BSM 00
Direct -> BSM 01
Level  -> BSM 10
```

Treat both observed raw values `00` and `11` as disabled.

Add the distinct job owner:

```text
JobKind::SET_BACKUP_SWITCH_MODE
```

Use exactly these states:

```text
BACKUP_READ_CONTROL3
BACKUP_READ_PMU
BACKUP_WRITE_PMU
BACKUP_VERIFY_PMU
BACKUP_WAIT_ACTIVATION
```

Add only this backup-specific scratch:

```cpp
uint8_t backupOriginalPmu = 0;
uint8_t backupTargetPmu = 0;
uint32_t backupWriteCompletedMs = 0;
uint32_t backupActivationNotBeforeMs = 0;
bool backupActivationRequired = false;
```

Do not duplicate operation status, cleanup status, final state, or mutation
evidence in backup-only fields. Backup uses the shared
`ConfigurationJobReport` defined below.

Implement this sequence exactly:

1. Read Control 3. If BSIE is one, return `BUSY` after that read and perform
   zero writes.
2. Read active PMU and preserve every implemented non-BSM bit.
3. Before any mutation, prove capacity for the optional C0 persistence entry.
   Queue-full or other admission failure returns before mutation.
4. If the requested active mode is already present, skip the write and the
   activation wait.
5. Otherwise issue the active PMU write once.
6. Always read back PMU after an attempted write, including a failed or late
   callback when time remains before the whole deadline. Never replay the
   write.
7. Accept requested active state only after exact implemented-bit readback
   proof. Never infer it from ACK.
8. For disabled (`00` or `11`) to Direct, wait until 2 ms after write callback
   completion. For disabled to Level, wait until 10 ms after write callback
   completion. No other transition needs this activation wait.
9. `BACKUP_WAIT_ACTIVATION` performs zero callbacks. It returns terminal
   success only at or after `backupActivationNotBeforeMs`.
10. Enqueue optional C0 persistence only after active readback proof, the
    applicable activation settle, and `operationStatus.ok()`.

Use the Phase 1 timed helpers. The backup mutation cutoff reserves the PMU
verification callback and requested settle interval. Admission uses this exact
conservative minimum:

```cpp
minimumMs = 4U * _config.i2cTimeoutMs +
            (mode == BackupSwitchMode::Direct ? 2U :
             mode == BackupSwitchMode::Level ? 10U : 0U) +
            1U;
```

Accept the caller timeout only when
`minimumMs <= operationTimeoutMs && operationTimeoutMs <=
BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS`. Reject either boundary violation with
`INVALID_PARAM` and zero I/O. The 250 ms default is valid for the default 50 ms
transport timeout. At the maximum 100 ms callback timeout, Direct requires at
least 403 ms and Level requires at least 411 ms.

Use one implementation-only mismatch-detail helper for backup and safe-state
verification. Do not invent operation-specific packing:

```cpp
static constexpr int32_t registerMismatchDetail(uint8_t expected,
                                                uint8_t observed) {
  return (static_cast<int32_t>(expected) << 8) |
         static_cast<int32_t>(observed);
}
```

Pass implemented-bit-masked expected and observed bytes to this helper.

Backup is reconciliation-only after its single PMU write attempt. It never
uses the generic safe-disable cleanup and never issues a second PMU write. Use
this exact terminal mapping:

- If requested PMU is proven, set `REQUESTED_VERIFIED` and complete the
  applicable 2 ms/10 ms wait. If the write callback failed, return that
  original effective write status and do not queue persistence.
- If original PMU is proven, set `UNCHANGED`. Return the original write
  failure. If the write callback returned `OK` but target proof failed, set
  `operationStatus` and the terminal result to exactly:

  ```cpp
  Status::Error(Err::REGISTER_WRITE_FAILED,
                "Backup PMU verification failed",
                registerMismatchDetail(targetImplemented,
                                       observedImplemented));
  ```

- If neither original nor requested PMU is proven, or the reconciliation read
  fails, set `UNKNOWN` and return `CONFIGURATION_CLEANUP_FAILED`. A failed
  reconciliation read becomes the exact `cleanupStatus`. A successful read
  that proves neither state sets the first cleanup cause to exactly:

  ```cpp
  Status::Error(Err::REGISTER_WRITE_FAILED,
                "Backup PMU reconciliation inconclusive",
                registerMismatchDetail(targetImplemented,
                                       observedImplemented));
  ```

  When the forward write callback was `OK`, also set `operationStatus` to the
  exact `"Backup PMU verification failed"` status above. When it failed,
  retain that original first forward failure instead.
- An already-requested PMU is `REQUESTED_VERIFIED` with
  `mutationAttempted=false`.

The complete backup job has a success cap of 4 driver callbacks and a worst
failure/reconciliation cap of 4. Wait polls consume zero callbacks.

Delete all old backup-setter states, branches, helpers, fields, and tests that
are superseded. Do not route the new API through the old generic
register-update owner.

### 2. Add one shared staged-configuration report

Add this public type exactly. It has five concrete users and is not a base for
a generic framework:

```cpp
enum class ConfigurationFinalState : uint8_t {
  UNCHANGED = 0,
  REQUESTED_VERIFIED = 1,
  SAFE_DISABLED_VERIFIED = 2,
  UNKNOWN = 3
};

struct ConfigurationJobReport {
  Status operationStatus = Status::Ok();
  Status cleanupStatus = Status::Ok();
  ConfigurationFinalState finalState =
      ConfigurationFinalState::UNCHANGED;
  bool mutationAttempted = false;
};
```

Use distinct job kinds:

```text
SET_TIMER
SET_PERIODIC_UPDATE
SET_BACKUP_SWITCH_MODE
SET_CLKOUT_CONFIG
SET_TEMPERATURE_EVENT_CONFIG
```

CLKOUT, periodic update, and temperature configuration must no longer be
classified as generic `REGISTER_UPDATE`.

Add these result getters exactly:

```cpp
Status getSetTimerJobResult(ConfigurationJobReport& out) const;
Status getSetPeriodicUpdateJobResult(ConfigurationJobReport& out) const;
Status getSetBackupSwitchModeJobResult(ConfigurationJobReport& out) const;
Status getSetClkoutConfigJobResult(ConfigurationJobReport& out) const;
Status getSetTemperatureEventConfigJobResult(
    ConfigurationJobReport& out) const;
```

Getter contract:

- While the matching job is active, return `IN_PROGRESS` without changing
  `out`.
- When no matching completed result exists, return `JOB_RESULT_UNAVAILABLE`
  without changing `out`.
- Otherwise copy the completed report and return the exact terminal job
  status, matching the existing typed job-result convention.
- Do not return a generic retrieval `OK` after copying a failed result.

Store the report and exact operation scratch in `JobOp`:

```cpp
ConfigurationJobReport configurationReport{};
bool configurationCleanupWriteAttempted = false;

uint8_t timerOriginalControl1 = 0;
uint8_t timerSafeControl1 = 0;
uint8_t timerTargetControl1 = 0;
uint8_t timerTargetPreset[2] = {};

uint8_t periodicOriginal[2] = {};
uint8_t periodicTarget[2] = {};
uint8_t periodicSafeControl2 = 0;

uint8_t clkoutOriginal[4] = {};
uint8_t clkoutTarget[4] = {};
uint8_t clkoutSafePmu = 0;

uint8_t temperatureOriginal[6] = {};
uint8_t temperatureTarget[6] = {};
uint8_t temperatureSafeControl3 = 0;
```

Do not add heap-backed or polymorphic report storage. Reuse the existing typed
completed-result owner rather than creating another job registry or scheduler.

Apply this rule to timer, periodic update, CLKOUT, and temperature jobs:

1. Store the first forward failure in `operationStatus`; never overwrite it.
2. Set `mutationAttempted` only from `callbackInvoked` returned by a forward
   mutating `writeRegsBefore()`. Guard and verification reads never mark it.
3. Track cleanup writes separately with
   `configurationCleanupWriteAttempted`.
4. A pre-mutation failure leaves `finalState=UNCHANGED` and returns that
   failure directly.
5. Never replay a failed or ambiguous requested write.
6. Normal success performs final exact implemented-bit readback and sets
   `REQUESTED_VERIFIED` only after proof.
7. A post-mutation failure enters the operation-specific safe-disable cleanup.
   Backup is excluded and follows its reconciliation-only rule.
8. Safe-disable cleanup first reads the gate. If it is already safe, do not
   write it again. If it is proven unsafe, write the safe value once and verify
   it once. If the first cleanup read cannot determine state, do not guess and
   do not replay a possible safe-disable mutation.
9. A failed or late cleanup write is never retried. If the bound permits,
   perform its one verification read. Retain that callback failure even if
   readback later proves the safe value.
10. If safe disable is verified, return the original operation failure and set
    `SAFE_DISABLED_VERIFIED`.
11. If safe disable cannot be proven, return
    `CONFIGURATION_CLEANUP_FAILED`, retain the first cleanup cause in
    `cleanupStatus`, and set `UNKNOWN`.
12. Enqueue configuration persistence only after `REQUESTED_VERIFIED` and
    `operationStatus.ok()`.

Use this exact cleanup-status matrix so no state invents error evidence:

- a transport/deadline failure keeps its exact effective `Status` as the first
  cleanup cause;
- if a successful cleanup verification read observes a gate byte different
  from the implemented-bit-masked safe value, use exactly:

  ```cpp
  Status::Error(Err::REGISTER_WRITE_FAILED,
                "Configuration safe-state verification failed",
                registerMismatchDetail(safeImplemented,
                                       observedImplemented));
  ```

- never overwrite an earlier cleanup-write callback failure with a later
  verification result, whether it proves the safe value or reports a
  mismatch; and
- `cleanupStatus` remains `OK` only when no cleanup step failed and the safe
  state was either already proven or was written and verified.

For every impossible/default state introduced or retained by this phase, form
`INTERNAL_STATE_ERROR` without overwriting prior evidence. If no forward
failure is latched, latch it into `operationStatus`; otherwise preserve the
existing first forward failure. Before mutation, finish `UNCHANGED` with the
latched operation status. After mutation but before cleanup/reconciliation,
enter the existing bounded owner exactly once: safe-disable for
timer/periodic/CLKOUT/temperature, readback-only reconciliation for backup, or
the persistent access cleanup owner. If the impossible state occurs while
already in cleanup/reconciliation, do not restart or replay that owner; latch
`INTERNAL_STATE_ERROR` into `cleanupStatus` only when it is still `OK`, set
configuration final state `UNKNOWN` where applicable, and terminate with
`CONFIGURATION_CLEANUP_FAILED` or the persistent semantic
`EEPROM_CLEANUP_FAILED`. No impossible state returns `OK` or bypasses required
cleanup.

Delete old failure branches that simply stop after a partial mutation. Do not
add a generic blind cleanup write around them.

### 3. Refactor timer configuration into one explicit owner

Use this exact state order:

```text
TIMER_READ_CONTROL2_GUARD
TIMER_READ_CONTROL1
TIMER_WRITE_SAFE_CONTROL1
TIMER_WRITE_PRESET
TIMER_WRITE_FINAL_CONTROL1
TIMER_VERIFY
TIMER_CLEANUP_READ
TIMER_CLEANUP_WRITE
TIMER_CLEANUP_VERIFY
```

Requirements:

- Guard-read Control 2 and require TIE=0 before any write. If TIE is set,
  return `BUSY` and perform zero writes.
- The job owns TE staging; no other helper toggles TE behind it.
- Write Timer Low and Timer High as one two-byte transfer at `REG_TIMER_LOW`.
- Delete separate low/high write states and the dead
  `SetTimerReadTimerHigh`/`SET_TIMER_READ_TIMER_HIGH` state rather than leaving
  an unreachable branch.
- The safe state is the original implemented Control 1 with TE=0. Write it
  once and verify it.
- Success reads `0x0B..0x10` once and verifies timer low/high reserved bits,
  requested TD, TE, and preset.
- Success uses at most 6 driver callbacks. Worst failure/reconciliation uses
  at most 9.

### 4. Refactor periodic-update configuration and correct UIE semantics

Use these exact public signatures and parameter names:

```cpp
Status setPeriodicUpdate(
    PeriodicUpdateFrequency frequency,
    bool updateEventEnabled);

Status getPeriodicUpdate(
    PeriodicUpdateFrequency& frequency,
    bool& updateEventEnabled);
```

The Boolean is update-event enable, not merely interrupt enable. Public
Doxygen and maintained documentation must state:

- `false` disables both UF generation and the INT event;
- there is no UF-polling-only mode; and
- `getPeriodicUpdateFlag()` cannot observe a newly generated update event
  while UIE=0.

Use this exact state order:

```text
PERIODIC_READ_CONTROLS
PERIODIC_WRITE_SAFE_CONTROL2
PERIODIC_WRITE_USEL
PERIODIC_WRITE_FINAL_CONTROL2
PERIODIC_VERIFY
PERIODIC_CLEANUP_READ
PERIODIC_CLEANUP_WRITE
PERIODIC_CLEANUP_VERIFY
```

Requirements:

- Read Control 1 and Control 2.
- Write Control 2 with UIE=0 while preserving every other implemented bit.
- Write requested USEL in Control 1.
- Write requested UIE in Control 2.
- Verify both controls.
- Safe state is Control 2 with UIE=0, written once and verified.
- Never clear UF implicitly.
- Success uses at most 5 driver callbacks. Worst failure/reconciliation uses
  at most 8.

Add exactly one fake event hook:

```cpp
void triggerPeriodicUpdateEvent();
```

It sets UF and INT evidence only when UIE is one. Do not add a production hook
or broaden the fake into a scheduler.

### 5. Refactor CLKOUT configuration into one explicit owner

Use this exact state order:

```text
CLKOUT_READ_ACTIVE
CLKOUT_READ_GUARDS
CLKOUT_WRITE_SAFE_CONFIG
CLKOUT_WRITE_FINAL_PMU
CLKOUT_VERIFY
CLKOUT_CLEANUP_READ
CLKOUT_CLEANUP_WRITE
CLKOUT_CLEANUP_VERIFY
```

Requirements:

- Retain the precondition that CLKIE=0 and CLKF=0.
- Safe state is the preserved implemented PMU byte with NCLKE=1. Write one
  byte and verify the full implemented PMU value. With the guard satisfied,
  this proves output disabled.
- Success verifies active C0..C3 against the requested implemented masks
  before queuing C0/C2/C3 persistence.
- Success uses at most 5 driver callbacks. Worst failure/reconciliation uses
  at most 8.

Delete the old generic-register-update classification and duplicated partial
failure paths.

### 6. Refactor temperature-event configuration into one explicit owner

Use this exact state order:

```text
TEMPERATURE_READ_CONFIG
TEMPERATURE_WRITE_SAFE_CONTROL3
TEMPERATURE_WRITE_TIMESTAMP_CONTROL
TEMPERATURE_WRITE_THRESHOLDS
TEMPERATURE_WRITE_INTERRUPTS
TEMPERATURE_WRITE_DETECTION
TEMPERATURE_VERIFY
TEMPERATURE_CLEANUP_READ
TEMPERATURE_CLEANUP_WRITE
TEMPERATURE_CLEANUP_VERIFY
```

Requirements:

- Safe state is the preserved implemented Control 3 value with
  THE/TLE/THIE/TLIE all zero. Write it once and verify it.
- Threshold or timestamp-control bytes may be partially new, but detection and
  interrupt signaling must be proven off.
- Success verifies Control 3, relevant timestamp-control overwrite bits, and
  both thresholds.
- Preserve the vendor order: write interrupt enables before detection enables.
- Success uses at most 7 driver callbacks. Worst failure/reconciliation uses
  at most 10.

### 7. Add one fixed quiescence-guard owner

Use explicit precondition reads. Do not silently disable, clear, re-enable, or
restore an interrupt source around a requested reconfiguration.

Required guards:

- `setTimer()`/its timer job requires TIE=0 through
  `TIMER_READ_CONTROL2_GUARD`.
- `setAlarmTime()` and `setAlarmMatch()` require AIE=0.
- `setEviEdge()`, `setEviDebounce()`, `setEviOverwrite()`,
  `setEventSynchronizationEnabled()`, and EVI-source `resetTimestamp()`
  require EIE=0.
- `startSetBackupSwitchModeJob()` requires BSIE=0 through
  `BACKUP_READ_CONTROL3`.
- Do not apply the EIE guard to `setClkoutStopDelayEnabled()`. CLKDE configures
  CLKOUT stop delay after I2C STOP; it is not an EVI capture setting.

Extend the existing register-update owner with exactly:

```cpp
struct QuiescenceGuard {
  uint8_t reg = 0;
  uint8_t forbiddenMask = 0;
};
```

Add:

```text
JobState::REGISTER_UPDATE_READ_QUIESCENCE
```

and these `JobOp` fields:

```cpp
QuiescenceGuard quiescenceGuard{};
JobState quiescenceNextState = JobState::IDLE;
```

Add a `const QuiescenceGuard&` parameter to the private
`updateRegisterSingle()` and `updateRegisterBlock()` helpers. A zero mask skips
the guard. A nonzero mask starts at
`REGISTER_UPDATE_READ_QUIESCENCE`; a clear guard continues to the saved
existing single/block read state.

If the forbidden enable bit is set, return `BUSY` after the guard read and
perform zero writes. Never clear a status flag implicitly. Guarded alarm and
EVI register updates have a 3-callback cap. EVI timestamp reset prepends the
same guard to its existing read/clear/set sequence and has a 4-callback cap.

Public Doxygen for every guarded API must give this caller sequence:

```text
disable interrupt -> consume/clear flag if appropriate -> configure -> enable
```

For EVI, document that EIE=0 suppresses interrupt signaling but does not stop
timestamp capture. An external edge may still occur during configuration.
Describe this as vendor-recommended hardening, not physically reproduced fault
proof.

Do not create operation-specific copies of the generic `QuiescenceGuard` path.
Timer and backup retain their dedicated guard reads because their staged jobs
already own those controls.

### 8. Separate persistent operation evidence from cleanup evidence

Use these exact public layouts:

```cpp
struct PersistentReadResult {
  uint8_t eepromAddress = 0;
  uint8_t data[USER_EEPROM_JOB_MAX_BYTES] = {};
  uint8_t length = 0;
  Status operationStatus = Status::Ok();
  Status cleanupStatus = Status::Ok();
  bool persistentVerified = false;
  bool cleanupVerified = false;
};

struct UserEepromWriteReport {
  uint8_t offset = 0;
  uint8_t requestedLength = 0;
  uint8_t completedBytes = 0;
  uint8_t durablyVerifiedBytes = 0;
  Status operationStatus = Status::Ok();
  Status cleanupStatus = Status::Ok();
  bool cleanupVerified = false;
};
```

Add these active-item fields to `JobOp`:

```cpp
Status persistentOperationStatus = Status::Ok();
Status persistentCleanupStatus = Status::Ok();
```

`rememberFailure()` stores only the first forward failure. Cleanup stores only
the first cleanup failure. `finishJob()` never overwrites either field. Typed
result getters copy both fields into the public report before returning.

Set `persistentVerified=true` immediately when the final requested byte is
directly proven and `length` becomes the requested length. A later active C0
or Control 1 access-state cleanup failure must not erase durable data proof.
For a write report, preserve the exact completed and durably verified counts
when later cleanup fails.

For generic queue diagnostics, add to `SettingsSnapshot`:

```cpp
Status eepromOperationStatus = Status::Ok();
Status eepromCleanupStatus = Status::Ok();
```

Back them with durable driver members, not `JobOp`:

```cpp
Status _eepromOperationStatus = Status::Ok();
Status _eepromCleanupStatus = Status::Ok();
```

Batch rules:

1. When an idle configuration queue admits the first item of a new batch,
   reset both durable batch fields to `OK`.
2. Before clearing `JobOp` at every item boundary, latch its first non-OK
   forward and cleanup statuses into the corresponding batch member.
3. A cleanup failure cancels later queue entries, but both batch fields remain
   observable until a new batch, `begin()`, or `end()` resets them.
4. Preserve the fixed eight-entry queue and fixed buffers. Do not create a
   report queue.

Use this terminal precedence in `getEepromStatus()`:

1. `IN_PROGRESS` while active;
2. semantic `EEPROM_CLEANUP_FAILED` if cleanup failed, while preserving the
   exact cleanup code in `SettingsSnapshot`/typed report;
3. otherwise the first operation failure; and
4. otherwise `OK`.

`getPersistentReadJobResult()` and `getUserEepromWriteJobResult()` keep the
typed getter convention: after copying a matching completed report, return the
exact terminal job status. On `IN_PROGRESS` or `JOB_RESULT_UNAVAILABLE`, leave
the caller's output unchanged.

This change is evidence separation, not a persistence-protocol rewrite or a
new queue. Retain the vendor-correct Phase 1 deadlines, mutation cutoff, direct
persistent readback, cleanup reserve, fixed phase-check caps, and at-most-one
`WRITE_ONE` attempt per byte/item. Never turn a cleanup failure into loss of
already established durable proof.

### 9. Make `tick()` fallible and observable

Change the public signature exactly:

```cpp
Status tick(uint32_t nowMs);
```

Its implementation returns this call exactly:

```cpp
uint8_t instructionsUsed = 0;
return pollEeprom(nowMs, 1, instructionsUsed);
```

Behavior:

- before `begin()`, return `NOT_INITIALIZED`;
- while an ordinary job owns the engine, return `BUSY`;
- otherwise expose EEPROM progress or terminal status from `pollEeprom()`;
- do not clear a cached error to avoid repeated reporting; and
- the library never internally discards the fallible result, although an
  application may explicitly discard it.

Update all current internal/example callers so the repository builds. Do not
introduce a second polling path or make `tick()` infer whether an ordinary job
or EEPROM surface should own an application command. Strict CLI ownership is
a later phase.

## Fixed callback-bound table

The Phase 2 implementation and source-contract tests must enforce these exact
driver callback caps. Wait-only polls use zero callbacks.

| Job path | Success cap | Worst failure/reconciliation cap |
| --- | ---: | ---: |
| generic single/block register update | 2 | 2 |
| guarded alarm/EVI register update | 3 | 3 |
| EVI timestamp reset with EIE guard | 4 | 4 |
| timer configuration | 6 | 9 |
| periodic-update configuration | 5 | 8 |
| backup-mode configuration | 4 | 4 |
| CLKOUT configuration | 5 | 8 |
| temperature-event configuration | 7 | 10 |

Retain the Phase 1 caps for TEMP_LSB clear and other jobs. Persistent work
retains its fixed per-phase check caps plus whole/phase deadlines. For each
mutating path, prove structurally and in native tests that a failed or ambiguous
requested write cannot be dispatched twice.

Per `AGENTS.md`, each no-wait fixed-transfer job derives its whole driver bound
as `callback cap * Config::i2cTimeoutMs`. Time the application spends between
cooperative polls is not hidden driver execution time. A job with a vendor wait
or polling phase additionally uses its wrap-safe phase and whole-operation
deadlines.

The application-owner exception remains read-only: one callback may perform
documented bounded recovery plus at most one read retry, with both physical
attempts inside that callback's timeout. The driver still counts it as one
instruction. No Phase 2 mutating callback receives that exception.

## Required deletion and simplification audit

Before adding a field or helper, identify the current owner it extends and
delete the path it replaces. At minimum, the final diff must remove:

- the synchronous/generic `setBackupSwitchMode()` workflow and its superseded
  states, branches, and tests;
- generic `REGISTER_UPDATE` classification for CLKOUT, periodic update, and
  temperature configuration;
- duplicated operation/cleanup/mutation fields that are replaced by
  `ConfigurationJobReport`;
- separate timer low/high write states and the dead timer-high read state;
- failure paths that terminate after partial mutation without safe-state
  evidence;
- persistence logic that overwrites a forward failure during cleanup or clears
  established durable proof; and
- every `void tick()` declaration, definition, example, test, and maintained
  documentation claim.

Do not retain old behavior behind a wrapper. Do not add parallel state
machines, a generic cleanup registry, polymorphism, callbacks for state
transitions, or a second scheduler. If two code paths differ only because the
old path remains, delete the old path.

## Native fake and acceptance tests

Extend the existing native fake; do not add a fake to production paths. Tests
must use fixed storage and deterministic time/callback fault injection.

### Backup tests

Cover all of the following:

- observed raw BSM `00` and `11` transitions to DSM and LSM;
- one millisecond before and exactly at each 2 ms/10 ms activation boundary;
- callback-duration anchoring from the write's post-callback timestamp;
- no-I/O activation wait polls;
- `uint32_t` wraparound;
- already requested mode with zero mutation;
- enabled-to-enabled and enabled-to-Off transitions;
- optional persistence capacity checked before mutation;
- no persistence before active proof and settle;
- operation timeout one millisecond below the exact admission minimum, exactly
  at the minimum, exactly at
  `BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS`, and one millisecond above the
  maximum; both rejected cases perform zero I/O;
- whole deadline and mutation-cutoff expiry;
- every callback fault ordinal;
- failed/late write followed by requested/original/unknown readback outcomes;
- exact verification/reconciliation messages and packed mismatch detail for
  successful reads that prove the original state, requested state, or neither;
- exact `ConfigurationJobReport` and terminal status for every outcome;
- callback cap of 4; and
- no retry of the requested PMU write.

### Staged configuration tests

For timer, periodic update, CLKOUT, and temperature event configuration:

- inject a failure at every callback ordinal before and after each commit;
- test requested-write failure/timeout ambiguity and prove no replay;
- test cleanup gate already safe, proven unsafe, unreadable, cleanup write
  failure, and cleanup verification failure;
- assert the exact safe gate and implemented-bit readback;
- assert `UNCHANGED`, `REQUESTED_VERIFIED`,
  `SAFE_DISABLED_VERIFIED`, or `UNKNOWN` exactly;
- assert first `operationStatus`, first `cleanupStatus`, exact getter return,
  and unchanged output on unavailable/in-progress retrieval;
- assert `mutationAttempted` only for a dispatched forward write and track
  cleanup mutation separately;
- assert no persistence entry after failed or unverified active configuration;
- assert persistence admission only after requested proof;
- assert the fixed success and worst-case callback caps; and
- assert no requested-write retry.

Add timer TIE, backup BSIE, alarm AIE, and every named EVI EIE busy case. Every
guard-busy case performs its guard read, returns `BUSY`, and performs zero
writes. Prove `setClkoutStopDelayEnabled()` does not receive the EIE guard.
Prove guarded alarm/EVI updates cap at 3 and EVI timestamp reset caps at 4.

### UIE tests

Use the one fake `triggerPeriodicUpdateEvent()` hook. Prove UIE=0 creates
neither UF nor INT evidence, UIE=1 creates the documented evidence, no API
claims a UF polling-only mode, periodic reconfiguration does not clear UF, and
the staged state order and 5/8 callback caps hold.

### Persistence evidence and `tick()` tests

Cover:

- forward success plus cleanup success;
- forward failure plus cleanup success;
- forward success/proof plus cleanup failure;
- combined forward and cleanup failures;
- partial multi-byte proof followed by failure;
- exact operation and cleanup status codes/details;
- `persistentVerified` retained after later cleanup failure;
- completed and durably verified byte counts retained;
- generic queue batch reset, first-cause latching, multi-item continuation, and
  cleanup-failure cancellation;
- `getEepromStatus()` precedence;
- typed getter exact terminal return and unchanged output on unavailable or
  active result;
- `tick()` before begin, during ordinary ownership, during EEPROM progress,
  and after terminal success/failure;
- repeated `tick()` reporting of the cached terminal failure; and
- one callback instruction maximum per `tick()` call.

Retain Phase 1 deadline, mutation-cutoff, TEMP_LSB race, lifecycle,
noncopyability, callback normalization, impossible-state cleanup, and health
regression tests. Do not weaken an existing safety assertion to fit the new
implementation.

## Documentation requirements

Update the affected current public Doxygen and maintained authorities in the
same phase so they describe implemented behavior, not intended future work:

```text
include/RV3032/RV3032.h
include/RV3032/Status.h, if error wording is affected
include/RV3032/Config.h, if timing/authority wording is affected
README.md
docs/README.md
docs/ARCHITECTURE.md
docs/DEVICE_REFERENCE.md
docs/IDF_PORT.md
CHANGELOG.md under Unreleased
affected example help and source comments
tools/check_* contract guards
```

Document:

- the cooperative backup API, explicit BSM encoding, raw-disabled values,
  BSIE precondition, callback cap, exclusive deadline, exact minimum formula,
  PMU readback evidence, reconciliation-only behavior, and 2 ms/10 ms
  activation boundary;
- that backup success does not prove physical retention or voltage/topology
  safety;
- all four configuration final states, first-failure preservation, safe gates,
  no ambiguous write replay, callback caps, result-getter behavior, and
  persistence admission only after verified requested state;
- `updateEventEnabled` semantics, including UIE=0 suppressing both UF
  generation and the INT event;
- the TIE/AIE/EIE/BSIE caller quiescence sequence and the EVI capture caveat;
- separate persistent operation, durable proof, cleanup proof, and terminal
  precedence;
- `Status tick(uint32_t nowMs)` and its one-instruction delegation; and
- application-owned serialization.

Do not silently rewrite historical Prompt Suite 04, Prompt 05, or dated v2
reports. Do not hand-edit `include/RV3032/Version.h`. Phase 3 owns the final
breaking-version update and broad release/documentation integration; this
phase only records its own changes under `CHANGELOG.md` Unreleased and keeps
the current generated version internally consistent.

## Device-free verification

Run focused tests during implementation, then run and record at least:

```text
python scripts/generate_version.py check
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_docs_contract.py source
pio test -e native
pio run -e esp32s2dev
pio run -e esp32s3dev
python tools/hil_cli_runner.py --parser-self-test
python tools/hil_cli_runner.py --dry-run
```

On Windows, use the PlatformIO executable under
`%USERPROFILE%\.platformio\penv\Scripts\` if `pio` is not on `PATH`.

Also run:

- `git diff --check`;
- source scans proving the old backup setter, old state names, duplicate
  fields, and `void tick` contract are gone;
- source-contract checks for the exact state orders and callback caps; and
- a final `git status --short` to distinguish your edits from pre-existing
  work.

Do not pack or publish a release in this phase. Do not run physical HIL without
the existing explicit port, module, power, and backup-cell confirmations.

## Physical validation boundary

Software and fake-bus tests do not prove backup retention, INT/CLKOUT
electrical behavior, voltage safety, or actual activation timing. Unless
separately authorized, report physical HIL as `NOT RUN`.

When explicitly authorized in a later validation pass:

- remove VDD immediately after terminal DSM/LSM job success and check
  retention;
- verify there is no terminal success one millisecond before either activation
  boundary;
- run DSM only with a documented rechargeable/lower-voltage backup topology or
  controlled source; never switch a primary-cell fixture near VDD into DSM;
- for LSM, keep VDD safely above the maximum documented LSM threshold
  throughout the complete 10 ms activation interval before removal;
- validate INT/CLKOUT behavior through the documented quiescence sequence; and
- validate EEPROM cleanup under voltage/busy fault conditions.

Never infer those physical properties from the native fake.

## Cross-phase dependencies and stop conditions

This phase deliberately does not close the following work:

- Phase 3 must make the application Wire adapter honor the complete callback
  timeout. Until then, fake timing proves driver logic but the shipped example
  transport still prevents a full field timing claim.
- Phase 3 must perform strict CLI/single-owner integration and update commands
  for `startSetBackupSwitchModeJob()`, result getters, `tick()` status, and
  renamed update-event semantics. This phase must make current examples compile
  but must not implement a parallel interim CLI scheduler.
- Phase 3 owns the broad v3 documentation/release integration and generated
  version update to `3.0.0`.
- The final audit phase must re-audit all findings and run package checks and
  the complete release-candidate matrix.

Stop instead of improvising when:

- a Phase 1 prerequisite above is missing or incompatible;
- required state/callback bounds cannot fit the existing fixed-capacity owner
  without changing an explicitly fixed public contract;
- the vendor manual contradicts this prompt;
- unrelated user changes overlap the same behavior and intent cannot be
  determined safely; or
- completion requires physical HIL, a commit, a release, or authority not
  granted here.

Report the exact blocker and evidence. Do not mark the phase complete.

## Phase 2 definition of done

Phase 2 is complete only when all of the following are true:

- H-05, M-01, M-02, M-04, M-05, and M-06 are closed by code-level regression
  tests;
- the old backup setter and every superseded path are deleted;
- backup cannot return terminal success before required active PMU proof and
  the 2 ms/10 ms not-before boundary;
- backup performs at most one requested PMU write, never generic cleanup, and
  never exceeds 4 callbacks;
- timer, periodic update, CLKOUT, and temperature jobs expose exact terminal
  evidence and prove requested or safe-disabled state within their exact caps;
- no failed, timed-out, or ambiguous requested mutation is replayed;
- TIE/AIE/EIE/BSIE guards perform zero writes when busy;
- UIE=0 produces neither a new UF nor INT event in the fake and documentation;
- persistent operation failure, durable proof, and cleanup failure remain
  separate for typed jobs and generic queue batches;
- `tick()` returns exactly the one-instruction `pollEeprom()` result;
- public headers, implementation, fake, tests, current examples, maintained
  docs, changelog, and contract guards agree;
- all required device-free checks pass;
- the primary coder has completed the pre-edit subagent audits, re-tasked the
  subagents for post-diff audits, independently checked their evidence, and
  fixed every confirmed defect;
- physical HIL is reported as `NOT RUN` unless separately authorized and
  actually executed; and
- no unrelated rewrite, generated junk, unauthorized commit, tag, or release
  exists.

The final handoff must lead with achieved behavior, map each finding to closure
evidence, list material refactors and deletions, identify every changed file,
give exact test/build/contract-check results, report HIL as
`PASS`/`FAIL`/`NOT RUN`, state remaining Phase 3 dependencies and field risks,
and identify any genuine blocker. A green build alone is not completion.
