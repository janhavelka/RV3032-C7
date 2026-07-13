# AI Coder Prompt 04.2: Composite Calendar Jobs And Capability Coverage

## Phase contract

This is phase 2 of 5 in Prompt Suite 04.

- Required first: read `04-00-shared-contract.md` completely.
- Prerequisites: phase 1 is complete or its architecture and lifecycle exit criteria have been re-audited.
- Current scope: Implement the status-first and verified-set calendar jobs, audit every vendor capability row, and add the smallest missing typed APIs without creating another scheduler.
- Exit criteria: calendar jobs obey exact callback budgets and result contracts, every capability row has an explicit owner/API/execution form, and focused tests pass.
- Preserve unrelated work and keep the shared dated requirement/evidence report
  current. Do not commit, tag, publish, flash, or run hardware EEPROM work.

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

