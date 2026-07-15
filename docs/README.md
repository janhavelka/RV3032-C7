# RV3032-C7 Documentation

This directory contains maintained supporting documentation for the RV3032-C7
library plus vendor reference PDFs. Durable project decisions belong in the
README, CHANGELOG, API headers, tests, or the maintained docs below.

## Maintained Docs

| Document | Purpose |
|---|---|
| [`../README.md`](../README.md) | Public usage, API, behavioral contracts, build, and validation notes. |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Release-facing change history. |
| [`../AGENTS.md`](../AGENTS.md) | Repository engineering rules. |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Driver lifecycle, health model, transport layering, instruction budget, and EEPROM policy. |
| [`DEVICE_REFERENCE.md`](DEVICE_REFERENCE.md) | Device facts used by the driver: I2C address, register map, flags, EEPROM sequence, timing, and implementation notes. |
| [`IDF_PORT.md`](IDF_PORT.md) | ESP-IDF adapter boundary and verification checklist. |
| [`reports/2026-07-14-full-library-functional-audit.md`](reports/2026-07-14-full-library-functional-audit.md) | Active v3 functional-hardening audit authority and finding set. |
| [`reports/2026-07-15-functional-hardening-closure-audit.md`](reports/2026-07-15-functional-hardening-closure-audit.md) | Phase 4 requirement-to-evidence closure audit and device-free verification record. |
| [`reports/2026-07-13-v2.0.0-implementation.md`](reports/2026-07-13-v2.0.0-implementation.md) | Version 2.0.0 audit, capability matrix, and software verification evidence. |
| [`reports/2026-07-14-tunnelmonitor-integration-readiness.md`](reports/2026-07-14-tunnelmonitor-integration-readiness.md) | Additive dependency-candidate compatibility remediation and verification evidence. |
| [`reports/HIL_SUMMARY.md`](reports/HIL_SUMMARY.md) | Concise HIL evidence summary. Raw runner artifacts are not retained. |

## Active Functional-Hardening Prompt Suite 06

The full functional audit and Prompt Suite 06 are the active v3 hardening
authority. Execute the dispatch contract and phases in filename order:

1. [`prompts/06-00-functional-hardening-dispatch.md`](prompts/06-00-functional-hardening-dispatch.md)
2. [`prompts/06-01-functional-hardening-core-safety.md`](prompts/06-01-functional-hardening-core-safety.md)
3. [`prompts/06-02-functional-hardening-cooperative-jobs.md`](prompts/06-02-functional-hardening-cooperative-jobs.md)
4. [`prompts/06-03-functional-hardening-integration-docs.md`](prompts/06-03-functional-hardening-integration-docs.md)
5. [`prompts/06-04-functional-hardening-final-audit.md`](prompts/06-04-functional-hardening-final-audit.md)

The current maintained source includes the Phase 1 core contracts, Phase 2
cooperative-job contracts, Phase 3 integration, and the independent Phase 4
closure audit. The Phase 4 evidence report records the complete device-free
matrix and separates software proof from physical HIL.

## Historical v2 prompts and reports

The v2.0.0 implementation prompt is ordered as one shared contract followed by
five bounded phases. Read the shared contract first and execute the remaining
files in filename order:

1. [`prompts/04-00-shared-contract.md`](prompts/04-00-shared-contract.md)
2. [`prompts/04-01-cooperative-core-architecture.md`](prompts/04-01-cooperative-core-architecture.md)
3. [`prompts/04-02-calendar-and-capability-coverage.md`](prompts/04-02-calendar-and-capability-coverage.md)
4. [`prompts/04-03-generic-eeprom-persistence.md`](prompts/04-03-generic-eeprom-persistence.md)
5. [`prompts/04-04-primary-cell-and-raw-access.md`](prompts/04-04-primary-cell-and-raw-access.md)
6. [`prompts/04-05-native-fake-verification-and-release.md`](prompts/04-05-native-fake-verification-and-release.md)

Prompt Suite 04, Prompt 05, and dated v2 reports are historical where they
conflict with the full functional audit or Prompt Suite 06. These prompts are
implementation workflow artifacts. The public README, API
headers, maintained architecture/device docs, tests, and changelog remain the
authority for shipped behavior.

The focused follow-up is
[`prompts/05-tunnelmonitor-integration-readiness.md`](prompts/05-tunnelmonitor-integration-readiness.md).
It corrects the weekday, typed snapshot, verified Status payload, and semantic
primary-report contracts without moving consumer policy into the library.

## Curated Source Notes

The compact notes in [`extracted-md/`](extracted-md/) preserve source-derived
chip facts and open questions from the original vendor-document review. They
are retained for traceability; maintained behavior should still be documented in
the public API, tests, README, changelog, or maintained docs above.

## Reference PDFs

Vendor PDFs live under [`reference-pdfs/`](reference-pdfs/). They are retained
for traceability and manual review, but are excluded from the normal PlatformIO
package export.

Current references:

- `RV-3032-C7_datasheet.pdf`
- `RV-3032-C7_App-Manual.pdf`

Regenerate raw extracted text from the PDFs only for a focused review. Do not
commit raw page-dump folders as maintained docs.

## Evidence Policy

- Software tests prove driver behavior in fake-bus and build environments only.
- CI builds prove configured PlatformIO targets compile; they do not prove board
  wiring, backup-cell behavior, oscillator accuracy, EEPROM endurance, or
  long-term field stability.
- Hardware validation is captured only as concise maintained evidence. Do not
  commit raw runner JSON, generated step tables, stdout/stderr captures, or full
  serial transcripts.
