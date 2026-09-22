// =============================================================
//  test/test_rssi/test_rssi.cpp — Unity tests: EMA + state machine
// =============================================================
#include <unity.h>
#include <stdint.h>

#include "../../src/common/rssi_tracker.cpp"

static RssiTracker makeTracker()
{
    RssiTracker t = {};
    rssiTrackerInit(&t, 0.2f, -50.0f, -55.0f, -85.0f, 1500u);
    return t;
}

void setUp()    {}
void tearDown() {}

void test_initial_state_is_clear()
{
    RssiTracker t = makeTracker();
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SmithState::CLEAR,
                            (uint8_t)rssiTrackerTick(&t, 1000u));
}

void test_timeout_resets_to_clear()
{
    RssiTracker t = makeTracker();
    rssiTrackerUpdate(&t, -70, 0u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SmithState::CLEAR,
                            (uint8_t)rssiTrackerTick(&t, 1500u));
}

void test_no_timeout_before_deadline()
{
    RssiTracker t = makeTracker();
    rssiTrackerUpdate(&t, -70, 0u);
    TEST_ASSERT_NOT_EQUAL((uint8_t)SmithState::CLEAR,
                          (uint8_t)rssiTrackerTick(&t, 1499u));
}

void test_ema_seeds_from_first_sample()
{
    RssiTracker t = makeTracker();
    rssiTrackerUpdate(&t, -70, 1000u);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -70.0f, t.rssi_f);
}

void test_ema_smooths_toward_new_value()
{
    RssiTracker t = makeTracker();
    rssiTrackerUpdate(&t, -80, 0u);
    rssiTrackerUpdate(&t, -60, 100u);
    // 0.2*(-60) + 0.8*(-80) = -76.0
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -76.0f, t.rssi_f);
}

void test_near_state_entered()
{
    RssiTracker t = makeTracker();
    rssiTrackerUpdate(&t, -80, 0u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SmithState::NEAR,
                            (uint8_t)rssiTrackerTick(&t, 500u));
}

void test_close_state_entered_above_threshold()
{
    RssiTracker t = makeTracker();
    for (int i = 0; i < 40; ++i) rssiTrackerUpdate(&t, -30, (uint32_t)(i * 50));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SmithState::CLOSE,
                            (uint8_t)rssiTrackerTick(&t, 2000u));
}

void test_hysteresis_stays_close_until_exit_threshold()
{
    RssiTracker t = makeTracker();
    for (int i = 0; i < 40; ++i) rssiTrackerUpdate(&t, -30, (uint32_t)(i * 10));
    rssiTrackerTick(&t, 500u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SmithState::CLOSE, (uint8_t)t.state);

    // Between exit (-55) and enter (-50) — should stay CLOSE.
    rssiTrackerUpdate(&t, -52, 600u);
    rssiTrackerTick(&t, 700u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SmithState::CLOSE, (uint8_t)t.state);

    // Drive below exit threshold — should leave CLOSE.
    for (int i = 0; i < 20; ++i) rssiTrackerUpdate(&t, -80, (uint32_t)(800 + i * 10));
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SmithState::NEAR,
                            (uint8_t)rssiTrackerTick(&t, 1000u));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_clear);
    RUN_TEST(test_timeout_resets_to_clear);
    RUN_TEST(test_no_timeout_before_deadline);
    RUN_TEST(test_ema_seeds_from_first_sample);
    RUN_TEST(test_ema_smooths_toward_new_value);
    RUN_TEST(test_near_state_entered);
    RUN_TEST(test_close_state_entered_above_threshold);
    RUN_TEST(test_hysteresis_stays_close_until_exit_threshold);
    return UNITY_END();
}
