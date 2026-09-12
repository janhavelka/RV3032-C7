# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Latest published release: **v3.0.1**. All subsequent changes below are
unreleased; the next intended release is **3.1.0**. Development build metadata
is not publication evidence.

### Migration from 3.0.1

The new `SettingsSnapshot::primaryCellI2cTimeoutMs` and
`persistentAccessStateUnproven` trailing fields preserve source aggregate
initialization compatibility but change binary layout. Rebuild consumers.

### Changed

- README now collects supported platforms, intentional feature boundaries,
  audit landing evidence, release status, and API documentation navigation.
- Doxygen groups driver methods by feature and clarifies passive lifecycle,
  explicit persistent-access recovery, polling budgets, callback ownership,
  and the distinction between admission and terminal success.

- The coherent-temperature default is 200 ms and the backup-switch default is
  500 ms, admitting the supported 100 ms callback timeout without a clock hook.
- `begin()` always validates the 10..250 ms EEPROM window so explicit access
  recovery remains available with generic writes disabled.
- Unproven persistent-access cleanup survives `end()`/`begin()` on the same
  object, including abandonment of active cleanup and callback rebinding.
- The CLI emits one diagnostic after 15 seconds of pending work and continues
  polling. Serial input received while pending is discarded through complete
  lines, including partial command tails.

- `setTime()`, `setUnix()`, `writeRegister()`, `writeRegisters()`, and
  `writeUserRam()` return `BUSY` while generic persistence is queued, even
  before its first poll. Single-transfer reads remain available under the
  application's transport serialization.
- Generic EEPROM queue work now owns fixed persistence state independent of the
  ordinary job record, preserving completed job status and typed results.
- Default snapshot and verified calendar-set deadlines are now 200 ms and
  700 ms so they are executable across the accepted callback-timeout range.
- EEPROM write mutation cutoffs reserve the full non-replayable post-WRITE_ONE
  busy, two-read durability proof, access cleanup, and settle chain.
- `isJobBusy()` retains combined active-work behavior; the public
  `isOrdinaryJobBusy()` selects `pollJob()` while generic persistence uses its
  independent fixed state and polling surface.
- Repository checks are focused semantic portability, ABI/version, and package
  validators instead of source-order and exact-prose archaeology.
- The reference transport is explicitly ESP32-S2/S3 scoped, uses the selected
  core's published Wire buffer capacity with a conservative fallback, and
  discards partial staging before address-only cleanup.

### Fixed

- Keep zero-byte Wire reads as generic `I2C_ERROR` with the received-byte
  count. Arduino discards the backend cause, so zero bytes cannot prove an
  address NACK or `DEVICE_NOT_FOUND`. Explicit Wire NACK results still retain
  their existing classification; partial reads preserve the caller's buffer.
- Remove redundant console drains from I2C scan output; probe order and bus
  timing are unchanged, without explicit USB transmit-buffer clearing.
- The exhaustive HIL harness now preserves active and durable C1 independently,
  including PORIE/VLIE, when they initially differ. Equal/changed/restored
  EEPROM write counts are verified against direct persistent readback.
- The HIL runner requires completed serial framing, retains live raw output,
  captures health around stress/failures, and stops at the first failure.
  Expected fixture errors no longer mask unrelated errors or failure counts.
  Complete memory payloads and stress health counters are required even when
  the prompt arrives; short command writes cannot consume a queued response.
  Every driver health row is checked, so missing middle lines cannot pass as
  a healthy snapshot merely because a terminal prompt arrived.
- Keep the Arduino example's startup bus-clear pins open-drain, honor bounded
  SCL stretching, and reject initialization if SDA or SCL remains LOW. Failed
  recovery releases both pins instead of driving against a target.
- Generated API pages link correctly to the retained HIL summary and the
  ESP-IDF notes' verification instructions.

- Direct EEPROM read/write admission includes both READ_ONE waits before the
  cleanup cutoff and charges the complete first-byte callback sequence when
  there is no clock hook. At a 5 ms I2C / 100 ms EEPROM timeout the minimum
  read/write budgets are 298/518 ms with a clock hook, or 398/648 ms without
  one. Slower callbacks, longer requests, and scheduling gaps need extra time.
- READ_ONE ready polling preserves time for the following data read, including
  when a clockless transport consumes its full clipped callback timeout.
- Direct EEPROM read/write defaults are 4000/6000 ms. Generic item deadlines
  retain a 4000 ms floor and grow to a calculated maximum of 5323 ms for the
  slowest supported transport settings, including initial-ready and cleanup
  allowances. Accepted clockless configurations can complete their writes.
- Expiration of the cleanup ready phase retains the failure and continues
  direct C0/EERD restoration within the whole-operation deadline.
- Busy CLI input exceeding the 256-byte per-poll drain remains marked for
  discard after the job finishes; complete queued commands and partial tails
  cannot become fresh commands. Input arriving during the terminal transport
  callback is captured before completion is reported.
- Cooperative contract checks follow inline members and members defined in
  other core files. Qualified health updates are checked against the same
  exact owner allowlist as unqualified calls. Trailing-return/reference-qualified
  members and final classes are recognized, and ABI guards ignore commented
  enum examples and cover public operation-timeout default arguments.
- Changelog comparisons start at the latest published release; unpublished
  development work is collected here without separate release headings.

- The post-WRITE_ONE reserve now permits durable readback after the mutation
  cutoff, retaining the original callback error without replaying the write.
- Persistent-access recovery continues direct C0/EERD restoration after a
  busy-read failure or timeout and preserves that original operation error.
- Re-requesting the current backup mode succeeds without an active PMU write
  even when TCM is nonzero; changes that enable charging remain guarded.
- Invalid password spans fail closed. The ESP32 example adapter has an
  explicit platform guard and a 64-byte fallback capacity; the largest legal
  public register burst remains 46 bytes (`0x00..0x2D`).
- Portability checks cover core stdio/parsers, transitive member calls, and an
  exact health-owner allowlist. ABI checks cover all public enums and timeout/
  size constants. Verification commands and retained-audit documentation agree.
- Fresh audit verification preserved the exact legacy member-function types
  for backup-switch and trickle-charge setters while exposing explicit charge
  policy through distinctly named methods.
- Ordinary-job polling now has an explicit public predicate, so owner loops do
  not select `pollJob()` while generic EEPROM work owns its separate surface.
- Pre-existing EERD is a cleanup obligation as soon as it is observed; exact
  C0/Control 1 proof is no longer discarded by auxiliary cleanup failures or
  by a later recovery activation-settle timeout.
- The ESP32 Wire adapter now accepts the selected core's published buffer
  capacity, and package validation directly enforces every declared export
  exclusion.
- CLI, transport, package, and cooperative-edge regression coverage plus the
  contributor verification gate now match the maintained contracts.

- Blocking mutators can no longer interleave with cooperative jobs; verified
  calendar readback now validates weekday rollover; persistent ranges exclude
  password bytes; alarm reset-date fallback and generic EEPROM poll caps match
  the documented silicon/configuration behavior.
- Fixed-layout build-time parsing removes `sscanf`, all public error ordinals
  are explicit, version-code component limits are enforced, and the native fake
  refuses password-register indirect commands.
- Persistent access-state latching now covers pre-existing EERD and primary
  ensure cleanup failures without falsely requiring recovery after readback-
  proven cleanup or a later activation-settle timeout.
- CLI parsing, whitespace handling, diagnostics, command-report naming, scanner
  branch coverage, and packaging/documentation inconsistencies identified by
  the 2026 code audit are corrected.

### Added

- `isEepromPollable()` selects the generic polling surface when queued work
  is waiting behind an ordinary job. Together with `isOrdinaryJobBusy()`, it
  permits an owner loop to drain both surfaces without repeatedly polling a
  surface that returns `BUSY`.
- A per-job callback-bound table checked by native fault matrices, plus
  maximum-timeout coverage for the four staged no-wait setters. Their existing
  operation behavior is unchanged.
- Native impossible-state coverage and regression probes for the CI contract
  checkers, including transitive cooperative I/O and public enum/default guards.

- An explicit `BackupChargePolicy` makes rechargeable-backup intent mandatory
  before either a BSM or TCM setter may produce an enabled charging pair.
- A cached `persistentAccessStateUnproven` setting and bounded cooperative
  persistent-access recovery job that proves caller-selected C0 and cleared
  EERD state without issuing an EEPROM command.
- A separate 1..5 ms `primaryCellI2cTimeoutMs` configuration field, plus native
  coverage for audit regressions, no-clock default admission, cleanup recovery,
  fake-password tripwires, and Wire short-staging cleanup.

## [3.0.1] - 2026-08-05

### Added

- Native coverage for shared job-status reporting and the active-only
  backup-switch interrupt setter/getter, including output preservation on
  transport failure.
- A reusable `esp32s3hil_persistence` environment for two-cycle configuration
  EEPROM HIL: stage typed alternate settings, verify persistent and active
  bytes after reboot, restore the exact original bytes, and verify them after a
  second reboot.
- Physical main-power-loss evidence with primary-cell RTC retention,
  backup-switch flagging, application reboot, and post-return stress coverage.
- Configuration persistence evidence across two additional physical
  main-power cycles, including exact C0-C5 verification and restoration.

### Changed

- Public documentation now has one maintained index, an accurate direct and
  indirect memory map, non-overlapping cooperative examples, wrapper-based
  Windows verification commands, and a concise current HIL evidence summary.
- The documentation/package contract now validates maintained release content
  instead of preserving completed prompt phases and point-in-time audit text.
- Device-free HIL help, parser self-test, and dry-run no longer require
  `pyserial`; physical execution requires an explicit serial port.
- GitHub Actions now use current Node 24-based majors: Checkout and Setup
  Python v7, plus Cache v6. This removes the hosted runner's Node 20
  deprecation warnings.
- The PlatformIO pin now uses PIOArduino `55.03.311` with Arduino-ESP32
  `3.3.11` and ESP-IDF `5.5.5`. ESP32-S3 builds target the built-in N16R8
  definition (16 MB QIO flash and 8 MB octal PSRAM), and the bring-up CLI
  reports the runtime MCU, memory, Arduino core, and ESP-IDF versions. HIL
  firmware banners are port-neutral so evidence from a selected runtime port
  is not mislabeled. Windows installation guidance covers the longer bundled
  header paths in the new framework package.
- Public and supporting documentation now describes the timer-zero restore
  contract, per-lifecycle callback health counters, current HIL evidence, and
  the distinction between direct registers and indirect user EEPROM.
- Doxygen input is limited to maintained public documentation, treats emitted
  documentation errors as build failures, and no longer duplicates the
  manifest version. Release checks now validate version agreement dynamically
  instead of hard-coding `3.0.0`.
- The duplicated timed read/write transport deadline bookkeeping now uses one
  shared preparation and completion path.

### Removed

- Completed AI prompt suites, superseded dated audit/implementation reports,
  duplicated extracted device notes, transcript-like HIL excerpts, and old
  hardware campaigns from the maintained release tree. All remain recoverable
  from Git history and, where applicable, prior release tags.
- Obsolete v1.1 build output and the superseded managed-driver proposal from
  the repository root, plus an unused signed-integer parser from example glue.

### Fixed

- Doxygen now documents the namespaced driver class instead of creating a
  bogus global class page, and explicitly disables host-dependent Graphviz
  output so CI and local builds behave consistently.
- CI now runs the device-free HIL parser self-test and dry-run. The strict host
  build also no longer reports signed-to-unsigned conversion at date rollover.
- Library packaging now excludes local `dist/` and `tmp/` trees so generated
  archives cannot be nested inside published packages.
- Ignore and exclude the bare ESP-IDF component manifests that PIOArduino can
  generate at the project root while its framework package is being repaired;
  this Arduino-only library does not publish those transient files.
- `WaitMsFn` now explicitly requires the full requested monotonic wait. The
  ESP32 example and HIL adapters add one scheduler guard tick so vendor settle
  intervals cannot be shortened by Arduino-ESP32's tick-relative `delay()`.
- The configuration-persistence HIL harness now reapplies and proves the
  primary-cell active C0 startup state after every backup-powered return before
  comparing or restoring the remaining configuration bytes. The reconciliation
  must issue zero additional persistent writes.
- A disabled timer can now use the vendor-defined non-running zero preset, so
  `getTimer()` output from a valid inactive device can be restored exactly;
  enabling the timer with a zero preset remains rejected before I/O.
- Doxygen and example comments now accurately describe cached poll results,
  transport-callback health accounting, EEPROM state, and application-owned
  board pins.

## [3.0.0] - 2026-07-17

### Added

- Canonical factory-delivery constants for PMU/CLKOUT configuration bytes and
  native regression coverage for active-only refresh loss, C0-only CLKOUT
  enable persistence, and exact C0/C2/C3 full-configuration persistence.
- A reusable `esp32s3hil` environment and autonomous real-device harness for
  lifecycle, calendar, RAM, control, persistent-read, bounded EEPROM-write,
  health-state, and restoration coverage.

### Removed

- Password credentials, protection-management APIs, authentication state,
  address-list persistence machinery, and their fake/test workflows. The full
  silicon register constants remain for reference; both password ranges now
  fail closed before I/O.
- Unchecked public BCD conversions, dead lifecycle fields/states, and the
  saturating lifetime-counter implementation.
- The synchronous/generic backup setter, generic-register classification for
  staged periodic/CLKOUT/temperature jobs, and superseded partial-failure and
  separate timer-byte state paths.
- The misleading `isOnline()` Boolean, the CLI's local truncating line reader,
  permissive numeric parsers, unconditional EEPROM poll, and inferred parallel
  job owner.
- Unused example-only transport, bus, and health facades; the duplicate scanner
  recovery path; dead logging/style and native-stub helpers; the unused HIL
  compatibility timeout; the redundant native environment alias; and a
  project-specific dependency-header generator that did not belong to this
  library.
- Obsolete `TS_OVERWRITE_BIT`, `PMU_CLKOUT_DISABLE`, and `VERSION_INT`
  compatibility aliases; use the canonical vendor bit names and
  `VERSION_CODE`.

### Changed

- Transport callbacks now have a closed I2C-only status domain and explicit
  attempted-transfer evidence. Cooperative transfers clip timeouts, check the
  earliest exclusive deadline again at completion, and conservatively charge
  the supplied timeout when no clock hook exists. Multi-transfer admission now
  rejects budgets that cannot dispatch every required callback under that
  accounting, with zero I/O.
- `end()` now performs unconditional zero-I/O teardown, and live driver objects
  are noncopyable and nonmovable.
- Public weekday/build/Unix conversions now return `Status`, validate the full
  supported domain, and preserve output arguments on failure.
- Generic persistence derives its cleanup reserve from six callback bounds plus
  ready/settle time; health lifetime counters wrap to zero after `UINT32_MAX`.
- Backup-mode configuration is now a four-callback cooperative job with
  explicit PMU encoding/readback, BSIE admission, readback-only reconciliation,
  and 2 ms/10 ms activation not-before boundaries.
- Timer, periodic-update, CLKOUT, and temperature-event configuration now use
  distinct bounded state owners and return `ConfigurationJobReport` evidence
  for requested, safe-disabled, unchanged, or unknown terminal state.
- `tick()` now returns the exact one-instruction EEPROM polling result.
- Persistent typed results and generic queue diagnostics now retain forward
  operation failure, cleanup failure, durable proof, and partial progress as
  separate evidence.
- The Arduino-ESP32 3.2.0 example platform is pinned. Its Wire adapter applies
  one complete-callback deadline, restores the previous Wire timeout, and
  returns only the transport callback's closed I2C status domain.
- The example CLI now has one overflow-discarding reader, strict range-checked
  tokens, exact argument counts, and one fixed pending owner through ordinary
  jobs and optional EEPROM persistence.
- Probe documentation now describes address-`0x51` Status communication rather
  than presence or identity. Version metadata is generated as `3.0.0` for the
  breaking v3 integration surface.
- Cooperative terminal handling, configuration-result retrieval, register-bit
  access, persistent evidence, cleanup-reserve calculation, and CLI diagnostic
  formatting now reuse their existing single owners instead of parallel
  bookkeeping.

### Fixed

- ESP32 example I2C initialization now supplies the requested clock to the
  single `Wire.begin()` call. This avoids a live ESP32-S3 bus being rejected
  when a redundant follow-up `Wire.setClock()` reports failure.
- Intensive HIL runner expectations now distinguish synchronous four-byte RAM
  completion from the full 16-byte cooperative RAM job's terminal wording.
- Destructive HIL primary-cell coverage now requires semantic success and
  cleanup proof, verifies the same-lifecycle admission latch, and confirms
  level/off/level cooperative transitions with the trickle charger disabled,
  plus final primary-profile preservation after the mutation mix.
- EEF/CLKF/BSF clearing now uses one two-state W0C owner with fixed preservation
  payloads, preventing a neighboring flag asserted between polls from being
  erased.
- Invalid callback statuses can no longer leave a cooperative engine stuck,
  post-callback deadline overruns cannot report success, `recover()` caches the
  same address-NACK mapping it returns, and impossible states return
  `INTERNAL_STATE_ERROR` with bounded persistent cleanup where required.
- Persistent whole-deadline handling now preserves a higher-precedence
  transport-contract or callback-timeout failure instead of replacing it with
  a generic operation timeout.
- Live timer/alarm/EVI/backup reconfiguration now checks TIE/AIE/EIE/BSIE before
  mutation. Periodic UIE now consistently means update-event enable: UIE=0
  creates neither a new UF nor the INT event.
- Ambiguous staged writes are never replayed; post-mutation failures enter one
  operation-specific bounded safe-state or backup reconciliation owner, and
  configuration persistence is admitted only after requested-state proof.
- A later EEPROM access-state cleanup failure no longer erases established
  durable content proof or overwrites the first forward operation failure.
- Entering persistent cleanup now makes access-state proof mandatory, so a
  late or non-dispatched restore reports semantic `EEPROM_CLEANUP_FAILED`
  instead of degrading to an ordinary operation timeout.
- Short Wire staging now emits one bounded final STOP instead of leaking the
  transaction owner; Wire initialization reports `begin()`/`setClock()`
  failure instead of continuing.
- Cooperative RAM writes and all other asynchronous CLI operations now print
  terminal evidence only after completion, and unset timestamp blocks no
  longer print a fictitious zero date.
- CLKOUT verification now checks all four implemented active-configuration
  bytes without an out-of-range mismatch lookup and reports the exact first
  mismatching expected/observed byte.
- The example scanner now restores the application Wire timeout after its
  bounded scan, and CLI health-rate formatting widens the independent wrapping
  counters before summing them.
- CLI/HIL timer input now enforces the public 1..4095-tick range, and HIL RAM
  checks require terminal success rather than accepting admission text.
- The CLI retains EEPROM ownership after an item-level failure while later
  fixed-queue entries remain, preventing untracked persistence work.
- Maintained weekday, Status-write side-effect, callback-timeout, presence,
  health-counter, README polling, and cross-phase documentation now match the
  implemented v3 contracts.
- CLKOUT documentation now distinguishes the factory-delivery 32.768 kHz
  output from later EEPROM-restored power-on state, names the exact persistent
  bytes for each setter, and records the POR/24-hour refresh and VBACKUP-low
  behavior from Application Manual Rev. 1.3.

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

- GitHub Actions CI now runs for version-tag pushes as well as `main` pushes,
  so each release tag receives checks against its exact commit.
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

Includes earlier development work that was not separately published.

### Added

- Granular I2C transport status codes: `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, and `I2C_BUS`.

- `inProgress()` convenience method on `Status` struct.
- `CommandHandler.h` example helper for serial command parsing (`cmd::readLine`, `cmd::match`, `cmd::parseInt`).
- `HealthDiag.h` example helper with verbose health diagnostics, color-coded output, snapshots, diffs, and `HealthMonitor` class for continuous monitoring.

### Changed

- Removed the unnecessary `Wire` library dependency from metadata.
- `RV3032/RV3032.h` now exposes `Version.h`, keeping version constants available from the canonical public include path.
- Updated the example transport adapter to treat `timeoutMs` as advisory and leave bus-timeout ownership with the application.
- Aligned README configuration and documentation references with the current RV3032-C7 API and shipped docs.

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

Includes the initial implementation and earlier development work that were
not separately published.

### Added

- EEPROM write statistics: `eepromWriteCount()`, `eepromWriteFailures()`, `eepromQueueDepth()`
- Driver health tracking with `DriverState` (READY, DEGRADED, OFFLINE)
- Health diagnostics: `state()`, `consecutiveFailures()`, `totalFailures()`, `totalSuccess()`
- Recovery API: `recover()` for manual device recovery after failures
- Probe function for diagnostic checks without affecting driver health
- `_beginInProgress` flag for proper initialization state management

- Non-blocking EEPROM commit state machine (tick-driven) with `isEepromBusy()` and `getEepromLastStatus()`
- Status/validity helpers: `readStatusFlags()`, `readValidity()`, `clearBackupSwitchFlag()`
- Conversion helpers: `bcdToBinary()`, `binaryToBcd()`, `unixToDateTime()`, `dateTimeToUnix()`
- Host-side unit tests with a native test environment and CI job

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

- Improved health tracking architecture with tracked vs raw transport wrappers
- Enhanced `begin()` and `recover()` to properly track I2C failures
- Reduced EEPROM write timeout for improved performance
- Disabled verbose mode by default in examples

- Added `eepromNonBlocking` config option (default true)
- EEPROM writes can return `BUSY` when a commit is already in progress
- CLI example includes validity and backup switchover helpers

### Fixed

- Initialization state management during `begin()` execution
- Health tracking now correctly excludes validation errors

[Unreleased]: https://github.com/janhavelka/RV3032-C7/compare/v3.0.1...HEAD
[3.0.1]: https://github.com/janhavelka/RV3032-C7/compare/v3.0.0...v3.0.1
[3.0.0]: https://github.com/janhavelka/RV3032-C7/compare/v2.0.0...v3.0.0
[2.0.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.6.0...v2.0.0
[1.6.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.5.0...v1.6.0
[1.5.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.4.1...v1.5.0
[1.4.1]: https://github.com/janhavelka/RV3032-C7/compare/v1.4.0...v1.4.1
[1.4.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.3.0...v1.4.0
[1.3.0]: https://github.com/janhavelka/RV3032-C7/compare/v1.2.2...v1.3.0
[1.2.2]: https://github.com/janhavelka/RV3032-C7/compare/v1.2.1...v1.2.2
[1.2.1]: https://github.com/janhavelka/RV3032-C7/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/janhavelka/RV3032-C7/releases/tag/v1.2.0
