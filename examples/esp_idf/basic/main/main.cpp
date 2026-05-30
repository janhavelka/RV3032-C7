#include <cstdint>
#include <cstddef>

#include "RV3032/CommandTable.h"
#include "RV3032/RV3032.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifndef RV3032_EXAMPLE_I2C_SDA
#define RV3032_EXAMPLE_I2C_SDA GPIO_NUM_8
#endif

#ifndef RV3032_EXAMPLE_I2C_SCL
#define RV3032_EXAMPLE_I2C_SCL GPIO_NUM_9
#endif

#ifndef RV3032_EXAMPLE_I2C_PORT
#define RV3032_EXAMPLE_I2C_PORT I2C_NUM_0
#endif

namespace {

constexpr char TAG[] = "rv3032_idf";
constexpr uint32_t kI2cFrequencyHz = 100000;
constexpr uint32_t kI2cTimeoutMs = 50;

struct IdfI2cContext {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t device = nullptr;
  SemaphoreHandle_t mutex = nullptr;
};

RV3032::Status mapEspErr(esp_err_t err, const char* operation) {
  switch (err) {
    case ESP_OK:
      return RV3032::Status::Ok();
    case ESP_ERR_TIMEOUT:
      return RV3032::Status::Error(RV3032::Err::I2C_TIMEOUT, operation, err);
    case ESP_ERR_INVALID_ARG:
      return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, operation, err);
    case ESP_FAIL:
      return RV3032::Status::Error(RV3032::Err::I2C_ERROR, operation, err);
    default:
      return RV3032::Status::Error(RV3032::Err::I2C_ERROR, operation, err);
  }
}

TickType_t timeoutTicks(uint32_t timeoutMs) {
  TickType_t ticks = pdMS_TO_TICKS(timeoutMs);
  return ticks == 0 ? 1 : ticks;
}

RV3032::Status lockBus(IdfI2cContext* ctx, uint32_t timeoutMs) {
  if (ctx == nullptr || ctx->mutex == nullptr || ctx->device == nullptr) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "I2C context is incomplete");
  }
  if (xSemaphoreTake(ctx->mutex, timeoutTicks(timeoutMs)) != pdTRUE) {
    return RV3032::Status::Error(RV3032::Err::I2C_TIMEOUT, "I2C mutex timeout");
  }
  return RV3032::Status::Ok();
}

void unlockBus(IdfI2cContext* ctx) {
  if (ctx != nullptr && ctx->mutex != nullptr) {
    xSemaphoreGive(ctx->mutex);
  }
}

RV3032::Status idfI2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                           uint32_t timeoutMs, void* user) {
  if (addr != RV3032::cmd::I2C_ADDR_7BIT) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "Unexpected I2C address");
  }
  if (data == nullptr || len == 0) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "Invalid I2C write buffer");
  }

  auto* ctx = static_cast<IdfI2cContext*>(user);
  RV3032::Status st = lockBus(ctx, timeoutMs);
  if (!st.ok()) {
    return st;
  }

  esp_err_t err = i2c_master_transmit(ctx->device, data, len, static_cast<int>(timeoutMs));
  unlockBus(ctx);
  return mapEspErr(err, "I2C write failed");
}

RV3032::Status idfI2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                               uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                               void* user) {
  if (addr != RV3032::cmd::I2C_ADDR_7BIT) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "Unexpected I2C address");
  }
  if (tx == nullptr || txLen == 0 || rx == nullptr || rxLen == 0) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, "Invalid I2C write-read buffers");
  }

  auto* ctx = static_cast<IdfI2cContext*>(user);
  RV3032::Status st = lockBus(ctx, timeoutMs);
  if (!st.ok()) {
    return st;
  }

  esp_err_t err = i2c_master_transmit_receive(ctx->device,
                                              tx,
                                              txLen,
                                              rx,
                                              rxLen,
                                              static_cast<int>(timeoutMs));
  unlockBus(ctx);
  return mapEspErr(err, "I2C write-read failed");
}

uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

esp_err_t initI2c(IdfI2cContext& ctx) {
  ctx.mutex = xSemaphoreCreateMutex();
  ESP_RETURN_ON_FALSE(ctx.mutex != nullptr, ESP_ERR_NO_MEM, TAG, "failed to create I2C mutex");

  i2c_master_bus_config_t busConfig = {};
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.i2c_port = RV3032_EXAMPLE_I2C_PORT;
  busConfig.sda_io_num = RV3032_EXAMPLE_I2C_SDA;
  busConfig.scl_io_num = RV3032_EXAMPLE_I2C_SCL;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;
  ESP_RETURN_ON_ERROR(i2c_new_master_bus(&busConfig, &ctx.bus), TAG, "i2c_new_master_bus failed");

  i2c_device_config_t deviceConfig = {};
  deviceConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  deviceConfig.device_address = RV3032::cmd::I2C_ADDR_7BIT;
  deviceConfig.scl_speed_hz = kI2cFrequencyHz;
  ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(ctx.bus, &deviceConfig, &ctx.device),
                      TAG,
                      "i2c_master_bus_add_device failed");
  return ESP_OK;
}

void logStatus(const char* operation, const RV3032::Status& st) {
  if (st.ok()) {
    ESP_LOGI(TAG, "%s: OK", operation);
  } else {
    ESP_LOGE(TAG,
             "%s: code=%u detail=%ld msg=%s",
             operation,
             static_cast<unsigned>(st.code),
             static_cast<long>(st.detail),
             st.msg ? st.msg : "");
  }
}

void logDateTime(const RV3032::DateTime& dt) {
  ESP_LOGI(TAG,
           "time=%04u-%02u-%02u %02u:%02u:%02u weekday=%u",
           static_cast<unsigned>(dt.year),
           static_cast<unsigned>(dt.month),
           static_cast<unsigned>(dt.day),
           static_cast<unsigned>(dt.hour),
           static_cast<unsigned>(dt.minute),
           static_cast<unsigned>(dt.second),
           static_cast<unsigned>(dt.weekday));
}

}  // namespace

extern "C" void app_main(void) {
  static IdfI2cContext i2c;
  ESP_ERROR_CHECK(initI2c(i2c));

  static RV3032::RV3032 rtc;
  RV3032::Config cfg;
  cfg.i2cWrite = idfI2cWrite;
  cfg.i2cWriteRead = idfI2cWriteRead;
  cfg.i2cUser = &i2c;
  cfg.nowMs = idfNowMs;
  cfg.i2cTimeoutMs = kI2cTimeoutMs;
  cfg.enableEepromWrites = false;

  RV3032::Status st = rtc.begin(cfg);
  logStatus("begin", st);
  if (!st.ok()) {
    rtc.end();
    return;
  }

  st = rtc.probe();
  logStatus("probe", st);

  while (true) {
    const uint32_t nowMs = idfNowMs(nullptr);
    rtc.tick(nowMs);

    RV3032::ValidityFlags validity;
    st = rtc.readValidity(validity);
    if (st.ok()) {
      ESP_LOGI(TAG,
               "validity: PORF=%d VLF=%d BSF=%d EEF=%d EEbusy=%d CLKF=%d timeInvalid=%d",
               validity.powerOnReset,
               validity.voltageLow,
               validity.backupSwitched,
               validity.eepromError,
               validity.eepromBusy,
               validity.clockFlag,
               validity.timeInvalid);
    } else {
      logStatus("readValidity", st);
    }

    RV3032::StatusFlags flags;
    st = rtc.readStatusFlags(flags);
    if (st.ok()) {
      ESP_LOGI(TAG,
               "flags: AF=%d TF=%d UF=%d EVF=%d PORF=%d VLF=%d EEF=%d",
               flags.alarm,
               flags.timer,
               flags.update,
               flags.event,
               flags.powerOnReset,
               flags.voltageLow,
               flags.eepromError);
    } else {
      logStatus("readStatusFlags", st);
    }

    RV3032::DateTime now;
    st = rtc.readTime(now);
    if (st.ok()) {
      logDateTime(now);
    } else {
      logStatus("readTime", st);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
