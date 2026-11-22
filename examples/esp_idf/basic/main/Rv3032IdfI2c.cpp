#include "Rv3032IdfI2c.h"

#include <unistd.h>

#include "RV3032/CommandTable.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace rv3032Idf {
namespace {

using RV3032::Err;
using RV3032::Status;

static_assert(configTICK_RATE_HZ == 1000, "I2C timeouts require a 1000 Hz tick");
constexpr uint32_t BUS_CLEAR_TIMEOUT_MS = 50;
constexpr uint32_t MAX_TRANSFER_TIMEOUT_MS = 100;

Status sdkStatus(esp_err_t error, const char* message) {
  if (error == ESP_OK) return Status::Ok();
  // INVALID_RESPONSE can mean either a NACK or a controller timeout.
  return Status::Error(error == ESP_ERR_TIMEOUT ? Err::I2C_TIMEOUT : Err::I2C_ERROR,
                       message, error);
}

esp_err_t releaseClock(int64_t startedUs) {
  const esp_err_t error = gpio_set_level(SCL_PIN, 1);
  if (error != ESP_OK) return error;
  for (uint32_t i = 0; i <= BUS_CLEAR_TIMEOUT_MS; ++i) {
    if (esp_timer_get_time() - startedUs >= BUS_CLEAR_TIMEOUT_MS * 1000) break;
    if (gpio_get_level(SCL_PIN)) return ESP_OK;
    vTaskDelay(1);
  }
  return ESP_ERR_TIMEOUT;
}

esp_err_t clearBus() {
  const int64_t startedUs = esp_timer_get_time();
  // Preload released levels before enabling open-drain outputs. Both lines
  // require external pull-ups suitable for the 400 kHz bus.
  esp_err_t error = gpio_set_level(SDA_PIN, 1);
  if (error != ESP_OK) return error;
  error = gpio_set_level(SCL_PIN, 1);
  if (error != ESP_OK) return error;
  gpio_config_t pins{};
  pins.pin_bit_mask = (1ULL << SDA_PIN) | (1ULL << SCL_PIN);
  pins.mode = GPIO_MODE_INPUT_OUTPUT_OD;
  error = gpio_config(&pins);
  if (error != ESP_OK) return error;

  error = releaseClock(startedUs);
  for (unsigned pulse = 0; pulse < 9 && error == ESP_OK; ++pulse) {
    error = gpio_set_level(SCL_PIN, 0);
    if (error != ESP_OK) break;
    esp_rom_delay_us(5);
    error = releaseClock(startedUs);
    esp_rom_delay_us(5);
  }
  // Generate STOP after the nine clocks.
  if (error == ESP_OK) error = gpio_set_level(SCL_PIN, 0);
  if (error == ESP_OK) error = gpio_set_level(SDA_PIN, 0);
  if (error == ESP_OK) {
    esp_rom_delay_us(5);
    error = releaseClock(startedUs);
    esp_rom_delay_us(5);
  }
  // Leave both outputs released, including on a GPIO or stretch failure.
  if (error != ESP_OK) {
    const esp_err_t releaseError = gpio_set_level(SCL_PIN, 1);
    if (releaseError != ESP_OK) error = releaseError;
  }
  const esp_err_t releaseError = gpio_set_level(SDA_PIN, 1);
  if (releaseError != ESP_OK) return releaseError;
  if (error != ESP_OK) return error;
  esp_rom_delay_us(5);
  if (esp_timer_get_time() - startedUs >= BUS_CLEAR_TIMEOUT_MS * 1000) {
    return ESP_ERR_TIMEOUT;
  }
  if (!gpio_get_level(SDA_PIN) || !gpio_get_level(SCL_PIN)) {
    return ESP_ERR_INVALID_STATE;
  }
  return ESP_OK;
}

Status invalidTransfer() {
  return Status::Error(Err::I2C_ERROR, "Invalid I2C callback argument",
                       ESP_ERR_INVALID_ARG);
}

Status transfer(uint8_t addr, const uint8_t* tx, size_t txLen, uint8_t* rx,
                size_t rxLen, uint32_t timeoutMs, void* user) {
  const auto* ctx = static_cast<const Context*>(user);
  if (!ctx || !ctx->bus || !ctx->device || addr != RV3032::cmd::I2C_ADDR_7BIT ||
      !tx || txLen == 0 || timeoutMs == 0 || timeoutMs > MAX_TRANSFER_TIMEOUT_MS) {
    return invalidTransfer();
  }

  const int64_t deadlineUs = esp_timer_get_time() + timeoutMs * 1000;
  const int64_t remainingUs = deadlineUs - esp_timer_get_time();
  if (remainingUs <= 1000) {
    return Status::Error(Err::I2C_TIMEOUT, "Insufficient I2C callback time");
  }
  // Largest integral timeout strictly inside the exclusive deadline.
  const int sdkTimeoutMs = static_cast<int>((remainingUs - 1) / 1000);
  const esp_err_t result =
      rx ? i2c_master_transmit_receive(ctx->device, tx, txLen, rx, rxLen, sdkTimeoutMs)
         : i2c_master_transmit(ctx->device, tx, txLen, sdkTimeoutMs);
  if (esp_timer_get_time() >= deadlineUs) {
    return Status::Error(Err::I2C_TIMEOUT, "I2C callback deadline reached", result);
  }
  return sdkStatus(result, "I2C transfer failed");
}

}  // namespace

Status init(Context& ctx) {
  const Status released = deinit(ctx);
  if (!released.ok()) return released;
  const esp_err_t clearError = clearBus();
  if (clearError != ESP_OK) return sdkStatus(clearError, "I2C bus clear failed");

  i2c_master_bus_config_t bus{};
  bus.i2c_port = I2C_NUM_0;
  bus.sda_io_num = SDA_PIN;
  bus.scl_io_num = SCL_PIN;
  bus.clk_source = I2C_CLK_SRC_DEFAULT;
  bus.glitch_ignore_cnt = 7;
  bus.trans_queue_depth = 0;
  esp_err_t error = i2c_new_master_bus(&bus, &ctx.bus);
  if (error != ESP_OK) return sdkStatus(error, "I2C bus creation failed");

  i2c_device_config_t device{};
  device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  device.device_address = RV3032::cmd::I2C_ADDR_7BIT;
  device.scl_speed_hz = 400000;
  error = i2c_master_bus_add_device(ctx.bus, &device, &ctx.device);
  if (error == ESP_OK) return Status::Ok();
  const Status cleanup = deinit(ctx);
  return cleanup.ok() ? sdkStatus(error, "I2C device creation failed") : cleanup;
}

Status deinit(Context& ctx) {
  if (ctx.device) {
    const esp_err_t error = i2c_master_bus_rm_device(ctx.device);
    if (error != ESP_OK) return sdkStatus(error, "I2C device removal failed");
    ctx.device = nullptr;
  }
  if (ctx.bus) {
    const esp_err_t error = i2c_del_master_bus(ctx.bus);
    if (error != ESP_OK) return sdkStatus(error, "I2C bus deletion failed");
    ctx.bus = nullptr;
  }
  return Status::Ok();
}

Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                uint32_t timeoutMs, void* user) {
  return transfer(addr, data, len, nullptr, 0, timeoutMs, user);
}

Status i2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                    uint8_t* rx, size_t rxLen, uint32_t timeoutMs, void* user) {
  if (!rx || rxLen == 0) return invalidTransfer();
  return transfer(addr, tx, txLen, rx, rxLen, timeoutMs, user);
}

uint32_t nowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void waitMs(uint32_t delayMs, void*) {
  // Keep microseconds representable by usleep(). Driver waits fit one chunk.
  constexpr uint32_t MAX_CHUNK_MS = UINT32_MAX / 1000;
  while (delayMs > 0) {
    const uint32_t chunkMs = delayMs < MAX_CHUNK_MS ? delayMs : MAX_CHUNK_MS;
    const int64_t startedUs = esp_timer_get_time();
    vTaskDelay(static_cast<TickType_t>(chunkMs + 1));  // Guard the entry tick.
    const int64_t remainingUs = static_cast<int64_t>(chunkMs) * 1000 -
                                (esp_timer_get_time() - startedUs);
    if (remainingUs > 0) {
      // IDF usleep rechecks its monotonic deadline after each early wake.
      // At least one whole tick selects its yielding path.
      usleep(static_cast<uint32_t>(remainingUs < 1000 ? 1000 : remainingUs));
    }
    delayMs -= chunkMs;
  }
}

}  // namespace rv3032Idf
