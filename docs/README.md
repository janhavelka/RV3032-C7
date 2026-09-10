# RV3032-C7 documentation

This directory contains the maintained supporting documentation for the
RV3032-C7 library. Normative API behavior belongs in
the public headers; release history belongs in `CHANGELOG.md`.

## Maintained documents

| Document | Purpose |
|---|---|
| [`../README.md`](../README.md) | Public integration guide, safety contract, and verification commands. |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Release-facing change history. |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Lifecycle, ownership, health tracking, cooperative jobs, and persistence policy. |
| [`DEVICE_REFERENCE.md`](DEVICE_REFERENCE.md) | Silicon facts used by the driver: memory, flags, timing, and EEPROM protocol. |
| [`IDF_PORT.md`](IDF_PORT.md) | ESP-IDF transport-adapter boundary and owner-loop guidance. |
| [`reports/HIL_SUMMARY.md`](reports/HIL_SUMMARY.md) | Concise retained hardware-validation evidence and fixture limitations. |

The public API reference is generated with Doxygen from the root README,
changelog, contributing guide, the three maintained technical docs above, and
headers under `include/RV3032/`. Markup errors are treated as build failures.

## Vendor references

The Micro Crystal datasheet and Application Manual Rev. 1.3 are retained in
the repository's
[vendor-reference directory](reference-pdfs/) for offline traceability. They remain the authority for electrical limits,
layout, qualification, and application circuits. PDFs are excluded from the
PlatformIO package.

## Retained working documents

| Document | Purpose |
|---|---|
| [`CODE_AUDIT.md`](CODE_AUDIT.md) | Original point-in-time review; retained for traceability. |
| [`CODE_AUDIT_RESOLUTION.md`](CODE_AUDIT_RESOLUTION.md) | Findings, implementation decisions, and subsequent verification. |

These working documents are excluded from the PlatformIO package and generated
API documentation. They are historical review records, not maintained API contracts.

## Evidence policy

- Native tests prove behavior against the bounded fake transport, not physical
  hardware.
- CI builds prove that the configured PlatformIO targets compile; they do not
  prove wiring, backup-cell behavior, oscillator accuracy, EEPROM endurance,
  or field stability.
- Hardware validation is retained only as a concise summary. Do not commit raw
  runner JSON, generated step tables, PID files, stdout/stderr captures, or
  full serial transcripts.
- Completed prompts remain in Git history. The retained implementation audits
  listed above also remain in this checkout and are not shipped as maintained
  documentation.
