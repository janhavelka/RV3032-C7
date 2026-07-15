# RV3032-C7 functional hardening implementation prompt

- Re-audited: 2026-07-15
- Audited revision: `0e2eb0d`
- Manifest version at audit: `2.0.0`
- Purpose: complete implementation prompt, not an implementation record

This document replaces the earlier narrative in this file. It revalidates the
findings against the current source, the binding repository rules, maintained
documentation, the native fake, and the local Micro Crystal RV-3032-C7
Application Manual Rev. 1.3. It gives one concrete correction design so an
implementing coder does not need to invent policy.

The requested product decision is final: **password authentication and password
protection management are not supported by this library. Remove that feature.**
Do not repair it, hide it behind a build option, or leave compatibility stubs.

## Executive engineering assessment

The audited revision is not ready for an unconditional production release.
After removing password management from scope, the re-audit leaves:

- 5 High-severity functional findings;
- 9 Medium-severity functional or integration findings;
- 5 Low-severity API, diagnostic, dead-code, and documentation findings; and
- no Critical finding.

The five High findings have credible field consequences:

1. A cooperative operation can finish after its advertised deadline and still
   return `OK`. For example, two legal 6 ms callbacks can complete a nominal
   10 ms operation at 12 ms. Consequence: a startup or cleanup time budget is
   not actually enforced.
2. Clearing one TEMP_LSB flag can clear a different flag that asserts between
   the read and write polls. Consequence: the system can lose EEPROM-failure,
   backup-switchover, or clock-output evidence while reporting success.
3. `end()` silently does nothing while work exists. Consequence: if the old bus
   owner is gone, the driver cannot be terminated or rebound and remains in a
   lifecycle dead end.
4. `RV3032` is implicitly copyable and movable. Consequence: copying just
   before an EEPROM `WRITE_ONE` state can let two objects issue the wear-limited
   command.
5. the ordinary backup-mode setter returns success before the vendor's 2 ms or
   10 ms activation interval has elapsed. Consequence: removing VDD immediately
   after success can occur before backup switchover is effective and can lose
   RTC time.

The current green test suite does not disprove these findings. The baseline was
re-run on the unchanged revision:

| Check | Baseline result |
| --- | --- |
| Native PlatformIO tests | PASS - 84/84 |
| ESP32-S2 build | PASS - RAM 37,024 bytes, flash 433,045 bytes |
| ESP32-S3 build | PASS - RAM 22,624 bytes, flash 444,130 bytes |
| Physical HIL | NOT RUN |

Several current tests encode the defective `end()` contract, and the suite
does not inject the decisive post-callback deadline overrun or inter-callback
TEMP_LSB event race. Software builds are therefore useful regression evidence,
not physical proof of backup retention, INT behavior, or electrical timing.

## Finding register

| ID | Severity | Revalidated finding | Real-system consequence |
| --- | --- | --- | --- |
| H-01 | High | Deadline checks are dispatch guards, not callback-completion bounds. | A job or EEPROM phase can exceed its promised bound and still succeed. |
| H-02 | High | TEMP_LSB W0C clears use a stale read-derived payload. | A newly asserted EEF, CLKF, or BSF event can be erased. |
| H-03 | High | `end()` silently refuses teardown while work exists. | No deterministic escape exists when the prior transport owner is unavailable. |
| H-04 | High | The live driver is copyable and movable. | In-flight state and a pending wear-limited mutation can be duplicated. |
| H-05 | High | Backup-mode success precedes the required activation time. | VDD can be removed before DSM/LSM is active. |
| M-01 | Medium | Timer, CLKOUT, and temperature jobs can fail after disabling or partially changing the feature without final-state evidence. | Monitoring or timing can remain silently disabled or partially configured. |
| M-02 | Medium | Persistent operation evidence is overwritten or delayed until cleanup. | The caller loses the original failure and can be told proven data was unproven. |
| M-03 | Medium | Transport callbacks may return driver-control statuses such as `IN_PROGRESS`. | An idle job can appear forever in progress; persistence can mis-handle the result as engine progress. |
| M-04 | Medium | `tick()` discards the fallible EEPROM poll result. | The normal integration path can silently lose a terminal failure. |
| M-05 | Medium | UIE is documented as an interrupt-only Boolean although UIE=0 also prevents UF. | A polling application can wait forever for an update flag it disabled. |
| M-06 | Medium | Several reconfiguration APIs omit vendor-recommended quiescence preconditions. | Live reconfiguration can create unintended INT or interrupt-controlled CLKOUT activity. |
| M-07 | Medium | The shipped Wire adapter ignores the callback timeout. | A requested 5 ms callback can use the global 50 ms timeout and invalidate cleanup timing. |
| M-08 | Medium | CLI parsing, truncation, and scheduling can admit unintended or overlapping mutation. | Invalid text can become a valid write; a truncated command can execute; owner serialization can be violated. |
| M-09 | Medium | `isOnline()` claims operation/presence after passive `begin()`; `probe()` claims identity without identifying the chip. | Integration health logic can treat a missing or wrong ACKing device as proven present. |
| L-01 | Low | Example completion/output and README polling snippets are inconsistent. | Operators can see failure followed by mutation, a fictitious empty timestamp, or copy invalid polling code. |
| L-02 | Low | Public BCD/date utilities silently accept invalid domains or use Boolean failure contracts. | Invalid input can produce plausible output and maximum-year input can cause unnecessary long calculation. |
| L-03 | Low | `recover()` and cached health disagree; counter docs say wrap but code saturates. | Diagnostics report two meanings for one transfer. |
| L-04 | Low | Dead states and `_beginInProgress` obscure the state machine; an impossible ordinary state returns `OK`. | A future transition error can be hidden as success. |
| L-05 | Low | Weekday and Status-write documentation is incomplete or wrong. | Maintainers can treat weekday as BCD or miss unavoidable THF/TLF clearing. |

The previous password-cleanup, password-cutoff, and credential-remanence
findings are not carried forward. Their complete solution is deletion of
password management, specified below.

---

# Instructions to the implementing coder

Implement every mandatory item in this document. Do not substitute a different
architecture without written user direction. Prefer deletion and small local
owners over compatibility layers or generic frameworks.

## 1. Authority, scope, and non-negotiable constraints

Use this authority order:

1. `AGENTS.md` is binding for lifecycle, transport ownership, boundedness,
   persistence, status handling, source layout, and release rules.
2. The local vendor PDFs define silicon register and timing behavior.
3. Public headers, maintained README/architecture/device documentation, and
   tests must agree with the corrected implementation.
4. Prompt Suite 04, Prompt 05, and dated v2.0.0 reports are historical when
   they conflict with this prompt. In particular, their password-management
   and unchecked `computeWeekday()` requirements are superseded.

Keep these architecture constraints:

- `begin(const Config&)` remains passive and performs zero I2C/wait callbacks.
- `probe()` remains an explicit one-callback diagnostic.
- `end()` performs zero I/O and always ends the lifecycle.
- ordinary multi-transfer work remains cooperative and fixed-capacity;
  `ensurePrimaryCellConfiguration()` remains the only synchronous
  multi-callback exception.
- one polling instruction invokes at most one driver transport callback.
- do not add internal retry, bus recovery, heap allocation, logging, platform
  delay, a new scheduler, a password service, or a compatibility facade.
- no blind retry of a possibly committed mutating callback.
- no code path may write reserved register bits.
- preserve unrelated user changes.
- do not commit, tag, publish, or claim physical HIL without explicit user
  authorization.

Append these exact error values after existing `INCOHERENT_DATA = 22`; never
renumber an existing `Err` value:

```cpp
CONFIGURATION_CLEANUP_FAILED = 23,
TRANSPORT_CONTRACT_VIOLATION = 24,
INTERNAL_STATE_ERROR = 25
```

Do not add a password-specific error.

## 2. Remove password management completely

This is deletion, not deprecation.

### 2.1 Delete the public surface

Delete from `include/RV3032/RV3032.h`:

```text
PasswordCredential
PasswordProtectionEvidence
PasswordProtectionStatus
unlockPasswordProtection(...)
startConfigurePasswordProtectionJob(...)
getPasswordProtectionStatus(...)
```

Do not leave `NOT_SUPPORTED` methods, aliases, deprecated wrappers, a compile
flag, or placeholder types.

### 2.2 Delete the production workflow

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
the three password method implementations. `persistentUseAddressList` and
`persistentAddresses` have no non-password caller, so make persistent
addressing strictly contiguous:

```cpp
const uint8_t address =
    static_cast<uint8_t>(_job.persistentAddress + _job.persistentIndex);
```

If the existing `currentAddress` lambda then has one caller, inline it and
delete the lambda. Simplify failure cleanup to active C0/Control 1 access-state
cleanup only.

### 2.3 Retain the register reference and fail closed

Do not delete these `CommandTable.h` constants:

```text
REG_PASSWORD0..REG_PASSWORD3               // 0x39..0x3C
REG_EEPROM_PASSWORD0..REG_EEPROM_PASSWORD3 // 0xC6..0xC9
REG_EEPROM_PW_ENABLE                       // 0xCA
CONFIG_EEPROM_END                          // 0xCA
```

`AGENTS.md` requires a complete vendor register reference. Keeping constants is
not feature support.

Add one implementation-only helper with this exact responsibility:

```cpp
bool intersectsUnsupportedPasswordRange(uint8_t reg, size_t len);
```

Treat this as a pure intersection predicate over an already validated,
non-empty, non-wrapping block. Use 16-bit start/end arithmetic and return true
when the block touches either `0x39..0x3C` or `0xC6..0xCA`. Normal block
validation remains the sole owner of zero-length and wrap errors. Call the
predicate from private `readRegs()` and `writeRegs()` after that validation and
before any transport callback. Return:

```cpp
Status::Error(Err::INVALID_PARAM,
              "Password registers are unsupported");
```

Do not block `EEADDR`, `EEDATA`, or `EECMD` (`0x3D..0x3F`); the managed EEPROM
engine requires them. Keep `ConfigurationEepromRegister` limited to `C0..C5`,
generic configuration persistence limited to `C0..C5`, and user EEPROM limited
to `CB..EA`.

Update `Config::enableEepromWrites` documentation to say it grants authority
only for configuration `C0..C5` and typed user EEPROM `CB..EA`. It never grants
password-register authority.

### 2.4 Simplify the fake and tests

Delete password credential storage, authentication state, protected-write
sequencing, password telemetry, and password workflow tests from
`test/test_native/FakeRv3032.h` and `test_datetime.cpp`.

Retain the fake's simple silicon fact that direct reads of write-only password
ranges return zero. It does not need to emulate an externally password-enabled
module. Add a comment stating that limitation; do not add fake-only production
hooks.

Add tests proving:

- every single address in `0x39..0x3C` and `0xC6..0xCA` is rejected for public
  raw read and write with zero callback-count change;
- blocks `0x38..0x39` and `0xC5..0xC6` are rejected before I/O;
- casts of `C6..CA` to `ConfigurationEepromRegister` are rejected before I/O;
- the generic configuration-persistence queue stages only `C0..C5`;
- typed user-EEPROM jobs stage only `CB..EA`; and
- a source-contract scan finds none of the deleted symbols in the public
  header or implementation while the command-table constants still exist.

Document the product precondition honestly: password protection must be
disabled. A module protected by other firmware requires out-of-band service
and is unsupported. Transport success alone is not proof that a WP register
changed on a protected part; typed multi-transfer operations rely on readback,
and ordinary single-write behavior is outside the supported precondition.

## 3. Refactor the transport boundary into a closed result domain

The current callbacks can return any library `Status`. A callback returning
`IN_PROGRESS` is currently mistaken for cooperative engine progress. Fix the
boundary once; do not patch individual public methods.

Document both callbacks in `Config.h` as synchronous. Buffers are borrowed only
for the callback duration. `Status::msg` must have static storage. The only
allowed callback `Err` values are:

```text
OK
I2C_ERROR
I2C_NACK_ADDR
I2C_NACK_DATA
I2C_TIMEOUT
I2C_BUS
```

Add:

```cpp
struct RawTransferResult {
  Status status = Status::Ok();
  bool callbackInvoked = false;
};

static Status normalizeTransportResult(const Status& status);
```

Any callback-returned code outside the allowlist becomes:

```cpp
Status::Error(
    Err::TRANSPORT_CONTRACT_VIOLATION,
    "Transport callback returned invalid status",
    static_cast<int32_t>(status.code));
```

Make the private raw write and write-read wrappers return `RawTransferResult`.
Local null/buffer/timeout validation returns `callbackInvoked=false`. Set
`callbackInvoked=true` immediately before calling application code, normalize
immediately on return, and never retain an application-owned message pointer
from an invalid-domain status.

Tracked wrappers call `_updateHealth()` exactly when `callbackInvoked` is true.
Raw `probe()` returns the normalized status without health tracking. Delete
`shouldTrackHealth()`; attempted-vs-local classification now comes from the
explicit Boolean rather than guessing from an error enum.

Acceptance tests must inject `IN_PROGRESS`, `BUSY`, `INVALID_PARAM`, and an
unknown cast value from each callback. None may escape. An ordinary job must
become idle with `TRANSPORT_CONTRACT_VIOLATION`, persistence must not reissue
the instruction, health must count one attempted-transfer failure, and raw
`probe()` must leave health unchanged.

## 4. Make cooperative deadlines bound callback completion

### 4.1 Required helpers

Keep deadlines exclusive: a completion at `nowMs == deadlineMs` is late. All
admitted intervals remain below `2^31` ms.

Add this exact result type so deadline handling does not destroy transport
evidence:

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
`callbackInvoked` is true. Do not add a hidden last-transfer member.

Add these private helpers; use these names and responsibilities:

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

`remainingBefore()` is wrap-safe and returns false at or after the exclusive
deadline. `earlierDeadline()` compares remaining intervals relative to the
supplied `nowMs`, not raw unsigned absolute values.

Implement timed tracked write/write-read wrappers below these register helpers;
they call the raw wrapper and update health exactly once. The timed register
helpers must:

1. sample `_nowMs()` before dispatch when a hook exists and advance the
   by-reference `nowMs` to the later observation;
2. reject with zero callbacks unless `remainingMs > 1`;
3. pass `min(_config.i2cTimeoutMs, remainingMs - 1)` to the new timed tracked
   wrapper, which delegates to the raw timeout wrapper;
4. sample `_nowMs()` immediately after the callback when a hook exists;
5. without a hook, conservatively add the timeout actually passed, even if the
   callback returned earlier;
6. classify a hook-observed duration greater than the supplied timeout as
   effective `I2C_TIMEOUT`, set `callbackTimeoutViolated=true`, and update
   health with that timeout rather than a callback-returned `OK`; and
7. set `deadlineCrossed=true` and reject post-callback time at or after the
   exclusive deadline.

Use this exact effective-result precedence:

1. local validation error when no callback was invoked;
2. normalized `TRANSPORT_CONTRACT_VIOLATION`;
3. observed callback-timeout violation as `I2C_TIMEOUT`;
4. another normalized callback failure; and
5. `TIMEOUT` when the callback returned `OK` but crossed the operation/phase
   deadline.

The normalized callback result normally feeds health. The observed
callback-timeout violation feeds health as `I2C_TIMEOUT`. A pure
operation-deadline failure after a transport-successful callback does not turn
the transport-health observation into an I2C failure.

For a deadline crossing after callback `OK`, set effective status to:

```cpp
Status::Error(Err::TIMEOUT,
              "Transport callback crossed operation deadline",
              static_cast<int32_t>(callbackStatus.code));
```

The returned timeout retains the callback code in `detail`. Only a state that
called `writeRegsBefore()` for a forward mutating write copies
`callbackInvoked` into mutation-attempt evidence; guard/read results never mark
mutation. Copy the evidence immediately after the helper returns and before any
transition. Use effective `status` for state and terminal handling while
retaining `callbackStatus` locally when readback reconciliation needs the
original callback result.

### 4.2 Use the earliest applicable boundary

Every deadline-controlled state must use `readRegsBefore()` or
`writeRegsBefore()`. It must not call plain `readRegs()`/`writeRegs()`.

Immediately before dispatch, calculate the earliest applicable absolute
boundary:

- whole-operation deadline, always;
- current EEPROM phase deadline for ready, READ_ONE, WRITE_ONE, and cleanup
  polling; and
- mutation cutoff for a forward mutating write.

The raw/timed wrapper sets `callbackInvoked=true` at the exact dispatch point;
the state machine copies that evidence from the returned result and never marks
an attempt merely because it called a helper. If an attempted mutation fails or
crosses a phase/mutation cutoff while time remains before the whole deadline,
enter bounded readback reconciliation or cleanup. Never replay
`WRITE_ONE` or another ambiguous requested write. If a non-compliant callback
overruns the timeout passed to it and reaches the whole deadline, dispatch no
further callback, return the applicable cleanup-failure terminal status, and
report hardware/access state as unverified. A whole-operation deadline remains
a hard no-dispatch boundary.

Refactor `pollJob()` and `processEeprom()` to carry one mutable
`currentNowMs` through the instruction loop. Do not recompute time from
`instructionsUsed * i2cTimeoutMs`. Change the persistence signature to:

```cpp
Status processPersistentJob(uint32_t& nowMs, bool& callbackUsed);
```

Anchor the 1 ms/10 ms vendor waits from the same post-callback timestamp.

### 4.3 Derive the persistence cleanup reserve

Delete the literal 300 ms cleanup reserve for generic persistence. After
password deletion, use this conservative worst-case expression:

```cpp
const uint32_t cleanupReserveMs =
    EEPROM_CLEANUP_READY_TIMEOUT_MS +
    6U * _config.i2cTimeoutMs +
    EEPROM_WRITE_SETTLE_MS;
```

The six callbacks cover selected-active write/verify, active PMU C0
write/verify, and Control 1 (`0x10`) write/verify. Do not confuse the last pair
with configuration EEPROM C1/offset. The cleanup-ready phase itself is bounded
by its phase deadline and fixed check cap.

Set the mutation cutoff from the whole deadline minus this reserve. Admission
must reject with `INVALID_PARAM` and zero I/O unless the requested whole timeout
leaves at least:

```text
cleanupReserveMs + one forward callback timeout + one millisecond
```

Use the derived minimum in admission and public documentation. Keep the public
maximum at 10,000 ms. Change the one-byte configuration-read default from 250
ms to 1,000 ms; retain the user-read default of 1,000 ms and user-write default
of 4,000 ms. These defaults all leave the derived safety reserve even when
`Config::i2cTimeoutMs` is 100 ms, although a long multi-byte operation may still
require a larger caller-selected timeout to finish. Keep all existing fixed
check caps.

For each generic configuration-queue item, retain the fixed 4,000 ms whole
deadline and replace the literal 3,700 ms cutoff with:

```cpp
mutationCutoffMs = itemStartMs + (4000U - cleanupReserveMs);
```

Validate at `begin()` that the configured callback timeout makes this
expression admissible; the current `1..100` ms range does. Typed persistent
start APIs apply the same derived-minimum check to their caller-supplied
timeout.

### 4.4 Deadline acceptance matrix

Add tests for:

- every terminal ordinary deadline state at exact boundary and 1 ms late;
- every EEPROM phase and cleanup state at exact boundary and 1 ms late;
- timeout argument clipping;
- live clock hook and no-hook accounting;
- `maxInstructions` values 1 and greater than 1;
- `uint32_t` wraparound;
- zero callbacks at/after a boundary;
- a mutating callback crossing its cutoff entering cleanup only; and
- impossible admission budgets producing zero I/O.

Update `tools/check_core_timing_guard.py` so it enforces the new helper use and
cannot pass the old pre-dispatch-only implementation.

### 4.5 Bounds for no-wait cooperative jobs

Per `AGENTS.md`, a no-wait operation with a fixed transfer count uses the
derived bound `callback cap * Config::i2cTimeoutMs`. Caller delay between polls
is not hidden driver execution time. Operations with a vendor wait or polling
phase additionally use the absolute deadlines above.

Enforce these exact driver-callback caps after the refactor:

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

Wait polls consume zero callbacks. Persistent work retains its fixed per-phase
check caps plus whole/phase deadlines. Add source-contract assertions for these
caps and native fault-ordinal tests for each mutating path.

The permitted application-owner exception remains: one read-only callback may
perform documented bounded recovery plus at most one read retry. Both physical
attempts together must remain inside the single callback timeout, and the
driver still counts the callback as one instruction. Mutating callbacks remain
exactly one physical attempt. Include the owner-side read-retry path in timing
and overrun tests.

## 5. Replace TEMP_LSB clear duplication with one W0C-safe owner

Vendor Application Manual page 23 defines EEF, CLKF, and BSF as sticky
read/clear-only flags. The target write must never contain stale zeroes for the
other W0C flags.

Add to `CommandTable.h`:

```cpp
static constexpr uint8_t TEMP_LSB_W0C_MASK =
    EEPROM_EEF_MASK | TEMP_CLKF_MASK | TEMP_BSF_MASK; // 0x0B
```

Add one private admission helper:

```cpp
Status startTempLsbFlagClear(uint8_t targetMask);
```

Add only these dedicated states and field:

```text
JobKind::TEMP_LSB_FLAG_CLEAR
JobState::TEMP_LSB_CLEAR_READ
JobState::TEMP_LSB_CLEAR_WRITE
JobOp::tempLsbClearMask
```

Route `clearClockOutputFlag()`, `clearEepromErrorFlag()`, and
`clearBackupSwitchFlag()` through this helper. Delete their duplicated generic
register-update setup. Admission accepts exactly one target bit equal to
`EEPROM_EEF_MASK`, `TEMP_CLKF_MASK`, or `TEMP_BSF_MASK`; any other or combined
mask returns `INVALID_PARAM` with zero I/O. The matching job has a fixed
two-callback cap.

The read state must:

- return `OK` with no write if the target is already clear;
- return `BUSY` with no write if EEbusy is set; and
- otherwise set the exact payload to
  `TEMP_LSB_W0C_MASK & ~targetMask`.

The write state writes that exact single byte once. Do not copy the temperature
fraction or any earlier flag zero. Exact payloads are:

| Target | Payload |
| --- | --- |
| CLKF | `0x09` |
| EEF | `0x03` |
| BSF | `0x0A` |

Do not reject a pre-existing neighboring EEF/CLKF/BSF; writing one preserves
it. Retain the EEbusy guard and the documented requirement for VDD power when
clearing BSF.

Test each target against each neighbor asserted after the read callback, all
neighbors asserted, target already clear, EEbusy, exact write log, and zero
collateral clears.

## 6. Make lifecycle ownership explicit

### 6.1 `end()`

Keep the signature:

```cpp
void end();
```

Implement it as unconditional, zero-I/O local teardown:

```cpp
void RV3032::end() {
  _resetRuntimeState();
}
```

Delete the busy early return. Do not add a cancellation API or a second end
method. Document that queued and active work is abandoned and already-issued
silicon writes cannot be undone. Emergency teardown can irreversibly discard
the saved C0/Control 1 restoration snapshot. A re-read alone cannot resume
abandoned cleanup. After rebind, the application must explicitly reinitialize
the affected PMU/Control 1 product policy or follow its documented
power-cycle/reprovisioning procedure before deployment.

Replace the current preservation tests with active ordinary job, queued EEPROM,
active EEPROM, repeated end, and immediate rebind cases. Every case must make
zero callbacks, enter `UNINIT`, clear callbacks/context, queues, jobs, health,
the saved restoration snapshot, and the primary-cell same-lifecycle latch, and
permit `begin()` with a different context. Do not test or document resumption
of abandoned work.

### 6.2 Delete copy and move

Add exactly:

```cpp
RV3032() = default;
RV3032(const RV3032&) = delete;
RV3032& operator=(const RV3032&) = delete;
RV3032(RV3032&&) = delete;
RV3032& operator=(RV3032&&) = delete;
```

Add Doxygen explaining that the object owns live cooperative state and a
borrowed transport binding. Add native `static_assert` coverage for all four
copy/move constructible/assignable traits using `<type_traits>`.

## 7. Replace the backup setter with a timed cooperative job

Delete the old `setBackupSwitchMode(BackupSwitchMode)` surface. Add:

```cpp
static constexpr uint32_t BACKUP_SWITCH_OPERATION_TIMEOUT_MS = 250;
static constexpr uint32_t BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS = 1000;

Status startSetBackupSwitchModeJob(
    BackupSwitchMode mode,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = BACKUP_SWITCH_OPERATION_TIMEOUT_MS);
```

Keep `getBackupSwitchMode()` as a single-transfer getter. Use an explicit
encoder; do not cast the public enum into silicon bits:

```text
Off    -> BSM 00
Direct -> BSM 01
Level  -> BSM 10
```

Treat both observed raw values `00` and `11` as disabled.

Add `JobKind::SET_BACKUP_SWITCH_MODE` and these states:

```text
BACKUP_READ_CONTROL3
BACKUP_READ_PMU
BACKUP_WRITE_PMU
BACKUP_VERIFY_PMU
BACKUP_WAIT_ACTIVATION
```

Add only the required fields:

```cpp
uint8_t backupOriginalPmu = 0;
uint8_t backupTargetPmu = 0;
uint32_t backupWriteCompletedMs = 0;
uint32_t backupActivationNotBeforeMs = 0;
bool backupActivationRequired = false;
```

Use the shared `ConfigurationJobReport` for write status and mutation evidence;
do not duplicate them in backup-only fields.

Required sequence:

1. Read Control 3 and return `BUSY` with zero writes when BSIE is one.
2. Read active PMU and preserve every implemented non-BSM bit.
3. Before mutation, prove capacity for an optional C0 persistence entry.
4. If the active target is already present, skip the write and activation wait.
5. Otherwise issue the active PMU write once.
6. Always read back PMU after an attempted write, including a failed/late
   callback when the remaining deadline permits. Never replay the write.
7. Accept the active target only after exact implemented-bit readback proof.
8. For disabled (`00` or `11`) to Direct, wait until 2 ms after write callback
   completion. For disabled to Level, wait until 10 ms after completion.
9. The wait state performs zero callbacks and returns terminal success only at
   or after the not-before time.
10. Enqueue optional C0 persistence only after active readback and activation
    settle are complete and `operationStatus` is `OK`.

Use the deadline helpers. The mutation cutoff must reserve PMU verification and
the requested settle. Admission uses this conservative minimum:

```cpp
minimumMs = 4U * _config.i2cTimeoutMs +
            (mode == BackupSwitchMode::Direct ? 2U :
             mode == BackupSwitchMode::Level ? 10U : 0U) +
            1U;
```

Accept only
`minimumMs <= operationTimeoutMs && operationTimeoutMs <=
BACKUP_SWITCH_OPERATION_TIMEOUT_MAX_MS`. Reject either boundary violation with
`INVALID_PARAM` and zero I/O. The 250 ms default is valid for the default 50 ms
transport timeout; a caller using the maximum 100 ms callback timeout must
supply at least 403 ms for Direct or 411 ms for Level.

Use this implementation-only detail helper for backup and configuration
safe-state verification mismatches:

```cpp
static constexpr int32_t registerMismatchDetail(uint8_t expected,
                                                uint8_t observed) {
  return (static_cast<int32_t>(expected) << 8) |
         static_cast<int32_t>(observed);
}
```

Pass implemented-bit-masked bytes. A callback-`OK` backup target mismatch uses
`REGISTER_WRITE_FAILED`, message `"Backup PMU verification failed"`, and
`registerMismatchDetail(targetImplemented, observedImplemented)` as
`operationStatus`. If the observation proves neither original nor requested,
also set `cleanupStatus` to `REGISTER_WRITE_FAILED`, message
`"Backup PMU reconciliation inconclusive"`, with the same detail. A failed
reconciliation read instead retains its exact effective status as the cleanup
cause.

Backup is reconciliation-only after its sole PMU write attempt. Never issue a
second PMU write as generic safe-disable cleanup. Use this exact result mapping:

- requested PMU proven: set `REQUESTED_VERIFIED`; perform the applicable 2/10
  ms wait; if the write callback failed, `pollJob()` returns that original
  effective write status and no persistence is queued;
- original PMU proven: set `UNCHANGED` and return the original write failure,
  or `REGISTER_WRITE_FAILED` if the callback returned `OK` but target proof
  failed;
- neither original nor requested proven, or reconciliation read failure: set
  `UNKNOWN`, store the reconciliation cause in `cleanupStatus`, and return
  `CONFIGURATION_CLEANUP_FAILED`.

An unchanged requested PMU is `REQUESTED_VERIFIED` with
`mutationAttempted=false`. Do not guess state from an ACK.

Test raw `00` and `11` to DSM/LSM at one millisecond early and exact time,
callback-duration anchoring, no-I/O wait polls, wraparound, unchanged mode,
enabled-to-enabled, enabled-to-off, queue timing, deadline expiry, failed-write
readback reconciliation, and no requested-write retry.

Prepare an authorized HIL case that removes VDD immediately after terminal
success and checks retention. Run DSM only with a documented
rechargeable/lower-voltage backup topology or controlled source; never switch a
primary-cell fixture near VDD into DSM. For LSM, keep VDD safely above the
maximum LSM threshold throughout the 10 ms activation interval before removal.
Do not claim either physical property from the fake.

## 8. Give staged configuration jobs a known fail-safe terminal state

Timer, CLKOUT, periodic-update, and temperature configuration are legitimate
multi-step operations. Their current failure handling is incomplete after the
first mutation.

Add this shared evidence type because it has four concrete current users plus
the backup job; do not generalize it beyond these jobs:

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

Do not leave CLKOUT, periodic update, or temperature configuration classified
as generic `REGISTER_UPDATE`.

Add these result getters:

```cpp
Status getSetTimerJobResult(ConfigurationJobReport& out) const;
Status getSetPeriodicUpdateJobResult(ConfigurationJobReport& out) const;
Status getSetBackupSwitchModeJobResult(ConfigurationJobReport& out) const;
Status getSetClkoutConfigJobResult(ConfigurationJobReport& out) const;
Status getSetTemperatureEventConfigJobResult(
    ConfigurationJobReport& out) const;
```

While the matching job is active, return `IN_PROGRESS` and do not change `out`.
When no matching completed result exists, return `JOB_RESULT_UNAVAILABLE` and
do not change `out`. Otherwise copy the report and return the exact terminal
job status, matching the existing typed job-result getter convention. The
report separately preserves operation and cleanup evidence.

General staged-job rule:

- Store the first forward failure in `operationStatus`; never overwrite it.
- Set `mutationAttempted` only from the timed result of a forward mutating
  `writeRegsBefore()` whose `callbackInvoked` is true. Never set it from a guard
  or verification read. Track cleanup writes separately with
  `configurationCleanupWriteAttempted`.
- A pre-mutation failure leaves `finalState=UNCHANGED`; `pollJob()` returns that
  failure directly.
- Do not replay a failed/ambiguous requested write.
- On normal success, perform final readback and report
  `REQUESTED_VERIFIED` only after exact implemented-bit proof.
- For timer, periodic update, CLKOUT, and temperature only, a post-mutation
  failure enters operation-specific safe-disable cleanup. Backup uses the
  reconciliation-only rule in section 7 and never receives a cleanup write.
- Safe-disable cleanup first reads the gate. If the read proves the safe value,
  do not write it again. If the read proves a different value, write the safe
  value once and verify it once. If the first cleanup read cannot determine
  state, do not guess and do not replay the possible safe-disable mutation.
- A failed/late cleanup write is never retried; perform its single verification
  read when the bound permits. Retain the cleanup callback failure even if
  readback later proves the safe value.
- If safe disable is verified, `pollJob()` returns the original operation
  failure and sets `SAFE_DISABLED_VERIFIED`.
- If safe disable cannot be proven, `pollJob()` returns
  `CONFIGURATION_CLEANUP_FAILED`, retain the first cleanup cause in
  `cleanupStatus`, and set `UNKNOWN`.
- Enqueue persistence only after `REQUESTED_VERIFIED` with
  `operationStatus.ok()`.

For a successful cleanup verification read whose implemented gate byte does
not equal the safe byte, set the first cleanup cause to
`REGISTER_WRITE_FAILED`, message
`"Configuration safe-state verification failed"`, and detail
`registerMismatchDetail(safeImplemented, observedImplemented)`. Transport and
deadline failures retain their exact effective status. Never overwrite an
earlier cleanup-write callback failure with any later verification result.

An impossible/default state never overwrites first-cause evidence. If no
forward failure is latched, latch `INTERNAL_STATE_ERROR` into
`operationStatus`; otherwise preserve the existing operation failure. Before
mutation, finish `UNCHANGED`. After mutation but before cleanup, enter the
existing bounded owner exactly once: safe-disable for staged configuration,
readback-only reconciliation for backup, or persistent access cleanup. If the
impossible state occurs while already in cleanup/reconciliation, do not restart
or replay it; latch `INTERNAL_STATE_ERROR` into `cleanupStatus` only when it is
still `OK`, set `UNKNOWN` where applicable, and terminate with
`CONFIGURATION_CLEANUP_FAILED` or persistent `EEPROM_CLEANUP_FAILED`. It never
returns `OK` or bypasses required cleanup.

Store the report and the exact operation scratch in `JobOp`:

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

Do not create heap-backed or polymorphic report storage.

Use these exact safe states:

### Timer

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

- Guard-read Control 2 and require TIE=0 before any write.
- The job already owns TE staging.
- Write Timer Low and Timer High as one two-byte transfer at `REG_TIMER_LOW`;
  delete separate low/high write states and dead
  `SetTimerReadTimerHigh`.
- Safe state is the original implemented Control 1 with TE=0. Write once and
  verify it.
- Success verification reads `0x0B..0x10` once and checks timer low/high
  reserved bits, requested TD, TE, and requested preset.
- Enforce success cap 6 and worst failure/reconciliation cap 9.

### Periodic update

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

- Read Control 1 and Control 2.
- Write Control 2 with UIE=0 while preserving every other implemented bit.
- Write requested USEL in Control 1.
- Write requested UIE in Control 2.
- Verify both controls.
- Safe state is Control 2 with UIE=0, written once and verified.
- Do not clear UF implicitly.
- Enforce success cap 5 and worst failure/reconciliation cap 8.

### CLKOUT

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

- Retain the existing precondition that CLKIE=0 and CLKF=0.
- Safe state is the preserved implemented PMU byte with NCLKE=1. Write one byte
  and verify the full implemented PMU value. With the guard satisfied, this
  proves output disabled.
- Success verifies active C0..C3 against the requested implemented masks before
  queuing C0/C2/C3 persistence.
- Enforce success cap 5 and worst failure/reconciliation cap 8.

### Temperature events

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

- Safe state is the preserved implemented Control 3 value with
  THE/TLE/THIE/TLIE all zero. Write it once and verify it.
- Threshold or timestamp-control bytes may be partially new, but detection and
  interrupt signaling must be proven off.
- Success verifies Control 3, the relevant timestamp-control overwrite bits,
  and both thresholds. Preserve the vendor order: interrupt enables before
  detection enables.
- Enforce success cap 7 and worst failure/reconciliation cap 10.

Fault-inject every callback ordinal before and after commit for all four jobs
and backup. Assert fixed callback/check bounds, no requested-write retry, exact
safe gate, correct report, `UNKNOWN` on cleanup failure, and no persistence
entry for failed or unverified active configuration.

## 9. Separate persistence operation proof from cleanup proof

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

Add `Status persistentOperationStatus` and
`Status persistentCleanupStatus` to `JobOp` for the active typed job or active
queue item. `rememberFailure()` stores only the first forward failure. Cleanup
stores only the first cleanup failure. `finishJob()` must not overwrite either
field, and typed result getters copy these fields into their public report
before returning.

Set `persistentVerified=true` immediately when the final requested byte is
directly proven and `length` becomes the requested length. A later active
C0/Control 1 access-state cleanup failure must not clear durable data proof.

For generic queue diagnostics, add to `SettingsSnapshot`:

```cpp
Status eepromOperationStatus = Status::Ok();
Status eepromCleanupStatus = Status::Ok();
```

Back those snapshot fields with durable driver members, not `JobOp`:

```cpp
Status _eepromOperationStatus = Status::Ok();
Status _eepromCleanupStatus = Status::Ok();
```

When an idle configuration queue admits the first item of a new batch, reset
both batch fields to `OK`. Before clearing `JobOp` at each item boundary, latch
its first non-OK operation/cleanup status into the corresponding batch member.
A cleanup failure still cancels later queue entries, but the two batch fields
remain available through `getEepromStatus()`/`SettingsSnapshot` until a new
batch, `begin()`, or `end()` resets them.

Keep a terminal-precedence status for `getEepromStatus()`:

1. return `IN_PROGRESS` while active;
2. if cleanup failed, return semantic `EEPROM_CLEANUP_FAILED` with the exact
   cleanup code retained in the snapshot/report;
3. otherwise return the first operation failure; and
4. otherwise return `OK`.

`getPersistentReadJobResult()` and `getUserEepromWriteJobResult()` retain their
existing convention: after copying a matching completed report, return the
exact terminal job status, not a generic retrieval `OK`. On `IN_PROGRESS` or
`JOB_RESULT_UNAVAILABLE`, leave output unchanged.

Test combined forward and cleanup failures, full proof followed by cleanup
failure, partial proof, exact details, and generic queue preservation of the
first forward cause.

## 10. Correct periodic-update and quiescence contracts

Use these exact signatures:

```cpp
Status setPeriodicUpdate(
    PeriodicUpdateFrequency frequency,
    bool updateEventEnabled);

Status getPeriodicUpdate(
    PeriodicUpdateFrequency& frequency,
    bool& updateEventEnabled);
```

The Boolean name is part of the documentation contract. State explicitly:

- false disables both UF generation and the INT event;
- there is no UF-polling-only mode; and
- `getPeriodicUpdateFlag()` cannot observe a newly generated update event while
  UIE=0.

Application Manual pages 81-82 support this behavior and the staged order in
section 8. Add exactly one fake event hook,
`triggerPeriodicUpdateEvent()`: it sets UF/INT evidence only when UIE is one.

For other live reconfiguration, use explicit precondition reads rather than
hidden disable/re-enable policy:

- `setTimer()`/`startSetTimerJob()` require TIE=0 through their dedicated timer
  guard state;
- `setAlarmTime()` and `setAlarmMatch()` require AIE=0;
- `setEviEdge()`, `setEviDebounce()`, `setEviOverwrite()`,
  `setEventSynchronizationEnabled()`, and EVI-source `resetTimestamp()` require
  EIE=0; and
- `startSetBackupSwitchModeJob()` requires BSIE=0 through its dedicated backup
  guard state.

Do not apply the EIE guard to `setClkoutStopDelayEnabled()`: CLKDE configures
CLKOUT stop delay after I2C STOP and is not an EVI event/capture setting in the
vendor procedure.

Extend the existing register-update owners with this private fixed guard, not
operation-specific duplicate code:

```cpp
struct QuiescenceGuard {
  uint8_t reg = 0;
  uint8_t forbiddenMask = 0;
};
```

Add `JobState::REGISTER_UPDATE_READ_QUIESCENCE` and these `JobOp` fields:

```cpp
QuiescenceGuard quiescenceGuard{};
JobState quiescenceNextState = JobState::IDLE;
```

Add a `const QuiescenceGuard&` parameter to private
`updateRegisterSingle()`/`updateRegisterBlock()`. A zero mask skips the guard.
Otherwise admission starts at `REGISTER_UPDATE_READ_QUIESCENCE`; a clear guard
continues to the saved existing single/block read state. Alarm and EVI guarded
updates therefore have a three-callback cap. EVI timestamp reset prepends the
same guard to its existing read/clear/set sequence and has a four-callback cap.

If the enable is set, return `BUSY` after the read and perform zero writes. Do
not clear any flag implicitly. Doxygen must give this caller sequence:

```text
disable interrupt -> consume/clear flag if appropriate -> configure -> enable
```

For EVI, say explicitly that EIE=0 suppresses interrupt signaling but does not
stop timestamp capture; an external edge can still occur during configuration.
This item implements vendor-recommended hardening, not a claim that a measured
transient fault was reproduced on physical silicon.

## 11. Return the result from `tick()`

Change:

```cpp
Status tick(uint32_t nowMs);
```

Return `pollEeprom(nowMs, 1, instructionsUsed)` exactly. Before `begin()`, it
returns `NOT_INITIALIZED`; while an ordinary job owns the engine, it returns
`BUSY`; otherwise it exposes EEPROM progress or terminal status. Do not clear a
cached error to avoid repeated reporting. Callers may explicitly discard the
result, but the library must not discard it internally.

Update public Doxygen, README, architecture, IDF port notes, examples, tests,
and contract guards that assume `void tick`.

## 12. Make the example transport honor its timeout

In `examples/common/I2cTransport.h`, add this fixed RAII owner:

```cpp
class ScopedWireTimeout {
 public:
  inline ScopedWireTimeout(TwoWire& wire, uint16_t timeoutMs)
      : _wire(wire), _previousTimeoutMs(wire.getTimeOut()) {
    _wire.setTimeOut(timeoutMs);
  }

  inline ~ScopedWireTimeout() {
    _wire.setTimeOut(_previousTimeoutMs);
  }

  ScopedWireTimeout(const ScopedWireTimeout&) = delete;
  ScopedWireTimeout& operator=(const ScopedWireTimeout&) = delete;

 private:
  TwoWire& _wire;
  uint16_t _previousTimeoutMs;
};
```

Define callback-local detail constants for invalid argument, invalid timeout,
and short staging. Every return from `wireWrite()`/`wireWriteRead()`, including
adapter validation, must be `OK` or one of the permitted `I2C_*` codes. Use
`I2C_ERROR` plus the detail constant for impossible local callback arguments;
do not return `INVALID_CONFIG` or `INVALID_PARAM` from application callbacks.

After validation and immediately before each transaction, require `timeoutMs`
in `1..UINT16_MAX`, sample `millis()`, and create one wrap-safe absolute
callback deadline. Before each potentially blocking Wire operation, recompute
remaining time and apply only that remainder with `ScopedWireTimeout`. Abort
with `I2C_TIMEOUT` when no time remains, and post-check the whole callback even
when Wire returns success. The outer timeout scope restores the previous Wire
timeout on every return.

Before `beginTransmission()`, require more than 1 ms remaining and reserve the
last millisecond for transaction release. Bound the normal physical
transaction by `remainingMs - 1`. The reserved millisecond lets the short-stage
error path execute its one final-stop release; if that release reaches the
callback deadline, return `I2C_TIMEOUT` rather than success.

For the pinned ESP32 Arduino 3.2.0 implementation,
`endTransmission(false)` only stages the repeated-start transaction and
`requestFrom(..., true)` executes the combined write-read with one timeout.
Still compute the remaining callback time immediately before `requestFrom()`;
do not give two phases a fresh full timeout. The application owner must keep
the shared Wire mutex uncontended; this adapter does not own bus scheduling.

After `beginTransmission()`, every exit path must close/release the Wire
transaction. If `wire.write()` stages fewer bytes than requested, call one
bounded `endTransmission(true)` to release ownership, then return `I2C_ERROR`
with the short-staging detail. That single partial physical attempt is
ambiguous and must never be retried. Normal write-read uses exactly one
`endTransmission(false)` followed by one final-stop `requestFrom()`.

The example does not add recovery or retry.

Map Wire result code 1 to `I2C_ERROR`, not `INVALID_PARAM`, because the callback
was already dispatched. Map all post-dispatch failures into the permitted I2C
domain.

Make `initWire()` return false when either `Wire.begin(sda, scl)` or
`Wire.setClock(frequency)` returns false. Never report the bus ready after
either failure.

Document `Config::i2cTimeoutMs` and callback `timeoutMs` as a hard upper bound
for the complete callback, not an advisory value. Extend the test Wire stub to
record requested/effective/restored timeouts and prove that the 5 ms
primary-cell callback bound is actually applied. Also inject a short staging
write and prove the following transaction is admitted rather than forever
waiting on a leaked Wire mutex; inject `begin()` and `setClock()` failures.

## 13. Make the example CLI strict and single-owner

Delete the local `read_line()` from
`examples/01_basic_bringup_cli/main.cpp`. Reuse the existing overflow-discarding
`cli_shell::readLine(String&)` as the sole line reader. Do not add another line reader.
Delete the unused permissive `CommandHandler::readLine`, `parseInt(atoi)`, and
`match` helpers so no alternate parsing path remains.

Place these strict helpers in the existing
`examples/common/CommandHandler.h` owner:

```cpp
bool parseU8Token(const String&, uint8_t&);
bool parseU16Token(const String&, uint16_t&);
bool parseU32Token(const String&, uint32_t&);
bool parseInt32Token(const String&, int32_t&);
bool parseFloatToken(const String&, float&);
bool parseBool01Token(const String&, bool&);
bool parseRegisterToken(const String&, uint8_t&);
```

Each helper must inspect the end pointer and `ERANGE`, reject negative unsigned
input, require exact token consumption, range-check before narrowing, and use
`isfinite()` for float. Every mutating command must require the exact argument
count and reject trailing tokens. Numeric helpers other than
`parseRegisterToken()` are base-10 only. `parseRegisterToken()` accepts either
decimal or an explicit `0x`/`0X` hexadecimal prefix; it never interprets a
leading zero as octal.

Replace every mutation-facing `toInt()`, `toFloat()`, unchecked `strtoul()`,
and permissive `sscanf()`. Cover calendar set, Unix time, alarm, Boolean
interrupts, CLKOUT, offset, timer, EVI, Status clear, register access, verbose,
and stress counts.

Track cooperative ownership with this fixed type:

```cpp
enum class PendingSurface : uint8_t {
  NONE,
  ORDINARY_JOB,
  EEPROM
};

struct PendingOperation {
  PendingSurface surface = PendingSurface::NONE;
  const char* name = nullptr;
  Status ordinaryStatus = Status::Ok();
};
```

`operationAccepted()` records the operation when a call returns `IN_PROGRESS`.
For `ORDINARY_JOB`, call `pollJob(nowMs, 1, instructionsUsed)`. On terminal,
store that status in `ordinaryStatus`; if `isEepromBusy()` is true, change the
surface to `EEPROM`, return from the owner poll, and do not admit or dispatch
another command in that iteration. If EEPROM is not busy, report
`ordinaryStatus`, replace the sole pending record with a value-initialized
`PendingOperation{}`, and return without another RTC/I2C callback. For
`EEPROM`, call `pollEeprom(nowMs, 1, instructionsUsed)` until terminal. Only
then report both the active-job and persistence outcomes and clear the pending
record. Never infer the current surface from shared `isJobBusy()`.

Do not dispatch another RTC/I2C command until both surfaces are terminal.
Non-I2C help text may remain available.

For a typed persistent read, set the helper's local deadline equal to the exact
whole-operation deadline passed at job admission. If status remains
`IN_PROGRESS`, call `pollJob()` once with that deadline value so the driver
terminates. A generic queue uses `pollEeprom()`, not `pollJob()`. Neither helper
may return while leaving untracked active work.

Parser tests must cover 256, negative unsigned values, integer overflow, junk,
trailing tokens, empty tokens, NaN, infinity, whitespace, and overlength lines.
Every invalid mutating command must make zero callbacks.

## 14. Correct example completion and public presence semantics

- `cmd_ram_write()` must treat `IN_PROGRESS` as accepted, poll the recorded
  ordinary job, and print success/failure only at terminal completion. A
  16-byte write must not print failure and then mutate.
- `cmd_ts()` must branch on timestamp `timeValid`; for an empty block, print an
  explicit empty/unset result plus count and never print a fictitious date.
- README snippets must sample time first:

  ```cpp
  const uint32_t now = nowMs(nullptr);
  ```

  Pass `now`, not the function pointer.
- Add compile-only coverage for maintained README snippets.
- Update HIL expected strings when completion wording changes.

Delete `isOnline()` from the public API. `state()` already exposes the
observational READY/DEGRADED/OFFLINE label; another Boolean invites a false
presence interpretation. Update health formatting to use `DriverState`.

Change `probe()` documentation from "presence and identity" to "address
response/communication". One Status-register read at `0x51` cannot identify
silicon. After passive `begin()`, READY remains an observational initial state,
not presence evidence. Applications that need presence retain the explicit
`probe()` result.

## 15. Remove unsafe public utilities and dead state

Delete public `binaryToBcd()` and `bcdToBinary()`. They have no production
caller and expose unchecked internal encoding. Keep private strict BCD helpers
only for already validated values; delete the silent `0x99` clamp.

Use these fallible public signatures:

```cpp
static Status computeWeekday(
    uint16_t year, uint8_t month, uint8_t day, uint8_t& weekday);

static Status parseBuildTime(DateTime& out);

static Status unixToDateTime(
    uint32_t timestamp, DateTime& out);

static Status dateTimeToUnix(
    const DateTime& time, uint32_t& timestamp);
```

Keep `isValidDateTime()` as a Boolean predicate. Add one private helper:

```cpp
static uint8_t weekdayFromValidDate(
    uint16_t year, uint8_t month, uint8_t day);
```

The public weekday method validates year `2000..2099` and the complete
Gregorian date before entering the bounded calculation. `unixToDateTime()`
accepts exactly `946684800..4102444799` (2000-01-01 through
2099-12-31 23:59:59 UTC). `dateTimeToUnix()` accepts the same calendar domain.
Invalid date/domain/build-string input returns `INVALID_DATETIME`; success
returns `OK`. Every output argument remains unchanged on failure. Update all
internal and example callers.

Delete:

```text
SettingsSnapshot::beginInProgress
RV3032::_beginInProgress
EepromState::ComparePersistent
EepromState::VerifyWriteFlags
EepromState::BeginVerifyPersistent
JobState::SetTimerReadTimerHigh
```

Delete all checks/resets for `_beginInProgress`. `begin()` is passive and does
not need an internal I/O bypass.

Because the private state machines are already being structurally rewritten,
mechanically rename every retained `JobKind`, `JobState`, and `EepromState`
enumerator to `CAPS_CASE` in the same change. This is a private naming cleanup,
not a logic change. All new private enumerators in this prompt use the required
target spelling.

An impossible/default state produces `INTERNAL_STATE_ERROR`, never `OK` or a
misleading EEPROM verification error. Latch it as the operation failure only
when no earlier operation failure exists. If mutation evidence is active and
cleanup has not started, enter the owning bounded cleanup/reconciliation once.
If already in cleanup, preserve the forward failure, latch the internal error
as cleanup failure only when no earlier cleanup failure exists, and terminate
with the applicable semantic cleanup result without restarting cleanup. If
persistent access cleanup is required, use bounded C0/Control 1 cleanup.
Finish immediately only when no cleanup is required. Extend the source-contract
guard to assert these routes. Do not add a production or test-only state
mutator.

## 16. Make health evidence internally consistent

After the raw-result refactor, add:

```cpp
Status _i2cWriteReadPresenceTracked(
    const uint8_t* tx,
    size_t txLen,
    uint8_t* rx,
    size_t rxLen);
```

It calls the raw wrapper, applies `mapPresenceError()` to the returned status,
and only then updates health when `callbackInvoked` is true. Use it in
`recover()`. `recover()` and `lastError()` must both report
`DEVICE_NOT_FOUND` for the same address NACK.

Follow the binding health contract: lifetime counters wrap from `UINT32_MAX` to
zero. Delete the saturation guards and use ordinary unsigned increment. Keep
Doxygen's wrap wording and add a source-contract check that the old
`< UINT32_MAX` guards are absent.

## 17. Documentation contract

Update all maintained authorities in the same change:

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

Required documentation corrections:

- Remove password-management capability, sequencing, credential, and cleanup
  claims. Retain vendor register facts and explicit unsupported-range denial.
- In `DEVICE_REFERENCE.md`, say password management is intentionally
  unsupported and pre-protected parts require out-of-band service.
- In `docs/README.md`, list this report as the active v3 hardening prompt and
  classify Prompt Suite 04, Prompt 05, and dated v2 reports as historical
  artifacts. Do not silently rewrite those historical files.
- Change weekday wording in both `CommandTable.h` and
  `DEVICE_REFERENCE.md` to a user-assigned three-bit binary value `0..6`, not
  BCD.
- Every named simple Status flag clearer for AF, TF, UF, EVF, PORF, and VLF
  must repeat or cross-link this warning:

  > Any Status-register write clears THF and TLF in silicon. If either omitted
  > flag is already set at the guard read, the operation returns INVALID_PARAM
  > without writing. An assertion after the guard read cannot be preserved.

- Document the verified calendar set separately. Its public name deliberately
  promises PORF/VLF clearing, it captures THF/TLF pre-clear evidence in
  `VerifiedTimeSetReport`, and the Status write then clears THF/TLF as an
  explicit unavoidable side effect. Do not document that job as taking the
  simple-clearer `INVALID_PARAM` exit.

- Change the broad `Status.h` statement from "All library functions return
  Status" to "All fallible operations return Status".
- Document all exclusive deadlines, callback hard bounds, fail-safe states,
  terminal evidence precedence, update-event semantics, and application-owned
  serialization introduced above.

Password removal, deleted `isOnline()`, noncopyability, `tick()` returning
`Status`, and checked conversion signatures are breaking API changes. Set
the version with `python scripts/generate_version.py set 3.0.0`; this updates
`library.json` and generates the header. Then run `check`; never hand-edit
`include/RV3032/Version.h`. Put the changes under `CHANGELOG.md` Unreleased with
clear Removed/Changed/Fixed sections. Do not add a release date, commit, or tag
without explicit authorization.

## 18. Required verification and acceptance

### 18.1 Native behavioral matrix

The native suite must cover at least:

1. password symbol deletion and hard denial of both password ranges;
2. callback status normalization and attempted-vs-local health accounting;
3. post-callback deadline boundaries, clipping, no-hook accounting, and wrap;
4. cleanup-reserve admission, no forward mutation callback at/after the
   mutation cutoff, permitted bounded cleanup before the whole deadline, and
   no callback at/after an applicable whole/phase deadline;
5. TEMP_LSB target-by-neighbor race injection and exact payloads;
6. unconditional zero-I/O `end()` during every owner state;
7. compile-time noncopyable/nonmovable traits;
8. backup raw-disabled variants, 2/10 ms waits, verification, and no early
   persistence;
9. every callback fault ordinal for timer, periodic update, CLKOUT,
   temperature, and backup staged jobs;
10. persistence operation/cleanup evidence combinations;
11. UIE=0 generating neither UF nor INT evidence;
12. TIE/AIE/EIE/BSIE guard reads producing zero writes when busy;
13. Wire effective whole-callback timeout, timeout restoration, short-staging
    transaction release, and initialization failure handling;
14. strict CLI invalid-input zero-I/O cases and overlength discard;
15. 16-byte asynchronous RAM completion and empty timestamp output;
16. passive READY without a presence Boolean and consistent recover health;
17. invalid conversion domains with unchanged outputs; and
18. source-contract and reachable-state coverage for
    `INTERNAL_STATE_ERROR` cleanup/default routes;
19. read-only owner recovery plus one retry staying inside one callback bound
    and one driver instruction; and
20. lifetime counter wrap without an unbounded runtime loop or a test-only
    production setter: source-contract evidence proves ordinary `uint32_t`
    increment and absence of saturation guards, and a test-local compile-time
    assertion proves `UINT32_MAX + uint32_t{1}` is zero.

For each mutating state machine, assert both a fixed callback cap and no retry
of an ambiguous requested write. Keep queue and buffer capacity fixed.

### 18.2 Commands

Run and record all of these after implementation:

```text
python scripts/generate_version.py check
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_docs_contract.py source
pio test -e native
pio run -e esp32s2dev
pio run -e esp32s3dev
$packagePath = "dist/RV3032-C7-hardening-audit.tar.gz"
if (Test-Path -LiteralPath $packagePath) { throw "Preserve existing package artifact" }
pio pkg pack -o $packagePath .
python tools/check_docs_contract.py package $packagePath
python tools/hil_cli_runner.py --parser-self-test
python tools/hil_cli_runner.py --dry-run
```

The package path belongs to the implementation only when the preflight proves
it did not exist. If it exists, preserve it and choose another unique path;
use the same path for pack and package validation. After inspecting the
archive, resolve its absolute path, prove it remains inside this repository's
`dist` directory, and remove only that newly created file with literal-path
semantics.

On this Windows workspace, use the PlatformIO executable under
`%USERPROFILE%\.platformio\penv\Scripts\` if `pio` is not on `PATH`.

Also run a forbidden-symbol scan. Production header/source must contain none
of the deleted password management API/state names. Password text/constants may
remain in `CommandTable.h` and in the narrowly scoped
`intersectsUnsupportedPasswordRange()` guard/message. Password words may also
remain in unsupported-access documentation/tests, vendor facts, historical
artifacts, and changelog history.

### 18.3 Physical validation boundary

Do not run destructive HIL without the existing explicit port/module/power and
backup-cell confirmations. When authorized, validate:

- VDD removal immediately after successful DSM and LSM job completion, using
  only the topology restrictions stated in section 7;
- no early success at 1 ms before each activation boundary;
- INT/CLKOUT behavior during the documented quiescence sequences;
- EEPROM cleanup under voltage/busy fault conditions; and
- effective transport timeout behavior on the actual application bus owner.

If HIL is not run, state `NOT RUN`. Do not convert fake-bus evidence into a
claim of retention, voltage safety, or physical timing.

## 19. Definition of done

The implementation is complete only when:

- all password-management code is deleted and the blocked ranges remain
  inaccessible;
- all five High findings have code-level regression tests;
- every Medium and Low item above is implemented and verified exactly;
- public headers, implementation, maintained docs, examples, fake, tests, and
  contract guards agree;
- all required device-free commands pass;
- the worktree contains no unrelated rewrite, generated junk, release tag, or
  unauthorized commit; and
- the final implementation report distinguishes software verification from
  physical HIL.

A genuine blocker must be reported with evidence and stops the work; it means
this definition of done has not been met.

The final handoff must lead with the achieved behavior, list each closed
finding, give exact test/build results, identify HIL as PASS/FAIL/NOT RUN, and
state any remaining field risk. A green build alone is not sufficient closure.
