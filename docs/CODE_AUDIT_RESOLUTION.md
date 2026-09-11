# Code audit resolution

Latest verification: [2026-09-10 complete correction review](#2026-09-10-complete-correction-review).
Earlier sections retain the state and conclusions of their respective reviews.

Date: 2026-08-30
Audit reviewed: `docs/CODE_AUDIT.md`

## Scope and method

The original audit was reread in full and checked against the current source,
public headers, examples, tests, repository checks, package manifest, pinned
Arduino-ESP32 implementation, and the retained RV-3032-C7 vendor documents.
The fresh pass began from clean, synchronized `main` at `497a382`; no previous
summary or resolution claim was accepted without checking the actual code and
diff.

Three independent read-only reviews ran in parallel:

- requirements coverage, API compatibility, scope, and documentation;
- implementation correctness, cleanup/deadline state machines, and edge cases;
- tests, Wire behavior, package tooling, CI parity, and registration coverage.

Their findings were then reproduced against the working tree. Confirmed gaps
were fixed and the reviewers inspected the resulting diff again.

## Result

The first audit-resolution pass addressed most findings correctly, but the
fresh review found eight material follow-up gaps:

1. C1's split state was correct internally, but the public combined
   `isJobBusy()` could still direct an owner to `pollJob()` while active generic
   EEPROM work required `pollEeprom()`.
2. C2's defaulted trailing arguments preserved ordinary calls but changed the
   exact legacy member-function types and symbols.
3. C4 did not create a cleanup obligation immediately after the first read
   observed pre-existing EERD.
4. Two auxiliary cleanup failures incorrectly destroyed otherwise attainable
   C0/Control 1 proof, producing a false sticky recovery requirement.
5. Persistent-access recovery could prove C0 and cleared EERD, enter electrical
   activation settling, then relabel that proven state as unknown at the whole
   deadline.
6. E2's fixed 32-byte example-adapter cap rejected the driver's valid 46-byte
   full register burst even though the pinned Arduino-ESP32 core publishes a
   128-byte Wire buffer.
7. The package archive checker did not enforce every declared export exclusion.
8. D7 and D11 were incomplete: the README retained an absolute HIL link and
   CONTRIBUTING omitted several CI gates.

The fresh tests also disproved the proposed F10 repair. `clkout 0|1` uses the
generic single-register update job, which has terminal `Status` but does not own
the staged CLKOUT `ConfigurationJobReport`. Renaming it to `"CLKOUT
configuration"` caused a false "result evidence unavailable" diagnostic; it
could not reveal evidence that does not exist. The simple correct behavior is
an accurate `CLKOUT enable`/`CLKOUT disable` terminal report. Typed
configuration evidence remains available for `clkout_freq` and the complete
staged CLKOUT job.

All confirmed gaps above are now closed.

## Original code findings

| Finding | Final verdict | Final resolution |
|---|---|---|
| C1 | Valid, high severity | Ordinary jobs and generic persistence retain separate fixed `PersistentOp` owners, so queue work cannot erase a completed ordinary status/result. Legacy `isJobBusy()` remains combined. New `isOrdinaryJobBusy()` selects `pollJob()` unambiguously; an active generic item makes `getJobStatus()` return `BUSY`, and the last ordinary terminal result is visible again after that item terminates. README and IDF owner loops use the ordinary-only predicate. |
| C2 | Valid, high severity; original proposal covered only the BSM-to-TCM transition | Both directions reject a charging-enabled BSM/TCM pair unless the caller explicitly supplies `ALLOW_BACKUP_CHARGING`. Exact legacy signatures remain `startSetBackupSwitchModeJob(mode, now, timeout)` and `setTrickleChargeMode(mode)`. Explicit intent uses the distinctly named `...WithChargePolicy()` methods, preserving legacy member-function types and symbols. |
| C3 | Valid | Snapshot and verified-set defaults remain 200/700 ms, executable across the accepted 1..100 ms callback timeout range. No-clock tests cover the accepted maximum. |
| C4 | Valid, high severity; the proposed reserve was incomplete | The sticky `persistentAccessStateUnproven` latch, zero-I/O admission blocks, and explicit bounded recovery job remain. The write cutoff reserves the complete post-WRITE_ONE busy, two-read proof, cleanup, and settle chain. This pass additionally latches cleanup as soon as EERD is observed, distinguishes auxiliary cleanup errors from loss of access-state proof, and clears the sticky latch immediately after recovery proves C0 and Control 1. A later activation-settle timeout remains an operation timeout with `REQUESTED_VERIFIED`, not a cleanup failure. |
| C5 | Valid | Synchronous writes reject all pending cooperative work. Blocking reads remain available under the documented application serialization contract. |
| C6 | Valid; a global 5 ms clamp would be unnecessarily disruptive | `primaryCellI2cTimeoutMs` has its own 1..5 ms domain and is used only by the dedicated synchronous primary-cell operation. Ordinary transport timeouts retain their 1..100 ms contract. |

### C7 dispositions

| Item | Final disposition |
|---|---|
| C7.1 | Valid. Persistent cleanup and recovery mark C0/Control 1 proof before electrical settling; later settle expiry cannot erase it. |
| C7.2 | Valid. One subtraction-based register-span validator avoids unsigned overflow and is reused for block/password checks. |
| C7.3 | Already correct. The 16-byte synchronous user-RAM API documents and returns `IN_PROGRESS`; no API change was needed. |
| C7.4 | Not a defect under the repository's binding architecture. Fixed, acyclic, no-wait operations may use a derived transfer-count times per-transfer bound. |
| C7.5 | Valid latent risk. Mutation sites use an explicit active cutoff flag. |
| C7.6 | Valid. Disabled alarm-date fallback is the reset-compatible value `0`. |
| C7.7 | Valid. Verified calendar readback validates weekday, including rollover. |
| C7.8 | Valid. Generic WRITE_ONE polling capacity derives from `eepromTimeoutMs + 1`; the primary-cell cap remains independent. |
| C7.9 | Valid, but the proposed contiguous C0..EA range included password bytes. Validation permits only C0..C5 and CB..EA. |
| C7.10 | Valid. The hardware century limitation and distinct invalid-weekday/date diagnostics are public. |
| C7.11 | Valid. The unreachable health-state branch was removed. |
| C7.12 | Valid. Build literals use fixed-layout digit parsing; core code does not include stdio. |
| C7.13 | Valid ABI concern. Public error ordinals are explicit, reserved values remain, unused state was removed, and this pass restored the exact legacy setter types. |

## Original example and test findings

| Finding | Final verdict and resolution |
|---|---|
| E1 | Valid. Short staging is flushed before the address-only STOP/owner release. Tests distinguish API calls from physical attempts and prove no partial payload is sent. |
| E2 | Valid. The first pass's 32-byte cap was too restrictive. `MAX_TRANSFER_BYTES` now uses Arduino-ESP32's published `I2C_BUFFER_LENGTH` with a 32-byte fallback. A 46-byte public register read is tested through the adapter. |
| E3 | Incorrect for pinned Arduino-ESP32 3.3.11. `endTransmission(false)` stages and retains the transaction; `requestFrom()` performs the combined physical attempt. The cleanup STOP is needed only before request dispatch. |
| E4 | Valid. The adapter is explicitly scoped to Arduino-ESP32 on ESP32-S2/S3. |
| E5 | Useful observation, proposed watchdog unsafe. Dropping the CLI owner would orphan active driver work. No cancel abstraction was added. |
| E6 | Valid. The fake rejects indirect password operations, and production range validation independently excludes them. |
| E7.1 | Valid. Zero-budget EEPROM polling is covered. |
| E7.2 | Valid. Primary write/verify failure stages are asserted. |
| E7.3 | Partly stale. Existing no-clock tests were extended for the documented defaults. |
| E7.4 | Overstated. Two fixture-only tests, not three, required clearer names. |
| E7.5 | Observation only. Existing nonzero callback-duration tests were retained and extended around cutoff behavior. |
| E7.6 | Valid. Scanner timeout/error rendering branches have queued-result coverage. |
| E8 | Valid. The established version encoding remains; generation rejects minor/patch components above 99 and values outside generated storage. |

F1-F9 and F11 were rechecked against the implementation and remain valid.
Direct regressions now cover F9's zero-byte `requestFrom()` mapping through
both the adapter and `probe()`, plus every unsigned parser's leading-plus
rejection. F10 is the exception described above: its claimed report-name fix
was invalid for the single-register job and was replaced with truthful
terminal-only reporting.

The audit's five unnumbered minor CLI items were also rechecked. Whitespace-only
input redraws the prompt, the user-EEPROM helper validates copy bounds locally,
user-RAM confirmation errors are specific, CLKOUT enum rendering is guarded,
and help text describes the intentionally narrow verbose scope. Those fixes
remain correct.

## Documentation and repository findings

D1-D15 were checked individually. Their substantive corrections remain. This
pass completed D7 with a relative HIL-summary link that also builds cleanly
under warnings-as-errors Doxygen, and completed D11 with all four PlatformIO
build environments, native tests, version/portability/ABI/package checks, HIL
parser self-test and dry-run, Doxygen, real package creation, and archive
validation.

The stale `.gitignore` comment now names `tools/check_package.py`. The package
checker applies `REQUIRED_EXPORT_EXCLUDES` directly, including `.github/**`,
`docs/doxygen/**`, and root archive patterns, while retaining defense-in-depth
checks for repository metadata and generated artifacts. Representative policy
probes prevent these exclusions from drifting silently.

The earlier follow-up corrections also remain valid:

- `library.json` excludes `docs/CODE_AUDIT*.md`, not stale root `AUDIT.md`;
- the HIL summary accurately limits what the retained physical campaign proves;
- `Version.h` is tracked and generated from `library.json`;
- the focused portability, ABI, and package checks replace brittle
  source-order/exact-prose archaeology.

The audit's documentation-duplication section remains a sound maintenance
recommendation, not a correctness defect. No wholesale consolidation was made:
README integration guidance, architecture rationale, public-header contracts,
and binding repository instructions serve different audiences, and local
safety warnings are intentionally repeated beside hazardous APIs.

No ignored `.pio` scratch data or generated Doxygen output was deleted; those
are local build artifacts outside the requested source changes.

## Additional regression coverage added in the fresh pass

- exact legacy member-function signatures at compile time;
- active generic EEPROM versus ordinary polling/status semantics;
- pre-existing EERD observed on the first callback followed by a hard deadline;
- cleanup-ready and selected-active verification failures followed by exact
  C0/Control 1 proof, without a false recovery latch;
- persistent-access recovery proof followed by activation-settle deadline;
- absent-device adapter/probe mapping and full 46-byte register reads;
- leading `+` rejection for U8, U16, U32, Boolean, register, and CLI commands;
- accurate `clkout 0` terminal reporting without nonexistent typed evidence;
- data-driven package exclusion probes.

All 120 native test functions are registered exactly once.

## Final verification

All device-free gates passed on the final worktree:

- `.\scripts\pio.cmd test -e native` - 120/120 passed.
- `.\scripts\pio.cmd run -e esp32s3dev -e esp32s2dev -e esp32s3hil -e esp32s3hil_persistence` - all four targets passed with platform 55.3.311 and Arduino-ESP32 3.3.11.
- `python scripts/generate_version.py check` - passed.
- `python tools/check_portability.py` - passed.
- `python tools/check_abi.py` - passed.
- `python tools/check_package.py source` - passed.
- `python -S tools/hil_cli_runner.py --parser-self-test` - passed.
- `python -S tools/hil_cli_runner.py --dry-run` - produced the expected 26-step plan.
- `doxygen Doxyfile` - passed with warnings treated as errors.
- `.\scripts\pio.cmd pkg pack -o RV3032-C7-audit-final.tar.gz .` followed by `python tools/check_package.py package RV3032-C7-audit-final.tar.gz` - passed.
- `git diff --check` - passed.
- Native definitions/registrations check - 120/120, exactly once.

No physical device was connected in this pass. The HIL firmware environments
were compile-checked, and the device-free HIL parser/dry-run checks passed; no
new physical HIL result is claimed.

## Independent follow-up review, 2026-09-09

Reviewed the 22-item follow-up against clean `main` at `f3db733`, after fetching
and confirming it matched `origin/main`. This section supplements the earlier
review; it does not reopen the original closed findings.

| Item | Assessment and correction | Evidence |
|---|---|---|
| 1 | Valid: exclude a dispatched WRITE_ONE from the forward mutation cutoff. The reserved proof/cleanup interval now remains usable. | Cutoff-crossing tests prove one durable byte, retain `I2C_TIMEOUT`, and count exactly one WRITE_ONE. The smallest admitted timeout for the default fake configuration (516 ms) completes successfully. |
| 2 | Valid: coherent-temperature default now uses `READ_TIME_OPERATION_TIMEOUT_MS` (200 ms). | A 100 ms callback configuration with no clock hook completes the two-sample job. |
| 3 | Valid: `begin()` validates the EEPROM window even with writes disabled; redundant recovery-time validation was removed. | Invalid 0/9/251/UINT32_MAX windows are rejected with zero I/O; recovery after a failed persistence operation also runs with writes disabled. |
| 4 | Valid: retain the unproven-access latch across passive lifecycle changes, including abandonment of active cleanup and callback rebinding. | Terminal-failure and active-abandonment tests preserve the latch through end/begin and clear it only after proven recovery. A sticky PORF read is not fresh access-state proof. |
| 5 | Valid: ready-read failure, ready timeout, or exhausted check count records the cause and continues direct recovery. EEbusy gates EECMD only. | Busy and transport-failure tests restore C0/EERD while returning the first error; no EEPROM command is issued. |
| 6 | Valid: backup default increased to 500 ms. | Direct tests at 60/100 ms callback limits, with and without a clock hook, complete disabled-to-Level activation. |
| 7 | Valid documentation omission: all five write APIs document queued-persistence `BUSY`; 3.1.0 history records the behavior. | Existing synchronous-write admission regressions and Doxygen. |
| 8 | Valid: the entire ESP32 adapter is now protected by an explicit architecture error/guard, including its Wire calls. | Native build explicitly selects ESP32 API stubs; embedded builds compile the production adapter. |
| 9 | Valid safe subset: a 15-second, one-shot pending diagnostic retains ownership. Serial input is drained with a fixed per-poll byte cap and incomplete lines are discarded through their terminator. | CLI tests cover unsigned clock wrap, one callback per poll, one warning, continued ownership, and discarded command tails. |
| 10 | Valid defensive correction: invalid spans count as unsupported in the password-range helper. | Direct invalid-span regression and the existing public zero-I/O allowlist tests. |
| 11 | Valid checker gaps. Core includes/parsers, transitive member calls from cooperative engines, and exact health owners are checked. The report's three-owner count is incorrect: six actual owners are necessary (ordinary, explicit-timeout, presence, and timed completion wrappers). | Python mutation probes reject stdio/parser insertion, nested untimed I/O, and a fabricated tracked-sounding owner. Reviewed six-name allowlist replaces name-pattern trust. |
| 12 | Valid: extend the ABI/default baseline to all 15 public enums including Err and both Config enums, and all eight public timeout/size constants. | Mutation probes alter enum representation, explicit/implicit values, and every constant. CI runs these probes. |
| 13 | Valid coverage gap: test all five internal-error sites with native-only state injection through a friend accessor. No production fake or conditional runtime path was added. | Idle active jobs, invalid ordinary state, backup verification without a mutation, absent Control 1 cleanup evidence, and invalid persistent state all report typed errors. Mutated persistent state still performs bounded cleanup. |
| 14 | Valid: password-command rejection now acts as a negative suite tripwire. | Twenty additional assertions alongside existing protocol-violation assertions, including generic persistence and primary-cell paths. |
| 15 | Partially valid: the 32-byte fallback was too small and is now 64. The claimed legal 57-byte public read is incorrect: the direct allowlist ends at `REG_TS_EVI_YEAR` (0x2D), giving 46 bytes. | The adapter test reads all 46 legal bytes and proves a 57-byte public span is rejected before another physical attempt. The allowlist remains intact. |
| 16 | Valid comment correction verified against installed Arduino-ESP32 3.3.11 `Wire.cpp`: flush clears staging; endTransmission(true) releases the mutex but not nonStop. | Existing address-only cleanup, timeout, and next-transaction tests. |
| 17 | Valid: determine already-requested BSM before applying the charging guard. | Both Direct and Level no-ops with nonzero TCM return success after two reads and zero PMU writes. Actual charging-enabling changes still fail without explicit policy. |
| 18 | Valid: add `isEepromPollable()`, document cross-surface BUSY, and distinguish ownership status from typed terminal evidence. | A combined owner loop drains persistence queued behind an ordinary update; existing typed-result retention tests cover the opposite ownership direction. README and IDF examples select one surface per pass. |
| 19 | Recommendation announced before implementation: materialize the existing no-wait bounds without introducing new setter deadlines. `NO_WAIT_JOB_CALLBACK_CAPS` is indexed by JobKind and checked by native success/fault matrices. Temperature requires at most 10 callbacks, not 11; guarded timestamp reset explains the four-callback REGISTER_UPDATE bound. | Maximum-timeout tests exercise all four staged setters, successful completion and final-read failure/cleanup, with and without a clock hook. Architecture documentation distinguishes callback time from caller scheduling gaps. |
| 20 | Valid documentation contradiction: list the two retained audit documents separately from maintained API documentation. | Documentation index and package exclusion checks. |
| 21 | Valid: README now includes both HIL build environments. | README, CONTRIBUTING, and the CI matrix cover all four targets; checker regression tests are also listed. |
| 22 | Valid release-hygiene concern. Follow-ups are collected in 3.2.0, with Version.h generated from library.json. A minor version follows the repository's rule for the new public polling predicate. 3.1.0 is identified as an untagged development baseline and its two trailing SettingsSnapshot fields are documented as source-compatible, binary-layout changes. | Version/ABI/package checks. No commit, tag, or publication is part of this review. |

The latest hosted CI run for the starting commit passed all six jobs:
[CI run 33322558762](https://github.com/janhavelka/RV3032-C7/actions/runs/33322558762).
The workflow now also runs the contract-checker mutation tests. A hosted run
for these uncommitted changes has not been created.

### Follow-up final verification

- Native: **131/131 passed**, with every test defined and registered once.
- Embedded: **esp32s3dev, esp32s2dev, esp32s3hil, and
  esp32s3hil_persistence all built successfully** against the pinned platform.
- Version generation check, portability, ABI/default contracts, and source
  package checks: passed.
- Contract-checker tests: four test methods passed, including parameterized
  mutation probes for every guarded enum and constant.
- Device-free HIL parser self-test and the expected 26-step dry run: passed.
- Doxygen with warnings treated as errors: passed.
- PlatformIO package and archive validation: passed for
  `dist/RV3032-C7.tar.gz` (version 3.2.0).
- Separate compiler probes verified the 64-byte unpublished-capacity fallback
  and the intentional non-ESP32 compile error.
- `git diff --check`: passed. Changes remain uncommitted and untagged.

The inherited `PLATFORMIO_CORE_DIR=C:\pio` selected an incomplete local compiler
store. Embedded verification used the already-installed current-user store:

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $env:USERPROFILE '.platformio'
$env:PYTHONIOENCODING = 'utf-8'
.\scripts\pio.cmd run -e esp32s3dev -e esp32s2dev -e esp32s3hil -e esp32s3hil_persistence
```

Its existing esptool 5.3.0 Python module worked, but the executable launcher
failed with `uv trampoline failed to canonicalize script path`. The launcher
was regenerated from the installed distribution's entry point using the same
VS Code-managed Python interpreter. The original launcher was preserved as
`.pio/audit-recheck-esptool-launcher.exe.bak`. No PlatformIO Core, dependency
version, global environment setting, or repository toolchain pin was changed.

These are host tests and firmware builds. No hardware was flashed and no new
physical HIL, persistence endurance, or backup-retention result is claimed.

## 2026-09-10 landing re-audit

Fetched all remotes, confirmed a clean worktree, and fast-forward checked
`main`. Both local `HEAD` and `origin/main` were
`16b700b87bb91ce88e0fcbc47945e5ac05d5e222` (`Enhance ABI and Portability Checks`).
That commit contains all 22 changed files from the preceding implementation.
Its [hosted CI run 34495585317](https://github.com/janhavelka/RV3032-C7/actions/runs/34495585317)
passed all six jobs, including native tests, four embedded targets, and library
validation. The unchanged baseline also passed all 131 native tests locally.

The changes had landed, but the prior claim of full closure was too broad:
items **1, 9, 11, and 22** still had gaps. The other 18 items were confirmed
against implementation, public contracts, and their registered regressions.
The corrections below are local follow-up work, prepared as **3.2.1**; they
have not been committed, pushed, tagged, or published by this re-audit.

### Remaining gaps reproduced and corrected

1. **EEPROM admission and proof timing (item 1).** At `i2cTimeoutMs=1`,
   `eepromTimeoutMs=100`, and with the clock hook enabled, the accepted 440 ms
   write budget expired after the two mandatory comparison waits: zero
   WRITE_ONE attempts, zero durable bytes. Clockless configurations at their
   accepted minimum failed even earlier. Giving a clockless 100 ms callback
   configuration a larger operation budget also exposed READ_ONE ready polling
   consuming the entire phase allowance before its data read.
   Admission now includes both waits; without a clock hook it charges the
   complete first-byte forward sequence (21 read / 27 write callbacks).
   Ready polling leaves a data-read allowance inside the existing 25 ms phase.
   Clocked admission continues to allow fast callbacks instead of forcing the
   pessimistic clockless bound on the default ESP32 CLI configuration.
   The existing post-WRITE_ONE cutoff correction remains intact: dispatched
   writes are reconciled once, retain their original error, and are not replayed.
2. **Busy CLI backlog (item 9).** Injecting 256 newlines followed by a command
   immediately before the final timer callback left the command queued. On the
   next loop it executed despite having arrived while the CLI was busy.
   The reader now carries the unconsumed input snapshot across completion.
   The regression also proves partial tails are discarded, newly received
   complete lines survive, and no additional transport callback is issued.
3. **Cooperative-checker bypasses (item 11).** Both an inline driver member
   calling untimed `readRegs()` and a qualified
   `RV3032::_updateHealth(st)` call from an unauthorized owner passed the old
   checker. The graph now includes inline members and all core source/header
   files, and health-call checks distinguish declarations from qualified calls.
   Mutation probes cover inline I/O, a separate implementation file, qualified
   health calls, and allowed timed calls.
4. **Changelog references (item 22).** The old Unreleased and 3.1.0 links still
   referenced nonexistent `v3.1.0`; 3.2.0 had no comparison reference.
   Development comparisons now use the existing `f3db733` and `16b700b` commits.
   Git tags and GitHub releases confirm `v3.0.1` is the latest published release.
   The 3.2.1 follow-up is explicitly marked prepared and unpublished, and its
   version header was generated from `library.json`.

### Item-by-item disposition

| Item | At pushed `16b700b` | Re-audit evidence / final disposition |
|---|---|---|
| 1 | Partial | Cutoff guard and ambiguous-write proof landed correctly. Minimum-budget coverage was too narrow; corrected locally with `test_smallest_persistent_budgets_execute`. |
| 2 | Complete | Coherent-temperature default references the 200 ms constant; no-clock 100 ms callback regression passes. |
| 3 | Complete | `begin()` unconditionally rejects EEPROM windows outside 10..250 with zero I/O. Writes-disabled recovery remains available. |
| 4 | Complete | `_resetRuntimeState()` preserves terminal and abandoned active cleanup obligations. Lifecycle/rebinding tests prove the latch survives and explicit recovery clears it. |
| 5 | Complete | Ready timeout, transport failure, and check-cap paths advance to `RECOVERY_READ_CONTROL1`; tests verify C0/EERD restoration while retaining the original cause. |
| 6 | Complete | Backup default is 500 ms; 60/100 ms callback configurations pass with and without a clock hook. |
| 7 | Complete | All five named write APIs document queued-persistence BUSY. Changelog and synchronous-write admission regressions agree. |
| 8 | Complete | The architecture error precedes all ESP32 dependencies and the full adapter body is guarded; both ESP32 builds and the negative compiler probe agree. |
| 9 | Partial | One-shot 15-second warning and retained ownership were correct. Input beyond one drain budget leaked across completion; corrected locally and reproduced by a failing-then-passing CLI test. |
| 10 | Complete | Invalid spans return true from the password-range helper; zero length, overflow, and SIZE_MAX regressions pass. |
| 11 | Partial | Core parser bans and exact six-owner allowlist landed. Inline/cross-file graph traversal and qualified health-call handling needed the local correction. Six owners, rather than the report's proposed three, remain required by the actual wrapper architecture. |
| 12 | Complete | Baselines cover all 15 public enums and eight public size/timeout constants. Mutation tests reject changed widths, explicit/implicit ordinals, and each changed constant. |
| 13 | Complete | Tests force all five INTERNAL_STATE_ERROR sites, including missing Control 1 evidence and corrupt persistent state after real access staging. Cleanup obligations survive corruption. |
| 14 | Complete | Negative password-command assertions accompany protocol-violation checks throughout the persistence suite. The fake's deliberate positive probe remains separate. |
| 15 | Complete with report correction | Fallback is 64. The actual public allowlist permits 46 contiguous bytes, not 57; tests transfer 46 and reject 57 before transport. |
| 16 | Complete | The cleanup comment correctly separates staging/mutex cleanup from nonStop reset. The installed pinned Wire implementation and transaction-order regressions agree. |
| 17 | Complete | `alreadyRequested` is evaluated before the charging guard. Direct/Level no-ops with nonzero TCM use two reads and zero PMU writes. |
| 18 | Complete | `isEepromPollable()` excludes an ordinary owner; both cross-surface BUSY contracts and typed-result distinction are documented. The combined owner regression drains queued work. |
| 19 | Complete | The chosen table exists for every JobKind and is checked by success/fault matrices. Four staged-setter maximum-timeout tests prove the derived callback bounds. This does not impose deadlines on caller scheduling gaps. Temperature's actual maximum is ten callbacks. |
| 20 | Complete | Both historical audit files are listed as retained working documents and excluded from package/API documentation. |
| 21 | Complete | README, CONTRIBUTING, and CI list all four embedded environments. |
| 22 | Partial | Version 3.2.0 and SettingsSnapshot binary-layout notes landed. Invalid comparison links remained; corrected locally alongside the explicitly unpublished 3.2.1 patch entry. |

### Re-audit verification

- Native suite: **132/132 passed**. The persistent minimum-budget test covers
  **144 combinations**: read/write, clock present/absent, 1/5/50/100 ms callback
  limits, 10/100/250 ms EEPROM windows, clock wrap, and zero/full clipped
  callback durations in clockless mode. Clocked minima are exercised with
  fast callbacks, as the public admission contract permits.
- Contract-checker suite: **6 test methods passed**, including the new
  qualified-call and inline/cross-file mutation probes.
- All four embedded environments built successfully against the pinned
  platform using the existing current-user PlatformIO installation.
- Generated version, portability, ABI, source/package validation, device-free
  HIL parser and 26-step dry run, Doxygen, and `git diff --check`: passed.
- The negative non-ESP32 and 64-byte fallback compiler probes passed.
- Hosted CI is green for the pushed baseline above. These local corrections
  require a future commit/push before hosted CI can validate them.

No physical HIL or new hardware evidence is claimed. The latest tagged release
remains v3.0.1; preparing version metadata does not publish a release.

## 2026-09-10 complete correction review

The user requested a further independent re-audit and full correction of the
remaining work. The starting worktree contained the preceding 3.2.1 edits;
they were preserved. Fetch and fast-forward checks confirmed that local and
upstream `main` still point to `16b700b`, with no divergence or conflicts.
The previous fixes therefore remain local, while the hosted six-job CI run
for `16b700b` is green.

Three independent subagents reviewed EEPROM timing/cleanup, portability/ABI
checks, and CLI/transport/API completeness. The parent reproduced findings,
reviewed their corrections, and retained the existing 22-item disposition
above as historical evidence. This pass found further gaps behind the earlier
closure claims:

| Finding | Reproduction and complete correction |
|---|---|
| 1: defaults and generic timing | With a clockless 100 ms transport and 250 ms EEPROM window, the accepted generic queue timed out before WRITE_ONE even with zero-duration callbacks. The previous direct-write default was also below the new admission minimum. Direct read/write defaults are now 4000/6000 ms. The generic budget is `max(4000, postWriteReserve + fullForwardMinimum + 250)`, bounded by 5323 ms across the accepted Config range. The obsolete fixed-budget `begin()` guard was removed. |
| 5: cleanup phase expiry | After safe C0/EERD staging and a forward failure, polling at the cleanup ready-phase deadline left the chip in access mode even though the whole-operation budget still had time. Expiry/check-cap now records the cleanup timeout and proceeds with direct restoration; whole-operation expiry still stops all callbacks. |
| 9: terminal callback input | A callback that injected `verbose 1` immediately before returning terminal status let the command execute on the next CLI loop. Both terminal polling surfaces now mark that input before reporting completion. Marking reads no bytes, so the existing 256-byte drain limit remains intact. |
| 11: normal C++ signature variants | Trailing-return and reference-qualified helper definitions, plus inline members in a final class, could bypass graph traversal. Balanced parameter parsing now recognizes these forms and distinguishes qualified calls inside conditions from definitions. |
| 12: documented enum and literal defaults | A commented old enum could hide a changed active ordinal. Enum discovery now ignores comments. The guard also baselines all eight public operation-timeout default arguments, including literal EEPROM defaults, and tests changes/removal of each. |

Independent review confirmed the remaining original findings, including the
46-byte public-read correction, all five impossible-state error sites,
six legitimate health-update owners, charged backup no-ops, the JobKind
callback-bound table, and source/package documentation consistency. No
additional change to the chosen no-wait setter architecture was needed.

The first-byte budget is an executable admission bound with the documented
clock assumptions; it does not promise completion of every 16-byte request
under arbitrary scheduling delays. A larger user-selected budget may be
needed, and typed results continue to retain verified partial progress.

### Final integrated verification

- **135/135 native tests passed**, all defined and registered exactly once.
  This includes 768 default-budget scenarios across all four persistence
  surfaces, 288 minimum-budget scenarios, eight cleanup expiry/cap scenarios,
  and four terminal-callback CLI input scenarios.
- An additional independent 192-case recovery probe passed at exact admission
  minima, including clock modes, timeout extremes, instruction budgets, wrap,
  permanent busy, and first-ready-read failures.
- **10 checker tests passed**. They cover active enum declarations, all 15
  public enum tables, eight public constants, eight operation-timeout defaults,
  core parser bans, health ownership, and transitive member-call detection.
- **All four embedded builds passed**: esp32s3dev, esp32s2dev, esp32s3hil,
  and esp32s3hil_persistence, using the pinned platform and existing managed
  PlatformIO installation.
- Version, portability, ABI, source/package checks, device-free HIL parser
  and 26-step dry run, Doxygen, fallback/non-ESP32 compiler probes, and
  `git diff --check`: passed.
- The 3.2.1 archive was rebuilt and validated at `dist/RV3032-C7.tar.gz`.
- Independent review of the final combined implementation found no further
  concrete correctness or operation-bound defects within the audit scope.

All requested code corrections are implemented and verified locally. At this
verification point, commit/push remains outstanding; hosted CI has validated
only `16b700b`. No release tag or hardware test was performed in this pass.
