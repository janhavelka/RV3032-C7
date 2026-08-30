# Code audit resolution

Date: 2026-08-30
Audit reviewed: `docs/CODE_AUDIT.md`
Starting repository state: clean `main` at `cdf0069`, synchronized with
`origin/main`. That commit contains the audit document; its parent `49ee81f`
contains the changes listed by the audit as already fixed.

## Method

Every reported item was traced to its current source, public contract, tests,
and—where the finding depended on silicon behavior—the retained RV-3032-C7
Application Manual Rev. 1.3. The already-applied changes were inspected rather
than accepted from the report text. Open findings were independently reviewed
for reachability, severity, and whether the proposed remedy was the smallest
complete production fix. Native tests were run before changes to establish the
111/111 baseline.

## Already-applied findings

F1–F11 were present in the synchronized source and were confirmed correct:

- cleanup continues after a non-deadline EEbusy read error;
- EVR is masked from unrelated Time Stamp Control read-modify-writes;
- backup activation and EEPROM settle constants have separate owners;
- the primary-cell PMU preserve mask and all corrected silicon/API comments
  agree with the manual;
- the example transport maps an absent read device to `I2C_NACK_ADDR`;
- CLKOUT reports use the configuration operation name; and
- strict unsigned parsing rejects both signs.

D1–D15 were also present and valid. The repository layout, CODEOWNERS,
version-file tracking, package description, build matrix, contribution checks,
documentation links, lifecycle-counter wording, stale generated artifacts, and
UTF-8 shebang files were checked. Two follow-up corrections were necessary:

- `library.json` still excluded the old root `AUDIT.md`; it now excludes
  `docs/CODE_AUDIT*.md`.
- The retained HIL summary implied v3.0.1 changed only non-driver surfaces. It
  now accurately says the physical campaign covers the v3.0.0 surface and does
  not validate later driver/API/tooling/harness changes.

The untracked `.pio` scratch artifacts called out by the audit were deliberately
left untouched because they are local ignored build data.

## Code findings

| Finding | Verdict | Resolution |
|---|---|---|
| C1 | Valid, high severity | Implemented the proposed ownership split. Ordinary jobs and the generic EEPROM queue now each own a fixed `PersistentOp`; queue polling never assigns or clears `_job`. Completed job status and typed result evidence remain available until another ordinary job is admitted. The public `isJobBusy()` retains its prior combined active-work behavior; internal polling uses a separate ordinary-job predicate. |
| C2 | Valid, high severity; proposal covered only one transition direction | Added trailing, source-compatible `BackupChargePolicy` parameters to both BSM and TCM setters. The safe default rejects every resulting Direct/Level plus nonzero-TCM combination; `ALLOW_BACKUP_CHARGING` is the explicit rechargeable-source opt-in. |
| C3 | Valid | Raised snapshot/set defaults to 200/700 ms. These cover the complete accepted 1..100 ms callback-timeout range; no-clock tests execute both operations at the accepted 100 ms maximum. |
| C4 | Valid, high severity; proposed reserve incomplete | Added a sticky cached `persistentAccessStateUnproven` condition, blocked new persistence and primary ensure while it is set, and added an explicit cooperative recovery job. Recovery waits for EEbusy, verifies caller-selected implemented C0, clears/verifies EERD while preserving other Control 1 bits, honors BSM activation settle, and issues no EEPROM command. Follow-up review closed false-negative paths for pre-existing EERD and failed primary-ensure cleanup, while proven cleanup no longer latches recovery merely because an earlier callback failed. The write cutoff now reserves the actual post-WRITE_ONE chain: write settle, configured busy window, twelve fixed proof callbacks, two bounded READ_ONE poll windows, and final cleanup. The audit's illustrative fourteen-callback-only formula omitted bounded poll windows and cleanup timing. |
| C5 | Valid | Added the guard once in the synchronous write-register owner. Blocking reads remain available under the documented application serialization contract; synchronous writes return `BUSY` during ordinary, queued, or active generic cooperative work. |
| C6 | Valid; proposed global rejection too disruptive | Added `Config::primaryCellI2cTimeoutMs` with an explicit 1..5 ms range. Primary ensure uses it directly; ordinary `i2cTimeoutMs` remains 1..100 ms, so default configuration and HIL integrations are not broken by a hidden clamp or a new global 5 ms limit. |

### C7 dispositions

| Item | Verdict and action |
|---|---|
| C7.1 | Valid. Cleanup is marked proven immediately after Control 1 readback. A later settle expiry is a plain timeout and cannot erase cleanup evidence. |
| C7.2 | Valid. One subtraction-based `validRegisterSpan()` avoids unsigned-addition overflow and is reused by block and password-range checks. |
| C7.3 | Already documented and tested. The 16-byte synchronous user-RAM call explicitly documents/returns `IN_PROGRESS`; no contract change was needed. |
| C7.4 | Not a defect under this repository's binding architecture. Fixed, acyclic, no-wait jobs may use the derived transfer-count × per-transfer bound. Making all of them deadline state machines would add API/state without improving boundedness. |
| C7.5 | Valid latent risk. Mutation sites now honor the active flag through one boundary helper instead of interpreting an inactive cutoff value. |
| C7.6 | Valid. Disabled alarm-date fallback is `0`, matching the reset encoding and round-trip contract. |
| C7.7 | Valid. Verified calendar readback checks weekday, including same-day +1 second and midnight weekday rollover. |
| C7.8 | Valid. Generic WRITE_ONE poll capacity derives from `eepromTimeoutMs + 1`; the fixed primary-cell cap remains independent. |
| C7.9 | Valid, but the audit's contiguous C0..EA range would include password bytes C6..CA. Validation now accepts only C0..C5 or CB..EA. |
| C7.10 | Valid. Public docs state the hardware century limitation; out-of-domain weekday input has a distinct message from an invalid Gregorian date. |
| C7.11 | Valid. The unreachable health-state branch was removed. |
| C7.12 | Valid. Build literals use fixed-layout digit parsing and the core no longer includes stdio. |
| C7.13 | Valid ABI concern. Every `Err` ordinal is explicit; reserved public values stay in place. The unused backup timestamp field was removed. Persistent queue state was simplified as part of C1. |

## Examples and tests

| Finding | Verdict | Resolution |
|---|---|---|
| E1 | Valid | On short staging, the ESP32 adapter calls `Wire.flush()` to discard staged payload, then performs the address-only STOP/owner release required by Arduino-ESP32. The stub now distinguishes API calls from physical attempts and proves no partial register payload is sent. |
| E2 | Valid smell, but the proposed cross-core macros do not match this repository's target | Replaced 128 with a named conservative 32-byte ESP32 bound. All driver transfers are smaller. |
| E3 | Incorrect for the pinned Arduino-ESP32 core | No removal. In that implementation `endTransmission(false)` stages state and retains the lock; `requestFrom()` performs the combined physical transaction. The cleanup `endTransmission(true)` is necessary only on pre-request failure to release that state. |
| E4 | Valid | The adapter header now explicitly scopes itself to Arduino-ESP32 on ESP32-S2/S3. A generic fallback would falsely imply timeout-equivalent portability. |
| E5 | Robustness observation, proposed fix unsafe | No watchdog was added. Dropping the CLI owner after 15 seconds would orphan still-active driver work and permit overlapping commands. All current driver work is bounded and continuously polled; a future cancel feature would need an explicit driver abandonment/rebind contract. |
| E6 | Valid | The fake raises EEF, records a protocol violation, and stores nothing for indirect password commands. Driver validation independently excludes the password span. |
| E7.1 | Valid | Added zero-budget EEPROM polling coverage. |
| E7.2 | Valid | Added destructive primary write/verify failure-stage assertions. |
| E7.3 | Partly stale | No-clock coverage already existed in narrower cases; added the missing documented-default combination. |
| E7.4 | Overstated | Two fixture-only tests, not three, were renamed to make their role explicit. |
| E7.5 | Observation only | Existing targeted latency/deadline tests already use nonzero callback durations. They were retained and extended around the corrected cutoff. |
| E7.6 | Valid | Scanner timeout/error rendering branches now have queued-result coverage. |
| E8 | Valid | Kept the established encoding and made its domain explicit: the generator rejects minor/patch >99 and components that exceed generated storage. This is simpler and avoids an unnecessary ABI encoding change. |

The minor CLI items were all valid. Whitespace-only lines repaint the prompt;
user-EEPROM copy bounds are checked locally; user-RAM confirmation diagnostics
are specific; CLKOUT frequency names use a guarded switch; and help text now
states the intentionally narrow verbose-command scope rather than advertising a
global behavior.

## Tooling and documentation findings

The tooling criticism was valid. The three 1,481-line exact-text/source-order
checkers were replaced with focused checks:

- `check_portability.py`: platform-call bans, transport-health owner, timed job
  I/O, example parser/owner bans, and EEPROM-command safety;
- `check_abi.py`: the complete public error table and generated version
  agreement; and
- `check_package.py`: source/package contents, Doxygen/package settings,
  workflow artifact hygiene, one stable Status-write safety phrase, and
  destructive-HIL authorization.

CI, README, and CONTRIBUTING now invoke those checks. Behavior-specific
invariants remain in native tests and compilation, where ordinary refactoring
does not break them.

The documentation-duplication section is a sound maintenance recommendation,
not a current correctness finding. A wholesale consolidation was not made in
this audit fix because the README integration path, architecture rationale,
public-header contracts, and binding `AGENTS.md` instructions serve different
audiences, and several repeated safety warnings are intentionally local to the
API that can cause the hazard. Contradictory or stale wording found during this
review was corrected; broader editorial restructuring should be a separate,
reviewable documentation change.

## Additional corrections found during verification

- The generated version-code contract is now enforced by the generator.
- New `Config` and `SettingsSnapshot` fields are trailing so existing
  positional aggregate prefixes retain their meaning.
- Generic persistence always clears EERD after access instead of preserving a
  pre-existing leaked EERD bit.
- Primary ensure now rejects a sticky unproven access state with zero I/O and
  sets that latch when its own cleanup proof fails. A later activation-settle
  timeout does not erase already-proven C0/Control 1 cleanup.
- The current API additions require a backward-compatible minor version bump;
  the source of truth is now 3.1.0 and `Version.h` was regenerated.

## Verification

All device-free gates passed on the synchronized audit worktree:

- `.\scripts\pio.cmd test -e native` â€” 118/118 cases passed.
- `.\scripts\pio.cmd run -e esp32s3dev` â€” passed with pinned platform
  55.3.311 and Arduino-ESP32 3.3.11.
- `.\scripts\pio.cmd run -e esp32s2dev -e esp32s3hil -e
  esp32s3hil_persistence` â€” all three compile targets passed.
- `python tools/check_portability.py`, `python tools/check_abi.py`,
  `python tools/check_package.py source`, and
  `python scripts/generate_version.py check` â€” passed.
- `python tools/hil_cli_runner.py --parser-self-test` â€” passed; `--dry-run`
  produced the expected 26-step plan.
- `doxygen Doxyfile` â€” passed with Doxygen 1.13.2 and warnings-as-errors.
- `.\scripts\pio.cmd pkg pack -o dist\RV3032-C7-3.1.0-audit-final.tar.gz .`
  followed by `python tools/check_package.py package ...` â€” archive contract
  passed.
- `git diff --check` â€” passed.

No physical HIL was claimed; the two HIL environments were compile-checked.
