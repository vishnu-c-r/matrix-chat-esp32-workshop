// =============================================================
//  rssi_tracker.h — EMA RSSI filter + Smith proximity state.
//  Pure C++, no Arduino deps — unit-testable on native.
// =============================================================
#pragma once
#include <stdint.h>

enum class SmithState : uint8_t {
    CLEAR = 0,   // No Smith signal (or timed out)
    NEAR  = 1,   // Smith detected but not very close
    CLOSE = 2,   // Smith very close (solid red)
};

struct RssiTracker {
    float      rssi_f;           // EMA-filtered RSSI (dBm)
    SmithState state;
    uint32_t   last_seen_ms;     // millis() at last Smith packet
    float      alpha;            // EMA coefficient
    float      thr_close_enter;  // Enter CLOSE above this (e.g. -50)
    float      thr_close_exit;   // Leave CLOSE below this (e.g. -55, hysteresis)
    float      thr_near;         // Enter NEAR above this (e.g. -85)
    uint32_t   timeout_ms;       // Time before CLEAR if no packet
    bool       initialized;
};

// Initialise with threshold values from config.h.
void rssiTrackerInit(RssiTracker* t,
                     float   alpha,
                     float   close_enter,
                     float   close_exit,
                     float   near_thr,
                     uint32_t timeout_ms);

// Call whenever a Smith packet arrives with the raw RSSI value.
void rssiTrackerUpdate(RssiTracker* t, int8_t rssi_raw, uint32_t now_ms);

// Call every loop iteration with current time; returns updated state.
SmithState rssiTrackerTick(RssiTracker* t, uint32_t now_ms);
