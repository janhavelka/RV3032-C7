# Modes, Interrupts, Status, And Faults

Primary status flags live in `0x0D` and selected support flags live in `0x0E`.

| Flag | Meaning area | Source |
|---|---|---|
| `THF` / `TLF` | Temperature high/low threshold event | application manual, pp. 14, 22, 89-94 |
| `UF` | Periodic time update event | application manual, pp. 14, 22, 81-82 |
| `TF` | Periodic countdown timer event | application manual, pp. 14, 22, 77-80 |
| `AF` | Alarm event | application manual, pp. 14, 22, 83-84 |
| `EVF` | External event | application manual, pp. 14, 22, 85-88 |
| `PORF` | Power-on reset event | application manual, pp. 14, 22, 97-98 |
| `VLF` | Voltage low event | application manual, pp. 14, 22, 99-100 |
| `EEF` | EEPROM write access failure | application manual, pp. 14, 70-71 |
| `EEbusy` | EEPROM operation busy | application manual, pp. 14, 70 |
| `CLKF` | Interrupt-controlled CLKOUT flag | application manual, pp. 14, 63-65 |
| `BSF` | Backup switchover flag | application manual, pp. 14, 95-96 |

Interrupt behavior:

- INT is open-drain and can be used for timer, update, alarm, temperature, external event, voltage low, backup switchover, and POR interrupts when enabled. Source: application manual, pp. 73-100.
- INT also works in VBACKUP state except for the external-event function, which is disabled in VBACKUP. Source: application manual, pp. 10-11.
- Interrupts are disabled by default in the application examples; enable only the sources the application services. Source: application manual, p. 140.
