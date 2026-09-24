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

// Indoor FSPL distance estimation (Path Loss Exponent N=3.0)
// Tuned for low TX power: -45 dBm reference at 1 meter.
static float estimateDistanceCm(float rssi) {
  return 100.0f * pow(10.0f, (-45.0f - rssi) / 30.0f);
}

// ----- Packet receive callback (called from radioLoop) ------
static void onPktRecv(const Pkt *pkt, int8_t rssi) {
  // ---------- System beacon / Network alert -------------------
  if (pkt->type == (uint8_t)PktType::SYS_SYNC ||
      pkt->type == (uint8_t)PktType::SYS_ALERT) {
    // SOLUTION-BEGIN stage:3 hint:"Update g_filtered_rssi (EMA) and
    // g_last_beacon_ms."
    if (g_filtered_rssi == -100.0f)
      g_filtered_rssi = (float)rssi;
    else
      g_filtered_rssi = 0.2f * (float)rssi + 0.8f * g_filtered_rssi;
    g_last_beacon_ms = millis();
    // SOLUTION-END

    if (pkt->type == (uint8_t)PktType::SYS_ALERT && pkt->len > 0) {
      // Show alert message with highlighted styling
      char sender[28];
      if (pkt->node_id == 99) {
        // System anomaly signature: "Agent Smith"
        static const uint8_t enc[] = {0x41, 0x67, 0x65, 0x6e, 0x74, 0x20,
                                      0x53, 0x6d, 0x69, 0x74, 0x68, 0x00};
        snprintf(sender, sizeof(sender), "%s", (const char *)enc);
      } else {
        snprintf(sender, sizeof(sender), "Node %u", pkt->node_id);
      }
      webAddMessage(pkt->node_id, sender, COLOR_SMITH.r, COLOR_SMITH.g,
                    COLOR_SMITH.b, pkt->text, /*is_alert=*/true);
      Serial.printf("[ALERT] %s: %s (RSSI: %d dBm)\n", sender, pkt->text, rssi);
    }
    return;
  }

  // ---------- Normal chat from another node ---------------
  if (pkt->type != (uint8_t)PktType::CHAT) {
    return; // unknown type — discard
  }

  // SOLUTION-BEGIN stage:2 hint:"Check dedupeSeen(); if new, show the message
  // and flash the LED."
  if (!dedupeSeen(pkt->node_id, pkt->seq)) {
    Color col = nodeColor(pkt->color_idx);
    // Display in web UI: use sender's team name if provided, fallback to Node ID.
    char sender[28];
    if (pkt->name[0] != '\0') {
      snprintf(sender, sizeof(sender), "%s", pkt->name);
    } else {
      snprintf(sender, sizeof(sender), "Node %u", pkt->node_id);
    }
    webAddMessage(pkt->node_id, sender, col.r, col.g, col.b, pkt->text,
                  /*is_smith=*/false);
    ledFlashMsg(col);
    Serial.printf("[chat] %s: %s\n", sender, pkt->text);
  }
  // SOLUTION-END
}

// ----- Called by web.cpp when the browser sends a message ---
void sendChatMessage(const char *text) {
  Pkt pkt = {};

  // SOLUTION-BEGIN stage:2 hint:"Fill in every Pkt field, then call
  // radioSend(&pkt)."
  pkt.magic = PKT_MAGIC;
  pkt.ver = PKT_VER;
  pkt.type = (uint8_t)PktType::CHAT;
  pkt.node_id = NODE_ID;
  pkt.color_idx = (uint8_t)(NODE_ID % N_COLORS);
  pkt.seq = g_seq++;
  strncpy(pkt.name, NODE_NAME, sizeof(pkt.name) - 1);
  pkt.name[sizeof(pkt.name) - 1] = '\0';
  strncpy(pkt.text, text, sizeof(pkt.text) - 1);
  pkt.text[sizeof(pkt.text) - 1] = '\0';
  pkt.len = (uint8_t)strlen(pkt.text);
  radioSend(&pkt);
  // SOLUTION-END

  // Echo to our own chat log (own messages don't come back via ESP-NOW).
  webAddMessage(NODE_ID, NODE_NAME, nodeColor(NODE_ID % N_COLORS).r,
                nodeColor(NODE_ID % N_COLORS).g,
                nodeColor(NODE_ID % N_COLORS).b, text, /*is_smith=*/false);

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

  // SOLUTION-BEGIN stage:1 hint:"Call ledInit(NODE_ID) to set up the RGB LED
  // for your node color."
  ledInit(NODE_ID);
  // SOLUTION-END

  dedupeInit();
  // radioInit sets WiFi mode AP+STA and channel before esp_now_init.
  radioInit(CHANNEL, onPktRecv);
  esp_wifi_set_max_tx_power(8); // Match Smith low TX power (2 dBm)

  // SoftAP and web server start after radio so they share the channel.
  webBegin(NODE_ID, NODE_NAME);

  Serial.printf("MAC:      %s\n", WiFi.macAddress().c_str());
  Serial.printf("AP SSID:  %s  password: matrix123\n", NODE_NAME);
  Serial.printf("Chat URL: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("Serial: type a message or /id /rssi /state");
}

// ----- loop() -----------------------------------------------
void loop() {
  radioLoop();
  webLoop();

  // SOLUTION-BEGIN stage:3 hint:"Calculate distance and call
  // ledSetSmithState/webSetSmithStatus."
  uint32_t now = millis();
  uint8_t st = 0;
  float dist = 999.0f;
  if (now - g_last_beacon_ms < BEACON_TIMEOUT_MS && g_filtered_rssi > -99.0f) {
    dist = estimateDistanceCm(g_filtered_rssi);
    if (dist < 100.0f)
      st = 2; // CLOSE
    else if (dist < 300.0f)
      st = 1; // NEAR
  } else {
    g_filtered_rssi = -100.0f;
  }
  ledSetSmithState(st, dist);
  webSetSmithStatus(st, dist);
  // SOLUTION-END

  ledLoop(millis());
  handleSerial();
}
