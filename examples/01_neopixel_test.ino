/*
 * =========================================================================
 * MATRIX WORKSHOP — EXAMPLE 01: SIMPLE NEOPIXEL (WS2812) TEST
 * =========================================================================
 * Board: ESP32-C3 SuperMini (Onboard NeoPixel is GPIO 2)
 *
 * Upload this file and open Serial Monitor at 115200 baud.
 * =========================================================================
 */

#include <Arduino.h>

#define RGB_PIN 2

void setup() {
  Serial.begin(115200);
  Serial.println("\n[NeoPixel] Test Ready (GPIO 2)");
}

void loop() {
  // Red
  Serial.println("LED: RED");
  neopixelWrite(RGB_PIN, 50, 0, 0);
  delay(1000);

  // Green
  Serial.println("LED: GREEN");
  neopixelWrite(RGB_PIN, 0, 50, 0);
  delay(1000);

  // Blue
  Serial.println("LED: BLUE");
  neopixelWrite(RGB_PIN, 0, 0, 50);
  delay(1000);

  // Off
  Serial.println("LED: OFF");
  neopixelWrite(RGB_PIN, 0, 0, 0);
  delay(1000);
}

