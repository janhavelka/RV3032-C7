#pragma once

#include "RV3032/Config.h"
#include "driver/i2c_master.h"

namespace rv3032Idf {

inline constexpr gpio_num_t SDA_PIN = GPIO_NUM_8;
inline constexpr gpio_num_t SCL_PIN = GPIO_NUM_9;

// Owned by the one application task that initializes, uses and releases I2C.
struct Context {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t device = nullptr;
  Context() = default;
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
};

RV3032::Status init(Context& ctx);
RV3032::Status deinit(Context& ctx);
RV3032::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                        uint32_t timeoutMs, void* user);
RV3032::Status i2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                            uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                            void* user);
uint32_t nowMs(void* user);
void waitMs(uint32_t delayMs, void* user);

}  // namespace rv3032Idf
