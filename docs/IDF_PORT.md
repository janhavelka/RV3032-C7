# RV3032-C7 ESP-IDF Portability Status

Last audited: 2026-05-30

## Current Reality
- Primary runtime remains PlatformIO + Arduino.
- I2C is callback-based (`Config.i2cWrite`, `Config.i2cWriteRead`).
- Optional timing hook is available (`Config.nowMs`, `Config.timeUser`).
- Core logic uses `_nowMs()` only for driver-owned health timestamps.
- There is no framework timing fallback in core code. When `Config.nowMs == nullptr`,
  driver-owned timestamps report `0`.
- Elapsed EEPROM work is driven by the `nowMs` argument passed to `tick(nowMs)`.

## ESP-IDF Adapter Requirements
To run under pure ESP-IDF, provide:
1. I2C write callback.
2. I2C write-read callback.
3. Optional `nowMs(user)` callback for health timestamps.

## Minimal Adapter Pattern
```cpp
static uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

RV3032::Config cfg{};
cfg.i2cWrite = myI2cWrite;
cfg.i2cWriteRead = myI2cWriteRead;
cfg.nowMs = idfNowMs;
```

## Porting Notes
- `tick(nowMs)` should be called regularly.
- Timeout checks are implemented with wrap-safe unsigned arithmetic.
- EEPROM queue behavior and offline/degraded transitions must remain unchanged in adapter code.
- The ESP-IDF adapter owns bus handles, pin configuration, locks, recovery, and timeout policy.

## Verification Checklist
- `python tools/check_core_timing_guard.py` passes.
- Native tests pass (`pio test -e native`).
- Example builds pass (`pio run -e esp32s3dev`, `pio run -e esp32s2dev`).
- No framework timing, scheduler, logging, bus, or RTOS calls are introduced in `include/` or `src/`.
