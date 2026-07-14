# AI Coder Prompt 05: TunnelMonitor Integration Readiness

## Mission

Make the current RV3032-C7 library a clean, reviewed dependency candidate for
TunnelMonitor without moving TunnelMonitor policy or I2C ownership into the
library.

This is a focused follow-up to Prompt Suite 04. The broad 2.0.0 hardening is
already present. Preserve its working passive lifecycle, cooperative jobs,
fixed-capacity state, primary-cell protocol, fake-chip model, and complete
general-purpose feature surface. Correct only the remaining integration
contract mismatches described here, add decisive regression tests, correct the
maintained documentation, and produce truthful verification evidence.

Do not edit the sibling TunnelMonitor repository in this task. Do not integrate
or pin the dependency there. The result of this task is an RV3032 library
worktree that can later be reviewed and handed off by full commit hash.

Do not stop after the first passing test. Implement the complete prompt, inspect
the full diff, audit every acceptance criterion against the final code and
tests, and continue until all software requirements are satisfied or a precise
external blocker is proven.

## Current audited baseline

The audit that created this prompt inspected clean `main` commit
`ca06b1dd5f8dc9d3d9c358079515599b0b313082`, declared library version `2.0.0`.
Treat that hash only as baseline evidence. Record the actual HEAD and worktree
state when executing this prompt and audit the live files before editing.

At the audited baseline these checks passed:

- 79 native tests;
- ESP32-S3 and ESP32-S2 example builds;
- generated-version, core-timing, CLI, documentation-source, HIL parser,
  device-free HIL dry-run, and whitespace guards.

Green baseline tests are not proof of TunnelMonitor compatibility. Some current
tests deliberately enforce behavior that this prompt must correct.

## Mandatory working method

1. Read `AGENTS.md` completely.
2. Record `git rev-parse HEAD`, `git status --short --branch`, and
   `git describe --tags --always --dirty`. Preserve all user changes. Never
   reset or reformat unrelated work.
3. Read these library files completely where relevant:
   - `include/RV3032/RV3032.h`;
   - `include/RV3032/CommandTable.h`;
   - `include/RV3032/Config.h`;
   - `include/RV3032/Status.h`;
   - `src/RV3032.cpp`;
   - `test/test_native/FakeRv3032.h`;
   - `test/test_native/test_datetime.cpp`;
   - `README.md`, `CHANGELOG.md`, `docs/ARCHITECTURE.md`,
     `docs/DEVICE_REFERENCE.md`, and
     `docs/reports/2026-07-13-v2.0.0-implementation.md`.
4. Read Prompt Suite 04 as historical design context, especially
   `04-00-shared-contract.md`, `04-02-calendar-and-capability-coverage.md`,
   `04-04-primary-cell-and-raw-access.md`, and
   `04-05-native-fake-verification-and-release.md`. This prompt overrides only
   their weekday/date-equality and verified-set Status-payload requirements.
5. When the sibling repository exists, inspect these files read-only to confirm
   the consumer contract:
   - `../TunnelMonitor-node/AGENTS.md`;
   - `../TunnelMonitor-node/prompts/prompt_43_rv3032_owner_integration_and_primary_cell_ensure.md`;
   - `../TunnelMonitor-node/docs/guidelines/rtc_fram.md`;
   - `../TunnelMonitor-node/docs/guidelines/i2c_peripherals.md`;
   - `../TunnelMonitor-node/docs/guidelines/dependency_policy.md`;
   - `../TunnelMonitor-node/docs/guidelines/validation_plan.md`.
6. Use subagents for independent read-only audits when viable, for example one
   for calendar/Status semantics and one for primary-cell report/failure
   evidence. Keep final design decisions, edits, and verification claims with
   the primary agent.
7. Run focused tests after each logical change, then the complete verification
   matrix below.
8. Inspect the complete final diff and search for stale weekday-equality,
   `0x3C` verified-set payload, raw-only PORF/VLF, or incomplete primary-report
   claims.
9. Do not commit, tag, publish, upload, flash hardware, issue physical EEPROM
   commands, or run destructive HIL without separate explicit authorization.

If current code or vendor documentation contradicts this prompt, stop and
report the exact file/line or manual page. Do not invent a compromise or put
consumer-specific chip protocol into TunnelMonitor.

## Frozen TunnelMonitor consumer contract

The later TunnelMonitor adapter will be private to its I2C owner. It will:

- configure address `0x51`, a 5 ms per-transfer callback timeout, and generic
  EEPROM persistence disabled;
- call library `begin()` passively with zero I2C;
- advance ordinary calendar jobs through `pollJob(nowMs, 1, callbacksUsed)`;
- use 100 ms read and at most 250 ms verified-set operation budgets;
- allow bounded owner recovery plus at most one retry only inside ordinary
  read-only callbacks;
- make every mutating callback one physical attempt;
- temporarily scope every callback, including reads, to one physical attempt
  while `ensurePrimaryCellConfiguration()` runs;
- invoke primary-cell ensure exactly once during I2C-owner startup; and
- map typed library results into driver-free TunnelMonitor contracts without
  parsing RV3032 register masks or raw C0 policy bytes.

The library remains product-neutral. It must not own the bus, recovery,
scheduler, startup invocation, primary-cell chemistry decision, CLI, web,
storage, or TunnelMonitor status publication.

## Preserve the working library abilities

Do not regress or replace these current properties:

- `begin()` and idle `end()` perform zero transport/wait callbacks;
- `probe()` is explicit and `recover()` is observational read-only health work;
- injected transport callbacks are the only core I2C boundary;
- no library `Wire`, pin, task, logging, heap, or platform-delay ownership;
- ordinary multi-transfer work remains `start...Job()` plus `pollJob()` with an
  exact caller-supplied instruction budget;
- `pollJob(..., 1, ...)` invokes at most one transport callback;
- normal deadlines remain wrap-safe and no callback starts after its hard
  deadline or mutation cutoff;
- verified calendar set still uses one calendar write, readback reconciliation,
  one Status write, final Status readback, and final calendar readback;
- exact requested or requested-plus-one-second readback remains accepted,
  including valid rollover; earlier, plus two seconds, and 2099-to-2000 wrap
  remain rejected;
- THF/TLF observed immediately before the Status write remain recorded;
- the dedicated primary-cell ensure remains the only synchronous
  multi-callback operation;
- the primary target remains `(persistentC0 & 0x4C) | 0x20`;
- already-correct primary configuration uses one completed `0x22` and zero
  `0x21`; correction uses at most one `0x21` and a second completed `0x22`;
- primary ensure retains 1000 ms whole-operation, elapsed `<500 ms` write
  dispatch, 300 ms cleanup reserve, 5 ms transfer, 1 ms read-settle/poll, and
  10 ms write/level-switch settle bounds;
- ambiguous writes are reconciled by readback and never replayed;
- untrusted persistence retains verified safe active BSM `00`/TCM `00` and
  EERD when communication permits;
- reset/daily-refresh, busy/EEF, low-VDD, ignored-command, ambiguous-completion,
  and separate active/persistent behavior remain modeled in the native fake;
- the complete non-TunnelMonitor feature surface remains available and tested.

Do not create a new scheduler, queue, manager, framework, alternate RTC driver,
or TunnelMonitor-specific public API.

## Required remediation 1: user-assigned weekday semantics

### Problem

RV3032 weekday values are user-assigned. Any BCD weekday in the documented
range `0..6` is a valid chip value even when it does not equal a weekday
computed from year/month/day.

The audited implementation calls `decodeCalendar(..., true)` from calendar
reads and verified readback, so it rejects a valid stored calendar solely for a
weekday/date mismatch. It also ignores the caller's weekday during `setTime()`
and `startSetTimeAndClearInvalidFlagsVerifiedJob()` and silently recomputes it.
The current native test even classifies an in-range weekday mismatch as an
invalid calendar.

### Required implementation

Use range-only weekday validation everywhere in the library:

- accept every integer weekday `0..6`;
- reject `7`, non-BCD values, and nonzero reserved weekday bits;
- do not require weekday/date agreement on `readTime()`,
  `startReadTimeSnapshotJob()`, either verified-set calendar readback, or any
  equivalent calendar decode path;
- validate the caller's weekday before any I2C in `setTime()` and
  `startSetTimeAndClearInvalidFlagsVerifiedJob()`;
- write the caller-provided weekday instead of replacing it with
  `computeWeekday()`;
- keep `computeWeekday()` as a public utility for callers that want that
  policy; do not impose it during encode/decode.

Prefer simplifying the private decoder to:

```cpp
static bool decodeCalendar(const uint8_t* data, DateTime& out);
```

Remove `requireWeekdayMatch` if it has no remaining valid caller. Do not add a
new configuration option or policy abstraction unless a real retained caller
requires date agreement.

`acceptedVerifiedTime()` may continue comparing instants without comparing
weekday, but its decoded inputs must still have weekday in `0..6`.

### Required tests

Add table-driven tests that prove:

- one fixed date with each weekday `0,1,2,3,4,5,6` succeeds through
  `readTime()` and the status-first snapshot job;
- the same range succeeds in verified-set admission and the exact supplied
  weekday is present in the calendar write payload;
- weekday `7`, reserved bits, and invalid BCD fail before mutation where
  applicable;
- verified readback accepts an in-range weekday different from the computed
  date weekday;
- all existing reserved-bit, Gregorian-date, rollover, backward, plus-two, and
  2099-wrap tests continue to pass.

Delete or rewrite the current test case that expects an in-range weekday/date
mismatch to fail.

## Required remediation 2: typed PORF/VLF snapshot evidence

### Problem

`TimeSnapshot` currently exposes a raw Status byte plus `statusValid` and
`timeValid`. The cooperative job internally detects PORF/VLF, but its result
does not expose those reasons as typed fields. A TunnelMonitor adapter would
have to decode raw `statusRaw`, which is forbidden because register-mask and
chip-status interpretation must remain inside the library.

The separate synchronous `readStatusFlags()` API does not solve this problem:
calling it would add another operation, lose the status/calendar snapshot
relationship, and violate the intended two-step job.

### Required public result

Extend the existing result rather than adding another job:

```cpp
struct TimeSnapshot {
  DateTime time{};
  StatusFlags statusFlags{};
  uint8_t statusRaw = 0;
  bool statusValid = false;
  bool timeValid = false;
};
```

The exact member name may follow an established nearby convention, but the
result must expose typed `powerOnReset` and `voltageLow` values obtained from
the same first Status callback. Keep `statusRaw` for diagnostics and backward
compatibility; the typed result is authoritative for decisions.

Create or reuse one small internal Status decoder so `readStatusFlags()` and
the snapshot job cannot drift. A suitable private helper is:

```cpp
static StatusFlags decodeStatusFlags(uint8_t raw);
```

It performs no I2C and has no side effects.

The snapshot sequence remains exact:

1. read Status once;
2. populate `statusRaw`, `statusFlags`, and `statusValid`;
3. when typed PORF or VLF is set, finish `OK` with `timeValid=false` and no
   calendar callback;
4. otherwise read/decode the calendar in the next eligible poll.

### Required tests

Prove separately for PORF-only, VLF-only, both flags, and neither flag that:

- typed fields match the raw Status callback;
- invalid flags short-circuit before calendar I2C;
- `statusValid` and `timeValid` remain truthful on success and partial failure;
- result getters preserve output on unavailable/in-progress status;
- one-callback-per-poll behavior is unchanged.

## Required remediation 3: exact verified-set Status payload

### Problem

The verified-set job currently constructs the Status write as
`statusBeforeClear & 0x3C`. That preserves only lower flags observed as set in
the preceding read. If UF, TF, AF, or EVF becomes set between that read and the
write callback, a zero in the constructed payload can clear the new event.

TunnelMonitor accepts the unavoidable RV3032 rule that every Status write also
clears THF/TLF. It does not accept avoidable loss of UF/TF/AF/EVF.

### Required implementation

Add one clear named constant to `CommandTable.h`, for example:

```cpp
static constexpr uint8_t STATUS_CLEAR_INVALID_TIME_VALUE = 0xFC;
```

Use that constant for the sole Status write in
`startSetTimeAndClearInvalidFlagsVerifiedJob()` processing:

- bits 7:2 are written as ones;
- PORF/VLF bits 1:0 are written as zeros;
- THF/TLF are still expected to clear because silicon clears them on every
  Status write, regardless of the written ones;
- the fresh pre-clear read and
  `temperatureHighWasSetBeforeClear`/`temperatureLowWasSetBeforeClear`
  evidence remain mandatory;
- the write is attempted at most once and an error remains ambiguous until
  later readback.

Do not build the write value from the current Status byte. Do not add a raw
Status-write API.

### Required tests

Prove:

- the callback payload is exactly `0xFC` for every pre-clear Status value;
- PORF/VLF are clear after a successful write;
- THF/TLF pre-clear evidence is retained and the fake models their
  unconditional clear;
- UF/TF/AF/EVF present before the read remain set;
- each of UF/TF/AF/EVF injected after the pre-clear read but before the next
  budget-one write poll remains set;
- the calendar and Status write callback counts remain exactly one;
- committed and noncommitted Status callback errors still reconcile only by
  readback.

Rewrite the current assertion that expects a `0x3C` callback payload. A final
fake Status value of `0x3C` may still be correct when all lower-six flags began
set because THF/TLF clear in silicon and PORF/VLF are deliberately cleared;
the transport payload itself must be `0xFC`.

## Required remediation 4: semantic primary-cell verification evidence

### Problem

`PrimaryCellConfigurationReport` provides raw persistent/active bytes,
validity markers, write evidence, operation status, and cleanup status. It does
not directly state whether the semantic persistent target and semantic active
target were proven.

TunnelMonitor must publish truthful `persistentVerified`, `activeVerified`, and
`cleanupVerified` flags even when the overall result is `FAILED`. Its adapter
must not compare raw C0 bytes or infer private primary-protocol state from
failure stages.

### Required public result

Extend `PrimaryCellConfigurationReport` with explicit semantic evidence:

```cpp
bool persistentTargetVerified = false;
bool activeTargetVerified = false;
```

Use equally explicit permanent names only if the existing naming scheme makes
them clearer. Document and implement these meanings exactly:

- `persistentTargetVerified` becomes true only after a direct persistent read
  proves exact equality with the internally derived primary-cell target. This
  includes the already-correct zero-write path and the corrected path. Once
  proved, it remains true if a later active-C0, EERD-cleanup, or settle step
  fails.
- `activeTargetVerified` becomes true only after readback proves the active C0
  implemented bits equal the derived persistent target. Once proved, it
  remains true if a later EERD verification or settle step fails.
- a verified safe active BSM `00`/TCM `00` hold while persistence is untrusted
  is safety evidence, not target equality; do not report
  `activeTargetVerified=true` for that state.
- `cleanupVerified` remains true only when the required terminal cleanup for
  that path is completely proved.
- `writeCommandAttempted` remains the canonical exact `0/1` write-attempt
  evidence. TunnelMonitor can map it to its `uint8_t` count because the library
  structurally caps `0x21` at one. Do not add duplicate state that can drift
  unless the existing API is deliberately and cleanly migrated.

The raw report bytes may remain as library diagnostic evidence, but the later
TunnelMonitor adapter must be able to map all public status without inspecting
or comparing them.

Set the semantic fields at the proof points, not by reconstructing them at the
end from `outcome` alone. Preserve valid partial evidence on every failure and
ambiguous-callback path.

### Required tests

Extend the primary-cell matrix to assert the semantic fields for at least:

- already correct, zero `0x21`;
- corrected and durably verified, one `0x21`;
- failure before direct persistent proof;
- wrong persistent state with no permitted/committed write;
- persistent target proven followed by cleanup busy failure;
- active target proven followed by Control 1/EERD verification failure;
- active target proven followed by settle timeout;
- untrusted persistence with verified safe active hold and EERD retained;
- committed and noncommitted ambiguous `0x21` callback outcomes;
- callback timeout/late return and every current fault-ordinal table.

For every case, assert no false success, at most one `0x21`, exact retained
operation/cleanup status, and no loss of already-proven semantic evidence.

## No other functional expansion

This task does not authorize:

- TunnelMonitor source, dependency-pin, board, queue, task, CLI, web, or status
  changes;
- a library-owned I2C bus, recovery action, retry loop, task, lock, scheduler,
  logger, or platform wait;
- automatic primary-cell ensure from `begin()`, `probe()`, `recover()`, normal
  calendar work, or generic persistence;
- another synchronous multi-callback API;
- raw C0 policy inputs, unrestricted Status writes, raw EECMD access, update-all
  `0x11`, refresh-all `0x12`, or a blind EEPROM write retry;
- new generic frameworks or speculative compatibility modes;
- removal of tested non-TunnelMonitor chip capabilities;
- physical HIL, EEPROM mutation, flashing, power removal, release, tag, or
  publication without explicit authorization.

## Documentation and evidence

Update maintained documentation so it describes the implemented final
behavior, including:

- weekday is range-validated and user-assigned, not required to match the date;
- setters preserve a valid caller weekday;
- `TimeSnapshot` contains typed PORF/VLF evidence;
- verified set writes exact `0xFC`, preserves UF/TF/AF/EVF against the
  cooperative read/write race, and records unavoidable THF/TLF loss;
- primary reports expose persistent-target and active-target verification;
- TunnelMonitor remains only a compatibility consumer and owns startup policy.

At minimum inspect and update `README.md`, `CHANGELOG.md`,
`docs/ARCHITECTURE.md`, `docs/DEVICE_REFERENCE.md`, public Doxygen, and any
example output that prints the changed reports.

Do not silently rewrite historical test results. Add a dated additive report
under `docs/reports/` that records:

- actual starting and ending HEAD/worktree state;
- the four closed compatibility gaps;
- requirement-to-code/test evidence;
- exact commands and results;
- current software build sizes;
- physical HIL truth;
- intentional omissions and remaining external integration gates.

Correct any maintained claim that the old baseline is already a complete
TunnelMonitor handoff. Historical 1.5.0 hardware evidence is not current 2.0.0
primary-cell or retention evidence.

The audited baseline has 2.0.0 metadata but no `v2.0.0` tag. Keep version
metadata coherent with the actual repository state. Do not bump, commit, or tag
merely for this task. If execution finds that 2.0.0 was already published, stop
and report the versioning conflict before changing a released contract.

## Required verification

Run all commands from the RV3032-C7 repository root:

```text
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

Also run focused searches proving:

- no production `Wire`, `delay`, task, logging, heap, recovery, or retry was
  introduced;
- no calendar decode path still requires weekday/date equality;
- no setter silently recomputes a supplied valid weekday;
- no verified-set path constructs the Status payload from `statusBeforeClear`;
- the verified-set payload uses one named `0xFC` constant;
- snapshot consumers can obtain typed PORF/VLF without another callback;
- primary target verification can be consumed without raw C0 comparison;
- no second `0x21` path exists;
- no TunnelMonitor file was modified.

If package validation is part of the current repository gate, pack to a
temporary/generated location, run `check_docs_contract.py package` against it,
and remove only the artifact created by this task after inspection.

Do not claim physical HIL passed unless it actually ran under fresh explicit
authorization. Otherwise record the current 2.0.0 EEPROM/voltage/backfeed/
power-cycle/retention cases as `NOT RUN` and keep the software completion claim
separate.

## Final independent audit

After all checks pass:

1. Re-read this prompt from the beginning.
2. Inspect every changed line and every changed public struct.
3. Re-run the focused weekday, snapshot, Status-race, and primary-report tests.
4. Confirm existing lifecycle, callback-budget, EEPROM-command-count, deadline,
   cleanup, fake-fidelity, and complete-capability tests still pass.
5. Confirm all docs and examples agree with the code.
6. Confirm the worktree contains only intended changes and generated artifacts
   are absent.
7. Confirm no commit, tag, publication, TunnelMonitor edit, or unauthorized HIL
   occurred.

Do not leave TODOs, disabled tests, weakened assertions, compatibility shims
without callers, or claims based only on code inspection.

## Completion criteria

The task is complete only when all of the following are true:

- every weekday `0..6` is accepted without date agreement, `7` is rejected,
  and setters preserve the valid supplied weekday;
- the status-first snapshot provides typed PORF/VLF from its first callback and
  still short-circuits invalid time;
- verified set writes exactly `0xFC`, preserves UF/TF/AF/EVF across the
  read/write race, and retains THF/TLF evidence;
- primary ensure reports persistent-target and active-target proof truthfully
  on success and partial failure without requiring raw-byte interpretation;
- all prior safety, ownership, timing, fake, feature, and command-count
  invariants remain intact;
- the complete software verification matrix passes;
- maintained documentation and a dated additive report match the final code;
- physical HIL and later TunnelMonitor integration are reported honestly as
  separate work when not authorized or not yet performed.

The final response must be concise and include changed files, test/build
results, exact HIL status, intentional omissions, remaining external gates, and
the final worktree state. Do not commit unless the user explicitly asks.
