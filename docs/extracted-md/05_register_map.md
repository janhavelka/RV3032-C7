# Register Map

Register groups:

| Address range | Group | Notes | Source |
|---:|---|---|---|
| `0x00` | 100th Seconds | Read-only sub-second counter. | application manual, p. 14 |
| `0x01-0x07` | Clock/calendar | Seconds, minutes, hours, weekday, date, month, year. | application manual, p. 14 |
| `0x08-0x0A` | Alarm | Minute/hour/date alarm registers. | application manual, pp. 14, 20 |
| `0x0B-0x0C` | Countdown timer value | 12-bit timer value split across two registers. | application manual, pp. 14, 21 |
| `0x0D` | Status | Flags: THF, TLF, UF, TF, AF, EVF, PORF, VLF. | application manual, pp. 14, 22 |
| `0x0E-0x0F` | Temperature and EEPROM/clock flags | TEMP low bits plus EEF, EEbusy, CLKF, BSF; TEMP high byte. | application manual, pp. 14, 23 |
| `0x10-0x12` | Control 1-3 | STOP, interrupt enables, timer/event controls, EEPROM read trigger. | application manual, pp. 14, 25-27 |
| `0x13-0x17` | Timestamp/event/temperature threshold controls | Timestamp overwrite and threshold registers. | application manual, pp. 14, 28-31 |
| `0x18-0x2D` | Timestamp data | TLow, THigh, and EVI timestamp counters/time fields. | application manual, pp. 14-15, 32-41 |
| `0x39-0x3C` | Password | Write-only password bytes. | application manual, p. 15 |
| `0x3D-0x3F` | EEPROM access | EEADDR, EEDATA, EECMD. | application manual, p. 15 |
| `0x40-0x4F` | User RAM | 16 bytes volatile user RAM. | application manual, p. 15 |
| `0xC0-0xCA` | Configuration EEPROM RAM mirror | PMU, offset, CLKOUT, temp reference, EEPROM password, password enable. | application manual, p. 16 |
| `0xCB-0xEA` | User EEPROM | 32 bytes nonvolatile user EEPROM. | application manual, p. 13 |

The application manual marks gaps outside the listed groups as reserved; it does not assign functions or reset behavior to those addresses. Source: application manual, pp. 15-16.

CLKOUT spans three configuration-mirror bytes rather than one contiguous
CLKOUT-only block: C0 owns PMU.NCLKE, C2 owns HFD[7:0], and C3 owns OS, FD, and
HFD[12:8]. Their factory-delivery EEPROM values are all `0x00`, selecting
direct XTAL 32.768 kHz output while retaining HFD=0 as the inactive 8.192 kHz
HF selection. C1 is the independent offset byte. Sources: application manual,
pp. 45, 47, 54, 56, 62-63.
