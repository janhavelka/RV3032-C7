# RV3032-C7 ESP-IDF Adapter Notes

Primary support is PlatformIO + Arduino. The core driver remains portable
because hardware access is supplied through callbacks, but this repository does
not currently ship a native ESP-IDF component or ESP-IDF example.

## Adapter Boundary

To use the core from ESP-IDF code, provide:

1. `Config::i2cWrite`
2. `Config::i2cWriteRead`
3. optional `Config::nowMs` for health timestamps

The adapter owns the I2C driver, pins, pull-ups, locking, bus timeout policy,
and task scheduling.

## Minimal Pattern

```cpp
static uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

RV3032::Config cfg{};
cfg.i2cWrite = myI2cWrite;
cfg.i2cWriteRead = myI2cWriteRead;
cfg.i2cUser = myContext;
cfg.nowMs = idfNowMs;
cfg.enableEepromWrites = false;
```

Call `tick(nowMs)` regularly if EEPROM persistence may be active. For strict
owner-task instruction budgets, use `pollEeprom()` and `pollJob()`.

## Porting Rules

- Keep I2C caller-owned; do not add `Wire` or ESP-IDF driver objects to public
  contracts.
- Keep timeout and error mapping inside the adapter callbacks.
- Do not add hidden timing fallbacks to the core. If `nowMs` is absent, health
  timestamps remain `0`.
- Keep EEPROM writes explicit opt-in.
- Keep `recover()` application-controlled; do not auto-recover in `tick()`.

## Local Verification

Run the same checks as CI:

```sh
python scripts/generate_version.py check
python tools/check_core_timing_guard.py
pio test -e native
python tools/check_cli_contract.py
pio run -e esp32s2dev
pio run -e esp32s3dev
pio pkg pack
```
