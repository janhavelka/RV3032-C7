# AI Coder Prompt 04.5: Native Fake, Verification, Documentation, And Release Evidence

## Phase contract

This is phase 5 of 5 in Prompt Suite 04.

- Required first: read `04-00-shared-contract.md` completely.
- Prerequisites: phases 1 through 4 are complete; do not use this phase to paper over an unresolved implementation defect.
- Current scope: Build the independent protocol fake and complete the native matrix, example/HIL safety policy, maintained documentation, versioning, package audit, final self-audit, and response.
- Exit criteria: all software checks and package validation pass, hardware evidence is stated truthfully, generated artifacts are removed, and the report contains final rather than planned claims.
- Preserve unrelated work and keep the shared dated requirement/evidence report
  current. Do not commit, tag, publish, flash, or run hardware EEPROM work.

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

1. Re-read the shared contract and all five phase prompts fully.
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
15. Read the complete prompt suite a third time and update final evidence.

## Acceptance criteria

Complete the full suite only when every preceding phase and the following criteria are satisfied:

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

