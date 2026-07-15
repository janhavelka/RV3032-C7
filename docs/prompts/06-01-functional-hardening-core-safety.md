# AI Coder Prompt 06.1: Core Safety, Transport, Lifecycle, and Simplification

## Mission

Implement the first phase of the RV3032-C7 functional-hardening refactor.
This prompt is independently dispatchable. Treat it as the complete assignment
for this phase; do not require the operator to interpret the larger audit while
you work.

Close these revalidated findings:

| Finding | Severity | Required outcome |
| --- | --- | --- |
| H-01 | High | Cooperative deadlines bound callback completion, not merely dispatch. |
| H-02 | High | TEMP_LSB W0C clears cannot erase a neighboring flag asserted between polls. |
| H-03 | High | `end()` always provides deterministic, zero-I/O teardown. |
| H-04 | High | A live driver cannot be copied or moved. |
| M-03 | Medium | Transport callbacks cannot inject driver-control statuses into state machines. |
| L-02 | Low | Public date/conversion utilities are checked and preserve outputs on failure. |
| L-03 | Low | Recovery, cached health, and documented counter behavior agree. |
| L-04 | Low | Dead state is deleted and impossible state never returns success. |

Also implement the final product decision: **delete password authentication and
password-protection management from the library.** Do not repair, deprecate,
hide, stub, or feature-flag it.

Prefer deletion and refactoring of the existing owners over local patches.
The result must be simpler than the starting design. A change that wraps a
defective path while leaving the path available is not complete.

## Phase position and baseline

This is Phase 1 of Prompt Suite 06. It was derived from the functional audit:

```text
docs/reports/2026-07-14-full-library-functional-audit.md
audited revision: 0e2eb0d
audited manifest version: 2.0.0
baseline native tests: PASS, 84/84
baseline ESP32-S2 build: PASS
baseline ESP32-S3 build: PASS
baseline physical HIL: NOT RUN
```

Record the actual HEAD and `git status --short` before editing. The revision
above is evidence, not permission to reset the worktree. If HEAD differs,
reconcile every requirement below against the live source and report the
delta. Preserve unrelated user changes. Stop only for a factual conflict that
changes the required design; identify the exact file/line or vendor page.

This phase establishes contracts used by later phases. It does **not**
implement:

- H-05 backup-mode activation timing or the new backup job;
- M-01 staged-configuration final-state reports and safe-disable cleanup;
- M-02 persistence operation-versus-cleanup public evidence;
- M-04 `tick()` returning `Status`;
- M-05 periodic-update UIE semantics;
- M-06 quiescence guards;
- M-07 example Wire timeout ownership;
- M-08 CLI parsing and scheduling;
- M-09 public presence semantics;
- L-01 example completion corrections; or
- L-05 the complete documentation cleanup.

Do not pre-implement those later phases, add placeholder APIs for them, or
create a parallel architecture in anticipation of them. Do not perform the
final `3.0.0` version generation, package/release step, commit, or tag in this
phase. Later phases build on the exact transport, deadline, lifecycle, and
private-state contracts defined here.

## Authority and facts to verify

Use this authority order:

1. Repository `AGENTS.md`.
2. Local Micro Crystal documents:
   `docs/reference-pdfs/RV-3032-C7_App-Manual.pdf` Rev. 1.3 and the local
   datasheet.
3. This prompt.
4. Maintained source and documentation as current-implementation evidence.
5. Prompt Suite 04, Prompt 05, and dated v2 reports only as history when they
   conflict with this prompt.

Before editing, read `AGENTS.md` completely and verify the relevant vendor
facts directly:

- Application Manual page 23: TEMP_LSB EEF, CLKF, and BSF are sticky W0C
  flags; EEbusy is separate and read-only.
- Configuration/password register facts on pages 42 and 50-51: active
  password bytes are `0x39..0x3C`; persistent password/PW-enable bytes are
  `0xC6..0xCA`; direct EEPROM control registers are `0x3D..0x3F`.
- EEPROM timing/error behavior on pages 70-71: completion polling and waits
  are bounded and an ambiguous mutating command is not replayed.

Keep the binding architecture:

- `begin(const Config&)` validates and binds callbacks with zero I2C and zero
  wait callbacks.
- `probe()` is the explicit one-callback diagnostic and does not update
  health.
- `end()` performs zero I/O and ends the lifecycle.
- The application owns I2C, serialization, recovery, retry policy, and
  scheduling.
- Ordinary multi-transfer work remains cooperative, fixed-capacity, and
  caller-budgeted through `start...Job()`/`pollJob()` or
  `tick()`/`pollEeprom()`.
- One counted instruction invokes at most one driver transport callback.
- `ensurePrimaryCellConfiguration()` remains the sole synchronous
  multi-callback exception.
- Mutating callbacks are one physical attempt. Never replay a possibly
  committed write.
- No heap allocation, hidden retry, task, new scheduler, logging, direct
  `Wire`, platform delay, unbounded wait, or reserved-bit write is permitted.

## Mandatory working method and subagent audit

The primary coder owns all design decisions, edits, and truth claims. Use
subagents as bounded independent reviewers, not as a substitute for reading
the code yourself.

Before source edits, spawn three subagents with read-only assignments:

1. **Transport/deadline reviewer** - audit raw/tracked wrappers, every
   deadline-controlled state, persistence phase/cutoff accounting, health
   update sites, callback caps, and timing tests. Return file/line evidence and
   a requirement matrix; do not edit.
2. **Lifecycle/state reviewer** - audit `begin()`/`end()`, runtime reset,
   copy/move traits, TEMP_LSB clear paths, private enum reachability, default
   state handling, and conversion utilities. Return file/line evidence; do not
   edit.
3. **Password/API/test reviewer** - enumerate every public/private password
   symbol, fake behavior, test, document claim, raw-access path, dead field,
   and source-contract guard affected by deletion. Return an exact deletion
   and retention matrix; do not edit.

If the runtime cannot execute all three concurrently, run them sequentially;
do not broaden their assignments. Read their evidence and independently
confirm it before implementation.

After the implementation and focused tests, re-task the same reviewers to
audit the final diff from their original perspectives. Require them to check
the actual code and tests against this prompt, including negative/error paths,
not merely summarize the diff. The primary coder must then:

1. independently reproduce or reject each reviewer finding with evidence;
2. fix every confirmed defect by simplifying the owning code;
3. remove dead code exposed by the fix;
4. rerun affected tests and contract guards; and
5. inspect the complete final diff personally.

Do not accept a reviewer statement such as "looks correct" as evidence. Do not
leave TODOs, disabled tests, compatibility shims, temporary duplicate paths,
or deferred Phase 1 defects.

## 1. Append the exact error codes

Append these values after existing `INCOHERENT_DATA = 22` in `Err`. Never
renumber an existing value:

```cpp
CONFIGURATION_CLEANUP_FAILED = 23,
TRANSPORT_CONTRACT_VIOLATION = 24,
INTERNAL_STATE_ERROR = 25
```

`CONFIGURATION_CLEANUP_FAILED` is reserved now for the staged-job cleanup
contract completed in Phase 2. It must not be used as a generic transport or
password error in this phase. Do not add any password-specific error.

Correct the broad `Status.h` statement from "All library functions return
Status" to "All fallible operations return Status".

## 2. Delete password management completely

This is deletion, not deprecation.

### 2.1 Delete the public surface

Delete from `include/RV3032/RV3032.h` and from implementation:

```text
PasswordCredential
PasswordProtectionEvidence
PasswordProtectionStatus
unlockPasswordProtection(...)
startConfigurePasswordProtectionJob(...)
getPasswordProtectionStatus(...)
```

Do not leave `NOT_SUPPORTED` methods, aliases, deprecated wrappers, a build
option, an empty service, or placeholder types.

### 2.2 Delete the workflow and simplify its owners

Delete these private states and fields:

```text
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

Delete all password branches from `pollJob()`, `finishJob()`,
`_resetRuntimeState()`, `processPersistentJob()`, and `processEeprom()`. Delete
the three password API implementations.

The address-list mechanism has no non-password caller. Persistent addressing
becomes strictly contiguous:

```cpp
const uint8_t address =
    static_cast<uint8_t>(_job.persistentAddress + _job.persistentIndex);
```

If the existing `currentAddress` lambda then has one caller, inline it and
delete the lambda. Simplify failure cleanup so it deals only with active
C0/Control 1 EEPROM-access state. Do not retain generalized credential or
address-list machinery.

### 2.3 Retain the vendor register reference and fail closed

Do not delete these constants from `CommandTable.h`:

```text
REG_PASSWORD0..REG_PASSWORD3               // 0x39..0x3C
REG_EEPROM_PASSWORD0..REG_EEPROM_PASSWORD3 // 0xC6..0xC9
REG_EEPROM_PW_ENABLE                       // 0xCA
CONFIG_EEPROM_END                          // 0xCA
```

The command table remains a full silicon reference. Constants are not feature
support.

Add one private, implementation-only pure predicate:

```cpp
static bool intersectsUnsupportedPasswordRange(uint8_t reg, size_t len);
```

It receives an already validated, non-empty, non-wrapping block. Use 16-bit
start/end arithmetic. Return true when the inclusive block intersects either
`0x39..0x3C` or `0xC6..0xCA`. Normal block validation remains the sole owner of
zero-length and wrapping errors.

Call it from private `readRegs()` and `writeRegs()` after ordinary block
validation and before any transport callback. On intersection, return exactly:

```cpp
Status::Error(Err::INVALID_PARAM,
              "Password registers are unsupported");
```

Do not block `EEADDR`, `EEDATA`, or `EECMD` at `0x3D..0x3F`; the managed
EEPROM engine requires them. Keep:

- `ConfigurationEepromRegister` limited to `C0..C5`;
- generic configuration persistence limited to `C0..C5`; and
- typed user EEPROM limited to `CB..EA`.

Update `Config::enableEepromWrites` Doxygen: it authorizes configuration
`C0..C5` and typed user EEPROM `CB..EA` only. It never grants password-register
authority.

### 2.4 Simplify the fake and tests

Delete password credential storage, authorization state, protected-write
sequencing, telemetry, and workflow tests from
`test/test_native/FakeRv3032.h` and `test/test_native/test_datetime.cpp`.

Retain only the simple fake silicon fact that direct reads of write-only
password ranges return zero. Add a comment that the fake does not emulate a
part externally protected by other firmware. Do not add fake-only production
hooks.

Add table-driven tests proving:

- every single address in `0x39..0x3C` and `0xC6..0xCA` is rejected for public
  raw read and write with no callback-count change;
- blocks `0x38..0x39` and `0xC5..0xC6` are rejected before I/O;
- casts of `C6..CA` to `ConfigurationEepromRegister` are rejected before I/O;
- the generic configuration queue stages only `C0..C5`;
- typed user-EEPROM jobs stage only `CB..EA`; and
- a source-contract scan finds none of the deleted API/state names in the
  public header or implementation while all retained command-table constants
  still exist.

Document the supported-product precondition honestly: password protection
must be disabled. A module protected by other firmware is unsupported and
requires out-of-band service. Transport success alone cannot prove a WP
register changed on a protected part. Typed multi-transfer operations rely on
readback; ordinary single-write behavior exists only inside the supported
precondition.

## 3. Close the transport callback result domain

The callbacks are synchronous borrowed-buffer calls. A callback currently can
return `IN_PROGRESS`, `BUSY`, or another driver-control status and corrupt the
cooperative engines. Fix the boundary once. Do not add per-API filters.

Document both callbacks in `Config.h`:

- invocation is synchronous;
- buffers are borrowed only for the callback duration;
- `Status::msg` has static storage; and
- the only legal callback `Err` values are:

```text
OK
I2C_ERROR
I2C_NACK_ADDR
I2C_NACK_DATA
I2C_TIMEOUT
I2C_BUS
```

Add these private definitions:

```cpp
struct RawTransferResult {
  Status status = Status::Ok();
  bool callbackInvoked = false;
};

static Status normalizeTransportResult(const Status& status);
```

For any callback-returned code outside the allowlist, return:

```cpp
Status::Error(
    Err::TRANSPORT_CONTRACT_VIOLATION,
    "Transport callback returned invalid status",
    static_cast<int32_t>(status.code));
```

Do not retain an application-owned message pointer from an invalid-domain
status.

Change both overloads of `_i2cWriteRaw()` and `_i2cWriteReadRaw()` to return
`RawTransferResult`. Local null, buffer, and timeout validation returns
`callbackInvoked=false`. Set `callbackInvoked=true` immediately before calling
application code, then normalize immediately on return.

The target raw declarations are therefore:

```cpp
RawTransferResult _i2cWriteReadRaw(
    const uint8_t* txBuf, size_t txLen,
    uint8_t* rxBuf, size_t rxLen);
RawTransferResult _i2cWriteRaw(const uint8_t* buf, size_t len);
RawTransferResult _i2cWriteReadRaw(
    const uint8_t* txBuf, size_t txLen,
    uint8_t* rxBuf, size_t rxLen,
    uint32_t timeoutMs);
RawTransferResult _i2cWriteRaw(
    const uint8_t* buf, size_t len, uint32_t timeoutMs);
```

Plain tracked wrappers may continue returning `Status`, but they must call
`_updateHealth()` exactly when the raw result says `callbackInvoked=true`.
Timed tracked wrappers used by section 4 must preserve all evidence in
`TimedTransferResult` and apply the same one-update rule. Raw `probe()` returns
the normalized raw status without health tracking.

Delete `shouldTrackHealth()`. Attempted-versus-local classification comes only
from `callbackInvoked`, never from guessing an `Err` value.

Add injections for `IN_PROGRESS`, `BUSY`, `INVALID_PARAM`, and an unknown cast
value from each callback. Prove:

- none escapes the boundary;
- an ordinary job becomes idle with
  `TRANSPORT_CONTRACT_VIOLATION` rather than remaining forever active;
- persistence does not reissue that instruction;
- tracked use counts one attempted-transfer health failure; and
- raw `probe()` leaves health unchanged.

## 4. Make deadlines bound callback completion

### 4.1 Exact time/evidence types and helpers

Deadlines are exclusive. Completion at `nowMs == deadlineMs` is late. Every
admitted interval remains below `2^31` milliseconds.

Add this exact private result type:

```cpp
struct TimedTransferResult {
  Status status = Status::Ok();
  Status callbackStatus = Status::Ok();
  uint32_t completedAtMs = 0;
  bool callbackInvoked = false;
  bool deadlineCrossed = false;
  bool callbackTimeoutViolated = false;
};
```

`status` is the effective instruction result. `callbackStatus` is the
normalized application callback result and is meaningful only when
`callbackInvoked` is true. Do not add a hidden `_lastTransfer` member.

Add these exact private helpers:

```cpp
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

`remainingBefore()` is wrap-safe and false at or after the exclusive
deadline. `earlierDeadline()` compares the two remaining intervals relative
to `nowMs`; never compare raw unsigned absolute deadlines.

Keep timed transport logic in the existing transport-wrapper owner. Do not
duplicate callback invocation or health updates in state cases. The timed
register helpers must:

1. When `Config::nowMs` exists, sample it before dispatch and advance the
   by-reference `nowMs` to the later wrap-safe observation.
2. Reject with zero callbacks unless `remainingMs > 1`.
3. Pass `min(_config.i2cTimeoutMs, remainingMs - 1)` to the explicit-timeout
   raw wrapper through a timed tracked wrapper.
4. With a clock hook, sample `_nowMs()` immediately after callback return.
5. Without a hook, conservatively advance `nowMs` by the exact timeout passed,
   even if the callback reports an earlier return.
6. If a hook-observed callback duration exceeds the supplied timeout, set
   `callbackTimeoutViolated=true`, use effective `I2C_TIMEOUT`, and feed that
   timeout to health rather than a callback-returned `OK`.
7. If post-callback time is at or after the exclusive boundary, set
   `deadlineCrossed=true` and reject the late result.

On every return, set `completedAtMs` to the final `nowMs` observation used for
the decision. After a callback, update both `completedAtMs` and the
by-reference `nowMs` before the state machine inspects the result. A no-callback
validation/deadline rejection reports the current admitted `nowMs` and leaves
`callbackStatus` at its default because it is not meaningful.

The timed register helpers must enforce the same register block validation and
unsupported-password intersection rule as plain `readRegs()`/`writeRegs()`.
Share the validation owner or call a common helper; do not create a timed path
that bypasses section 2.

Use this effective-result precedence exactly:

1. local validation error when no callback was invoked;
2. normalized `TRANSPORT_CONTRACT_VIOLATION`;
3. observed callback-timeout violation as `I2C_TIMEOUT`;
4. any other normalized callback failure; and
5. `TIMEOUT` when the callback returned `OK` but crossed the operation or
   phase deadline.

For case 5, return:

```cpp
Status::Error(Err::TIMEOUT,
              "Transport callback crossed operation deadline",
              static_cast<int32_t>(callbackStatus.code));
```

The normalized callback result normally feeds health. An observed callback
timeout feeds health as `I2C_TIMEOUT`. A pure operation-deadline failure after
a transport-successful callback does not turn the transport-health observation
into an I2C failure.

Only a forward mutating call through `writeRegsBefore()` copies
`callbackInvoked` into mutation-attempt evidence. Reads, guards,
verification, and cleanup dispatches do not mark forward mutation. Copy the
evidence immediately after helper return and before any state transition.

### 4.2 Apply the earliest applicable boundary everywhere

Every deadline-controlled cooperative state must use `readRegsBefore()` or
`writeRegsBefore()`. It must not call plain `readRegs()` or `writeRegs()`.

Immediately before dispatch, choose the earliest applicable absolute
boundary:

- the whole-operation deadline, always;
- the active EEPROM phase deadline for ready, READ_ONE, WRITE_ONE, and cleanup
  polling; and
- the mutation cutoff for a forward mutating write.

The raw/timed wrapper owns the exact dispatch point. A state copies its Boolean
evidence and never marks an attempt because it merely called a helper. If an
attempted mutation fails or crosses a phase/mutation cutoff while time remains
before the whole deadline, enter the existing bounded readback reconciliation
or persistent cleanup. Never replay `WRITE_ONE` or another ambiguous requested
write.

If a non-conforming callback overruns its passed timeout and reaches the whole
deadline, dispatch no further callback. Return the applicable existing
cleanup-failure terminal status and mark hardware/access state unverified. A
whole-operation deadline is a hard no-dispatch boundary.

Refactor `pollJob()` and `processEeprom()` to carry one mutable
`currentNowMs` through an instruction loop. Delete time derivation based on
`instructionsUsed * i2cTimeoutMs`. Change the exact signature to:

```cpp
Status processPersistentJob(uint32_t& nowMs, bool& callbackUsed);
```

Anchor 1 ms and 10 ms vendor waits from the same post-callback timestamp.

This phase must apply the timing engine to all currently retained cooperative
paths. Do not redesign the staged timer/periodic/CLKOUT/temperature ownership
or add Phase 2 result-report APIs yet. Phase 2 will add their fail-safe terminal
evidence and may replace their internal state lists; code added here must not
make that refactor harder.

### 4.3 Derive persistence cleanup reserve

Delete the literal 300 ms cleanup reserve. After password deletion use:

```cpp
const uint32_t cleanupReserveMs =
    EEPROM_CLEANUP_READY_TIMEOUT_MS +
    6U * _config.i2cTimeoutMs +
    EEPROM_WRITE_SETTLE_MS;
```

The six callbacks are:

1. selected-active write;
2. selected-active verify;
3. active PMU C0 write;
4. active PMU C0 verify;
5. Control 1 register `0x10` write; and
6. Control 1 verify.

The Control 1 pair is not configuration EEPROM C1/offset. Cleanup-ready has
its own phase deadline and fixed check cap.

Set the mutation cutoff from the whole deadline minus this reserve. Admission
returns `INVALID_PARAM` with zero I/O unless the whole timeout leaves at least:

```text
cleanupReserveMs + one forward callback timeout + one millisecond
```

Use that derived minimum in admission and Doxygen. Keep the public maximum at
10,000 ms. Change one-byte configuration-read default from 250 ms to 1,000 ms.
Retain user-read default 1,000 ms and user-write default 4,000 ms.

For every generic configuration-queue item retain a 4,000 ms whole deadline
and replace literal 3,700 ms with:

```cpp
mutationCutoffMs = itemStartMs + (4000U - cleanupReserveMs);
```

At `begin()`, validate that configured callback timeout makes this expression
admissible. The specified `1..100` ms `i2cTimeoutMs` range does. Typed
persistent starts apply the same derived-minimum validation to their caller
timeout. Preserve every existing fixed ready/check cap.

### 4.4 Fixed callback caps

One counted instruction still invokes at most one driver callback. Wait polls
consume none. No-wait fixed-transfer jobs use the derived whole-operation bound
`callback cap * Config::i2cTimeoutMs`; caller delay between polls is not hidden
driver execution time. Vendor waits and polling additionally obey absolute
phase/whole deadlines.

The complete target cap table is fixed here so later phases cannot drift:

| Job path | Success cap | Worst failure/reconciliation cap |
| --- | ---: | ---: |
| generic single/block register update | 2 | 2 |
| guarded alarm/EVI register update | 3 | 3 |
| TEMP_LSB flag clear | 2 | 2 |
| 16-byte cooperative user RAM write | 2 | 2 |
| EVI timestamp reset with EIE guard | 4 | 4 |
| coherent temperature read | 2 | 2 |
| status-first time snapshot | 2 | 2 |
| verified calendar set/Status clear | 7 | 7 |
| timer configuration | 6 | 9 |
| periodic-update configuration | 5 | 8 |
| backup-mode configuration | 4 | 4 |
| CLKOUT configuration | 5 | 8 |
| temperature-event configuration | 7 | 10 |

In this phase, assert and test the exact caps for paths whose structure is
owned here: generic updates, TEMP_LSB clear, RAM write, coherent temperature,
snapshot, verified calendar, and current persistent phases. Do not exceed any
cap in the table. Phase 2 owns the new quiescence callbacks, backup job, and
staged-job cleanup states that make the guarded/configuration worst-case rows
fully testable. Do not fake those future callbacks or reports in Phase 1.

Retain the application-owner exception: one read-only callback may implement
documented bounded recovery plus at most one read retry. Both physical attempts
must fit the single callback timeout; the driver still counts one callback and
one instruction. A mutating callback is exactly one physical attempt. Include
the owner read-retry timing path and callback-overrun behavior in tests.

### 4.5 Deadline acceptance tests and contract guard

Add deterministic tests for:

- every retained terminal ordinary deadline state at exact boundary and 1 ms
  late;
- every EEPROM phase and cleanup state at exact boundary and 1 ms late;
- timeout argument clipping;
- live clock-hook and no-hook accounting;
- `maxInstructions=1` and values greater than one;
- `uint32_t` wraparound;
- zero callbacks at or after an applicable boundary;
- a mutating callback crossing its cutoff entering reconciliation/cleanup
  only;
- a callback returning `OK` after the operation deadline returning `TIMEOUT`;
- a callback exceeding its passed timeout returning `I2C_TIMEOUT` and updating
  health once;
- impossible admission budgets producing zero I/O; and
- no ambiguous requested-write replay.

Update `tools/check_core_timing_guard.py` so the old pre-dispatch-only design
cannot pass. It must check use of the new helpers, mutable-now propagation,
the derived cleanup reserve/cutoff, and absence of literal old reserve/cutoff
logic. Do not rely only on source text; native behavioral tests remain
mandatory.

## 5. Give TEMP_LSB W0C flags one safe owner

Add to `include/RV3032/CommandTable.h`:

```cpp
static constexpr uint8_t TEMP_LSB_W0C_MASK =
    EEPROM_EEF_MASK | TEMP_CLKF_MASK | TEMP_BSF_MASK; // 0x0B
```

Add exactly one private admission helper:

```cpp
Status startTempLsbFlagClear(uint8_t targetMask);
```

Add only these dedicated private values/field:

```text
JobKind::TEMP_LSB_FLAG_CLEAR
JobState::TEMP_LSB_CLEAR_READ
JobState::TEMP_LSB_CLEAR_WRITE
JobOp::tempLsbClearMask
```

Route `clearClockOutputFlag()`, `clearEepromErrorFlag()`, and
`clearBackupSwitchFlag()` through this helper. Delete their duplicated generic
register-update setup.

Admission accepts exactly one target equal to `EEPROM_EEF_MASK`,
`TEMP_CLKF_MASK`, or `TEMP_BSF_MASK`. Any other or combined mask returns
`INVALID_PARAM` with zero I/O. The job cap is two callbacks.

The read state:

- returns `OK` without a write if the target is already clear;
- returns `BUSY` without a write if EEbusy is set; and
- otherwise computes only
  `TEMP_LSB_W0C_MASK & static_cast<uint8_t>(~targetMask)`.

The write state writes that exact one-byte payload once. It never copies the
temperature fraction or any earlier neighboring flag value. Exact payloads:

| Target | Payload |
| --- | --- |
| CLKF | `0x09` |
| EEF | `0x03` |
| BSF | `0x0A` |

Do not reject a pre-existing neighboring EEF/CLKF/BSF; writing one preserves
it. Keep the EEbusy guard and documented VDD-power requirement for BSF clear.

For each target, test each neighbor asserting after the read callback, all
neighbors asserted, target already clear, EEbusy, exact transport write log,
and zero collateral clears. The fake must permit deterministic event injection
between budget-one polls.

## 6. Make lifecycle ownership explicit

### 6.1 Unconditional zero-I/O `end()`

Keep the public signature and implement exactly:

```cpp
void RV3032::end() {
  _resetRuntimeState();
}
```

Delete the busy early return. Do not add cancellation, a Boolean result, or a
second teardown method.

Document that queued and active work is abandoned; an already-issued silicon
write cannot be undone. Emergency teardown can irreversibly discard the saved
C0/Control 1 restoration snapshot. Re-reading the device does not resume that
cleanup. After rebind, the application must explicitly reinitialize affected
PMU/Control 1 product policy or follow its documented power-cycle and
reprovisioning procedure before deployment.

Replace tests that preserve work across `end()` with:

- active ordinary job;
- queued generic EEPROM work;
- active EEPROM work;
- repeated `end()`; and
- immediate `begin()` using a different context.

Every case makes zero callbacks, ends in `DriverState::UNINIT`, clears
callbacks/context, queues, jobs, health, saved restoration snapshot, and the
primary-cell same-lifecycle latch, and permits rebinding. Do not test or
document abandoned-work resumption.

### 6.2 Delete copy and move

Add exactly to the public class:

```cpp
RV3032() = default;
RV3032(const RV3032&) = delete;
RV3032& operator=(const RV3032&) = delete;
RV3032(RV3032&&) = delete;
RV3032& operator=(RV3032&&) = delete;
```

Add Doxygen: the object owns live fixed-capacity cooperative state and a
borrowed transport binding, so copying or moving could duplicate an in-flight
wear-limited mutation.

Add native `static_assert` checks for all four constructible/assignable copy
and move traits using `<type_traits>`.

## 7. Remove unsafe public utilities and dead private state

### 7.1 Checked public conversion contracts

Delete public `binaryToBcd()` and `bcdToBinary()`. They have no production
caller and expose unchecked internal encoding. Keep only private strict BCD
helpers for values already validated. Delete the silent `0x99` clamp.

Use these exact public signatures:

```cpp
static Status computeWeekday(
    uint16_t year, uint8_t month, uint8_t day, uint8_t& weekday);

static Status parseBuildTime(DateTime& out);

static Status unixToDateTime(
    uint32_t timestamp, DateTime& out);

static Status dateTimeToUnix(
    const DateTime& time, uint32_t& timestamp);
```

Keep `isValidDateTime()` as a Boolean predicate. Add exactly:

```cpp
static uint8_t weekdayFromValidDate(
    uint16_t year, uint8_t month, uint8_t day);
```

This is a private helper and is called only after full date validation.

`computeWeekday()` validates year `2000..2099` and a complete Gregorian date
before entering the bounded calculation. `unixToDateTime()` accepts exactly:

```text
946684800..4102444799
2000-01-01 00:00:00 through 2099-12-31 23:59:59 UTC
```

`dateTimeToUnix()` accepts the same calendar domain. Invalid date, timestamp,
or build string returns `INVALID_DATETIME`. Success returns `OK`. Every output
argument remains byte-for-byte unchanged on failure. Update every internal,
test, example, and maintained documentation caller in the same phase.

Do not conflate this utility with weekday storage policy: RV3032 weekday is a
user-assigned three-bit binary value `0..6`; the utility merely calculates one
possible value for callers.

### 7.2 Delete dead fields/states and normalize private names

Delete:

```text
SettingsSnapshot::beginInProgress
RV3032::_beginInProgress
EepromState::ComparePersistent
EepromState::VerifyWriteFlags
EepromState::BeginVerifyPersistent
JobState::SetTimerReadTimerHigh
```

Delete every check and reset of `_beginInProgress`. Passive `begin()` requires
no internal I/O bypass.

Because the private state machines are already structurally edited, rename
every retained `JobKind`, `JobState`, and `EepromState` enumerator to
`CAPS_CASE` in the same mechanical change. All new enumerators in this prompt
already use target spelling. Do not mix naming styles, and do not change logic
merely to rename.

After the Phase 1 deletions/addition, the exact retained private enumerator
sets are:

```cpp
enum class EepromState : uint8_t {
  IDLE,
  READ_CONTROL1,
  ENABLE_EERD,
  VERIFY_EERD,
  WAIT_READY,
  READ_ACTIVE_C0,
  WRITE_SAFE_C0,
  VERIFY_SAFE_C0,
  WRITE_ADDR,
  VERIFY_ADDR,
  WRITE_SENTINEL1,
  VERIFY_SENTINEL1,
  PRE_READ_BUSY1,
  READ_CMD1,
  WAIT_READ1,
  POLL_READ1,
  READ_DATA1,
  WRITE_SENTINEL2,
  VERIFY_SENTINEL2,
  PRE_READ_BUSY2,
  READ_CMD2,
  WAIT_READ2,
  POLL_READ2,
  READ_DATA2,
  CLEAR_EEF,
  VERIFY_EEF,
  WRITE_DATA,
  VERIFY_DATA,
  WAIT_READY_PRE_CMD,
  WRITE_CMD,
  WAIT_WRITE_SETTLE,
  WAIT_READY_POST_CMD,
  CLEANUP_WAIT_READY,
  RESTORE_SELECTED_ACTIVE,
  VERIFY_SELECTED_ACTIVE,
  RESTORE_ACTIVE,
  VERIFY_ACTIVE,
  RESTORE_CONTROL1,
  VERIFY_CONTROL1,
  SETTLE
};

enum class JobKind : uint8_t {
  NONE,
  SET_TIMER,
  REGISTER_UPDATE,
  TEMP_LSB_FLAG_CLEAR,
  WRITE_USER_RAM,
  READ_COHERENT_TEMPERATURE,
  READ_TIME_SNAPSHOT,
  SET_TIME_VERIFIED,
  PERSISTENT_READ,
  USER_EEPROM_WRITE
};

enum class JobState : uint8_t {
  IDLE,
  SET_TIMER_READ_CONTROL1,
  SET_TIMER_WRITE_CONTROL1,
  SET_TIMER_WRITE_LOW,
  SET_TIMER_WRITE_HIGH,
  SET_TIMER_WRITE_FINAL_CONTROL1,
  REGISTER_UPDATE_READ,
  REGISTER_UPDATE_WRITE,
  TEMP_LSB_CLEAR_READ,
  TEMP_LSB_CLEAR_WRITE,
  EVI_RESET_READ,
  EVI_RESET_WRITE_CLEAR,
  EVI_RESET_WRITE_SET,
  REGISTER_BLOCK_UPDATE_READ,
  REGISTER_BLOCK_UPDATE_WRITE,
  CLKOUT_READ_ACTIVE,
  CLKOUT_READ_GUARDS,
  CLKOUT_WRITE_STOPPED_CONFIG,
  CLKOUT_WRITE_FINAL_PMU,
  TEMPERATURE_CONFIG_READ,
  TEMPERATURE_CONFIG_WRITE_DISABLED,
  TEMPERATURE_CONFIG_WRITE_TIMESTAMP_CONTROL,
  TEMPERATURE_CONFIG_WRITE_THRESHOLDS,
  TEMPERATURE_CONFIG_WRITE_INTERRUPTS,
  TEMPERATURE_CONFIG_WRITE_DETECTION,
  WRITE_USER_RAM_CHUNK,
  READ_TEMPERATURE_FIRST,
  READ_TEMPERATURE_SECOND,
  READ_TIME_STATUS,
  READ_TIME_CALENDAR,
  SET_TIME_READ_STATUS_BEFORE,
  SET_TIME_WRITE_CALENDAR,
  SET_TIME_VERIFY_CALENDAR,
  SET_TIME_READ_STATUS_BEFORE_CLEAR,
  SET_TIME_WRITE_STATUS,
  SET_TIME_READ_STATUS_AFTER,
  SET_TIME_READ_FINAL_CALENDAR,
  PERSISTENT
};
```

Retain this order except where the required deletion/addition forces a local
change. Do not add reserved values or aliases. Phase 2 deliberately replaces
some staged-configuration values; it must migrate them directly rather than
preserve obsolete Phase 1 aliases.

An impossible/default state returns `INTERNAL_STATE_ERROR`, never `OK`,
`EEPROM_VERIFY_FAILED`, or another misleading status. Use a static message and
include useful enum detail where available.

- If no mutation/access cleanup is required, finish immediately with
  `INTERNAL_STATE_ERROR`.
- If persistent access cleanup is required, preserve the internal error as the
  first operation failure through `rememberFailure()` and enter the existing
  bounded C0/Control 1 cleanup path.
- If an existing staged owner has already issued a mutation, do not replay it
  or claim success. Preserve the internal error and use only an already
  existing bounded reconciliation path. Do not invent Phase 2's
  `ConfigurationJobReport` or safe-disable state sets in this phase. If the
  current owner has no safe reconciliation path, terminate with the error and
  document hardware state as unverified; Phase 2 will add the exact fail-safe
  reports and cleanup sequences.

Extend source-contract checks to prove default/impossible routes cannot return
success and persistent cleanup routing is retained. Do not add production or
test-only state mutation accessors.

## 8. Make health evidence consistent

After the raw-result refactor add exactly:

```cpp
Status _i2cWriteReadPresenceTracked(
    const uint8_t* tx,
    size_t txLen,
    uint8_t* rx,
    size_t rxLen);
```

It calls the raw wrapper, maps its status through `mapPresenceError()`, then
updates health only when `callbackInvoked=true`. Use it in `recover()`.
`recover()` and `lastError()` must both report `DEVICE_NOT_FOUND` for the same
address NACK.

Keep the required health model:

- `_updateHealth()` is called only from tracked wrappers;
- local validation and precondition failures do not affect health;
- `probe()` remains raw and does not affect health;
- a successful tracked callback from DEGRADED/OFFLINE returns state to READY;
- lifetime counters wrap from `UINT32_MAX` to zero.

Delete saturation guards such as `< UINT32_MAX`; use ordinary unsigned
increment. Keep Doxygen's wrap wording. Do not add a production or test-only
counter setter, use a layout hack, or execute `UINT32_MAX` public calls merely
to test the language's unsigned arithmetic. Prove the contract with both:

- a source-contract check that each lifetime counter uses ordinary `uint32_t`
  increment and that no saturation guard such as `< UINT32_MAX` remains; and
- a compile-time test-local unsigned arithmetic assertion equivalent to:

  ```cpp
  constexpr uint32_t wrappedCounter = UINT32_MAX + uint32_t{1};
  static_assert(wrappedCounter == 0U,
                "uint32_t lifetime counters must wrap");
  ```

Add native behavioral tests for address-NACK mapping, attempted invalid
callback status, local validation, raw probe, and recover.

## 9. Documentation and source-contract scope

Update every maintained authority affected by this phase. At minimum inspect:

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
example help and Doxygen
tools/check_core_timing_guard.py
other tools/check_* contract guards
```

Required Phase 1 documentation results:

- Password management, credential, sequence, and cleanup claims are removed
  from maintained docs. Vendor register facts remain. Password support is
  explicitly unavailable, direct ranges are denied, and pre-protected parts
  require out-of-band service.
- `Config::enableEepromWrites` has the exact limited authority described in
  section 2.
- Callback results, synchronous buffer lifetime, exclusive deadlines, hard
  callback timeout, conservative no-hook accounting, cleanup reserve, and
  health behavior match sections 3, 4, and 8.
- `end()` abandonment and reinitialization consequences, noncopyability, and
  checked conversion contracts are public and unambiguous.
- Weekday is described as a user-assigned three-bit binary value `0..6`, not
  BCD.
- `Status.h` says all **fallible operations** return `Status`.
- `docs/README.md` identifies the full functional audit as the active v3
  hardening authority and identifies Prompt Suite 04, Prompt 05, and dated v2
  reports as historical when conflicting. Do not rewrite historical prompt or
  report files.
- Add Phase 1 work under `CHANGELOG.md` Unreleased in clear Removed/Changed/
  Fixed entries. Do not add a release date.

Do not update `library.json` or hand-edit `include/RV3032/Version.h` in this
phase. The final suite phase owns `python scripts/generate_version.py set
3.0.0` after all breaking changes are complete. `generate_version.py check`
must still pass at this boundary.

## 10. Required native verification matrix

The final Phase 1 native suite must cover at least:

1. Deleted password symbols and hard denial of both password ranges, including
   intersecting blocks and zero-I/O invalid enum casts.
2. Callback status normalization for each callback and attempted-versus-local
   health accounting.
3. Post-callback deadline boundary, timeout clipping, hook/no-hook accounting,
   wraparound, and instruction budgets.
4. Derived cleanup-reserve admission, no mutation at/after cutoff, permitted
   bounded cleanup before whole deadline, and no callback at/after whole or
   phase deadline.
5. TEMP_LSB target-by-neighbor race injection and exact payloads.
6. Unconditional zero-I/O `end()` during each active owner state.
7. Compile-time noncopyable and nonmovable traits.
8. Invalid conversion/date domains with unchanged outputs and both exact Unix
   endpoints.
9. `INTERNAL_STATE_ERROR` default routing and source-contract reachability
   without a test-only production mutator.
10. Consistent recover/last-error address-NACK mapping.
11. Lifetime counter wrap, using the specified source-contract check and
    test-local compile-time unsigned-wrap assertion rather than an unbounded
    public-call loop or production counter setter.
12. Read-only owner recovery plus one retry remaining within one callback
    bound and one driver instruction.
13. Fixed callback caps and no replay of an ambiguous requested write for
    every Phase 1-owned mutating path.

Preserve fixed queue and buffer capacities. Tests must assert callback counts,
transport payloads, state idleness/ownership, output preservation, and exact
`Err` values. A passing happy path is not enough.

## 11. Commands and evidence

Run from repository root and record exact results:

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
git diff --check
```

On Windows, use `%USERPROFILE%\.platformio\penv\Scripts\platformio.exe` if
`pio` is not on `PATH`.

Also run focused searches proving:

- none of the deleted production password API/state symbols remains;
- password-management implementation text in production exists only in
  retained `CommandTable.h` vendor constants and the narrow unsupported-range
  guard/message; password words are also permitted in unsupported-access
  public Doxygen, maintained documentation/tests, vendor facts, changelog
  history, and historical artifacts;
- no `_beginInProgress`, public BCD utility, dead state, `shouldTrackHealth`,
  saturation guard, or mixed-case retained private enumerator remains;
- every deadline-controlled state dispatches through `readRegsBefore()` or
  `writeRegsBefore()`;
- no old literal 300 ms reserve or 3,700 ms cutoff remains;
- no callback-returned driver-control status can escape normalization;
- no second scheduler, retry, recovery, heap, task, logging, direct `Wire`, or
  delay was introduced into production library code; and
- no unrelated file was rewritten.

Do not run physical HIL, flash a board, remove VDD, issue physical EEPROM
commands, publish, commit, or tag. Report HIL exactly as `NOT RUN`.

Package validation and the final `3.0.0` version transition are deferred until
the later phases because the public contract is intentionally incomplete at
this boundary.

## 12. Final independent audit

After the first green verification pass:

1. Re-read this prompt from the beginning.
2. Re-task the three original subagents to audit the final diff and tests.
3. Resolve each returned item with independent file/line and test evidence.
4. Correct confirmed defects through deletion or refactoring, not conditional
   exceptions around the defective code.
5. Re-run all affected focused tests, then the complete command matrix.
6. Inspect every changed public declaration, private state transition,
   transport dispatch, timeout result, and cleanup boundary.
7. Inspect `git diff --check`, `git status --short`, and the full diff. Remove
   only generated artifacts created by this work; preserve unrelated changes.

Specifically audit for the failure patterns this phase is intended to remove:

- callback returns after deadline but job reports `OK`;
- callback reports `IN_PROGRESS` and an idle job remains forever active;
- stale TEMP_LSB zeros clear a newly asserted flag;
- active work prevents teardown;
- two driver objects can own the same in-flight state;
- password access survives through a raw/block or enum-cast path;
- local validation changes health;
- recover and cached health disagree on address NACK;
- a counter saturates contrary to its documented wrap contract;
- invalid conversion changes its output; or
- an impossible state returns success or replays mutation.

## 13. Phase definition of done

Phase 1 is complete only when:

- H-01, H-02, H-03, H-04, M-03, L-02, L-03, and L-04 have code-level
  regression evidence;
- password management code is deleted and both password ranges fail closed
  without blocking managed EEPROM control registers;
- transport results form the specified closed domain;
- every retained cooperative callback is post-checked against the earliest
  applicable exclusive deadline;
- mutation evidence comes only from actual forward-write dispatch;
- persistence admission uses the derived cleanup reserve and retains bounded
  reconciliation without blind replay;
- TEMP_LSB flag clear has one W0C-safe two-state owner;
- `end()` always tears down locally and the driver is noncopyable/nonmovable;
- checked public conversions preserve outputs on failure;
- dead state is removed, retained private state names use `CAPS_CASE`, and
  impossible state cannot report success;
- health mapping, tracking, and wrapping counters agree with documentation;
- source, public headers, fake, tests, maintained documentation, examples, and
  contract guards agree for this phase;
- all required device-free commands pass;
- physical HIL is truthfully `NOT RUN`; and
- no later-phase feature, compatibility shim, release artifact, commit, or tag
  was introduced.

A genuine conflict or blocker must be reported with exact evidence and means
the phase is not complete. Do not weaken an acceptance criterion to obtain a
green build.

## 14. Final handoff format

Lead with achieved behavior. Then report:

1. each closed finding and its code/test evidence;
2. deleted password/dead code and material structural simplifications;
3. exact changed files;
4. exact native test count and command results;
5. ESP32-S2/S3 RAM and flash sizes;
6. subagent audit findings, which were confirmed/rejected, and the corrections
   made by the primary coder;
7. physical HIL as `NOT RUN`;
8. any genuine remaining field risk or cross-phase dependency; and
9. final HEAD/worktree state.

Do not claim full library hardening completion. State that Phase 2 must consume
the timed-transfer contract when it implements backup timing, staged
configuration reports/cleanup, persistence evidence, quiescence, periodic
semantics, and fallible `tick()`.
