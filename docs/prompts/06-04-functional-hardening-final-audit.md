# AI Coder Prompt 06.4: Independent Functional Hardening Closure Audit

## Mission

This is phase 4 of 4. Independently audit the complete RV3032-C7 v3 functional
hardening implementation against every requirement in
`docs/reports/2026-07-14-full-library-functional-audit.md`, then correct every
confirmed defect in the existing owner. This is a closure audit, not permission
for feature expansion.

The audit covers:

- all findings `H-01` through `H-05`;
- all findings `M-01` through `M-09`;
- all findings `L-01` through `L-05`;
- complete password-management deletion and fail-closed raw access;
- all cross-phase API, timeout, callback, cleanup, persistence, example,
  documentation, version, and package contracts; and
- the verification and definition of done in audit sections 18 and 19.

Do not accept a green build as closure. Require code-path, test, source-contract,
and documentation evidence. If evidence exposes a defect, simplify or refactor
the owning implementation and delete obsolete code. Do not add a bandaid,
compatibility shim, silent retry, parallel abstraction, or fake-only production
hook.

Do not commit, tag, publish, flash hardware, issue physical EEPROM commands, or
run physical HIL without separate explicit authorization. Preserve unrelated
user changes.

The original audit baseline was revision `0e2eb0d`, manifest version `2.0.0`,
with 84/84 native tests and passing ESP32-S2/S3 builds; physical HIL was
`NOT RUN`. It is evidence only. Sequential hardening phases are expected to
make the current worktree dirty and change public API/version; never reset to
the baseline.

## 1. Authority, baseline, and prerequisite reconciliation

Use this authority order:

1. `AGENTS.md`.
2. The local Micro Crystal RV-3032-C7 vendor PDFs.
3. `docs/reports/2026-07-14-full-library-functional-audit.md`.
4. The four Prompt Suite 06 phase contracts.
5. Maintained public headers, implementation, tests, examples, and docs, which
   must all agree with the authorities above.

Prompt Suite 04, Prompt 05, and dated v2 reports are historical where they
conflict with v3. Do not rewrite historical evidence to manufacture agreement.

Read `docs/reference-pdfs/RV-3032-C7_App-Manual.pdf` Rev. 1.3 and the local
datasheet. Revalidate at least the facts cited from Application Manual pages
23, 42, 50-51, 59-60, 70-71, 78, 81-82, 84, 87, and 96: TEMP_LSB W0C,
password/register ranges, DSM/LSM activation delays, EEPROM timing/error
handling, update-event behavior, and quiescence procedures. If the local
edition differs, report the page/table conflict rather than guessing.

Before any audit conclusion or edit:

1. Read `AGENTS.md`, the full audit report, and all four Prompt Suite 06 files
   completely.
2. Record:

   ```powershell
   git rev-parse HEAD
   git status --short --branch
   git describe --tags --always --dirty
   git log -1 --oneline
   ```

3. Record the actual manifest version and generated-version check.
4. Inspect the complete diff from the implementation baseline, including
   untracked files. Do not assume prior phase completion messages are accurate.
5. Read all changed files completely and trace every affected public call into
   its terminal paths. Inspect at least:

   ```text
   include/RV3032/*.h
   src/RV3032.cpp
   examples/common/*.h
   examples/01_basic_bringup_cli/main.cpp
   test/test_native/FakeRv3032.h
   test/test_native/test_datetime.cpp
   README.md
   CHANGELOG.md
   docs/README.md
   docs/ARCHITECTURE.md
   docs/DEVICE_REFERENCE.md
   docs/IDF_PORT.md
   tools/check_*.py
   tools/hil_cli_runner.py
   scripts/generate_version.py
   platformio.ini
   library.json
   ```

6. Compare tests with behavior rather than counting test names. Existing tests
   can encode the original defect and must be corrected, not preserved.

If phases were applied out of order, partially, or against a changed HEAD,
reconcile every requirement against the live tree. Do not duplicate missing
types or preserve incompatible APIs. A genuine vendor/authority contradiction
is a blocker: report exact files, lines, manual page, and consequence.

## 2. Mandatory independent subagent workflow

The primary coder remains responsible for design decisions, edits, commands,
and all final claims.

Before the primary audit/fix pass, spawn exactly three bounded, read-only
subagents. They must not edit files or delegate further:

1. **Core safety reviewer**
   - password deletion and raw-range denial;
   - transport result normalization and health accounting;
   - callback-completion deadlines and timing guards;
   - TEMP_LSB W0C behavior;
   - lifecycle teardown and copy/move deletion;
   - checked conversions, dead-state removal, impossible-state routing, and
     counter wrap.
2. **Cooperative state-machine reviewer**
   - backup activation timing and reconciliation;
   - timer, periodic update, CLKOUT, and temperature configuration jobs;
   - quiescence guards and callback caps;
   - persistent read/write and generic-queue operation/cleanup evidence;
   - `tick()` propagation, mutation cutoffs, cleanup reserve, ambiguous-write
     handling, and fixed capacity.
3. **Integration and evidence reviewer**
   - Wire timeout, transaction release, and initialization;
   - strict CLI parsing and single cooperative owner;
   - RAM/timestamp output and README snippets;
   - presence/health semantics;
   - public Doxygen, maintained docs, changelog, version, contract tools,
     native tests, builds, HIL parser/dry-run, and package contents.

Require each reviewer to return a requirement-by-requirement table with exact
file/line and test evidence, not a general impression. Require explicit
negative-path tracing and identify any weakened or missing assertion.

The primary coder must independently reproduce every claimed issue before
changing code. Reject speculative redesign. Correct confirmed defects by
refactoring the existing owner and deleting superseded code.

After the primary coder has completed all corrections and focused tests,
re-task the same three subagents to audit the resulting full diff in their
original partitions. Even if the first pass found no defect, perform this
second review against the final tree. Independently validate the second-pass
findings, fix every confirmed issue, remove exposed dead code, and rerun the
affected focused tests plus the complete matrix. Subagents cannot declare the
work complete; only the primary coder may do so from verified evidence.

If the environment genuinely lacks subagent capability, stop and report the
process blocker before claiming closure.

## 3. Non-negotiable architecture and no-bandaid rules

The final implementation must retain:

- passive, zero-I/O `begin(const Config&)`;
- explicit one-callback raw `probe()`;
- unconditional zero-I/O `end()`;
- application ownership of I2C, serialization, admission, recovery, retry,
  and scheduling;
- fixed-capacity cooperative `pollJob()` and `pollEeprom()` work;
- at most one transport callback per counted instruction;
- `ensurePrimaryCellConfiguration()` as the sole synchronous multi-callback
  exception;
- no hidden retry of a mutating callback and no replay of an ambiguous write;
- wrap-safe exclusive deadlines and observable terminal failure;
- no heap allocation in steady library paths, no Wire/Arduino dependency in
  public library code, no logging, tasks, platform delay, or bus recovery; and
- preserved reserved bits and no writes to reserved register bits.

When fixing a defect:

- extend the current owner rather than add a second path;
- prefer deletion and a small explicit state machine over flags layered on the
  defective path;
- do not add deprecated wrappers, aliases, build flags, password stubs,
  generic frameworks, registries, schedulers, or compatibility facades;
- do not weaken a timeout, callback cap, validation rule, fault injection, or
  source guard;
- do not change a public contract not required by the audited v3 design; and
- remove obsolete states, fields, helpers, tests, and documentation exposed by
  the correction.

## 4. Exact global contracts to audit

Existing `Err` values must retain their numeric values. These exact values are
appended after `INCOHERENT_DATA = 22`:

```cpp
CONFIGURATION_CLEANUP_FAILED = 23,
TRANSPORT_CONTRACT_VIOLATION = 24,
INTERNAL_STATE_ERROR = 25
```

There is no password-specific error. All private retained `JobKind`,
`JobState`, and `EepromState` enumerators use `CAPS_CASE`.

Password management is deleted, not deprecated. Public types/methods,
credentials, workflow states, fake authentication, telemetry, and compatibility
stubs are absent. `CommandTable.h` retains vendor constants for
`REG_PASSWORD0..REG_PASSWORD3` (`0x39..0x3C`),
`REG_EEPROM_PASSWORD0..REG_EEPROM_PASSWORD3` (`0xC6..0xC9`),
`REG_EEPROM_PW_ENABLE` (`0xCA`), and `CONFIG_EEPROM_END` (`0xCA`). Private
`intersectsUnsupportedPasswordRange(uint8_t, size_t)` rejects any validated
block intersecting either range before I/O with `INVALID_PARAM` and static
message `"Password registers are unsupported"`. It does not block
`EEADDR/EEDATA/EECMD` `0x3D..0x3F`, configuration EEPROM `C0..C5`, or typed user
EEPROM `CB..EA`.

The product precondition is explicit: password protection must be disabled. A
part protected by other firmware is unsupported and requires out-of-band
service. The native fake retains only the silicon fact that direct reads of
write-only password ranges return zero and documents that it does not emulate
external password authorization; do not add credential or authentication
emulation back.

Require table-driven zero-I/O tests for every address in both blocked ranges,
intersecting blocks `0x38..0x39` and `0xC5..0xC6`, casts of `C6..CA` to
`ConfigurationEepromRegister`, generic configuration staging limited to
`C0..C5`, and typed user-EEPROM staging limited to `CB..EA`. Verify
`EEADDR/EEDATA/EECMD` `0x3D..0x3F` remain available to the managed engine.

The forbidden production symbol set is exact:

```text
PasswordCredential
PasswordProtectionEvidence
PasswordProtectionStatus
unlockPasswordProtection
startConfigurePasswordProtectionJob
getPasswordProtectionStatus
EepromState::WritePassword
EepromState::ApplyPassword
EepromState::ApplyPasswordBytes
EepromState::ApplyPasswordCredential
EepromState::FinalizePasswordEnable
EepromState::CleanupLockPassword
JobKind::PasswordProtection
JobOp::persistentUseAddressList
JobOp::persistentAddresses
JobOp::currentCredential
JobOp::newCredential
JobOp::passwordEnable
JobOp::passwordAuthorizationMayBeActive
RV3032::_passwordStatus
```

Persistent addressing is contiguous after deletion:

```cpp
const uint8_t address =
    static_cast<uint8_t>(_job.persistentAddress + _job.persistentIndex);
```

The public driver is noncopyable/nonmovable. All fallible APIs return `Status`;
output arguments are unchanged on failed checked conversions or unavailable
typed result retrieval.

## 5. Closure matrix: High findings

### H-01: callback completion is inside the advertised bound

Audit these exact helpers and evidence type:

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
TimedTransferResult readRegsBefore(uint8_t reg, uint8_t* buf, size_t len,
                                   uint32_t& nowMs, uint32_t deadlineMs);
TimedTransferResult writeRegsBefore(uint8_t reg, const uint8_t* buf, size_t len,
                                    uint32_t& nowMs, uint32_t deadlineMs);
```

Deadlines are exclusive and wrap-safe. A callback is dispatched only with
more than 1 ms remaining and receives
`min(_config.i2cTimeoutMs, remainingMs - 1)`. Every admitted wrap-safe interval
is strictly less than `2^31` ms. With `_nowMs()`, time is sampled before and
immediately after the callback; without it, the supplied timeout is added
conservatively. A callback exceeding its supplied timeout becomes effective
`I2C_TIMEOUT`; callback `OK` crossing an operation boundary becomes `TIMEOUT`.
Health observes the normalized callback result, except an observed callback
timeout violation is `I2C_TIMEOUT`; a pure operation deadline after transport
success does not become an I2C health failure.

Audit this effective-result precedence exactly:

1. local validation error when no callback was invoked;
2. normalized `TRANSPORT_CONTRACT_VIOLATION`;
3. observed callback-timeout violation as `I2C_TIMEOUT`;
4. another normalized callback failure; and
5. `TIMEOUT` when callback `OK` crossed an operation or phase deadline.

The last case returns static message
`"Transport callback crossed operation deadline"` and retains the callback
code in `detail`. Only a forward mutating `writeRegsBefore()` copies
`callbackInvoked` into mutation evidence; reads, guards, and verification do
not.

Every deadline-controlled state uses the earliest whole, phase, and applicable
mutation-cutoff boundary. `pollJob()`/`processEeprom()` carry one mutable current
time. No callback starts at/after a boundary. An attempted ambiguous mutation
is reconciled/cleaned up within the remaining bound and is never replayed.

For each no-wait operation with a fixed callback count, audit the complete
driver-operation bound as exactly
`callback cap * Config::i2cTimeoutMs`. Caller delay between cooperative polls
is not hidden driver execution time. Jobs with vendor waits or polling phases
also retain their absolute phase and whole-operation deadlines.

Audit exact-boundary, one-late, clipped-timeout, hook/no-hook, budget 1/N,
wraparound, mutation-overrun, cleanup, and impossible-admission tests. Confirm
`tools/check_core_timing_guard.py` rejects the old dispatch-only design.

### H-02: TEMP_LSB W0C clears cannot erase a new neighbor

Require:

```cpp
static constexpr uint8_t TEMP_LSB_W0C_MASK =
    EEPROM_EEF_MASK | TEMP_CLKF_MASK | TEMP_BSF_MASK; // 0x0B

Status startTempLsbFlagClear(uint8_t targetMask);
```

The only owner is `JobKind::TEMP_LSB_FLAG_CLEAR` with
`TEMP_LSB_CLEAR_READ`, `TEMP_LSB_CLEAR_WRITE`, and
`JobOp::tempLsbClearMask`. Admission accepts exactly one of EEF, CLKF, or BSF.
Target already clear causes no write; EEbusy returns `BUSY`; otherwise exact
payloads are CLKF `0x09`, EEF `0x03`, BSF `0x0A`. The cap is two callbacks.
Test every target/neighbor inter-poll race and exact write log.

### H-03: lifecycle always has a zero-I/O escape

Require:

```cpp
void RV3032::end() {
  _resetRuntimeState();
}
```

There is no busy early return or cancellation facade. Active ordinary, queued
EEPROM, active EEPROM, repeated-end, and rebind tests prove zero callbacks,
`UNINIT`, cleared bindings/work/health/restoration snapshots/latches, and new
context admission. Documentation states abandoned silicon writes cannot be
undone and product policy must be explicitly reinitialized after emergency
teardown.

### H-04: live cooperative state cannot be copied

Require exactly the default constructor and four deleted copy/move operations,
plus `<type_traits>` static assertions for constructible and assignable traits:

```cpp
RV3032() = default;
RV3032(const RV3032&) = delete;
RV3032& operator=(const RV3032&) = delete;
RV3032(RV3032&&) = delete;
RV3032& operator=(RV3032&&) = delete;
```

### H-05: backup success follows activation time

The old synchronous setter is absent. Require:

```cpp
static constexpr uint32_t BACKUP_SWITCH_OPERATION_TIMEOUT_MS = 250;
static constexpr uint32_t BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS = 1000;
Status startSetBackupSwitchModeJob(
    BackupSwitchMode mode,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = BACKUP_SWITCH_OPERATION_TIMEOUT_MS);
```

The encoder is explicit: Off `00`, Direct `01`, Level `10`; observed `00` and
`11` are disabled. Required states are
`BACKUP_READ_CONTROL3`, `BACKUP_READ_PMU`, `BACKUP_WRITE_PMU`,
`BACKUP_VERIFY_PMU`, and `BACKUP_WAIT_ACTIVATION`. BSIE is a guard. The sole
PMU write is verified and never replayed. Disabled-to-Direct waits 2 ms and
disabled-to-Level waits 10 ms from write callback completion. Wait polls use
zero callbacks. Persistence queues only after requested active proof, settle,
and `operationStatus.ok()`.

The only backup-specific scratch is:

```cpp
uint8_t backupOriginalPmu = 0;
uint8_t backupTargetPmu = 0;
uint32_t backupWriteCompletedMs = 0;
uint32_t backupActivationNotBeforeMs = 0;
bool backupActivationRequired = false;
```

The shared configuration report owns operation, cleanup, final-state, and
mutation evidence; backup must not duplicate them. Before mutation the job
proves optional C0 queue capacity. An already-requested PMU is
`REQUESTED_VERIFIED` with `mutationAttempted=false` and no wait/write.

Admission minimum is:

```cpp
4U * _config.i2cTimeoutMs +
    (mode == BackupSwitchMode::Direct ? 2U :
     mode == BackupSwitchMode::Level ? 10U : 0U) +
    1U
```

At a 100 ms callback timeout the minima are 403 ms Direct and 411 ms Level.
Admission accepts only the calculated minimum through the exact 1000 ms
maximum and rejects either boundary violation with `INVALID_PARAM` and zero
I/O. Audit requested/original/unknown reconciliation, no second write, exact
one-millisecond-early and boundary behavior, wrap, and topology-safe HIL plans.
Use this exact reconciliation mapping:

- requested PMU proven: `REQUESTED_VERIFIED`; complete the applicable wait;
  if the write callback failed, return its first effective status and queue no
  persistence;
- original PMU proven: `UNCHANGED`; return the original write failure, or
  `REGISTER_WRITE_FAILED` with message `"Backup PMU verification failed"`
  when callback `OK` did not produce target proof; and
- neither state proven or reconciliation read failure: `UNKNOWN`, preserve the
  first reconciliation cause in `cleanupStatus`, and return
  `CONFIGURATION_CLEANUP_FAILED`.

The implementation-only mismatch detail is exact:

```cpp
static constexpr int32_t registerMismatchDetail(uint8_t expected,
                                                uint8_t observed) {
  return (static_cast<int32_t>(expected) << 8) |
         static_cast<int32_t>(observed);
}
```

Use implemented-bit-masked bytes. Backup target verification uses
`registerMismatchDetail(targetImplemented, observedImplemented)`. A successful
read proving neither original nor requested sets `cleanupStatus` to
`REGISTER_WRITE_FAILED`, message
`"Backup PMU reconciliation inconclusive"`, with that detail. A failed
reconciliation read retains its exact effective status.

## 6. Closure matrix: Medium findings

### M-01: staged jobs expose their final hardware state

Require:

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
  ConfigurationFinalState finalState = ConfigurationFinalState::UNCHANGED;
  bool mutationAttempted = false;
};
```

Audit the five exact typed getters for timer, periodic update, backup, CLKOUT,
and temperature configuration. `IN_PROGRESS`/`JOB_RESULT_UNAVAILABLE` leave
output unchanged; otherwise the getter copies the report and returns the exact
terminal status.

The job kinds and getters are exact:

```text
SET_TIMER
SET_PERIODIC_UPDATE
SET_BACKUP_SWITCH_MODE
SET_CLKOUT_CONFIG
SET_TEMPERATURE_EVENT_CONFIG
```

```cpp
Status getSetTimerJobResult(ConfigurationJobReport& out) const;
Status getSetPeriodicUpdateJobResult(ConfigurationJobReport& out) const;
Status getSetBackupSwitchModeJobResult(ConfigurationJobReport& out) const;
Status getSetClkoutConfigJobResult(ConfigurationJobReport& out) const;
Status getSetTemperatureEventConfigJobResult(
    ConfigurationJobReport& out) const;
```

Audit the exact fixed scratch; no heap-backed/polymorphic report store or
parallel job registry is allowed:

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

The exact state orders are:

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

PERIODIC_READ_CONTROLS
PERIODIC_WRITE_SAFE_CONTROL2
PERIODIC_WRITE_USEL
PERIODIC_WRITE_FINAL_CONTROL2
PERIODIC_VERIFY
PERIODIC_CLEANUP_READ
PERIODIC_CLEANUP_WRITE
PERIODIC_CLEANUP_VERIFY

CLKOUT_READ_ACTIVE
CLKOUT_READ_GUARDS
CLKOUT_WRITE_SAFE_CONFIG
CLKOUT_WRITE_FINAL_PMU
CLKOUT_VERIFY
CLKOUT_CLEANUP_READ
CLKOUT_CLEANUP_WRITE
CLKOUT_CLEANUP_VERIFY

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

Audit these exact job-specific contracts, not only the state names:

- Timer guard-reads Control 2 and requires TIE=0 before any write. It writes
  Timer Low and Timer High in one two-byte transfer at `REG_TIMER_LOW`, uses
  the original implemented Control 1 with TE=0 as the safe gate, and verifies
  `0x0B..0x10` once for timer-low/high reserved bits, requested TD, TE, and
  requested preset.
- Periodic update reads Control 1/2, writes Control 2 with UIE=0 while
  preserving every other implemented bit, writes USEL, writes the requested
  UIE, and verifies both controls. Its safe value preserves Control 2 except
  UIE=0. It never clears UF implicitly.
- CLKOUT retains the CLKIE=0 and CLKF=0 guards. Its safe gate is the preserved
  implemented PMU byte with NCLKE=1, written and verified as a full implemented
  PMU value. Success verifies active C0..C3 against requested implemented
  masks, then queues exactly C0, C2, and C3 persistence.
- Temperature uses the preserved implemented Control 3 byte with
  THE/TLE/THIE/TLIE all zero, writes and verifies that safe state, and proves
  detection and interrupt signaling off before later configuration. Success
  verifies Control 3, relevant timestamp-control overwrite bits, and both
  thresholds, with interrupt enables written before detection enables.

Timer, periodic, CLKOUT, and temperature jobs store the first forward failure,
never replay requested writes, verify requested success, and use
operation-specific safe-disable cleanup after mutation. Cleanup reads the gate
before deciding to write; an ambiguous cleanup write is never retried. Verified
safe disable returns the original operation failure; unproved cleanup returns
`CONFIGURATION_CLEANUP_FAILED` with `UNKNOWN`. Backup is reconciliation-only
and never receives a second safe-disable PMU write.

A successful cleanup verification read with a mismatching safe gate sets the
first cleanup cause to `REGISTER_WRITE_FAILED`, message
`"Configuration safe-state verification failed"`, and
`registerMismatchDetail(safeImplemented, observedImplemented)`. Exact
transport/deadline failures remain unchanged, and an earlier cleanup-write
failure is never overwritten by any later verification result.

An impossible/default state never overwrites first-cause evidence. If no
forward failure exists, latch `INTERNAL_STATE_ERROR` into `operationStatus`;
otherwise preserve the existing operation failure. Before mutation, finish
`UNCHANGED`. After mutation but before cleanup, enter the existing job-specific
bounded cleanup/reconciliation owner exactly once. If already in cleanup or
reconciliation, do not restart or replay it: latch `INTERNAL_STATE_ERROR` into
an empty `cleanupStatus`, set `UNKNOWN` where applicable, and terminate with
`CONFIGURATION_CLEANUP_FAILED` or persistent `EEPROM_CLEANUP_FAILED`.

Audit every callback fault ordinal and these caps:

| Job path | Success | Worst failure/reconciliation |
| --- | ---: | ---: |
| generic single/block update | 2 | 2 |
| guarded alarm/EVI update | 3 | 3 |
| TEMP_LSB clear | 2 | 2 |
| 16-byte RAM write | 2 | 2 |
| EVI timestamp reset | 4 | 4 |
| coherent temperature read | 2 | 2 |
| status-first time snapshot | 2 | 2 |
| verified calendar set/Status clear | 7 | 7 |
| timer configuration | 6 | 9 |
| periodic update configuration | 5 | 8 |
| backup configuration | 4 | 4 |
| CLKOUT configuration | 5 | 8 |
| temperature-event configuration | 7 | 10 |

### M-02: durable operation proof is separate from cleanup proof

Audit public `PersistentReadResult` and `UserEepromWriteReport` for separate
`operationStatus` and `cleanupStatus`, durable proof/count fields, and
`cleanupVerified`. `persistentVerified` becomes true at final direct byte
proof and is not cleared by later cleanup failure.

The public layouts are exact:

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

Active `JobOp` contains exactly the separate
`persistentOperationStatus`/`persistentCleanupStatus` values needed for the
current item. `rememberFailure()` stores the first forward cause;
`finishJob()` does not overwrite it or the first cleanup cause.

`JobOp` owns active operation/cleanup statuses. Durable generic queue members
`_eepromOperationStatus` and `_eepromCleanupStatus` feed matching
`SettingsSnapshot` fields. A new idle batch resets them; item boundaries latch
the first failures before scratch is cleared. `getEepromStatus()` precedence
is exact: return `IN_PROGRESS` while active; otherwise return
`EEPROM_CLEANUP_FAILED` when cleanup failed while retaining the first exact
cleanup code in the snapshot/report; otherwise return the first operation
failure; otherwise return `OK`. Typed result getters copy evidence and return
the exact terminal job status.
Audit combined failures, proof-then-cleanup-failure, partial proof, and first
cause retention.

The durable public/private fields are exact:

```cpp
// SettingsSnapshot
Status eepromOperationStatus = Status::Ok();
Status eepromCleanupStatus = Status::Ok();

// RV3032
Status _eepromOperationStatus = Status::Ok();
Status _eepromCleanupStatus = Status::Ok();
```

### M-03: transport callbacks cannot control driver state

Require:

```cpp
struct RawTransferResult {
  Status status = Status::Ok();
  bool callbackInvoked = false;
};

static Status normalizeTransportResult(const Status& status);
```

Only `OK`, `I2C_ERROR`, `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, and
`I2C_BUS` pass. Any other callback code becomes
`TRANSPORT_CONTRACT_VIOLATION` with original code in `detail`. Local validation
has `callbackInvoked=false`; set true immediately before application code.
Tracked wrappers update health only for an invoked callback; raw `probe()` does
not. `shouldTrackHealth()` is absent. Inject `IN_PROGRESS`, `BUSY`, parameter
errors, and an unknown enum through both callbacks and prove terminal behavior,
one health failure, no reissue, and no borrowed invalid message pointer.

### M-04: normal EEPROM integration cannot discard failure

Require:

```cpp
Status tick(uint32_t nowMs);
```

It returns `pollEeprom(nowMs, 1, instructionsUsed)` exactly. Before begin it is
`NOT_INITIALIZED`; during an ordinary job it is `BUSY`; otherwise it exposes
EEPROM progress or terminal status without clearing a cached error.

### M-05: periodic update Boolean describes event generation

Require exact parameter naming:

```cpp
Status setPeriodicUpdate(PeriodicUpdateFrequency frequency,
                         bool updateEventEnabled);
Status getPeriodicUpdate(PeriodicUpdateFrequency& frequency,
                         bool& updateEventEnabled);
```

UIE=false disables UF generation and INT; there is no polling-only UF mode.
The fake's sole `triggerPeriodicUpdateEvent()` hook sets UF/INT evidence only
when UIE=1. The staged job writes safe UIE=0, USEL, final UIE, verifies both,
never clears UF implicitly, and has caps 5/8.

### M-06: live reconfiguration is guarded

Audit the fixed private owner:

```cpp
struct QuiescenceGuard {
  uint8_t reg = 0;
  uint8_t forbiddenMask = 0;
};
```

`REGISTER_UPDATE_READ_QUIESCENCE`, `quiescenceGuard`, and
`quiescenceNextState` extend existing single/block owners. Audit every exact
guarded API:

- `setTimer()` and `startSetTimerJob()` require TIE=0;
- `setAlarmTime()` and `setAlarmMatch()` require AIE=0;
- `setEviEdge()`, `setEviDebounce()`, `setEviOverwrite()`,
  `setEventSynchronizationEnabled()`, and EVI-source `resetTimestamp()`
  require EIE=0; and
- `startSetBackupSwitchModeJob()` requires BSIE=0.

Do not apply the EIE guard to `setClkoutStopDelayEnabled()`. A set guard returns
`BUSY` after one read with zero writes or implicit flag clear. Doxygen gives
`disable -> consume/clear -> configure -> enable` and states EIE=0 does not
stop capture.

The exact `JobOp` fields are:

```cpp
QuiescenceGuard quiescenceGuard{};
JobState quiescenceNextState = JobState::IDLE;
```

Private `updateRegisterSingle()` and `updateRegisterBlock()` each take a
`const QuiescenceGuard&`. A zero mask skips the guard; a nonzero mask enters
`REGISTER_UPDATE_READ_QUIESCENCE` and a clear guard continues to the saved
single/block read state.

### M-07: Wire callback timeout is real

Audit `examples/common/I2cTransport.h` for the exact noncopyable
`ScopedWireTimeout` owner, one wrap-safe callback deadline, remaining-time
application before every blocking call, timeout restoration, 1 ms release
reserve, final STOP after short staging, no retry, allowed transport statuses,
Arduino-ESP32 3.2.0 repeated-start behavior, uncontended owner mutex
precondition, and false return from `initWire()` on either `Wire.begin()` or
`Wire.setClock()` failure. Prove 5 ms effective bound, boundary/wrap behavior,
release of the next transaction, and initialization faults in the stub.

The timeout owner is exact:

```cpp
class ScopedWireTimeout {
 public:
  inline ScopedWireTimeout(TwoWire& wire, uint16_t timeoutMs)
      : _wire(wire), _previousTimeoutMs(wire.getTimeOut()) {
    _wire.setTimeOut(timeoutMs);
  }
  inline ~ScopedWireTimeout() { _wire.setTimeOut(_previousTimeoutMs); }

  ScopedWireTimeout(const ScopedWireTimeout&) = delete;
  ScopedWireTimeout& operator=(const ScopedWireTimeout&) = delete;

 private:
  TwoWire& _wire;
  uint16_t _previousTimeoutMs;
};
```

Callback validation/fault paths return only `OK`, `I2C_ERROR`,
`I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, or `I2C_BUS`. Local invalid
argument, invalid timeout, and short-stage failures use fixed details under
`I2C_ERROR`; Wire code 1 is also `I2C_ERROR`. Before transaction start more
than 1 ms must remain; `remaining-1` bounds normal work and the final
millisecond is reserved for one STOP on short staging.

```cpp
static constexpr int32_t I2C_DETAIL_INVALID_ARGUMENT = -1;
static constexpr int32_t I2C_DETAIL_INVALID_TIMEOUT = -2;
static constexpr int32_t I2C_DETAIL_SHORT_STAGING = -3;
```

### M-08: CLI cannot admit malformed or overlapping mutation

The only line reader is overflow-discarding
`cli_shell::readLine(String&)`. The old
local reader and permissive `readLine`/`parseInt(atoi)`/`match` helpers are
deleted. Require exact checked token helpers for U8/U16/U32/int32/float/bool01/
register, exact consumption and argument counts, decimal default, explicit
`0x` register support, finite float, and unchanged outputs on failure.

The token surface is exact:

```cpp
bool parseU8Token(const String&, uint8_t&);
bool parseU16Token(const String&, uint16_t&);
bool parseU32Token(const String&, uint32_t&);
bool parseInt32Token(const String&, int32_t&);
bool parseFloatToken(const String&, float&);
bool parseBool01Token(const String&, bool&);
bool parseRegisterToken(const String&, uint8_t&);
```

Every helper checks end pointer and `ERANGE`, rejects a negative unsigned
token, sets `errno = 0` before every `strto*` conversion, rejects `ERANGE`,
requires non-empty exact token consumption, range-checks before narrowing, and
leaves output unchanged on failure. Float also requires `isfinite()`. Only
register tokens accept explicit `0x`/`0X`; a leading zero is decimal, never
octal. Apply strict parsing and exact argument counts to calendar set, Unix
time, alarm, Boolean interrupt controls, CLKOUT, offset, timer, EVI, Status
clear, raw register access, verbose, and stress counts; reject every trailing
token before any RTC/I2C callback.

Require exact ownership:

```cpp
enum class PendingSurface : uint8_t { NONE, ORDINARY_JOB, EEPROM };
struct PendingOperation {
  PendingSurface surface = PendingSurface::NONE;
  const char* name = nullptr;
  Status ordinaryStatus = Status::Ok();
};
```

The owner polls ordinary work with budget 1 and retains its terminal status.
If `isEepromBusy()` is true, it changes the surface to `EEPROM` and returns
without admitting a command or dispatching another callback in that
iteration. If EEPROM is not busy, it reports `ordinaryStatus`, resets the
sole record to a value-initialized `PendingOperation{}`, and returns without a
second RTC/I2C callback. The EEPROM surface reports both outcomes at terminal
and then resets the record. Invalid/truncated commands perform zero callbacks.
No helper returns with untracked active work. The CLI removes its unconditional
loop-level `g_rtc.tick(millis())`; one pending owner must not race a second
EEPROM polling path.

### M-09: health state is not presence/identity

`isOnline()` is absent. Maintained code formats `DriverState` directly.
Passive initial READY is callback binding/observational health only. `probe()`
is one raw address-response/communication read and does not prove identity or
alter health. Audit public Doxygen, examples, self-tests, README, architecture,
and IDF notes for the same meaning.

## 7. Closure matrix: Low findings

### L-01: example completion and snippets are truthful

`cmd_ram_write()` records `IN_PROGRESS` and prints only terminal outcome.
`cmd_ts()` prints explicit empty/unset plus count when `timeValid=false`, never
a zero date. Maintained README snippets sample
`const uint32_t now = nowMs(nullptr);` and pass `now`. Compile-only snippet and
HIL expected-string checks exist.

### L-02: public conversions are checked

Public BCD conversion utilities and the silent `0x99` clamp are absent. Require:

```cpp
static Status computeWeekday(uint16_t year, uint8_t month, uint8_t day,
                             uint8_t& weekday);
static Status parseBuildTime(DateTime& out);
static Status unixToDateTime(uint32_t timestamp, DateTime& out);
static Status dateTimeToUnix(const DateTime& time, uint32_t& timestamp);
```

`weekdayFromValidDate()` is private. Dates are Gregorian `2000..2099`.
Unix accepts `946684800..4102444799`. Invalid input returns
`INVALID_DATETIME` and leaves output unchanged. Weekday remains a user-assigned
binary `0..6` field; the computation helper is optional caller policy.

### L-03: recover and health evidence agree

The exact presence wrapper is:

```cpp
Status _i2cWriteReadPresenceTracked(
    const uint8_t* tx,
    size_t txLen,
    uint8_t* rx,
    size_t rxLen);
```

It calls the raw wrapper, applies `mapPresenceError()` to the returned status,
and only then performs the sole health update when `callbackInvoked` is true.
`recover()` and `lastError()` both expose `DEVICE_NOT_FOUND` for the same
address NACK. Lifetime counters use ordinary unsigned increment and wrap from
`UINT32_MAX` to zero; saturation guards are absent.

### L-04: dead/impossible state cannot look successful

`SettingsSnapshot::beginInProgress`, `_beginInProgress`, obsolete EEPROM
states, and `SetTimerReadTimerHigh` are absent. Persistent addressing is
contiguous after password deletion. Impossible/default state returns
`INTERNAL_STATE_ERROR`; if mutation or persistent access cleanup evidence is
active, it enters that existing bounded reconciliation/cleanup owner rather
than return OK or invent another state machine. Source-contract checks cover
the route without adding a state mutator.

### L-05: register documentation states the real side effects

Weekday is documented as user-assigned three-bit binary `0..6`, not BCD. Every
simple AF/TF/UF/EVF/PORF/VLF Status clearer repeats or cross-links:

> Any Status-register write clears THF and TLF in silicon. If either omitted
> flag is already set at the guard read, the operation returns INVALID_PARAM
> without writing. An assertion after the guard read cannot be preserved.

Verified calendar set is documented separately: its name promises PORF/VLF
clear, report captures THF/TLF pre-clear evidence, and the Status write clears
THF/TLF unavoidably. `Status.h` says all fallible operations return Status, not
all functions.

## 8. Whole-operation persistence and deadline audit

In addition to the finding rows, trace the generic and typed EEPROM engines
from admission to terminal cleanup.

The exact generic cleanup reserve is:

```cpp
const uint32_t cleanupReserveMs =
    EEPROM_CLEANUP_READY_TIMEOUT_MS +
    6U * _config.i2cTimeoutMs +
    EEPROM_WRITE_SETTLE_MS;
```

The six callbacks are selected-active write/verify, active PMU C0
write/verify, and active Control 1 `0x10` write/verify. Typed admission requires
the reserve plus one forward callback timeout plus 1 ms. Configuration-read
default is 1000 ms, user read 1000 ms, user write 4000 ms, max 10,000 ms.
Generic queue item deadline stays 4000 ms and its cutoff is
`itemStartMs + (4000U - cleanupReserveMs)`. `begin()` validates admissibility.

`processPersistentJob(uint32_t& nowMs, bool& callbackUsed)` carries mutable
time. Whole, phase, and mutation boundaries use the earliest deadline. Waits
anchor from post-callback time. No forward write begins at/after cutoff; no
callback begins at/after the whole/phase deadline. Persistent phase check caps
remain fixed. Verify no `0x11` update-all or `0x12` refresh-all use and no blind
WRITE_ONE replay.

## 9. Maintained documentation, version, and package audit

Audit these maintained authorities together:

```text
include/RV3032/Status.h
include/RV3032/Config.h
include/RV3032/RV3032.h
include/RV3032/CommandTable.h
README.md
docs/README.md
docs/ARCHITECTURE.md
docs/DEVICE_REFERENCE.md
docs/IDF_PORT.md
CHANGELOG.md
example help/Doxygen
tools/check_* contract guards
```

They must document password unsupported access, passive lifecycle, closed
transport status domain, hard exclusive deadlines, callback caps, final-state
evidence, backup settle, persistence evidence, periodic UIE semantics,
quiescence, `tick()` status, Wire ownership, strict CLI, checked conversions,
presence semantics, and health wrap consistently.

`docs/README.md` identifies the full audit as active v3 and classifies Prompt
Suite 04, Prompt 05, and dated v2 reports as historical where conflicting.
Historical files remain historical; do not rewrite them to make grep output
empty.

If the live version is not already correct, set and verify it only with:

```powershell
python scripts/generate_version.py set 3.0.0
python scripts/generate_version.py check
```

`library.json` is the sole version source and must be exactly `3.0.0`.
`include/RV3032/Version.h` is generated; never hand-edit it.
`CHANGELOG.md` records the breaking work under `Unreleased` with `Removed`,
`Changed`, and `Fixed`, no release date. There is no commit, tag, publication,
or claimed release.

Inspect the packed archive, not just its exit status. It contains the intended
public library files, maintained README/examples/version metadata, and no test
tree, build cache, credentials, unrelated user files, or generated debris.

## 10. Required native behavioral audit

The native suite must prove at least these twenty matrices with fixed buffers
and callback/check caps:

1. deleted password symbols and zero-I/O denial of both password ranges;
2. callback normalization and invoked-vs-local health accounting;
3. post-callback deadline boundary, clipping, no-hook accounting, and wrap;
4. cleanup-reserve admission, cutoff, bounded cleanup, and hard no-dispatch
   boundaries;
5. TEMP_LSB target/neighbor race injection and exact payloads;
6. unconditional zero-I/O `end()` from every owner state;
7. compile-time noncopyable/nonmovable traits;
8. backup raw-disabled variants, 2/10 ms wait, proof, and no early persistence;
9. every callback fault ordinal for timer, periodic, CLKOUT, temperature, and
   backup jobs;
10. persistence operation/cleanup evidence combinations;
11. UIE=0 producing neither UF nor INT evidence;
12. TIE/AIE/EIE/BSIE guards producing zero writes when busy;
13. Wire whole-callback timeout, restoration, short-stage release, and init
    failures;
14. strict CLI invalid-input zero-I/O and overlength discard;
15. asynchronous 16-byte RAM completion and empty timestamp output;
16. passive READY without presence Boolean and consistent recover health;
17. invalid conversion domains with unchanged outputs;
18. reachable/default `INTERNAL_STATE_ERROR` cleanup routes;
19. read-only owner recovery plus one retry inside one callback bound and one
    driver instruction; and
20. lifetime counter wrap, proven without an unbounded call loop or test-only
    production setter: source-contract evidence shows ordinary `uint32_t`
    increment and no saturation guard, and a test-local compile-time assertion
    proves `UINT32_MAX + uint32_t{1}` is zero.

For every mutating state machine, assert the fixed callback cap and no retry of
an ambiguous requested write. Include exact success, pre-mutation failure,
post-mutation failure, committed-error, noncommitted-error, verification
mismatch, cleanup failure, exact-boundary, and wrap cases where applicable.

## 11. Exact verification commands

Run focused tests while correcting defects. After the final subagent diff
audit and final edit, run every command below from the repository root and
record the exact command, exit result, native test count, firmware RAM/flash
sizes, package path, parser result, and dry-run step count:

```powershell
python scripts/generate_version.py check
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_docs_contract.py source
pio test -e native
pio run -e esp32s2dev
pio run -e esp32s3dev
$packagePath = "dist/RV3032-C7-phase4-audit.tar.gz"
if (Test-Path -LiteralPath $packagePath) { throw "Preserve existing package artifact" }
pio pkg pack -o $packagePath .
python tools/check_docs_contract.py package $packagePath
python tools/hil_cli_runner.py --parser-self-test
python tools/hil_cli_runner.py --dry-run
git diff --check
```

The package path is audit-owned only when the preflight proves it did not
exist. If it exists, do not overwrite/delete it; choose another unique
audit-owned path and pass that same path to both package commands, recording
the equivalent commands. On Windows, use
`%USERPROFILE%\.platformio\penv\Scripts\pio.exe` or the
installed equivalent when `pio` is not on `PATH`. Do not substitute
`pio run -e native` for `pio test -e native`. Record any equivalent command
syntax honestly.

Inspect the archive contents after package validation. Resolve its absolute
path, prove it is inside this repository's `dist` directory, then remove only
that audit-created file with `Remove-Item -LiteralPath`. Preserve every
pre-existing user artifact. Re-run `git status --short` and prove no
build/package debris remains.

## 12. Mandatory source and forbidden-symbol audits

Run these scans, inspect every match, and record the expected/no-match result.
Adjust path separators for the current shell, not the semantic scope.

### 12.1 Deleted password production symbols: no matches

```powershell
rg -n "PasswordCredential|PasswordProtectionEvidence|PasswordProtectionStatus|unlockPasswordProtection|startConfigurePasswordProtectionJob|getPasswordProtectionStatus|WritePassword|ApplyPassword|ApplyPasswordBytes|ApplyPasswordCredential|FinalizePasswordEnable|CleanupLockPassword|persistentUseAddressList|persistentAddresses|currentCredential|newCredential|passwordEnable|passwordAuthorizationMayBeActive|_passwordStatus" include/RV3032/RV3032.h src/RV3032.cpp
```

Then prove the complete vendor constants and the narrow denial guard remain:

```powershell
rg -n "REG_PASSWORD0|REG_PASSWORD1|REG_PASSWORD2|REG_PASSWORD3|REG_EEPROM_PASSWORD0|REG_EEPROM_PASSWORD1|REG_EEPROM_PASSWORD2|REG_EEPROM_PASSWORD3|REG_EEPROM_PW_ENABLE" include/RV3032/CommandTable.h
rg -n "intersectsUnsupportedPasswordRange|Password registers are unsupported" src/RV3032.cpp include/RV3032/RV3032.h
```

Password text may remain only in the command-table vendor facts, narrow guard,
unsupported-access docs/tests, changelog history, and historical artifacts.

### 12.2 Removed/dead public and private paths: no live matches

```powershell
rg -n "isOnline\(|binaryToBcd|bcdToBinary|_beginInProgress|beginInProgress|ComparePersistent|VerifyWriteFlags|BeginVerifyPersistent|SetTimerReadTimerHigh|shouldTrackHealth" include src examples test README.md docs/README.md docs/ARCHITECTURE.md docs/DEVICE_REFERENCE.md docs/IDF_PORT.md
rg -n "void tick\(" include src examples test README.md docs/README.md docs/ARCHITECTURE.md docs/DEVICE_REFERENCE.md docs/IDF_PORT.md
rg -n "< UINT32_MAX" src include
```

Do not count historical prompts/reports as live-code failures.

### 12.3 No permissive CLI mutation path

```powershell
rg -n "read_line\(|cmd::readLine|parseInt\(|atoi\(|\.toInt\(|\.toFloat\(|sscanf\(" examples/01_basic_bringup_cli examples/common
```

Low-level `strto*` functions are expected only inside the exact checked token
helpers. Inspect their end-pointer, `ERANGE`, sign, finite, range, and unchanged
output behavior.

### 12.4 No forbidden architecture or EEPROM command use

```powershell
rg -n '#include[[:space:]]*[<"](Arduino|Wire|freertos)|\bWire\b|\bdelay\s*\(|\bnew\b|std::vector|std::queue|\bString\b' include/RV3032 src
rg -n "EEPROM_CMD_UPDATE_ALL|EEPROM_CMD_REFRESH_ALL|REG_EE_COMMAND|EECMD" src include/RV3032 test examples
rg -n "retry|recover" src include/RV3032
```

Inspect expected reference-only matches manually. `CommandTable.h` may list
vendor commands; tests may assert they are unused; documentation may explain
the read-only owner exception. There must be no production use of update-all,
refresh-all, hidden recovery, mutating retry, platform ownership, or steady
heap.

### 12.5 Contract consistency searches

```powershell
rg -n "presence and identity|All library functions return Status|weekday.*BCD|BCD.*weekday|advisory timeout|isOnline" README.md docs/README.md docs/ARCHITECTURE.md docs/DEVICE_REFERENCE.md docs/IDF_PORT.md include/RV3032 examples
rg -n "CONFIGURATION_CLEANUP_FAILED|TRANSPORT_CONTRACT_VIOLATION|INTERNAL_STATE_ERROR" include/RV3032/Status.h src test
rg -n "TEMP_LSB_W0C_MASK|0x09|0x03|0x0A" include/RV3032/CommandTable.h src test
rg -n "ScopedWireTimeout|PendingSurface|parseU8Token|parseRegisterToken|ConfigurationFinalState|RawTransferResult|TimedTransferResult|QuiescenceGuard" include src examples test
```

Expected positive searches are evidence locations, not proof by themselves.
Trace their behavior and tests.

## 13. Physical HIL boundary

Parser self-test and `--dry-run` are device-free. They are not physical HIL.

Do not flash, issue EEPROM commands, change backup mode on a powered fixture,
remove VDD, or run destructive HIL without fresh explicit authorization naming
the serial port, exact module, power/backfeed conditions, backup-cell chemistry
and voltage, possible persistent C0 write, and VDD-off scope.

When separately authorized, the physical plan must validate:

- no terminal backup success one millisecond before DSM/LSM activation;
- VDD removal immediately after terminal DSM/LSM success under the topology
  rules below;
- INT/CLKOUT behavior through quiescence sequences;
- EEPROM cleanup under voltage/busy faults; and
- the effective callback timeout on the real application bus owner.

DSM is permitted only with a documented rechargeable/lower-voltage backup
topology or controlled source. Never put a primary cell near VDD into DSM.
For LSM, hold VDD safely above the maximum LSM threshold throughout the 10 ms
activation interval before removal.

If physical HIL did not run, report exactly `NOT RUN`. Never convert native
fake evidence into a retention, voltage-safety, INT waveform, or physical
timing claim.

## 14. Definition of done

The complete hardening is done only when:

- password-management code is deleted and both password ranges fail closed;
- every finding `H-01..H-05`, `M-01..M-09`, and `L-01..L-05` has direct
  code, negative-path test, and maintained-documentation evidence;
- all five High findings have decisive regression tests reproducing the old
  failure boundary;
- every mutating job has a proven callback cap, terminal deadline, final-state
  evidence, and no ambiguous requested-write retry;
- headers, implementation, fake, tests, examples, maintained docs, changelog,
  contract guards, generated version, and package agree;
- all device-free commands and forbidden-symbol audits pass after the final
  edit;
- version is generated as `3.0.0` with Unreleased changelog entries;
- this hardening work introduced no unrelated rewrite, disabled test, TODO, or
  generated debris; pre-existing unrelated state is preserved and reported;
- any unauthorized commit or tag created by an earlier phase is reported as a
  blocker and is not reset, rewritten, or deleted without user authorization;
  and
- software verification is explicitly separated from physical HIL.

A failed command, unresolved authority conflict, missing phase, unverified
callback path, or unavailable mandatory subagent review is a genuine blocker.
Do not label the implementation complete.

## 15. Final evidence report and response

Produce a concise requirement-to-evidence closure table containing every
finding ID and:

- final behavior;
- owning files/symbols;
- decisive positive and negative tests;
- callback/deadline/failure-path evidence where applicable; and
- closure state `CLOSED` or `BLOCKED`.

Also report:

- actual starting and ending HEAD/worktree state;
- all deleted public/private paths and material simplifications;
- exact native count, both build results and sizes, contract checks, package
  validation, HIL parser self-test, and dry-run result;
- physical HIL as `PASS`, `FAIL`, or `NOT RUN`, with authorization and topology
  truth if it ran;
- package artifact cleanup and final `git diff --check` result;
- remaining field risks and external gates; and
- confirmation that no commit, tag, release, publication, or unauthorized
  hardware operation occurred.

Lead with achieved behavior, not activity. Do not report `CLOSED` from code
inspection alone, and do not hide a blocker behind a partial green matrix.
