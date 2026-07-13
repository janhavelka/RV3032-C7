# AI Coder Prompt Suite 04: RV3032-C7 Cooperative Driver And Optional Primary-Cell Provisioning

## Suite order

This release task is split so each implementation pass has one coherent
responsibility. Read this shared contract first, then execute the phase prompts
in filename order. If earlier phases are already implemented, audit their exit
criteria instead of redoing working code.

| Order | Prompt | Scope |
| ---: | --- | --- |
| 0 | `04-00-shared-contract.md` | Binding assignment, execution model, authority, baseline, and vendor facts |
| 1 | `04-01-cooperative-core-architecture.md` | Ownership, passive lifecycle, Config, job budgets, and mutual exclusion |
| 2 | `04-02-calendar-and-capability-coverage.md` | Composite calendar jobs and complete typed capability coverage |
| 3 | `04-03-generic-eeprom-persistence.md` | Explicit persistent inspection and cooperative generic EEPROM protocol |
| 4 | `04-04-primary-cell-and-raw-access.md` | Dedicated synchronous ensure operation and raw/register safety |
| 5 | `04-05-native-fake-verification-and-release.md` | Native fake, tests, example/HIL policy, docs, versioning, and final evidence |

The shared contract is binding for every phase. A phase prompt may narrow the
current edit scope but may not relax a shared safety, ownership, timing, or
verification rule.

## Assignment

Work in the `RV3032-C7` library repository and implement a deliberate `2.0.0`
general-purpose driver hardening release. TunnelMonitor is one demanding
integration profile and validation consumer; it must not become the library's
lifecycle, persistence, battery-chemistry, retry, or scheduling policy.

Preserve the library's useful cooperative, fixed-capacity execution model.
Ordinary multi-transfer operations remain chunked through the existing
`start...Job()`/`pollJob()` and `tick()`/`pollEeprom()` surfaces. Each caller
chooses the instruction budget. A resource owner such as TunnelMonitor may use
a budget of one callback per I2C-owner poll; simpler applications may use a
larger bounded budget.

Make the library useful independently of TunnelMonitor. Audit the complete
vendor-documented functional surface, preserve and correct existing features,
and add missing typed functionality rather than treating unused TunnelMonitor
features as out of scope. At minimum this release adds:

1. A passive, zero-I/O `begin()`.
2. A documented capability matrix and typed public coverage for the chip's
   calendar, alarm, periodic timer/update, event/timestamp, temperature,
   CLKOUT, calibration, backup/trickle, status/reset, user RAM, user EEPROM,
   configuration persistence, and password/protection functions where the
   vendor documents safe application control.
3. A product-neutral chunked status-first calendar-read job.
4. A product-neutral chunked verified calendar-set-and-invalid-flag-clear job.
5. Explicit persistent inspection operations that never run from lifecycle
   methods.
6. One opt-in synchronous `ensurePrimaryCellConfiguration()` convenience
   operation for applications that explicitly declare a non-rechargeable
   primary backup cell.

The primary-cell operation is the only multi-callback method allowed to finish
its full chip sequence in one call. The library never invokes it automatically.
An application may never call it, call it on an operator-controlled path, or
adopt its own once-per-lifecycle policy. TunnelMonitor's later integration will
explicitly invoke it once in the I2C worker during startup. It normally checks
the persistent byte and performs no EEPROM write. It may issue one write-one
command only when the persistent byte is wrong.

Do not replace the existing cooperative library with an all-synchronous driver.
Do not force any application's scheduler into the library. Preserve explicit
instruction budgets so TunnelMonitor can retain normal one-step-per-poll
scheduling while other callers select another bounded budget. Prefer extending
the existing `JobOp`, `JobState`, `EepromOp`, and fixed buffers over adding a
second scheduler, queue, registry, task, or framework.

The result must be simple, deterministic, fixed-capacity, vendor-correct, and
neutral about application policy. When a caller explicitly selects the
primary-cell helper, that path must be safe for a non-rechargeable cell.

Do not edit TunnelMonitor in this task. Exact dependency pinning and firmware
integration are later work.

## Non-negotiable execution model

| Work | Required execution |
| --- | --- |
| `begin()` / `end()` | Zero device I/O |
| `probe()` | One explicit read callback |
| Existing single-transfer APIs | May remain synchronous |
| Ordinary multi-transfer jobs | Existing start/poll model; caller controls callback budget |
| Composite calendar read/set | Start a typed library job; caller selects a bounded `pollJob()` instruction budget |
| Generic opt-in persistence | Retain cooperative `tick()`/`pollEeprom()` behavior; harden the protocol rather than bypassing its bounds |
| Primary-cell ensure | Sole synchronous multi-callback exception; terminal within 1000 ms with conforming callbacks |

The library owns chip phases, register knowledge, typed capabilities, and safe
protocol execution. The application owns the I2C bus, worker/task if any,
queue admission, scheduling, physical bus recovery, retry policy, persistence
policy, battery-chemistry policy, and result publication.

## Product-neutral library rule

- `begin()` and `recover()` never infer battery chemistry, desired backup mode,
  or persistence policy and never inspect or modify EEPROM implicitly.
- Every active-only change, persistent change, persistent inspection, and
  semantic battery-policy helper is an explicit caller action.
- `enableEepromWrites=false` remains a fully supported normal configuration,
  not a degraded mode. Active-only APIs still work and report that the change
  is volatile.
- Persistent inspection is cheap in EEPROM wear but not side-effect-free: safe
  direct access temporarily changes volatile EERD/BSM state. Therefore even a
  read must be explicit, bounded, mutually exclusive, and cleanup-verified.
- `ensurePrimaryCellConfiguration()` is a convenience policy operation, not a
  lifecycle requirement. It is available to every caller but is never invoked
  by the library itself.
- TunnelMonitor-specific timing, one-instruction polling, automatic startup
  invocation, and disabled-generic-persistence choices are compatibility tests,
  not defaults imposed on other applications.
- Generality means typed coverage of documented RV3032 functionality. It does
  not mean a framework, hidden task, unbounded queue, or unsafe generic EECMD
  escape hatch.

## Mandatory working method

1. Read repository `AGENTS.md` completely.
2. Run `git status --short`; preserve all user changes and do not reset or
   reformat unrelated work.
3. Read this shared contract and all five ordered phase prompts before editing source.
4. Read and visually inspect the local vendor PDFs cited below.
5. Audit current public headers, source, native tests, example, tools, and
   maintained documentation.
6. Use subagents for parallel read-only audits covering at least:
   - vendor register/protocol facts;
   - complete vendor capability-to-public-API coverage, including functionality
     not consumed by TunnelMonitor;
   - lifecycle, transport, health, and public API compatibility;
   - `pollJob()` and all current multi-transfer APIs;
   - `pollEeprom()` protocol and fake fidelity;
   - product-neutral API/persistence behavior and TunnelMonitor adapter fit as
     one compatibility profile;
   - examples, HIL, documentation, and versioning.
7. Integrate the findings yourself. Do not delegate final design judgment,
   edits, or verification claims.
8. Create a requirement-to-evidence matrix in a dated implementation report
   before source edits.
9. Implement a complete first pass and run focused tests.
10. Re-read this shared contract and the applicable phase prompts, close omissions, and run the relevant matrix.
11. Inspect the complete diff, then read the complete prompt suite a third time and audit
    every acceptance criterion against actual code/tests.
12. Do not commit, tag, publish, upload, flash, issue EEPROM commands, or edit
    the sibling TunnelMonitor repository without separate authorization.

If this prompt suite conflicts with the vendor manual, stop and report the exact page,
table, and conflict. Do not guess.

## Authority and required reading

Use this order:

1. Repository `AGENTS.md`.
2. Local Micro Crystal documents:
   - `docs/reference-pdfs/RV-3032-C7_App-Manual.pdf`, Rev. 1.3, May 2023;
   - `docs/reference-pdfs/RV-3032-C7_datasheet.pdf`.
3. This shared contract and the ordered phase prompts.
4. Maintained code/docs as current-implementation evidence only.

Visually inspect at least application-manual pages 13-16, 21, 23-30, 43-51,
65-72, 130, 139, and 141. Confirm Control registers, Status semantics,
configuration mirrors, direct EEPROM access, `EEbusy`, `EEF`, minimum waits,
voltage, endurance, and primary-cell application guidance.

For integration fit, inspect these sibling files read-only:

- `../TunnelMonitor-node/AGENTS.md`
- `../TunnelMonitor-node/docs/guidelines/decisions.md`
- `../TunnelMonitor-node/docs/guidelines/rtc_fram.md`
- `../TunnelMonitor-node/docs/guidelines/i2c_peripherals.md`
- `../TunnelMonitor-node/docs/guidelines/dependency_policy.md`
- `../TunnelMonitor-node/docs/guidelines/ownership.md`
- `../TunnelMonitor-node/docs/guidelines/interfaces.md`
- `../TunnelMonitor-node/docs/guidelines/validation_plan.md`
- `../TunnelMonitor-node/docs/guidelines/implementation_plan.md`
- `../TunnelMonitor-node/docs/guidelines/open_questions.md`
- `../TunnelMonitor-node/src/i2c/I2cTask.cpp`
- `../TunnelMonitor-node/src/i2c/I2cDiagnostics.cpp`
- `../TunnelMonitor-node/include/TunnelMonitor/i2c/I2cTask.h`
- `../TunnelMonitor-node/test/native/test_i2c_task_fake.cpp`

TunnelMonitor prompts/reports do not override its guidelines. Treat the
following as one later integration profile, not as general library defaults:

- normal calendar/status operations remain owner-poll stepped;
- the library owns those chip-specific job phases;
- TunnelMonitor advances at most one normal library callback per owner poll;
- only primary-cell ensure may execute a full synchronous sequence;
- TunnelMonitor, not the library, will explicitly call ensure once per
  I2C-owner startup;
- that application call reads persistent C0 directly every startup;
- correct C0 causes zero `0x21`; wrong C0 permits at most one;
- no periodic, recovery, CLI, web, settings, `ReadRtc`, or `SetRtc` call starts
  provisioning;
- no RV3032 register, mask, BCD codec, or protocol-phase enum remains in
  `I2cTask` after later integration.

The existing TunnelMonitor Prompt 43 predates this exact hybrid decision. Do not
use its external-only provisioning or chunked-provisioning requirements as
authority.

## Audited current baseline to verify

Verify and record the exact current baseline before editing:

- Current version is `1.6.0`.
- Native tests currently report 56 tests.
- `esp32s3dev` currently builds.
- `pollJob()` already provides fixed state, fixed storage, caller instruction
  budget, and useful one-step-per-poll scaffolding.
- `tick()`/`pollEeprom()` and the eight-entry queue provide bounded cooperative
  scheduling scaffolding.
- The EEPROM protocol inside `processEeprom()` is incomplete: it lacks verified
  safe BSM/TCM access state, EEADDR/EEDATA staging readback, the required 10 ms
  pre-poll wait, stale-EEF separation, ambiguous-write reconciliation, direct
  persistent verification, and fully verified cleanup.
- Current `processEeprom()` writes command value `0x21`; it does not issue
  update-all `0x11`. The current name `EEPROM_CMD_UPDATE` is misleading and must
  become WRITE_ONE.
- `begin()` probes and calls `_applyConfig()`, so lifecycle binding can change
  active C0 and may queue persistence.
- `recover()` probes and reapplies configuration.
- `setPrimaryBatteryBackupDefaults()` clears C0 low nibble `0x0F`, which also
  changes TCR.
- The flat native fake does not model separate active mirrors and persistent
  EEPROM, EECMD execution, busy timing, EEF, ambiguity, or POR refresh.
- The example enables persistence and applies primary-cell defaults during
  startup.
- The example user-EEPROM helper reads EEDATA too soon after `0x22`.
- README/Config incorrectly claim about 100,000 EEPROM writes.

Also audit and correct at least these known register/API problems:

- fictitious Control 1 bit 7 `TRPT`;
- incorrect Control 2 definitions; AIE is bit 3 and EIE is bit 2;
- Control 3 incorrectly described as reserved;
- fictitious EVI bit 3 `EVI_EN`;
- incorrect `PMU_TRICKLE_MASK = 0x0F`;
- misleading `EEPROM_CMD_UPDATE = 0x21`; it is write-one;
- direct register access incorrectly accepts indirect user EEPROM `0xCB..0xEA`;
- timer high register `0x0C` uses only bits 3:0;
- incorrect TEMP_LSB description;
- CLKOUT getters/setters ignore C3.OS high-frequency mode;
- offset documentation overstates the signed six-bit range.

This list is a minimum. Compare the complete register/command table and every
vendor-documented functional block against the manual. Do not omit a documented
chip capability merely because TunnelMonitor does not use it. Add or correct
small typed APIs in the existing driver for missing functionality; do not create
unrelated frameworks, speculative application services, or unsafe raw command
surfaces.

## Vendor facts that must remain true

### Memory model

- Direct `0xC0..0xCA` are active configuration RAM mirrors.
- Separate configuration EEPROM stores persistent `0xC0..0xCA`.
- User EEPROM `0xCB..0xEA` is persistent and indirect through
  EEADDR/EEDATA/EECMD; it is not a direct register window.
- Power-on refresh, typically about 66 ms, copies configuration EEPROM to active
  mirrors.
- An automatic refresh copies EEPROM to active mirrors at date increment about
  once per 24 hours in VDD state when EERD=0.
- Writing an active mirror changes live behavior only. It does not automatically
  commit to EEPROM.
- Calendar timekeeping is maintained from VBACKUP and does not wear EEPROM.
- The chip contains no FRAM.

### EEPROM controls

- Control 1 is register `0x10`.
- EERD is Control 1 bit 2, mask `0x04`.
- EERD=1 disables automatic EEPROM-to-active refresh during explicit access; it
  does not enable writes or commit anything.
- EEADDR=`0x3D`, EEDATA=`0x3E`, EECMD=`0x3F`.
- EECMD is write-only and reads zero.
- Commands are exactly:
  - update all active mirrors to EEPROM: `0x11`;
  - refresh all EEPROM values to active mirrors: `0x12`;
  - write one staged byte to EEADDR: `0x21`;
  - read one EEPROM byte at EEADDR into EEDATA: `0x22`.
- Primary-cell ensure uses only `0x21` and `0x22`; never `0x11` or `0x12`.
- EEbusy is TEMP_LSB bit 2, mask `0x04`; a command presented while busy is
  ignored.
- EEF is TEMP_LSB bit 3, mask `0x08`; it is sticky until cleared and indicates
  EEPROM write failure such as low VDD. It does not prove read success or byte
  equality.
- CLKF/BSF are TEMP_LSB bits 1/0 and use write-zero-to-clear semantics; writing
  one preserves them.
- I2C ACK proves only transport acceptance. Durable success requires direct
  persistent readback.

### Status register

- Status is register `0x0D`.
- PORF is bit 1 and VLF is bit 0.
- UF/TF/AF/EVF and PORF/VLF are write-zero-to-clear.
- Every Status write unavoidably clears THF/TLF regardless of the written value.
- A verified calendar set may clear PORF/VLF only through an API whose name
  states that behavior and whose report records THF/TLF observed immediately
  before the Status write.

### Timing, voltage, and endurance

- Read-one typical completion is about 1.1 ms. Do not check completion before
  1 ms; then poll EEbusy until clear.
- Write-one minimum/typical/maximum are about 1.2/4.8/9 ms. Wait at least 10 ms
  before the first completion check.
- EEPROM access requires stable VDD. Write-one requires at least 1.6 V.
- TunnelMonitor's fixed 400 kHz I2C requires RTC VDD at least 2.0 V.
- Enabling level switching requires VDD safely above the maximum 2.2 V LSM
  threshold through activation and the complete 10 ms settle.
- TunnelMonitor expects a 3.3 V main rail but must prove the rail and backfeed
  assumptions by board evidence/HIL. The library cannot measure VDD quality.
- Guaranteed EEPROM endurance is 10,000 cycles at 3.0 V/25 C and 100 cycles at
  5.5 V/85 C. Never claim 100,000 cycles.
- No EEPROM wait may busy-spin or be unbounded.

### Primary-cell C0 policy

C0 fields:

- bit 7 unimplemented; write zero;
- bit 6 NCLKE; preserve;
- bits 5:4 BSM: `00` disabled, `01` direct, `10` level, `11` disabled;
- bits 3:2 TCR; preserve;
- bits 1:0 TCM: `00` disables trickle charging.

For a non-rechargeable cell, derive the target exactly:

```cpp
target = static_cast<uint8_t>((persistentC0 & 0x4C) | 0x20);
```

This preserves NCLKE/TCR, clears reserved bit 7 and TCM, and selects level
switching.

Application-manual page 141 illustrates this policy from the delivery-default
state: its `TCR=00` bits are black (unchanged), while `TCM=00` is red as the
required non-rechargeable-cell setting. The semantic requirement is therefore
to disable charging through `TCM=00`, not to canonicalize the inactive TCR
field. An implementation operating on an already configured device preserves
TCR with mask `0x0C` as shown in the exact target above.

During direct EEPROM access derive the safe live value exactly:

```cpp
safeActive = static_cast<uint8_t>(activeC0 & 0x4C);
```

This selects BSM `00`, TCM `00`, preserves NCLKE/TCR, and clears bit 7.
