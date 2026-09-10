# Code audit resolution

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
