# RV3032-C7 documentation

This directory contains maintained integration and verification guides plus
the vendor PDFs. Public headers define API contracts; the root changelog
records release history.

## Guides

| Document | Purpose |
|---|---|
| [Library README](../README.md) | Integration sketch, capabilities, support boundaries, and release status. |
| [Changelog](../CHANGELOG.md) | Published release history and changes under Unreleased. |
| [Architecture](ARCHITECTURE.md) | Ownership, lifecycle, health, cooperative execution, timing, and persistence. |
| [Device reference](DEVICE_REFERENCE.md) | Register behavior, flags, memory, EEPROM protocol, and electrical prerequisites. |
| [ESP-IDF integration](IDF_PORT.md) | Native component, S2/S3 bring-up example, adapter error mapping and timing limits. |
| [Verification](VERIFICATION.md) | CI commands, HIL surfaces, restoration procedures, and evidence limits. |

Run `doxygen Doxyfile` from the repository root to generate
`docs/doxygen/html/index.html`. The API reference includes the root README,
changelog, contributor guide, maintained technical guides, and public headers.
Generated output is ignored by Git and excluded from the library package.

## Vendor references

- [RV-3032-C7 datasheet](https://github.com/janhavelka/RV3032-C7/blob/main/docs/reference-pdfs/RV-3032-C7_datasheet.pdf)
- [RV-3032-C7 Application Manual](https://github.com/janhavelka/RV3032-C7/blob/main/docs/reference-pdfs/RV-3032-C7_App-Manual.pdf)

These Micro Crystal documents are retained under `docs/reference-pdfs/` for offline traceability
and remain authoritative for electrical limits, layout, qualification, and
application circuits. The device reference identifies the manual revision used
by the driver. PDFs are excluded from the PlatformIO package.

## Keeping documentation current

Keep reusable contracts, procedures, and limitations in these guides. Record
user-visible changes under Unreleased in the changelog and update public
headers when an API contract changes. Earlier audits and campaign reports
are available in Git history. Private HIL artifacts default to `.pio/hil-runs/`.

The three completed native ESP-IDF prompts are archived in Git history.
[Archived ESP-IDF requirements](https://github.com/janhavelka/RV3032-C7/tree/3c894cc84640aadbe9ea0948ae3c3db355f52d16/docs/prompts)
