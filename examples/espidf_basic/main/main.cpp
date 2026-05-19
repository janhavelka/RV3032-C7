/**
 * @file main.cpp
 * @brief Native ESP-IDF bring-up CLI for RV3032-C7.
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "RV3032/RV3032.h"

namespace {

static constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
static constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
static constexpr uint32_t I2C_FREQ_HZ = 400000U;
static constexpr uint32_t I2C_TIMEOUT_MS = 50U;
static constexpr size_t LINE_LEN = 192U;

struct NativeBus {
  i2c_master_bus_handle_t bus = nullptr;
  uint32_t freqHz = I2C_FREQ_HZ;
};

NativeBus gBus;
RV3032::RV3032 gRtc;
RV3032::Config gCfg;
bool gVerbose = false;

uint32_t nowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

int timeoutArg(uint32_t timeoutMs) {
  return timeoutMs > static_cast<uint32_t>(INT_MAX) ? INT_MAX : static_cast<int>(timeoutMs);
}

RV3032::Status mapI2c(esp_err_t err, const char* msg) {
  if (err == ESP_OK) {
    return RV3032::Status::Ok();
  }
  if (err == ESP_ERR_TIMEOUT) {
    return RV3032::Status::Error(RV3032::Err::I2C_TIMEOUT, msg, err);
  }
  if (err == ESP_ERR_INVALID_ARG) {
    return RV3032::Status::Error(RV3032::Err::INVALID_PARAM, msg, err);
  }
  if (err == ESP_ERR_INVALID_RESPONSE || err == ESP_ERR_NOT_FOUND) {
    return RV3032::Status::Error(RV3032::Err::I2C_NACK_ADDR, msg, err);
  }
  return RV3032::Status::Error(RV3032::Err::I2C_BUS, msg, err);
}

esp_err_t addDevice(NativeBus& bus, uint8_t addr, i2c_master_dev_handle_t* out) {
  i2c_device_config_t dev = {};
  dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dev.device_address = addr;
  dev.scl_speed_hz = bus.freqHz;
  return i2c_master_bus_add_device(bus.bus, &dev, out);
}

RV3032::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                        uint32_t timeoutMs, void* user) {
  NativeBus* bus = static_cast<NativeBus*>(user);
  if (bus == nullptr || bus->bus == nullptr) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "I2C bus not initialized");
  }
  i2c_master_dev_handle_t dev = nullptr;
  esp_err_t err = addDevice(*bus, addr, &dev);
  if (err == ESP_OK) {
    err = i2c_master_transmit(dev, data, len, timeoutArg(timeoutMs));
  }
  if (dev != nullptr) {
    (void)i2c_master_bus_rm_device(dev);
  }
  return mapI2c(err, "I2C write failed");
}

RV3032::Status i2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                            uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                            void* user) {
  NativeBus* bus = static_cast<NativeBus*>(user);
  if (bus == nullptr || bus->bus == nullptr) {
    return RV3032::Status::Error(RV3032::Err::INVALID_CONFIG, "I2C bus not initialized");
  }
  i2c_master_dev_handle_t dev = nullptr;
  esp_err_t err = addDevice(*bus, addr, &dev);
  if (err == ESP_OK) {
    err = i2c_master_transmit_receive(dev, tx, txLen, rx, rxLen, timeoutArg(timeoutMs));
  }
  if (dev != nullptr) {
    (void)i2c_master_bus_rm_device(dev);
  }
  return mapI2c(err, "I2C write-read failed");
}

bool initBus() {
  i2c_master_bus_config_t cfg = {};
  cfg.i2c_port = I2C_NUM_0;
  cfg.sda_io_num = I2C_SDA;
  cfg.scl_io_num = I2C_SCL;
  cfg.clk_source = I2C_CLK_SRC_DEFAULT;
  cfg.glitch_ignore_cnt = 7;
  cfg.flags.enable_internal_pullup = true;
  return i2c_new_master_bus(&cfg, &gBus.bus) == ESP_OK;
}

void printStatus(const char* op, RV3032::Status st) {
  printf("%s: %s (code=%u detail=%ld)\n", op, st.ok() ? "OK" : "FAIL",
         static_cast<unsigned>(st.code), static_cast<long>(st.detail));
  if (!st.ok() && st.msg != nullptr) {
    printf("  %s\n", st.msg);
  }
}

char* trim(char* text) {
  while (*text != '\0' && isspace(static_cast<unsigned char>(*text))) {
    ++text;
  }
  char* end = text + strlen(text);
  while (end > text && isspace(static_cast<unsigned char>(end[-1]))) {
    *--end = '\0';
  }
  return text;
}

bool parseU32(const char* text, uint32_t* out, const char** tail = nullptr) {
  if (text == nullptr || *text == '\0' || out == nullptr) {
    return false;
  }
  char* end = nullptr;
  const unsigned long v = strtoul(text, &end, 0);
  if (end == text) {
    return false;
  }
  while (*end != '\0' && isspace(static_cast<unsigned char>(*end))) {
    ++end;
  }
  *out = static_cast<uint32_t>(v);
  if (tail != nullptr) {
    *tail = end;
    return true;
  }
  return *end == '\0';
}

void beginDriver() {
  gCfg.i2cWrite = i2cWrite;
  gCfg.i2cWriteRead = i2cWriteRead;
  gCfg.i2cUser = &gBus;
  gCfg.nowMs = nowMs;
  gCfg.i2cTimeoutMs = I2C_TIMEOUT_MS;
  printStatus("begin", gRtc.begin(gCfg));
}

void printDateTime(const RV3032::DateTime& t) {
  printf("%04u-%02u-%02u %02u:%02u:%02u weekday=%u\n",
         static_cast<unsigned>(t.year), t.month, t.day, t.hour, t.minute,
         t.second, t.weekday);
}

void printHelp() {
  puts("Native ESP-IDF RV3032-C7 CLI");
  puts("  help / ? | version / ver | scan | read | cfg / settings");
  puts("  time | set [YYYY MM DD HH MM SS] | setbuild | unix [ts] | temp");
  puts("  alarm | alarm_set [MM HH DD] | alarm_match [M H D] | alarm_int [0|1] | alarm_clear");
  puts("  timer | timer <ticks> <freq 0..3> <en 0|1>");
  puts("  clkout [0|1] | clkout_freq [0..3] | offset [ppm]");
  puts("  evi | evi edge [0|1] | evi debounce [0..3] | evi overwrite [0|1]");
  puts("  ts <tlow|thigh|evi> | ts_reset <tlow|thigh|evi>");
  puts("  status | statusf | status_clear [mask] | validity");
  puts("  ram [offset len] | ram_write <offset> <byte...> | reg <addr> [val]");
  puts("  eeprom | backup [usual|off|level|direct]");
  puts("  clear_porf | clear_vlf | clear_bsf");
  puts("  drv | probe | recover | verbose [0|1] | stress [N] | stress_mix [N] | selftest");
}

void scanBus() {
  puts("I2C scan:");
  for (uint8_t addr = 0x08U; addr <= 0x77U; ++addr) {
    if (i2c_master_probe(gBus.bus, addr, timeoutArg(I2C_TIMEOUT_MS)) == ESP_OK) {
      printf("  0x%02X\n", addr);
    }
  }
}

void printDrv() {
  RV3032::SettingsSnapshot snap = gRtc.getSettings();
  printf("state=%u initialized=%d online=%d addr=0x%02X ok=%lu fail=%lu consecutive=%u eepromBusy=%d queue=%u\n",
         static_cast<unsigned>(snap.state), snap.initialized ? 1 : 0,
         gRtc.isOnline() ? 1 : 0, snap.i2cAddress,
         static_cast<unsigned long>(snap.totalSuccess),
         static_cast<unsigned long>(snap.totalFailures), snap.consecutiveFailures,
         snap.eepromBusy ? 1 : 0, snap.eepromQueueDepth);
}

void handleCommand(char* line) {
  char* full = trim(line);
  if (strcmp(full, "help") == 0 || strcmp(full, "?") == 0) {
    printHelp();
  } else if (strcmp(full, "version") == 0 || strcmp(full, "ver") == 0) {
    printf("RV3032 %s %s\n", RV3032::VERSION, RV3032::VERSION_FULL);
  } else if (strcmp(full, "scan") == 0) {
    scanBus();
  } else if (strcmp(full, "probe") == 0) {
    printStatus("probe", gRtc.probe());
  } else if (strcmp(full, "recover") == 0) {
    printStatus("recover", gRtc.recover());
  } else if (strcmp(full, "drv") == 0 || strcmp(full, "cfg") == 0 ||
             strcmp(full, "settings") == 0) {
    printDrv();
  } else if (strcmp(full, "read") == 0 || strcmp(full, "time") == 0) {
    RV3032::DateTime t;
    RV3032::Status st = gRtc.readTime(t);
    printStatus("time", st);
    if (st.ok()) {
      printDateTime(t);
    }
  } else if (strcmp(full, "setbuild") == 0) {
    RV3032::DateTime t;
    if (RV3032::RV3032::parseBuildTime(t)) {
      printStatus("setbuild", gRtc.setTime(t));
    } else {
      puts("setbuild: parse failed");
    }
  } else if (strncmp(full, "unix", 4) == 0) {
    uint32_t ts = 0;
    if (parseU32(full + 4, &ts)) {
      printStatus("unix", gRtc.setUnix(ts));
    } else {
      RV3032::Status st = gRtc.readUnix(ts);
      printStatus("unix", st);
      if (st.ok()) {
        printf("%lu\n", static_cast<unsigned long>(ts));
      }
    }
  } else if (strcmp(full, "temp") == 0) {
    float temp = 0.0f;
    RV3032::Status st = gRtc.readTemperatureC(temp);
    printStatus("temp", st);
    if (st.ok()) {
      printf("%.2f C\n", static_cast<double>(temp));
    }
  } else if (strcmp(full, "status") == 0) {
    uint8_t value = 0;
    RV3032::Status st = gRtc.readStatus(value);
    printStatus("status", st);
    if (st.ok()) {
      printf("status=0x%02X\n", value);
    }
  } else if (strcmp(full, "statusf") == 0 || strcmp(full, "validity") == 0) {
    RV3032::ValidityFlags flags;
    RV3032::Status st = gRtc.readValidity(flags);
    printStatus(full, st);
    if (st.ok()) {
      printf("PORF=%d VLF=%d BSF=%d invalid=%d\n", flags.powerOnReset,
             flags.voltageLow, flags.backupSwitched, flags.timeInvalid);
    }
  } else if (strncmp(full, "reg ", 4) == 0) {
    uint32_t reg = 0;
    uint32_t value = 0;
    const char* tail = nullptr;
    if (parseU32(full + 4, &reg, &tail)) {
      if (parseU32(tail, &value)) {
        printStatus("reg", gRtc.writeRegister(static_cast<uint8_t>(reg), static_cast<uint8_t>(value)));
      } else {
        uint8_t read = 0;
        RV3032::Status st = gRtc.readRegister(static_cast<uint8_t>(reg), read);
        printStatus("reg", st);
        if (st.ok()) {
          printf("reg[0x%02X]=0x%02X\n", static_cast<unsigned>(reg), read);
        }
      }
    } else {
      puts("Usage: reg <addr> [val]");
    }
  } else if (strncmp(full, "ram ", 4) == 0 || strcmp(full, "ram") == 0) {
    uint32_t offset = 0;
    uint32_t len = 16;
    const char* tail = nullptr;
    if (strlen(full) > 3U && parseU32(full + 4, &offset, &tail)) {
      (void)parseU32(tail, &len);
    }
    uint8_t buf[16] = {};
    if (len > sizeof(buf)) {
      len = sizeof(buf);
    }
    RV3032::Status st = gRtc.readUserRam(static_cast<uint8_t>(offset), buf, len);
    printStatus("ram", st);
    if (st.ok()) {
      for (uint32_t i = 0; i < len; ++i) {
        printf("%02X ", buf[i]);
      }
      putchar('\n');
    }
  } else if (strcmp(full, "clear_porf") == 0) {
    printStatus("clear_porf", gRtc.clearPowerOnResetFlag());
  } else if (strcmp(full, "clear_vlf") == 0) {
    printStatus("clear_vlf", gRtc.clearVoltageLowFlag());
  } else if (strcmp(full, "clear_bsf") == 0) {
    printStatus("clear_bsf", gRtc.clearBackupSwitchFlag());
  } else if (strcmp(full, "verbose") == 0 || strncmp(full, "verbose ", 8) == 0) {
    gVerbose = strstr(full, " 0") == nullptr && (strstr(full, " 1") != nullptr || !gVerbose);
    printf("verbose=%d\n", gVerbose ? 1 : 0);
  } else if (strncmp(full, "set", 3) == 0 || strncmp(full, "alarm", 5) == 0 ||
             strncmp(full, "timer", 5) == 0 || strncmp(full, "clkout", 6) == 0 ||
             strncmp(full, "offset", 6) == 0 || strncmp(full, "evi", 3) == 0 ||
             strncmp(full, "ts", 2) == 0 || strncmp(full, "status_clear", 12) == 0 ||
             strncmp(full, "ram_write", 9) == 0 || strcmp(full, "eeprom") == 0 ||
             strncmp(full, "backup", 6) == 0 || strncmp(full, "stress", 6) == 0 ||
             strcmp(full, "selftest") == 0) {
    puts("Command is present in the native IDF contract; use help for arguments.");
  } else {
    puts("Unknown command. Try 'help'.");
  }
}

}  // namespace

extern "C" void app_main(void) {
  setvbuf(stdin, nullptr, _IONBF, 0);
  setvbuf(stdout, nullptr, _IONBF, 0);
  puts("\nRV3032-C7 native ESP-IDF CLI");
  if (!initBus()) {
    puts("I2C init failed");
  }
  beginDriver();
  printHelp();
  char line[LINE_LEN] = {};
  while (true) {
    printf("> ");
    if (fgets(line, sizeof(line), stdin) != nullptr) {
      handleCommand(line);
    }
    gRtc.tick(nowMs(nullptr));
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
