/*
 * =========================================================================
 * MATRIX WORKSHOP — EXAMPLE 02: SIMPLE ESP-NOW BROADCAST TEST
 * =========================================================================
 * Purpose: Test wireless broadcast between two or more ESP32-C3 boards.
 *
 * Open Serial Monitor at 115200 baud on each board.
 * Each board broadcasts "Ping #1", "Ping #2"... every 2 seconds.
 * =========================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define RGB_PIN 2
#define WIFI_CHANNEL 11

// Broadcast address: sends to all devices listening on this channel
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint32_t g_count = 0;

// Called automatically when a packet is received
void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  Serial.printf("[RX] From: %02X:%02X:%02X:%02X:%02X:%02X | RSSI: %d dBm | Msg: %.*s\n",
                info->src_addr[0], info->src_addr[1], info->src_addr[2],
                info->src_addr[3], info->src_addr[4], info->src_addr[5],
                info->rx_ctrl->rssi, len, (const char*)data);

  // Quick green flash on receive
  neopixelWrite(RGB_PIN, 0, 50, 0);
  delay(30);
  neopixelWrite(RGB_PIN, 0, 0, 0);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n[ESP-NOW] Broadcast Test Ready (Channel 11)");

  // 1. Set Wi-Fi to Station mode on Channel 11
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  // 2. Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW init failed!");
    return;
  }

  // 3. Register receive callback
  esp_now_register_recv_cb(onDataRecv);

  // 4. Register broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  Serial.printf("[ESP-NOW] My MAC: %s\n", WiFi.macAddress().c_str());
}

void loop() {
  // Broadcast a message every 2 seconds
  char msg[32];
  snprintf(msg, sizeof(msg), "Ping #%u", ++g_count);

  Serial.printf("[TX] Sending: %s\n", msg);
  neopixelWrite(RGB_PIN, 0, 0, 50); // Blue flash on send
  esp_now_send(BROADCAST_MAC, (uint8_t*)msg, strlen(msg));
  delay(30);
  neopixelWrite(RGB_PIN, 0, 0, 0);

  delay(2000);
}

