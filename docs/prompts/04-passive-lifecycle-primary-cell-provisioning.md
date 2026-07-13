# AI Coder Prompt: General RV3032-C7 Cooperative Driver And Optional Primary-Cell Provisioning

## Assignment

Work in the `RV3032-C7` library repository and implement a deliberate `2.0.0`
general-purpose driver hardening release. TunnelMonitor is one demanding
integration profile and validation consumer; it must not become the library's
lifecycle, persistence, battery-chemistry, retry, or scheduling policy.

Preserve the library's useful cooperative, fixed-capacity execution model.
Ordinary multi-transfer operations remain chunked through the existing
`start...Job()`/`pollJob()` and `tick()`/`pollEeprom()` surfaces. Each caller
chooses the instruction budget. A resource owner such as TunnelMonitor may use
a budget of one callback per I2C-owner poll; simpler applications may use a
larger bounded budget.

Make the library useful independently of TunnelMonitor. Audit the complete
vendor-documented functional surface, preserve and correct existing features,
and add missing typed functionality rather than treating unused TunnelMonitor
features as out of scope. At minimum this release adds:

1. A passive, zero-I/O `begin()`.
2. A documented capability matrix and typed public coverage for the chip's
   calendar, alarm, periodic timer/update, event/timestamp, temperature,
   CLKOUT, calibration, backup/trickle, status/reset, user RAM, user EEPROM,
   configuration persistence, and password/protection functions where the
   vendor documents safe application control.
3. A product-neutral chunked status-first calendar-read job.
4. A product-neutral chunked verified calendar-set-and-invalid-flag-clear job.
5. Explicit persistent inspection operations that never run from lifecycle
   methods.
6. One opt-in synchronous `ensurePrimaryCellConfiguration()` convenience
   operation for applications that explicitly declare a non-rechargeable
   primary backup cell.

The primary-cell operation is the only multi-callback method allowed to finish
its full chip sequence in one call. The library never invokes it automatically.
An application may never call it, call it on an operator-controlled path, or
adopt its own once-per-lifecycle policy. TunnelMonitor's later integration will
explicitly invoke it once in the I2C worker during startup. It normally checks
the persistent byte and performs no EEPROM write. It may issue one write-one
command only when the persistent byte is wrong.

Do not replace the existing cooperative library with an all-synchronous driver.
Do not force any application's scheduler into the library. Preserve explicit
instruction budgets so TunnelMonitor can retain normal one-step-per-poll
scheduling while other callers select another bounded budget. Prefer extending
the existing `JobOp`, `JobState`, `EepromOp`, and fixed buffers over adding a
second scheduler, queue, registry, task, or framework.

The result must be simple, deterministic, fixed-capacity, vendor-correct, and
neutral about application policy. When a caller explicitly selects the
primary-cell helper, that path must be safe for a non-rechargeable cell.

Do not edit TunnelMonitor in this task. Exact dependency pinning and firmware
integration are later work.

## Non-negotiable execution model

| Work | Required execution |
| --- | --- |
| `begin()` / `end()` | Zero device I/O |
| `probe()` | One explicit read callback |
| Existing single-transfer APIs | May remain synchronous |
| Ordinary multi-transfer jobs | Existing start/poll model; caller controls callback budget |
| Composite calendar read/set | Start a typed library job; caller selects a bounded `pollJob()` instruction budget |
| Generic opt-in persistence | Retain cooperative `tick()`/`pollEeprom()` behavior; harden the protocol rather than bypassing its bounds |
| Primary-cell ensure | Sole synchronous multi-callback exception; terminal within 1000 ms with conforming callbacks |

The library owns chip phases, register knowledge, typed capabilities, and safe
protocol execution. The application owns the I2C bus, worker/task if any,
queue admission, scheduling, physical bus recovery, retry policy, persistence
policy, battery-chemistry policy, and result publication.

## Product-neutral library rule

- `begin()` and `recover()` never infer battery chemistry, desired backup mode,
  or persistence policy and never inspect or modify EEPROM implicitly.
- Every active-only change, persistent change, persistent inspection, and
  semantic battery-policy helper is an explicit caller action.
- `enableEepromWrites=false` remains a fully supported normal configuration,
  not a degraded mode. Active-only APIs still work and report that the change
  is volatile.
- Persistent inspection is cheap in EEPROM wear but not side-effect-free: safe
  direct access temporarily changes volatile EERD/BSM state. Therefore even a
  read must be explicit, bounded, mutually exclusive, and cleanup-verified.
- `ensurePrimaryCellConfiguration()` is a convenience policy operation, not a
  lifecycle requirement. It is available to every caller but is never invoked
  by the library itself.
- TunnelMonitor-specific timing, one-instruction polling, automatic startup
  invocation, and disabled-generic-persistence choices are compatibility tests,
  not defaults imposed on other applications.
- Generality means typed coverage of documented RV3032 functionality. It does
  not mean a framework, hidden task, unbounded queue, or unsafe generic EECMD
  escape hatch.

## Mandatory working method

1. Read repository `AGENTS.md` completely.
2. Run `git status --short`; preserve all user changes and do not reset or
   reformat unrelated work.
3. Read this entire prompt before editing source.
4. Read and visually inspect the local vendor PDFs cited below.
5. Audit current public headers, source, native tests, example, tools, and
   maintained documentation.
6. Use subagents for parallel read-only audits covering at least:
   - vendor register/protocol facts;
   - complete vendor capability-to-public-API coverage, including functionality
     not consumed by TunnelMonitor;
   - lifecycle, transport, health, and public API compatibility;
   - `pollJob()` and all current multi-transfer APIs;
   - `pollEeprom()` protocol and fake fidelity;
   - product-neutral API/persistence behavior and TunnelMonitor adapter fit as
     one compatibility profile;
   - examples, HIL, documentation, and versioning.
7. Integrate the findings yourself. Do not delegate final design judgment,
   edits, or verification claims.
8. Create a requirement-to-evidence matrix in a dated implementation report
   before source edits.
9. Implement a complete first pass and run focused tests.
10. Re-read this prompt fully, close omissions, and run the full matrix.
11. Inspect the complete diff, then read this prompt a third time and audit
    every acceptance criterion against actual code/tests.
12. Do not commit, tag, publish, upload, flash, issue EEPROM commands, or edit
    the sibling TunnelMonitor repository without separate authorization.

If this prompt conflicts with the vendor manual, stop and report the exact page,
table, and conflict. Do not guess.

## Authority and required reading

Use this order:

1. Repository `AGENTS.md`.
2. Local Micro Crystal documents:
   - `docs/reference-pdfs/RV-3032-C7_App-Manual.pdf`, Rev. 1.3, May 2023;
   - `docs/reference-pdfs/RV-3032-C7_datasheet.pdf`.
3. This prompt.
4. Maintained code/docs as current-implementation evidence only.

Visually inspect at least application-manual pages 13-16, 21, 23-30, 43-51,
65-72, 130, 139, and 141. Confirm Control registers, Status semantics,
configuration mirrors, direct EEPROM access, `EEbusy`, `EEF`, minimum waits,
voltage, endurance, and primary-cell application guidance.

For integration fit, inspect these sibling files read-only:

- `../TunnelMonitor-node/AGENTS.md`
- `../TunnelMonitor-node/docs/guidelines/decisions.md`
- `../TunnelMonitor-node/docs/guidelines/rtc_fram.md`
- `../TunnelMonitor-node/docs/guidelines/i2c_peripherals.md`
- `../TunnelMonitor-node/docs/guidelines/dependency_policy.md`
- `../TunnelMonitor-node/docs/guidelines/ownership.md`
- `../TunnelMonitor-node/docs/guidelines/interfaces.md`
- `../TunnelMonitor-node/docs/guidelines/validation_plan.md`
- `../TunnelMonitor-node/docs/guidelines/implementation_plan.md`
- `../TunnelMonitor-node/docs/guidelines/open_questions.md`
- `../TunnelMonitor-node/src/i2c/I2cTask.cpp`
- `../TunnelMonitor-node/src/i2c/I2cDiagnostics.cpp`
- `../TunnelMonitor-node/include/TunnelMonitor/i2c/I2cTask.h`
- `../TunnelMonitor-node/test/native/test_i2c_task_fake.cpp`

TunnelMonitor prompts/reports do not override its guidelines. Treat the
following as one later integration profile, not as general library defaults:

- normal calendar/status operations remain owner-poll stepped;
- the library owns those chip-specific job phases;
- TunnelMonitor advances at most one normal library callback per owner poll;
- only primary-cell ensure may execute a full synchronous sequence;
- TunnelMonitor, not the library, will explicitly call ensure once per
  I2C-owner startup;
- that application call reads persistent C0 directly every startup;
- correct C0 causes zero `0x21`; wrong C0 permits at most one;
- no periodic, recovery, CLI, web, settings, `ReadRtc`, or `SetRtc` call starts
  provisioning;
- no RV3032 register, mask, BCD codec, or protocol-phase enum remains in
  `I2cTask` after later integration.

The existing TunnelMonitor Prompt 43 predates this exact hybrid decision. Do not
use its external-only provisioning or chunked-provisioning requirements as
authority.

## Audited current baseline to verify

Verify and record the exact current baseline before editing:

- Current version is `1.6.0`.
- Native tests currently report 56 tests.
- `esp32s3dev` currently builds.
- `pollJob()` already provides fixed state, fixed storage, caller instruction
  budget, and useful one-step-per-poll scaffolding.
- `tick()`/`pollEeprom()` and the eight-entry queue provide bounded cooperative
  scheduling scaffolding.
- The EEPROM protocol inside `processEeprom()` is incomplete: it lacks verified
  safe BSM/TCM access state, EEADDR/EEDATA staging readback, the required 10 ms
  pre-poll wait, stale-EEF separation, ambiguous-write reconciliation, direct
  persistent verification, and fully verified cleanup.
- Current `processEeprom()` writes command value `0x21`; it does not issue
  update-all `0x11`. The current name `EEPROM_CMD_UPDATE` is misleading and must
  become WRITE_ONE.
- `begin()` probes and calls `_applyConfig()`, so lifecycle binding can change
  active C0 and may queue persistence.
- `recover()` probes and reapplies configuration.
- `setPrimaryBatteryBackupDefaults()` clears C0 low nibble `0x0F`, which also
  changes TCR.
- The flat native fake does not model separate active mirrors and persistent
  EEPROM, EECMD execution, busy timing, EEF, ambiguity, or POR refresh.
- The example enables persistence and applies primary-cell defaults during
  startup.
- The example user-EEPROM helper reads EEDATA too soon after `0x22`.
- README/Config incorrectly claim about 100,000 EEPROM writes.

Also audit and correct at least these known register/API problems:

- fictitious Control 1 bit 7 `TRPT`;
- incorrect Control 2 definitions; AIE is bit 3 and EIE is bit 2;
- Control 3 incorrectly described as reserved;
- fictitious EVI bit 3 `EVI_EN`;
- incorrect `PMU_TRICKLE_MASK = 0x0F`;
- misleading `EEPROM_CMD_UPDATE = 0x21`; it is write-one;
- direct register access incorrectly accepts indirect user EEPROM `0xCB..0xEA`;
- timer high register `0x0C` uses only bits 3:0;
- incorrect TEMP_LSB description;
- CLKOUT getters/setters ignore C3.OS high-frequency mode;
- offset documentation overstates the signed six-bit range.

This list is a minimum. Compare the complete register/command table and every
vendor-documented functional block against the manual. Do not omit a documented
chip capability merely because TunnelMonitor does not use it. Add or correct
small typed APIs in the existing driver for missing functionality; do not create
unrelated frameworks, speculative application services, or unsafe raw command
surfaces.

## Vendor facts that must remain true

### Memory model

- Direct `0xC0..0xCA` are active configuration RAM mirrors.
- Separate configuration EEPROM stores persistent `0xC0..0xCA`.
- User EEPROM `0xCB..0xEA` is persistent and indirect through
  EEADDR/EEDATA/EECMD; it is not a direct register window.
- Power-on refresh, typically about 66 ms, copies configuration EEPROM to active
  mirrors.
- An automatic refresh copies EEPROM to active mirrors at date increment about
  once per 24 hours in VDD state when EERD=0.
- Writing an active mirror changes live behavior only. It does not automatically
  commit to EEPROM.
- Calendar timekeeping is maintained from VBACKUP and does not wear EEPROM.
- The chip contains no FRAM.

### EEPROM controls

- Control 1 is register `0x10`.
- EERD is Control 1 bit 2, mask `0x04`.
- EERD=1 disables automatic EEPROM-to-active refresh during explicit access; it
  does not enable writes or commit anything.
- EEADDR=`0x3D`, EEDATA=`0x3E`, EECMD=`0x3F`.
- EECMD is write-only and reads zero.
- Commands are exactly:
  - update all active mirrors to EEPROM: `0x11`;
  - refresh all EEPROM values to active mirrors: `0x12`;
  - write one staged byte to EEADDR: `0x21`;
  - read one EEPROM byte at EEADDR into EEDATA: `0x22`.
- Primary-cell ensure uses only `0x21` and `0x22`; never `0x11` or `0x12`.
- EEbusy is TEMP_LSB bit 2, mask `0x04`; a command presented while busy is
  ignored.
- EEF is TEMP_LSB bit 3, mask `0x08`; it is sticky until cleared and indicates
  EEPROM write failure such as low VDD. It does not prove read success or byte
  equality.
- CLKF/BSF are TEMP_LSB bits 1/0 and use write-zero-to-clear semantics; writing
  one preserves them.
- I2C ACK proves only transport acceptance. Durable success requires direct
  persistent readback.

### Status register

- Status is register `0x0D`.
- PORF is bit 1 and VLF is bit 0.
- UF/TF/AF/EVF and PORF/VLF are write-zero-to-clear.
- Every Status write unavoidably clears THF/TLF regardless of the written value.
- A verified calendar set may clear PORF/VLF only through an API whose name
  states that behavior and whose report records THF/TLF observed immediately
  before the Status write.

### Timing, voltage, and endurance

- Read-one typical completion is about 1.1 ms. Do not check completion before
  1 ms; then poll EEbusy until clear.
- Write-one minimum/typical/maximum are about 1.2/4.8/9 ms. Wait at least 10 ms
  before the first completion check.
- EEPROM access requires stable VDD. Write-one requires at least 1.6 V.
- TunnelMonitor's fixed 400 kHz I2C requires RTC VDD at least 2.0 V.
- Enabling level switching requires VDD safely above the maximum 2.2 V LSM
  threshold through activation and the complete 10 ms settle.
- TunnelMonitor expects a 3.3 V main rail but must prove the rail and backfeed
  assumptions by board evidence/HIL. The library cannot measure VDD quality.
- Guaranteed EEPROM endurance is 10,000 cycles at 3.0 V/25 C and 100 cycles at
  5.5 V/85 C. Never claim 100,000 cycles.
- No EEPROM wait may busy-spin or be unbounded.

### Primary-cell C0 policy

C0 fields:

- bit 7 unimplemented; write zero;
- bit 6 NCLKE; preserve;
- bits 5:4 BSM: `00` disabled, `01` direct, `10` level, `11` disabled;
- bits 3:2 TCR; preserve;
- bits 1:0 TCM: `00` disables trickle charging.

For a non-rechargeable cell, derive the target exactly:

```cpp
target = static_cast<uint8_t>((persistentC0 & 0x4C) | 0x20);
```

This preserves NCLKE/TCR, clears reserved bit 7 and TCM, and selects level
switching.

During direct EEPROM access derive the safe live value exactly:

```cpp
safeActive = static_cast<uint8_t>(activeC0 & 0x4C);
```

This selects BSM `00`, TCM `00`, preserves NCLKE/TCR, and clears bit 7.

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
  while EEPROM state/queue is active. A persistence-producing setter returns
  BUSY before active mutation while a job is active.
- Ensure returns BUSY with zero I2C and leaves both machines untouched if any
  job, EEPROM operation, or queued EEPROM item exists.
- `tick()` advances only generic EEPROM persistence. It never advances a normal
  job, calls ensure, probes, or recovers.
- No lifecycle/recovery/periodic method starts provisioning.

Health may remain, but OFFLINE is observational for a resource-owner
integration: it must not silently suppress a valid explicitly requested
operation. Preserve exact typed errors and counters; local validation errors do
not count as transport failures.

## General chunked composite calendar jobs

### Public types and APIs

Add:

```cpp
struct TimeSnapshot {
  DateTime time{};
  uint8_t statusRaw = 0;
  bool statusValid = false;
  bool timeValid = false;
};

struct VerifiedTimeSetReport {
  DateTime requested{};
  DateTime verified{};
  Status calendarWriteStatus = Status::Ok();
  Status statusWriteStatus = Status::Ok();
  uint8_t statusBefore = 0;
  uint8_t statusBeforeClear = 0;
  uint8_t statusAfter = 0;
  bool calendarWriteAttempted = false;
  bool statusWriteAttempted = false;
  bool calendarWriteAmbiguous = false;
  bool statusWriteAmbiguous = false;
  bool verifiedAfterAmbiguousWrite = false;
  bool verifiedValid = false;
  bool statusBeforeValid = false;
  bool statusBeforeClearValid = false;
  bool statusAfterValid = false;
  bool temperatureHighWasSetBeforeClear = false;
  bool temperatureLowWasSetBeforeClear = false;
};

Status startReadTimeSnapshotJob(
    uint32_t nowMs,
    uint32_t operationTimeoutMs = READ_TIME_OPERATION_TIMEOUT_MS);

Status getReadTimeSnapshotJobResult(TimeSnapshot& out) const;

Status startSetTimeAndClearInvalidFlagsVerifiedJob(
    const DateTime& value,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = SET_TIME_OPERATION_TIMEOUT_MS);

Status getSetTimeAndClearInvalidFlagsVerifiedJobResult(
    VerifiedTimeSetReport& out) const;
```

Constants:

```cpp
READ_TIME_OPERATION_TIMEOUT_MS = 100;
SET_TIME_OPERATION_TIMEOUT_MS = 250;
MIN_SET_TIME_OPERATION_BUDGET_MS = 125;
```

Extend the existing private `JobState`/`JobOp`; do not add another queue or
operation object. Store a fixed last-completed job kind/status and fixed result
storage.

Start semantics:

- require begin and no active/pending EEPROM work;
- validate all input and timeout locally;
- perform zero I2C;
- reject a second active job with BUSY;
- store a wrap-safe 32-bit deadline from the supplied `nowMs`;
- reject timeout zero or over 1000 ms;
- verified set rejects `operationTimeoutMs <= 125` with zero I2C;
- verified set stores one mutation cutoff at
  `nowMs + (operationTimeoutMs - 125)` and starts no calendar or Status write at
  or after that cutoff; the final 125 ms remain available for ambiguous-write
  readback and terminal result formation;
- normalize weekday from year/month/day rather than trusting the caller field;
- clear prior typed-result availability only after successful admission.

Result getter semantics:

- while the matching job is active, return IN_PROGRESS and leave output
  unchanged;
- before a matching job or after a different job, return
  JOB_RESULT_UNAVAILABLE and leave output unchanged;
- after matching completion, copy the fixed result/report and return the exact
  terminal job status;
- a verified-set failure after mutation still returns its complete report;
- getters perform zero I2C and do not consume the result.

Append, without renumbering current errors:

```cpp
EEPROM_VERIFY_FAILED = 18,
EEPROM_CLEANUP_FAILED = 19,
PRIMARY_CELL_ALREADY_ATTEMPTED = 20,
JOB_RESULT_UNAVAILABLE = 21,
```

Keep existing `Status::Ok()` and `Status::Error()` factories. Do not rename all
status call sites as unrelated churn.

### Chunked status-first read

The read job performs no more than one callback for each `pollJob(..., 1, ...)`:

1. Read Status `0x0D`.
2. If PORF or VLF is set, finish OK with `statusValid=true`,
   `timeValid=false`, and no calendar callback.
3. Otherwise burst-read `0x01..0x07` in the next eligible poll.
4. Reject nonzero reserved bits exactly: seconds/minutes bit 7, hours bits 7:6,
   weekday bits 7:3, date bits 7:6, and month bits 7:5.
5. Strictly validate BCD/date/ranges, weekday `0..6`, and weekday equality with
   the decoded date.
6. Finish with `timeValid=true` only on complete proof.

Deadline expiry before a callback finishes TIMEOUT with no callback. Output is
available through the typed result getter only after terminal completion.

### Chunked verified set and invalid-flag clear

The set job uses one callback per budgeted instruction and never blindly
retries a write:

1. Read Status before mutation.
2. Write all seven normalized calendar bytes once.
3. Treat any calendar-write callback error as may-have-committed. Do not resend.
4. Read calendar back on a later poll when the operation deadline permits.
5. Accept normalized requested time or exactly one second later, including
   normal date/month/year rollover. Reject earlier or more than one second
   later. At `2099-12-31 23:59:59`, do not interpret two-digit-year wrap to
   2000 as 2100.
6. Read Status again immediately before clearing invalid flags.
7. Build the Status write from that fresh byte: write ones for current
   UF/TF/AF/EVF, zeros for PORF/VLF, and explicitly record current THF/TLF that
   will be unavoidably cleared.
8. Write Status once. A callback error is ambiguous; never resend it.
9. Read final Status and calendar in later polls.
10. Success requires PORF/VLF clear and final calendar in the accepted window,
    never backward from the first verified readback.

The `calendarWriteStatus`/`statusWriteStatus` defaults are storage initialization
only and are meaningful only when their matching attempted flag is true. Retain
the exact callback error/detail after readback proves an ambiguous write.

The library owns every chip-specific phase. An application owns only generic
admission, scheduling, deadline selection, and result mapping. A caller may
poll with any bounded instruction budget; the TunnelMonitor compatibility
profile calls `pollJob(..., 1, ...)` once per owner poll.

### Existing APIs

Keep and correct existing `readTime`, `setTime`, `readUnix`, `setUnix`, alarm,
timer, timestamp, temperature, offset, CLKOUT, event-input, status, RAM, raw
bounded register, persistence, and health APIs unless the vendor manual proves
that an API cannot be implemented as documented. Do not remove or narrow a chip
feature merely because TunnelMonitor uses only the composite calendar jobs.

Create a vendor-capability matrix before source edits. It must cover every
documented application-facing functional block and identify its typed public
API, execution form, persistence behavior, validation, and tests:

| Functional block | Required public coverage |
| --- | --- |
| Calendar/time and Unix conversion | Existing active calendar helpers plus the new status-first and verified-set jobs |
| Alarm and alarm interrupt | Typed set/get/match/enable/flag/clear operations |
| Periodic countdown timer and timer interrupt | Typed set/get/enable/flag/clear operations and retained chunked timer job |
| Periodic time-update interrupt | Typed frequency/enable/flag/clear operations using vendor enum values |
| External-event input and timestamping | Typed edge/debounce/overwrite/enable, timestamp read/reset, event flag/interrupt control |
| Low/high/event timestamps | Typed source enum, strict decoding, reset and count/status evidence |
| Temperature and temperature events | Typed temperature read, threshold/reference configuration, interrupt enable, flag inspection/clear |
| CLKOUT | Typed enable, complete frequency/mode set/get including Control 3 OS behavior |
| Frequency offset/calibration | Typed signed set/get with exact vendor range and quantization |
| Backup switching and trickle charging | Independent typed BSM, TCM, and TCR set/get; active-only and persistent behavior stated explicitly |
| Status, reset and validity | Typed read/decode, explicit side-effect-aware flag clearing, and documented reset behavior |
| User RAM | Existing bounded read/write and chunked write coverage |
| Configuration EEPROM | Explicit persistent inspection plus typed persistent updates; no lifecycle invocation |
| User EEPROM | Bounded chunked read/write APIs over offsets `0..31`, with wear and verification semantics |
| EEPROM password/protection | Typed set/enable/unlock/status flow only as documented; secrets never enter logs/status and no password-guessing API |
| Diagnostics/health | Passive observational state that never becomes a second bus-policy owner |

Where current coverage is missing, add the smallest typed method/enum/result in
the existing `RV3032` class and existing fixed job engine. Use permanent chip-
domain names. Do not create a feature manager, plugin system, service registry,
application task, or second scheduler. Do not use unrestricted raw writes as a
substitute for a typed API with known side effects.

Document `readTime`/`setTime` accurately as single-transfer calendar helpers:
they do not perform the status-first/verified/flag-clearing composite operation.
Applications that need those stronger semantics use the typed chunked jobs;
TunnelMonitor is one such application.

Retain existing chunked timer and user-RAM jobs. Remove or strictly allowlist
the public generic `startRegisterUpdateJob()` so callers cannot RMW Status,
Control, EEPROM-command, password, or reserved registers through an unsafe raw
path. Audit every existing multi-transfer method; all newly completed chip
features use the existing cooperative engine unless they are a true single-
transfer operation. Primary-cell ensure remains the only synchronous multi-
callback exception.

## Cooperative generic EEPROM persistence

Persistent access is an explicit general-library capability, not an automatic
startup check. Add product-neutral typed inspection/read surfaces using the
existing fixed job engine:

```cpp
enum class ConfigurationEepromRegister : uint8_t {
  PMU = 0xC0,
  OFFSET = 0xC1,
  CLKOUT1 = 0xC2,
  CLKOUT2 = 0xC3,
  TEMPERATURE_REFERENCE0 = 0xC4,
  TEMPERATURE_REFERENCE1 = 0xC5,
};

constexpr uint8_t USER_EEPROM_SIZE = 32;
constexpr uint8_t USER_EEPROM_JOB_MAX_BYTES = 16;

struct PersistentReadResult {
  uint8_t eepromAddress = 0;
  uint8_t data[USER_EEPROM_JOB_MAX_BYTES] = {};
  uint8_t length = 0;
  bool persistentVerified = false;
  bool cleanupVerified = false;
};

struct UserEepromWriteReport {
  uint8_t offset = 0;
  uint8_t requestedLength = 0;
  uint8_t completedBytes = 0;
  uint8_t durablyVerifiedBytes = 0;
  bool cleanupVerified = false;
};

Status startReadConfigurationEepromJob(
    ConfigurationEepromRegister reg,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = 250);

Status startReadUserEepromJob(
    uint8_t offset,
    uint8_t length,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = 1000);

Status startWriteUserEepromJob(
    uint8_t offset,
    const uint8_t* data,
    uint8_t length,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = 4000);

Status getPersistentReadJobResult(PersistentReadResult& out) const;
Status getUserEepromWriteJobResult(UserEepromWriteReport& out) const;
```

Use offsets `0..31` for public user-EEPROM APIs and translate internally to
`0xCB..0xEA`. Reject zero length, length above 16, or a range crossing byte 31
with zero I2C. Copy write input into fixed job storage at successful admission.
Persistent reads are permitted when `enableEepromWrites=false`; explicit user-
EEPROM writes and persistent configuration changes require it to be true.
Every persistent read uses direct read-one, positive data replacement proof,
safe access state, and verified cleanup. Every user-EEPROM byte write performs
compare-before-write, at most one `0x21` attempt for that byte, direct readback,
and bounded cleanup. A multi-byte failure stops before later bytes and reports
the verified completed-byte count. Do not expose password bytes `0xC6..0xCA`
through the configuration-inspection enum; their vendor protection flow uses
dedicated typed APIs and redacted evidence.

Keep these bounded public surfaces and their fixed queue:

```cpp
void tick(uint32_t nowMs);
Status pollEeprom(uint32_t nowMs,
                  uint8_t maxInstructions,
                  uint8_t& instructionsUsed);
bool isEepromBusy() const;
Status getEepromStatus() const;
uint32_t eepromWriteCount() const;
uint32_t eepromWriteFailures() const;
uint8_t eepromQueueDepth() const;
```

`enableEepromWrites=false` is a normal application choice. It disables generic
persistent mutation while preserving active-only control and explicit
persistent reads. Calling primary-cell ensure is separate explicit authority
for that method's possible single C0 write; it does not depend on or change the
generic persistence setting and never uses this queue. TunnelMonitor's later
integration will keep generic persistence disabled, but other applications may
enable it. Retaining the API does not permit retaining the current unsafe
protocol or making false durability claims.

Refactor each queued item to mean: ensure the selected supported configuration
EEPROM byte equals the desired active value. It remains chunked and must:

1. Serialize against `_job` and ensure.
2. Reserve queue capacity before changing the active mirror; queue-full returns
   before mutation.
3. Save and verify Control 1/EERD and active C0.
4. Establish EEbusy clear with bounded checks.
5. Disable and verify active BSM=`00`, TCM=`00` while preserving NCLKE/TCR.
6. Direct-read the selected persistent byte before deciding to write.
7. If already equal, perform zero `0x21` and clean up.
8. Clear/check stale EEF, verify EEADDR/EEDATA staging, issue `0x21` once, wait
   at least 10 ms through poll timestamps, then check busy/EEF.
9. Direct-read persistent state and require exact equality.
10. Restore and verify the intended active mirror, active C0 access state, and
    exact saved Control 1 EERD state. Honor required BSM settle time.
11. Preserve primary failure and distinguish cleanup failure.

For arbitrary persistent bytes, no single impossible EEDATA sentinel exists.
Use a vendor-correct adaptive two-read proof: verify EEADDR, preload a first
sentinel, issue `0x22`, wait/poll, capture R1; preload a second value chosen not
equal to R1, issue a second `0x22`, wait/poll, and require R2==R1 and R2 different
from the second preload. Never infer persistence from the active mirror.

Every state consumes at most one callback instruction. Minimum 1/10 ms waits are
represented by wrap-safe not-before timestamps; a poll before the timestamp
does zero I2C and returns IN_PROGRESS. Busy polling has explicit interval,
count, phase-deadline, and whole-item bounds.

A mutating callback error is never resent. A possibly committed `0x21` proceeds
to bounded busy/EEF/direct-read reconciliation. At most one `0x21` physical
attempt is structurally possible per queued item.

Coalesce an exact duplicate pending address/value without adding a second queue
entry. Do not silently discard a different pending value. Keep the queue fixed
at eight entries and allocate no heap.

Do not issue `0x11` or `0x12` from generic persistence. Rename command constants
to `EEPROM_CMD_WRITE_ONE`, `EEPROM_CMD_READ_ONE`,
`EEPROM_CMD_UPDATE_ALL`, and `EEPROM_CMD_REFRESH_ALL` with exact values.

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

## Protocol-faithful native fake

Replace the flat fake with a native-only fixed-capacity device model that
independently models:

- normal registers;
- active C0-CA mirrors separate from persistent C0-EA EEPROM;
- POR refresh and explicit daily-refresh hook;
- configurable initial busy interval;
- EECMD write-only/read-zero;
- EEADDR/EEDATA staging;
- delayed `0x22` and `0x21`;
- commands ignored while EEbusy=1, as vendor documented;
- a sticky fake protocol violation when EECMD is presented with EERD=0 or active
  BSM outside 00/11; do not invent undocumented silicon behavior;
- optional ACKed/ignored illegal-command fault mode for negative testing only;
- 1/10 ms earliest completion;
- sticky EEF and injected low-VDD failure;
- TEMP_LSB W0C/preservation behavior;
- Status W0C plus unconditional THF/TLF clear;
- command commit with timeout/NACK/bus error and noncommit with same errors;
- ignored-but-ACKed live/staging writes;
- test-controlled time and wait requests;
- fixed transfer/wait logs with sticky overflow failure;
- callback counts and physical-attempt counts;
- no production compilation, heap, or unbounded logs.

Model device semantics independently, not the driver's expected state machine.

## Required native tests

Keep valid current coverage and rewrite only tests tied to corrected behavior.

### Lifecycle and compatibility

- valid begin uses zero I2C/wait callbacks;
- invalid/second begin leaves current state untouched and uses zero callbacks;
- end/rebegin binds new context and resets ensure latch only then;
- probe is one read callback and no write;
- recover never applies configuration or invokes ensure;
- tick/pollEeprom/pollJob remain public and bounded;
- current time/Unix/feature APIs remain unless explicitly removed above;
- settings snapshot reports retained queue/job/health fields truthfully;
- public headers contain no Arduino/Wire/FreeRTOS types.

### General capability coverage

- every capability-matrix row has positive, invalid-input, transport-failure,
  reserved-bit, and readback/round-trip coverage appropriate to that feature;
- functionality not consumed by TunnelMonitor is exercised directly through
  the public library API;
- active-only configuration works with generic persistence disabled and does
  not claim durability;
- typed BSM, TCM, and TCR operations change only their documented fields;
- periodic update, timer, alarm, event, timestamp, temperature, CLKOUT, offset,
  reset, RAM, user EEPROM, and password/protection behavior matches the vendor
  tables and side effects;
- password/protection tests use fixed dummy secrets, prove redaction, and never
  expose a password-guessing or unrestricted EECMD route;
- every new multi-transfer feature honors the existing instruction budget and
  mutual exclusion; no second synchronous multi-callback exception appears.

### Cooperative jobs

- every existing job retains its callback-budget behavior;
- `maxInstructions=0/1/N` is exact;
- status-first read uses one callback per poll and short-circuits invalid time;
- strict reserved-bit/BCD/date/weekday rejection;
- verified set start validation uses zero I2C;
- set job has exact phase/callback order and one callback per poll;
- no calendar/Status write callback is invoked twice;
- committed/noncommitted timeout/NACK/bus errors reconcile by later readback;
- accepted readback is requested or requested+1 second only;
- rollover, backward final read, +2 second, and 2099 wrap cases;
- THF/TLF evidence and UF/TF/AF/EVF behavior;
- typed result getter availability, wrong-kind, in-progress, success, and failed
  mutation reports;
- 32-bit deadline wrap and no callback after deadline;
- resource-owner `pollJob(now, 1, used)` regression proving that a caller can
  retain one-callback-per-poll scheduling.

### Generic persistence

- explicit persistent configuration inspection works while writes are
  disabled, performs zero EEPROM write commands, and proves cleanup;
- user-EEPROM reads cover offsets 0/15/16/31, 16-byte maximum chunks, and range
  rejection with zero I2C;
- user-EEPROM writes require generic persistence enabled, copy caller input,
  compare before write, verify each changed byte, skip equal bytes, report
  completed/verified counts, and stop safely on the first failed byte;
- queue capacity, FIFO, exact duplicate coalescing, and no active mutation on
  QUEUE_FULL/BUSY;
- job/EEPROM mutual exclusion;
- one callback maximum per instruction;
- minimum wait timestamps perform zero I2C before due;
- direct persistent compare avoids write when already correct;
- wrong value issues one `0x21` and verifies persistence;
- adaptive two-read persistent proof for arbitrary bytes;
- stale EEF clear, low-VDD EEF, busy timeout, staging mismatch;
- committed/noncommitted ambiguous `0x21`, never blind resend;
- safe BSM/TCM access, exact Control 1 restoration, active mirror restoration,
  and level-switch settle;
- cleanup failure precedence and exact status/counters;
- no `0x11`/`0x12` use.

### Primary-cell success and idempotence

- all NCLKE/TCR combinations preserved;
- bit 7 never accepted/written as target;
- unrelated Control 1 bits preserved;
- correct persistence: one `0x22`, zero `0x21`, ALREADY_CONFIGURED;
- stale active with correct persistence repaired, zero `0x21`;
- wrong persistence: one `0x21`, two `0x22`, EEPROM_UPDATED;
- second same-lifecycle call: zero callbacks;
- end/begin rechecks and writes zero when correct;
- BUSY precondition with job, active EEPROM item, and queued EEPROM item uses
  zero callbacks, does not consume attempt, and leaves machines unchanged;
- no calendar/C1-CA/user-memory mutation;
- safe BSM00/TCM00 before every EECMD;
- final target/C1/settle proof before success;
- compact public report matches test-only detailed evidence.

### Primary-cell timing/failure matrix

- no read completion check before 1 ms;
- no write completion check before 10 ms;
- busy polling interval >=1 ms;
- initial typical 66 ms busy succeeds;
- every phase cap/deadline/check cap terminates;
- effective callback timeout clips to 5 ms/phase/overall remainder;
- conforming callbacks finish <=1000 ms including cleanup;
- late/early wait callback behavior is detected;
- no `0x21` begins at/after 500 ms, including staging crossing cutoff;
- no EECMD when busy is set/unknown;
- no verification `0x22` after busy cannot be proved clear;
- `UINT32_MAX` wrap;
- exactly one `0x21` structurally possible.

Run table-driven fault injection at every relevant callback ordinal for correct
and write-needed paths. For mutating callbacks test failure before mutation and
committed/error-returned variants. Cover EERD mismatch, safe-active mismatch,
initial busy timeout, EEADDR/sentinel/target mismatch, stale EEF failure,
write-one ignored with ACK, low VDD, ambiguous commit, final mismatch, cleanup
busy timeout, active C0 failure, Control 1 failure, settle deadline, and overall
expiry in every major phase.

Central assertions: no false success, no second write-one, no provisioning queue
use, cleanup whenever communication safely permits, no BSM enable while busy is
known/unknown, cleanup failure precedence, and exact operation/cleanup evidence.

## Example, HIL, documentation, and versioning

Refactor `examples/01_basic_bringup_cli/main.cpp`:

- setup calls passive begin then explicit probe;
- loop continues to call `tick()` so retained generic persistence remains
  demonstrated cooperatively;
- setup never calls primary-cell ensure and never assumes battery chemistry;
- normal startup/soak is EEPROM-write-free;
- ordinary setters clearly print active-only or persistence-pending state;
- no raw EECMD command;
- optional dangerous command
  `primary-cell ensure CONFIRM-PRIMARY-CELL` may invoke ensure once and print its
  terminal report;
- confirmation is example/operator policy, not part of library API.

Do not flash, issue EEPROM commands, remove VDD, or run retention HIL without
fresh authorization naming port/module, primary-cell chemistry, power
conditions, possible C0 write, and VDD-off/backfeed scope.

If authorized, record:

- firmware/library revision and port;
- battery chemistry/voltage;
- RTC VDD >=2.0 V at 400 kHz and safely >2.2 V during LSM activation/settle;
- persistent/active C0 before/target/after;
- zero/one `0x21`, one/two `0x22`;
- next reboot already-correct zero-write path;
- isolated-backfeed main-power-loss retention, calendar advance, PORF/VLF/BSF.

Update README, architecture, device reference, IDF port notes, example help,
Doxygen, changelog, and version. State clearly:

- active mirrors versus persistent EEPROM; no FRAM;
- passive lifecycle;
- retained cooperative jobs and persistence;
- primary ensure is the only synchronous multi-callback exception;
- product-neutral explicit instruction budgets and optional persistence;
- persistent inspection is explicit and never lifecycle-triggered;
- TunnelMonitor compatibility profile: chunked calendar jobs, generic
  persistence disabled, and application-owned once-per-start ensure invocation;
- exact primary-cell target and per-lifecycle invocation guard;
- correct EEPROM endurance figures;
- ambiguous writes and durable verification;
- current code does not use update-all;
- generic example does not auto-provision;
- HIL evidence status and limitations.

Produce a dated report under `docs/reports/` containing baseline, requirement-
evidence matrix, API/behavior changes, exact checks, HIL status, intentional
omissions, and worktree state. Do not claim hardware evidence that did not run.

## Verification commands

Run after final edits:

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
git diff --check
```

There is no separate `esp32s3dev_hil` environment in the audited repository.
Do not invent one for this task. Hardware execution remains separately
authorization-gated; the parser self-test and dry run below do not touch the
device.

Package to a temporary artifact, validate package docs, then remove it:

```powershell
python -m platformio pkg pack . --output .artifacts/RV3032-C7-2.0.0.tar.gz
python tools/check_docs_contract.py package .artifacts/RV3032-C7-2.0.0.tar.gz
```

Use installed PlatformIO-equivalent syntax if needed and record it.

Run source audits such as:

```powershell
rg -n "_applyConfig|setPrimaryBatteryBackupDefaults|PMU_TRICKLE_MASK|EEPROM_CMD_UPDATE" include src examples test README.md docs
rg -n "EEPROM_CMD_UPDATE_ALL|EEPROM_CMD_REFRESH_ALL|REG_EE_COMMAND|EECMD" include src test examples
rg -n "100k|100,000|all-synchronous|one bounded synchronous library call" README.md docs include examples tools
```

Inspect expected matches manually. `tick`, `pollEeprom`, `pollJob`, queue, and
job symbols are expected and must not be treated as stale merely because the old
all-synchronous prompt removed them. Update-all/refresh-all may remain in the
complete command table and forbidden-use tests only.

## Final self-audit

Before completion:

1. Re-read this prompt fully.
2. Close every requirement/evidence row.
3. Prove normal calendar operations are chunked and ensure alone is synchronous.
4. Trace every current public multi-transfer API and record whether it is a
   single callback, existing job, retained compatibility path, or corrected
   scope; do not accidentally broaden synchronous use.
5. Close every vendor-capability-matrix row with a typed API, documentation,
   and tests; verify that TunnelMonitor usage did not define the library scope.
6. Trace generic persistence success/failure and its exact one-instruction poll
   behavior.
7. Trace primary ensure straight-line success/failure and cleanup.
8. Prove one ensure cannot issue two physical `0x21` attempts.
9. Prove one begin lifecycle cannot run ensure twice.
10. Prove the 500 ms cutoff leaves cleanup inside 1000 ms.
11. Compare every touched register/mask/command with vendor tables.
12. Review fake semantics independently from driver implementation.
13. Inspect diff for unrelated edits, generated debris, heap use, stale docs,
    accidental TunnelMonitor edits, or hidden write retry.
14. Re-run all verification after the final edit.
15. Read the prompt a third time and update final evidence.

## Acceptance criteria

Complete only when:

- begin/end are zero-I/O and probe is explicit;
- existing cooperative `tick`/`pollEeprom`/`pollJob` behavior is retained and
  bounded;
- normal composite read/set jobs perform at most one callback when polled with
  budget one and may consume a larger caller-supplied bounded budget;
- library owns all RV3032 calendar/status job phases and results;
- existing supported APIs are not removed without a documented vendor/safety
  reason, and every documented application-facing functional block in the
  capability matrix has typed public coverage and tests;
- generic persistence is optional, disabled by default, vendor-correct,
  directly verified, chunked, and independent of primary-cell ensure;
- persistent inspection is explicit, works while generic writes are disabled,
  and is never invoked by begin/recover/tick;
- no lifecycle/recovery path implicitly changes PMU or starts persistence;
- ensure is explicit, synchronous, queue-independent, callable at most once per
  begin lifecycle, and terminal only; begin does not call it;
- ensure returns BUSY with zero I/O when cooperative work is pending;
- correct C0 causes zero `0x21`; wrong C0 causes at most one;
- direct persistent read, busy, EEF, ambiguous reconciliation, cleanup, and LSM
  settle gate success;
- whole ensure is <=1000 ms for conforming callbacks, transfer cap is 5 ms, and
  no `0x21` starts at/after 500 ms;
- target is exactly `(persistentC0 & 0x4C) | 0x20`;
- provisioning never uses `0x11`, `0x12`, generic queue, or blind retry;
- untrusted communicable failure holds verified BSM00/TCM00 plus EERD1 when
  possible and never reports success;
- fake separates active/persistent state, models timing/fault semantics, and
  counts callbacks/physical attempts;
- callback-ordinal faults create no false success or second write;
- register/API audit corrections are complete;
- generic example startup/normal soak is EEPROM-write-free;
- native tests, both firmware builds, guards, parser/dry-run, package validation,
  and diff check pass;
- physical HIL is exact current evidence or clearly NOT RUN;
- report contains final evidence, not planned claims;
- no commit, tag, publication, flash, EEPROM command, or TunnelMonitor edit was
  performed without authorization.

## Final response

Report concisely:

- files/APIs changed;
- preserved cooperative behavior;
- general chunked composite jobs, persistent inspection, optional persistence,
  and the explicit synchronous provisioning boundary;
- complete vendor capability matrix and typed public coverage;
- TunnelMonitor compatibility results as one integration profile;
- corrected unsafe/incorrect behavior;
- exact test/build/guard/package results;
- HIL status;
- intentional omissions/open issues;
- working-tree state;
- no commit/tag/publication performed.
