// =============================================================
//  rssi_tracker.cpp — EMA RSSI filter + Smith proximity state.
//  Pure C++, no Arduino deps — unit-testable on native.
// =============================================================
#include "rssi_tracker.h"

void rssiTrackerInit(RssiTracker* t,
                     float    alpha,
                     float    close_enter,
                     float    close_exit,
                     float    near_thr,
                     uint32_t timeout_ms)
{
    t->rssi_f          = -100.0f;
    t->state           = SmithState::CLEAR;
    t->last_seen_ms    = 0;
    t->alpha           = alpha;
    t->thr_close_enter = close_enter;
    t->thr_close_exit  = close_exit;
    t->thr_near        = near_thr;
    t->timeout_ms      = timeout_ms;
    t->initialized     = false;
}

void rssiTrackerUpdate(RssiTracker* t, int8_t rssi_raw, uint32_t now_ms)
{
    if (!t->initialized)
    {
        // Seed with first measurement instead of starting from -100.
        t->rssi_f      = (float)rssi_raw;
        t->initialized = true;
    }
    else
    {
        // Exponential moving average: rssi_f = α·raw + (1-α)·rssi_f
        t->rssi_f = t->alpha * (float)rssi_raw + (1.0f - t->alpha) * t->rssi_f;
    }
    t->last_seen_ms = now_ms;
}

SmithState rssiTrackerTick(RssiTracker* t, uint32_t now_ms)
{
    // Timeout: no Smith packet for timeout_ms → CLEAR.
    if (!t->initialized ||
        (now_ms - t->last_seen_ms) >= t->timeout_ms)
    {
        t->state       = SmithState::CLEAR;
        t->initialized = false;
        return t->state;
    }

    // Hysteresis: CLOSE state is "sticky" until RSSI drops below thr_close_exit.
    if (t->state == SmithState::CLOSE)
    {
        if (t->rssi_f < t->thr_close_exit)
        {
            t->state = SmithState::NEAR;
        }
    }
    else
    {
        if (t->rssi_f >= t->thr_close_enter)
        {
            t->state = SmithState::CLOSE;
        }
        else if (t->rssi_f >= t->thr_near)
        {
            t->state = SmithState::NEAR;
        }
        else
        {
            t->state = SmithState::CLEAR;
        }
    }

    return t->state;
}
