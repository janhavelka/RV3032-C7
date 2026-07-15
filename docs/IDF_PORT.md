# RV3032-C7 ESP-IDF adapter notes

Primary support is PlatformIO with Arduino. The core remains portable because
all hardware access is injected; this repository does not ship a native
ESP-IDF component or example.

## Adapter boundary

An ESP-IDF integration supplies:

1. `Config::i2cWrite` - one bounded physical mutating attempt;
2. `Config::i2cWriteRead` - one bounded read attempt from the library's point
   of view;
3. `Config::nowMs` - a wrapping monotonic millisecond clock;
4. `Config::waitMs` - a yielding sleep callback, required only if the
   application will call `ensurePrimaryCellConfiguration()`.

The adapter owns I2C installation, pins, pull-ups, locking, per-transfer timeout
enforcement, error mapping, bus recovery, retry policy, and task scheduling.
Never replay a possibly mutating callback. If the application documents a
bounded recovery plus one retry for read-only transfers, that extra physical
attempt must be covered by its timing tests. Both attempts together must finish
inside the one supplied callback timeout and count as one driver instruction.

Both callbacks are synchronous and may retain no buffer pointer after return.
They return only `OK`, `I2C_ERROR`, `I2C_NACK_ADDR`, `I2C_NACK_DATA`,
`I2C_TIMEOUT`, or `I2C_BUS`, with a static-lifetime message. Cooperative
deadlines are exclusive; the core clips the supplied timeout and checks the
callback completion timestamp. The supplied interval is a hard bound on the complete adapter callback,
including application serialization, permitted
read-only recovery, and all physical phases. Arrange an uncontended serialized
owner before dispatch. Without `nowMs`, the driver conservatively charges the
full supplied timeout.

```cpp
static uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static void idfWaitMs(uint32_t delayMs, void*) {
  vTaskDelay(pdMS_TO_TICKS(delayMs));
}

RV3032::Config cfg{};
cfg.i2cWrite = myI2cWrite;
cfg.i2cWriteRead = myI2cWriteRead;
cfg.i2cUser = myContext;
cfg.nowMs = idfNowMs;
cfg.waitMs = idfWaitMs;
cfg.enableEepromWrites = false;

RV3032::RV3032 rtc;
RV3032::Status st = rtc.begin(cfg);  // passive: no I2C
if (st.ok()) {
  st = rtc.probe();                  // address communication, not identity
}
```

A successful probe proves only that address `0x51` answered the Status-register
read for that transaction. It does not prove RV3032 silicon identity and it
does not update health counters or state.

## Cooperative owner loop

Admission methods perform no transfer. The I2C owner advances work explicitly:

```cpp
const uint32_t now = idfNowMs(nullptr);
uint8_t used = 0;
RV3032::Status pollStatus;
if (rtc.isJobBusy()) {
  pollStatus = rtc.pollJob(now, 1, used);
} else {
  pollStatus = rtc.pollEeprom(now, 1, used);
}
// Retain, report, or otherwise handle pollStatus according to product policy.
```

Choose exactly one surface per owner-loop iteration. Do not poll EEPROM in the
same iteration in which an ordinary job terminates and may enqueue persistence.
A budget of one therefore guarantees at most one library transport callback in
that owner pass. `Status tick(uint32_t nowMs)` is only the one-instruction
scheduler for generic persistence. It returns
`NOT_INITIALIZED`, ordinary-job `BUSY`, progress, or cached terminal EEPROM
status; the integration must observe or explicitly discard that result.

Staged timer, periodic-update, backup, CLKOUT, and temperature-event jobs expose
`ConfigurationJobReport`. Keep their exact terminal Status and separate
operation/cleanup evidence together; do not infer success from an ACK or queue
persistence before requested-state readback. A requested mutation is one
physical attempt and is never retried. Backup admission additionally uses
`4 * i2cTimeoutMs + activationMs + 1`, with 2 ms for disabled-to-Direct and
10 ms for disabled-to-Level, and waits after callback completion without I/O.

Before live reconfiguration, the application owner uses `disable interrupt ->
consume/clear flag if appropriate -> configure -> enable`. The corresponding
TIE/AIE/EIE/BSIE guard returns `BUSY` without mutation. EIE=0 does not stop EVI
timestamp capture. UIE=0 suppresses both new UF and the INT event; it is not a
UF-polling-only mode.

The synchronous primary-cell ensure is a deliberate startup-only boundary. Run
it in the serialized I2C owner, with no pending job or persistence work. Its
wait callback must yield the task; never implement it as a spin loop.

## Porting rules

- Keep ESP-IDF types and error codes inside the adapter.
- Map all failures to `RV3032::Status` and honor the supplied timeout.
- Do not add bus recovery, retry, driver installation, or logging to the core.
- Do not treat `OFFLINE` as an admission block.
- Keep EEPROM writes disabled unless the product explicitly owns an endurance
  policy and regularly polls the cooperative engine. That authority covers
  only configuration C0..C5 and typed user EEPROM CB..EA.
- Preserve separate persistent `operationStatus`, `cleanupStatus`, durable
  proof, and partial counts. A cleanup failure has semantic terminal precedence
  but does not erase content proof already established by readback.
- Password management is unsupported. Password protection must be disabled;
  a protected part requires out-of-band service.
- Call `end()` before rebinding; a second `begin()` is intentionally `BUSY`.
  `end()` performs zero I/O and abandons active local work, so reinitialize any
  affected product policy after abandoning persistent cleanup.

## Local verification

```sh
python scripts/generate_version.py check
python tools/check_core_timing_guard.py
python -m platformio test -e native
python tools/check_cli_contract.py
python -m platformio run -e esp32s2dev
python -m platformio run -e esp32s3dev
```

These checks do not prove hardware wiring, power-loss behavior, backup-cell
chemistry, EEPROM endurance, or long-term timing accuracy.
