#include "FakeIdf.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/task.h"

using fakeIdf::Call;
using fakeIdf::sdk;
fakeIdf::State fakeIdf::sdk;

struct i2c_master_bus_t {};
struct i2c_master_dev_t {};
static i2c_master_bus_t bus;
static i2c_master_dev_t device;

int64_t esp_timer_get_time() {
  ++sdk.clockReads;
  const int64_t result = sdk.nowUs;
  sdk.nowUs += sdk.advanceAfterNextClockReadUs;
  sdk.advanceAfterNextClockReadUs = 0;
  return result;
}

void esp_rom_delay_us(uint32_t us) { sdk.nowUs += us; }

void vTaskDelay(TickType_t ticks) {
  sdk.record(Call::DELAY);
  sdk.taskDelays.push_back(ticks);
  if (sdk.earlyNextWake) {
    sdk.earlyNextWake = false;
    return;
  }
  // A relative delay can start just before the next tick.
  sdk.nowUs = (sdk.nowUs / 1000 + ticks) * 1000;
}

esp_err_t gpio_config(const gpio_config_t* config) {
  sdk.pins = *config;
  return sdk.record(Call::GPIO_CONFIG);
}

esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level) {
  const esp_err_t err = sdk.record(Call::GPIO_SET);
  if (err == ESP_OK) {
    sdk.levels.emplace_back(pin, level);
    (pin == GPIO_NUM_8 ? sdk.sdaHigh : sdk.sclHigh) = level != 0;
  }
  return err;
}

int gpio_get_level(gpio_num_t pin) {
  sdk.record(Call::GPIO_GET);
  if (pin == GPIO_NUM_8) return sdk.sdaHigh && !sdk.holdSdaLow;
  return sdk.sclHigh && !sdk.holdSclLow;
}

esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t* config,
                             i2c_master_bus_handle_t* out) {
  const esp_err_t err = sdk.record(Call::NEW_BUS);
  if (err != ESP_OK) return err;
  if (sdk.busLive) return ESP_ERR_INVALID_STATE;
  sdk.busConfig = *config;
  sdk.busLive = true;
  *out = &bus;
  return ESP_OK;
}

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t handle,
                                    const i2c_device_config_t* config,
                                    i2c_master_dev_handle_t* out) {
  const esp_err_t err = sdk.record(Call::ADD_DEVICE);
  if (err != ESP_OK) return err;
  if (handle != &bus || !sdk.busLive || sdk.deviceLive) return ESP_ERR_INVALID_STATE;
  sdk.deviceConfig = *config;
  sdk.deviceLive = true;
  *out = &device;
  return ESP_OK;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t handle) {
  const esp_err_t err = sdk.record(Call::REMOVE_DEVICE);
  if (err != ESP_OK) return err;
  if (handle != &device || !sdk.deviceLive) return ESP_ERR_INVALID_STATE;
  sdk.deviceLive = false;
  return ESP_OK;
}

esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t handle) {
  const esp_err_t err = sdk.record(Call::DELETE_BUS);
  if (err != ESP_OK) return err;
  if (handle != &bus || !sdk.busLive || sdk.deviceLive) return ESP_ERR_INVALID_STATE;
  sdk.busLive = false;
  return ESP_OK;
}

esp_err_t i2c_master_bus_reset(i2c_master_bus_handle_t) {
  return sdk.record(Call::RESET_BUS);
}

static esp_err_t startTransfer(Call call, i2c_master_dev_handle_t handle,
                               int timeoutMs) {
  const esp_err_t err = sdk.record(call);
  sdk.transferTimeouts.push_back(timeoutMs);
  sdk.nowUs += sdk.transferUs;
  sdk.rtc.nowMs = static_cast<uint32_t>(sdk.nowUs / 1000);
  if (handle != &device || !sdk.deviceLive) return ESP_ERR_INVALID_STATE;
  return err;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t handle, const uint8_t* data,
                              size_t len, int timeoutMs) {
  const esp_err_t err = startTransfer(Call::TRANSMIT, handle, timeoutMs);
  if (err != ESP_OK) return err;
  const auto result = sdk.rtc.writeCallback(sdk.deviceConfig.device_address,
                                            data, len, timeoutMs, &sdk.rtc);
  return result.ok() ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t handle,
                                      const uint8_t* tx, size_t txLen,
                                      uint8_t* rx, size_t rxLen, int timeoutMs) {
  const esp_err_t err = startTransfer(Call::TRANSMIT_RECEIVE, handle, timeoutMs);
  if (err != ESP_OK) return err;
  const auto result = sdk.rtc.readCallback(sdk.deviceConfig.device_address, tx,
                                           txLen, rx, rxLen, timeoutMs, &sdk.rtc);
  return result.ok() ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}
