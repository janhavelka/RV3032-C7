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
| [`reports/HIL_SUMMARY.md`](reports/HIL_SUMMARY.md) | Concise HIL evidence summary. Raw runner artifacts are not retained. |

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
