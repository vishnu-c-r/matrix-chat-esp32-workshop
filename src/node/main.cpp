// =============================================================
//  src/node/main.cpp — Human node reference solution.
//
//  Students edit ONLY include/config.h (NODE_ID, NODE_NAME).
//  SOLUTION-BEGIN/END blocks are stripped by tools/make_student.py
//  to generate the skeleton with TODO hints.
//
//  Stage guide (what make_student.py leaves for students):
//    Stage 1 — Call ledInit() so the LED shows your node color.
//    Stage 2 — Fill in the Pkt fields and handle incoming CHAT packets.
//    Stage 3 — Network link quality & RSSI proximity tracking.
// =============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "common/board_pins.h"
#include "common/dedupe.h"
#include "common/led.h"
#include "common/palette.h"
#include "common/protocol.h"
#include "common/radio.h"
#include "common/web.h"
#include "config.h"

// Implemented here; declared in web.h — called by the web module when
// the browser POSTs a message.
void sendChatMessage(const char *text);

// ----- Global state ----------------------------------------
static uint16_t g_seq = 0;
static float g_filtered_rssi = -100.0f;
static uint32_t g_last_beacon_ms = 0;

// Indoor FSPL distance estimation model
static float estimateDistanceCm(float rssi) {
  return 100.0f * pow(10.0f, (RSSI_REF_1M - rssi) / RSSI_PATH_LOSS_EXP);
}

// ----- Packet receive callback (called from radioLoop) ------
static void onPktRecv(const Pkt *pkt, int8_t rssi) {
  // ---------- System beacon / Network alert -------------------
  if (pkt->type == (uint8_t)PktType::SYS_SYNC ||
      pkt->type == (uint8_t)PktType::SYS_ALERT) {
    // TODO(stage3): Update g_filtered_rssi (EMA) and
    // g_last_beacon_ms.

    if (pkt->type == (uint8_t)PktType::SYS_ALERT && pkt->len > 0) {
      // Show alert message with highlighted styling
      char sender[28];
      if (pkt->node_id == 99) {
        // Network anomaly signature
        static const uint8_t enc[] = {0x41, 0x67, 0x65, 0x6e, 0x74, 0x20,
                                      0x53, 0x6d, 0x69, 0x74, 0x68, 0x00};
        snprintf(sender, sizeof(sender), "%s", (const char *)enc);
      } else if (pkt->name[0] != '\0') {
        snprintf(sender, sizeof(sender), "%s", pkt->name);
      } else {
        snprintf(sender, sizeof(sender), "Node %u", pkt->node_id);
      }
      webAddMessage(pkt->node_id, sender, COLOR_ALERT.r, COLOR_ALERT.g,
                    COLOR_ALERT.b, pkt->text, /*is_alert=*/true);
      Serial.printf("[ALERT] %s: %s (RSSI: %d dBm)\n", sender, pkt->text, rssi);
    }
    return;
  }

  // ---------- Normal chat from another node ---------------
  if (pkt->type != (uint8_t)PktType::CHAT) {
    return; // unknown type — discard
  }

  // TODO(stage2): Check dedupeSeen(); if new, show the message
  // and flash the LED.
}

// ----- Called by web.cpp when the browser sends a message ---
void sendChatMessage(const char *text) {
  Pkt pkt = {};

  // TODO(stage2): Fill in every Pkt field, then call
  // radioSend(&pkt).

  // Echo to our own chat log (own messages don't come back via ESP-NOW).
  webAddMessage(NODE_ID, NODE_NAME, nodeColor(NODE_ID % N_COLORS).r,
                nodeColor(NODE_ID % N_COLORS).g,
                nodeColor(NODE_ID % N_COLORS).b, text, /*is_alert=*/false);

  Serial.printf("[me] %s: %s\n", NODE_NAME, text);
}

// ----- Serial command handler --------------------------------
static void handleSerial() {
  if (!Serial.available())
    return;
  String line = Serial.readStringUntil('\n');
  line.trim();

  if (line == "/id") {
    Serial.printf("NODE_ID=%u  NAME=%s  MAC=%s\n", NODE_ID, NODE_NAME,
                  WiFi.macAddress().c_str());
  } else if (line == "/rssi") {
    Serial.printf("rssi_f=%.1f  dist_cm=%.1f\n", g_filtered_rssi,
                  estimateDistanceCm(g_filtered_rssi));
  } else if (line == "/state") {
    float dist = estimateDistanceCm(g_filtered_rssi);
    Serial.printf("Proximity dist: %.1f cm (RSSI: %.1f)\n", dist,
                  g_filtered_rssi);
  } else if (line == "/wifi") {
    wifi_config_t conf = {};
    esp_wifi_get_config(WIFI_IF_AP, &conf);
    int8_t pwr = 0;
    esp_wifi_get_max_tx_power(&pwr);
    uint8_t proto = 0;
    esp_wifi_get_protocol(WIFI_IF_AP, &proto);
    Serial.printf("AP SSID: '%s', hidden: %d, ch: %d, auth: %d\n",
                  (char*)conf.ap.ssid, conf.ap.ssid_hidden, conf.ap.channel, conf.ap.authmode);
    Serial.printf("IP: %s, TX power: %d, proto: 0x%02X, stations: %d\n",
                  WiFi.softAPIP().toString().c_str(), pwr, proto, WiFi.softAPgetStationNum());
  } else if (line.length() > 0 && line[0] != '/') {
    // Plain text → send as chat.
    sendChatMessage(line.c_str());
  } else {
    Serial.println("Commands: /id /rssi /state /wifi  or type a message");
  }
}

// ----- setup() ----------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1500); // give USB CDC time to attach
  Serial.printf("\n=== Matrix Chat Node %u (%s) ===\n", NODE_ID, NODE_NAME);

  // TODO(stage1): Call ledInit(NODE_ID) to set up the RGB LED
  // for your node color.

  dedupeInit();
  // radioInit sets WiFi mode AP+STA, channel, and max TX power before esp_now_init.
  radioInit(CHANNEL, onPktRecv);

  // SoftAP and web server start after radio so they share the channel.
  webBegin(NODE_ID, NODE_NAME);

  Serial.printf("MAC:      %s\n", WiFi.macAddress().c_str());
  Serial.printf("AP SSID:  %s  password: %s\n", NODE_NAME, WIFI_AP_PASSWORD);
  Serial.printf("Chat URL: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("Serial: type a message or /id /rssi /state");
}

// ----- loop() -----------------------------------------------
void loop() {
  radioLoop();
  webLoop();

  // TODO(stage3): Calculate distance and call
  // ledSetRadarState/webSetRadarStatus.

  ledLoop(millis());
  handleSerial();
}
