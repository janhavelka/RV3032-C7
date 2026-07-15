# AI Coder Prompt 06.3: Functional Hardening Integration, Examples, and Documentation

## Phase contract

This is phase 3 of 4 in the RV3032-C7 functional-hardening implementation.
It closes findings `M-07`, `M-08`, `M-09`, `L-01`, and `L-05` from
`docs/reports/2026-07-14-full-library-functional-audit.md`. It also applies the
authority rules in section 1, the maintained-documentation contract in section
17, and the relevant verification requirements in sections 18 and 19.

This file is a standalone execution prompt. Read the full audit report as the
authoritative cross-phase design, but do not require the user to restate any
contract listed here.

The result of this phase is a buildable, tested v3 integration surface with:

- a Wire example adapter that enforces the complete callback timeout;
- one strict CLI parser and one explicit cooperative-operation owner;
- truthful asynchronous example completion and presence semantics;
- maintained public documentation aligned with all completed hardening phases;
- version `3.0.0` generated from `library.json`; and
- no compatibility shims, stale parallel paths, or unrelated redesign.

Do not commit, tag, publish, flash hardware, issue physical EEPROM commands, or
run physical HIL. Preserve unrelated user work.

The source audit was performed against revision `0e2eb0d`, manifest version
`2.0.0`. Its unchanged baseline passed 84/84 native tests plus ESP32-S2 and
ESP32-S3 builds; physical HIL was `NOT RUN`. Those values are historical
evidence, not permission to reset the sequential phase worktree and not proof
that the live phase-3 implementation is correct.

## Authority and live-tree reconciliation

Use this authority order:

1. `AGENTS.md` is binding.
2. The local Micro Crystal RV-3032-C7 vendor PDFs define silicon behavior.
3. `docs/reports/2026-07-14-full-library-functional-audit.md` defines the v3
   correction design.
4. Maintained headers, implementation, examples, tests, and documentation must
   agree with that design.
5. Prompt Suite 04, Prompt 05, and dated v2 reports are historical wherever
   they conflict with the v3 audit.

The local source documents are
`docs/reference-pdfs/RV-3032-C7_App-Manual.pdf` Rev. 1.3 and the local
datasheet. Recheck any silicon claim touched during documentation work rather
than copying stale prose. In particular, preserve the audited Status-write
side effects, UIE event-generation rule, EEPROM ownership/timing, and backup
activation requirements from the manual pages cited in the full audit.

Before editing:

1. Read `AGENTS.md` and the full audit report.
2. Read phase prompts `06-01` and `06-02` if present, then inspect their actual
   implementation rather than trusting a completion claim.
3. Record:

   ```powershell
   git rev-parse HEAD
   git status --short --branch
   git describe --tags --always --dirty
   ```

4. Read completely, where present:

   ```text
   include/RV3032/Status.h
   include/RV3032/Config.h
   include/RV3032/RV3032.h
   include/RV3032/CommandTable.h
   src/RV3032.cpp
   examples/common/I2cTransport.h
   examples/common/CliShell.h
   examples/common/CommandHandler.h
   examples/common/HealthDiag.h
   examples/common/HealthView.h
   examples/01_basic_bringup_cli/main.cpp
   test/test_native/FakeRv3032.h
   test/test_native/test_datetime.cpp
   README.md
   docs/README.md
   docs/ARCHITECTURE.md
   docs/DEVICE_REFERENCE.md
   docs/IDF_PORT.md
   CHANGELOG.md
   tools/check_core_timing_guard.py
   tools/check_cli_contract.py
   tools/check_docs_contract.py
   tools/hil_cli_runner.py
   platformio.ini
   library.json
   scripts/generate_version.py
   ```

5. Preserve dirty user changes and identify generated artifacts before work.
   Never reset, revert, or broadly reformat unrelated files.

### Required phase-1 and phase-2 prerequisites

Reconcile the live source against these exact dependencies before phase-3
implementation:

- existing `Err` numeric values are unchanged and these values have been
  appended:

  ```cpp
  CONFIGURATION_CLEANUP_FAILED = 23,
  TRANSPORT_CONTRACT_VIOLATION = 24,
  INTERNAL_STATE_ERROR = 25
  ```

- password management APIs and workflow are deleted, while password register
  constants remain as vendor reference and raw access fails closed;
- callbacks are synchronous and normalized into the closed transport result
  domain described below;
- callback completion is bounded by the cooperative deadline helpers;
- `end()` is unconditional zero-I/O teardown and `RV3032` is noncopyable and
  nonmovable;
- the cooperative backup/configuration jobs and typed final-state reports are
  present;
- persistence exposes separate operation and cleanup evidence;
- periodic update and quiescence behavior use the corrected contracts;
- `tick(uint32_t nowMs)` returns `Status`; and
- checked conversions and internally consistent health tracking are present.

Do not conceal a missing prerequisite with example-only logic, duplicated
types, or compatibility wrappers. If a prerequisite is absent or contradicts
the authoritative audit, stop this phase and report the exact file, symbol,
and conflict. A small integration defect at a phase boundary may be corrected
in its existing owner, but phase 3 is not permission to invent an alternate
core architecture.

## Mandatory subagent workflow

The primary coder owns all final decisions, edits, verification, and claims.
Subagent output is review evidence, not authority.

Before implementation, spawn three bounded, independent, read-only subagents:

1. **Wire audit** - inspect `I2cTransport.h`, the Wire test stub, Arduino-ESP32
   3.2.0 transaction semantics, timeout restoration, short-staging release,
   callback status-domain compliance, and `initWire()` failure handling.
2. **CLI/ownership audit** - inspect all line readers, token parsing, argument
   counts, asynchronous command completion, ordinary-job/EEPROM handoff, RAM
   and timestamp output, and invalid-input callback evidence.
3. **API/docs/version audit** - inspect `isOnline()`, probe/READY semantics,
   health formatting, public Doxygen, examples, README snippets, maintained
   docs, changelog, contract tools, and generated version flow.

Give each agent a narrow file list and require concrete file/line evidence.
Agents must not edit files and must not delegate further. The primary coder
must compare their findings with the live source and this prompt before
editing.

After implementation and focused tests, re-task those same three subagents to
audit the resulting complete diff in their original partitions. Require them
to identify missing tests, stale paths, weakened assertions, contradictions,
and scope drift. Independently validate every reported issue, correct every
confirmed defect in the owning code, then rerun the affected focused and full
verification. Do not copy a speculative subagent suggestion into production.

If the execution environment genuinely cannot spawn subagents, state that as
a process blocker before editing; do not silently replace the required
independent review with a self-claim.

## Engineering method: simplify the owner, do not patch around it

- Prefer deleting obsolete readers, permissive parsers, presence aliases,
  stale output paths, and false documentation over wrapping them.
- Extend the existing transport, CLI, and cooperative-operation owners. Do not
  create a second adapter, parser, scheduler, queue, manager, framework, or
  facade.
- Do not preserve removed API through aliases, deprecated methods, feature
  flags, conditional compilation, or `NOT_SUPPORTED` stubs.
- Do not weaken validation, tests, callback caps, deadlines, or error evidence
  to make the build pass.
- Do not retry a mutating physical transaction. Do not hide hardware failure,
  leaked work, or an ambiguous write behind success.
- Keep library code free of Wire, Arduino, logging, heap ownership, tasks,
  locks, and platform delays. `examples/common/` remains example-only.
- Keep one clear application I2C owner. The example adapter does not become a
  library-owned bus manager.
- Remove newly dead code exposed by the refactor. Avoid speculative helpers.

## 1. Wire transport: enforce the complete callback bound

Implement this section in `examples/common/I2cTransport.h` and its native Wire
stub/tests. Do not move Wire into the library.

### 1.1 Closed callback status domain

Both callbacks are synchronous. Their buffers are borrowed only until the
callback returns, and every returned `Status::msg` has static storage. Every
path from `wireWrite()` and `wireWriteRead()`, including local adapter
validation, returns only one of:

```text
OK
I2C_ERROR
I2C_NACK_ADDR
I2C_NACK_DATA
I2C_TIMEOUT
I2C_BUS
```

Never return `INVALID_CONFIG`, `INVALID_PARAM`, `IN_PROGRESS`, `BUSY`, or any
driver-control status from these callbacks. Use `I2C_ERROR` for local adapter
contract faults and distinguish them with fixed callback-local detail values:

```cpp
static constexpr int32_t I2C_DETAIL_INVALID_ARGUMENT = -1;
static constexpr int32_t I2C_DETAIL_INVALID_TIMEOUT = -2;
static constexpr int32_t I2C_DETAIL_SHORT_STAGING = -3;
```

If equivalent constants already exist from an intentional prerequisite
change, keep one owner and one numeric definition; do not duplicate them.
Wire result code `1` maps to `I2C_ERROR`, because the driver already dispatched
the callback. Preserve the established mappings for address NACK, data NACK,
bus failure, and timeout.

### 1.2 Required RAII timeout owner

Add exactly this fixed, noncopyable helper:

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

After ordinary argument/buffer validation and immediately before beginning a
transaction:

1. Require `timeoutMs` in `1..UINT16_MAX`. Invalid timeout returns
   `I2C_ERROR/I2C_DETAIL_INVALID_TIMEOUT` without beginning a transaction.
2. Sample `millis()` once and create one wrap-safe absolute callback deadline.
   The admitted interval is below `2^31` ms and the deadline is exclusive.
3. Before every potentially blocking Wire operation, resample `millis()`,
   compute the positive remaining interval relative to that one deadline, and
   apply only that remainder with `ScopedWireTimeout`.
4. Return `I2C_TIMEOUT` when no time remains. Post-check the whole callback
   even if Wire reports success.
5. Let RAII restore the previous Wire timeout on every return path.

Before `beginTransmission()`, require more than 1 ms remaining. Reserve the
last millisecond for releasing the transaction, and bound the normal physical
transaction by `remainingMs - 1`. Never grant each Wire phase a fresh copy of
the original timeout.

For the pinned ESP32 Arduino 3.2.0 implementation,
`endTransmission(false)` stages the repeated-start transaction and
`requestFrom(..., true)` executes the combined write-read with its final STOP.
Recompute and apply the remaining callback time immediately before
`requestFrom()`. Normal write-read remains exactly one
`endTransmission(false)` followed by one final-stop `requestFrom()`.

The application owner must keep the shared Wire mutex uncontended during the
callback. Document that precondition; the adapter must not add another lock or
scheduler.

### 1.3 Close every started transaction

After `beginTransmission()`, no exit may leak the Wire transaction.

If `wire.write()` stages fewer bytes than requested:

1. do not retry staging or the high-level operation;
2. recompute the remaining callback time;
3. call exactly one bounded `endTransmission(true)` to issue a final STOP and
   release ownership;
4. return `I2C_TIMEOUT` if the release reaches/crosses the callback deadline;
   otherwise return `I2C_ERROR` with `I2C_DETAIL_SHORT_STAGING`; and
5. treat the partial physical attempt as ambiguous.

Do not return the short-stage error before the bounded final STOP. A later
transaction must be admitted normally rather than remaining stuck behind a
leaked Wire owner.

Post-dispatch buffer, availability, response-length, and unknown Wire errors
also remain inside the allowed `I2C_*` domain. Do not add transport recovery or
retry. The sole owner exception remains a separately documented read-only
callback that may perform bounded recovery plus at most one read retry, with
both attempts inside one callback timeout and counted by the driver as one
instruction. Mutating callbacks always make one physical attempt.

### 1.4 Initialization must report failure

Keep the existing example/application ownership of optional pre-begin bus
recovery. Make `initWire()` return `false` if either:

```cpp
Wire.begin(sda, scl)
Wire.setClock(frequency)
```

returns false. Never report the bus ready or continue RTC setup after either
failure. Apply the configured baseline Wire timeout only after successful
initialization. Do not move pins, bus frequency, recovery, or initialization
into `RV3032`.

### 1.5 Wire acceptance tests

Extend the native Wire stub without putting a fake device into production.
Record requested, effective, and restored timeout values and transaction
release evidence. Prove:

- null pointers, zero lengths, oversize lengths, and timeout `0` or above
  `UINT16_MAX` return an allowed status with zero transaction start;
- every callback return code belongs to the six-value transport domain;
- a requested 5 ms callback, including the primary-cell profile, actually
  applies a complete 5 ms bound rather than the global 50 ms setting;
- every blocking phase receives only the remaining time and the old Wire
  timeout is restored on success and every failure path;
- exact-boundary and one-millisecond-late completions return `I2C_TIMEOUT`;
- `uint32_t` `millis()` wrap is handled correctly;
- a short staging write invokes one final STOP, is not retried, returns the
  exact detail, and permits the following transaction;
- normal write and repeated-start read have the exact physical call order;
- Wire result code 1 maps to `I2C_ERROR`, not parameter failure;
- `begin()` failure and `setClock()` failure each make `initWire()` return
  false; and
- the documented read-only owner recovery/retry exception stays within one
  callback bound and one driver instruction.

## 2. CLI: one strict parser and one operation owner

Implement this section in the existing example helper and CLI. The library
must not acquire CLI or scheduler responsibilities.

### 2.1 Keep one overflow-safe line reader

Delete local `read_line()` from
`examples/01_basic_bringup_cli/main.cpp`. Use
`cli_shell::readLine(String&)` from `examples/common/CliShell.h` as the sole
reader. Retain its rule that an overlength line is discarded through its line
terminator and no truncated prefix is executed.

Delete these obsolete permissive helpers from
`examples/common/CommandHandler.h`:

```text
cmd::readLine(char*, size_t)
cmd::parseInt(... atoi ...)
cmd::match(...)
```

Do not leave unused alternate parsers or a second input buffer.

### 2.2 Required strict token helpers

Place exactly these helpers in the existing
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

Each helper must:

- parse into a wide temporary;
- set `errno = 0`, inspect the end pointer, reject `ERANGE`, and require exact
  token consumption;
- reject a leading minus sign for unsigned values;
- range-check before narrowing and leave the output unchanged on failure;
- use `isfinite()` for floating point; and
- accept no implicit default for an empty token.

All numeric helpers except `parseRegisterToken()` are base-10 only.
`parseRegisterToken()` accepts decimal or an explicit `0x`/`0X` hexadecimal
prefix. It never interprets a leading zero as octal. Whitespace may separate
command tokens, but junk within or after a token is rejected.

Replace every mutation-facing `toInt()`, `toFloat()`, unchecked `strtoul()`,
permissive `sscanf()`, and `atoi()`. Apply strict parsing and exact argument
counts to calendar set, Unix time, alarm, Boolean interrupt controls, CLKOUT,
offset, timer, EVI, Status clear, raw register access, verbose, and stress
counts. Reject trailing tokens. An invalid command performs zero RTC/I2C
callbacks and starts no cooperative work.

### 2.3 Required cooperative ownership type

Track accepted asynchronous work with exactly this fixed type:

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

Keep one `PendingOperation` instance in the CLI owner. Do not infer ownership
from shared `isJobBusy()` and do not add a command-specific polling loop.

Delete the current unconditional loop-level `g_rtc.tick(millis())` polling
path when installing this owner. The same EEPROM work must never be advanced
both by an untracked `tick()` call and by `PendingOperation`/`pollEeprom()`.
The README may still demonstrate `tick()` as the simple application integration
surface, but this CLI's richer terminal reporting requires its one explicit
pending owner.

`operationAccepted()` records a pending operation only when the starting API
returns `IN_PROGRESS`:

- `ORDINARY_JOB`: call `pollJob(nowMs, 1, instructionsUsed)`. On terminal,
  retain that exact status in `ordinaryStatus`. If `isEepromBusy()` is true,
  change only the surface to `EEPROM`; do not admit a command or dispatch a
  second callback in the same ownership transition, and return from the owner
  poll. Otherwise report `ordinaryStatus`, replace the sole pending record with
  a value-initialized `PendingOperation{}`, and return without dispatching
  another RTC/I2C callback in that iteration.
- `EEPROM`: call `pollEeprom(nowMs, 1, instructionsUsed)` until terminal. Then
  report both the ordinary operation and persistence outcomes before clearing
  the pending record.
- No new RTC/I2C command is dispatched while either surface remains active.
  Non-I2C help text may remain available.

This handoff is required because a successful staged ordinary job can enqueue
generic persistence after its active result is terminal. Do not clear the
owner at the ordinary terminal edge and accidentally admit overlapping work.

For a blocking example helper around a typed persistent read, make its local
deadline exactly equal to the whole-operation deadline supplied at admission.
If it is still `IN_PROGRESS`, call `pollJob()` once with that deadline value so
the job reaches a terminal state. A generic configuration queue is advanced
with `pollEeprom()`, not `pollJob()`. No helper may return while leaving active
work untracked.

### 2.4 CLI acceptance tests

Add or extend device-free parser/contract tests for:

- `256` for `uint8_t`, negative unsigned values, signed/unsigned integer
  overflow, junk suffixes, trailing tokens, empty tokens, NaN, infinity,
  decimal/hex register tokens, leading-zero decimal, and whitespace;
- exact Boolean tokens `0` and `1`, rejecting every other integer;
- an overlength command followed by a valid command, proving the truncated
  prefix never executes;
- every invalid mutating command producing zero callbacks;
- one-callback-per-poll ordinary execution;
- ordinary terminal success/failure followed by pending EEPROM success/failure;
- no command admission between the two surfaces;
- no unconditional `tick()` or second polling path advancing owned work;
- no status loss when ordinary work fails and cleanup/persistence follows; and
- typed persistent helper timeout reaching terminal without orphaned work.

Update `tools/check_cli_contract.py` so the old permissive paths cannot return
without detection. Do not make a textual guard the only behavioral evidence.

## 3. Truthful example completion and output

### 3.1 Cooperative RAM write

`cmd_ram_write()` must treat `IN_PROGRESS` as accepted. Record it as an
ordinary pending operation, advance it through the single owner, and print
success or failure only after terminal completion. A 16-byte RAM write must
never print immediate failure and then continue mutating in later loop polls.

### 3.2 Empty timestamps

`cmd_ts()` must inspect the timestamp result's `timeValid` field. When the
timestamp block is empty/unset, print an explicit empty/unset result and the
count. Do not format or print a fictitious date from a zero-initialized
timestamp.

### 3.3 README polling snippets

Every maintained snippet that needs the current time must first sample it:

```cpp
const uint32_t now = nowMs(nullptr);
```

Pass `now` to polling APIs, not the `nowMs` function pointer. Add compile-only
coverage for maintained README snippets. Update HIL parser expectations if
terminal output wording changes, but do not claim that parser tests are
physical HIL.

## 4. Remove false presence semantics

Delete `isOnline()` from the public API. Do not leave an alias, deprecation
shim, derived replacement Boolean, or compatibility flag. `DriverState`
already exposes the observational `READY`, `DEGRADED`, and `OFFLINE` health
label.

Replace all maintained example and helper use of `isOnline()` with explicit
`DriverState` formatting. A view may color or print the enum, but it must not
rename `READY` to "present" or "online". Update at least
`HealthDiag.h`, `HealthView.h`, the CLI driver/status output, self-tests, and
tests.

Document these exact meanings:

- passive `begin()` binds validated callbacks and sets initial observational
  state to `READY`; it performs no I2C and proves no device presence;
- `probe()` performs one raw Status-register read at address `0x51` and proves
  only address response/communication for that transaction;
- `probe()` cannot prove chip identity because another device can ACK the same
  address and register access;
- raw `probe()` does not update health counters; and
- callers needing presence evidence retain and interpret the explicit probe
  result. `state()` remains health evidence, not an admission gate.

Change every "presence and identity" claim to "address response/communication"
or equally precise wording. Finding `M-09` is not closed by renaming
`isOnline()` while retaining its semantics elsewhere.

Acceptance evidence must prove passive READY before any callback, failed and
successful raw probe with unchanged health, no public `isOnline()` symbol, and
no maintained example/doc claim that READY or a single read identifies the
silicon.

## 5. Maintained documentation and v3 metadata

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
example help and output
public Doxygen
tools/check_* contract guards
```

Do not silently rewrite Prompt Suite 04, Prompt 05, dated v2 reports, or other
historical evidence. In `docs/README.md`, list
`docs/reports/2026-07-14-full-library-functional-audit.md` as the active v3
hardening prompt and classify those prior prompts/reports as historical where
they conflict.

### 5.1 Required final documentation corrections

Document all completed phases, not just the phase-3 diff:

- password management, credentials, unlocking, and protection workflows are
  unsupported and removed; the vendor password register constants remain and
  raw access is denied. A pre-protected module requires out-of-band service.
- `Config::enableEepromWrites` grants only configuration EEPROM `C0..C5` and
  typed user EEPROM `CB..EA`, never password-register access.
- transport callbacks are synchronous, buffers are borrowed only during the
  call, `Status::msg` is static, and only the six `OK`/`I2C_*` result codes are
  allowed.
- callback timeout and cooperative deadlines are hard, exclusive bounds;
  configuration jobs expose requested/safe-disabled/unknown final evidence;
  no ambiguous mutating callback is blindly retried.
- `end()` abandons active/queued work through unconditional zero-I/O local
  teardown, and the driver is noncopyable/nonmovable.
- backup DSM/LSM terminal success follows verified active configuration and
  the vendor 2/10 ms activation interval.
- persistence reports operation proof separately from cleanup proof;
  `tick()` returns the exact `pollEeprom(nowMs, 1, instructionsUsed)` status.
- `setPeriodicUpdate(..., bool updateEventEnabled)` uses UIE semantics: false
  prevents UF generation and INT; there is no UF-polling-only mode.
- interrupt-sensitive reconfiguration requires the documented TIE/AIE/EIE/
  BSIE quiescence sequence.
- Wire callback time is a hard bound for the complete adapter callback and the
  bus owner must serialize access.
- CLI validation is strict, asynchronous commands retain ownership through
  optional persistence, and outputs are terminal evidence.
- `READY` is observational health after passive binding, not presence;
  `probe()` is address communication, not identity.
- fallible date/Unix helpers leave output unchanged on failure; public BCD
  conversion utilities are removed.
- lifetime health counters wrap at `UINT32_MAX`, and `recover()`/`lastError()`
  use consistent mapped presence evidence.

Change weekday wording in `CommandTable.h` and `DEVICE_REFERENCE.md` to a
user-assigned three-bit binary value `0..6`, not BCD and not required to match
the calendar date.

Every named simple Status-register flag clearer for AF, TF, UF, EVF, PORF, and
VLF must repeat or cross-link this exact warning:

> Any Status-register write clears THF and TLF in silicon. If either omitted
> flag is already set at the guard read, the operation returns INVALID_PARAM
> without writing. An assertion after the guard read cannot be preserved.

Document the verified calendar-set job separately: its public name promises
PORF/VLF clearing, it captures THF/TLF pre-clear evidence in
`VerifiedTimeSetReport`, and its Status write has unavoidable THF/TLF clearing.
Do not incorrectly describe it as taking the simple-clearer's
`INVALID_PARAM` exit.

In `Status.h`, replace the broad claim "All library functions return Status"
with "All fallible operations return Status".

### 5.2 Breaking version and changelog

The password deletion, `isOnline()` deletion, noncopyability, `tick()` return
type, and checked conversion signatures are breaking changes. Set the version
only through:

```powershell
python scripts/generate_version.py set 3.0.0
python scripts/generate_version.py check
```

`library.json` is the version source of truth. Never hand-edit
`include/RV3032/Version.h`. Put the work under `CHANGELOG.md` `Unreleased` with
clear `Removed`, `Changed`, and `Fixed` sections. Do not add a release date,
commit, or tag.

## 6. Required verification

Run focused tests after each logical owner is changed. After all corrections
and after the subagents' final-diff audit, run the complete device-free matrix
from the repository root and record exact pass/fail counts and firmware sizes:

```powershell
python scripts/generate_version.py check
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_docs_contract.py source
pio test -e native
pio run -e esp32s2dev
pio run -e esp32s3dev
$packagePath = "dist/RV3032-C7-phase3-audit.tar.gz"
if (Test-Path -LiteralPath $packagePath) { throw "Preserve existing package artifact" }
pio pkg pack -o $packagePath .
python tools/check_docs_contract.py package $packagePath
python tools/hil_cli_runner.py --parser-self-test
python tools/hil_cli_runner.py --dry-run
git diff --check
```

The package path is phase-owned only when the preflight proves it did not
exist. If it already exists, do not overwrite or delete it; choose another
unique phase-owned path, use that same path for both commands, and record the
equivalent commands. On Windows, use the executable under
`%USERPROFILE%\.platformio\penv\Scripts\` if `pio` is not on `PATH`, and report
the actual equivalent command used. Inspect the package before deleting only
the package artifact created by this phase. Resolve its absolute path and
confirm it is inside this repository's `dist` directory before using
`Remove-Item -LiteralPath`. Do not delete pre-existing user artifacts or leave
generated package debris.

Also run and inspect focused source audits. At minimum prove:

```powershell
rg -n "isOnline\(" include src examples test README.md docs/README.md docs/ARCHITECTURE.md docs/DEVICE_REFERENCE.md docs/IDF_PORT.md
rg -n "read_line\(|cmd::readLine|parseInt\(|atoi\(|\.toInt\(|\.toFloat\(|sscanf\(" examples/01_basic_bringup_cli examples/common
rg -n "void tick\(|All library functions return Status|presence and identity" include src examples README.md docs/README.md docs/ARCHITECTURE.md docs/DEVICE_REFERENCE.md docs/IDF_PORT.md
rg -n "PasswordCredential|PasswordProtectionEvidence|PasswordProtectionStatus|unlockPasswordProtection|startConfigurePasswordProtectionJob|getPasswordProtectionStatus|WritePassword|ApplyPassword|ApplyPasswordBytes|ApplyPasswordCredential|FinalizePasswordEnable|CleanupLockPassword|PasswordProtection|persistentUseAddressList|persistentAddresses|currentCredential|newCredential|passwordEnable|passwordAuthorizationMayBeActive|_passwordStatus" include/RV3032/RV3032.h src/RV3032.cpp
```

The first and fourth scans must have no live maintained-code matches. For the
second scan, strict low-level conversion calls may exist only inside the new
checked token helpers; permissive mutation-facing calls and the deleted helper
names must not. Historical prompts/reports and vendor facts are expected to
retain some old words and must not be rewritten merely to make a global grep
empty.

Review the package contents for public headers, README, examples, version, and
absence of test/build/user debris. A green package command without content
inspection is insufficient.

## 7. Physical-validation boundary

Do not flash, remove VDD, alter backup topology, issue wear-limited EEPROM
commands, or run destructive HIL in this phase. Parser self-test and `--dry-run`
are device-free and are not physical HIL.

This implementation phase is device-free even if a fixture is available.
Record physical HIL exactly as:

```text
NOT RUN
```

Fresh authorization for physical validation starts a separate physical task;
it does not broaden this phase. Never infer retention, INT/CLKOUT electrical
behavior, or physical timeout compliance from the native fake.

## 8. Phase completion criteria

Phase 3 is complete only when:

- all prerequisites are reconciled without a parallel compatibility layer;
- `M-07` is closed by a complete callback timeout, restoration, final STOP on
  short staging, allowed status domain, and truthful initialization result;
- `M-08` is closed by the sole overflow-discarding line reader, strict token
  helpers, exact arguments, zero-I/O invalid input, and explicit ownership
  through ordinary and EEPROM surfaces;
- `M-09` is closed by deleting `isOnline()` and documenting READY/probe
  evidence precisely;
- `L-01` is closed by terminal RAM output, truthful empty timestamp output,
  corrected README polling, and compile/output checks;
- `L-05` is closed by binary weekday wording and complete Status-write side
  effect documentation;
- maintained headers, examples, docs, changelog, guards, and package agree;
- version `3.0.0` is generated and its check passes;
- all device-free commands pass after the final edit;
- no test was weakened and no stale alternate reader/parser/owner remains;
- physical HIL is reported truthfully as `NOT RUN`; and
- no unrelated edit, generated debris, commit, tag, or publication exists.

A genuine conflict or failed gate is a blocker, not completion. Report it with
the exact command, status, and file/line evidence.

## 9. Phase handoff

Lead with achieved behavior. Report:

- the five findings closed and their code/test evidence;
- deleted paths and materially simplified owners;
- exact files and public APIs changed;
- exact focused/native/build/guard/package/parser results and firmware sizes;
- physical HIL exactly as `NOT RUN` for this device-free phase;
- prerequisite deviations, remaining field risks, and genuine blockers;
- final HEAD and `git status --short`; and
- confirmation that no commit, tag, release, publication, or unauthorized
  hardware action occurred.

Do not claim the full four-phase hardening complete. Phase 4 performs the
independent closure audit across every High, Medium, and Low finding.
