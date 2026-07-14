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
| [`reports/2026-07-13-v2.0.0-implementation.md`](reports/2026-07-13-v2.0.0-implementation.md) | Version 2.0.0 audit, capability matrix, and software verification evidence. |
| [`reports/2026-07-14-tunnelmonitor-integration-readiness.md`](reports/2026-07-14-tunnelmonitor-integration-readiness.md) | Additive dependency-candidate compatibility remediation and verification evidence. |
| [`reports/HIL_SUMMARY.md`](reports/HIL_SUMMARY.md) | Concise HIL evidence summary. Raw runner artifacts are not retained. |

## Implementation Prompt Suite 04

The v2.0.0 implementation prompt is ordered as one shared contract followed by
five bounded phases. Read the shared contract first and execute the remaining
files in filename order:

1. [`prompts/04-00-shared-contract.md`](prompts/04-00-shared-contract.md)
2. [`prompts/04-01-cooperative-core-architecture.md`](prompts/04-01-cooperative-core-architecture.md)
3. [`prompts/04-02-calendar-and-capability-coverage.md`](prompts/04-02-calendar-and-capability-coverage.md)
4. [`prompts/04-03-generic-eeprom-persistence.md`](prompts/04-03-generic-eeprom-persistence.md)
5. [`prompts/04-04-primary-cell-and-raw-access.md`](prompts/04-04-primary-cell-and-raw-access.md)
6. [`prompts/04-05-native-fake-verification-and-release.md`](prompts/04-05-native-fake-verification-and-release.md)

These prompts are implementation workflow artifacts. The public README, API
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
