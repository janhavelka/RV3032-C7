# Protocol Commands And Transactions

The RV-3032-C7 uses a conventional I2C register pointer model at 7-bit address `0x51`. The full bus bytes are `0xA2` for write and `0xA3` for read. Source: application manual, pp. 8, 121-122.

| Operation | Sequence | Notes | Source |
|---|---|---|---|
| Set pointer/write | START, `0xA2`, register address, data bytes, STOP | Multiple bytes auto-increment. | application manual, pp. 11, 121-126 |
| Random read | START, `0xA2`, register address, repeated START, `0xA3`, read bytes, STOP | Preferred for public register reads. | application manual, pp. 121-126 |
| Sequential read/write | Continue bytes after initial address | Address pointer auto-increments and wraps from `0xFF` to `0x00`. | application manual, p. 11 |
| EEPROM command | Use `EEADDR` (`0x3D`), `EEDATA` (`0x3E`), and `EECMD` (`0x3F`) | EEPROM operations have busy/failure flags. | application manual, pp. 43, 68-71 |

EEPROM/configuration model:

- RAM registers `0x00-0x4F` are directly accessed through the register pointer. Source: application manual, pp. 13-15.
- Configuration EEPROM is mirrored in RAM at `0xC0-0xCA`; updates to persistent EEPROM require the documented EEPROM command flow. Source: application manual, pp. 16, 68-72.
- User EEPROM occupies `0xCB-0xEA` as nonvolatile user memory. Source: application manual, p. 13.
