# Initialization, Reset, And Operational Notes

Recommended bring-up sequence:

1. Confirm I2C address `0x51` responds after POR timing. This communication
   evidence does not prove chip identity or that the longer configuration
   EEPROM refresh has completed.
2. Read `0x0D`/`0x0E`; if `PORF`, `VLF`, `EEF`, or `EEbusy` are set, handle them before trusting time/configuration state.
3. If CLKOUT is unused, disable it to reduce current (`NCLKE = 1` and `CLKF = 0` per application guidance).
4. If EVI is unused, ensure the board ties it to a defined level; do not leave it floating.
5. Set clock/calendar registers with STOP/control sequencing according to the time-setting section.
6. Configure interrupts and clear pending flags before enabling host interrupt handling.

Sources: application manual, pp. 66-73, 140.

EEPROM and configuration notes:

- Configuration registers are EEPROM-backed and mirrored in RAM. A RAM change is not necessarily persistent until the EEPROM update procedure is run. Source: application manual, pp. 13, 16, 68-72.
- The POR refresh copies stored configuration EEPROM into C0..CA in
  approximately 66 ms. With EERD=0, the date-increment refresh can later
  replace active-only changes. An EEPROM-only WRITE_ONE is durable but does
  not itself activate a different mirror value. Source: application manual,
  pp. 68-72.
- CLKOUT persistence consists of C0 for NCLKE plus C2/C3 for OS, FD, and HFD.
  CLKIE, CLKF, Clock Interrupt Mask, and CLKDE are separate active controls or
  evidence. Source: application manual, pp. 23, 26, 29-30, 45, 47, 62-65.
- Check `EEbusy` before EEPROM access and `EEF` after failure-prone EEPROM operations. Source: application manual, pp. 70-71.
- The silicon password mechanism can write-protect time, control, and
  configuration registers. This library deliberately does not manage or unlock
  password protection and rejects raw access to those ranges. Source:
  application manual, pp. 11, 42, 50-51.
- Any single I2C communication from START to STOP must complete within 950 ms or the internal timeout resets the interface. Source: application manual, p. 139.
