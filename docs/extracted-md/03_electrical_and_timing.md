# Electrical And Timing

| Parameter | Value | Source |
|---|---:|---|
| Timekeeping voltage | 1.3 V to 5.5 V | datasheet, p. 1 |
| Timekeeping current headline | 160 nA at 3 V | datasheet, p. 1 |
| I2C address | `0x51` 7-bit | application manual, pp. 8, 121 |
| I2C max speed at VDD >= 1.4 V | 100 kHz | application manual, p. 139 |
| I2C max speed at VDD >= 2.0 V | 400 kHz | application manual, p. 139 |
| I2C bus timeout | Communication from START to STOP must complete within 950 ms | application manual, p. 139 |
| Power-on reset time | Typical 6 ms, max 10 ms in power-on AC table | application manual, p. 138 |
| CLKOUT factory-delivery setting | C0/C2/C3=`0x00`: direct XTAL 32.768 kHz enabled; pin LOW in VBACKUP | application manual, pp. 45, 47, 54, 56, 62-65 |
| Configuration POR refresh | Approximately 66 ms; stored EEPROM replaces C0..CA RAM mirrors | application manual, pp. 68-72 |
| Offset correction LSB | 0.2384 ppm per step, range -32 to +31 steps | application manual, p. 118 |

I2C timing highlights at 400 kHz: SCL low min 1.3 us, SCL high min 0.6 us, rise time max 300 ns, fall time max 300 ns, SDA setup min 100 ns, bus-free min 1.3 us. Source: application manual, p. 139.

The short-form datasheet lists I2C interface support at 400 kHz; the application manual qualifies that fast-mode limit with VDD >= 2.0 V. Source: datasheet, p. 1; application manual, p. 139.
