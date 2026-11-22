#pragma once

#include <algorithm>
#include <map>
#include <vector>

#include "driver/i2c_master.h"
#include "test/test_native/FakeRv3032.h"

namespace fakeIdf {

enum class Call {
  GPIO_CONFIG, GPIO_SET, GPIO_GET, NEW_BUS, ADD_DEVICE, REMOVE_DEVICE,
  DELETE_BUS, TRANSMIT, TRANSMIT_RECEIVE, RESET_BUS, DELAY
};

struct State {
  std::vector<Call> calls;
  std::map<Call, esp_err_t> errors;
  int64_t nowUs = 0;
  int64_t advanceAfterNextClockReadUs = 0;
  unsigned clockReads = 0;
  uint32_t transferUs = 0;
  bool earlyNextWake = false;
  std::vector<uint32_t> taskDelays;
  std::vector<int> transferTimeouts;

  gpio_config_t pins{};
  bool sdaHigh = true;
  bool sclHigh = true;
  bool holdSdaLow = false;
  bool holdSclLow = false;
  std::vector<std::pair<gpio_num_t, uint32_t>> levels;

  bool busLive = false;
  bool deviceLive = false;
  i2c_master_bus_config_t busConfig{};
  i2c_device_config_t deviceConfig{};
  test_rv3032::FakeRv3032 rtc;

  size_t count(Call call) const {
    return static_cast<size_t>(std::count(calls.begin(), calls.end(), call));
  }
  esp_err_t record(Call call) {
    calls.push_back(call);
    const auto found = errors.find(call);
    return found == errors.end() ? ESP_OK : found->second;
  }
};

extern State sdk;

}  // namespace fakeIdf
