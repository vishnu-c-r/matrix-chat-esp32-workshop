// =============================================================
//  board_pins.h — Pin assignments for ESP32-C3 SuperMini.
// =============================================================
#pragma once
#include <Arduino.h>
#include <stdint.h>

// ---- ESP32-C3 SuperMini -------------------------------------
// Onboard NeoPixel data pin (GPIO 2):
static constexpr uint8_t PIN_NEOPIXEL = 2;
static constexpr bool    HAS_NEOPIXEL = true;
// BOOT button is GPIO 9 (GPIO 0 is a strapping pin on C3):
static constexpr uint8_t PIN_BOOT     = 9;

