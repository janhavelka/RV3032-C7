/**
 * @file CommandTable.h
 * @brief RV-3032-C7 command table definitions and register addresses.
 *
 * Contains all RV-3032-C7 registers and control addresses from the datasheet.
 * Side-effecting addresses are available to driver internals but are not
 * necessarily accepted by the public raw-access allowlists.
 *
 * @note These are register or indirect-memory addresses, not I2C addresses.
 *       Only fields documented as BCD use BCD encoding.
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
/// Binary: b7-b3=reserved, b2-b0 = user-assigned value 0..6. It is not BCD
/// and is not required to match the calendar date.
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
/// Bit 7 = AE_M (0 enables minute matching), b6-b0 = BCD minutes (0–59)
static constexpr uint8_t REG_ALARM_MINUTE = 0x08;

/// @brief Hours Alarm register (0x09, read/write-protectable)
/// Bit 7 = AE_H (0 enables hour matching), bit 6 reserved, b5-b0 = BCD hours
static constexpr uint8_t REG_ALARM_HOUR = 0x09;

/// @brief Date Alarm register (0x0A, read/write-protectable)
/// Bit 7 = AE_D (0 enables date matching), bit 6 reserved, b5-b0 = BCD date
static constexpr uint8_t REG_ALARM_DATE = 0x0A;

// ========== Timer Registers (0x0B–0x0C) ==========

/// @brief Timer Value 0 (Low Byte) register (0x0B, read/write-protectable)
/// Bits 7:0 are the low byte of the 12-bit countdown.
/// Valid running presets are 1..4095; preset 0 does not start the timer.
static constexpr uint8_t REG_TIMER_LOW = 0x0B;

/// @brief Timer Value 1 (High Nibble) register (0x0C, read/write-protectable)
/// Bits 3:0 are the high nibble; bits 7:4 are reserved and written zero.
static constexpr uint8_t REG_TIMER_HIGH = 0x0C;

// ========== Status / Flag Registers (0x0D–0x0F) ==========

/// @brief Status register (0x0D, read/write-protectable)
/// Flags: VLF, PORF, EVF, AF, TF, UF, TLF, THF
/// @see StatusFlags for bit definitions
static constexpr uint8_t REG_STATUS = 0x0D;

/// @brief Temperature LSB/support-flags register (0x0E)
/// Bits 7:4 are the fractional temperature nibble; bits 3:0 are EEF, EEBUSY,
/// CLKF, and BSF support flags with documented write-clear semantics.
static constexpr uint8_t REG_TEMP_LSB = 0x0E;

/// @brief Temperature integer register (0x0F, read-only)
/// Signed integer portion of the 12-bit two's-complement temperature value.
static constexpr uint8_t REG_TEMP_MSB = 0x0F;

// ========== Control Registers (0x10–0x12) ==========

/// @brief Control 1 register (0x10, read/write-protectable)
/// Bits: GP0, USEL, TE, EERD, TD1, TD0; bits 7:6 reserved.
/// @see Control1Bits for bit definitions
static constexpr uint8_t REG_CONTROL1 = 0x10;

/// @brief Control 2 register (0x11, read/write-protectable)
/// Bits: CLKIE, UIE, TIE, AIE, EIE, GP1, STOP; bit 7 reserved.
/// @see Control2Bits for bit definitions
static constexpr uint8_t REG_CONTROL2 = 0x11;

/// @brief Control 3 register (0x12, read/write-protectable)
/// Bits: BSIE, THE, TLE, THIE, TLIE; bits 7:5 reserved.
static constexpr uint8_t REG_CONTROL3 = 0x12;

// ========== Timestamp Control (0x13–0x15) ==========

/// @brief Timestamp Control register (0x13, read/write-protectable)
/// Bits: EVR, THR, TLR, EVOW, THOW, TLOW; bits 7:6 read as 0.
static constexpr uint8_t REG_TS_CONTROL = 0x13;

/// @brief Clock Interrupt Mask register (0x14, read/write-protectable)
/// Mask bits for various interrupt sources
static constexpr uint8_t REG_CLOCK_INT_MASK = 0x14;

/// @brief EVI Control register (0x15, read/write-protectable)
/// Bits: CLKDE, EHL, EVI_DB1, EVI_DB0, ESYN. There is no enable bit 3.
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
/// Address pointer for configuration/user EEPROM access (range 0xC0–0xEA)
static constexpr uint8_t REG_EE_ADDRESS = 0x3D;

/// @brief EE Data register (0x3E, read/write-protectable)
/// Data byte for EEPROM read/write operations
static constexpr uint8_t REG_EE_DATA = 0x3E;

/// @brief EE Command register (0x3F, write-only/read-zero)
/// WRITE_ONE=0x21 and READ_ONE=0x22; UPDATE_ALL=0x11 and REFRESH_ALL=0x12.
static constexpr uint8_t REG_EE_COMMAND = 0x3F;

// ========== User RAM (0x40–0x4F) ==========

/// @brief User RAM start address (0x40, read/write-protectable)
/// 16 bytes of volatile user storage
static constexpr uint8_t REG_USER_RAM_START = 0x40;

/// @brief User RAM end address (0x4F, read/write-protectable)
/// Last byte of volatile user storage
static constexpr uint8_t REG_USER_RAM_END = 0x4F;

// ========== Active configuration mirrors (0xC0–0xCA) ==========

/// @brief Active PMU mirror (0xC0, read/write-protectable)
/// Direct access changes active state only; persistent proof uses READ_ONE.
static constexpr uint8_t REG_ACTIVE_PMU = 0xC0;

/// @brief Active offset mirror (0xC1, read/write-protectable)
static constexpr uint8_t REG_ACTIVE_OFFSET = 0xC1;

/// @brief Active CLKOUT 1 mirror (0xC2, read/write-protectable)
static constexpr uint8_t REG_ACTIVE_CLKOUT1 = 0xC2;

/// @brief Active CLKOUT 2 mirror (0xC3, read/write-protectable)
static constexpr uint8_t REG_ACTIVE_CLKOUT2 = 0xC3;

/// @brief Active temperature-reference 0 mirror (0xC4)
static constexpr uint8_t REG_ACTIVE_TREFERENCE0 = 0xC4;

/// @brief Active temperature-reference 1 mirror (0xC5)
static constexpr uint8_t REG_ACTIVE_TREFERENCE1 = 0xC5;

/// @brief Configuration-EEPROM RAM mirror for persistent password byte 0
/// (0xC6, write-only/read-zero)
static constexpr uint8_t REG_EEPROM_PASSWORD0 = 0xC6;

/// @brief Configuration-EEPROM RAM mirror for persistent password byte 1
/// (0xC7, write-only/read-zero)
static constexpr uint8_t REG_EEPROM_PASSWORD1 = 0xC7;

/// @brief Configuration-EEPROM RAM mirror for persistent password byte 2
/// (0xC8, write-only/read-zero)
static constexpr uint8_t REG_EEPROM_PASSWORD2 = 0xC8;

/// @brief Configuration-EEPROM RAM mirror for persistent password byte 3
/// (0xC9, write-only/read-zero)
static constexpr uint8_t REG_EEPROM_PASSWORD3 = 0xC9;

/// @brief Configuration-EEPROM RAM mirror for persistent password enable
/// (0xCA, write-only/read-zero)
static constexpr uint8_t REG_EEPROM_PW_ENABLE = 0xCA;

// ========== User EEPROM (0xCB–0xEA) ==========

/// @brief User EEPROM start address (0xCB)
/// 32 bytes of non-volatile user storage
/// Accessed via REG_EE_ADDRESS, REG_EE_DATA, REG_EE_COMMAND
static constexpr uint8_t CONFIG_EEPROM_START = 0xC0;
static constexpr uint8_t CONFIG_EEPROM_END = 0xCA;
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
/// Lower six Status flags are write-zero-to-clear; writing one preserves them.
static constexpr uint8_t STATUS_W0C_PRESERVE_MASK = 0x3F;
/// Verified calendar-set payload: preserve bits 7:2 and clear PORF/VLF.
/// THF/TLF still clear unconditionally in silicon on every Status write.
static constexpr uint8_t STATUS_CLEAR_INVALID_TIME_VALUE = 0xFC;

// Control 1 register bits (REG_CONTROL1, 0x10)
static constexpr uint8_t CONTROL1_IMPLEMENTED_MASK = 0x3F;
static constexpr uint8_t CTRL1_GP0_BIT = 5;
static constexpr uint8_t CTRL1_USEL_BIT = 4;
static constexpr uint8_t CTRL1_TE_BIT = 3;
static constexpr uint8_t CTRL1_EERD_BIT = 2;
static constexpr uint8_t CONTROL1_EERD_MASK = 0x04;
static constexpr uint8_t CTRL1_TD_MASK = 0x03;
static constexpr uint8_t CTRL1_TD_SHIFT = 0;

// Control 2 register bits (REG_CONTROL2, 0x11)
static constexpr uint8_t CONTROL2_IMPLEMENTED_MASK = 0x7F;
static constexpr uint8_t CTRL2_CLKIE_BIT = 6;
static constexpr uint8_t CTRL2_UIE_BIT = 5;
static constexpr uint8_t CTRL2_TIE_BIT = 4;
static constexpr uint8_t CTRL2_AIE_BIT = 3;
static constexpr uint8_t CTRL2_EIE_BIT = 2;
static constexpr uint8_t CTRL2_GP1_BIT = 1;
static constexpr uint8_t CTRL2_STOP_BIT = 0;

// Control 3 register bits (REG_CONTROL3, 0x12)
static constexpr uint8_t CONTROL3_IMPLEMENTED_MASK = 0x1F;
static constexpr uint8_t CTRL3_BSIE_BIT = 4;
static constexpr uint8_t CTRL3_THE_BIT = 3;
static constexpr uint8_t CTRL3_TLE_BIT = 2;
static constexpr uint8_t CTRL3_THIE_BIT = 1;
static constexpr uint8_t CTRL3_TLIE_BIT = 0;

// Timestamp Control register bits (REG_TS_CONTROL, 0x13)
static constexpr uint8_t TS_TLOW_OVERWRITE_BIT = 0;   ///< TLow timestamp overwrite enable
static constexpr uint8_t TS_THIGH_OVERWRITE_BIT = 1;  ///< THigh timestamp overwrite enable
static constexpr uint8_t TS_EVI_OVERWRITE_BIT = 2;    ///< EVI timestamp overwrite enable
static constexpr uint8_t TS_TLOW_RESET_BIT = 3;        ///< Reset TLow timestamp
static constexpr uint8_t TS_THIGH_RESET_BIT = 4;       ///< Reset THigh timestamp
static constexpr uint8_t TS_EVI_RESET_BIT = 5;         ///< Reset EVI timestamp
static constexpr uint8_t TS_CONTROL_IMPLEMENTED_MASK = 0x3F;
static constexpr uint8_t TS_CONTROL_OVERWRITE_MASK = 0x07;
/// EVR may read back as 1; TLR and THR always read back as 0.
static constexpr uint8_t TS_CONTROL_READBACK_MASK = 0x27;

// Clock Interrupt Mask register bits (REG_CLOCK_INT_MASK, 0x14)
static constexpr uint8_t CLOCK_INT_MASK_IMPLEMENTED_MASK = 0xFF;
static constexpr uint8_t CLOCK_INT_CLKD_BIT = 7;
static constexpr uint8_t CLOCK_INT_INTDE_BIT = 6;
static constexpr uint8_t CLOCK_INT_CEIE_BIT = 5;
static constexpr uint8_t CLOCK_INT_CAIE_BIT = 4;
static constexpr uint8_t CLOCK_INT_CTIE_BIT = 3;
static constexpr uint8_t CLOCK_INT_CUIE_BIT = 2;
static constexpr uint8_t CLOCK_INT_CTHIE_BIT = 1;
static constexpr uint8_t CLOCK_INT_CTLIE_BIT = 0;

// EVI Control register bits (REG_EVI_CONTROL, 0x15)
static constexpr uint8_t EVI_CLKDE_BIT = 7;
static constexpr uint8_t EVI_EB_BIT = 6;              ///< EHL: falling/low or rising/high
static constexpr uint8_t EVI_DB_MASK = 0x30;          ///< EVI Debounce mask (2 bits)
static constexpr uint8_t EVI_DB_SHIFT = 4;
static constexpr uint8_t EVI_ESYN_BIT = 0;
static constexpr uint8_t EVI_IMPLEMENTED_MASK = 0xF1;

// Active/persistent PMU byte fields (C0)
/// Factory-delivery C0 value: direct CLKOUT enabled, backup/charger disabled.
static constexpr uint8_t PMU_DEFAULT_ON_DELIVERY = 0x00;
static constexpr uint8_t PMU_IMPLEMENTED_MASK = 0x7F;
static constexpr uint8_t PMU_NCLKE_MASK = 0x40;
static constexpr uint8_t PMU_BSM_MASK = 0x30;         ///< Backup Switching Mode mask
static constexpr uint8_t PMU_BSM_DISABLED = 0x00;
static constexpr uint8_t PMU_BSM_DISABLED_ALT = 0x30; ///< Alternate disabled encoding
static constexpr uint8_t PMU_BSM_LEVEL = 0x20;        ///< BSM: Level switching mode
static constexpr uint8_t PMU_BSM_DIRECT = 0x10;       ///< BSM: Direct switching mode
static constexpr uint8_t PMU_TCR_MASK = 0x0C;
static constexpr uint8_t PMU_TCM_MASK = 0x03;
static constexpr uint8_t PMU_PRIMARY_PRESERVE_MASK = 0x4C;

// Active/persistent Offset byte fields (C1)
static constexpr uint8_t OFFSET_REGISTER_IMPLEMENTED_MASK = 0xFF;
static constexpr uint8_t OFFSET_PORIE_MASK = 0x80;
static constexpr uint8_t OFFSET_VLIE_MASK = 0x40;
static constexpr uint8_t OFFSET_VALUE_MASK = 0x3F;

// Active/persistent CLKOUT 1/2 byte fields (C2/C3)
/// Factory-delivery C2 value: HFD[7:0] = 0 (stored HF selection 8.192 kHz).
static constexpr uint8_t CLKOUT1_DEFAULT_ON_DELIVERY = 0x00;
/// Factory-delivery C3 value: OS=0, FD=00, HFD[12:8]=0 (XTAL 32.768 kHz).
static constexpr uint8_t CLKOUT2_DEFAULT_ON_DELIVERY = 0x00;
static constexpr uint8_t CLKOUT_FREQ_MASK = 0x60;     ///< CLKOUT frequency select mask
static constexpr uint8_t CLKOUT_FREQ_SHIFT = 5;       ///< CLKOUT frequency bit shift
static constexpr uint8_t CLKOUT_OS_MASK = 0x80;
static constexpr uint8_t CLKOUT_HFD_HIGH_MASK = 0x1F;

// EEPROM Command values
static constexpr uint8_t EEPROM_CMD_UPDATE_ALL = 0x11;
static constexpr uint8_t EEPROM_CMD_REFRESH_ALL = 0x12;
static constexpr uint8_t EEPROM_CMD_WRITE_ONE = 0x21;
static constexpr uint8_t EEPROM_CMD_READ_ONE = 0x22;
static constexpr uint8_t EEPROM_BUSY_BIT = 2;
static constexpr uint8_t EEPROM_BUSY_MASK = 0x04;
static constexpr uint8_t EEPROM_ERROR_BIT = 3;
static constexpr uint8_t EEPROM_EEF_MASK = 0x08;
static constexpr uint8_t TEMP_CLKF_BIT = 1;
static constexpr uint8_t TEMP_CLKF_MASK = 0x02;
static constexpr uint8_t TEMP_BSF_BIT = 0;
static constexpr uint8_t TEMP_BSF_MASK = 0x01;
static constexpr uint8_t TEMP_LSB_W0C_MASK =
    EEPROM_EEF_MASK | TEMP_CLKF_MASK | TEMP_BSF_MASK; // 0x0B
static constexpr uint8_t EEPROM_CLEAR_EEF_VALUE = 0x03;
static constexpr uint8_t PERSISTENT_READ_SENTINEL = 0x80;

// I2C Address
static constexpr uint8_t I2C_ADDR_7BIT = 0x51;        ///< 7-bit I2C slave address

}  // namespace cmd

}  // namespace RV3032
