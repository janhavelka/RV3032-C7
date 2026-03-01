# RV3032-C7 ESP-IDF Portability Status

Last audited: 2026-03-01

## Current Reality
- Primary runtime remains PlatformIO + Arduino.
- I2C is callback-based (`Config.i2cWrite`, `Config.i2cWriteRead`).
- Optional timing hook is available (`Config.nowMs`, `Config.timeUser`).
- Core logic uses `_nowMs()` wrapper for health and timeout timing.
- Arduino timing is used only as fallback in one place:
  - `RV3032::_nowMs()` -> `millis()` when `Config.nowMs == nullptr`

## ESP-IDF Adapter Requirements
To run under pure ESP-IDF, provide:
1. I2C write callback.
2. I2C write-read callback.
3. Optional `nowMs(user)` callback.

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

## Verification Checklist
- `python tools/check_core_timing_guard.py` passes.
- Native tests pass (`pio test -e native`).
- Example build passes (`pio run -e ex_bringup_s3`).
- No direct Arduino timing calls are introduced outside wrapper fallback.
