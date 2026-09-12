# RV3032-C7 library audit

Audit of the driver against the Micro Crystal **RV-3032-C7 Application Manual
Rev. 1.3 (May 2023)** and the datasheet, plus a software-architecture review of
the cooperative engine, the example glue, the tests, and the repository tooling.

Baseline at audit start: `v3.0.1` (`4c9ae36`), clean worktree, 111/111 native
tests passing, all four check scripts passing.

Everything in "Fixed directly" is already applied and verified. Everything in
"Open findings" is a design decision that should not be made unilaterally, and
each carries a concrete proposal.

---

## Contents

- [Fixed directly](#fixed-directly)
- [Open findings — code](#open-findings--code)
  - [C1. The EEPROM queue erases a completed job's terminal status](#c1)
  - [C2. Enabling backup switchover can silently start trickle charging](#c2)
  - [C3. Default operation timeouts are unusable with the default Config](#c3)
  - [C4. A leaked `EERD=1` / `BSM=00` access state has no recovery path](#c4)
  - [C5. Blocking calendar helpers have no cooperative-job guard](#c5)
  - [C6. `ensurePrimaryCellConfiguration()` silently clamps `i2cTimeoutMs`](#c6)
  - [C7. Smaller correctness items](#c7)
- [Open findings — examples, tests, tooling](#open-findings--examples-tests-tooling)
- [Open findings — repository tooling](#open-findings--repository-tooling)
- [Verified correct](#verified-correct)

---

## Fixed directly

Applied, with `pio test -e native` (111/111), all four check scripts, and
`pio run -e esp32s3dev` green after each change.

| # | Change | Why |
|---|---|---|
| F1 | **`src/RV3032.cpp`** — a transport error while polling EEbusy in `CLEANUP_WAIT_READY` no longer abandons the operation. It now records the cleanup failure and still runs the active-mirror and Control 1 restore. | A single transient NACK on one `0x0E` read left the chip with `EERD=1` **and** `BSM=00`/`TCM=00` installed — automatic refresh *and* backup switchover both disabled — with no in-library repair. The restore states already tolerate further failures, so terminal `EEPROM_CLEANUP_FAILED` precedence is unchanged. The *deadline* branch deliberately still stops, because "no new callback past a hard deadline" is a documented contract (and a native test asserts it). |
| F2 | **`src/RV3032.cpp`, `include/RV3032/CommandTable.h`** — every read-modify-write of Time Stamp Control `0x13` that is not an intentional EVI reset now writes `EVR=0`, via a new `TS_CONTROL_RMW_MASK = 0x1F`. Affects `setEviOverwrite()`, `resetTimestamp(TLow\|THigh)`, and the staged temperature job. | `EVR` is the only bit of `0x13` that reads back as 1, and the manual contradicts itself on whether re-writing a set `EVR` resets the bank again — §3.10 says *"EVR may remain set. No further reset occurs"*, §4.17 note 9 says *"Bit EVR can be left at 1. Writing or overwriting a 1 causes reset."* Under the second reading, an unrelated setter silently wiped all eight TS EVI registers. The driver's own EVI-reset path already assumes the pessimistic reading, so it was internally inconsistent. Writing `EVR=0` is safe under **both** readings because `EVR` is a command bit, not stored state. |
| F3 | **`src/RV3032.cpp`** — introduced `BACKUP_ACTIVATION_DIRECT_MS` / `BACKUP_ACTIVATION_LEVEL_MS` (tSWA, §4.2.2/4.2.3) and used them at all four sites instead of `EEPROM_WRITE_SETTLE_MS` and bare `2U`/`10U` literals. | The LSM activation settle was borrowing the EEPROM write-settle constant. They coincide at 10 ms today, so no behaviour changed — but tuning the EEPROM constant would silently have shortened an electrical safety settle. |
| F4 | **`include/RV3032/CommandTable.h`** — documented `PMU_PRIMARY_PRESERVE_MASK = 0x4C` with its derivation and a "do not widen" warning. | This one constant is the sole mechanism forcing `TCM=00` across eight write sites. Widening it to "also preserve TCM" would enable charging into a primary cell, and no existing test would necessarily fail. |
| F5 | **`include/RV3032/RV3032.h`** — corrected four API doc blocks: the BSM×TCM charging coupling warning on `startSetBackupSwitchModeJob()`; `getBackupSwitchMode()` reads the *active mirror*, not durable state; `setBackupSwitchInterruptEnabled()` asserts INT immediately if BSF is stale (§4.14.1); `ValidityFlags::backupSwitched` reads 0 while switchover is disabled and does not survive POR. | See [C2](#c2). These are the facts a caller needs and could not get from the header. |
| F6 | **`include/RV3032/RV3032.h`** — `setStopEnabled()` no longer claims STOP "disables CLKOUT". It stops only the XTAL-derived 1024/64/1 Hz selections. | §4.21.1: *"The STOP bit function will not affect the CLKOUT of 32.768 kHz"*; the §3.20.3 HFD table says STOP has *"No effect"* on HF mode. |
| F7 | **`include/RV3032/RV3032.h`** — `setTemperatureReference()` no longer claims TREF feeds temperature compensation. | §4.20 states the opposite outright: *"The change in TREF has no effect on the temperature compensation of the RTC."* It shifts only the readable thermometer value. |
| F8 | **`include/RV3032/CommandTable.h`** — the user-EEPROM Doxygen block was attached to `CONFIG_EEPROM_START = 0xC0`, so Doxygen published "User EEPROM start address (0xCB)" as the documentation for the constant whose value is `0xC0`. Split into three correct blocks. Also: `EEBUSY` is read-only, not write-clear; the EVI unimplemented span is bits 3:1, not bit 3 alone. | Values were always right; only the comments were wrong. Found independently by two reviewers. |
| F9 | **Superseded 2026-09-12**: the original zero-byte-to-NACK mapping was too specific. | Arduino `requestFrom()` discards the backend error and exposes only received bytes. Zero and partial reads now retain `I2C_ERROR` with the actual byte count; explicit transport address NACKs still map to `DEVICE_NOT_FOUND`. See `CODE_AUDIT_RESOLUTION.md`. |
| F10 | **`examples/01_basic_bringup_cli/main.cpp`** — `cmd_clkout` now registers the operation name `"CLKOUT configuration"` instead of `"CLKOUT enable"`/`"CLKOUT disable"`, and the line in `check_cli_contract.py` that required the old strings was deleted. | `printConfigurationReport()` dispatches on the operation *name string*; the enable/disable names matched nothing, hit the `else { return; }`, and silently dropped all `final=`/`mutation_attempted=`/`cleanup=` evidence for that command — unlike every other configuration command. The contract script **required the buggy literal**, so fixing the defect previously failed CI. |
| F11 | **`examples/common/CommandHandler.h`** — the "strict" numeric parsers now reject a leading `+` as well as `-`. | `strtoull` consumes `+`, so `timer +1 +2 +0` and `reg +5` were accepted by parsers whose header claims strictness. |

### Documentation and repository hygiene (also applied)

| # | Change | Why |
|---|---|---|
| D1 | **`.gitignore`** — removed `include/RV3032/Version.h`. | The file is **git-tracked** *and* was gitignored. Tracked is correct: CI's `generate_version.py check` fails on a fresh clone without it, `check_docs_contract.py` requires it in both `REQUIRED_SOURCE_FILES` and `REQUIRED_PACKAGE_FILES`, the CLI example `#include`s it, and non-PlatformIO consumers have no generator. Left as-is, a routine `git clean -Xdf` would delete a tracked, CI-required file. |
| D2 | Deleted **`docs/doxygen/`** (4.2 MB of stale generated HTML), **`tools/__pycache__/`**, and the empty **`docs/prompts/`**, **`docs/extracted-md/`**, **`tmp/`**. | All gitignored. The Doxygen output was stale enough to still contain pages for inputs the current `Doxyfile` no longer lists. Regenerate with `doxygen Doxyfile`. |
| D3 | **`SECURITY.md`** — supported version `1.4.x` → `3.0.x` (five releases stale); contact aligned to the `library.json` maintainer address; deleted the "Scope"/"Best Practices" filler, which claimed *"No persistent storage by default (NVS side effects are opt-in)"* — there is no NVS anywhere in this repo. | Factually wrong and misleading about what the library touches. |
| D4 | **`CODEOWNERS`** — default owner `info@thymos.cz` → `@janhavelka`. | GitHub CODEOWNERS resolves only `@user`/`@org/team`; a bare email silently matches nobody, so the default rule was inert while `/include/` and `/.github/` worked. |
| D5 | **`docs/ARCHITECTURE.md`** — `0xC0..0xC5 are active configuration mirrors` → `0xC0..0xCA`, noting only `C0..C5` are library-supported. | Contradicted `docs/DEVICE_REFERENCE.md`, `README.md`, and `CommandTable.h` (`CONFIG_EEPROM_END = 0xCA`). |
| D6 | **`docs/ARCHITECTURE.md`**, **`AGENTS.md`** — "lifetime counters" → per-`begin()`/`end()`-lifecycle counters. | `_resetRuntimeState()` zeroes them on every `begin()`. `README.md` and the header already said this correctly; these two said the opposite. |
| D7 | **`README.md`** — deleted the leftover narration *"Completed prompts and point-in-time audit reports remain available in Git history… TunnelMonitor integration and an immutable consumer commit pin remain external work."*; unpinned the `v3.0.1` HIL-summary URL to a relative link; expanded the repository listing (it omitted `tools/`, `scripts/`, `test/stubs/` and both HIL harnesses); `cfg.i2cTimeoutMs = 5` → `50`. | Self-referential process narration plus a product-specific TODO in a general library. The pinned tag goes stale at every release. The `5` contradicted the library default, `BoardConfig.h`, and the validated HIL fixture. |
| D8 | **`docs/README.md`** — unpinned the `v3.0.1` vendor-PDF URL. | Same tag rot. |
| D9 | **`docs/IDF_PORT.md`** — replaced the "Local verification" block with a link to the README list. | It used `python -m platformio`, which `check_docs_contract.py` explicitly *bans* in `README.md` while permitting here, contradicted the `.\scripts\pio.cmd` rule in `AGENTS.md`, and omitted two of the checks CI runs. |
| D10 | **`docs/reports/HIL_SUMMARY.md`** — dropped the release-process apology about commit `ae08889`; added a plain note that the evidence predates v3.0.1. | Session notes, not hardware evidence. |
| D11 | **`CONTRIBUTING.md`** — step 4 now lists the full gate CI runs. | A contributor following it to the letter previously failed four CI jobs. |
| D12 | **`AGENTS.md`** — repo tree listed a nonexistent `examples/00_*/`, omitted `CliShell.h`/`CliStyle.h`, and hid `tools/`, `test/`, `docs/`, `scripts/`; de-TunnelMonitored the primary-cell rule. | A "Repository Model" section that hides the four scripts CI depends on is worse than none. |
| D13 | **`library.json`** — description no longer claims "temperature compensation". | The library exposes the sensor, thresholds and references; it does not control the RTC's internal compensation (§4.20). |
| D14 | **`.github/workflows/ci.yml`** — added `esp32s3hil` and `esp32s3hil_persistence` to the build matrix. | 1,661 lines of HIL harness were compiled by *nothing*. A driver rename would rot them silently until someone plugged in hardware — while `check_cli_contract.py` greps those same files for exact strings, giving a false sense they were validated. Both compile clean today; verified. |
| D15 | Stripped the UTF-8 BOM from `scripts/generate_version.py`, `AGENTS.md`, `CHANGELOG.md`. | The kernel does not recognise `ï»¿#!` as a shebang, so `./scripts/generate_version.py` fails with "cannot execute" on Linux/macOS. Latent only because CI invokes it as `python scripts/...`. |

**Not touched, flagged for you:** `.pio/` holds ~25 MB of hand-made scratch —
`repro.exe`, `rv3032_phase1_check.o`, `audit-package/`,
`doxygen-release-audit/`, `doxygen-release-final-2/`, `-final-3/`, and
`release-package/RELEASE_NOTES_v3.0.1.md`. That last one is a real release
artifact parked in a gitignored build cache. It is your local build area, so I
left it alone.

---

## Open findings — code

<a id="c1"></a>
### C1. The EEPROM queue erases a completed job's terminal status — a hardware failure is reported as `OK`

**Severity: HIGH. Confidence: certain — verified in code.**

`processEeprom()` does not own state; it *borrows* `_job`:

```cpp
// src/RV3032.cpp:5313
if (_job.state == JobState::IDLE) {
  _job = JobOp{};                    // wipes completedKind AND lastStatus
  _job.state = JobState::PERSISTENT;
  _job.activeKind = JobKind::NONE;   // "queue owns the shared engine"
```

`JobOp{}` resets `completedKind = NONE` and `lastStatus = Status::Ok()` — the
two fields every result accessor keys on — plus every result payload. The same
wipe happens at item completion and on both cancel paths.

**Reachable on the ordinary happy path whenever `enableEepromWrites` is true:**

1. A persistence-producing setter succeeds and queues a byte. Queue depth ≥ 1.
2. A second persistence-producing setter is admitted (its guard deliberately
   permits a non-empty queue when `persist` is true) and **fails** — say
   `startSetBackupSwitchModeJob()` ends `CONFIGURATION_CLEANUP_FAILED`.
   `_job.lastStatus` and `_job.configurationReport` now hold that evidence.
3. The application's ordinary `tick(nowMs)` runs. `pollEeprom`'s guard is
   `isJobBusy() && _job.activeKind != JobKind::NONE`; the job is *finished*, so
   it passes. The queue pops an item and hits the line above.
4. `getSetBackupSwitchModeJobResult()` now returns `JOB_RESULT_UNAVAILABLE` with
   `report` untouched, and **`getJobStatus()` returns `Status::Ok()`.**

This violates the header contract *"Results remain available until another job
is admitted"* (an EEPROM tick is not a job admission) and AGENTS.md's *"Do not
hide hardware failures behind silent retries or fake success."*

**Proposal — split the ownership rather than patch the wipe.** The root cause is
one `JobOp` with two owners. Extract the state the two paths genuinely share:

```cpp
struct PersistentOp {
  EepromState state;
  uint8_t address, length, index, sentinel;
  uint32_t deadlineMs, mutationCutoffMs, phaseDeadlineMs, notBeforeMs;
  bool writeMode, cleanupRequired, writeAttempted;
  uint8_t buf[16];
};
```

Give `JobOp` one (`_job.persistent`) and `EepromOp` its own
(`_eeprom.persistent`), and change `processPersistentJob` to take a
`PersistentOp&` instead of reaching into `_job`. Then `processEeprom` never
touches `_job` at all.

This also deletes three things that exist only to paper over the sharing: the
`_job.activeKind == JobKind::NONE` sentinel checks scattered through the
persistent engine, the asymmetric three-predicate mutual-exclusion scheme
(`pollEeprom`, `pollJob`, and the setters each use a *different* test), and the
observable inconsistency where `getJobStatus()` reports `IN_PROGRESS` while
`pollJob()` answers `BUSY "EEPROM queue owns the shared engine"` — a pair an
application can deadlock on.

*Stop-gap if the refactor is out of scope for now:* replace each
`_job = JobOp{}` inside `processEeprom` with a helper that resets only the
persistent fields, leaving `completedKind`, `lastStatus` and the result payloads
intact. This is strictly worse — it leaves the two owners entangled — but it
closes the false-success hole.

---

<a id="c2"></a>
### C2. Enabling backup switchover can silently start trickle charging into the backup cell

**Severity: HIGH. Confidence: certain — manual verified directly.**

App Manual §4.3, verbatim:

> The trickle charger is disabled when TCM = 00 **or when Switchover function is
> disabled (BSM = 00 or 11)** or when the device is in VBACKUP Power state.

Charging is gated by the **conjunction** of `TCM != 00` **and** `BSM ∈ {01,10}`.
But `startSetBackupSwitchModeJob` preserves every non-BSM bit:

```cpp
// src/RV3032.cpp:849
_job.backupTargetPmu = static_cast<uint8_t>(
    (_job.backupOriginalPmu & ~cmd::PMU_BSM_MASK) | requestedBsm);
```

So on a part whose C0 holds e.g. `0x01` (BSM=00, TCM=01) — completely inert,
charger off — `startSetBackupSwitchModeJob(Direct)` writes `0x11` and **starts
charging current into whatever is on VBACKUP.** `0x01` is not exotic: it is what
any earlier `setTrickleChargeMode()` leaves behind, and it survives POR once
persisted. §4.2.2 separately warns *"Do not use [DSM] if the backup source on
VBACKUP is a primary battery."*

I have **already documented the coupling** in the header (F5 above). What
remains is a behavioural decision.

**Why I did not just make it refuse:** §8.4 CeraCharge™ is a legitimate,
vendor-documented configuration — BSM=10 with TCM=01/10/11 is exactly how you
charge a rechargeable backup cell. A blanket refusal would break that.

**Proposal — make the caller state the intent, rather than inferring it.** Add
an explicit parameter so the charging outcome is never implicit:

```cpp
enum class BackupChargePolicy : uint8_t {
  RequireChargerOff,   // default: fail if TCM != 00 would go live
  PreserveExistingTcm, // caller has a rechargeable cell and means it
};

Status startSetBackupSwitchModeJob(
    BackupSwitchMode mode, uint32_t nowMs,
    uint32_t operationTimeoutMs = BACKUP_SWITCH_OPERATION_TIMEOUT_MS,
    BackupChargePolicy charge = BackupChargePolicy::RequireChargerOff);
```

In `BACKUP_READ_PMU`, after the C0 read, when the request enables switchover and
`(backupOriginalPmu & PMU_TCM_MASK) != 0`:

- `RequireChargerOff` → fail with `INVALID_PARAM` and `detail = observed C0`,
  message naming the required action ("clear TCM first");
- `PreserveExistingTcm` → proceed unchanged.

The default is the safe one, and a non-default argument is a visible, greppable
statement that the board has a rechargeable cell. This is a source-compatible
addition (defaulted trailing parameter), so it is a **MINOR** bump, not MAJOR.

*Cheaper alternative if you would rather not grow the signature:* surface the
observed C0 (and hence TCM) in `ConfigurationJobReport` so the application can at
least detect it after the fact. That is strictly weaker — the charging has
already started by then.

---

<a id="c3"></a>
### C3. Both default operation timeouts are unusable with the default `Config`

**Severity: MEDIUM. Confidence: certain — arithmetic verified.**

With `Config` left at its documented defaults (`nowMs = nullptr`,
`i2cTimeoutMs = 50`), `startSetTimeAndClearInvalidFlagsVerifiedJob()` computes
(`src/RV3032.cpp:1899`):

```
fullTransferBound          = 7 * 50            = 350
statusWriteAdmissionBound  = 125 + 4 * 50 + 2  = 327
minimumTimeoutMs           = max(350, 327)     = 350
```

and the default argument is `SET_TIME_OPERATION_TIMEOUT_MS = 250`. `250 < 350`
→ **`INVALID_PARAM` on every call**, before any I/O.

| `i2cTimeoutMs` | verified-set @ default 250 | snapshot @ default 100 |
|---|---|---|
| ≤ 30 | OK | OK |
| **50 (library default)** | **REJECT** (min 350) | OK — but exactly on the boundary |
| 60 | REJECT (min 420) | REJECT (min 120) |
| 100 (max accepted) | REJECT (min 700) | REJECT (min 200) |

`READ_TIME_OPERATION_TIMEOUT_MS = 100` survives only because `2 × 50 = 100`
lands exactly on the boundary; any `i2cTimeoutMs > 50` breaks it too.

This is invisible to the test suite because `FakeRv3032` always supplies
`cfg.nowMs` and uses `i2cTimeoutMs = 5`, so no native test exercises the
no-clock-hook admission bound at all.

**Proposal.** Raise the two defaults so they are executable across the whole
accepted `i2cTimeoutMs` range (1..100), both still inside the 1000 ms ceiling:

```cpp
// include/RV3032/RV3032.h
static constexpr uint32_t READ_TIME_OPERATION_TIMEOUT_MS = 200;  // was 100
static constexpr uint32_t SET_TIME_OPERATION_TIMEOUT_MS  = 700;  // was 250
```

Widening a default can only turn rejections into acceptances — the admission
minimum itself is unchanged — so nothing previously accepted breaks. Add a
native test that admits both jobs with `nowMs = nullptr` and `i2cTimeoutMs = 50`,
which is the case the fake currently cannot express.

While there: `README.md` never mentions `MIN_SET_TIME_OPERATION_BUDGET_MS`, so a
caller passing 100 ms gets an unexplained rejection. Worth one sentence.

---

<a id="c4"></a>
### C4. A leaked `EERD=1` / `BSM=00` access state is sticky and has no recovery path

**Severity: HIGH. Confidence: certain.**

F1 closed the transport-error route into this state. Two routes remain, and the
deeper problem is that **nothing can get out of it.**

`grep -n CONTROL1_EERD_MASK src/RV3032.cpp` finds exactly one site that ever
*clears* EERD: `cleanupPrimaryCellEnsure`, and only on its `persistenceTrusted`
branch. The generic engine has no path that writes `EERD=0`. `RESTORE_CONTROL1`
writes back the value observed at `READ_CONTROL1`, and `ENABLE_EERD` skips the
write when EERD is already set — so once EERD is stuck at 1, every subsequent
persistent operation reads `0x04`, preserves it, and restores `0x04`, forever.

Consequences per §4.6.2 and §3.20.1: the 24-hour configuration-integrity refresh
stays disabled, and the C0 mirror's `BSM=00` is never re-populated from EEPROM.
The only remaining repair is the POR refresh (§4.6.1) — which needs a power
cycle, which is precisely the event the backup cell exists to survive.

**Remaining routes in:**

- **Hard deadline during cleanup.** Contractual ("no new callback past a hard
  deadline") and asserted by a native test, so I left it. It still leaves the
  chip in the degraded state.
- **Deadline expiry mid-proof after `WRITE_ONE`.** The mutation cutoff is
  explicitly disabled once `persistentWriteAttempted` is set, so the ~14-callback
  post-write verification chain runs with no cutoff — while the cleanup reserve
  (`250 + 6*i2cTimeoutMs + 10`) budgets nothing for it. With
  `i2cTimeoutMs = 100` and a 1000 ms operation timeout (just above the derived
  minimum), the deadline lands mid-proof and the job is abandoned.

**Proposal, in three parts:**

1. **Make the state observable.** Add `bool persistentAccessStateUnproven` to
   the driver, set it wherever cleanup is abandoned, and surface it in
   `SettingsSnapshot`. Today the application cannot distinguish "EEPROM idle" from
   "EEPROM idle, chip left with refresh and switchover disabled" — `getEepromStatus()`
   reports only a stale `EEPROM_CLEANUP_FAILED`.
2. **Make it recoverable.** Add a small explicit job:
   ```cpp
   Status startPersistentAccessStateRecoveryJob(uint8_t desiredC0, uint32_t nowMs,
                                                uint32_t operationTimeoutMs);
   ```
   It unconditionally drives `EERD=0` and the caller's C0, verifies both, and
   clears the flag. Explicit, bounded, application-invoked — consistent with the
   library's "application owns recovery" stance, which currently promises a
   recovery the application has no way to perform.
3. **Size the reserve for what actually follows the write.** Fold the post-write
   proof into the reserve so the deadline cannot land mid-chain:
   ```cpp
   constexpr uint32_t persistentPostWriteProofMs(uint32_t i2cTimeoutMs) {
     return EEPROM_WRITE_SETTLE_MS + 2U * EEPROM_READ_SETTLE_MS + 14U * i2cTimeoutMs;
   }
   ```
   `begin()` and the typed job minimums already derive from
   `persistentCleanupReserveMs()`, so they pick this up automatically.

---

<a id="c5"></a>
### C5. Blocking calendar helpers have no cooperative-job guard

**Severity: MEDIUM. Confidence: certain — reproduced.**

`clearStatus()`, `updateRegisterSingle()`, `startTempLsbFlagClear()` and every
`start*Job()` check `workIdle()`. `setTime()`, `setUnix()`, `readTime()`,
`readHundredths()`, `writeRegister(s)` and `writeUserRam()` do not. Reproduced
against the real driver: with a `SET_TIME_VERIFIED` job in flight,

```
start=13 busy=1
setTime during job: code=0 msg=OK  regs01=00 regs07=20   <-- wrote 2020-01-01 mid-job
clearStatus during job: code=12 msg=Driver work already in progress   <-- correctly refused
```

Landing between `SET_TIME_WRITE_CALENDAR` and `SET_TIME_VERIFY_CALENDAR`, the
interleaved write makes the job's own readback fail — reporting
`EEPROM_VERIFY_FAILED "Calendar readback mismatch"`, i.e. **attributing an
application sequencing error to the hardware.**

AGENTS.md does assign serialization to the application, so this is arguably
intentional — but the guard is applied inconsistently across direct peers, and
the failure mode is a misleading hardware error.

**Proposal.** Add `if (!workIdle()) return BUSY;` to the *mutating* synchronous
helpers (`setTime`, `setUnix`, `writeRegister`, `writeRegisters`, `writeUserRam`)
for symmetry with `clearStatus()`. Leave reads unguarded — a single coherent
burst is harmless thanks to the chip's register blocking — but say so explicitly
in the `readTime()` / `readHundredths()` doc comments, so the asymmetry is a
documented decision rather than an oversight.

---

<a id="c6"></a>
### C6. `ensurePrimaryCellConfiguration()` silently clamps `i2cTimeoutMs` to 5 ms

**Severity: MEDIUM. Confidence: certain.**

`begin()` accepts `i2cTimeoutMs` in 1..100 ms. `ensureRead`/`ensureWrite` then do:

```cpp
uint32_t timeout = _config.i2cTimeoutMs;
if (timeout > PRIMARY_CELL_TRANSFER_TIMEOUT_MS) timeout = PRIMARY_CELL_TRANSFER_TIMEOUT_MS;  // 5
```

and afterwards judge `returnedLate = after - callbackStart > timeout` against the
*clamped* value. On a board that legitimately needs 30 ms per transfer (shared
bus, RTOS mutex, slow bridge), an honest 6 ms transfer is classified as a
contract violation → `TIMEOUT` → the ensure fails with the safe BSM00/TCM00 hold.
**Primary-cell provisioning simply cannot complete on such a board**, and nothing
in the header, `README.md` or `docs/DEVICE_REFERENCE.md` mentions the cap.

The clamp exists for a real reason: `PRIMARY_CELL_OPERATION_TIMEOUT_MS` is
1000 ms and the sequence is ~20 callbacks, so 100 ms each would not fit.

**Proposal.** Reject at admission instead of narrowing silently — this matches
the library's own stated rule that *"admission rejects a whole-operation budget
that cannot execute the fixed callback sequence"*:

```cpp
if (_config.i2cTimeoutMs > PRIMARY_CELL_TRANSFER_TIMEOUT_MS) {
  return Status::Error(Err::INVALID_CONFIG,
                       "Primary ensure requires i2cTimeoutMs <= 5 ms",
                       static_cast<int32_t>(PRIMARY_CELL_TRANSFER_TIMEOUT_MS));
}
```

Placed after the `nowMs`/`waitMs` check, this is zero-I/O and does not consume
the lifecycle attempt. Document the 5 ms per-callback bound in the header.

---

<a id="c7"></a>
### C7. Smaller correctness items

| # | Finding | Proposal |
|---|---|---|
| C7.1 | **Cleanup evidence destroyed by a truncated settle.** `VERIFY_CONTROL1` proves the restore and sets `cleanupVerified = true`, then waits 10 ms in `SETTLE`. If the deadline lands inside that window, `pollJob` sets `cleanupVerified = false` and reports `EEPROM_CLEANUP_FAILED` — for an operation whose access state *was* fully restored and verified. | Clear `persistentCleanupRequired` at the end of `VERIFY_CONTROL1` (the state is restored at that point) and track the outstanding settle with a separate `settlePending` flag whose expiry maps to a plain `TIMEOUT`. Never overwrite `cleanupVerified` once true. |
| C7.2 | **Unsound overflow check** in `readRegisters()`/`writeRegisters()`: `static_cast<uint16_t>(reg) + len - 1U > 0xFFu` promotes to `size_t`, so `reg=2, len=SIZE_MAX` evaluates to `0` and passes. Not exploitable today (`validateReadRegsRequest` rejects `len > 255` downstream). | Check length first in a narrow type: `if (len == 0 \|\| len > 255 \|\| unsigned(reg) + unsigned(len) > 256u) return INVALID_PARAM;`. The same shape appears in `isKnownRegisterBlock` and `intersectsUnsupportedPasswordRange`; hoist one shared `validRegisterSpan()` helper. |
| C7.3 | **`writeUserRam()` changes execution model at exactly `len == 16`.** `kUserRamSize` is 16 but `REGISTER_WRITE_PAYLOAD_CAPACITY` is 15, so `offset=0, len=16` silently returns `IN_PROGRESS` from a nominally synchronous API — and the write does not happen unless the caller drives `pollJob()`. | Either reject `len > 15` with `INVALID_PARAM` pointing at `startWriteUserRamJob()`, or document the `IN_PROGRESS` return for this exact boundary. One length value should not change the API contract. |
| C7.4 | **Seven job kinds have no whole-operation deadline.** `deadlineActive` is set at only 7 of ~17 start sites; the rest fall back to a per-transfer bound. They still terminate (verified acyclic), but `SET_TEMPERATURE_EVENT_CONFIG` can run 11 transfers × 100 ms = 1.1 s with no `TIMEOUT` ever produced. | Make the deadline mandatory: delete `deadlineActive`, and have the start helpers always populate `deadlineMs` from a per-`JobKind` transfer-count table. That makes "no whole-operation bound" unrepresentable rather than opt-in. |
| C7.5 | **`mutationCutoffActive` is checked in `processPersistentJob` but not at the three `pollJob` sites** that consume `mutationCutoffMs`. Latent only — all current callers set it — but if a future kind reaches those states without it, `earlierDeadline(now, boundary, 0)` returns `0` and the write is silently never dispatched. | Delete `mutationCutoffActive` and encode "no cutoff" as `mutationCutoffMs == deadlineMs`, so the pair cannot be half-initialized. |
| C7.6 | **`getAlarmConfig()` reports `date = 1` when AE_D is disabled and the register holds `00`.** `AlarmConfig::date` documents `0` as a valid value (the POR inactive state), so this is a misreport — and a `getAlarmConfig()` → `setAlarmTime()` round trip then writes `01h`, silently changing stored hardware state. | Use `disabledFallback = 0` for the date field, matching the register's own inactive encoding. |
| C7.7 | **Verified-set never checks the weekday landed.** `acceptedVerifiedTime()` compares Unix seconds only, which ignores weekday, while the header promises the weekday is *"written exactly"*. A burst that committed `01h..03h`/`05h..07h` but not `04h` still reports verified. | Either check `observed.weekday == requested.weekday \|\| (rolledOver && observed.weekday == (requested.weekday + 1) % 7)` — the rollover term is why it was presumably omitted — or soften the header to "written but not readback-verified; compare `verified.weekday` yourself". |
| C7.8 | **`EEPROM_WRITE_CHECK_CAP = 101`** silently truncates `Config::eepromTimeoutMs`, which is validated and documented as `10..250 ms`. Any value above ~101 ms is unreachable. | Derive the cap (`eepromTimeoutMs + 1`) or narrow the accepted range to `10..100` and update the two docs. |
| C7.9 | **`startPersistentReadJob` does not validate `address`.** Both current callers clamp correctly, so this is defensive only. | Add the range check to the existing guard: `address < CONFIG_EEPROM_START \|\| address + length - 1 > USER_EEPROM_END`. |
| C7.10 | **Century rollover past 2099 is silently reported as year 2000**, with `Status::Ok()` and a 99-year-wrong timestamp. Unavoidable in silicon (no century bit), but undocumented. | One `@note` on `readTime()`/`readUnix()`. Related: `computeWeekday()` rejects valid Gregorian dates outside 2000..2099 with the misleading message `"Invalid Gregorian date"` — change to `"Date outside the supported 2000..2099 domain"`. |
| C7.11 | **Dead branch** in `_updateHealth()`: the `_driverState == UNINIT` disjunct is unreachable, since `_initialized` and `_driverState` are always set and cleared together. | Delete the disjunct; the remainder reduces to `_driverState = DriverState::READY;`. |
| C7.12 | **`parseBuildTime()` pulls in `sscanf`** (`#include <cstdio>`), the library's only stdio dependency — roughly 10–20 KB of newlib `scanf` machinery for parsing two fixed-layout literals. | `__DATE__`/`__TIME__` have fixed layout; ~20 lines of digit arithmetic replaces it and drops the include. |
| C7.13 | **Dead public surface**: `Err::REGISTER_READ_FAILED` is never produced anywhere in `src/` (its comment already says "Reserved legacy"), and `PrimaryCellFailureStage::PRECONDITION` is never assigned. Removing either renumbers a public enum → MAJOR. | Leave both for now, but pin them with explicit numeric values so the layout is frozen, and schedule removal for the next major. Also: `JobOp::backupWriteCompletedMs` is written and read in two adjacent statements and should be a local; `EepromOp::state` is a 40-value enum used as a two-state flag. |

---

## Open findings — examples, tests, tooling

### E1. The reference Wire adapter still transmits a partially-staged write

**Severity: MEDIUM. Confidence: certain.**

When `wire->write(data, len)` stages fewer bytes than requested, the adapter
calls `releaseStartedTransaction`, whose entire body is
`(void)wire.endTransmission(true)`. In every Arduino Wire implementation
`endTransmission` **is** the transfer — there is no abort primitive. So a
truncated `[reg, v0, v1, v2]` becomes `[reg, v0]` physically written (v0 lands
in `reg`), while the callback returns `"I2C write staging incomplete"`, which
reads as "nothing was sent". No test can observe this: the stub discards written
bytes.

The driver treats write errors as ambiguous and reconciles by readback, so this
is not silent corruption — but the adapter's own message is misleading.

**Proposal.** Make the path unreachable by validating length against the real
platform buffer (E2) before `beginTransmission`. If a short stage can still
occur, say plainly in the header that the partial bytes may have reached the
device.

### E2. The adapter's `128`-byte bound is a magic number unrelated to the platform

**Severity: MEDIUM. Confidence: certain.**

`len > 128U` matches `I2C_BUFFER_LENGTH` on Arduino-ESP32, but not
`BUFFER_LENGTH == 32` on AVR or `256` on some SAMD cores. On a core with a
smaller buffer, every `len` in 33..128 falls into the E1 partial-write path
instead of being rejected up front.

**Proposal.**

```cpp
#if defined(I2C_BUFFER_LENGTH)
static constexpr size_t kMaxTransfer = I2C_BUFFER_LENGTH;
#elif defined(BUFFER_LENGTH)
static constexpr size_t kMaxTransfer = BUFFER_LENGTH;
#else
static constexpr size_t kMaxTransfer = 32U;
#endif
```

The driver's largest single transfer is well under 32 bytes, so nothing
legitimate is lost.

### E3. Adapter release paths issue an extra addressed bus transaction

**Severity: MEDIUM. Confidence: likely.**

`releaseStartedTransaction` unconditionally calls `endTransmission(true)`, at
five points where no transmission is meaningfully open — including twice
*after* `endTransmission(false)` has already run. Since Arduino-ESP32 defers the
transfer, the TX buffer still holds the register-pointer byte, so this second
call re-transmits it as a standalone write. The repo's own test asserts the
resulting `endTransmissionCalls == 2`.

That matters because `Config` states that during
`ensurePrimaryCellConfiguration()` *"every read is one physical attempt with no
recovery or retry"* — and the CLI binds the same callbacks for ensure as for
everything else. Two addressed transactions in one read callback is at least in
tension with that.

**Proposal.** Track whether the transmission is actually open and release only
then; never call the release helper after `requestFrom`, which already issued
STOP (the existing code correctly does not).

### E4. The reference adapter is ESP32-only but is presented as generic

**Severity: MEDIUM. Confidence: certain.** `TwoWire::getTimeOut()`,
`setTimeOut(uint16_t)` and the three-argument `bool Wire.begin(sda, scl, freq)`
are all Arduino-ESP32-specific and used unguarded — while the bus-recovery block
below them *is* guarded by `#if defined(ARDUINO_ARCH_ESP32)`. The guard is on the
wrong half. **Proposal:** state the platform in the file header, and either
`#error` on unsupported cores or provide a `ScopedWireTimeout` fallback, so
`Config::i2cTimeoutMs` enforcement stays honest.

### E5. The CLI's pending-operation owner has no deadline and blocks all input

**Severity: MEDIUM. Confidence: needs discussion.** While an operation is
pending, `loop()` returns early and never reads input; the wait has no CLI-side
timeout and no escape key. If `isEepromBusy()` ever failed to clear, the console
would be permanently unresponsive. The driver does bound its own EEPROM work, so
this is a robustness gap in reference code rather than a demonstrated hang.
**Proposal:** record `startedMs` in `PendingOperation` and release ownership
after a generous ceiling (~15 s), printing the surface and last status so the
operator learns *what* stalled.

### E6. The test fake services indirect EEPROM commands to the password registers

**Severity: MEDIUM. Confidence: certain.**

`FakeRv3032` accepts EECMD addresses across the whole `0xC0..0xEA` window, which
spans `0xC6..0xCA` — the EEPROM password bytes. The same class the fake
carefully protects on the *direct* path is fully writable through the *indirect*
path. On silicon a `WRITE_ONE` to `0xCA` arms password protection, which is
permanently destructive.

Unreachable from the public API today — but `startPersistentReadJob` validates
**no address at all** and bypasses `intersectsUnsupportedPasswordRange`
(see C7.9), so any future widening of an EEPROM entry point would pass every
test. **Proposal:** in the fake, raise `EEF` and store nothing for that range,
plus a `passwordCommandAttempted` flag the suite asserts is false. Fix C7.9 in
the driver at the same time.

### E7. Coverage gaps worth closing

| Gap | Detail |
|---|---|
| `pollEeprom` is never called with `maxInstructions == 0` | Budget-0 admission is tested for `pollJob` at three sites, but every literal budget passed to `pollEeprom` is 1, 2, 4 or 5. Despite its name, `test_tick_zero_budget_and_eeprom_end_guards` tests `tick()` before `begin()` — `tick` takes no budget. |
| Primary-cell `WRITE_PERSISTENT` / `VERIFY_PERSISTENT` stages are never asserted | The eight `failureStage` assertions cover only `CLEANUP`, `SETTLE`, `READ_PERSISTENT`, `PREPARE_ACCESS`. The two stages describing a **failed or unverified persistent C0 write** — the destructive ones — have no test. |
| No test exercises the no-clock-hook admission bound | See C3: `FakeRv3032` always supplies `nowMs` and uses `i2cTimeoutMs = 5`, which is exactly why C3 went unnoticed. |
| Three of the 111 "tests" execute zero driver code | They assert the fixture's own bookkeeping. Not worthless — a broken fixture weakens the other 108 — but they should be renamed `test_fixture_*` so "111 native test cases" is not read as 111 driver behaviours. |
| The fake's default bus has zero latency | `callbackDurationMs = 0` unless a test opts in, so timing races that occur when a real 400 kHz transfer takes ~0.2-1 ms are barely exercised. |
| The scanner's error branches are unreachable in its only test | No end-results are queued, so all 112 addresses report as found and the `TO`/`--` render branches never run. |

**Genuinely strong, no gap found:** BCD/date boundary coverage (2099, 2000, leap
years, Feb-29, Dec-31) and millis-wraparound coverage.

### E8. `VERSION_CODE` collides for minor/patch >= 100

**Severity: LOW. Confidence: certain.** `major*10000 + minor*100 + patch` makes
`1.0.100` and `1.1.0` both encode to `10100`. **Proposal:** use
`major*1000000 + minor*1000 + patch`, or document the <= 99 constraint.

Minor items: a whitespace-only input line swallows the CLI prompt;
`read_user_eeprom_chunk` `memcpy`s `length` bytes relying entirely on the driver
to bound it; `cmd_reg` prints the wrong diagnostic for a superfluous `confirm`
on a user-RAM address; `cmd_clkout_freq` indexes a 4-element array with a
driver-supplied enum with no guard; `verbose` is advertised as a global switch
but wired into 3 of ~44 commands.

---

## Open findings — repository tooling

`tools/check_docs_contract.py` (442 lines), `check_core_timing_guard.py` (800),
and `check_cli_contract.py` (239) are **1,481 lines of CI gate, of which roughly
180 lines protect a real invariant.** The rest is archaeology asserting that
long-removed files and symbols are still absent, or exact-substring /
exact-ordering / exact-prose matching that will fire on any legitimate
refactor — including on `clang-format`.

This is not hypothetical. One of these checks **actively pinned a real defect in
place**: `check_cli_contract.py` required the literal
`'enable ? "CLKOUT enable" : "CLKOUT disable"'`, which was exactly the string
causing the CLI to drop its typed job evidence (fixed as F10 above — the fix
required deleting the check). Another required the exact sentence
`"157 PASS, 0 FAIL, 1 SKIP"` in the HIL summary, which means **the next hardware
campaign cannot be recorded without editing CI.**

Three structural problems cut across all three scripts:

1. **`check_core_timing_guard.py` slices `src/RV3032.cpp` by `str.find` of
   function signatures in a fixed order.** Reordering two definitions does not
   fail loudly — it silently mis-slices and produces meaningless pass/fail on
   ~25 downstream checks.
2. **Several checks are indentation-sensitive.** One requires the literal
   `"_job.persistentAddress +\n                                _job.persistentIndex"`
   — 32 exact leading spaces. Running `clang-format` breaks CI.
3. **The same archaeology is triplicated.** `BuildConfig.h`/`BusDiag.h`/
   `HealthDiag.h`/`HealthView.h`/`TransportAdapter.h` are forbidden in two
   scripts; `TS_OVERWRITE_BIT`/`PMU_CLKOUT_DISABLE`/`VERSION_INT` in two more.

Some checks cannot fail at all. `check_core_timing_guard.py`'s
`if success_cap > len(states) or worst_cap != len(states)` compares two
hardcoded constants in the same file. Another asserts
`source.count("persistentCleanupReserveMs(") != 5` with the message *"all four
cleanup-reserve owners"* — the check and its own message already disagree.

### What is worth keeping

| Check | Why it earns its place |
|---|---|
| `millis(`/`micros(`/`delay(`/`delayMicroseconds(`/`yield(` and `#include <Arduino.h>` absent from `src/` + `include/` | **The best check in the repo.** Enforces the core portability contract, and is unprovable by unit test because the native build links `test/stubs/Arduino.h`. Already correctly strips comments and strings. |
| Every `_updateHealth(` call site lies inside a tracked-transport wrapper | **Second best.** A true global "one owner for terminal bookkeeping" invariant that no unit test can express. Reimplement with brace-matching rather than ordered `str.find`. |
| No untimed `readRegs(`/`writeRegs(` reachable from `pollJob`/`processPersistentJob` | Real invariant: all job I/O must be deadline-clipped. |
| `atoi`/`sscanf`/`.toInt(`/`.toFloat(`/`parseInt(`/`g_rtc.tick(`/`g_rtc.isJobBusy(` absent from `examples/` | Genuine negative invariants: never reintroduce a permissive parser or a parallel ownership path. |
| `REG_EE_COMMAND` absent from `examples/` | Real safety invariant — examples must not drive the EEPROM command register directly. |
| Destructive-HIL authorization phrase `CONFIRM-POSSIBLE-C0-WRITE` + `--authorization-c0-write` | Real safety gate on a destructive path. |
| Explicitly-numbered `Err` values pinned in `Status.h` | ABI-visible. Should be **extended** to freeze every numbered enumerator, not just three. |
| `library.json` semver + `export.exclude` completeness + Doxyfile switches | Real packaging invariants. |
| `check_package()` in its entirety | **The best-designed part of the tooling** — it validates the actual shipped tarball rather than source text. |
| Workflow-artifact hygiene (`docs/prompts/**`, `docs/extracted-md/**`, `docs/reports/20??-*.md` absent) | Real hygiene against artifact leakage. |
| The six `clear*Flag` Doxygen warning blocks | Intent is right — this is the one place duplicated prose is load-bearing, because it *is* the public safety contract. Reduce to one short key phrase so rewording does not fail CI. |

### What should go

- **All 44 `deleted_symbols`.** Pure archaeology, and several are generic enough
  (`completedStatus`, `firstVerified`, `newCredential`) to collide with
  unrelated future code.
- **All `REMOVED_COMMON` / deprecated-example / removed-alias checks.** Deleted
  files cannot return by accident.
- **The three private-enum order lists** (40 + 14 + 65 names in exact order) —
  the single biggest refactoring obstacle in the repo. Adding one `JobState`
  fails CI with a message that reads like a real defect. This directly blocks
  the [C1](#c1) ownership split and the [C7.4](#c7) deadline change.
- **Every Markdown prose token**, including the HIL result numbers.
- **Every indentation-sensitive source literal**, and the `str.find`-ordered
  function slicing that underpins them.
- **`re.search(r"<\s*UINT32_MAX", source)`**, which forbids that substring
  anywhere for any purpose, and `"3700" in source`, which will false-positive on
  any future literal containing those digits.
- **The ~22 exact CLI source substrings and the `PendingOperation` struct-order
  regex** — the latter blocks the `startedMs` field recommended in E5.
- **The six native test *function names*** — renaming a test should not fail CI.

### Proposal

Replace 1,481 lines with roughly **200-220 lines** across three focused scripts.
Everything cut is already proven by the 111-case native suite or by compilation.

- **`tools/check_portability.py` (~70 lines)** — the negative invariants no test
  can express: the platform-call and `Arduino.h` bans; the `_updateHealth`
  single-owner check; no untimed `readRegs`/`writeRegs` in the job engines; the
  forbidden parser/ownership calls and `REG_EE_COMMAND` in `examples/`.
- **`tools/check_abi.py` (~40 lines)** — the frozen `Err` value table, and
  version agreement between `library.json` and `Version.h`.
- **`tools/check_package.py` (~110 lines)** — required files, workflow-artifact
  hygiene, `export.exclude`, Doxyfile settings, the destructive-HIL
  authorization phrase, and the existing `check_package()` unchanged.

**Move into the native suite rather than a script:** the CLKOUT-persistence set
(`{0, 2, 3}`), the one-callback-per-job-state cap, the "persistence only after
`REQUESTED_VERIFIED`" ordering, the cleanup-reserve formulas, and the
impossible-state routes. The suite is already structured to express all of
these (`test_phase2_*`, `test_persistent_*`), and as tests they survive
refactoring.

### Documentation duplication

Separately from tooling: every safety-relevant statement exists 3–5 times.
Measured across `README.md`, `docs/ARCHITECTURE.md`, `docs/DEVICE_REFERENCE.md`,
`docs/IDF_PORT.md` and the headers — the THF/TLF status rule appears **5+**
times; the backup enum encoding, the quiescence sequence, the UIE semantics, the
CLKOUT persistence mapping, the 66 ms POR refresh, the transport callback
contract, the password rule and the `probe()` caveat appear **4** times each; the
callback caps and the endurance numbers **3** each.

They all currently agree. That is luck, and it will not survive the next
behavioural change. Roughly 40% of `docs/ARCHITECTURE.md` is `README.md`
restated in different sentences.

Suggested single homes: **`docs/DEVICE_REFERENCE.md`** owns all silicon facts
(register map, status side effects, backup encoding, UIE, POR refresh,
endurance); **`docs/ARCHITECTURE.md`** owns driver mechanics (lifecycle,
transport layering, job/EEPROM engines, budgeting, reconciliation);
**`README.md`** keeps a ~250-line integration path plus the safety contract and
links out; **`docs/IDF_PORT.md`** keeps only the adapter boundary. `AGENTS.md`
should drop its verbatim copies of the `DriverState` block, the transport
diagram, the health-tracking sections and the `Status` struct, and link instead.

---

## Verified correct

Worth recording so it is not re-audited. All of the following were checked
against the manual, and the numeric ones were traced or brute-forced.

**Register map.** Every address and every bit mask in `CommandTable.h` matches
Rev 1.3 — calendar `0x00-0x07`, alarm `0x08-0x0A`, timer `0x0B/0x0C`, status
`0x0D`, temperature `0x0E/0x0F`, control 1/2/3, TS control `0x13`, clock
interrupt mask `0x14`, EVI `0x15`, thresholds `0x16/0x17`, all three timestamp
banks, password/EE staging `0x39-0x3F`, user RAM `0x40-0x4F`, config mirrors
`0xC0-0xCA`, user EEPROM `0xCB-0xEA`, and I2C address `0x51`. **No mismatches.**

**Calendar.** `dateToDays`/`unixToDate`/`dateTimeToUnix` were brute-forced: all
36,525 days from 2000-01-01 to 2099-12-31 are strictly monotonic and match libc
`gmtime`, and ~873,000 timestamp samples round-trip exactly. No overflow — the
2106 32-bit problem cannot occur because the domain caps at `4102444799`.
Boundaries reject correctly on both sides. Weekday is correctly treated as an
opaque user-assigned field per §3.4, with `computeWeekday()` offered separately
as Gregorian policy. BCD validity is enforced at every one of the four decode
sites. `readTime()`'s single `0x01..0x07` burst is exactly what §4.5.2's register
blocking requires — no re-read needed, and the 100th-seconds register is
correctly excluded from the burst.

**Status register.** The RV3032 rule that *any* Status write clears THF/TLF
(§3.7 footnote 1) is honoured in two distinct, deliberate ways, and every writer
of `0x0D` was audited — there are exactly two, and neither writes blind.

**EEPROM protocol.** The Write-One and Read-One sequences match §4.6.5/§4.6.6
step for step, including staged-value readback before the command. EEbusy is
polled from the right register, the right bit, and the right polarity. The 10 ms
/ 1 ms settles match the vendor recommendations exactly. `WRITE_ONE` is never
blindly retried; the two-`READ_ONE` sentinel proof is sound for all 256 byte
values. The vendor's easily-missed prerequisite — disable backup switchover
(BSM=00/11) before *any* EEPROM access, reads included — is implemented.
*Note:* the audit brief's premise that the manual requires writing `0x00` to
EECMD before a command is **not** in Rev 1.3 — that is an RV-3028-C7 convention,
and the driver is right not to do it.

**PMU / primary cell.** `ensurePrimaryCellConfiguration()` **cannot enable
charging on any path** — every byte it can write is either `target` or
`x & 0x4C`, both of which force `TCM=00`. Verified by enumerating all six write
sites. Cleanup runs on every error path (exactly one early return exists, before
any device mutation). The wear-limited `WRITE_ONE` is genuinely never replayed.

**Wrap-safety.** Every deadline and elapsed computation in the driver is
wrap-safe. Two independent reviewers searched for the unsafe `now >= deadline`
form and **neither found a single instance** — the code consistently uses
`(int32_t)(a - b)` comparisons or unsigned elapsed subtraction.

**Instruction budget.** All 66 `pollJob` case blocks and all 40
`processPersistentJob` blocks were mechanically checked: every one performs at
most one transport dispatch, and counting excludes validation/deadline
rejections. `maxInstructions == 1` provably yields at most one callback.

**Health tracking.** `_updateHealth` is called from exactly six sites, all inside
tracked wrappers, all gated on `callbackInvoked`. It is provably never called
for `INVALID_CONFIG`, `INVALID_PARAM`, `NOT_INITIALIZED` or `IN_PROGRESS`, and
`probe()` uses the raw path as documented.

**Buffer bounds.** Every fixed buffer and length parameter was proven bounded.
No unguarded `size_t → uint8_t` truncation. **Zero** `new`/`String`/
`std::vector`/`malloc`/`delay()`/`Wire` in `src/` or `include/`; the only wait is
the injected `waitMs`, reachable solely from the sanctioned primary-cell path.

**Peripherals.** Alarm AE-bit polarity (0 = enabled) and the date-0 POR state;
timer TD encodings and the vendor's stop-before-re-preset ordering; UIE's
non-obvious "suppresses the flag itself" semantics; CLKOUT FD/OS/HFD encoding
including the `divider - 1` scaling in both directions and the stop-output-before-
reconfigure sequence; the offset register's sign convention, verified against the
manual's own worked examples; the 12-bit two's-complement temperature decode,
verified against every row of the vendor table; EVI ET/EHL/ESYN; all three
timestamp block layouts. **No defects found in any of these.**

**Result accessors.** All seven reject unless `completedKind` matches, and no
path was found where an accessor returns another job type's data. ([C1](#c1) is
the inverse failure — results destroyed too eagerly.)
