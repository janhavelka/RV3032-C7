# RV-3032-C7 Register Reference (implementation-ready)

Source: `RV-3032-C7_App-Manual.pdf` (Micro Crystal RV-3032-C7 Application Manual, Rev. 1.3, May 2023).

Goal: give a coding agent enough detail to implement a **robust I2C driver** without reopening the PDF.

---

## 1) I2C basics

### 1.1 7-bit slave address
- 7-bit slave address = `0b1010001` = **0x51**
- 8-bit addresses (for reference):
  - write = **0xA2**
  - read  = **0xA3**

### 1.2 Address pointer auto-increment

**Important address map notes (from the app manual):**
- **RAM register space:** `0x00–0x4F` (runtime registers)
- **User EEPROM:** `0xCB–0xEA` (32 bytes non-volatile)
- The register pointer **auto-increments** and may **wrap** at `0xFF → 0x00` depending on access mode.


The manual states: **the address pointer increments automatically after each written or read data byte**.
Practical implication:
- You can burst-read/write consecutive registers by writing a start register, then reading/writing N bytes.

---

## 2) Register conventions (access / protection)

| Conv | Meaning |
|---|---|
| R | Read only. Writing to this register has no effect. |
| W | Write only. Returns 0 when read. |
| R/WP | Read: Always readable. Write: Can be write-protected by password. |
| WP | Write only. Returns 0 when read. Can be write-protected by password. |
| *WP | EEPW registers: Can be write-protected by password. RAM mirror is Write only. Returns 0 when read. EEPROM can be READ when unlocked. |
| Prot. | Protected. Writing to this register has no effect. |

**Notes:**
- `R/WP` and `WP` registers can be **write-protected by password**.
- `*WP` applies to EEPROM-backed registers (EEPW): EEPROM read/write requires unlock rules described in the manual.

---

## 3) Register overview (registers covered in this file)

> Note: The RV-3032-C7 has additional RAM registers up to 0x4F and a User EEPROM range 0xCB–0xEA. If you need per-address bitfields for those, extend this reference from the manual tables.


This is the full register overview table extracted from the manual (addresses, functions, access convention).

| Address | Function | Conv |
|---|---|---|
| 00h | 100th Seconds | R |
| 00h | 100th Seconds | R |
| 01h | Seconds | R/WP |
| 02h | Minutes | R/WP |
| 03h | Hours | R/WP |
| 03h | Hours | R/WP |
| 04h | Weekday | R/WP |
| 05h | Date | R/WP |
| 06h | Month | R/WP |
| 06h | Month | R/WP |
| 07h | Year | R/WP |
| 08h | Minutes Alarm | R/WP |
| 08h | Minutes Alarm | R/WP |
| 09h | Hours Alarm | R/WP |
| 0ah | Date Alarm | R/WP |
| 0bh | Timer Value 0 | R/WP |
| 0bh | Timer Value 0 | R/WP |
| 0ch | Timer Value 1 | R/WP |
| 0dh | Status | R/WP |
| 0dh | Status | R/WP |
| 0eh | Temperature LSBs | R/WP |
| 0eh | Temperature LSBs | R/WP |
| 0fh | Temperature MSBs | R |
| 0fh | Temperature MSBs | R |
| 10h | Control 1 | R/WP |
| 10h | Control 1 | R/WP |
| 11h | Control 2 | R/WP |
| 11h | Control 2 | R/WP |
| 12h | Control 3 | R/WP |
| 12h | Control 3 | R/WP |
| 13h | Time Stamp Contr. | R/WP |
| 13h | Time Stamp Control | R/WP |
| 14h | Clock Int. Mask | R/WP |
| 14h | Clock Interrupt Mask | R/WP |
| 15h | EVI Control | R/WP |
| 15h | EVI Control | R/WP |
| 16h | TLow Threshold | R/WP |
| 16h | TLow Threshold | R/WP |
| 17h | THigh Threshold | R/WP |
| 18h | TS TLow Count | R |
| 18h | TS TLow Count | R |
| 19h | TS TLow Seconds | R |
| 1ah | TS TLow Minutes | R |
| 1bh | TS TLow Hours | R |
| 1bh | TS TLow Hours | R |
| 1ch | TS TLow Date | R |
| 1dh | TS TLow Month | R |
| 1eh | TS TLow Year | R |
| 1eh | TS TLow Year | R |
| 1fh | TS THigh Count | R |
| 1fh | TS THigh Count | R |
| 20h | TS THigh Seconds | R |
| 21h | TS THigh Minutes | R |
| 22h | TS THigh Hours | R |
| 22h | TS THigh Hours | R |
| 23h | TS THigh Date | R |
| 24h | TS THigh Month | R |
| 25h | TS THigh Year | R |
| 25h | TS THigh Year | R |
| 26h | TS EVI Count | R |
| 26h | TS EVI Count | R |
| 27h | TS EVI 100th Secs. | R |
| 28h | TS EVI Seconds | R |
| 28h | TS EVI Seconds | R |
| 29h | TS EVI Minutes | R |
| 2ah | TS EVI Hours | R |
| 2bh | TS EVI Date | R |
| 2bh | TS EVI Date | R |
| 2ch | TS EVI Month | R |
| 2dh | TS EVI Year | R |
| 2dh | TS EVI Year | R |
| 39h | Password 0 | W |
| 39h | Password 0 | W |
| 3ah | Password 1 | W |
| 3bh | Password 2 | W |
| 3ch | Password 3 | W |
| 3dh | EE Address | R/WP |
| 3dh | EE Address | R/WP |
| 3eh | EE Data | R/WP |
| 3fh | EE Command | WP |
| c0h | EEPROM PMU | R/WP |
| c1h | EEPROM Offset | R/WP |
| c2h | EEPROM Clkout 1 | R/WP |
| c3h | EEPROM Clkout 2 | R/WP |
| c4h | EEPROM TReference 0 | R/WP |
| c5h | EEPROM TReference 1 | R/WP |
| c6h | EEPROM Password 0 | *WP |
| c7h | EEPROM Password 1 | *WP |
| c8h | EEPROM Password 2 | *WP |
| c9h | EEPROM Password 3 | *WP |
| cah | EEPROM PW Enable | WP |

---


### Added coverage (user storage areas)
The RV-3032-C7 provides user storage areas that were missing from earlier versions of this file:

- **User RAM (volatile): 0x40–0x4F** — 16 bytes, R/W (optionally password write-protected), default 0x00.
- **User EEPROM (non-volatile): 0xCB–0xEA** — 32 bytes, accessed via the EEPROM access registers (`EEADDR/EEDATA/EECMD`), optionally password write-protected.

These are intended for application-defined storage (no fixed bit meaning in the manual).

## 4) Per-register detail (bit layout + field notes)

Legend:
- “○” in the bit layout typically means *reserved / unused / fixed 0* (per table formatting).
- Numeric weights like `80, 40, 20, ...` indicate **BCD** digit weights for time/calendar fields.

### 0x00 (00H) — 100th Seconds  
**Access:** R  
**Bit layout:** b7=80, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x01 (01H) — Seconds  
**Access:** R/WP  
**Bit layout:** b7=○, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x02 (02H) — Minutes  
**Access:** R/WP  
**Bit layout:** b7=○, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x03 (03H) — Hours  
**Access:** R/WP  
**Bit layout:** b7=○, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x04 (04H) — Weekday  
**Access:** R/WP  
**Bit layout:** b7=○, b6=○, b5=○, b4=○, b3=○, b2=4, b1=2, b0=1

### 0x05 (05H) — Date  
**Access:** R/WP  
**Bit layout:** b7=○, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x06 (06H) — Month  
**Access:** R/WP  
**Bit layout:** b7=○, b6=○, b5=○, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x07 (07H) — Year  
**Access:** R/WP  
**Bit layout:** b7=80, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x08 (08H) — Minutes Alarm  
**Access:** R/WP  
**Bit layout:** b7=AE_M, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x09 (09H) — Hours Alarm  
**Access:** R/WP  
**Bit layout:** b7=AE_H, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x0A (0AH) — Date Alarm  
**Access:** R/WP  
**Bit layout:** b7=AE_D, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x0B (0BH) — Timer Value 0  
**Access:** R/WP  
**Bit layout:** b7=128, b6=64, b5=32, b4=16, b3=8, b2=4, b1=2, b0=1

### 0x0C (0CH) — Timer Value 1  
**Access:** R/WP  
**Bit layout:** b7=○, b6=○, b5=○, b4=○, b3=2048, b2=1024, b1=512, b0=256

### 0x0D (0DH) — Status  
**Access:** R/WP  
**Bit layout:** b7=THF, b6=TLF, b5=UF, b4=TF, b3=AF, b2=EVF, b1=PORF, b0=VLF

### 0x0E (0EH) — Temperature LSBs  
**Access:** R/WP  
**Bit layout:** b7=TEMP [3:0], b6=—, b5=—, b4=—, b3=EEF, b2=EEbusy, b1=CLKF, b0=BSF

### 0x0F (0FH) — Temperature MSBs  
**Access:** R  
**Bit layout:** b7=TEMP [11:4], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0x10 (10H) — Control 1  
**Access:** R/WP  
**Bit layout:** b7=-, b6=-, b5=GP0, b4=USEL, b3=TE, b2=EERD, b1=TD, b0=—

### 0x11 (11H) — Control 2  
**Access:** R/WP  
**Bit layout:** b7=-, b6=CLKIE, b5=UIE, b4=TIE, b3=AIE, b2=EIE, b1=GP1, b0=STOP

### 0x12 (12H) — Control 3  
**Access:** R/WP  
**Bit layout:** b7=-, b6=-, b5=-, b4=BSIE, b3=THE, b2=TLE, b1=THIE, b0=TLIE

### 0x13 (13H) — Time Stamp Contr.  
**Access:** R/WP  
**Bit layout:** b7=-, b6=-, b5=EVR, b4=THR, b3=TLR, b2=EVOW, b1=THOW, b0=TLOW

### 0x13 (13H) — Time Stamp Control  
**Access:** R/WP  
**Bit layout:** b7=○, b6=○, b5=EVR, b4=THR, b3=TLR, b2=EVOW, b1=THOW, b0=TLOW

Field details:
- `7:6` **○** — values: 0 | Read only. Always 0.
- `5` **EVR** — values: Time Stamp EVI Reset bit (see TIME STAMP EVI FUNCTION)
- `4` **THR** — values: Time Stamp THigh Reset bit (see TIME STAMP THIGH FUNCTION)
- `3` **TLR** — values: Time Stamp TLow Reset bit (see TIME STAMP TLOW FUNCTION)
- `2` **EVOW** — values: Time Stamp EVI Overwrite bit. Controls the overwrite function of the TS EVI registers (TS EVI 100th Seconds to TS EVI Year). The TS EVI Count register always counts events, regardless of the settings of the override bit EVOW. (see TIME STAMP EVI FUNCTION)
- `1` **THOW** — values: Time Stamp THigh Overwrite bit. Controls the overwrite function of the TS THigh registers (TS THigh Seconds to TS THigh Year). The TS THigh Count register always counts events, regardless of the settings of the override bit THOW. (see TIME STAMP THIGH FUNCTION)
- `0` **TLOW** — values: Time Stamp TLow Overwrite bit. Controls the overwrite function of the TS TLow registers (TS TLow Seconds to TS TLow Year). The TS TLow Count register always counts events, regardless of the settings of the override bit TLOW. (see TIME STAMP TLOW FUNCTION)

### 0x14 (14H) — Clock Int. Mask  
**Access:** R/WP  
**Bit layout:** b7=CLKD, b6=INTDE, b5=CEIE, b4=CAIE, b3=CTIE, b2=CUIE, b1=CTHIE, b0=CTLIE

### 0x14 (14H) — Clock Interrupt Mask  
**Access:** R/WP  
**Bit layout:** b7=CLKD, b6=INTDE, b5=CEIE, b4=CAIE, b3=CTIE, b2=CUIE, b1=CTHIE, b0=CTLIE

Field details:
- `7` **CLKD** — values: CLKOUT (switch) off Delay Value after I2C STOP Selection bit. Applicable only when CLKDE bit in the EVI Control register is set to 1. (see CLKOUT OFF DELAY AFTER I2C STOP)
- `6` **INTDE** — values: Interrupt Delay after CLKOUT On Enable bit. Applicable only when NCLKE bit in the EEPROM PMU register is set to 1 (CLKOUT not directly enabled) and for interrupts enabled by CEIE, CAIE, CTIE, CUIE, CTHIE or CTLIE (see INTERRUPT DELAY AFTER CLKOUT ON)
- `5` **CEIE** — values: Clock output when EVI Interrupt Enable bit.
- `4` **CAIE** — values: Clock output when Alarm Interrupt Enable bit.
- `3` **CTIE** — values: Clock output when Periodic Countdown Timer Interrupt Enable bit.
- `2` **CUIE** — values: Clock output when Periodic Time Update Interrupt Enable bit.
- `1` **CTHIE** — values: Clock output when THigh Interrupt Enable bit.
- `0` **CTLIE** — values: Clock output when TLow Interrupt Enable bit

### 0x15 (15H) — EVI Control  
**Access:** R/WP  
**Bit layout:** b7=CLKDE, b6=EHL, b5=ET, b4=—, b3=○, b2=○, b1=○, b0=ESYN

### 0x16 (16H) — TLow Threshold  
**Access:** R/WP  
**Bit layout:** b7=TLT, b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0x17 (17H) — THigh Threshold  
**Access:** R/WP  
**Bit layout:** b7=THT, b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0x18 (18H) — TS TLow Count  
**Access:** R  
**Bit layout:** b7=128, b6=64, b5=32, b4=16, b3=8, b2=4, b1=2, b0=1

### 0x19 (19H) — TS TLow Seconds  
**Access:** R  
**Bit layout:** b7=○, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x1A (1AH) — TS TLow Minutes  
**Access:** R  
**Bit layout:** b7=○, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x1B (1BH) — TS TLow Hours  
**Access:** R  
**Bit layout:** b7=○, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x1C (1CH) — TS TLow Date  
**Access:** R  
**Bit layout:** b7=○, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x1D (1DH) — TS TLow Month  
**Access:** R  
**Bit layout:** b7=○, b6=○, b5=○, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x1E (1EH) — TS TLow Year  
**Access:** R  
**Bit layout:** b7=80, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x1F (1FH) — TS THigh Count  
**Access:** R  
**Bit layout:** b7=128, b6=64, b5=32, b4=16, b3=8, b2=4, b1=2, b0=1

### 0x20 (20H) — TS THigh Seconds  
**Access:** R  
**Bit layout:** b7=○, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x21 (21H) — TS THigh Minutes  
**Access:** R  
**Bit layout:** b7=○, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x22 (22H) — TS THigh Hours  
**Access:** R  
**Bit layout:** b7=○, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x23 (23H) — TS THigh Date  
**Access:** R  
**Bit layout:** b7=○, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x24 (24H) — TS THigh Month  
**Access:** R  
**Bit layout:** b7=○, b6=○, b5=○, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x25 (25H) — TS THigh Year  
**Access:** R  
**Bit layout:** b7=80, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x26 (26H) — TS EVI Count  
**Access:** R  
**Bit layout:** b7=128, b6=64, b5=32, b4=16, b3=8, b2=4, b1=2, b0=1

### 0x27 (27H) — TS EVI 100th Secs.  
**Access:** R  
**Bit layout:** b7=80, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x28 (28H) — TS EVI Seconds  
**Access:** R  
**Bit layout:** b7=○, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x29 (29H) — TS EVI Minutes  
**Access:** R  
**Bit layout:** b7=○, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x2A (2AH) — TS EVI Hours  
**Access:** R  
**Bit layout:** b7=○, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x2B (2BH) — TS EVI Date  
**Access:** R  
**Bit layout:** b7=○, b6=○, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x2C (2CH) — TS EVI Month  
**Access:** R  
**Bit layout:** b7=○, b6=○, b5=○, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x2D (2DH) — TS EVI Year  
**Access:** R  
**Bit layout:** b7=80, b6=40, b5=20, b4=10, b3=8, b2=4, b1=2, b0=1

### 0x39 (39H) — Password 0  
**Access:** W  
**Bit layout:** b7=PW [7:0], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0x3A (3AH) — Password 1  
**Access:** W  
**Bit layout:** b7=PW [15:8], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0x3B (3BH) — Password 2  
**Access:** W  
**Bit layout:** b7=PW [23:16], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0x3C (3CH) — Password 3  
**Access:** W  
**Bit layout:** b7=PW [31:24], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0x3D (3DH) — EE Address  
**Access:** R/WP  
**Bit layout:** b7=EEADDR, b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0x3E (3EH) — EE Data  
**Access:** R/WP  
**Bit layout:** b7=EEDATA, b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0x3F (3FH) — EE Command  
**Access:** WP  
**Bit layout:** b7=EECMD, b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—



### 0x40–0x4F — User RAM (16 bytes)

**Function:** 16 bytes of general-purpose **volatile** storage.

- **Access:** R/W (the manual marks this as **R/WP**: readable always; writes may be password-protected depending on configuration).
- **Reset/default value:** 0x00 for all 16 bytes (explicitly stated in the manual’s RAM reset-value table).
- **Notes:**
  - No predefined bitfields; treat as opaque bytes for your application.
  - Recommended driver policy: expose `readUserRam(offset, len)` / `writeUserRam(offset, ...)` with bounds checking.


### 0xC0 (C0H) — EEPROM PMU  
**Access:** R/WP  
**Bit layout:** b7=-, b6=NCLKE, b5=BSM, b4=—, b3=TCR, b2=—, b1=TCM, b0=—

### 0xC1 (C1H) — EEPROM Offset  
**Access:** R/WP  
**Bit layout:** b7=PORIE, b6=VLIE, b5=OFFSET, b4=—, b3=—, b2=—, b1=—, b0=—

### 0xC2 (C2H) — EEPROM Clkout 1  
**Access:** R/WP  
**Bit layout:** b7=HFD [7:0], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0xC3 (C3H) — EEPROM Clkout 2  
**Access:** R/WP  
**Bit layout:** b7=OS, b6=FD, b5=—, b4=HFD [12:8], b3=—, b2=—, b1=—, b0=—

### 0xC4 (C4H) — EEPROM TReference 0  
**Access:** R/WP  
**Bit layout:** b7=TREF [7:0], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0xC5 (C5H) — EEPROM TReference 1  
**Access:** R/WP  
**Bit layout:** b7=TREF [15:8], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0xC6 (C6H) — EEPROM Password 0  
**Access:** *WP  
**Bit layout:** b7=EEPW [7:0], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0xC7 (C7H) — EEPROM Password 1  
**Access:** *WP  
**Bit layout:** b7=EEPW [15:8], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0xC8 (C8H) — EEPROM Password 2  
**Access:** *WP  
**Bit layout:** b7=EEPW [23:16], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0xC9 (C9H) — EEPROM Password 3  
**Access:** *WP  
**Bit layout:** b7=EEPW [31:24], b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0xCA (CAH) — EEPROM PW Enable  
**Access:** WP  
**Bit layout:** b7=EEPWE, b6=—, b5=—, b4=—, b3=—, b2=—, b1=—, b0=—

### 0xCB–0xEA — User EEPROM (32 bytes)

**Function:** 32 bytes of general-purpose **non-volatile** storage.

- **Access:** The manual marks this as **R/WP** (readable always; writes may be password-protected depending on configuration / password unlock state).
- **Default value:** Not guaranteed by the manual; do not assume erased state. Read and initialize explicitly in your application if needed.
- **How to access:** User EEPROM is **not** memory-mapped to the normal register pointer. You must use the EEPROM access mechanism:
  1. Disable auto-refresh from EEPROM by setting `EERD = 1` (Control 1).
  2. Write EEPROM byte address to **EEADDR (0x3D)**.
  3. For **read:** write `EECMD = 0x22`, then read result from **EEDATA (0x3E)**.
     - Typical read time stated in the manual: ~1.1 ms (poll `EEbusy` until 0).
  4. For **write:** write data to **EEDATA (0x3E)**, then write `EECMD = 0x21`.
     - Typical write time stated in the manual: ~4.8 ms (poll `EEbusy` until 0).
  5. Re-enable auto-refresh by setting `EERD = 0`.

- **Important constraints from the manual:**
  - Maintain minimum supply voltage during EEPROM write until `EEbusy = 0`.
  - The `EEF` flag can indicate EEPROM write failure (referenced by the manual).

---

## Appendix — Register field details (added)

### 0x00 — 100th Seconds
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `100th` | Seconds 00 to 99 ESYN bit is 1 in case of an External Event detection on EVI pin the 100th Seconds register value is cleared to 00 |

### 0x01 — Seconds
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6:0 | `Seconds` | 00 to 59 to 00 (similar to ESYN Bit function) |

### 0x02 — Minutes
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6:0 | `Minutes` | 00 to 59 Holds the count of minutes, coded in BCD format |

### 0x03 — Hours
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5:0 | `Hours` | 00 to 23 Holds the count of hours, coded in BCD format |

### 0x04 — Weekday
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 2:0 | `Weekday` | 0 to 6 Holds the weekday counter value |

### 0x05 — Date
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 1` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5:0 | `Date` | 01 to 31 – Default value = 01 (01 is a valid date) DTCXO Temp |

### 0x06 — Month
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 1` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 4:0 | `Month` | 01 to 12 Holds the current month, coded in BCD format |
| 0 | `0` | 0 July 0 0 1 1 1 August 0 1 0 0 0 September 0 1 0 0 1 October 1 0 0 0 0 November 1 0 0 0 1 December 1 0 0 1 0 |

### 0x07 — Year
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `Year` | 00 to 99 Holds the current year, coded in BCD format |

### 0x08 — Minutes Alarm
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7 | `AE_M` | 0 Enabled |

### 0x09 — Hours Alarm
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7 | `AE_H` | 0 Enabled |

### 0x0A — Date Alarm
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7 | `AE_D` | 0 Enabled |

### 0x0B — Timer Value 0
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 8 | `bit` | ) |
| 7:0 | `Timer` | Value 0 only the preset value is returned and not the current value |

### 0x0C — Timer Value 1
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 3:0 | `Timer` | Value 1 0h to Fh 4 bit) |

### 0x0D — Status
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 X 1 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7 | `THF` | If set to 0 beforehand, indicates the occurrence of a temperature that is above the stored Temperature High Threshold value THT |
| 6 | `TLF` | If set to 0 beforehand, indicates the occurrence of a temperature that is below the stored Temperature Low Threshold value TLT |
| 5 | `UF` | 0 No event detected |
| 4 | `TF` | If set to 0 beforehand, indicates the occurrence of a Periodic Countdown 1 Timer Interrupt event |
| 3 | `AF` | If set to 0 beforehand, indicates the occurrence of an Alarm Interrupt event |
| 2 | `EVF` | If X = 1, a LOW level was detected on EVI pin |
| 1 | `PORF` | in VDD Power state has occurred |
| 0 | `VLF` | The sampling frequency is 1 Hz |

### 0x0E — Temperature LSBs
- **Access:** Read: Always readable; Write: Can be write-protected by password

| Bits | Field | Meaning (summary) |
|---|---|---|
| 3 | `EEF` | If set to 0 beforehand, indicates that the EEPROM write access has failed 1 because V has dropped below V (1 |
| 2 | `EEbusy` | Indicates that the EEPROM is currently handling a read or write request and will ignore any further commands until the current one is finished |
| 1 | `CLKF` | If set to 0 beforehand, indicates the occurrence of an interrupt driven clock 1 output on CLKOUT pin |
| 0 | `BSF` | disabled (BSM field = 00 or 11) BSF is always logic 0 |

### 0x0F — Temperature MSBs
- (No field table extracted on this register block; treat as byte-level register per manual.)

### 0x10 — Control 1
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5 | `GP0` | 0 or 1 Register bit for general purpose use |
| 4 | `USEL` | Seconds register or when ESYN bit is 1 in case of an External Event detection on EVI pin the length of the current update period is affected |
| 3 | `TE` | 0 Stops the Periodic Countdown Timer Interrupt function |
| 2 | `EERD` | the data stored in the EEPROM each 24 hours, at date increment (at the 0 beginning of the last second before midnight) |
| 1:0 | `TD` | 00 to 11 coordinated with the clock update timing but has a maximum jitter of 30 |

### 0x11 — Control 2
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6 | `CLKIE` | When set to 1, the clock output on CLKOUT pin is automatically enabled when an interrupt occurs, based on the Clock Interrupt Mask (register 14h) 1 and accor... |
| 5 | `UIE` | cancelled |
| 4 | `TIE` | RTN1 – Default value An interrupt signal is generated on I̅N̅̅̅T̅ pin when a Periodic Countdown 1 Timer event occurs |
| 3 | `AIE` | An interrupt signal is generated on I̅N̅̅̅T̅ pin when an Alarm event occurs |
| 2 | `EIE` | pin occurs or the signal is cancelled on I̅N̅̅̅T̅ pin |
| 1 | `GP1` | 0 or 1 Register bit for general purpose use |
| 0 | `STOP` | The following functions are stopped: Clock and calendar (with alarm), CLKOUT, timer clock, update timer clock, EVI input filter, temperature 1 measurement, t... |

### 0x12 — Control 3
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 4 | `BSIE` | 0 Switchover occurs or the signal is cancelled on I̅N̅̅̅T̅ pin |
| 2 | `TLE` | Enables the Temperature Low detection with programmable Temperature 1 Low Threshold, and the corresponding Time Stamp function |
| 1 | `THIE` | detected or the signal is cancelled on I̅N̅̅̅T̅ pin |
| 0 | `TLIE` | detected or the signal is cancelled on I̅N̅̅̅T̅ pin |

### 0x13 — Time Stamp Control
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5 | `EVR` | Writing 1 to the EVR bit resets all eight Time Stamp EVI registers (TS EVI 1 Count to TS EVI Year) to 00h |
| 4 | `THR` | Writing 1 to the THR bit resets all seven Time Stamp THigh registers (TS 1 THigh Count to TS THigh Year) to 00h and the THF flag is also cleared to 0 |
| 3 | `TLR` | Writing 1 to the TLR bit resets all seven Time Stamp TLow registers (TS 1 TLow Count to TS TLow Year) to 00h and the TLF flag is also cleared to 0 |
| 2 | `EVOW` | written to the EVR bit to clear all TS EVI registers (POR has same effect, 0 when EVI pin = HIGH) |
| 1 | `THOW` | THigh registers |
| 0 | `TLOW` | TLow registers |

### 0x14 — Clock Interrupt Mask
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7 | `CLKD` | 0 Typical delay time t = 1 |
| 6 | `INTDE` | CTLIE  0 No delay |
| 5 | `CEIE` | 0 Disabled – Default value 1 Enabled |
| 4 | `CAIE` | 0 Disabled – Default value 1 Enabled |
| 3 | `CTIE` | Enabled: Internal signal TI is selected |
| 2 | `CUIE` | 0 Disabled – Default value 1 Enabled |
| 1 | `CTHIE` | 0 Disabled – Default value 1 Enabled |
| 0 | `CTLIE` | 0 Disabled – Default value 1 Enabled |

### 0x15 — EVI Control
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7 | `CLKDE` | Enabled |
| 6 | `EHL` | 0 External Event on pin EVI |
| 5:4 | `ET` | 01 Sampling period t = 3 |
| 0 | `ESYN` | is reset to 00 |

### 0x16 — TLow Threshold
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TLT` | 127 when TEMP [11:4] < TLT |

### 0x17 — THigh Threshold
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `THT` | 127 is generated when TEMP [11:4] > THT |

### 0x18 — TS TLow Count
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TS` | TLow Count 0 to 255 The TS TLow Count register always counts events, regardless of the settings of the override bit TLOW |

### 0x19 — TS TLow Seconds
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6:0 | `TS` | TLow Seconds 00 to 59 occurred event |

### 0x1A — TS TLow Minutes
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6:0 | `TS` | TLow Minutes 00 to 59 occurred event |

### 0x1B — TS TLow Hours
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5:0 | `TS` | TLow Hours 00 to 23 occurred event |

### 0x1C — TS TLow Date
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5:0 | `TS` | TLow Date 01 to 31 The TS TLow Date register is reset to 00h when 1 is written to the Time Stamp TLow Reset bit TLR |

### 0x1D — TS TLow Month
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 4:0 | `TS` | TLow Month 01 to 12 The TS TLow Month register is reset to 00h when 1 is written to the Time Stamp TLow Reset bit TLR |

### 0x1E — TS TLow Year
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TS` | TLow Year 00 to 99 occurred event |

### 0x1F — TS THigh Count
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TS` | THigh Count 0 to 255 The TS THigh Count register always counts events, regardless of the settings of the override bit THOW |

### 0x20 — TS THigh Seconds
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6:0 | `TS` | THigh Seconds 00 to 59 occurred event |

### 0x21 — TS THigh Minutes
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6:0 | `TS` | THigh Minutes 00 to 59 occurred event |

### 0x22 — TS THigh Hours
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5:0 | `TS` | THigh Hours 00 to 23 occurred event |

### 0x23 — TS THigh Date
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5:0 | `TS` | THigh Date 01 to 31 The TS THigh Date register is reset to 00h when 1 is written to the Time Stamp THigh Reset bit THR |

### 0x24 — TS THigh Month
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 4:0 | `TS` | THigh Month 01 to 12 The TS THigh Month register is reset to 00h when 1 is written to the Time Stamp THigh Reset bit THR |

### 0x25 — TS THigh Year
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TS` | THigh Year 00 to 99 occurred event |

### 0x26 — TS EVI Count
- **Reset (bitwise):** `0 0 0 0 0 0 0 X` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TS` | EVI Count 0 to 255 The Reset value X depends on the voltage on the EVI pin at POR |

### 0x27 — TS EVI 100th Seconds
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TS` | EVI 100th Seconds 00 to 99 the first or last occurred event |

### 0x28 — TS EVI Seconds
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6:0 | `TS` | EVI Seconds 00 to 59 the first or last occurred event |

### 0x29 — TS EVI Minutes
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6:0 | `TS` | EVI Minutes 00 to 59 the first or last occurred event |

### 0x2A — TS EVI Hours
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5:0 | `TS` | EVI Hours 0 to 23 the first or last occurred event |

### 0x2B — TS EVI Date
- **Reset (bitwise):** `0 0 0 0 0 0 0 X` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 5:0 | `TS` | EVI Date 01 to 31 Because EHL = 0 at POR, the low level is regarded as an External Event Interrupt and an External Event Time Stamp is recorded |

### 0x2C — TS EVI Month
- **Reset (bitwise):** `0 0 0 0 0 0 0 X` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 4:0 | `TS` | EVI Month 01 to 12 Because EHL = 0 at POR, the low level is regarded as an External Event Interrupt and an External Event Time Stamp is recorded |

### 0x2D — TS EVI Year
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TS` | EVI Year 00 to 99 the first or last occurred event |

### 0x39 — Password 0
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `PW` | [7:0] Bit 0 to 7 from 32-bit Password FFh |

### 0x3A — Password 1
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `PW` | [15:8] Bit 8 to 15 from 32-bit Password FFh |

### 0x3B — Password 2
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `PW` | [23:16] Bit 16 to 23 from 32-bit Password FFh |

### 0x3C — Password 3
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `PW` | [31:24] Bit 24 to 31 from 32-bit Password FFh DTCXO Temp |

### 0x3D — EE Address
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `1 1 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `EEADDR` | FFh The default address C0h points to the first Configuration EEPROM Register (EEPROM PMU) |

### 0x3E — EE Data
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `EEDATA` |  |

### 0x3F — EE Command
- **Access:** Read: Always readable; Write: Can be write-protected by password
- **Reset (bitwise):** `0 0 0 0 0 0 0 0` (as shown in manual tables)

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `EECMD` | RAM bytes are written |
| 16 | `Bytes` | of User RAM for general purpose storage are provided |

### 0x40–0x4F — User RAM (added)
- **Type:** 16 bytes general-purpose RAM.
- **Reset:** 0x00 (per manual default).
- **Notes:** write-protected if password protection is enabled; safe for small application state.

### 0xC0 — EEPROM Power Management Unit (PMU)
- **Access:** Read: Always readable; Write: Can be write-protected by password

| Bits | Field | Meaning (summary) |
|---|---|---|
| 6 | `NCLKE` | 0 CLKOUT is directly enabled |
| 5:4 | `BSM` | 00 Switchover Disabled |
| 01 | `Switchover` | when V < V (PMU selects pin with the greater voltage DD BACKUP (V or V )) |
| 10 | `Switchover` | when V < V (2 |
| 11 | `Switchover` | Disabled |
| 00 | `TCR` | 0 |
| 3:2 | `TCR` |  |
| 01 | `TCR` | 2 kΩ |
| 10 | `TCR` | 7 kΩ |
| 11 | `TCR` | 12 kΩ Trickle Charger Mode |
| 00 | `Trickle` | Charger off |
| 01 | `DD` |  In LSM Mode (BSM = 10), the internal regulated voltage with the typical value of 1 |
| 1:0 | `TCM` | TCM 3 |

### 0xC1 — EEPROM Offset
- **Access:** Read: Always readable; Write: Can be write-protected by password

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7 | `PORIE` | or the signal is cancelled on I̅N̅̅̅T̅ pin |
| 1 | `occurs` |  |
| 6 | `VLIE` | occurs or the signal is cancelled on I̅N̅̅̅T̅ pin |
| 5:0 | `OFFSET` | range is roughly ±7 |
| 011111 | `31` | 31 7 |
| 011110 | `30` | 30 7153 : : : : |
| 000001 | `1` | 1 0 |
| 111111 | `63` | -1 -0 |
| 111110 | `62` | -2 -0 |
| 100001 | `33` | -31 -7 |
| 100000 | `32` | -32 -7 |

### 0xC2 — EEPROM Clkout 1
- **Access:** Read: Always readable; Write: Can be write-protected by password

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `HFD` | [7:0] FFh See table on next page |

### 0xC3 — EEPROM Clkout 2
- **Access:** Read: Always readable; Write: Can be write-protected by password

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7 | `OS` | 0 XTAL mode is selected |
| 1 | `HF` | mode is selected |
| 6:5 | `FD` | 00 to 11 CLKOUT Frequency Selection in XTAL mode |
| 4:0 | `HFD` | [12:8] to See table on next page |
| 00 | `32` |  |
| 01 | `1024` | Hz (1) (2) If STOP bit = 1, the clock output is stopped |
| 10 | `64` | Hz (1) (2) remains HIGH or LOW |
| 11 | `1` | Hz (1) (2) (1) 1024 Hz to 1 Hz clock pulses can be affected by compensation pulses |
| 0000000000000 | `0` | 1 – Default value on delivery |
| 0000000000001 | `1` | 2 16 |
| 0000000000010 | `2` | 3 24 |
| 1100011001011 | `6347` | 6348 52 |
| 1111111111110 | `8190` | 8191 67 |
| 1111111111111 | `8191` | 8192 67 |

### 0xC4 — EEPROM TReference 0
- **Access:** Read: Always readable; Write: Can be write-protected by password

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TREF` | [7:0] Lower 8 bits of the TREF value |

### 0xC5 — EEPROM TReference 1
- **Access:** Read: Always readable; Write: Can be write-protected by password

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `TREF` | [15:8] Upper 8 bits of the TREF value |

### 0xC6 — EEPROM Password 0

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `EEPW` | [7:0] Bit 0 to 7 from 32-bit EEPROM Password FFh |

### 0xC7 — EEPROM Password 1

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `EEPW` | [15:8] Bit 8 to 15 from 32-bit EEPROM Password FFh |

### 0xC8 — EEPROM Password 2

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `EEPW` | [23:16] Bit 16 to 23 from 32-bit EEPROM Password FFh |

### 0xC9 — EEPROM Password 3

| Bits | Field | Meaning (summary) |
|---|---|---|
| 7:0 | `EEPW` | [31:24] Bit 24 to 31 from 32-bit EEPROM Password FFh DTCXO Temp |

### 0xCA — EEPROM Password Enable
- **Access:** Read: Always readable; Write: Can be write-protected by password

| Bits | Field | Meaning (summary) |
|---|---|---|
| 0 | `to` | 254 When writing a value not equal 255, the password function is disabled |
| 7:0 | `EEPWE` | – 00h is the default value preset on delivery Password function enabled |
| 255 | `When` | writing a value of 255, the Password registers (39h to 3Ch) can be used to enter the 32-bit Password |
| 32 | `Bytes` | of User EEPROM for general purpose storage are provided |

### 0xCB–0xEA — User EEPROM (added)
- **Type:** 32 bytes user EEPROM space.
- **Access:** not directly memory-mapped; accessed through `EEADDR (0x3D)`, `EEDATA (0x3E)`, `EECMD (0x3F)`.
- **Notes:** respect `EEbusy` while operations are in progress; use the app-manual command values for read/write via `EECMD`.

