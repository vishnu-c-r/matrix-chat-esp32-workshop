/*
 * =========================================================================
 * MATRIX WORKSHOP — EXAMPLE 02: STANDALONE ESP-NOW BROADCAST TEST
 * =========================================================================
 * Purpose: Learn how ESP-NOW works with raw 2.4 GHz radio frames!
 *
 * How to run:
 *   1. Flash this exact same code to TWO (or more) ESP32 boards.
 *   2. Open Serial Monitor on both boards at 115200 baud.
 *   3. Watch them instantly exchange packets with <1ms latency without
 *      connecting to any Wi-Fi router or entering any IP address!
 * =========================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Onboard LED pin (ESP32-C3 onboard NeoPixel is GPIO 2)
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define RGB_PIN 2
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define RGB_PIN 48
#else
  #define RGB_PIN 2
#endif

#define WIFI_CHANNEL 1

// 1. Packed Binary Packet Struct (Guarantees identical memory layout)
#pragma pack(push, 1)
struct PingPacket {
  uint8_t  magic;       // 0x42 signature
  uint32_t counter;     // Incremented count
  char     sender[16];  // Board MAC string or name
  char     message[32]; // Text message
};
#pragma pack(pop)

// Broadcast MAC address (FF:FF:FF:FF:FF:FF sends to everyone in range)
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint32_t g_tx_count = 0;
static uint32_t g_last_send_ms = 0;

// 2. Asynchronous Interrupt Callback triggered when radio packet arrives
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  int8_t rssi = info->rx_ctrl->rssi;
  const uint8_t* mac = info->src_addr;
#else
void onDataRecv(const uint8_t* mac, const uint8_t* data, int len) {
  int8_t rssi = 0; // Legacy IDF
#endif
  if (len < (int)sizeof(PingPacket)) return;

  const PingPacket* pkt = (const PingPacket*)data;
  if (pkt->magic != 0x42) return; // Discard non-matching frames

  Serial.printf("\n[ESP-NOW RX] From: %02X:%02X:%02X:%02X:%02X:%02X | RSSI: %d dBm\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], rssi);
  Serial.printf("             Count: #%u | Msg: \"%s\" (Sender: %s)\n",
                pkt->counter, pkt->message, pkt->sender);

  // Flash LED Green on Receive
  neopixelWrite(RGB_PIN, 0, 150, 0);
  delay(40);
  neopixelWrite(RGB_PIN, 0, 0, 0);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n======================================");
  Serial.println("  ESP-NOW Standalone Broadcast Test   ");
  Serial.println("======================================");

  // Step A: Set Wi-Fi radio mode to Station and set fixed channel
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  // Set fixed radio channel
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  #endif

  Serial.printf("[WiFi] MAC Address: %s (Channel %d)\n", WiFi.macAddress().c_str(), WIFI_CHANNEL);

  // Step B: Initialize ESP-NOW subsystem
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ERROR] ESP-NOW Init Failed!");
    return;
  }
  Serial.println("[ESP-NOW] Subsystem initialized successfully.");

  // Step C: Register the asynchronous receive callback
  esp_now_register_recv_cb(onDataRecv);

  // Step D: Register the broadcast peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ERROR] Failed to add broadcast peer!");
  } else {
    Serial.println("[ESP-NOW] Broadcast peer registered (FF:FF:FF:FF:FF:FF). Ready!\n");
  }
}

void loop() {
  uint32_t now = millis();

  // Send a broadcast ping every 2500 ms (Non-blocking timer)
  if (now - g_last_send_ms >= 2500) {
    g_last_send_ms = now;

    PingPacket pkt = {};
    pkt.magic = 0x42;
    pkt.counter = ++g_tx_count;
    snprintf(pkt.sender, sizeof(pkt.sender), "%s", WiFi.macAddress().c_str() + 9); // Last 3 octets
    snprintf(pkt.message, sizeof(pkt.message), "Ping from %s!", pkt.sender);

    // Flash LED Blue on Transmit
    neopixelWrite(RGB_PIN, 0, 0, 150);

    esp_err_t res = esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
    if (res == ESP_OK) {
      Serial.printf("[ESP-NOW TX] Broadcast ping #%u sent successfully!\n", pkt.counter);
    } else {
      Serial.printf("[ESP-NOW TX ERROR] Code: %d\n", res);
    }

    delay(30);
    neopixelWrite(RGB_PIN, 0, 0, 0);
  }
}
