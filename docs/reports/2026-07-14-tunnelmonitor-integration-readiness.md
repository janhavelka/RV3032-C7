# TunnelMonitor integration readiness follow-up — 2026-07-14

## Scope and starting state

This additive report records the focused compatibility correction after the
broad RV3032-C7 2.0.0 hardening. The library remains product-neutral; no
TunnelMonitor source, dependency pin, application policy, bus ownership, or
scheduler was moved into this repository.

- Starting `HEAD`: `ca06b1dd5f8dc9d3d9c358079515599b0b313082`.
- Starting branch: `main`, aligned with `origin/main`.
- Starting `git describe --tags --always --dirty`:
  `v1.6.0-4-gca06b1d`.
- Starting worktree: only the user-owned untracked
  `docs/prompts/05-tunnelmonitor-integration-readiness.md`.
- Declared version: `2.0.0`; no local or remote `v2.0.0` tag was found, so no
  published-contract/versioning conflict was present.
- Native baseline: 79/79 tests passed before source edits.
- Sibling consumer audit: TunnelMonitor was inspected read-only at
  `697cc5133d0c0ff5f858775012d9556109dadddd` and was not modified.

## Four closed compatibility gaps

1. Calendar weekdays are user-assigned. Reads and setters accept `0..6`
   without date agreement, reject malformed/out-of-range encodings, and write
   the supplied valid value. `computeWeekday()` remains an optional utility.
2. `TimeSnapshot` now exposes typed `StatusFlags` decoded from the same first
   Status callback, including authoritative PORF/VLF short-circuit evidence.
3. Verified calendar set writes the named fixed Status payload `0xFC`. This
   clears PORF/VLF, preserves UF/TF/AF/EVF across the cooperative read/write
   race, and retains the unavoidable THF/TLF pre-clear evidence.
4. Primary-cell reports expose `persistentTargetVerified` and
   `activeTargetVerified` at their direct proof points. Evidence survives later
   cleanup/settle failures, while a safe BSM00/TCM00 hold is not mislabeled as
   active-target proof.

## Requirement-to-evidence matrix

| Requirement | Code evidence | Native evidence |
| --- | --- | --- |
| User-assigned weekday `0..6` | `decodeCalendar`, `setTime`, and verified-set admission preserve range-only values | All seven values pass single-burst read, snapshot, set payload, and verified set; `7`, reserved bits, and invalid BCD fail; mismatched verified readback passes |
| Typed snapshot PORF/VLF | `TimeSnapshot::statusFlags` and shared `decodeStatusFlags` | PORF-only, VLF-only, both, neither, first-read failure, result preservation, and exact one-callback polls |
| Exact verified Status payload | `STATUS_CLEAR_INVALID_TIME_VALUE=0xFC` is the sole verified-set payload | All 256 pre-clear bytes emit `0xFC`; each UF/TF/AF/EVF race injection remains set; PORF/VLF and THF/TLF behavior remains exact |
| Semantic primary proof | Report fields are set at direct persistent and active target readbacks | Correct/wrong/ambiguous paths, safe holds, cleanup busy, Control 1 failure, targeted settle timeout, and complete fault-ordinal tables |
| Existing ownership/timing/safety | Cooperative job engine and sole synchronous ensure architecture are unchanged | Existing lifecycle, deadline, instruction-budget, EEPROM/fake, feature, ambiguity, and command-count tests remain green |

## Verification evidence

| Command | Result |
| --- | --- |
| `python -m platformio test -e native` | PASS — 84/84 tests after remediation |
| `python -m platformio run -e esp32s3dev` | PASS — RAM 22,624/327,680 bytes; flash 444,098/1,310,720 bytes |
| `python -m platformio run -e esp32s2dev` | PASS — RAM 37,024/327,680 bytes; flash 433,037/1,310,720 bytes |
| `python scripts/generate_version.py check` | PASS |
| `python tools/check_core_timing_guard.py` | PASS |
| `python tools/check_cli_contract.py` | PASS |
| `python tools/check_docs_contract.py source` | PASS |
| `python tools/hil_cli_runner.py --parser-self-test` | PASS — device-free |
| `python tools/hil_cli_runner.py --dry-run` | PASS — 26 planned steps, no serial port opened |
| `git diff --check` | PASS |
| Temporary package plus `check_docs_contract.py package` | PASS — generated under `.pio/`, inspected, then removed |

PlatformIO reported the pre-existing multiple-core warning (6.1.18 active while
6.1.19 is also installed); it did not affect tests or builds.

## Physical HIL status is **NOT RUN**

No hardware was flashed or contacted. No physical EEPROM command, primary-cell
mutation, voltage/backfeed test, reset/power-cycle, or retention test ran. The
historical HIL summary identifies library 1.5.0 and is not current 2.0.0
primary-cell or retention evidence.

## Intentional omissions and external gates

- No TunnelMonitor source or dependency pin was changed. Its later integration
  still requires a reviewed immutable full RV3032 commit hash.
- No release commit, tag, publication, version bump, upload, or hardware action
  was performed during this implementation pass. This is point-in-time
  evidence, not the current release state.
- Physical VDD, backfeed, boot write count, reset, power-cycle, and retention
  evidence remains a separately authorized external gate.
- TunnelMonitor native/build/HIL validation remains in the consumer repository
  after dependency pinning and adapter integration.

## Final worktree state (implementation pass)

At the end of this implementation pass, the worktree was intentionally
uncommitted. It contained the focused library, fake/test, example,
maintained-documentation, guard, and this additive report changes, plus the
original user-owned untracked Prompt 05 file. Later release-preparation work
does not alter this historical snapshot. Generated PlatformIO build output was
kept under ignored `.pio/`; no package artifact was retained. No TunnelMonitor
file was modified.

- Ending `HEAD`: `ca06b1dd5f8dc9d3d9c358079515599b0b313082`
  (unchanged).
- Ending describe: `v1.6.0-4-gca06b1d-dirty`.
- Modified tracked files: `CHANGELOG.md`, `README.md`,
  `docs/ARCHITECTURE.md`, `docs/DEVICE_REFERENCE.md`, `docs/README.md`,
  `docs/reports/2026-07-13-v2.0.0-implementation.md`,
  `docs/reports/HIL_SUMMARY.md`, `examples/01_basic_bringup_cli/main.cpp`,
  `include/RV3032/CommandTable.h`, `include/RV3032/RV3032.h`,
  `src/RV3032.cpp`, `test/test_native/FakeRv3032.h`,
  `test/test_native/test_datetime.cpp`, `tools/check_cli_contract.py`, and
  `tools/check_docs_contract.py`.
- Untracked files: the original user-owned
  `docs/prompts/05-tunnelmonitor-integration-readiness.md` and this additive
  report.
