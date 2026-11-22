#include <cstdio>

#include "RV3032/RV3032.h"
#include "Rv3032IdfI2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

rv3032Idf::Context transport;
RV3032::RV3032 rtc;

// Status codes reachable during bring-up with the example adapter.
const char* startupError(RV3032::Err code) {
  switch (code) {
    case RV3032::Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case RV3032::Err::BUSY: return "BUSY";
    case RV3032::Err::I2C_ERROR: return "I2C_ERROR";
    case RV3032::Err::I2C_TIMEOUT: return "I2C_TIMEOUT";
    default: return "UNKNOWN";
  }
}

void closeBus() {
  rtc.end();
  const auto status = rv3032Idf::deinit(transport);
  if (!status.ok()) {
    std::printf("[E] I2C cleanup failed: %s (detail=%ld)\n",
                status.msg, static_cast<long>(status.detail));
  }
}

void printTime() {
  RV3032::DateTime time{};
  const auto status = rtc.readTime(time);
  if (status.ok()) {
    std::printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                time.year, time.month, time.day, time.hour, time.minute, time.second);
  } else {
    std::printf("[E] readTime() failed: %s (detail=%ld)\n",
                status.msg, static_cast<long>(status.detail));
  }
}

}  // namespace

extern "C" void app_main() {
  std::printf("[I] Initializing I2C (SDA=%d, SCL=%d)...\n",
              rv3032Idf::SDA_PIN, rv3032Idf::SCL_PIN);
  auto status = rv3032Idf::init(transport);
  if (!status.ok()) {
    std::puts("[E] I2C init failed");
    std::printf("[E] %s (detail=%ld)\n", status.msg, static_cast<long>(status.detail));
    return;
  }

  std::puts("[I] Initializing RTC...");
  RV3032::Config cfg{};
  cfg.i2cWrite = rv3032Idf::i2cWrite;
  cfg.i2cWriteRead = rv3032Idf::i2cWriteRead;
  cfg.i2cUser = &transport;
  cfg.nowMs = rv3032Idf::nowMs;
  cfg.waitMs = rv3032Idf::waitMs;
  cfg.enableEepromWrites = false;
  status = rtc.begin(cfg);
  if (!status.ok()) {
    std::printf("[E] RTC init failed: %s (code=%s, detail=%ld)\n",
                status.msg, startupError(status.code), static_cast<long>(status.detail));
    std::puts("[E] Check I2C wiring and RTC power");
    closeBus();
    return;
  }

  std::puts("[I] RTC callbacks bound; probing address 0x51 communication explicitly...");
  status = rtc.probe();
  if (!status.ok()) {
    std::printf("[E] RTC probe failed: %s (code=%s, detail=%ld)\n",
                status.msg, startupError(status.code), static_cast<long>(status.detail));
    closeBus();
    return;
  }
  std::puts("[I] RTC address communication succeeded; chip identity, backup chemistry, and EEPROM policy were not proven or applied");
  // begin() entered READY and the diagnostic probe does not change health.
  std::puts("[I] Driver state: READY");

  uint32_t lastReadMs = rv3032Idf::nowMs(nullptr);
  printTime();
  for (;;) {
    vTaskDelay(1);
    const uint32_t now = rv3032Idf::nowMs(nullptr);
    if (now - lastReadMs >= 1000U) {
      lastReadMs = now;
      printTime();
    }
  }
}
