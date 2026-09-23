// =============================================================
//  board_pins.h — Per-target pin assignments.
//  Avoids strapping and flash pins on every supported board.
// =============================================================
#pragma once
#include <stdint.h>
#include <Arduino.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
// ---- ESP32-S3-DevKitC-1 -------------------------------------
// External common-cathode RGB (optional wiring):
static constexpr uint8_t PIN_LED_R    = 4;
static constexpr uint8_t PIN_LED_G    = 5;
static constexpr uint8_t PIN_LED_B    = 6;
// Onboard addressable RGB (WS2812, always present on DevKitC-1):
static constexpr uint8_t PIN_NEOPIXEL = 48;
static constexpr bool    HAS_NEOPIXEL = true;
// BOOT button (safe GPIO, not a strapping pin):
static constexpr uint8_t PIN_BOOT     = 0;

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
// ---- ESP32-C3 SuperMini -------------------------------------
// External NeoPixel data pin (Connect your NeoPixel Data-IN here):
static constexpr uint8_t PIN_NEOPIXEL = 2;
static constexpr bool    HAS_NEOPIXEL = true;
// C3 BOOT button is GPIO9 (GPIO0 is a strapping pin on C3):
static constexpr uint8_t PIN_BOOT     = 9;

#else
// ---- ESP32-WROOM-32 (esp32dev) ------------------------------
// GPIO 25/26/27 avoid flash (6-11) and strapping (0,2,12,15) pins:
static constexpr uint8_t PIN_LED_R    = 25;
static constexpr uint8_t PIN_LED_G    = 26;
static constexpr uint8_t PIN_LED_B    = 27;
static constexpr bool    HAS_NEOPIXEL = false;
static constexpr uint8_t PIN_BOOT     = 0;

#endif
