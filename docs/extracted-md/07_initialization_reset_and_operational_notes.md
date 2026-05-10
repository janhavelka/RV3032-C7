# Initialization, Reset, And Operational Notes

Recommended bring-up sequence:

1. Confirm I2C address `0x51` responds after POR timing.
2. Read `0x0D`/`0x0E`; if `PORF`, `VLF`, `EEF`, or `EEbusy` are set, handle them before trusting time/configuration state.
3. If CLKOUT is unused, disable it to reduce current (`NCLKE = 1` and `CLKF = 0` per application guidance).
4. If EVI is unused, ensure the board ties it to a defined level; do not leave it floating.
5. Set clock/calendar registers with STOP/control sequencing according to the time-setting section.
6. Configure interrupts and clear pending flags before enabling host interrupt handling.

Sources: application manual, pp. 66-73, 140.

EEPROM and configuration notes:

- Configuration registers are EEPROM-backed and mirrored in RAM. A RAM change is not necessarily persistent until the EEPROM update procedure is run. Source: application manual, pp. 13, 16, 68-72.
- Check `EEbusy` before EEPROM access and `EEF` after failure-prone EEPROM operations. Source: application manual, pp. 70-71.
- The password mechanism can write-protect time, control, and configuration registers. Source: application manual, pp. 11, 42, 50-51.
- Any single I2C communication from START to STOP must complete within 950 ms or the internal timeout resets the interface. Source: application manual, p. 139.
