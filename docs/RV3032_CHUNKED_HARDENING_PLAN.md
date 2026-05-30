# RV3032-C7 Chunked Hardening Plan

Branch: `hardening/rv3032-industry-readiness`

## Purpose

This document tracks the chunked industry-readiness hardening workflow for the RV3032-C7 RTC library. The work is intentionally split into bounded prompts. Each chunk must finish with validation, a commit, a push or explicit sync failure report, and then stop until the next prompt.

This plan was created by Prompt 00. Prompt 00 is documentation-only and does not authorize functional driver changes.

## Repository Guardrails

- Work only in the RV3032-C7 repository.
- Keep public library headers under `include/RV3032/` and implementation under `src/`.
- Treat `examples/common/` as example-only project glue, not library API.
- Keep core library code framework-neutral: no Arduino, Wire, ESP-IDF, FreeRTOS, logging framework, global bus objects, or framework-owned delays.
- Keep I2C transport injected and non-owning. Application code owns bus handles, pins, locks, and timeout policy.
- Keep fallible APIs on `Status`; do not use exceptions.
- Do not claim hardware, ESP-IDF, or fault-path validation unless it was actually run.
- Do not edit generated `include/RV3032/Version.h` manually.

## Per-Chunk Workflow

1. Re-read `AGENTS.md` and this plan.
2. Confirm repository root, current branch, and working tree status:

   ```bash
   git rev-parse --show-toplevel
   git status --short
   git branch --show-current
   ```

3. If the worktree is dirty before the chunk starts, stop and report the uncommitted files.
4. Use `hardening/rv3032-industry-readiness` for all chunks.
5. Do only the current chunk's requested work.
6. Spawn focused subagents when useful. Each subagent should return factual findings before implementation decisions are finalized.
7. Run the relevant available checks for the chunk.
8. Update or create a concise chunk report under `docs/`.
9. Commit with a clear message.
10. Push or sync:

   ```bash
   git push -u origin hardening/rv3032-industry-readiness || git push
   ```

11. Report the result and stop. Do not begin the next chunk until explicitly prompted.

## Planned Chunk Sequence

| Chunk | Focus | Notes |
| --- | --- | --- |
| 00 | Workflow, branch, subagents, commit/sync discipline | Documentation only. |
| 01 | Baseline audit, agents, docs, implementation matrix | Establish current gaps without inventing validation. |
| 02 | Core contracts, framework neutrality, timebase, probe errors, copy/move, direct register safety | Core API and safety contracts. |
| 03 | Time, flags, synchronization | STOP-controlled time setting, coherent reads, validity/fault flags. |
| 04 | EEPROM, alarm, timer, interrupts | EEPROM command APIs, dirty state, timer/alarm/event/interrupt semantics. |
| 05 | ESP-IDF, CI, examples, docs | Native ESP-IDF component/example and build coverage. |
| 06 | Hardware validation and release readiness | Validation matrix and release/readiness report. |

If a later chunk depends on a design choice from an earlier chunk, do not silently redesign previous work. Document the conflict, make the smallest clean adjustment, commit it, and report the change.

## Subagent Model

Use these roles when useful:

- `rv3032-datasheet-agent`: checks local datasheet/app manual material for registers, flags, timing, STOP/ESYN, EEPROM commands, alarms, timers, events, timestamps, and backup behavior.
- `core-contracts-agent`: checks framework neutrality, transport/timebase injection, error propagation, health state, dirty state, copy/move safety, thread/ISR safety, and heap/logging/framework leakage.
- `rtc-semantics-agent`: checks clock/calendar coherence, write synchronization, hundredths/seconds behavior, rollover, leap-year validity, voltage/fault flags, interrupts, timestamps, and backup switchover semantics.
- `eeprom-agent`: checks user EEPROM access through `EEADDR`, `EEDATA`, `EECMD`, including `EEBUSY`, `EEF`, timeouts, partial writes, write protection, and documentation.
- `espidf-ci-agent`: checks pure ESP-IDF component/example coverage, Arduino leakage, and ESP32-S2/S3 build commands or CI.
- `test-fault-agent`: checks fake-transport tests for failures, timeouts, NACKs, bus errors, register bit writes, dirty state, and boundary cases.
- `integration-review-agent`: reviews the final chunk diff for scope control, hidden coupling, framework leakage, untested public behavior, and false validation claims.

## Prompt 00 Status

- Repository root confirmed: `C:/Users/HonzovoSpectre/Documents/Projects/RV3032-C7`.
- Branch confirmed: `hardening/rv3032-industry-readiness`.
- Initial worktree status: clean.
- Subagents used:
  - `core-contracts-agent`: found that `AGENTS.md` already contained production principles but lacked explicit subagent roles and chunked commit/sync/stop discipline.
  - `integration-review-agent`: found that this plan file did not exist, related hardening reports were present, and cheap validation commands/scripts are available.
- Scope: documentation only.
- Validation run:
  - `git status --short`: showed only `AGENTS.md` and this plan file changed.
  - `python tools/check_core_timing_guard.py`: passed.
  - `python tools/check_cli_contract.py`: passed.
  - `python scripts/generate_version.py check`: passed; `include/RV3032/Version.h` was up to date.
  - `python -m platformio test -e native`: passed; 34/34 test cases succeeded.
- Hardware validation: not run.
- ESP-IDF build validation: not run for this documentation-only chunk.
