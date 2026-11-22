# RV3032-C7 ESP-IDF integration

The repository root is a C++17 ESP-IDF component. Add it to a project's
component directory to use the public headers under `RV3032/`. The manifest
declares ESP-IDF >=6.0.1 for ESP32-S2 and ESP32-S3. The reference build uses
6.0.1. The core needs only standard headers and the application's callbacks.

## Native bring-up example

Build `examples/esp_idf/basic` for either target using the commands in the
[verification guide](VERIFICATION.md). Its local `components/RV3032` wrapper
registers the same source and public headers under a fixed dependency name,
including when the package is unpacked into an arbitrarily named directory.
The defaults select USB Serial/JTAG on S3 and USB CDC on S2. Both request
no secondary console. ESP-IDF 6.0.1 applies that choice on S3 and omits it
from the generated S2 sdkconfig because S2 lacks the Serial/JTAG peripheral.
[Console Kconfig](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_stdio/Kconfig#L20-L50)

One task owns I2C port 0, SDA GPIO8 and SCL GPIO9 at 400 kHz. Fit external
pull-ups sized for the bus. Internal pull-ups are insufficient at this speed.
Startup releases the open-drain lines, clocks SCL nine times with a bounded
stretch wait, generates STOP and checks both lines before installing the bus
and the 7-bit device at 0x51. Teardown removes the device before the bus and
retains any handle whose removal failed. The task binds the callbacks with
generic EEPROM writes disabled, explicitly probes the Status register, then
prints one calendar read per second.

## ESP-IDF 6.0.1 transport behavior

The example uses synchronous transfers with a zero transaction queue depth
and no event callbacks. Each admitted write calls `i2c_master_transmit` once.
Each admitted register read calls `i2c_master_transmit_receive` once.
The adapter never retries, probes the address separately, or requests a bus
reset inside a callback. This also satisfies the single-attempt rule during
`ensurePrimaryCellConfiguration()`. The ordinary callback timeout is
1..100 ms (default 50 ms), and the primary-cell timeout is 1..5 ms.

| SDK transfer result | Callback status before the deadline |
|---|---|
| `ESP_OK` | `OK` |
| `ESP_ERR_TIMEOUT` | `I2C_TIMEOUT` |
| `ESP_ERR_INVALID_RESPONSE` or any other SDK error | `I2C_ERROR`, with the original `esp_err_t` in `detail` |

- The API takes a signed millisecond timeout. Passing `-1` waits forever.
  Internally the driver converts milliseconds to whole RTOS ticks. A positive
  sub-tick interval can become zero. The adapter requires a 1000 Hz tick.
  It reserves the exclusive deadline, skips dispatch with 1 ms or less left,
  and always supplies a positive finite SDK timeout. [Transfer declarations](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/include/driver/i2c_master.h#L167-L215)
  [Tick conversion](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/i2c_master.c#L671-L679)
- A stalled SDK call can consume its timeout more than once. The SDK waits
  for the bus lock, then for each command batch and the completion event.
  Lock expiry returns `ESP_ERR_TIMEOUT`. The adapter measures the whole call
  with `esp_timer_get_time()` and returns `I2C_TIMEOUT` at or after its
  deadline, even for SDK success. It cannot cancel the transfer, undo a write,
  or guarantee the calling task returns within budget on a stalled bus.
  [Bus lock](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/i2c_master.c#L999-L1007)
  [Command and completion waits](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/i2c_master.c#L534-L602)
- Synchronous transfer completion maps every non-DONE state, including NACK
  and controller timeout, to `ESP_ERR_INVALID_RESPONSE`. There is no byte
  count or phase to distinguish address from data NACK. The SDK address-only
  probe has a separate `ESP_ERR_NOT_FOUND` mapping. The library's `probe()`
  uses the ordinary register-read callback. [Completion status](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/i2c_master.c#L715-L724)
  [Address-only probe](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/i2c_master.c#L1389-L1397)
- After a transaction error the SDK resets the controller FSM. At the next
  transaction start, a timeout state or busy bus triggers a reset with bus
  clear. The SDK can also clear the bus during a hardware timeout or stuck
  STOP. These paths do not replay the application transfer. The adapter never
  calls `i2c_master_bus_reset()`. [Error exit](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/i2c_master.c#L1026-L1030)
  [Transaction start](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/i2c_master.c#L675-L680)
  [In-transfer resets](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/i2c_master.c#L545-L595)
  [Explicit bus reset](https://github.com/espressif/esp-idf/blob/v6.0.1/components/esp_driver_i2c/i2c_master.c#L1241-L1247)

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

Preserve the backend's actual failure category when available. A zero or short
read alone does not prove an address NACK; report `I2C_ERROR` with the received
byte count when no more specific cause is available. `I2C_NACK_DATA` remains
distinct from `I2C_NACK_ADDR`, which `probe()`/`recover()` map to
`DEVICE_NOT_FOUND`.

```cpp
static uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static void idfWaitMs(uint32_t delayMs, void*) {
  TickType_t ticks = pdMS_TO_TICKS(delayMs);
  if (ticks == 0 || ticks * portTICK_PERIOD_MS < delayMs) {
    ++ticks;  // Round up to the requested duration.
  }
  vTaskDelay(ticks + 1U);  // Guard against entering near a tick boundary.
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
RV3032::Status pollStatus = RV3032::Status::Ok();
if (rtc.isOrdinaryJobBusy()) {
  pollStatus = rtc.pollJob(now, 1, used);
} else if (rtc.isEepromPollable()) {
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

CLKOUT persistence is a two-surface operation. Active job success proves C0,
C2, and C3 mirrors only. When `enableEepromWrites` is true, keep ownership and
poll the generic EEPROM surface until its separate terminal success. Enable-only
changes queue C0; legacy-frequency and complete-configuration changes queue
C0/C2/C3. On cold power-up, wait for EEbusy to clear before relying on a custom
stored CLKOUT selection; the POR refresh takes approximately 66 ms. The pin is
LOW in VBACKUP regardless of the selected frequency.

Before live reconfiguration, the application owner uses `disable interrupt ->
consume/clear flag if appropriate -> configure -> enable`. The corresponding
TIE/AIE/EIE/BSIE guard returns `BUSY` without mutation. EIE=0 does not stop EVI
timestamp capture. UIE=0 suppresses both new UF and the INT event; it is not a
UF-polling-only mode.

The synchronous primary-cell ensure is a deliberate startup-only boundary. Run
it in the serialized I2C owner, with no pending job or persistence work. Its
wait callback must yield the task and must not return before the requested
monotonic duration; never implement it as a spin loop. A relative RTOS tick
delay therefore needs both upward tick rounding and one entry-phase guard tick.

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
  `end()` performs zero I/O and abandons active local work. An unproven
  persistent-access state survives rebinding on the same object; explicitly
  run `startPersistentAccessStateRecoveryJob()` with the intended C0 and
  inspect its restoration evidence. `begin()`, `probe()`, and `recover()`
  cannot clear that evidence.
- Keep consumer dependency pins and application-version metadata in the
  consumer project. This repository's generator owns only the RV3032
  `Version.h` and build metadata; an adapter must not turn it into a shared
  application dependency registry.

## Local verification

Use the command list and evidence boundaries in the [verification guide](VERIFICATION.md).
Those checks do not prove hardware wiring, power-loss behavior, backup-cell
chemistry, EEPROM endurance, or long-term timing accuracy.
