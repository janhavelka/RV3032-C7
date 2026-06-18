# RV3032-C7 Device Reference

This is the maintained device-facing reference for the driver. It records the
device facts the library depends on in one concise document.

Source documents are kept in [`reference-pdfs/`](reference-pdfs/). This file is
not hardware-validation evidence.

## Device Scope

The RV-3032-C7 is a Micro Crystal temperature-compensated real-time clock with
I2C, backup supply switching, alarms, periodic timer, timestamp capture,
temperature readout, EEPROM-backed configuration, 16 bytes of volatile user RAM,
and 32 bytes of user EEPROM.

The library targets the fixed 7-bit I2C address `0x51`.

## Pins And Board Notes

| Pin | Signal | Notes |
|---:|---|---|
| 1 | `VBACKUP` | Backup supply. If backup is unused, do not leave it floating; vendor application guidance recommends tying it to `VSS` through 10 kOhm to keep test access possible. |
| 2 | `SDA` | Open-drain I2C data; requires a pull-up to the active bus rail. High-Z in backup state. |
| 3 | `INT` | Open-drain interrupt output. Pull up according to the host interrupt domain. Can operate in backup state except for the external-event function. |
| 4 | `EVI` | External event input. Do not leave floating and do not tie to `VBACKUP`; EVI capture is disabled in backup state. |
| 5 | `VSS` | Ground; metal lid is connected to `VSS`. |
| 6 | `VDD` | Main supply. |
| 7 | `CLKOUT` | Programmable clock output. Vendor delivery default enables 32.768 kHz; disable if unused to reduce current. Low in backup state. |
| 8 | `SCL` | Open-drain I2C clock; requires a pull-up to the active bus rail. |

Use local decoupling for `VDD` and `VBACKUP` where applicable. Software cannot
validate pull-up sizing, backup-cell installation, oscillator accuracy, or
interrupt wiring.

## Electrical And Timing Notes

| Parameter | Value |
|---|---:|
| Timekeeping voltage range | 1.3 V to 5.5 V |
| Headline timekeeping current | 160 nA at 3 V |
| Time accuracy | +/-2.5 ppm from -40 degC to +85 degC; wider outside that range |
| I2C maximum at `VDD >= 1.4 V` | 100 kHz |
| I2C maximum at `VDD >= 2.0 V` | 400 kHz |
| I2C communication timeout | START-to-STOP must complete within 950 ms |
| Power-on reset time | 6 ms typical, 10 ms max |
| Offset correction step | about 0.2384 ppm |

The short datasheet advertises 400 kHz I2C; the application manual qualifies
that fast-mode limit with the higher `VDD >= 2.0 V` operating condition.

## Bring-Up Checklist

1. Confirm I2C address `0x51` responds after power-on reset timing.
2. Read `0x0D` and `0x0E`; handle `PORF`, `VLF`, `EEF`, or `EEbusy` before trusting time or persistent configuration state.
3. If `CLKOUT` is unused, disable it to reduce current.
4. If `EVI` is unused, tie it to a defined board level.
5. Set clock/calendar registers using the documented control/STOP sequence when the application requires a precise set boundary.
6. Configure interrupts and clear pending flags before enabling host interrupt handling.

## Transport And Timing Facts

- 7-bit I2C address: `0x51`.
- 8-bit address bytes for datasheet comparison: write `0xA2`, read `0xA3`.
- Register reads use a one-byte register pointer followed by a repeated-start
  read.
- The internal register pointer auto-increments after each byte.
- A single register read/write, a contiguous block transfer, and one `EECMD`
  write are each one bus instruction in the driver budget model.
- Do not split a repeated-start register read across polls.

## Register Windows

| Range | Purpose | Driver policy |
|---:|---|---|
| `0x00..0x2D` | RTC, alarm, timer, status, control, timestamp registers | Public helpers and bounded low-level block access. |
| `0x39..0x3F` | Password and EEPROM access registers | Internal EEPROM state machine and guarded low-level access. |
| `0x40..0x4F` | 16-byte volatile user RAM | `readUserRam()` and `writeUserRam()` with bounds checks. |
| `0xC0..0xCA` | EEPROM-backed configuration mirror | Config helpers preserve reserved bits and optionally queue persistence. |
| `0xCB..0xEA` | 32-byte user EEPROM | Addressed through `EEADDR`, `EEDATA`, and `EECMD`; not direct RAM-mapped storage. |

The driver rejects undocumented register windows and wraparound block accesses
before touching I2C.

## Time And Calendar

The runtime calendar block is BCD encoded:

| Register | Name | Notes |
|---:|---|---|
| `0x00` | 100th seconds | Read-only; not part of `DateTime`. |
| `0x01` | Seconds | BCD `00..59`; bit 7 reserved. |
| `0x02` | Minutes | BCD `00..59`; bit 7 reserved. |
| `0x03` | Hours | BCD `00..23`; bits 7:6 reserved. |
| `0x04` | Weekday | `0..6`; driver computes weekday on writes. |
| `0x05` | Date | BCD `01..31`. |
| `0x06` | Month | BCD `01..12`. |
| `0x07` | Year | BCD `00..99`, interpreted as `2000..2099`. |

Driver consequences:

- `readTime()` performs one burst read of `0x01..0x07`.
- `setTime()` validates calendar fields, computes weekday, and performs one
  burst write of `0x01..0x07`.
- BCD nibbles and calendar ranges are validated strictly.

## Status And Validity

`0x0D` Status:

| Bit | Name | Meaning |
|---:|---|---|
| 7 | `THF` | Temperature high event flag. |
| 6 | `TLF` | Temperature low event flag. |
| 5 | `UF` | Periodic update flag. |
| 4 | `TF` | Periodic countdown timer flag. |
| 3 | `AF` | Alarm flag. |
| 2 | `EVF` | External event flag. |
| 1 | `PORF` | Power-on reset flag. |
| 0 | `VLF` | Voltage low flag; time/data may be invalid. |

Writing the Status register clears `THF` and `TLF` regardless of the written bit
values. Public flag clearing must therefore be explicit and documented.

`0x0E` Temperature LSBs and fault flags:

| Bit | Name | Meaning |
|---:|---|---|
| 7:4 | `TEMP[3:0]` | Fractional temperature bits. |
| 3 | `EEF` | EEPROM write access failed. |
| 2 | `EEbusy` | EEPROM operation or refresh in progress. |
| 1 | `CLKF` | Interrupt-controlled clock-output flag. |
| 0 | `BSF` | Backup switchover flag. |

The driver derives time-invalid state from `PORF || VLF`.

## Alarm And Timer

Alarm registers:

| Register | Name | Notes |
|---:|---|---|
| `0x08` | Minutes alarm | Bit 7 disables minute matching when set. |
| `0x09` | Hours alarm | Bit 7 disables hour matching when set. |
| `0x0A` | Date alarm | Bit 7 disables date matching when set. |

Timer registers:

| Register | Name | Notes |
|---:|---|---|
| `0x0B` | Timer low byte | Low 8 bits of timer value. |
| `0x0C` | Timer high nibble | Low nibble holds bits 11:8; high nibble is preserved. |
| `0x10` | Control 1 | `TE` enables timer, `TD[1:0]` selects frequency. |

Timer frequency encodings in `Control 1`:

| `TD` | Meaning |
|---:|---|
| `0` | 4096 Hz |
| `1` | 64 Hz |
| `2` | 1 Hz |
| `3` | 1/60 Hz |

The driver preserves the high nibble of `0x0C` and configures timer counts
before enabling the timer.

## Control Registers

| Register | Name | Implemented driver uses |
|---:|---|---|
| `0x10` | Control 1 | Timer enable/frequency and EEPROM `EERD` gate. |
| `0x11` | Control 2 | Alarm interrupt enable and related status helpers. |
| `0x12` | Control 3 | Temperature/backup interrupt flags; reserved bits preserved. |
| `0x13` | Timestamp Control | Timestamp reset and overwrite controls. |
| `0x14` | Clock Interrupt Mask | Low-level access only today. |
| `0x15` | EVI Control | Event edge/debounce helpers. |

Simple control read-modify-write operations should use the budgeted register
update job when instruction accounting matters.

## Timestamp Blocks

| Source | Range | Length | Notes |
|---|---:|---:|---|
| TLow | `0x18..0x1E` | 7 bytes | Count + BCD seconds, minutes, hours, date, month, year. |
| THigh | `0x1F..0x25` | 7 bytes | Count + BCD seconds, minutes, hours, date, month, year. |
| EVI | `0x26..0x2D` | 8 bytes | Count + BCD hundredths and date/time fields. |

`readTimestamp()` reads each timestamp block as one contiguous transfer and
validates BCD/date fields.

Timestamp reset bits in `0x13`:

| Bit | Name | Effect |
|---:|---|---|
| 5 | `EVR` | Reset EVI timestamp registers. |
| 4 | `THR` | Reset THigh timestamp registers and clear `THF`. |
| 3 | `TLR` | Reset TLow timestamp registers and clear `TLF`. |

## Temperature And Offset

Temperature readout uses `0x0E` upper nibble plus `0x0F` as a signed 12-bit
fixed-point value with 1/16 degC resolution. The public helper returns Celsius
as `float` for convenience.

`0xC1` EEPROM Offset stores a signed 6-bit frequency offset. The public helper
maps it to approximately `0.2384 ppm` steps while preserving bits 7:6.

## EEPROM-Backed Configuration

EEPROM-backed configuration mirrors:

| Register | Name | Driver use |
|---:|---|---|
| `0xC0` | EEPROM PMU | Backup switch mode, trickle charger, CLKOUT enable. |
| `0xC1` | EEPROM Offset | Frequency offset. |
| `0xC2` | EEPROM CLKOUT 1 | Low-level access only today. |
| `0xC3` | EEPROM CLKOUT 2 | CLKOUT frequency. |
| `0xC4..0xC5` | Temperature reference | Low-level access only today. |
| `0xC6..0xC9` | EEPROM password bytes | Low-level/password flow only. |
| `0xCA` | EEPROM password enable | Low-level/password flow only. |

Default library behavior is RAM-only for configuration helpers:
`Config::enableEepromWrites = false`. EEPROM persistence is opt-in because
EEPROM endurance is finite and writes require a multi-step busy-gated sequence.

## EEPROM Write Sequence

The state machine for one-byte EEPROM persistence is:

1. Read `Control 1`.
2. Set `EERD=1` to disable automatic refresh.
3. Write target EEPROM address to `EEADDR` (`0x3D`).
4. Write target byte to `EEDATA` (`0x3E`).
5. Poll `EEbusy` clear before command write, with a deadline.
6. Write one `EECMD` update command (`0x21`) to `0x3F`.
7. Poll `EEbusy` clear after command, with `Config::eepromTimeoutMs`.
8. Check `EEF`.
9. Restore original `Control 1` with `EERD` cleared.

Each state performs at most one I2C instruction per `tick()` or `pollEeprom()`
call. Waiting on `EEbusy` returns to the caller; the library never uses delay
loops for EEPROM.

## User Storage

Volatile user RAM:

- Range: `0x40..0x4F`.
- Size: 16 bytes.
- Direct register window.
- Writes larger than the 15-byte low-level write limit are split by
  `startWriteUserRamJob()` and `pollJob()`.

User EEPROM:

- Range: `0xCB..0xEA`.
- Size: 32 bytes.
- Accessed through `EEADDR`, `EEDATA`, and `EECMD`.
- Do not assume erased state; applications should initialize their own layout.

## Reserved Bits And Write Policy

- Public helpers preserve reserved bits by read-modify-write.
- Validation errors do not touch I2C and do not update health.
- I2C health is updated only by tracked transport wrappers.
- `probe()` is diagnostic raw I2C and does not update health.
- `recover()` is explicit; normal public I2C operations do not auto-recover from
  `OFFLINE`.
