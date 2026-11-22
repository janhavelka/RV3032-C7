#include <string>
#include <vector>

#include <unity.h>
#include "HostConsole.h"

namespace {
std::string input;
std::string output;
size_t cursor = 0;
bool injectDuringRead = false;
std::vector<uint16_t> probeAddresses;
std::vector<int> probeTimeouts;

int scriptedGetchar() {
  return cursor == input.size() ? EOF : static_cast<unsigned char>(input[cursor++]);
}

void captureText(void*, const char* text, bool endLine) {
  output += text;
  if (endLine) output += '\n';
}

void resetConsole() {
  input.clear();
  cursor = 0;
  output.clear();
  injectDuringRead = false;
  probeAddresses.clear();
  probeTimeouts.clear();
}
}  // namespace

size_t usb_serial_jtag_get_read_bytes_available() { return input.size() - cursor; }

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus, uint16_t address, int timeoutMs) {
  if (!bus) return ESP_ERR_INVALID_STATE;
  probeAddresses.push_back(address);
  probeTimeouts.push_back(timeoutMs);
  if (address == 0x08) return ESP_OK;
  if (address == 0x09) return ESP_ERR_TIMEOUT;
  return ESP_ERR_NOT_FOUND;
}

// Exercise the shipped shell's stdio reader and SDK hooks with scripted input.
#define getchar scriptedGetchar
#include "examples/esp_idf/basic/main/main.cpp"
#undef getchar

namespace {
using fakeIdf::Call;
using fakeIdf::sdk;
using rv3032cli::PendingSurface;

size_t transfers() {
  return sdk.count(Call::TRANSMIT) + sdk.count(Call::TRANSMIT_RECEIVE);
}

struct ConsoleFixture {
  rv3032Idf::Context bus;
  RV3032::RV3032 rtc;
  rv3032cli::Engine cli{rtc, {&bus, captureText, readByte, receivedBytes,
                              rv3032Idf::nowMs, cliNowUs, cliSleepMs, probeAddress, platformVersion}};

  void begin(bool eeprom) {
    resetConsole();
    TEST_ASSERT_TRUE(rv3032Idf::init(bus).ok());
    RV3032::Config cfg{};
    cfg.i2cUser = &bus;
    cfg.i2cWrite = rv3032Idf::i2cWrite;
    cfg.i2cWriteRead = [](uint8_t addr, const uint8_t* tx, size_t txLen,
                          uint8_t* rx, size_t rxLen, uint32_t timeoutMs, void* context) {
      const auto status = rv3032Idf::i2cWriteRead(addr, tx, txLen, rx, rxLen, timeoutMs, context);
      if (injectDuringRead) {
        input += "verbose 1\nclear_porf";
        injectDuringRead = false;
      }
      return status;
    };
    cfg.nowMs = rv3032Idf::nowMs;
    cfg.waitMs = rv3032Idf::waitMs;
    cfg.enableEepromWrites = eeprom;
    TEST_ASSERT_TRUE(rtc.begin(cfg).ok());
  }
};

void test_idf_terminal_input_and_busy_backlog() {
  for (bool genericEeprom : {false, true}) {
    for (size_t backlog : {size_t{0}, 2 * rv3032cli::MAX_INPUT_BYTES_PER_POLL}) {
      sdk = fakeIdf::State{};
      ConsoleFixture fixture;
      fixture.begin(genericEeprom);
      input += genericEeprom ? "offset 1.0\n" : "timer 50 2 1\n";
      fixture.cli.poll();
      if (genericEeprom) {
        for (unsigned i = 0; i < 100 && fixture.cli.pending().surface == PendingSurface::ORDINARY_JOB; ++i) {
          fixture.cli.poll();
          sdk.nowUs += 1000;
        }
        TEST_ASSERT_TRUE(fixture.cli.pending().surface == PendingSurface::EEPROM);
        sdk.errors[Call::TRANSMIT_RECEIVE] = ESP_ERR_TIMEOUT;
      } else {
        for (unsigned i = 0; i < 5; ++i) fixture.cli.poll();
        TEST_ASSERT_TRUE(fixture.cli.pending().surface == PendingSurface::ORDINARY_JOB);
      }
      // Complete the operation with both old queued input and new bytes that
      // arrive inside its final transfer, including a partial mutating command.
      injectDuringRead = true;
      input.append(backlog, '\n');
      const size_t before = transfers();
      fixture.cli.poll();
      TEST_ASSERT_FALSE(injectDuringRead);
      TEST_ASSERT_TRUE(fixture.cli.pending().surface == PendingSurface::NONE);
      TEST_ASSERT_EQUAL_UINT(before + 1, transfers());
      TEST_ASSERT_EQUAL_UINT((backlog ? backlog - rv3032cli::MAX_INPUT_BYTES_PER_POLL : 0) +
                             sizeof("verbose 1\nclear_porf") - 1, receivedBytes(nullptr));

      // Fresh input is already queued before the old backlog finishes draining.
      // Only the fresh line can produce output; the old complete line and the
      // partial command must never execute.
      output.clear();
      input += "\nverbose 0\n";
      for (unsigned pass = 0; pass < 4 && receivedBytes(nullptr); ++pass) {
        const size_t consumedBefore = cursor;
        fixture.cli.poll();
        TEST_ASSERT_TRUE(cursor - consumedBefore <= rv3032cli::MAX_INPUT_BYTES_PER_POLL);
      }
      TEST_ASSERT_EQUAL_UINT(0, receivedBytes(nullptr));
      TEST_ASSERT_EQUAL_STRING("\033[36m[I]\033[0m Verbose mode: \033[0mOFF\033[0m\n> ", output.c_str());
      TEST_ASSERT_EQUAL_UINT(before + 1, transfers());
      TEST_ASSERT_TRUE(rv3032Idf::deinit(fixture.bus).ok());
    }
  }
}

void test_idf_scan_mapping_and_archive_version() {
  ConsoleFixture fixture;
  fixture.begin(false);
  TEST_ASSERT_EQUAL_INT(EOF, readByte(nullptr));
  input += "scan\nver\n";
  fixture.cli.poll();
  TEST_ASSERT_TRUE(output.find("08 TO --") != std::string::npos);
  TEST_ASSERT_TRUE(output.find("Scan complete. Found 1 device(s).") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT(112, probeAddresses.size());
  for (size_t i = 0; i < probeAddresses.size(); ++i) {
    TEST_ASSERT_EQUAL_UINT16(0x08 + i, probeAddresses[i]);
    TEST_ASSERT_EQUAL_INT(50, probeTimeouts[i]);
  }
  output.clear();
  fixture.cli.poll();
  TEST_ASSERT_TRUE(output.find("Example firmware commit: unknown (unknown)\n") != std::string::npos);
  TEST_ASSERT_TRUE(output.find("MCU: ESP32-S3 rev 101, flash 8388608 bytes, PSRAM not available (0 bytes)\n") != std::string::npos);
  TEST_ASSERT_TRUE(output.find("ESP-IDF: v6.0.1 (host test)\n") != std::string::npos);
  TEST_ASSERT_TRUE(output.find("RV3032 library commit: " RV3032_GIT_COMMIT " (" RV3032_GIT_STATUS ")\n") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT(0, transfers());
  TEST_ASSERT_TRUE(rv3032Idf::deinit(fixture.bus).ok());
}

void test_idf_primary_cell_cli_uses_single_attempt_adapter() {
  ConsoleFixture fixture;
  fixture.begin(false);
  sdk.rtc.persistent[0] = 0;
  sdk.rtc.resetFromPersistent();
  input += "primary-cell ensure\n";
  fixture.cli.poll();
  TEST_ASSERT_EQUAL_UINT(0, transfers());
  input += "primary-cell ensure CONFIRM-PRIMARY-CELL\n";
  fixture.cli.poll();
  TEST_ASSERT_TRUE(output.find("durable=yes") != std::string::npos);
  TEST_ASSERT_EQUAL_UINT16(1, sdk.rtc.writeOneAttempts);
  TEST_ASSERT_EQUAL_UINT(transfers(), sdk.rtc.physicalAttemptCount);
  TEST_ASSERT_EQUAL_UINT(0, sdk.count(Call::RESET_BUS));
  TEST_ASSERT_TRUE(rv3032Idf::deinit(fixture.bus).ok());
}
}  // namespace

void runIdfCliTests() {
  RUN_TEST(test_idf_terminal_input_and_busy_backlog);
  RUN_TEST(test_idf_scan_mapping_and_archive_version);
  RUN_TEST(test_idf_primary_cell_cli_uses_single_attempt_adapter);
}
