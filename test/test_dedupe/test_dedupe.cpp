// =============================================================
//  test/test_dedupe/test_dedupe.cpp — Unity tests: ring buffer
// =============================================================
#include <unity.h>

#include "../../src/common/dedupe.cpp"

void setUp()    { dedupeInit(); }
void tearDown() {}

void test_new_packet_not_seen()
{
    TEST_ASSERT_FALSE(dedupeSeen(1, 100));
}

void test_same_packet_seen_second_time()
{
    dedupeSeen(1, 200);
    TEST_ASSERT_TRUE(dedupeSeen(1, 200));
}

void test_different_seq_not_seen()
{
    dedupeSeen(1, 300);
    TEST_ASSERT_FALSE(dedupeSeen(1, 301));
}

void test_different_node_id_not_seen()
{
    dedupeSeen(1, 400);
    TEST_ASSERT_FALSE(dedupeSeen(2, 400));
}

void test_ring_eviction_after_64_entries()
{
    dedupeInit();
    // Insert 65 distinct entries (seq 0..64).
    // The 65th insert (seq=64) overwrites the oldest slot (seq=0), evicting it.
    for (uint16_t i = 0; i <= 64; ++i) dedupeSeen(5, i);
    // seq=0 was evicted by seq=64 — it must no longer be found in the ring.
    TEST_ASSERT_FALSE(dedupeSeen(5, 0));
}

void test_seq_still_in_ring_is_seen()
{
    dedupeInit();
    for (uint16_t i = 0; i < 10; ++i) dedupeSeen(3, i);
    TEST_ASSERT_TRUE(dedupeSeen(3, 9));
}

void test_zero_seq_is_valid()
{
    TEST_ASSERT_FALSE(dedupeSeen(0, 0));
    TEST_ASSERT_TRUE (dedupeSeen(0, 0));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_new_packet_not_seen);
    RUN_TEST(test_same_packet_seen_second_time);
    RUN_TEST(test_different_seq_not_seen);
    RUN_TEST(test_different_node_id_not_seen);
    RUN_TEST(test_ring_eviction_after_64_entries);
    RUN_TEST(test_seq_still_in_ring_is_seen);
    RUN_TEST(test_zero_seq_is_valid);
    return UNITY_END();
}
