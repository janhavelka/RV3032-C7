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
attempt must be covered by its timing tests.

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
  st = rtc.probe();                  // explicit one-read presence check
}
```

## Cooperative owner loop

Admission methods perform no transfer. The I2C owner advances work explicitly:

```cpp
uint8_t used = 0;
rtc.pollJob(idfNowMs(nullptr), 1, used);
rtc.pollEeprom(idfNowMs(nullptr), 1, used);
```

Keep the two calls serialized; inspect `isJobBusy()`/`isEepromBusy()` before
choosing which owner action to admit. A budget of one guarantees at most one
library transport callback in that poll. `tick()` is only the one-instruction
compatibility scheduler for generic persistence.

The synchronous primary-cell ensure is a deliberate startup-only boundary. Run
it in the serialized I2C owner, with no pending job or persistence work. Its
wait callback must yield the task; never implement it as a spin loop.

## Porting rules

- Keep ESP-IDF types and error codes inside the adapter.
- Map all failures to `RV3032::Status` and honor the supplied timeout.
- Do not add bus recovery, retry, driver installation, or logging to the core.
- Do not treat `OFFLINE` as an admission block.
- Keep EEPROM writes disabled unless the product explicitly owns an endurance
  policy and regularly polls the cooperative engine.
- Call `end()` before rebinding; a second `begin()` is intentionally `BUSY`.

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
