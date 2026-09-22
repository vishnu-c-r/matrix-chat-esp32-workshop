// =============================================================
//  led.h — Non-blocking LED state machine.
//
//  Priority (highest first):
//    1. SMITH_CLOSE  — solid red
//    2. SMITH_NEAR   — blink red, rate from RSSI
//    3. MSG_FLASH    — 300 ms flash in sender color
//    4. IDLE         — breathing in own color (dim)
//
//  Hardware:
//    ESP32-S3  → onboard NeoPixel (pin 48, neopixelWrite)
//    WROOM/C3  → external common-cathode RGB via LEDC
// =============================================================
#pragma once
#include <stdint.h>
#include "palette.h"

// Call once in setup() after WiFi/radio init.
void ledInit(uint8_t node_id);

// Call every loop iteration — updates LED output non-blocking.
void ledLoop(uint32_t now_ms);

// Trigger a 300 ms flash in the given color (lower priority than Smith).
void ledFlashMsg(Color c);

// Update Smith proximity state; also recalculates blink period.
//   state : 0=CLEAR, 1=NEAR, 2=CLOSE  (SmithState cast to uint8_t)
//   rssi_f: EMA-filtered RSSI for blink period mapping
void ledSetSmithState(uint8_t state, float rssi_f);
