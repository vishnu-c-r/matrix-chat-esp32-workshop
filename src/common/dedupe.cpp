// =============================================================
//  dedupe.cpp — 64-entry ring buffer duplicate filter.
//  Pure C++, no Arduino deps — unit-testable on native.
// =============================================================
#include "dedupe.h"

static constexpr uint8_t RING_SIZE = 64;

struct DedupeEntry {
    uint8_t  node_id;
    uint16_t seq;
};

static DedupeEntry g_ring[RING_SIZE];
static uint8_t     g_head  = 0;
static uint8_t     g_count = 0;

void dedupeInit()
{
    g_head  = 0;
    g_count = 0;
}

bool dedupeSeen(uint8_t node_id, uint16_t seq)
{
    // Linear scan over the live portion of the ring.
    for (uint8_t i = 0; i < g_count; ++i)
    {
        uint8_t idx = (uint8_t)((g_head + RING_SIZE - g_count + i) % RING_SIZE);
        if (g_ring[idx].node_id == node_id && g_ring[idx].seq == seq)
        {
            return true;  // duplicate — seen before
        }
    }

    // Not seen: insert at head and advance.
    g_ring[g_head] = {node_id, seq};
    g_head         = (g_head + 1) % RING_SIZE;
    if (g_count < RING_SIZE)
    {
        ++g_count;
    }
    return false;
}
