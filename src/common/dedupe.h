// =============================================================
//  dedupe.h — Duplicate packet filter (ring buffer on node_id+seq).
//  Pure C++, no Arduino deps — unit-testable on native.
// =============================================================
#pragma once
#include <stdint.h>

// Initialise the ring buffer (call once at startup).
void dedupeInit();

// Returns true if (node_id, seq) was already seen; inserts it if new.
// Ring holds 64 entries; oldest entry is evicted when full.
bool dedupeSeen(uint8_t node_id, uint16_t seq);
