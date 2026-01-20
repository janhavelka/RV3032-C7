# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.2.0] - 2026-01-20

### Added
- EEPROM write statistics: `eepromWriteCount()`, `eepromWriteFailures()`, `eepromQueueDepth()`
- Driver health tracking with `DriverState` (READY, DEGRADED, OFFLINE)
- Health diagnostics: `state()`, `consecutiveFailures()`, `totalFailures()`, `totalSuccess()`
- Recovery API: `recover()` for manual device recovery after failures
- Probe function for diagnostic checks without affecting driver health
- `_beginInProgress` flag for proper initialization state management

### Changed
- Improved health tracking architecture with tracked vs raw transport wrappers
- Enhanced `begin()` and `recover()` to properly track I2C failures
- Reduced EEPROM write timeout for improved performance
- Disabled verbose mode by default in examples

### Fixed
- Initialization state management during `begin()` execution
- Health tracking now correctly excludes validation errors

## [1.1.0] - 2026-01-12

### Added
- Non-blocking EEPROM commit state machine (tick-driven) with `isEepromBusy()` and `getEepromLastStatus()`
- Status/validity helpers: `readStatusFlags()`, `readValidity()`, `clearBackupSwitchFlag()`
- Conversion helpers: `bcdToBinary()`, `binaryToBcd()`, `unixToDateTime()`, `dateTimeToUnix()`
- Host-side unit tests with a native test environment and CI job

### Changed
- Added `eepromNonBlocking` config option (default true)
- EEPROM writes can return `BUSY` when a commit is already in progress
- CLI example includes validity and backup switchover helpers

### Fixed
- N/A

### Removed
- N/A

## [1.0.0] - 2026-01-10

### Added
- Complete RV-3032-C7 RTC driver implementation
- `begin(Config) -> Status`, `tick(now_ms)`, `end()` lifecycle API
- Time/date operations: `readTime()`, `setTime()`, `readUnix()`, `setUnix()`
- Unix timestamp support with automatic date conversion
- Alarm functionality: configurable time matching and interrupt output
- Periodic countdown timer with programmable frequency
- External event input (EVI) with debouncing and timestamping
- Programmable CLKOUT output (32.768 kHz to 1 Hz)
- Frequency offset calibration in PPM (±200 ppm range)
- Built-in temperature sensor reading (±3°C accuracy)
- Battery backup modes: Off, Level (threshold), Direct (immediate)
- EEPROM persistence for configuration (optional, off by default)
- Status error model with detailed error codes
- Static utility functions: `isValidDateTime()`, `computeWeekday()`, `parseBuildTime()`
- Interactive CLI example (`01_basic_bringup_cli`) demonstrating all features
- Comprehensive Doxygen documentation in public headers
- Auto-generated version constants from library.json
- GitHub Actions CI for ESP32-S2 and ESP32-S3

### Changed
- N/A (initial RV3032 release)

### Removed
- N/A (initial RV3032 release)

[Unreleased]: https://github.com/janhavelka/RV3032-C7/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/janhavelka/RV3032-C7/releases/tag/v1.0.0
