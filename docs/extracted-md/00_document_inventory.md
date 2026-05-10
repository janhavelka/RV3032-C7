# Document Inventory

These are curated engineering notes for the RV-3032-C7 RTC module. They are derived from the PDFs under `docs/` and the raw text extraction in `docs/pdf-extracted-md`; raw page dumps are intentionally not repeated in `docs/extracted-md`.

| Source PDF | Raw extract | Pages used | Notes |
|---|---|---:|---|
| `docs/RV-3032-C7_datasheet.pdf` | `docs/pdf-extracted-md/RV-3032-C7_datasheet.md` | 1-2 | Short-form datasheet: features, block diagram, pin list, headline electrical data. |
| `docs/RV-3032-C7_App-Manual.pdf` | `docs/pdf-extracted-md/RV-3032-C7_App-Manual.md` | 1-154 | Primary source for registers, I2C protocol, interrupts, backup, EEPROM, timing, and application notes. |

Compact note set:

| File | Purpose |
|---|---|
| `01_chip_overview.md` | Device capabilities and software-visible scope. |
| `02_pinout_and_signals.md` | 8-pin module signals and board-level notes. |
| `03_electrical_and_timing.md` | Supply, I2C timing, clock accuracy, POR and bus timeout. |
| `04_protocol_commands_and_transactions.md` | I2C address, register pointer, auto-increment, EEPROM commands. |
| `05_register_map.md` | Driver-relevant register groups and key addresses. |
| `06_modes_interrupts_status_and_faults.md` | Status flags, interrupt sources, backup/voltage/POR behavior. |
| `07_initialization_reset_and_operational_notes.md` | Bring-up, time setting, EEPROM update, and low-power notes. |
| `08_variant_differences_and_open_questions.md` | Variant/qualification notes and unresolved choices. |
