#include <Arduino.h>
#include <Wire.h>

#include "examples/common/BoardConfig.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/Rv3032Cli.h"

static RV3032::RV3032 g_rtc;

static uint32_t rtc_now_ms(void*) {
  return millis();
}

static void sleepMs(void*, uint32_t durationMs) {
  if (durationMs == 0) {
    yield();
    return;
  }
  const uint32_t started = millis();
  uint32_t remaining = durationMs;
  for (unsigned attempt = 0; attempt < 4; ++attempt) {
    delay(remaining);
    const uint32_t elapsed = millis() - started;
    if (elapsed >= durationMs) break;
    remaining = durationMs - elapsed;
  }
}

static void rtc_wait_ms(uint32_t durationMs, void*) {
  // Guard the tick-relative first sleep used by primary-cell ensure.
  sleepMs(nullptr, durationMs + 1U);
}

static void platformVersion(void*, rv3032cli::Engine& engine) {
  engine.print("  Example firmware build: %s %s\n", __DATE__, __TIME__);
#if defined(ARDUINO_ARCH_ESP32)
  engine.print("  MCU: %s rev %u, flash %lu bytes, PSRAM %s (%lu bytes)\n",
               ESP.getChipModel(),
               static_cast<unsigned int>(ESP.getChipRevision()),
               static_cast<unsigned long>(ESP.getFlashChipSize()),
               psramFound() ? "ready" : "not available",
               static_cast<unsigned long>(ESP.getPsramSize()));
  engine.print("  Arduino-ESP32: %s\n", ESP.getCoreVersion());
  engine.print("  ESP-IDF: %s\n", ESP.getSdkVersion());
#endif
}

static rv3032cli::Hooks makeHooks(TwoWire& wire) {
  return {
      &wire,
      [](void*, const char* text, bool endLine) {
        if (endLine) Serial.println(text);
        else Serial.print(text);
      },
      [](void*) { return Serial.read(); },
      [](void*) -> size_t {
        const int count = Serial.available();
        return count > 0 ? static_cast<size_t>(count) : 0U;
      },
      rtc_now_ms,
      [](void*) { return static_cast<uint32_t>(micros()); },
      sleepMs,
      [](void* user, uint8_t address, uint16_t timeoutMs) {
        return i2c_scanner::probeAddress(*static_cast<TwoWire*>(user), address, timeoutMs);
      },
      platformVersion,
  };
}

static rv3032cli::Engine g_cli(g_rtc, makeHooks(Wire));

void setup() {
  delay(1000);  // Allow USB CDC enumeration before the banner.
  Serial.begin(115200);
  while (!Serial && millis() < 4000) delay(10);

  g_cli.startBus(board::I2C_SDA, board::I2C_SCL);
  if (!g_cli.busResult(board::initI2c())) return;

  RV3032::Config cfg;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = &Wire;
  cfg.nowMs = rtc_now_ms;
  cfg.waitMs = rtc_wait_ms;
  cfg.enableEepromWrites = false;

  if (g_cli.beginResult(g_rtc.begin(cfg))) {
    g_cli.probeResult(g_rtc.probe());
  }
}

void loop() {
  g_cli.poll();
  delay(10);
}
