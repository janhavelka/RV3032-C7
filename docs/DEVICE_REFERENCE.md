# RV3032-C7 device reference used by the driver

This is an implementation-oriented summary of the local Micro Crystal
datasheet and application manual. The vendor PDFs in `reference-pdfs/` remain
the authority for electrical limits, layout, and qualification.

## Interface and memory model

- 7-bit I2C address: `0x51`.
- Active calendar/control/register space: `0x00..0x2D`.
- Volatile user RAM: `0x40..0x4F` (16 bytes).
- Active configuration mirrors: `0xC0..0xCA`.
- Indirect persistent configuration EEPROM: `0xC0..0xCA`.
- Indirect persistent user EEPROM: `0xCB..0xEA` (32 bytes).
- EEADDR/EEDATA/EECMD staging registers: `0x3D..0x3F`.
- EECMD is write-only/read-zero. Persistent EEPROM is not directly mapped and
  is not FRAM.

The driver exposes typed persistent reads instead of pretending that an active
mirror read proves durable state.

## Calendar and alarm

Calendar registers `0x01..0x07` are packed BCD. Seconds, minutes, hours, date,
month, and year have reserved upper bits that must be zero. Weekday is a
binary value from 0..6 and must agree with the Gregorian date. Supported years are
2000..2099.
Register `0x00` is packed-BCD hundredths. It is read independently by the typed
API. Writing Seconds or setting STOP resets hundredths and the 4096 Hz through
1 Hz prescalers.

Alarm registers `0x08..0x0A` contain minute, hour, and date match fields. Their
AE bits disable individual matches. The reset-state date value `00` is accepted
only with AE_D clear in the documented inactive configuration; the typed alarm
setter restores that state when passed date 0. Setting all three AE bits does
not disable the alarm: the vendor truth table defines an event every minute.

## Timer, event, and timestamps

The countdown timer is 12 bits. Running presets are `1..4095`; zero does not
start the timer and is rejected by the typed setter. A write to Timer High
writes only bits 3:0; reserved bits 7:4 are zero. The readable value is the
configured preset, not a live remaining-count register. Timer source and enable
are in Control 1.

External-event ET=`00` detects the selected edge only; ET=`01/10/11` also
samples the selected high/low level at 256/64/8 Hz. EHL selects rising/high or
falling/low. Synchronization and CLKOUT stop-delay enable are also in EVI
Control and have typed set/get coverage. ESYN arms one event to reset the
hundredths/prescaler chain and then self-clears. Event and low/high-temperature timestamp blocks have
different layouts; the typed source selects the exact burst and decoder.
Timestamp reset is a cooperative control-register mutation. TLow/THigh reset
also clears TLF/THF. EVR may read back set, so a repeated EVI reset emits a
preserved 0-to-1 EVR sequence; it does not clear EVF.

## Status side effects

Status contains THF, TLF, UF, TF, AF, EVF, PORF, and VLF. Status bits use
write-zero-to-clear semantics, but the device additionally clears THF and TLF
on every Status-register write regardless of the written values. Every driver
Status writer accounts for that rule. The named verified calendar job is the
only calendar API that deliberately clears PORF/VLF.

Guarded general Status clearing writes one to every unnamed lower-six W0C flag,
so a newly asserted UF/TF/AF/EVF/PORF/VLF between its read and write is
preserved. THF/TLF cannot be protected across that window because silicon
clears both on every Status write; callers that cannot tolerate that race must
serialize the producing condition externally.

TEMP_LSB combines the temperature fraction with CLKF, BSF, EEBUSY, and EEF
support flags. Typed hardware inspection distinguishes EEbusy/EEF from library
queue state. BSF/EEF clearing is guarded so unrelated support state is not
silently destroyed and the read-only temperature fraction is preserved.

## Control bit facts

- Control 1 contains EERD, USEL, GP0, timer enable, and timer source.
- Control 2 AIE is bit 3 and EIE is bit 2; it also contains STOP, CLKIE, UIE,
  TIE, and GP1.
- STOP also halts the documented calendar/CLKOUT/timer/update/EVI-filter/
  temperature functions until restart and resets the lower prescalers.
- Control 3 contains temperature event/interrupt controls and BSIE.
- EVI Control has no generic enable bit 3.
- PMU C0 fields are NCLKE, BSM, TCR, and TCM. TCR and TCM are independent.
- TCM `00` disables the trickle charger. BSM `10` selects level switching.

The primary-cell target is exactly:

```text
(persistentC0 & 0x4C) | 0x20
```

It preserves NCLKE and TCR, selects BSM level mode, and selects TCM disabled.
Primary provisioning requires stable VDD: at least 1.6 V for WRITE_ONE, at
least 2.0 V when the owner uses 400 kHz, and safely above the maximum 2.2 V LSM
threshold through level-switch activation and the measured 10 ms settle. The
application must also prove the board's backup/backfeed conditions; the driver
cannot observe any of these electrical prerequisites.

## CLKOUT, calibration, and temperature

CLKOUT uses PMU NCLKE plus active C2/C3. C3 OS selects crystal-derived versus
high-frequency-divider operation. In high-frequency mode the 13-bit divider is
encoded across C2 and C3 and represents `divider - 1` for a public range of
1..8192. Both FD and HFD remain stored when OS selects the other mode. The
largest encodings exceed the vendor's guaranteed CLKOUT electrical range;
applications must keep output at or below 52 MHz when those characteristics
are required. The legacy enable getter reports direct NCLKE state, not possible
interrupt-selected activity, and the legacy frequency getter rejects OS=1;
complete mode inspection uses the typed CLKOUT configuration getter.
The typed CLKF helpers inspect and cooperatively clear the interrupt-controlled
clock-output latch while preserving the neighboring EEF and BSF W0C flags.

Offset C1 is a signed six-bit measured-error correction with an exact nominal
step of `1000000 / (32768 * 128)` = 0.238418579 ppm. The
driver rejects values outside the exact representable range and preserves the
PORIE/VLIE bits while changing only the six-bit offset field. PORIE and VLIE
also have independent typed set/get APIs.

Temperature is a signed 12-bit value with 1/16 degree C resolution. Thresholds
are independent signed bytes and may use either ordering. The cooperative
configuration job disables detection/interrupts before replacing thresholds,
then restores interrupts before detection. Temperature references use signed
1/128 degree C steps in active C4/C5 and can be handed to
explicit opt-in persistence by their typed cooperative setter. TEMP_LSB and
TEMP_MSB have no blocking/shadowing; the cooperative coherent-temperature job
compares two samples and reports `INCOHERENT_DATA` if they differ. Typed flag
helpers inspect THF/TLF and clear both together, matching the Status-write side
effect.

## Indirect EEPROM commands and timing

Commands used by the implementation:

| Command | Value | Driver use |
| --- | ---: | --- |
| UPDATE_ALL | `0x11` | Defined for reference; never dispatched by generic persistence or primary ensure |
| REFRESH_ALL | `0x12` | Defined for reference; never dispatched by generic persistence or primary ensure |
| WRITE_ONE | `0x21` | One staged persistent byte; at most once per byte/invocation |
| READ_ONE | `0x22` | Direct persistent-byte inspection and verification |

EERD must be controlled through Control 1. EEBUSY and EEF are checked before
mutation and after vendor waits. A READ_ONE requires at least 1 ms settling; a
WRITE_ONE requires at least 10 ms before completion polling. Both waits are
measured from completion of the command transport callback. The configured
generic EEPROM timeout is an additional bounded busy-poll window after the
write settle. Safe C0 access and cleanup include the documented 10 ms
backup-switch settle interval.

A command callback error does not prove the command failed to reach silicon.
The driver therefore never retries WRITE_ONE blindly. It performs direct
persistent proof and reports ambiguity or failure. EEF remains failure
evidence rather than content evidence, so its observation does not replace the
post-command direct readback attempt.

## EEPROM endurance

The vendor guarantees at least 10,000 write cycles at 3 V and 25 degrees C,
but only 100 cycles at 5.5 V and 85 degrees C. Configuration and user EEPROM
are wear-limited. The driver compares before writing, coalesces exact queued
duplicates, and requires durable readback; the application remains responsible
for a product-level write-frequency policy.

## Password protection

Active password inputs are write-only/read-zero. Persistent password bytes and
the enable byte are accessed only through the typed cooperative protection job.
Enabling protection first persists disabled state, then password bytes, then
the enable byte, each with WRITE_ONE and direct proof. Callback success while
loading an active credential is not by itself authentication evidence. Public
status is redacted and never returns credential bytes. After the durable job,
the active mirrors are applied in the safe disabled-password state and an
input credential that differs byte-for-byte from both old and new references
is written to relock protected registers.
