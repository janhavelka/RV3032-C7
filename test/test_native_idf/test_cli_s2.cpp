#include <algorithm>
#include <string>

#include <unity.h>
#include "HostConsole.h"

namespace {
ssize_t rxBytes = 0;
unsigned interruptPolls = 0;
bool endpointReady = true;
unsigned endpointChecks = 0;
uint8_t checkedEndpoint = 0;
size_t acceptedPerWrite = 7;
size_t largestWrite = 0;
std::string pending;
std::string published;
unsigned syncCalls = 0;
int syncedFd = -1;

int syncEndpoint(int fd) {
  ++syncCalls;
  syncedFd = fd;
  published += pending;
  pending.clear();
  return 0;
}
}  // namespace

void esp_usb_console_poll_interrupts() { ++interruptPolls; }
ssize_t esp_usb_console_available_for_read() { return rxBytes; }
// The ROM's TX IRQ indication can already have been consumed by the SDK ISR.
bool esp_usb_console_write_available() { return false; }
int usb_write_would_block(uint8_t endpoint) {
  ++endpointChecks;
  checkedEndpoint = endpoint;
  return endpointReady ? 0 : 1;
}
ssize_t esp_usb_console_write_buf(const char* text, size_t len) {
  largestWrite = std::max(largestWrite, len);
  const size_t accepted = std::min(len, acceptedPerWrite);
  pending.append(text, accepted);
  return static_cast<ssize_t>(accepted);
}

// Compile the shipped shell for S2's alternate SDK configuration.
#define CONFIG_ESP_CONSOLE_USB_CDC 1
#define app_main s2AppMain
#define fsync syncEndpoint
#include "examples/esp_idf/basic/main/main.cpp"
#undef fsync
#undef app_main

void test_idf_s2_cdc_endpoint_readiness_and_prompt() {
  rxBytes = -1;
  TEST_ASSERT_EQUAL_UINT(0, receivedBytes(nullptr));
  rxBytes = 510;
  TEST_ASSERT_EQUAL_UINT(510, receivedBytes(nullptr));
  TEST_ASSERT_EQUAL_UINT(2, interruptPolls);

  writeText(nullptr, "> ", false);
  TEST_ASSERT_EQUAL_STRING("> ", pending.c_str());
  TEST_ASSERT_GREATER_THAN_UINT(0, endpointChecks);
  TEST_ASSERT_EQUAL_HEX8(0x84, checkedEndpoint);
  endpointReady = false;
  flushConsole();
  TEST_ASSERT_EQUAL_UINT(0, syncCalls);
  TEST_ASSERT_TRUE(published.empty());
  endpointReady = true;
  flushConsole();
  TEST_ASSERT_EQUAL_UINT(1, syncCalls);
  TEST_ASSERT_EQUAL_INT(fileno(stdout), syncedFd);
  TEST_ASSERT_EQUAL_STRING("> ", published.c_str());

  // The ROM endpoint can accept a short prefix of each full packet.
  std::string longLine;
  for (unsigned i = 0; i < 149; ++i) longLine += static_cast<char>('a' + i % 26);
  writeText(nullptr, longLine.c_str(), true);
  flushConsole();
  TEST_ASSERT_EQUAL_UINT(64, largestWrite);
  TEST_ASSERT_EQUAL_STRING(("> " + longLine + "\r\n").c_str(), published.c_str());

  // Both endpoint backpressure and a ready endpoint accepting zero bytes
  // must yield the task and return within the same finite wait budget.
  for (bool ready : {false, true}) {
    endpointReady = ready;
    acceptedPerWrite = 0;
    fakeIdf::sdk.taskDelays.clear();
    writeText(nullptr, "blocked", false);
    TEST_ASSERT_EQUAL_UINT(25, fakeIdf::sdk.taskDelays.size());
    for (auto ticks : fakeIdf::sdk.taskDelays) TEST_ASSERT_EQUAL_UINT(1, ticks);
    TEST_ASSERT_TRUE(pending.empty());
  }
}
