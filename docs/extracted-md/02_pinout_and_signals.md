# Pinout And Signals

| Pin | Signal | Direction | Notes | Source |
|---:|---|---|---|---|
| 1 | `VBACKUP` | Power | Backup supply. If backup is not used, do not leave floating; application manual recommends tying to VSS through 10 kOhm to keep test access possible. | datasheet, p. 2; application manual, pp. 10, 140 |
| 2 | `SDA` | I2C data | Open-drain; requires pull-up to VDD. Disabled/high-Z in VBACKUP power state. | datasheet, p. 2; application manual, p. 10 |
| 3 | `INT` | Open-drain output | Interrupt output; can work in VBACKUP state except for external-event function. Pull up as required by the application. | datasheet, p. 2; application manual, pp. 10-11 |
| 4 | `EVI` | Input | External event input. Do not leave floating; external-event function is disabled in VBACKUP state. Do not tie to VBACKUP. | datasheet, p. 2; application manual, p. 10 |
| 5 | `VSS` | Power | Ground. Metal lid is connected to VSS. | datasheet, p. 2 |
| 6 | `VDD` | Power | Main supply. | datasheet, p. 2 |
| 7 | `CLKOUT` | Output | Programmable clock output. Default delivery enables 32.768 kHz; disable if unused to reduce current. Low in VBACKUP state. | application manual, pp. 10, 140 |
| 8 | `SCL` | I2C clock | Open-drain bus with pull-up to VDD. | datasheet, p. 2; application manual, p. 140 |

Board notes: use local decoupling for VDD and VBACKUP where applicable, and keep pull-ups compatible with the active power state. Source: application manual, pp. 140-143.
