# RV3032-C7 -- ESP-IDF Migration Prompt

> **Library**: RV3032-C7 (Micro Crystal RTC with temperature compensation)
> **Current version**: 1.2.1 -> **Target**: 2.0.0
> **Namespace**: `RV3032`
> **Include path**: `#include "RV3032/RV3032.h"`
> **Difficulty**: Easy -- 3x `millis()` replacement in .cpp only, I2C already callback-based

---

## Pre-Migration

```bash
git tag v1.2.1   # freeze Arduino-era version
```

---

## Current State -- Arduino Dependencies (exact)

| API | Count | Locations |
|-----|-------|-----------|
| `#include <Arduino.h>` | 1 | `.cpp` only (not in header) |
| `millis()` | 3 | Lines 233, 252, 1017 |

Note: Uses C-style headers (`<string.h>`, `<stdint.h>`) rather than C++ `<cstring>`.

I2C already abstracted via injected callbacks. Config is struct-based, time via `tick(uint32_t nowMs)`.

Features: BackupSwitchMode, EEPROM write queue, alarm/timer configuration.

---

## Steps

### 1. Remove `#include <Arduino.h>`

### 2. Add timing helper at file scope in .cpp

```cpp
#include "esp_timer.h"

static inline uint32_t nowMs() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
```

### 3. Replace all 3x `millis()` -> `nowMs()`

At lines 233, 252, 1017.

### 4. Optionally modernize C headers

Replace `<string.h>` -> `<cstring>`, `<stdint.h>` -> `<cstdint>`, etc. This is cosmetic but keeps things consistent.

### 5. Add `CMakeLists.txt` (library root)

```cmake
idf_component_register(
    SRCS "src/RV3032.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
)
```

### 6. Add `idf_component.yml` (library root)

```yaml
version: "2.0.0"
description: "RV3032-C7 RTC driver with EEPROM and alarm support"
targets:
  - esp32s2
  - esp32s3
dependencies:
  idf: ">=5.0"
```

### 7. Version bump

- `library.json` -> `2.0.0`
- `Version.h` (if present) -> `2.0.0`

### 8. Replace Arduino example with ESP-IDF example

Create `examples/espidf_basic/main/main.cpp`:

```cpp
#include <cstdio>
#include "RV3032/RV3032.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t dev;

static RV3032::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len, void*) {
    esp_err_t err = i2c_master_transmit(dev, data, len, 100);
    return err == ESP_OK ? RV3032::Status{RV3032::Err::Ok}
                         : RV3032::Status{RV3032::Err::I2cNack, "transmit failed"};
}

static RV3032::Status i2cWriteRead(uint8_t addr,
                                    const uint8_t* wdata, size_t wlen,
                                    uint8_t* rdata, size_t rlen, void*) {
    esp_err_t err = i2c_master_transmit_receive(dev, wdata, wlen, rdata, rlen, 100);
    return err == ESP_OK ? RV3032::Status{RV3032::Err::Ok}
                         : RV3032::Status{RV3032::Err::I2cNack, "xfer failed"};
}

extern "C" void app_main() {
    i2c_master_bus_config_t busCfg{};
    busCfg.i2c_port = I2C_NUM_0;
    busCfg.sda_io_num = GPIO_NUM_8;
    busCfg.scl_io_num = GPIO_NUM_9;
    busCfg.clk_source = I2C_CLK_SRC_DEFAULT;
    busCfg.glitch_ignore_cnt = 7;
    busCfg.flags.enable_internal_pullup = true;
    i2c_new_master_bus(&busCfg, &bus);

    i2c_device_config_t devCfg{};
    devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devCfg.device_address = 0x51;
    devCfg.scl_speed_hz = 400000;
    i2c_master_bus_add_device(bus, &devCfg, &dev);

    RV3032::Config cfg{};
    cfg.i2cAddr = 0x51;
    cfg.i2cWrite = i2cWrite;
    cfg.i2cWriteRead = i2cWriteRead;

    RV3032::Rtc rtc;
    auto st = rtc.begin(cfg);
    if (st.err != RV3032::Err::Ok) {
        printf("begin() failed: %s\n", st.msg);
    }

    while (true) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        rtc.tick(now);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

Create `examples/espidf_basic/main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "." REQUIRES driver esp_timer)
```

Create `examples/espidf_basic/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "../..")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(rv3032_espidf_basic)
```

---

## Verification

```bash
cd examples/espidf_basic && idf.py set-target esp32s2 && idf.py build
```

- [ ] `idf.py build` succeeds
- [ ] Zero `#include <Arduino.h>` anywhere
- [ ] Zero `millis()` calls remaining
- [ ] Version bumped to 2.0.0
- [ ] `git tag v2.0.0`
