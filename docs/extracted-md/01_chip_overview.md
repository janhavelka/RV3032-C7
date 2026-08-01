# Chip Overview

The RV-3032-C7 is a temperature-compensated real-time clock module with an integrated 32.768 kHz crystal, I2C interface, backup switchover support, trickle charger, clock output, alarm/timer/event interrupts, temperature sensor, user RAM, and user EEPROM. Source: datasheet, p. 1; application manual, pp. 8, 11.

Key documented facts:

| Feature | Fact | Source |
|---|---|---|
| I2C address | 7-bit `0x51`; write byte `0xA2`, read byte `0xA3` | application manual, pp. 8, 121-122 |
| I2C speed | Fast-mode up to 400 kHz when VDD >= 2.0 V; 100 kHz limit when VDD >= 1.4 V | application manual, p. 139 |
| Timekeeping voltage | 1.3 V to 5.5 V headline range | datasheet, p. 1; application manual, p. 8 |
| Time accuracy | +/-2.5 ppm from -40 degC to +85 degC; +/-20 ppm from +85 degC to +105 degC | datasheet, p. 1; application manual, p. 11 |
| Temperature sensor | 12-bit readable value, 0.0625 degC/step | application manual, p. 11 |
| Nonvolatile memory | 32 bytes user EEPROM plus configuration EEPROM mirrored into RAM | application manual, pp. 11, 13, 16 |
| Volatile memory | 16 bytes user RAM at `0x40-0x4F` | application manual, p. 15 |

The RV-3032-C7 exposes direct time/calendar, status, EEPROM-control, user-RAM,
and active configuration-mirror registers through the I2C register pointer.
User EEPROM `0xCB..0xEA` is a separate indirect address space selected through
EEADDR/EEDATA/EECMD; direct register addresses `0xCB..0xFF` are reserved.
Source: application manual, pp. 13-16, 68-73.
