# RV-3032-C7 — Register & Flag Reference (for driver/library work)

Source: *RV-3032-C7 Application Manual* (Micro Crystal, Rev. 1.3, May 2023).  
This document is meant to be “copilot-friendly”: quick lookups, bitfields, reset behavior, and the “gotchas” you’ll hit when implementing a library.

---

## I²C basics

- **7-bit slave address:** `0x51` (binary `1010_001`)  
- **8-bit address byte:** `0xA2` (write), `0xA3` (read)  
- **Auto-increment:** after each byte; wraps `0xFF → 0x00`  
Pages: 124–125 (*Slave address*, read/write sequences). fileciteturn3file4L17-L48

---

## Register spaces

- **RAM registers:** `0x00–0x4F` (normal runtime registers)  
- **Config EEPROM (mirrored into RAM):** `0xC0–0xCA`  
- **User EEPROM:** `0xCB–0xEA` (32 bytes)  
Pages: 13–14 (*Register organization / overview*). fileciteturn4file2L41-L49

---

## Register overview (address → purpose)

> Tip: implement these as strongly-typed “register descriptors” (addr, access, reset, encoding, bitfields).

### Core clock/calendar (BCD)
- `00h` 100th Seconds
- `01h` Seconds
- `02h` Minutes
- `03h` Hours
- `04h` Weekday
- `05h` Date
- `06h` Month
- `07h` Year  
Register overview page: 14. fileciteturn4file2L63-L66

### Alarm (BCD + enable bits)
- `08h` Minutes Alarm
- `09h` Hours Alarm
- `0Ah` Date Alarm
- `0Bh` Timer Value 0
- `0Ch` Timer Value 1 / Control bits  
Register overview page: 14. fileciteturn4file2L63-L66

### Status / temperature / interrupts
- `0Dh` **Status** (event + validity flags)
- `0Eh` **Temperature LSBs** + **EEPROM/clock/battery flags**
- `0Fh` Temperature MSBs
- `10h` Control 1
- `11h` Control 2
- `12h` Control 3
- `13h` Time Stamp Control
- `14h` Clock Interrupt Mask
- `15h` EVI Control
- `16h` TLow Threshold
- `17h` THigh Threshold  
Register overview page: 14. fileciteturn4file2L63-L66

### Time-stamp blocks (read-only capture registers)
- `18h–1Fh` TS TLow (count + timestamp fields)
- `20h–25h` TS THigh (count + timestamp fields)
- `26h–2Dh` TS EVI (count + timestamp fields)  
Register overview page: 14. fileciteturn4file2L63-L66

### Password / EEPROM control / user RAM
- `39h–3Ch` Password bytes (write-only)
- `3Dh` EE Address
- `3Eh` EE Data
- `3Fh` EE Command
- `40h–4Fh` User RAM (16 bytes)  
Reset/overview pages: 54–55. fileciteturn4file9L1-L12 fileciteturn4file6L4-L49

---

## Status & validity flags (what your library should expose)

### 0Dh — Status register (R/WP)
Purpose: interrupt flags + reliability flags in internal data.  
Page: 22 (section “3.7 STATUS REGISTER”). fileciteturn3file13L13-L13

| Bit | Name | Meaning | Reset | Sticky? | Clear method |
|---:|---|---|---|---|---|
| 7 | THF | Temperature High event occurred | 0 | Yes (until cleared) | Write `0` to THF (but **writing anything to 0Dh resets THF/TLF** – see note) |
| 6 | TLF | Temperature Low event occurred | 0 | Yes (until cleared) | Write `0` to TLF (same note as above) |
| 5 | UF | Periodic time update event occurred | 0 | Yes (until cleared) | Write `0` to UF |
| 4 | TF | Periodic countdown timer event occurred | 0 | Yes (until cleared) | Write `0` to TF |
| 3 | AF | Alarm event occurred | 0 | Yes (until cleared) | Write `0` to AF |
| 2 | EVF | External event (EVI) occurred | X | Yes (until cleared) | Write `0` to EVF |
| 1 | PORF | Power-on reset occurred | 1 | Yes (until cleared) | Write `0` to PORF |
| 0 | VLF | Voltage low detected (internal below VLOW) | 0 | Yes (until cleared) | Write `0` to VLF |

Notes / driver consequences:
- **“Time invalid” conditions:** treat `PORF=1` or `VLF=1` as “time may be invalid / reinit required”. VLF explicitly warns “data may no longer be valid and all registers should be reinitialized.” fileciteturn3file13L7-L10
- **THF/TLF special clearing behavior:** “THF and TLF flags are always reset whenever 0Dh Status is written to (using 0s or 1s).” fileciteturn3file13L11-L13

#### Flag mapping requested (library-facing)
- **Power-on reset:** `PORF` (0Dh bit1)
- **Voltage low:** `VLF` (0Dh bit0)
- **Time invalid:** derive from `PORF` and/or `VLF` (recommended).
- **Oscillator stop / clock issue:** use `CLKF` (0Eh bit1, see below) for “clock interrupt / oscillator/clock-domain issue” style flag (library should document its meaning per datasheet section).
- **Battery switchover:** `BSF` (0Eh bit0)

---

### 0Eh — Temperature LSBs + reliability flags (R/WP with RO fields)
Purpose: fractional temperature + EEPROM/clock/battery flags.  
Page: 23 (section “3.8 TEMPERATURE REGISTERS”). fileciteturn3file13L16-L26

| Bit | Name | Meaning | Reset | Sticky? | Clear method |
|---:|---|---|---|---|---|
| 7:4 | TEMP[3:0] | Fractional temp (0.0625°C steps). Read-only. | `0h→Xh` | n/a | n/a |
| 3 | EEF | EEPROM write access failed (VDD dropped below VDD:EEF ~1.3V). If pre-cleared to 0, later set to 1 on failure. | 0 | Yes | Write `0` to clear |
| 2 | EEbusy | EEPROM busy (read-only). 1 while EEPROM op in progress, incl. POR refresh (~66 ms). | `1→0` | n/a | auto-clears |
| 1 | CLKF | Clock/CLKOUT related flag (read/clear only). Used in Interrupt Controlled Clock Output scheme. | 0 | Yes | Read/clear only (read then clear per scheme) |
| 0 | BSF | Backup switchover flag (read/clear only). Set on automatic switchover (VDD↔VBACKUP). | 0 | Yes | Write `0` to clear (flag is read/clear-only per section text) |

Flag details in reset summary:
- Default flags after power-up include `PORF=1`, `VLF=0`, `EEF=0`, `CLKF=0`, `BSF=0` and EEbusy transitions `1→0`. Page 56. fileciteturn3file9L16-L30

---

## Control registers (interrupt enables, timer, stop)

### 10h — Control 1 (R/WP)
Page: 25. fileciteturn4file5L14-L23

| Bit | Name | Meaning | Reset |
|---:|---|---|---|
| 5 | GP0 | General purpose bit | 0 |
| 4 | USEL | Update interrupt selects **second** (0) or **minute** (1) | 0 |
| 3 | TE | Periodic countdown timer enable | 0 |
| 2 | EERD | EEPROM auto-refresh disable (1 disables) | 0 |
| 1:0 | TD | Timer clock frequency select (00=4096Hz, 01=64Hz, 10=1Hz, 11=1/60Hz) | 00 |

Details/table: page 25–26. fileciteturn4file10L37-L58

### 11h — Control 2 (R/WP)
Page: 26. fileciteturn4file8L6-L13

| Bit | Name | Meaning | Reset |
|---:|---|---|---|
| 6 | CLKIE | Enable Interrupt Controlled Clock Output on CLKOUT | 0 |
| 5 | UIE | Periodic time update interrupt enable | 0 |
| 4 | TIE | Periodic countdown timer interrupt enable | 0 |
| 3 | AIE | Alarm interrupt enable | 0 |
| 2 | EIE | External event interrupt enable (EVI) | 0 |
| 1 | GP1 | General purpose bit | 0 |
| 0 | STOP | Stop prescaler and 100th seconds (time sync helper) | 0 |

STOP behavior summary: when set, prescalers 4096→1Hz are stopped/reset and many peripherals pause (timer/update/temp/EVI filter/etc). Page 26–27. fileciteturn4file1L7-L21

### 12h — Control 3 (R/WP)
Page: 27. fileciteturn4file1L26-L33

| Bit | Name | Meaning | Reset |
|---:|---|---|---|
| 4 | BSIE | Backup switchover interrupt enable (INT pin retained until BSF cleared) | 0 |
| 3 | THE | Temperature High detect/time-stamp enable | 0 |
| 2 | TLE | Temperature Low detect/time-stamp enable | 0 |
| 1 | THIE | Temperature High interrupt enable (INT retained until THF cleared) | 0 |
| 0 | TLIE | Temperature Low interrupt enable (INT retained until TLF cleared) | 0 |

Retention note: INT stays asserted until the corresponding flag cleared (BSF/THF/TLF). fileciteturn4file1L35-L45

---

## Time stamp control & EVI

### 13h — Time Stamp Control (R/WP)
Page: 28–29. fileciteturn4file0L3-L11

| Bit | Name | Meaning | Reset | Clear behavior |
|---:|---|---|---|---|
| 5 | EVR | Reset TS EVI registers to 00h; may remain set, no further resets | 0 | Write 1 triggers reset once |
| 4 | THR | Reset TS THigh regs to 00h and clears THF; reads back 0 | 0 | Write 1 triggers reset, auto-returns 0 |
| 3 | TLR | Reset TS TLow regs to 00h and clears TLF; reads back 0 | 0 | Write 1 triggers reset, auto-returns 0 |
| 2 | EVOW | TS EVI overwrite control (0=first event, 1=last event) | 0 | n/a |
| 1 | THOW | TS THigh overwrite control | 0 | n/a |
| 0 | TLOW | TS TLow overwrite control | 0 | n/a |

---

## Interrupt-controlled CLKOUT

### 14h — Clock Interrupt Mask (R/WP)
Pages: 29–30. fileciteturn3file0L23-L35

| Bit | Name | Meaning | Reset |
|---:|---|---|---|
| 7 | CLKD | CLKOUT switch-off delay after I²C STOP select (0≈1.4ms, 1≈75ms) when CLKDE=1 | 0 |
| 6 | INTDE | Add delay (~3.9–5.9ms) after CLKOUT-on (wakeup aid), conditions apply | 0 |
| 5 | CEIE | Clock output when EVI interrupt enabled (select EI) | 0 |
| 4 | CAIE | Clock output when Alarm interrupt enabled (select AI) | 0 |
| 3 | CTIE | Clock output when timer interrupt enabled (select TI) | 0 |
| 2 | CUIE | Clock output when update interrupt enabled (select UI) | 0 |
| 1 | CTHIE | Clock output when THigh interrupt enabled (select THI) | 0 |
| 0 | CTLIE | Clock output when TLow interrupt enabled (select TLI) | 0 |

---

## External event detection (EVI pin)

### 15h — EVI Control (R/WP)
Page: 30. fileciteturn3file1L24-L35

| Bit | Name | Meaning | Reset |
|---:|---|---|---|
| 7 | CLKDE | Enable CLKOUT switch-off delay after I²C STOP (delay chosen by CLKD) | 0 |
| 6 | EHL | Event polarity: falling/low (0) vs rising/high (1) | 0 |
| 5:4 | ET | EVI filter: 00 edge/no filter; 01 tSP=3.9ms; 10 tSP=15.6ms; 11 tSP=125ms | 00 |
| 0 | ESYN | External-event synchronization (hardware time adjust) | 0 |

---

## EEPROM control (configuration + user EEPROM)

### 3Fh — EE Command (WP, write-only)
This is the command “doorbell” to refresh/update or read/write single EEPROM bytes.  
Page: 44. fileciteturn4file6L4-L43

**Required preconditions (library should enforce):**
1) Set `EERD=1` (disable auto refresh)  
2) Wait until `EEbusy=0`  
3) Use `EEF` to detect failed writes  
Page: 44 + 68. fileciteturn4file6L5-L9 fileciteturn4file7L8-L21

Command values:
- `0x11` UPDATE all config RAM mirror (`C0–CA`) → EEPROM
- `0x12` REFRESH all config EEPROM → RAM mirror (`C0–CA`)
- `0x21` WRITE one EEPROM byte: `EEDATA(RAM)` → EEPROM[`EEADDR`]
- `0x22` READ one EEPROM byte: EEPROM[`EEADDR`] → `EEDATA(RAM)`  
Page: 44. fileciteturn4file6L19-L42

### Auto refresh timing
- POR refresh time: ~66 ms (`EEbusy` indicates progress)  
- Periodic refresh: every 24 hours (at date increment), ~1.4 ms (manual also mentions ~3.5 ms in Control1 detail—treat as datasheet variance; measure if critical)  
Page: 68 + 25–26. fileciteturn4file7L15-L28 fileciteturn4file10L32-L36

---

## Write protection / password (summary)

- Password bytes `39h–3Ch` are write-only.
- EEPROM password enable stored at `CAh` (EEPWE). Writing 255 enables password protection; unlocking requires writing correct PW = EEPW; locking by writing incorrect PW.  
Pages: 115–116. fileciteturn3file6L5-L15 fileciteturn3file2L5-L13

---

## Reset behavior you should encode in unit tests

Default/reset snapshot highlights (RAM + EEPROM defaults on delivery):
- Many features disabled (alarm/timer/update/temp thresholds/interrupts), `PORF=1` after POR and can be cleared by writing 0.  
Page: 56–57. fileciteturn3file9L13-L30 fileciteturn3file9L40-L52

---

## Practical driver API suggestions

Expose these as high-level status checks:
- `rtc_get_validity()` → { `por:bool`, `vlow:bool`, `time_valid:bool` }  
  - `time_valid = !(PORF||VLF)` (recommended)
- `rtc_get_faults()` → { `eeprom_write_failed(EEF)`, `backup_switchover(BSF)`, `clock_flag(CLKF)` }
- Provide clear methods:
  - `rtc_clear_status_flags(mask)` (write 0s for bits to clear in 0Dh; document THF/TLF side effect)
  - `rtc_clear_fault_flags(mask)` (clear EEF, CLKF, BSF as appropriate)

---

## Page index (quick)
- Register organization/overview: 13–14 fileciteturn4file2L41-L66
- Status register 0Dh (PORF/VLF etc): 22–23 fileciteturn3file13L13-L13
- Temperature/EEF/EEbusy/CLKF/BSF: 23 fileciteturn3file13L16-L26
- Control 1/2/3 + STOP: 25–27 fileciteturn4file5L14-L23 fileciteturn4file8L6-L13 fileciteturn4file1L26-L33
- Time stamp control: 28–29 fileciteturn4file0L3-L11
- Clock interrupt mask + EVI control: 29–30 fileciteturn3file0L23-L35 fileciteturn3file1L24-L35
- EE command (EECMD) + user RAM: 44–45 fileciteturn4file6L4-L49
- EEPROM R/W flow & timings: 68 fileciteturn4file7L8-L41
- Password / write protection flows: 115–116 fileciteturn3file6L5-L15

---
