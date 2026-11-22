#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "Rv3032BuildIdentity.h"
#include "Rv3032Cli.h"
#include "Rv3032IdfI2c.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#if CONFIG_SPIRAM
#include "esp_psram.h"
#endif
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
#include "driver/usb_serial_jtag_select.h"
#include "driver/usb_serial_jtag_vfs.h"
#elif CONFIG_ESP_CONSOLE_USB_CDC
#include "esp_private/usb_console.h"
#include "esp_vfs_cdcacm.h"
#else
#error "Select the USB console for the target"
#endif

namespace {

bool configureConsole() {
  if (std::setvbuf(stdin, nullptr, _IONBF, 0) != 0 ||
      std::setvbuf(stdout, nullptr, _IONBF, 0) != 0) return false;
  const int inputFd = fileno(stdin);
  const int flags = fcntl(inputFd, F_GETFL, 0);
  if (flags < 0 || fcntl(inputFd, F_SETFL, flags | O_NONBLOCK) < 0) return false;

  // Preserve individual CR/LF bytes, so a terminal input snapshot counts
  // exactly the bytes that subsequent unbuffered reads will consume.
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  if (!usb_serial_jtag_is_driver_installed()) {
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    if (usb_serial_jtag_driver_install(&config) != ESP_OK) return false;
  }
  usb_serial_jtag_vfs_use_driver();
  usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
  usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_LF);
#else
  esp_vfs_dev_cdcacm_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
#endif
  return true;
}

size_t receivedBytes(void*) {
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
  return usb_serial_jtag_get_read_bytes_available();
#else
  // Match the bounded interrupt service used by the nonblocking ROM CDC VFS.
  esp_usb_console_poll_interrupts();
  const auto count = esp_usb_console_available_for_read();
  return count > 0 ? static_cast<size_t>(count) : 0;
#endif
}

int readByte(void*) {
  const int byte = getchar();
  if (byte == EOF) std::clearerr(stdin);
  return byte;
}

#if CONFIG_ESP_CONSOLE_USB_CDC
// The ROM CDC VFS reports full writes even when its endpoint accepts fewer
// bytes. Honor the actual count and bound waiting for a stalled USB host.
void writeCdc(const char* text, size_t length) {
  constexpr size_t PACKET_SIZE = 64;
  constexpr unsigned MAX_WAIT_TICKS = 25;
  for (unsigned waits = 0; length && waits < MAX_WAIT_TICKS;) {
    if (esp_usb_console_write_available()) {
      const size_t chunk = length < PACKET_SIZE ? length : PACKET_SIZE;
      const auto sent = esp_usb_console_write_buf(text, chunk);
      if (sent < 0) return;
      if (sent > 0) {
        text += sent;
        length -= static_cast<size_t>(sent);
        continue;
      }
    }
    vTaskDelay(1);
    ++waits;
  }
}
#endif

void writeText(void*, const char* text, bool endLine) {
#if CONFIG_ESP_CONSOLE_USB_CDC
  writeCdc(text, std::strlen(text));
  if (endLine) writeCdc("\r\n", 2);
#else
  std::fputs(text, stdout);
  if (endLine) std::fputs("\r\n", stdout);
#endif
}

void flushConsole() {
  std::fflush(stdout);
#if CONFIG_ESP_CONSOLE_USB_CDC
  // The ROM CDC driver buffers short writes even with unbuffered stdio.
  // Flush the unterminated prompt once the previous USB packet has drained.
  if (esp_usb_console_write_available()) (void)fsync(fileno(stdout));
#endif
}

uint32_t cliNowUs(void*) { return static_cast<uint32_t>(esp_timer_get_time()); }

void cliSleepMs(void*, uint32_t duration) {
  if (duration == 0) vTaskDelay(1);
  else rv3032Idf::waitMs(duration, nullptr);
}

rv3032cli::ScanResult probeAddress(void* user, uint8_t address, uint16_t timeoutMs) {
  const auto& ctx = *static_cast<rv3032Idf::Context*>(user);
  if (ctx.bus && ctx.device) {
    const esp_err_t result = i2c_master_probe(ctx.bus, address, timeoutMs);
    if (result == ESP_OK) return rv3032cli::ScanResult::PRESENT;
    if (result == ESP_ERR_TIMEOUT) return rv3032cli::ScanResult::TIMEOUT;
    // The scan table uses "--" for ESP_ERR_NOT_FOUND and other failed probes.
  }
  return rv3032cli::ScanResult::ABSENT;
}

void platformVersion(void*, rv3032cli::Engine& cli) {
  esp_chip_info_t chip{};
  esp_chip_info(&chip);
  const char* model = chip.model == CHIP_ESP32S3 ? "ESP32-S3" :
                      chip.model == CHIP_ESP32S2 ? "ESP32-S2" : "unknown";
  uint32_t flashBytes = 0;
  const auto flashStatus = esp_flash_get_physical_size(nullptr, &flashBytes);
  bool psramReady = false;
  size_t psramBytes = 0;
#if CONFIG_SPIRAM
  psramReady = esp_psram_is_initialized();
  if (psramReady) psramBytes = esp_psram_get_size();
#endif
  cli.print("  Example firmware build: %s\n", exampleBuild::TIMESTAMP);
  cli.print("  Example firmware commit: %s (%s)\n", exampleBuild::COMMIT, exampleBuild::STATUS);
  cli.print("  MCU: %s rev %u, ", model, static_cast<unsigned>(chip.revision));
  if (flashStatus == ESP_OK) cli.print("flash %lu bytes, ", static_cast<unsigned long>(flashBytes));
  else cli.print("flash size unavailable (%s), ", esp_err_to_name(flashStatus));
  cli.print("PSRAM %s (%lu bytes)\n", psramReady ? "ready" : "not available",
            static_cast<unsigned long>(psramBytes));
  cli.print("  ESP-IDF: %s\n", esp_get_idf_version());
}

}  // namespace

extern "C" void app_main() {
  if (!configureConsole()) {
    std::puts("Could not initialize the nonblocking USB console");
    return;
  }
  static rv3032Idf::Context transport;
  static RV3032::RV3032 rtc;
  rv3032cli::Engine cli(rtc, {&transport, writeText, readByte, receivedBytes,
                             rv3032Idf::nowMs, cliNowUs, cliSleepMs, probeAddress, platformVersion});
  cli.startBus(rv3032Idf::SDA_PIN, rv3032Idf::SCL_PIN);
  if (cli.busResult(rv3032Idf::init(transport).ok())) {
    RV3032::Config cfg{};
    cfg.i2cWrite = rv3032Idf::i2cWrite;
    cfg.i2cWriteRead = rv3032Idf::i2cWriteRead;
    cfg.i2cUser = &transport;
    cfg.nowMs = rv3032Idf::nowMs;
    cfg.waitMs = rv3032Idf::waitMs;
    cfg.enableEepromWrites = false;
    if (cli.beginResult(rtc.begin(cfg))) cli.probeResult(rtc.probe());
  }

  for (;;) {
    cli.poll();
    flushConsole();
    vTaskDelay(pdMS_TO_TICKS(10));  // 10 ticks at the adapter's required 1000 Hz.
  }
}
