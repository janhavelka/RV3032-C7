# RV3032-C7 v3 Functional-Hardening Closure Audit

The complete device-free v3 hardening implementation closes every audited
finding H-01 through H-05, M-01 through M-09, and L-01 through L-05. The
closure is based on traced terminal paths, negative-path tests, source-contract
guards, maintained documentation, target builds, and package inspection. It is
not a physical-retention, electrical-waveform, or real-bus timing claim.

Authority was checked in this order: `AGENTS.md`, the local Application Manual
Rev. 1.3 and datasheet, the 2026-07-14 full audit, and Prompt Suite 06. Manual
pages 23, 42, 50-51, 59-60, 70-71, 78, 81-82, 84, 87, and 96 were visually
revalidated. No vendor or authority contradiction was found.

## Requirement-to-evidence closure matrix

| ID | Final behavior and owner | Decisive evidence | State |
| --- | --- | --- | --- |
| H-01 | `TimedTransferResult`, `readRegsBefore()`, and `writeRegsBefore()` bind callback completion to wrap-safe exclusive deadlines; ambiguous writes enter bounded reconciliation and are not replayed. | `test_timed_callback_completion_clipping_no_hook_and_wrap`, `test_ordinary_deadline_states_are_exclusive_at_boundary_and_late`, persistent cutoff/deadline tests, and `check_core_timing_guard.py`. | CLOSED |
| H-02 | The sole `TEMP_LSB_FLAG_CLEAR` owner uses W0C-safe target payloads and never writes a stale full snapshot. | `test_temp_lsb_flag_clear_uses_race_safe_fixed_payloads` injects every target/neighbor race and checks `0x09`, `0x03`, and `0x0A`. | CLOSED |
| H-03 | `RV3032::end()` always calls `_resetRuntimeState()` with zero I/O, abandoning every active/queued owner locally. | `test_end_unconditionally_abandons_work_with_zero_io` and `test_tick_zero_budget_and_eeprom_end_guards` cover ordinary/EEPROM ownership, repeated-end, rebind, health, and latch state. | CLOSED |
| H-04 | `RV3032` is explicitly noncopyable and nonmovable. | Four compile-time constructible/assignable trait assertions in `test_datetime.cpp`. | CLOSED |
| H-05 | The cooperative backup owner explicitly encodes BSM, guards BSIE, verifies PMU, waits 2/10 ms from write completion, and reconciles without a second write. | Backup activation/admission tests plus `test_phase2_backup_fault_timing_and_evidence_matrix`, including callback-OK/noncommit, late requested/original/unknown, exact mismatch evidence, and one-ms-early zero-I/O waits. | CLOSED |
| M-01 | Timer, periodic, backup, CLKOUT, and temperature owners return exact first-failure `ConfigurationJobReport` evidence and prove requested, safe-disabled, unchanged, or unknown state within fixed caps. | Staged ordinal/cleanup matrices and `test_phase4_staged_safe_payloads_preserve_neighbor_bits` prove exact preserved safe payloads, already-safe no-cleanup-write behavior, and unsafe cleanup writes. | CLOSED |
| M-02 | Typed and generic persistence retain operation status, durable proof/counts, and cleanup status independently with cleanup-failure precedence. Entering cleanup latches mandatory access-state proof. | Typed mixed-failure and generic batch precedence/reset matrices plus `test_phase4_late_cleanup_write_requires_semantic_cleanup_failure` for late and non-dispatched typed/generic restore paths. | CLOSED |
| M-03 | Transport callbacks have a closed six-code domain; invalid callback statuses normalize to `TRANSPORT_CONTRACT_VIOLATION`, and health changes only after actual callbacks. | `test_transport_status_domain_and_health_attempt_evidence` injects driver-control, local, and unknown codes through both callback paths. | CLOSED |
| M-04 | `Status tick(uint32_t)` returns the exact one-instruction `pollEeprom()` result and preserves cached terminal failure. | `test_phase2_quiescence_uie_and_tick_contracts`. | CLOSED |
| M-05 | `updateEventEnabled=false` means UIE=0, so the fake creates neither UF nor INT and there is no UF-polling-only mode. | Fake `triggerPeriodicUpdateEvent()` and periodic staged/UIE tests. | CLOSED |
| M-06 | TIE/AIE/EIE/BSIE quiescence reads return `BUSY` with zero writes; the shared register guard does not apply EIE to CLKDE. | Quiescence busy/cap and EVI-reset no-replay matrices. | CLOSED |
| M-07 | The example Wire adapter applies one complete callback deadline, restores prior timeout by RAII, closes short staging with one bounded STOP, and reports initialization failure. | Three Phase 3 Wire validation/deadline/release/init native tests. | CLOSED |
| M-08 | One strict token parser and one `PendingOperation` owner reject malformed/trailing input before I/O and retain ownership through optional EEPROM work. Timer CLI/HIL input now enforces the public 1..4095 range. | Strict numeric, overlength discard, invalid zero-I/O, owner-handoff, and persistent-helper native tests; `check_cli_contract.py`. | CLOSED |
| M-09 | `isOnline()` is deleted; READY is passive observational health and raw `probe()` is one untracked address-communication read, not identity evidence. | `test_passive_lifecycle_and_explicit_probe_evidence`, deleted-symbol scans, Doxygen and maintained docs. | CLOSED |
| L-01 | RAM writes print only a terminal result, empty timestamps print empty/unset, and maintained polling snippets sample time before use. HIL parsing requires terminal RAM success rather than acceptance text. | CLI RAM/timestamp native test, compile-only README snippets, HIL parser self-test acceptance-only negative case. | CLOSED |
| L-02 | Public conversions are checked, public BCD utilities are absent, Gregorian/Unix bounds are exact, and failure preserves outputs. | `test_static_calendar_and_status_utility_coverage` covers invalid domains and both Unix endpoints. | CLOSED |
| L-03 | `recover()` maps address NACK before the sole health update; cached error agrees, and unsigned lifetime counters wrap. | Presence/health tests, no-saturation source scan, and test-local `UINT32_MAX + 1 == 0` static assertion. | CLOSED |
| L-04 | Dead states/fields are deleted; impossible ordinary/persistent states route to `INTERNAL_STATE_ERROR` and existing bounded cleanup owners. | Forbidden-symbol scans and `check_core_timing_guard.py` source-contract reachability checks. | CLOSED |
| L-05 | Weekday is documented as user-assigned binary 0..6, and every simple Status clearer documents the unavoidable THF/TLF side effect and post-guard race. Verified calendar clearing is documented separately. | Public Doxygen inspection and `check_docs_contract.py source`. | CLOSED |

Password management types, APIs, states, fake authentication, and telemetry
are absent. Vendor password constants remain. Every public address and
intersecting block in `0x39..0x3C` and `0xC6..0xCA` fails before I/O through
the public allowlist, and the shared private validators retain the narrower
`"Password registers are unsupported"` fail-closed guard. The managed
`0x3D..0x3F` EEPROM control path, configuration EEPROM `C0..C5`, and typed
user EEPROM `CB..EA` remain available. Table-driven native tests cover these
boundaries.

## Corrections made by the closure audit

- Added missing callback-OK/noncommitted and late requested/original/unknown
  backup reconciliation evidence, including the failed-write activation wait
  boundary and zero-callback wait polls.
- Added nontrivial neighbor-bit tests for exact timer, periodic, CLKOUT, and
  temperature safe-gate payloads, already-safe cleanup elision, unsafe cleanup
  writes, terminal state, and fixed callback caps.
- Corrected the CLI/HIL timer contract from the impossible `ticks=0` path to
  the public `1..4095` range, with invalid-input zero-I/O evidence.
- Strengthened HIL RAM-write expectations and parser self-test so acceptance
  text cannot masquerade as terminal success.
- Fixed persistent cleanup ownership so a late or non-dispatched restore after
  a forward failure returns semantic `EEPROM_CLEANUP_FAILED` while retaining
  the exact operation and cleanup causes. Typed and generic regressions cover
  original EERD=1, callback-overrun, and hard no-dispatch boundaries.
- Added this durable Phase 4 closure record and made the documentation/package
  contract require it.

The independent reviews found and the primary audit reproduced the persistence
cleanup-precedence defect above. No other core production defect was confirmed.
The extra private `clkoutLegacyFrequency` flag was reviewed and retained: it is
small scratch for the retained public `setClkoutFrequency()` API inside the
same CLKOUT owner, not duplicate state machinery. Replacing it with a sentinel
would weaken clarity. Compiler-macro failure branches in `parseBuildTime()` and
impossible state routing remain source-traced rather than exposed through
production test mutators.

## Device-free verification

- `python scripts/generate_version.py check` - PASS, generated version 3.0.0.
- `python tools/check_core_timing_guard.py` - PASS.
- `python tools/check_cli_contract.py` - PASS.
- `python tools/check_docs_contract.py source` - PASS.
- `pio test -e native` - PASS, **109/109**.
- `pio run -e esp32s2dev` - PASS; RAM 37,192 / 327,680 bytes, flash 455,069 / 1,310,720 bytes.
- `pio run -e esp32s3dev` - PASS; RAM 22,792 / 327,680 bytes, flash 466,170 / 1,310,720 bytes.
- Phase-owned `dist/RV3032-C7-phase4-audit.tar.gz` - packed, content-inspected, and `check_docs_contract.py package` PASS; no test tree, build cache, credentials, or generated debris was included. The artifact was removed after inspection.
- `python tools/hil_cli_runner.py --parser-self-test` - PASS.
- `python tools/hil_cli_runner.py --dry-run` - PASS, 26 planned device-free steps.
- Mandatory deleted-symbol, architecture, CLI, EEPROM-command, and contract-consistency scans - inspected with expected no-match/reference-only results.
- `git diff --check` - PASS (Git emitted only its CRLF conversion warning).

Physical HIL status is **NOT RUN**. Native and fake-bus evidence does not prove
backup retention, backup-cell/topology safety, INT/CLKOUT electrical behavior,
real-device EEPROM cleanup under voltage faults, or physical adapter timing.
Those remain explicit field-validation gates requiring fresh hardware
authorization.

## Final worktree state

The audit started and ended at HEAD
`76f0d95be782e313aec95161c50880fcbe61103d` on
`develop/functional-hardening-phase1`; the worktree contains the uncommitted
cumulative Prompt Suite 06 implementation and this closure evidence. No commit,
tag, publication, flash, physical EEPROM command, or other hardware action was
performed. Phase-created PDF renders and package artifacts were removed;
pre-existing unrelated files were preserved.
