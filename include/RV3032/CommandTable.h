/**
 * @file CommandTable.h
 * @brief RV-3032-C7 command table definitions and register addresses.
 *
 * Contains all RV-3032-C7 registers and control addresses from the datasheet.
 * Use for direct register access via transport layer.
 *
 * @note All addresses are 7-bit I2C addresses. Register values are BCD unless noted.
 */

#pragma once

#include <stdint.h>

namespace RV3032 {

/**
 * @brief RV-3032-C7 register and command addresses.
 *
 * All registers from RV-3032-C7 Application Manual (Rev 1.3, May 2023).
 * Organized by functional group for clarity.
 */
namespace cmd {

// ========== Time / Calendar Registers (Volatile RAM) ==========

/// @brief 100th Seconds register (0x00, read-only)
/// BCD: b7-b0 = 80, 40, 20, 10, 8, 4, 2, 1 (hundredths of seconds)
static constexpr uint8_t REG_100TH_SECONDS = 0x00;

/// @brief Seconds register (0x01, read/write-protectable)
/// BCD: b7=reserved, b6-b0 = 40, 20, 10, 8, 4, 2, 1 (0–59 seconds)
static constexpr uint8_t REG_SECONDS = 0x01;

/// @brief Minutes register (0x02, read/write-protectable)
/// BCD: b7=reserved, b6-b0 = 40, 20, 10, 8, 4, 2, 1 (0–59 minutes)
static constexpr uint8_t REG_MINUTES = 0x02;

/// @brief Hours register (0x03, read/write-protectable)
/// BCD: b7-b6=reserved, b5-b0 = 20, 10, 8, 4, 2, 1 (0–23 hours, 24-hour mode)
static constexpr uint8_t REG_HOURS = 0x03;

/// @brief Weekday register (0x04, read/write-protectable)
/// BCD: b7-b3=reserved, b2-b0 = 4, 2, 1 (0–6, 0=Sunday)
static constexpr uint8_t REG_WEEKDAY = 0x04;

/// @brief Date/Day-of-Month register (0x05, read/write-protectable)
/// BCD: b7-b6=reserved, b5-b0 = 20, 10, 8, 4, 2, 1 (1–31)
static constexpr uint8_t REG_DATE = 0x05;

/// @brief Month register (0x06, read/write-protectable)
/// BCD: b7-b5=reserved, b4-b0 = 10, 8, 4, 2, 1 (1–12, 1=January)
static constexpr uint8_t REG_MONTH = 0x06;

/// @brief Year register (0x07, read/write-protectable)
/// BCD: b7-b0 = 80, 40, 20, 10, 8, 4, 2, 1 (year within century, 00–99)
static constexpr uint8_t REG_YEAR = 0x07;

// ========== Alarm Registers (0x08–0x0A) ==========

/// @brief Minutes Alarm register (0x08, read/write-protectable)
/// Bit 7 = AE_M (alarm enable for minutes), b6-b0 = BCD minutes (0–59)
static constexpr uint8_t REG_ALARM_MINUTE = 0x08;

/// @brief Hours Alarm register (0x09, read/write-protectable)
/// Bit 7 = AE_H (alarm enable for hours), b6-b0 = BCD hours (0–23)
static constexpr uint8_t REG_ALARM_HOUR = 0x09;

/// @brief Date Alarm register (0x0A, read/write-protectable)
/// Bit 7 = AE_D (alarm enable for date), b6-b0 = BCD date (1–31)
static constexpr uint8_t REG_ALARM_DATE = 0x0A;

// ========== Timer Registers (0x0B–0x0C) ==========

/// @brief Timer Value 0 (Low Byte) register (0x0B, read/write-protectable)
/// 8-bit value (0–255), LSB of 16-bit timer countdown
static constexpr uint8_t REG_TIMER_LOW = 0x0B;

/// @brief Timer Value 1 (High Byte) register (0x0C, read/write-protectable)
/// 8-bit value (0–255), MSB of 16-bit timer countdown
static constexpr uint8_t REG_TIMER_HIGH = 0x0C;

// ========== Status / Flag Registers (0x0D–0x0F) ==========

/// @brief Status register (0x0D, read/write-protectable)
/// Flags: VLF, PORF, EVF, AF, TF, UF, TLF, THF
/// @see StatusFlags for bit definitions
static constexpr uint8_t REG_STATUS = 0x0D;

/// @brief Temperature LSBs register (0x0E, read/write-protectable)
/// Lower 8 bits of temperature measurement (1/256 °C per LSB)
static constexpr uint8_t REG_TEMP_LSB = 0x0E;

/// @brief Temperature MSBs register (0x0F, read-only)
/// Upper 8 bits of temperature measurement (1 °C per LSB, two's complement)
static constexpr uint8_t REG_TEMP_MSB = 0x0F;

// ========== Control Registers (0x10–0x12) ==========

/// @brief Control 1 register (0x10, read/write-protectable)
/// Bits: TRPT, EERD, TE, TD1, TD0
/// @see Control1Bits for bit definitions
static constexpr uint8_t REG_CONTROL1 = 0x10;

/// @brief Control 2 register (0x11, read/write-protectable)
/// Bits: THFM, TLFM, UIE, TAFIE, TIE, AIE, OUT_A, OUT_B
/// @see Control2Bits for bit definitions
static constexpr uint8_t REG_CONTROL2 = 0x11;

/// @brief Control 3 register (0x12, read/write-protectable)
/// Reserved or for future use; typically read as 0x00
static constexpr uint8_t REG_CONTROL3 = 0x12;

// ========== Timestamp Control (0x13–0x15) ==========

/// @brief Timestamp Control register (0x13, read/write-protectable)
/// Bits: TSOW, TSOVF, TSOS, TSR1, TSR0, TSHR, TSMIN, TSSEC
static constexpr uint8_t REG_TS_CONTROL = 0x13;

/// @brief Clock Interrupt Mask register (0x14, read/write-protectable)
/// Mask bits for various interrupt sources
static constexpr uint8_t REG_CLOCK_INT_MASK = 0x14;

/// @brief EVI Control register (0x15, read/write-protectable)
/// Bits: EVI_EB, EVI_DB1, EVI_DB0, EVI_EN, EVI_DEB, reserved
static constexpr uint8_t REG_EVI_CONTROL = 0x15;

/// @brief TLow Threshold register (0x16, read/write-protectable)
/// Temperature low alarm threshold (two's complement, 1 °C per LSB)
static constexpr uint8_t REG_TLOW_THRESHOLD = 0x16;

/// @brief THigh Threshold register (0x17, read/write-protectable)
/// Temperature high alarm threshold (two's complement, 1 °C per LSB)
static constexpr uint8_t REG_THIGH_THRESHOLD = 0x17;

// ========== Timestamp Data: TLow (0x18–0x1E) ==========

/// @brief TS TLow Count register (0x18, read-only)
/// Count of temperature low events since last clear
static constexpr uint8_t REG_TS_TLOW_COUNT = 0x18;

/// @brief TS TLow Seconds register (0x19, read-only)
/// BCD seconds when TLow event was timestamped
static constexpr uint8_t REG_TS_TLOW_SECONDS = 0x19;

/// @brief TS TLow Minutes register (0x1A, read-only)
/// BCD minutes when TLow event was timestamped
static constexpr uint8_t REG_TS_TLOW_MINUTES = 0x1A;

/// @brief TS TLow Hours register (0x1B, read-only)
/// BCD hours when TLow event was timestamped
static constexpr uint8_t REG_TS_TLOW_HOURS = 0x1B;

/// @brief TS TLow Date register (0x1C, read-only)
/// BCD date when TLow event was timestamped
static constexpr uint8_t REG_TS_TLOW_DATE = 0x1C;

/// @brief TS TLow Month register (0x1D, read-only)
/// BCD month when TLow event was timestamped
static constexpr uint8_t REG_TS_TLOW_MONTH = 0x1D;

/// @brief TS TLow Year register (0x1E, read-only)
/// BCD year when TLow event was timestamped
static constexpr uint8_t REG_TS_TLOW_YEAR = 0x1E;

// ========== Timestamp Data: THigh (0x1F–0x25) ==========

/// @brief TS THigh Count register (0x1F, read-only)
/// Count of temperature high events since last clear
static constexpr uint8_t REG_TS_THIGH_COUNT = 0x1F;

/// @brief TS THigh Seconds register (0x20, read-only)
/// BCD seconds when THigh event was timestamped
static constexpr uint8_t REG_TS_THIGH_SECONDS = 0x20;

/// @brief TS THigh Minutes register (0x21, read-only)
/// BCD minutes when THigh event was timestamped
static constexpr uint8_t REG_TS_THIGH_MINUTES = 0x21;

/// @brief TS THigh Hours register (0x22, read-only)
/// BCD hours when THigh event was timestamped
static constexpr uint8_t REG_TS_THIGH_HOURS = 0x22;

/// @brief TS THigh Date register (0x23, read-only)
/// BCD date when THigh event was timestamped
static constexpr uint8_t REG_TS_THIGH_DATE = 0x23;

/// @brief TS THigh Month register (0x24, read-only)
/// BCD month when THigh event was timestamped
static constexpr uint8_t REG_TS_THIGH_MONTH = 0x24;

/// @brief TS THigh Year register (0x25, read-only)
/// BCD year when THigh event was timestamped
static constexpr uint8_t REG_TS_THIGH_YEAR = 0x25;

// ========== Timestamp Data: EVI (0x26–0x2D) ==========

/// @brief TS EVI Count register (0x26, read-only)
/// Count of external event inputs (EVI) since last clear
static constexpr uint8_t REG_TS_EVI_COUNT = 0x26;

/// @brief TS EVI 100th Seconds register (0x27, read-only)
/// BCD hundredths of seconds when EVI was timestamped
static constexpr uint8_t REG_TS_EVI_100TH_SECONDS = 0x27;

/// @brief TS EVI Seconds register (0x28, read-only)
/// BCD seconds when EVI was timestamped
static constexpr uint8_t REG_TS_EVI_SECONDS = 0x28;

/// @brief TS EVI Minutes register (0x29, read-only)
/// BCD minutes when EVI was timestamped
static constexpr uint8_t REG_TS_EVI_MINUTES = 0x29;

/// @brief TS EVI Hours register (0x2A, read-only)
/// BCD hours when EVI was timestamped
static constexpr uint8_t REG_TS_EVI_HOURS = 0x2A;

/// @brief TS EVI Date register (0x2B, read-only)
/// BCD date when EVI was timestamped
static constexpr uint8_t REG_TS_EVI_DATE = 0x2B;

/// @brief TS EVI Month register (0x2C, read-only)
/// BCD month when EVI was timestamped
static constexpr uint8_t REG_TS_EVI_MONTH = 0x2C;

/// @brief TS EVI Year register (0x2D, read-only)
/// BCD year when EVI was timestamped
static constexpr uint8_t REG_TS_EVI_YEAR = 0x2D;

// ========== Password / EEPROM Access (0x39–0x3F) ==========

/// @brief Password 0 register (0x39, write-only)
/// First byte of 4-byte write-protection password
static constexpr uint8_t REG_PASSWORD0 = 0x39;

/// @brief Password 1 register (0x3A, write-only)
/// Second byte of 4-byte write-protection password
static constexpr uint8_t REG_PASSWORD1 = 0x3A;

/// @brief Password 2 register (0x3B, write-only)
/// Third byte of 4-byte write-protection password
static constexpr uint8_t REG_PASSWORD2 = 0x3B;

/// @brief Password 3 register (0x3C, write-only)
/// Fourth byte of 4-byte write-protection password
static constexpr uint8_t REG_PASSWORD3 = 0x3C;

/// @brief EE Address register (0x3D, read/write-protectable)
/// Address pointer for EEPROM access (range 0xCB–0xEA for user EEPROM)
static constexpr uint8_t REG_EE_ADDRESS = 0x3D;

/// @brief EE Data register (0x3E, read/write-protectable)
/// Data byte for EEPROM read/write operations
static constexpr uint8_t REG_EE_DATA = 0x3E;

/// @brief EE Command register (0x3F, write-only)
/// Command for EEPROM operations (read 0x00 when read)
/// Typical command: 0x21 (EEPROM update)
static constexpr uint8_t REG_EE_COMMAND = 0x3F;

// ========== User RAM (0x40–0x4F) ==========

/// @brief User RAM start address (0x40, read/write-protectable)
/// 16 bytes of volatile user storage
static constexpr uint8_t REG_USER_RAM_START = 0x40;

/// @brief User RAM end address (0x4F, read/write-protectable)
/// Last byte of volatile user storage
static constexpr uint8_t REG_USER_RAM_END = 0x4F;

// ========== EEPROM Control (0xC0–0xCA) ==========

/// @brief EEPROM Power Management Unit register (0xC0, read/write-protectable)
/// Bit-managed register for power and oscillator control
static constexpr uint8_t REG_EEPROM_PMU = 0xC0;

/// @brief EEPROM Offset register (0xC1, read/write-protectable)
/// Calibration offset for crystal frequency
static constexpr uint8_t REG_EEPROM_OFFSET = 0xC1;

/// @brief EEPROM CLKOUT 1 register (0xC2, read/write-protectable)
/// Primary CLKOUT configuration in EEPROM
static constexpr uint8_t REG_EEPROM_CLKOUT1 = 0xC2;

/// @brief EEPROM CLKOUT 2 register (0xC3, read/write-protectable)
/// Secondary CLKOUT frequency / divider configuration in EEPROM
static constexpr uint8_t REG_EEPROM_CLKOUT2 = 0xC3;

/// @brief EEPROM TReference 0 register (0xC4, read/write-protectable)
/// Temperature compensation reference 0 in EEPROM
static constexpr uint8_t REG_EEPROM_TREFERENCE0 = 0xC4;

/// @brief EEPROM TReference 1 register (0xC5, read/write-protectable)
/// Temperature compensation reference 1 in EEPROM
static constexpr uint8_t REG_EEPROM_TREFERENCE1 = 0xC5;

/// @brief EEPROM Password 0 register (0xC6, write-only, EEPROM-backed)
/// EEPROM copy of first password byte
static constexpr uint8_t REG_EEPROM_PASSWORD0 = 0xC6;

/// @brief EEPROM Password 1 register (0xC7, write-only, EEPROM-backed)
/// EEPROM copy of second password byte
static constexpr uint8_t REG_EEPROM_PASSWORD1 = 0xC7;

/// @brief EEPROM Password 2 register (0xC8, write-only, EEPROM-backed)
/// EEPROM copy of third password byte
static constexpr uint8_t REG_EEPROM_PASSWORD2 = 0xC8;

/// @brief EEPROM Password 3 register (0xC9, write-only, EEPROM-backed)
/// EEPROM copy of fourth password byte
static constexpr uint8_t REG_EEPROM_PASSWORD3 = 0xC9;

/// @brief EEPROM Password Enable register (0xCA, write-only)
/// Register to enable/manage password protection
static constexpr uint8_t REG_EEPROM_PW_ENABLE = 0xCA;

// ========== User EEPROM (0xCB–0xEA) ==========

/// @brief User EEPROM start address (0xCB)
/// 32 bytes of non-volatile user storage
/// Accessed via REG_EE_ADDRESS, REG_EE_DATA, REG_EE_COMMAND
static constexpr uint8_t USER_EEPROM_START = 0xCB;

/// @brief User EEPROM end address (0xEA)
/// Last byte of non-volatile user storage
static constexpr uint8_t USER_EEPROM_END = 0xEA;

// ========== Register Bit Masks & Control Values ==========

// Status register bits (REG_STATUS, 0x0D)
static constexpr uint8_t STATUS_VLF_BIT = 0;          ///< Voltage Low Flag
static constexpr uint8_t STATUS_PORF_BIT = 1;         ///< Power-On Reset Flag
static constexpr uint8_t STATUS_EVF_BIT = 2;          ///< External Event Flag
static constexpr uint8_t STATUS_AF_BIT = 3;           ///< Alarm Flag
static constexpr uint8_t STATUS_TF_BIT = 4;           ///< Timer Flag
static constexpr uint8_t STATUS_UF_BIT = 5;           ///< Update Flag
static constexpr uint8_t STATUS_TLF_BIT = 6;          ///< Temperature Low Flag
static constexpr uint8_t STATUS_THF_BIT = 7;          ///< Temperature High Flag

// Control 1 register bits (REG_CONTROL1, 0x10)
static constexpr uint8_t CTRL1_TRPT_BIT = 7;          ///< Timer Repeat
static constexpr uint8_t CTRL1_EERD_BIT = 2;          ///< EEPROM Refresh/Read
static constexpr uint8_t CTRL1_TE_BIT = 3;            ///< Timer Enable
static constexpr uint8_t CTRL1_TD_MASK = 0x03;        ///< Timer Divisor (2 bits)
static constexpr uint8_t CTRL1_TD_SHIFT = 0;

// Control 2 register bits (REG_CONTROL2, 0x11)
static constexpr uint8_t CTRL2_THFM_BIT = 7;          ///< Temperature High Flag Mask
static constexpr uint8_t CTRL2_TLFM_BIT = 6;          ///< Temperature Low Flag Mask
static constexpr uint8_t CTRL2_UIE_BIT = 5;           ///< Update Interrupt Enable
static constexpr uint8_t CTRL2_TAFIE_BIT = 4;         ///< Timer Alarm Flag Interrupt Enable
static constexpr uint8_t CTRL2_TIE_BIT = 3;           ///< Timer Interrupt Enable
static constexpr uint8_t CTRL2_AIE_BIT = 2;           ///< Alarm Interrupt Enable
static constexpr uint8_t CTRL2_OUT_A_BIT = 1;         ///< Output A
static constexpr uint8_t CTRL2_OUT_B_BIT = 0;         ///< Output B

// Timestamp Control register bits (REG_TS_CONTROL, 0x13)
static constexpr uint8_t TS_OVERWRITE_BIT = 2;        ///< Timestamp overwrite enable

// EVI Control register bits (REG_EVI_CONTROL, 0x15)
static constexpr uint8_t EVI_EB_BIT = 6;              ///< EVI Edge Bit (0=fall, 1=rise)
static constexpr uint8_t EVI_DB_MASK = 0x30;          ///< EVI Debounce mask (2 bits)
static constexpr uint8_t EVI_DB_SHIFT = 4;
static constexpr uint8_t EVI_EN_BIT = 3;              ///< EVI Enable

// EEPROM PMU register bits (REG_EEPROM_PMU, 0xC0)
static constexpr uint8_t PMU_CLKOUT_DISABLE = 0x40;   ///< CLKOUT disable bit
static constexpr uint8_t PMU_BSM_MASK = 0x30;         ///< Backup Switching Mode mask
static constexpr uint8_t PMU_BSM_LEVEL = 0x20;        ///< BSM: Level switching mode
static constexpr uint8_t PMU_BSM_DIRECT = 0x10;       ///< BSM: Direct switching mode

// EEPROM CLKOUT 2 register bits (REG_EEPROM_CLKOUT2, 0xC3)
static constexpr uint8_t CLKOUT_FREQ_MASK = 0x60;     ///< CLKOUT frequency select mask
static constexpr uint8_t CLKOUT_FREQ_SHIFT = 5;       ///< CLKOUT frequency bit shift

// EEPROM Command values
static constexpr uint8_t EEPROM_CMD_UPDATE = 0x21;    ///< EEPROM update/write command
static constexpr uint8_t EEPROM_BUSY_BIT = 2;         ///< EEPROM operation busy flag (in REG_TEMP_LSB)
static constexpr uint8_t EEPROM_ERROR_BIT = 3;        ///< EEPROM operation error flag (in REG_TEMP_LSB)
static constexpr uint8_t TEMP_CLKF_BIT = 1;           ///< Clock flag (in REG_TEMP_LSB)
static constexpr uint8_t TEMP_BSF_BIT = 0;            ///< Backup switchover flag (in REG_TEMP_LSB)

// I2C Address
static constexpr uint8_t I2C_ADDR_7BIT = 0x51;        ///< 7-bit I2C slave address

}  // namespace cmd

}  // namespace RV3032
