#include <unity.h>

#include "FakeIdf.h"
#include "RV3032/RV3032.h"
#include "examples/esp_idf/basic/main/Rv3032IdfI2c.h"

namespace idf = rv3032Idf;
using fakeIdf::Call;
using fakeIdf::sdk;
using RV3032::Err;

void setUp() { sdk = fakeIdf::State{}; }
void tearDown() {}

namespace {
constexpr uint8_t ADDRESS = 0x51;
constexpr uint8_t FRAME[] = {RV3032::cmd::REG_SECONDS, 0x23};

void test_startup_and_teardown() {
  idf::Context ctx{};
  TEST_ASSERT_TRUE(idf::init(ctx).ok());
  TEST_ASSERT_NOT_NULL(ctx.device);
  TEST_ASSERT_NOT_NULL(ctx.bus);
  TEST_ASSERT_EQUAL(GPIO_MODE_INPUT_OUTPUT_OD, sdk.pins.mode);
  TEST_ASSERT_EQUAL_UINT64((1ULL << GPIO_NUM_8) | (1ULL << GPIO_NUM_9),
                           sdk.pins.pin_bit_mask);
  // Nine clear clocks and one STOP with SDA released while SCL is high.
  bool sclHigh = true;
  bool sdaHigh = true;
  unsigned pulses = 0;
  unsigned stops = 0;
  for (const auto& level : sdk.levels) {
    if (level.first == GPIO_NUM_9) {
      if (!sclHigh && level.second && sdaHigh) ++pulses;
      sclHigh = level.second != 0;
    } else {
      if (!sdaHigh && level.second && sclHigh) ++stops;
      sdaHigh = level.second != 0;
    }
  }
  TEST_ASSERT_EQUAL_UINT(9, pulses);
  TEST_ASSERT_EQUAL_UINT(1, stops);
  TEST_ASSERT_TRUE(sclHigh && sdaHigh);
  TEST_ASSERT_EQUAL(GPIO_NUM_8, sdk.busConfig.sda_io_num);
  TEST_ASSERT_EQUAL(GPIO_NUM_9, sdk.busConfig.scl_io_num);
  TEST_ASSERT_EQUAL(0, sdk.busConfig.trans_queue_depth);
  TEST_ASSERT_EQUAL(I2C_ADDR_BIT_LEN_7, sdk.deviceConfig.dev_addr_length);
  TEST_ASSERT_EQUAL_HEX8(ADDRESS, sdk.deviceConfig.device_address);
  TEST_ASSERT_EQUAL_UINT32(400000, sdk.deviceConfig.scl_speed_hz);
  sdk.calls.clear();
  TEST_ASSERT_TRUE(idf::deinit(ctx).ok());
  TEST_ASSERT_TRUE((sdk.calls == std::vector<Call>{Call::REMOVE_DEVICE, Call::DELETE_BUS}));
  TEST_ASSERT_NULL(ctx.device);
  TEST_ASSERT_NULL(ctx.bus);
  sdk.calls.clear();
  TEST_ASSERT_TRUE(idf::deinit(ctx).ok());
  TEST_ASSERT_TRUE(sdk.calls.empty());
}

void test_init_fails_on_stuck_lines_before_installing_bus() {
  for (const bool holdClock : {false, true}) {
    sdk = fakeIdf::State{};
    sdk.holdSclLow = holdClock;
    sdk.holdSdaLow = !holdClock;
    idf::Context ctx{};
    TEST_ASSERT_FALSE(idf::init(ctx).ok());
    TEST_ASSERT_EQUAL(0, sdk.count(Call::NEW_BUS));
    TEST_ASSERT_TRUE(sdk.nowUs <= 51000);
    TEST_ASSERT_NULL(ctx.device);
    TEST_ASSERT_NULL(ctx.bus);
  }
}

void test_init_errors_unwind_owned_resources() {
  for (const auto fail : {Call::GPIO_SET, Call::GPIO_CONFIG, Call::NEW_BUS, Call::ADD_DEVICE}) {
    sdk = fakeIdf::State{};
    sdk.errors[fail] = ESP_FAIL;
    idf::Context ctx{};
    TEST_ASSERT_FALSE(idf::init(ctx).ok());
    TEST_ASSERT_EQUAL(1, sdk.count(fail));
    TEST_ASSERT_EQUAL(fail == Call::ADD_DEVICE ? 1 : 0, sdk.count(Call::DELETE_BUS));
    TEST_ASSERT_FALSE(sdk.busLive || sdk.deviceLive);
    TEST_ASSERT_NULL(ctx.device);
    TEST_ASSERT_NULL(ctx.bus);
  }
}

void test_failed_cleanup_retains_owned_handles() {
  idf::Context ctx{};
  sdk.errors[Call::ADD_DEVICE] = ESP_FAIL;
  sdk.errors[Call::DELETE_BUS] = ESP_ERR_INVALID_STATE;
  TEST_ASSERT_FALSE(idf::init(ctx).ok());
  TEST_ASSERT_NOT_NULL(ctx.bus);
  TEST_ASSERT_NULL(ctx.device);
  sdk.errors.clear();
  TEST_ASSERT_TRUE(idf::deinit(ctx).ok());

  TEST_ASSERT_TRUE(idf::init(ctx).ok());
  const auto bus = ctx.bus;
  const auto device = ctx.device;
  sdk.calls.clear();
  sdk.errors[Call::REMOVE_DEVICE] = ESP_FAIL;
  TEST_ASSERT_FALSE(idf::deinit(ctx).ok());
  TEST_ASSERT_EQUAL_PTR(device, ctx.device);
  TEST_ASSERT_EQUAL_PTR(bus, ctx.bus);
  TEST_ASSERT_TRUE((sdk.calls == std::vector<Call>{Call::REMOVE_DEVICE}));
  sdk.errors.clear();
  sdk.errors[Call::DELETE_BUS] = ESP_FAIL;
  TEST_ASSERT_FALSE(idf::deinit(ctx).ok());
  TEST_ASSERT_NULL(ctx.device);
  TEST_ASSERT_EQUAL_PTR(bus, ctx.bus);
  sdk.errors.clear();
  sdk.calls.clear();
  TEST_ASSERT_TRUE(idf::deinit(ctx).ok());
  TEST_ASSERT_TRUE((sdk.calls == std::vector<Call>{Call::DELETE_BUS}));
  TEST_ASSERT_NULL(ctx.bus);
}

void test_validation_precedes_all_sdk_calls() {
  idf::Context ctx{}, unbound{};
  TEST_ASSERT_TRUE(idf::init(ctx).ok());
  sdk.calls.clear();
  sdk.clockReads = 0;
  uint8_t rx = 0;
  for (void* invalid : {static_cast<void*>(nullptr), static_cast<void*>(&unbound)}) {
    TEST_ASSERT_TRUE(idf::i2cWrite(ADDRESS, FRAME, 2, 5, invalid).is(Err::I2C_ERROR));
    TEST_ASSERT_TRUE(idf::i2cWriteRead(ADDRESS, FRAME, 1, &rx, 1, 5, invalid).is(Err::I2C_ERROR));
  }
  TEST_ASSERT_TRUE(idf::i2cWrite(0x50, FRAME, 2, 5, &ctx).is(Err::I2C_ERROR));
  TEST_ASSERT_TRUE(idf::i2cWriteRead(0x50, FRAME, 1, &rx, 1, 5, &ctx).is(Err::I2C_ERROR));
  TEST_ASSERT_TRUE(idf::i2cWrite(ADDRESS, nullptr, 2, 5, &ctx).is(Err::I2C_ERROR));
  TEST_ASSERT_TRUE(idf::i2cWrite(ADDRESS, FRAME, 0, 5, &ctx).is(Err::I2C_ERROR));
  TEST_ASSERT_TRUE(idf::i2cWriteRead(ADDRESS, nullptr, 1, &rx, 1, 5, &ctx).is(Err::I2C_ERROR));
  TEST_ASSERT_TRUE(idf::i2cWriteRead(ADDRESS, FRAME, 0, &rx, 1, 5, &ctx).is(Err::I2C_ERROR));
  TEST_ASSERT_TRUE(idf::i2cWriteRead(ADDRESS, FRAME, 1, nullptr, 1, 5, &ctx).is(Err::I2C_ERROR));
  TEST_ASSERT_TRUE(idf::i2cWriteRead(ADDRESS, FRAME, 1, &rx, 0, 5, &ctx).is(Err::I2C_ERROR));
  for (uint32_t timeout : {0U, 101U, UINT32_MAX}) {
    TEST_ASSERT_TRUE(idf::i2cWrite(ADDRESS, FRAME, 2, timeout, &ctx).is(Err::I2C_ERROR));
    TEST_ASSERT_TRUE(idf::i2cWriteRead(ADDRESS, FRAME, 1, &rx, 1, timeout, &ctx).is(Err::I2C_ERROR));
  }
  TEST_ASSERT_TRUE(sdk.calls.empty());
  TEST_ASSERT_EQUAL_UINT(0, sdk.clockReads);
}

RV3032::Status transfer(Call call, idf::Context& ctx, uint32_t budget) {
  if (call == Call::TRANSMIT) return idf::i2cWrite(ADDRESS, FRAME, 2, budget, &ctx);
  uint8_t rx = 0;
  return idf::i2cWriteRead(ADDRESS, FRAME, 1, &rx, 1, budget, &ctx);
}

void test_sdk_status_mapping_and_single_attempt() {
  idf::Context ctx{};
  TEST_ASSERT_TRUE(idf::init(ctx).ok());
  const struct { esp_err_t sdkError; Err expected; } outcomes[] = {
      {ESP_OK, Err::OK}, {ESP_ERR_TIMEOUT, Err::I2C_TIMEOUT},
      {ESP_ERR_INVALID_RESPONSE, Err::I2C_ERROR}, {ESP_ERR_NOT_FOUND, Err::I2C_ERROR},
      {ESP_ERR_INVALID_ARG, Err::I2C_ERROR}, {ESP_ERR_INVALID_STATE, Err::I2C_ERROR},
      {ESP_FAIL, Err::I2C_ERROR}, {0x789, Err::I2C_ERROR}};
  for (Call call : {Call::TRANSMIT, Call::TRANSMIT_RECEIVE}) {
    for (const auto& outcome : outcomes) {
      sdk.calls.clear();
      sdk.errors[call] = outcome.sdkError;
      sdk.transferUs = 49000;
      sdk.advanceAfterNextClockReadUs = 1000;
      const int64_t start = sdk.nowUs;
      const auto result = transfer(call, ctx, 100);
      TEST_ASSERT_TRUE(result.is(outcome.expected));
      TEST_ASSERT_EQUAL_INT(outcome.sdkError, result.detail);
      TEST_ASSERT_TRUE(sdk.nowUs - start == 50000);
      TEST_ASSERT_TRUE((sdk.calls == std::vector<Call>{call}));
      TEST_ASSERT_EQUAL_INT(98, sdk.transferTimeouts.back());
    }
  }
}

void test_dispatch_guard_and_late_completion() {
  idf::Context ctx{};
  TEST_ASSERT_TRUE(idf::init(ctx).ok());
  for (Call call : {Call::TRANSMIT, Call::TRANSMIT_RECEIVE}) {
    sdk.calls.clear();
    TEST_ASSERT_TRUE(transfer(call, ctx, 1).is(Err::I2C_TIMEOUT));
    sdk.advanceAfterNextClockReadUs = 4000;
    TEST_ASSERT_TRUE(transfer(call, ctx, 5).is(Err::I2C_TIMEOUT));
    TEST_ASSERT_TRUE(sdk.calls.empty());
    sdk.transferUs = 4999;
    TEST_ASSERT_TRUE(transfer(call, ctx, 5).ok());
    for (uint32_t elapsed : {5000U, 5100U}) {
      sdk.transferUs = elapsed;
      const auto result = transfer(call, ctx, 5);
      TEST_ASSERT_TRUE(result.is(Err::I2C_TIMEOUT));
      TEST_ASSERT_EQUAL_INT(ESP_OK, result.detail);
    }
    TEST_ASSERT_TRUE((sdk.calls == std::vector<Call>{call, call, call}));
    for (const int timeout : sdk.transferTimeouts) TEST_ASSERT_EQUAL_INT(4, timeout);
  }
}

void test_wait_rechecks_monotonic_time_across_wrap_and_yields() {
  sdk.nowUs = (static_cast<int64_t>(UINT32_MAX) - 2) * 1000 + 999;
  // The wake crosses five millisecond boundaries in only 4001 microseconds.
  sdk.earlyWakesUs = {4001};
  const auto before = idf::nowMs(nullptr);
  const auto startUs = sdk.nowUs;
  idf::waitMs(5, nullptr);
  TEST_ASSERT_TRUE(idf::nowMs(nullptr) - before >= 5U);
  TEST_ASSERT_TRUE(sdk.nowUs - startUs >= 5000);
  TEST_ASSERT_EQUAL(2, sdk.taskDelays.size());
  TEST_ASSERT_EQUAL_UINT32(6, sdk.taskDelays.front());
  TEST_ASSERT_TRUE(idf::nowMs(nullptr) < before);
  TEST_ASSERT_TRUE((sdk.sleepsUs == std::vector<uint32_t>{1000}));

  sdk.nowUs = 999;
  sdk.earlyWakesUs = {4001, 0, 0, 0, 0, 0, 0};
  sdk.sleepsUs.clear();
  idf::waitMs(5, nullptr);
  TEST_ASSERT_TRUE(sdk.nowUs >= 5999);
  TEST_ASSERT_TRUE(sdk.earlyWakesUs.empty());
  TEST_ASSERT_TRUE((sdk.sleepsUs == std::vector<uint32_t>{1000}));
}

RV3032::Config config(idf::Context& ctx) {
  RV3032::Config cfg{};
  cfg.i2cWrite = idf::i2cWrite;
  cfg.i2cWriteRead = idf::i2cWriteRead;
  cfg.i2cUser = &ctx;
  cfg.nowMs = idf::nowMs;
  cfg.waitMs = idf::waitMs;
  return cfg;
}

void test_driver_calendar_and_cooperative_snapshot() {
  idf::Context ctx{};
  TEST_ASSERT_TRUE(idf::init(ctx).ok());
  sdk.calls.clear();
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(config(ctx)).ok());
  TEST_ASSERT_TRUE(sdk.calls.empty());
  TEST_ASSERT_TRUE(rtc.probe().ok());
  const RV3032::DateTime desired{2026, 11, 22, 14, 17, 39, 0};
  TEST_ASSERT_TRUE(rtc.setTime(desired).ok());
  RV3032::DateTime actual{};
  TEST_ASSERT_TRUE(rtc.readTime(actual).ok());
  TEST_ASSERT_EQUAL_UINT16(desired.year, actual.year);
  TEST_ASSERT_EQUAL_UINT8(desired.day, actual.day);
  TEST_ASSERT_EQUAL_UINT8(desired.second, actual.second);
  TEST_ASSERT_TRUE((sdk.calls == std::vector<Call>{Call::TRANSMIT_RECEIVE,
      Call::TRANSMIT, Call::TRANSMIT_RECEIVE}));

  sdk.rtc.direct[RV3032::cmd::REG_STATUS] = 0;
  sdk.calls.clear();
  auto st = rtc.startReadTimeSnapshotJob(idf::nowMs(nullptr));
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(sdk.calls.empty());
  for (unsigned i = 0; i < 4 && st.inProgress(); ++i) {
    const size_t before = sdk.calls.size();
    uint8_t used = 0;
    st = rtc.pollJob(idf::nowMs(nullptr), 1, used);
    TEST_ASSERT_TRUE(used <= 1);
    TEST_ASSERT_EQUAL_UINT(used, sdk.calls.size() - before);
  }
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE((sdk.calls == std::vector<Call>{Call::TRANSMIT_RECEIVE, Call::TRANSMIT_RECEIVE}));
  RV3032::TimeSnapshot snapshot{};
  TEST_ASSERT_TRUE(rtc.getReadTimeSnapshotJobResult(snapshot).ok());
  TEST_ASSERT_TRUE(snapshot.timeValid);
  TEST_ASSERT_EQUAL_UINT8(desired.minute, snapshot.time.minute);
}

void test_primary_cell_ensure_through_single_attempt_adapter() {
  idf::Context ctx{};
  TEST_ASSERT_TRUE(idf::init(ctx).ok());
  sdk.rtc.persistent[0] = 0;
  sdk.rtc.resetFromPersistent();
  sdk.calls.clear();
  RV3032::RV3032 rtc;
  TEST_ASSERT_TRUE(rtc.begin(config(ctx)).ok());
  RV3032::PrimaryCellConfigurationReport report{};
  TEST_ASSERT_TRUE(rtc.ensurePrimaryCellConfiguration(report).ok());
  TEST_ASSERT_TRUE(report.writeDurablyVerified && report.activeTargetVerified && report.cleanupVerified);
  TEST_ASSERT_EQUAL_UINT16(1, sdk.rtc.writeOneAttempts);
  TEST_ASSERT_FALSE(sdk.rtc.protocolViolation);
  TEST_ASSERT_EQUAL_UINT(sdk.rtc.physicalAttemptCount, sdk.count(Call::TRANSMIT) +
                         sdk.count(Call::TRANSMIT_RECEIVE));
  TEST_ASSERT_EQUAL_UINT(0, sdk.count(Call::RESET_BUS));
  TEST_ASSERT_FALSE(sdk.taskDelays.empty());
  for (const int timeout : sdk.transferTimeouts) TEST_ASSERT_TRUE(timeout > 0 && timeout < 5);
}
}  // namespace

void runIdfCliTests();
void test_idf_s2_cdc_endpoint_readiness_and_prompt();

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_startup_and_teardown);
  RUN_TEST(test_init_fails_on_stuck_lines_before_installing_bus);
  RUN_TEST(test_init_errors_unwind_owned_resources);
  RUN_TEST(test_failed_cleanup_retains_owned_handles);
  RUN_TEST(test_validation_precedes_all_sdk_calls);
  RUN_TEST(test_sdk_status_mapping_and_single_attempt);
  RUN_TEST(test_dispatch_guard_and_late_completion);
  RUN_TEST(test_wait_rechecks_monotonic_time_across_wrap_and_yields);
  RUN_TEST(test_driver_calendar_and_cooperative_snapshot);
  RUN_TEST(test_primary_cell_ensure_through_single_attempt_adapter);
  runIdfCliTests();
  RUN_TEST(test_idf_s2_cdc_endpoint_readiness_and_prompt);
  return UNITY_END();
}
