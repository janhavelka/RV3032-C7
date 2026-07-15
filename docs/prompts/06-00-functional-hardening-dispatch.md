# AI Coder Prompt Suite 06: RV3032-C7 Functional Hardening Dispatch

## Purpose

This file defines the dispatch order for the implementation prompts derived
from the
[full functional audit](../reports/2026-07-14-full-library-functional-audit.md).
The audit was performed against revision `0e2eb0d`, manifest version `2.0.0`.

Keep the full audit report as the engineering source of truth. Execute the
implementation in phases so transport, lifecycle, and state-machine contracts
are stable before dependent work is changed. Each phase prompt repeats all
contracts needed for that phase and can be pasted into an AI-coder chat as a
standalone assignment.

## Required order

| Order | Prompt | Findings and responsibility |
| ---: | --- | --- |
| 1 | [`06-01-functional-hardening-core-safety.md`](06-01-functional-hardening-core-safety.md) | Password deletion; H-01 through H-04; M-03; L-02 through L-04; transport, deadlines, TEMP_LSB, lifecycle, conversions, dead state, and health |
| 2 | [`06-02-functional-hardening-cooperative-jobs.md`](06-02-functional-hardening-cooperative-jobs.md) | H-05; M-01, M-02, M-04, M-05, M-06; backup, staged configuration, persistence, quiescence, and `tick()` |
| 3 | [`06-03-functional-hardening-integration-docs.md`](06-03-functional-hardening-integration-docs.md) | M-07 through M-09; L-01 and L-05; Wire owner, CLI, examples, public presence semantics, maintained documentation, and versioning |
| 4 | [`06-04-functional-hardening-final-audit.md`](06-04-functional-hardening-final-audit.md) | Independent closure audit of every finding, complete device-free verification, package verification, and final evidence |

Run these prompts sequentially on the same worktree. Do not run implementation
phases concurrently. A later phase must audit the earlier phase's exit criteria
before it edits dependent code. If the current HEAD differs from the audited
revision, reconcile the intervening changes against the findings before
implementation; never reset or overwrite user work.

## Dispatch rules

1. Give the AI coder one complete phase file, not an extracted section.
2. Require it to read `AGENTS.md`, the full phase prompt, the referenced local
   vendor material, and the current affected code before editing.
3. The primary coder must spawn bounded subagents for independent inspection
   and post-change audit as specified in that phase. Subagents provide
   evidence; the primary coder retains design judgment, integrates the work,
   inspects the complete diff, and fixes confirmed defects.
4. Prefer deletion and refactoring of the existing owner over local patches.
   Do not accept compatibility shims, password stubs, duplicate schedulers,
   parallel state machines, hidden retries, or speculative abstractions.
5. Keep each phase buildable and tested. Do not continue when an earlier phase
   has an unresolved contract conflict.
6. Do not commit, tag, publish, flash hardware, or claim physical HIL without
   explicit user authorization.

## Product decision

Password authentication and password-protection management are intentionally
unsupported. Phase 1 deletes the capability instead of repairing or
deprecating it. The complete vendor register reference remains, and raw access
to the password ranges fails before transport I/O. Later phases must not
reintroduce credentials, password state, unlock flows, compatibility methods,
or a feature flag.

## Final boundary

Passing an intermediate phase does not establish production readiness. Only
Phase 4 evaluates the complete definition of done. Software tests do not prove
backup retention, electrical timing, voltage safety, or physical INT/CLKOUT
behavior. Unless separately authorized and executed, physical HIL must be
reported as `NOT RUN`.
