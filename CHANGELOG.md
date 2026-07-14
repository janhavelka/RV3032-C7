# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.0.0] - 2026-07-14

### Added

- Typed PORF/VLF evidence in status-first calendar snapshots and semantic
  persistent-target/active-target proof in primary-cell reports.
- Status-first calendar snapshot and verified set/invalid-flag-clear jobs with
  fixed typed result storage, instruction budgets, deadlines, and ambiguous
  write reconciliation.
- Explicit configuration/user EEPROM read jobs, verified bounded user EEPROM
  writes, and typed password/protection evidence.
- `ensurePrimaryCellConfiguration()` as the sole synchronous multi-callback
  operation, with exact C0 policy, once-per-lifecycle admission, bounded
  yielding waits, durable verification, and safety cleanup reporting.
- Typed periodic-update, timer/event/clock interrupts, temperature events,
  complete CLKOUT, independent trickle-mode/resistance, STOP, synchronization,
  and general-purpose-bit coverage.
- Typed clock-output interrupt flag inspection and cooperative W0C-safe clear.
- Typed hundredths, PORIE/VLIE, EVI synchronization/CLKOUT-delay readback, and
  hardware EEbusy/EEF inspection plus guarded stale-EEF clearing.
- Two-sample cooperative temperature reads with explicit incoherence errors,
  plus typed THF/TLF inspection and combined clearing.
- Protocol-faithful native fake with separate active and persistent state,
  EEPROM command timing, W0C semantics, read/status/ambiguity transfer evidence,
  owner-side read-retry timing, bounded logs, and fault injection.

### Changed

- Calendar weekday values are user-assigned: every value `0..6` is accepted
  without Gregorian-date agreement, and setters preserve the supplied value.
- `begin()` is now passive and performs zero I2C/wait callbacks; `probe()` is
  the explicit raw presence check and `recover()` is one tracked read-only
  health re-probe.
- OFFLINE is observational and no longer suppresses an explicitly requested
  transport operation.
- Ordinary multi-transfer setup work uses the cooperative job/EEPROM engines;
  caller budgets of `0`, `1`, or `N` are honored.
- Generic persistence now uses safe PMU/EERD access, compare-before-write,
  write-one only, adaptive direct-read proof, durable verification, minimum
  waits measured after callback completion, intended-active-mirror restoration,
  verified cleanup, and hard-deadline cleanup-failure evidence.
  Cleanup failure cancels later queued entries; update-all/refresh-all are not
  used.
- Ordinary generic-persistence item failures now remain the observable batch
  status while later queued entries can be advanced; a new batch resets it.
- Raw register access is restricted to reviewed direct read/write allowlists.
- Misleading `REG_EEPROM_*` aliases for active C0-C5 mirrors were removed;
  active-only register names are now explicit.
- The CLI example starts with passive begin plus explicit probe, keeps generic
  persistence disabled, and requires the exact command
  `primary-cell ensure CONFIRM-PRIMARY-CELL` for provisioning.
- Destructive HIL execution now requires and records fresh port/module,
  primary-cell chemistry, power, possible-C0-write, and backfeed scope.

### Removed

- `Config::backupMode`, lifecycle PMU application, and the unsafe
  `setPrimaryBatteryBackupDefaults()` helper.

### Fixed

- Verified calendar set now writes the fixed Status payload `0xFC`, preserving
  UF/TF/AF/EVF that assert between its cooperative pre-clear read and write
  while retaining evidence of the unavoidable THF/TLF clear.
- Control 1/2/3, EVI, timer-high, PMU TCR/TCM, CLKOUT, alarm, TEMP_LSB, and
  EEPROM command encodings now match the vendor register tables.
- Calendar reads reject reserved bits and out-of-range weekdays; offset input
  outside the exact signed six-bit range is rejected instead of clamped.
- Timer readback rejects reserved high bits; password cleanup deterministically
  loads a credential distinct from both old and new references.
- Timer preset zero is rejected, C1 interrupt bits survive offset/CLKOUT
  updates, legacy CLKOUT frequency reads reject high-frequency mode, and a
  repeated EVI timestamp reset performs the required cooperative EVR 0-to-1
  transition.
- Password changes now persist and activate protection-disable before changing
  the reference, establish the new credential before protected cleanup, and
  lock only after cleanup and optional re-enable.
- Persistence-producing setters can now fill the fixed queue while its engine
  is idle; capacity is proven before active mutation, and coalescing uses the
  newest pending value so a later revert is not silently discarded.
- Primary-cell cleanup reports retain the specific `SETTLE` failure stage
  instead of flattening a level-switch settle timeout to generic cleanup.
- Primary READ_ONE/WRITE_ONE completion deadlines now include their mandatory
  1/10 ms waits, and direct persistent-read failures report the correct stage.
- Primary-cell callback admission now preserves the full cleanup reserve,
  records command attempts only at dispatch, stops after late callbacks,
  classifies prepare/READ_ONE/EEF failures exactly, and readback-classifies an
  ambiguous active-PMU write before any safety cleanup mutation.
- Trusted level-switch settling is measured from verified target-C0 readback;
  guarded Status clearing preserves unnamed lower-six flags that assert between
  its cooperative read and write callbacks.
- The native fake preserves the read-only TEMP_LSB temperature fraction and
  ignores EECMD presented while EEbusy, matching the vendor behavior.
- Cooperative hard deadlines now remain active when their wrap-safe value is
  exactly zero and are refreshed between callbacks in larger-budget polls.
- Generic EEPROM polling now refreshes elapsed time between callbacks, keeps
  the minimum accepted busy-poll window usable after the mandatory 10 ms wait,
  reconciles post-write EEF through direct readback, and reports only proven
  bytes after a partial persistent read.
- Queued C0..C5 persistence now restores and verifies the requested active
  mirror, including when an earlier active-write callback was acknowledged but
  ignored.
- Verified calendar writes preserve the caller weekday, retain readback proof for
  an ambiguous Status write, and expose complete partial-result evidence.
- Alarm date zero now restores the vendor-deactivated comparator state; all-AE
  every-minute behavior is documented and covered.
- CLKOUT reconfiguration now retains FD/HFD, stops direct output before changing
  frequency/mode, and rejects enabled/active interrupt-controlled output before
  mutation. Temperature configuration stages detection around independent
  threshold changes, and offset conversion uses the exact nominal step.

## [1.6.0] - 2026-06-29

### Added
- Hardware-in-the-loop CLI runner for bounded RTC validation and opt-in destructive soak scenarios.
- Maintained HIL summary report under `docs/reports/HIL_SUMMARY.md`; raw runner transcripts and generated step logs are not kept in the repository.
- Documentation/package contract validation for source trees and packaged archives.

### Changed
- `Config::enableEepromWrites` now defaults to `false`; EEPROM persistence is explicit opt-in for configuration changes.
- Core health timestamping no longer falls back to Arduino `millis()` when `Config::nowMs` is unset.
- Documentation was simplified into maintained `docs/README.md`, `docs/ARCHITECTURE.md`, `docs/DEVICE_REFERENCE.md`, and `docs/IDF_PORT.md`; vendor PDFs moved under `docs/reference-pdfs/`, while compact source-derived chip notes remain under `docs/extracted-md/` for traceability.
- Package exports now exclude raw HIL runner JSON, stdout/stderr captures, PID files, and generated runner transcripts.

## [1.5.0] - 2026-05-14

### Added
- Backup PMU helpers for reading and setting backup switchover mode, plus primary-cell defaults that select level switching and disable the trickle charger.
- CLI `backup` command to show PMU/validity state and apply the usual primary-cell time-retention setup.
- `SettingsSnapshot` struct for reading cached configuration, EEPROM state, and health counters without I2C.
- `getSettings(SettingsSnapshot&)` method to populate a settings snapshot.
- `Status::is(Err)` method for type-safe error code comparison.
- `Status::operator bool()` explicit conversion for concise success checks.
- `readRegisters()` and `writeRegisters()` public block-register access methods.
- Native coverage proving latched `OFFLINE` blocks normal I2C operations without touching the bus while `recover()` remains the explicit recovery path.

### Changed
- Doxyfile project metadata now matches `library.json`.
- Reference documentation now uses human-readable vendor PDF names.
- Explicit recovery bypass internals now use the shared `ScopedOfflineI2cAllowance` / `_reassertOfflineLatch()` procedure so failed recovery attempts that begin from `OFFLINE` keep the latch asserted.
- Low-level register access now rejects undocumented address ranges, wraparound blocks, and invalid buffers before dispatching to I2C.
- Transport validation/configuration errors are excluded from driver health counters while EEPROM `IN_PROGRESS` remains a successful in-flight state.
- Health behavior is now standardized on latched `OFFLINE`: normal public I2C operations and EEPROM `tick()` work return/hold off without touching I2C until `recover()` succeeds.

### Fixed
- `begin()` now rejects invalid `BackupSwitchMode` enum values instead of silently treating them as backup-off configuration.
- `getAlarmConfig()` now accepts the documented POR alarm state where `AE_D=0` and Date Alarm is `00h`, preventing the safe self-test from failing on a reset/default RTC.

## [1.4.1] - 2026-04-05

### Changed
- README now correctly describes the current public EVI surface as configuration support rather than exposed timestamp readout.
- README configuration and API tables now include the current health/recovery accessors, timing hooks, validity-flag helpers, and `offlineThreshold`.
- Top-level Doxygen and README version examples now match the current release-prep state and avoid overstating capabilities.

## [1.4.0] - 2026-04-03

### Added
- Granular I2C transport status codes: `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, and `I2C_BUS`.

### Changed
- Removed the unnecessary `Wire` library dependency from metadata.
- `RV3032/RV3032.h` now exposes `Version.h`, keeping version constants available from the canonical public include path.
- Updated the example transport adapter to treat `timeoutMs` as advisory and leave bus-timeout ownership with the application.
- Aligned README configuration and documentation references with the current RV3032-C7 API and shipped docs.

## [1.3.1] - 2026-04-03

### Added
- `inProgress()` convenience method on `Status` struct.
- `CommandHandler.h` example helper for serial command parsing (`cmd::readLine`, `cmd::match`, `cmd::parseInt`).
- `HealthDiag.h` example helper with verbose health diagnostics, color-coded output, snapshots, diffs, and `HealthMonitor` class for continuous monitoring.

### Changed
- `I2cScanner.h`: standardized to `LOG_SERIAL` macro, added common address hints for all supported devices.
- `Log.h`: added `LOGV` runtime-verbose macro, ESP32-S3 USB CDC delay in `log_begin()`.

## [1.3.0] - 2026-03-01

### Changed
- Updated portability documentation and public-header documentation to match current behavior and API surface.

### Removed
- Obsolete template documentation files superseded by current docs and standards.

## [1.2.2] - 2026-02-28

### Added
- Unified bringup CLI support files under `examples/common/*` for consistent cross-library diagnostics and command flow
- CLI/timing contract check tools (`tools/check_cli_contract.py`, `tools/check_core_timing_guard.py`)
- `docs/UNIFICATION_STANDARD.md` with shared unification conventions

### Changed
- `examples/01_basic_bringup_cli` help and command output style aligned to the common I2C CLI appearance
- Repository test/CI profile aligned with the wave-based unification quality gates

### Fixed
- Release metadata and changelog links synchronized to the `v1.2.2` release

## [1.2.1] - 2026-02-22

### Fixed
- **Critical**: Undefined behavior in `readTemperatureC()` from left-shifting negative signed value; replaced with unsigned arithmetic and explicit 12-bit sign extension
- `setTime()` called `computeWeekday()` before validating inputs, risking garbage computation when month=0
- `_totalFailures` / `_totalSuccess` counters silently wrapped to 0 instead of saturating at `UINT32_MAX`
- `clearPowerOnResetFlag()` / `clearVoltageLowFlag()` used magic numbers instead of named bit constants
- `binToBcd()` produced corrupt BCD for values > 99; now clamps defensively
- `unixToDate()` accepted timestamps before year 2000, producing dates rejected by `setTime()`

### Changed
- `setAlarmTime()`, `setAlarmMatch()`, `getAlarmConfig()` now use burst I2C reads/writes instead of 6 individual transactions each

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
- Frequency offset calibration in PPM (+/-200 ppm range)
- Built-in temperature sensor reading (+/-3 degC accuracy)
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

[Unreleased]: https://github.com/janhavelka/RV3032-C7/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.6.0...v2.0.0
[1.6.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.5.0...v1.6.0
[1.5.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.4.1...v1.5.0
[1.4.1]: https://github.com/janhavelka/RV3032-C7/compare/v1.4.0...v1.4.1
[1.4.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.3.1...v1.4.0
[1.3.1]: https://github.com/janhavelka/RV3032-C7/compare/v1.3.0...v1.3.1
[1.3.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.2.2...v1.3.0
[1.2.2]: https://github.com/janhavelka/RV3032-C7/compare/v1.2.1...v1.2.2
[1.2.1]: https://github.com/janhavelka/RV3032-C7/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/janhavelka/RV3032-C7/releases/tag/v1.0.0
