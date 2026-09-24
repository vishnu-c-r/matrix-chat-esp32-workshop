/*
 * =========================================================================
 * MATRIX WORKSHOP — EXAMPLE 01: STANDALONE NEOPIXEL (WS2812) TEST
 * =========================================================================
 * Purpose: Learn how the WS2812 single-wire RGB LED works on ESP32
 * without using delay()!
 *
 * Supported Boards:
 *   - ESP32-C3 (SuperMini / DevKit): Onboard RGB LED is on GPIO 2
 *   - ESP32-S3 (Zero / DevKit):      Onboard RGB LED is on GPIO 48
 *   - ESP32 WROOM / External WS2812: Set RGB_PIN to your data pin
 *
 * Upload this single file to your ESP32, open Serial Monitor at 115200.
 * =========================================================================
 */

#include <Arduino.h>

// Select the pin for your hardware:
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define RGB_PIN 2     // ESP32-C3 SuperMini onboard NeoPixel
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define RGB_PIN 48    // ESP32-S3 onboard NeoPixel
#else
  #define RGB_PIN 2     // Default fallback
#endif

// Preset palette: Red, Green, Blue, Cyan, Magenta, Yellow
struct RGBColor { uint8_t r; uint8_t g; uint8_t b; const char* name; };
const RGBColor PALETTE[] = {
  {255,   0,   0, "RED"},
  {  0, 255,   0, "GREEN"},
  {  0,   0, 255, "BLUE"},
  {  0, 255, 255, "CYAN"},
  {255,   0, 255, "MAGENTA"},
  {255, 200,   0, "YELLOW"}
};
const int NUM_COLORS = sizeof(PALETTE) / sizeof(PALETTE[0]);

int colorIndex = 0;
uint32_t lastColorSwitchMs = 0;

void setup() {
  Serial.begin(115200);
  delay(1000); // Allow USB Serial to enumerate

  Serial.println("\n======================================");
  Serial.println("  NeoPixel (WS2812) Standalone Test   ");
  Serial.printf("  Controlling Pin: GPIO %d\n", RGB_PIN);
  Serial.println("======================================\n");
}

void loop() {
  uint32_t now = millis();

  // 1. Switch color every 2000 ms (Non-blocking!)
  if (now - lastColorSwitchMs >= 2000) {
    lastColorSwitchMs = now;
    colorIndex = (colorIndex + 1) % NUM_COLORS;
    Serial.printf("[NeoPixel] Switched to: %s\n", PALETTE[colorIndex].name);
  }

  // 2. Compute smooth sine-wave breathing brightness (0.05 to 0.40)
  // Period = 2000 ms full breath cycle
  float phase = (now % 2000) * (2.0f * 3.14159f / 2000.0f);
  float brightness = 0.05f + 0.35f * (0.5f + 0.5f * sinf(phase));

  // 3. Write out the GRB 24-bit pulse over single wire
  uint8_t r = (uint8_t)(PALETTE[colorIndex].r * brightness);
  uint8_t g = (uint8_t)(PALETTE[colorIndex].g * brightness);
  uint8_t b = (uint8_t)(PALETTE[colorIndex].b * brightness);

  neopixelWrite(RGB_PIN, r, g, b);

  // Tiny 15ms sleep to give CPU time for background tasks (~60 FPS)
  delay(15);
}
