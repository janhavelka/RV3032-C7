# AI Coder Prompt 04.4: Dedicated Primary-Cell Provisioning And Raw-Access Safety

## Phase contract

This is phase 4 of 5 in Prompt Suite 04.

- Required first: read `04-00-shared-contract.md` completely.
- Prerequisites: phases 1 through 3 are complete or their lifecycle, job, and persistence invariants have been re-audited.
- Current scope: Implement the sole synchronous multi-callback exception, its once-per-lifecycle latch, exact C0 procedure, bounded cleanup, and reviewed raw/register policy.
- Exit criteria: ensure is explicit and structurally limited to one WRITE_ONE attempt, all timing/cleanup/report contracts hold, and unsafe raw side-effect routes are closed.
- Preserve unrelated work and keep the shared dated requirement/evidence report
  current. Do not commit, tag, publish, flash, or run hardware EEPROM work.

## Dedicated primary-cell configuration API

### Public result

Use a compact production report. Exact callback logs stay test-only.

```cpp
enum class PrimaryCellConfigurationOutcome : uint8_t {
  NOT_ATTEMPTED = 0,
  ALREADY_CONFIGURED = 1,
  EEPROM_UPDATED = 2,
  FAILED = 3,
};

enum class PrimaryCellFailureStage : uint8_t {
  NONE = 0,
  PRECONDITION = 1,
  PREPARE_ACCESS = 2,
  READ_PERSISTENT = 3,
  WRITE_PERSISTENT = 4,
  VERIFY_PERSISTENT = 5,
  CLEANUP = 6,
  SETTLE = 7,
};

struct PrimaryCellConfigurationReport {
  PrimaryCellConfigurationOutcome outcome =
      PrimaryCellConfigurationOutcome::NOT_ATTEMPTED;
  PrimaryCellFailureStage failureStage = PrimaryCellFailureStage::NONE;
  Status operationStatus = Status::Ok();
  Status cleanupStatus = Status::Ok();
  uint8_t persistentBefore = 0;
  uint8_t persistentTarget = 0;
  uint8_t persistentAfter = 0;
  uint8_t activeAfter = 0;
  uint8_t control1After = 0;
  bool persistentBeforeValid = false;
  bool persistentAfterValid = false;
  bool writeCommandAttempted = false;
  bool writeDurablyVerified = false;
  bool cleanupVerified = false;
  bool autoRefreshHeldDisabledForSafety = false;
};

Status ensurePrimaryCellConfiguration(
    PrimaryCellConfigurationReport& report);
```

Test-only/fake evidence must separately retain exact callback statuses,
ambiguity, command counts, EEF/busy observations, and transfer log. Do not bloat
the public report only to mirror the fake.

### Invocation and latch

The method:

- requires successful begin;
- requires `Config::nowMs` and `Config::waitMs`;
- returns BUSY with zero callbacks, unchanged report, and no consumed attempt if
  a job, EEPROM state, or EEPROM queue item is active;
- may be invoked once per successful begin/end lifecycle;
- marks `_primaryCellEnsureAttempted=true` after all local preconditions and
  immediately before the first time/transport callback;
- a second same-lifecycle call returns PRIMARY_CELL_ALREADY_ATTEMPTED with zero
  callbacks and unchanged report;
- is synchronous and never returns IN_PROGRESS;
- does not enqueue work or call `tick()`, `pollEeprom()`, or `pollJob()`;
- allocates no heap and stores no async provisioning object;
- never runs from begin, recover, tick, setters, calendar jobs, or periodic work.

The library enforces at most one invocation per begin/end lifecycle; this is a
wear and ambiguity guard, not an instruction to call the method every
lifecycle. Applications choose whether to call it at all and may add a stricter
outer policy. TunnelMonitor's later integration will invoke it once per worker
startup. Library recovery never clears the latch. Only `end()` followed by a
new successful `begin()` creates a new opportunity for an explicit caller.

### Constants and deadlines

Use these names/values:

```cpp
CONTROL1_IMPLEMENTED_MASK = 0x3F;
CONTROL1_EERD_MASK = 0x04;
PMU_IMPLEMENTED_MASK = 0x7F;
PMU_NCLKE_MASK = 0x40;
PMU_BSM_MASK = 0x30;
PMU_BSM_DISABLED = 0x00;
PMU_BSM_DIRECT = 0x10;
PMU_BSM_LEVEL = 0x20;
PMU_TCR_MASK = 0x0C;
PMU_TCM_MASK = 0x03;
PMU_PRIMARY_PRESERVE_MASK = 0x4C;
EEPROM_BUSY_MASK = 0x04;
EEPROM_EEF_MASK = 0x08;
TEMP_CLKF_MASK = 0x02;
TEMP_BSF_MASK = 0x01;
EEPROM_CLEAR_EEF_VALUE = 0x03;
PERSISTENT_READ_SENTINEL = 0x80;

EEPROM_READ_SETTLE_MS = 1;
EEPROM_WRITE_SETTLE_MS = 10;
EEPROM_BUSY_POLL_INTERVAL_MS = 1;
EEPROM_READY_TIMEOUT_MS = 250;
EEPROM_READ_TIMEOUT_MS = 25;
EEPROM_WRITE_TIMEOUT_MS = 100;
EEPROM_CLEANUP_READY_TIMEOUT_MS = 250;
PRIMARY_CELL_OPERATION_TIMEOUT_MS = 1000;
PRIMARY_CELL_WRITE_START_CUTOFF_MS = 500;
PRIMARY_CELL_MIN_CLEANUP_RESERVE_MS = 300;
PRIMARY_CELL_TRANSFER_TIMEOUT_MS = 5;

EEPROM_READY_CHECK_CAP = 256;
EEPROM_READ_CHECK_CAP = 32;
EEPROM_WRITE_CHECK_CAP = 101;
EEPROM_CLEANUP_CHECK_CAP = 256;
PRIMARY_CELL_WRITE_COMMAND_CAP = 1;
```

Use wrap-safe unsigned 32-bit elapsed subtraction; every duration is below
`2^31`.

The 1000 ms terminal bound includes normal work, ambiguous-command resolution,
cleanup, and final 10 ms settle. Pass to each provisioning callback:

```text
min(config.i2cTimeoutMs,
    PRIMARY_CELL_TRANSFER_TIMEOUT_MS,
    phaseRemaining,
    overallRemaining)
```

Require the 300 ms cleanup reserve before each non-cleanup mutation or
post-command reconciliation callback. Re-read time immediately before `0x21`;
no write-one may start at or after 500 ms elapsed. Recheck the cutoff before
marking the command attempted and before dispatch.

The callback/wait contracts are external. The library cannot preempt a callback
that returns late. The <=1000 ms wall claim applies when callbacks honor their
supplied bounds. On a late return, start no new normal phase; run only safety
cleanup that still fits measured remaining time and return TIMEOUT.

### Wait behavior

The synchronous wait helper:

- requests only the needed 1/10 ms or smaller remaining interval;
- calls injected `waitMs`, which must yield/sleep and never perform I2C;
- reads `nowMs` after return;
- requires measured elapsed >= requested delay;
- treats an early-returning wait as TIMEOUT before a completion poll;
- never loops without a callback or wait;
- stops at the earliest check cap, phase deadline, or overall deadline.

## Exact primary-cell procedure

Any non-EECMD mutating callback error ends normal forward progress. Never resend
it and never authorize a later EECMD. One readback may gather cleanup evidence,
but it does not erase the typed transport failure. Only an ambiguous `0x22` with
complete sentinel proof or an ambiguous `0x21` with busy/EEF/persistent proof
may reconcile to success.

### A. Prepare access

1. Record operation start/deadline.
2. Read Control 1 and preserve valid evidence.
3. Compute EERD-on as
   `(control1Before & CONTROL1_IMPLEMENTED_MASK) | CONTROL1_EERD_MASK`.
4. Write EERD-on only if needed; read and verify all implemented bits.
5. Poll TEMP_LSB until EEbusy=0, bounded by 250 ms/256 checks/overall time.
   The first TEMP_LSB read is check one.
6. Read active C0 only after not-busy and preserve it.
7. Compute `safeActive = activeBefore & 0x4C`.
8. Write only if needed; read back and require
   `(value & 0x7F) == safeActive`.
9. Issue no EECMD unless EERD-on and safe active C0 are both verified.

This ordering prevents a refresh from making the active snapshot stale.

### B. Direct-read persistent C0

1. Write EEADDR=`0xC0`; read back and require `0xC0`.
2. Write EEDATA=`0x80`; read back and require the sentinel.
3. Positively require EEbusy=0 immediately before EECMD.
4. Start the 25 ms completion phase, record command attempt evidence, and issue
   read-one `0x22` exactly once.
5. If the callback errors, mark the command ambiguous; never resend it.
6. Wait at least 1 ms, then poll EEbusy at intervals >=1 ms, bounded by
   25 ms/32 checks/overall time.
7. Read EEDATA. Require bit 7 clear and value different from `0x80`.

Because valid C0 bit 7 is always zero, replacement of a verified `0x80`
sentinel is direct persistent-read evidence in the serialized single-owner
model. If an ambiguous `0x22` satisfies every proof step, retain the callback
error in test evidence and allow the read result.

Compute:

```cpp
target = static_cast<uint8_t>((persistentBefore & 0x4C) | 0x20);
```

If `persistentBefore == target`, issue no `0x21`, copy before to after evidence,
and enter trusted cleanup. ALREADY_CONFIGURED is terminal only after cleanup and
settle succeed.

### C. Write at most one byte

If persistent state differs:

1. Require elapsed <500 ms and sufficient cleanup reserve before preparation.
2. Clear stale EEF by writing TEMP_LSB exactly `0x03`, preserving CLKF/BSF.
3. Read TEMP_LSB and require EEbusy=0 and EEF=0.
4. Write/read back EEADDR=`0xC0`.
5. Write/read back EEDATA=`target`.
6. Immediately before dispatch, require EEbusy=0, re-read `nowMs`, require
   elapsed <500 ms, and require callback bound plus 300 ms reserve to fit.
7. Only then set write-attempt evidence and issue `0x21` exactly once. Any
   callback error is ambiguous; never issue a second `0x21`.
8. Wait at least 10 ms, then poll EEbusy at >=1 ms intervals, bounded by
   100 ms/101 checks/overall time.
9. Read TEMP_LSB and record EEF.
10. Only after EEbusy=0 is positively proved, perform one more sentinel-protected
    direct `0x22` read regardless of `0x21` callback result or EEF. If busy is
    unknown/timed out, issue no more EECMD.
11. Success requires one attempted `0x21`, busy clear, EEF valid/clear, and exact
    persistent target equality.
12. If the `0x21` callback errored but every proof succeeds, set
    `writeDurablyVerified=true` and allow EEPROM_UPDATED while preserving exact
    callback evidence in tests.

### D. Cleanup

Once Control 1 has been read, route every communicable exit through cleanup.
Track whether EERD-on was verified, C0 changed, safe active C0 was verified, an
EEPROM command may still be active, and persistence is trusted.

Persistence is trusted only when:

- the first direct read already matched target; or
- the post-write direct read matched target and current EEF is valid/clear.

Branches:

1. No verified access-state change:
   - invent/write no C0 value;
   - if a Control 1 write may have occurred, best-effort restore/read back the
     original implemented-bit value.
2. Trusted persistent target:
   - establish EEbusy=0 within cleanup cap and remaining time;
   - write exact target to active C0 at most once and read it back;
   - only after exact active target proof, write/read back Control 1 with EERD
     cleared while preserving original implemented bits;
   - anchor a 10 ms level-switch settle at the exact target-C0 readback and
     require measured elapsed >=10 ms before success;
   - success requires active target, EERD=0, cleanup verification, and settle.
3. Untrusted persistence after access began:
   - establish EEbusy=0 if possible;
   - use verified safeActive; otherwise derive from valid activeBefore; otherwise
     make one fresh active-C0 read and derive `fresh & 0x4C`;
   - if no valid byte exists, invent/write no C0;
   - when a safe value exists, write once and verify exact BSM00/TCM00;
   - best-effort write/read back Control 1 with EERD set and other implemented
     bits preserved;
   - set `autoRefreshHeldDisabledForSafety=true` only after exact EERD=1 proof;
   - always return failure.

If EEbusy cannot be established clear, never enable BSM. Hold EERD set when
possible and return cleanup failure. A cleanup mutating callback error is never
resent; one readback may classify state but does not erase the error.

The EERD1/BSM00 failure hold is volatile and POR does not honor it. Document that
a failed module is not deployment-ready until the application explicitly calls
ensure again in a later complete lifecycle and it succeeds. That later explicit
call reads actual chip state first and never trusts process RAM.

### E. Repeatability and wear

- Correct invocation: exactly one completed `0x22`, zero `0x21`.
- Wrong invocation: at most one `0x21`, at most two completed `0x22`.
- Second same lifecycle: zero callbacks, PRIMARY_CELL_ALREADY_ATTEMPTED.
- End/begin creates a new lifecycle opportunity; a subsequent explicit ensure
  call directly rechecks persistence.
- Prior ambiguous committed write is observed as correct and causes zero new
  `0x21`.
- Begin, probe, recover, tick, generic setters, calendar jobs, example startup,
  and periodic work never invoke primary-cell ensure. Persistent C0 is not
  inspected merely because a library lifecycle began.
- Primary ensure leaves calendar, C1-CA, alarm, timer, RAM, and user EEPROM
  unchanged.

## Register and raw-access policy

Correct the command table and used masks. At minimum:

- Control 1 implements bits 5:0; bits 7:6 write zero.
- EERD is bit 2.
- Control 2 AIE is bit 3 and EIE is bit 2.
- Control 3 is documented.
- EVI has no enable bit 3.
- Timer-high writes only bits 3:0.
- TEMP_LSB flag/temperature semantics are exact.
- C0 TCR and TCM are distinct.
- `0x21` is WRITE_ONE; `0x11` is UPDATE_ALL.
- Active mirrors and indirect EEPROM constants are distinct:

```cpp
REG_ACTIVE_PMU = 0xC0;
REG_ACTIVE_OFFSET = 0xC1;
REG_ACTIVE_CLKOUT1 = 0xC2;
REG_ACTIVE_CLKOUT2 = 0xC3;
REG_ACTIVE_TREFERENCE0 = 0xC4;
REG_ACTIVE_TREFERENCE1 = 0xC5;
CONFIG_EEPROM_START = 0xC0;
CONFIG_EEPROM_END = 0xCA;
USER_EEPROM_START = 0xCB;
USER_EEPROM_END = 0xEA;
```

Public direct reads may address documented direct ranges only. Public direct
writes require a reviewed per-register allowlist and reject reserved,
read-only, Status/control side-effect, password, EEADDR/EEDATA/EECMD, and
indirect EEPROM addresses before I2C. Do not expose password guessing/unlock or
a generic EECMD API.

Audit every Status writer for unconditional THF/TLF clear. Prefer explicit API
names over a raw status-mask helper.

