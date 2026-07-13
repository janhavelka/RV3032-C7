# AI Coder Prompt 04.3: Cooperative Generic EEPROM Persistence

## Phase contract

This is phase 3 of 5 in Prompt Suite 04.

- Required first: read `04-00-shared-contract.md` completely.
- Prerequisites: phases 1 and 2 are complete or their public/job contracts have been re-audited.
- Current scope: Implement explicit persistent inspection, bounded user EEPROM jobs, and the vendor-correct shared cooperative persistence engine.
- Exit criteria: persistent reads/writes are explicit, fixed-capacity, deadline-bounded, durably verified, cleanup-evidenced, and tested without UPDATE_ALL or REFRESH_ALL dispatch.
- Preserve unrelated work and keep the shared dated requirement/evidence report
  current. Do not commit, tag, publish, flash, or run hardware EEPROM work.

## Cooperative generic EEPROM persistence

Persistent access is an explicit general-library capability, not an automatic
startup check. Add product-neutral typed inspection/read surfaces using the
existing fixed job engine:

```cpp
enum class ConfigurationEepromRegister : uint8_t {
  PMU = 0xC0,
  OFFSET = 0xC1,
  CLKOUT1 = 0xC2,
  CLKOUT2 = 0xC3,
  TEMPERATURE_REFERENCE0 = 0xC4,
  TEMPERATURE_REFERENCE1 = 0xC5,
};

constexpr uint8_t USER_EEPROM_SIZE = 32;
constexpr uint8_t USER_EEPROM_JOB_MAX_BYTES = 16;

struct PersistentReadResult {
  uint8_t eepromAddress = 0;
  uint8_t data[USER_EEPROM_JOB_MAX_BYTES] = {};
  uint8_t length = 0;
  bool persistentVerified = false;
  bool cleanupVerified = false;
};

struct UserEepromWriteReport {
  uint8_t offset = 0;
  uint8_t requestedLength = 0;
  uint8_t completedBytes = 0;
  uint8_t durablyVerifiedBytes = 0;
  bool cleanupVerified = false;
};

Status startReadConfigurationEepromJob(
    ConfigurationEepromRegister reg,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = 250);

Status startReadUserEepromJob(
    uint8_t offset,
    uint8_t length,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = 1000);

Status startWriteUserEepromJob(
    uint8_t offset,
    const uint8_t* data,
    uint8_t length,
    uint32_t nowMs,
    uint32_t operationTimeoutMs = 4000);

Status getPersistentReadJobResult(PersistentReadResult& out) const;
Status getUserEepromWriteJobResult(UserEepromWriteReport& out) const;
```

Use offsets `0..31` for public user-EEPROM APIs and translate internally to
`0xCB..0xEA`. Reject zero length, length above 16, or a range crossing byte 31
with zero I2C. Copy write input into fixed job storage at successful admission.
Persistent reads are permitted when `enableEepromWrites=false`; explicit user-
EEPROM writes and persistent configuration changes require it to be true.
Every persistent read uses direct read-one, positive data replacement proof,
safe access state, and verified cleanup. Every user-EEPROM byte write performs
compare-before-write, at most one `0x21` attempt for that byte, direct readback,
and bounded cleanup. A multi-byte failure stops before later bytes and reports
the verified completed-byte count. Do not expose password bytes `0xC6..0xCA`
through the configuration-inspection enum; their vendor protection flow uses
dedicated typed APIs and redacted evidence.

Keep these bounded public surfaces and their fixed queue:

```cpp
void tick(uint32_t nowMs);
Status pollEeprom(uint32_t nowMs,
                  uint8_t maxInstructions,
                  uint8_t& instructionsUsed);
bool isEepromBusy() const;
Status getEepromStatus() const;
uint32_t eepromWriteCount() const;
uint32_t eepromWriteFailures() const;
uint8_t eepromQueueDepth() const;
```

`enableEepromWrites=false` is a normal application choice. It disables generic
persistent mutation while preserving active-only control and explicit
persistent reads. Calling primary-cell ensure is separate explicit authority
for that method's possible single C0 write; it does not depend on or change the
generic persistence setting and never uses this queue. TunnelMonitor's later
integration will keep generic persistence disabled, but other applications may
enable it. Retaining the API does not permit retaining the current unsafe
protocol or making false durability claims.

Refactor each queued item to mean: ensure the selected supported configuration
EEPROM byte equals the desired active value. It remains chunked and must:

1. Serialize against `_job` and ensure.
2. Reserve queue capacity before changing the active mirror; queue-full returns
   before mutation.
3. Save and verify Control 1/EERD and active C0.
4. Establish EEbusy clear with bounded checks.
5. Disable and verify active BSM=`00`, TCM=`00` while preserving NCLKE/TCR.
6. Direct-read the selected persistent byte before deciding to write.
7. If already equal, perform zero `0x21` and clean up.
8. Clear/check stale EEF, verify EEADDR/EEDATA staging, issue `0x21` once, wait
   at least 10 ms through poll timestamps, then check busy/EEF.
9. Direct-read persistent state and require exact equality.
10. Restore and verify the intended active mirror, active C0 access state, and
    exact saved Control 1 EERD state. Honor required BSM settle time.
11. Preserve primary failure and distinguish cleanup failure.

For arbitrary persistent bytes, no single impossible EEDATA sentinel exists.
Use a vendor-correct adaptive two-read proof: verify EEADDR, preload a first
sentinel, issue `0x22`, wait/poll, capture R1; preload a second value chosen not
equal to R1, issue a second `0x22`, wait/poll, and require R2==R1 and R2 different
from the second preload. Never infer persistence from the active mirror.

Every state consumes at most one callback instruction. Minimum 1/10 ms waits are
represented by wrap-safe not-before timestamps; a poll before the timestamp
does zero I2C and returns IN_PROGRESS. Busy polling has explicit interval,
count, phase-deadline, and whole-item bounds.

A mutating callback error is never resent. A possibly committed `0x21` proceeds
to bounded busy/EEF/direct-read reconciliation. At most one `0x21` physical
attempt is structurally possible per queued item.

Coalesce an exact duplicate pending address/value without adding a second queue
entry. Do not silently discard a different pending value. Keep the queue fixed
at eight entries and allocate no heap.

Do not issue `0x11` or `0x12` from generic persistence. Rename command constants
to `EEPROM_CMD_WRITE_ONE`, `EEPROM_CMD_READ_ONE`,
`EEPROM_CMD_UPDATE_ALL`, and `EEPROM_CMD_REFRESH_ALL` with exact values.

