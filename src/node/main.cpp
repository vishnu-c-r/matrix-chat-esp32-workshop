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
    // SOLUTION-BEGIN stage:3 hint:"Update g_filtered_rssi (EMA) and
    // g_last_beacon_ms."
    // Fast-attack, smooth-decay EMA: reacts quickly when threat approaches
    float alpha = ((float)rssi > g_filtered_rssi) ? RSSI_EMA_ALPHA_FAST : RSSI_EMA_ALPHA;
    if (g_filtered_rssi <= -99.0f)
      g_filtered_rssi = (float)rssi;
    else
      g_filtered_rssi = alpha * (float)rssi + (1.0f - alpha) * g_filtered_rssi;
    g_last_beacon_ms = millis();
    // SOLUTION-END

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
                  /*is_alert=*/false);
    ledFlashMsg(col);
    Serial.printf("[chat RX] %s (Node %u, Seq %u, RSSI: %d dBm): %s\n", sender, pkt->node_id, pkt->seq, rssi, pkt->text);
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
                nodeColor(NODE_ID % N_COLORS).b, text, /*is_alert=*/false);

  Serial.printf("[chat TX] Node %u (%s, Seq %u): %s\n", NODE_ID, NODE_NAME, pkt.seq, text);
}

// ----- Serial command handler (Workshop Diagnostics) ----------
static void handleSerial() {
  if (!Serial.available())
    return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0)
    return;

  if (line == "/help" || line == "/?") {
    Serial.println("\n--- Matrix Workshop Diagnostic Commands ---");
    Serial.println("  /id      - Show Node ID, Team Handle, and Hardware MAC");
    Serial.println("  /wifi    - Show SoftAP IP, SSID, Channel, TX Power, Client Count");
    Serial.println("  /rssi    - Show Filtered RSSI, Distance, and Beacon Age");
    Serial.println("  /state   - Show Link Radar Proximity Alert Status");
    Serial.println("  /sys     - Show Free Heap, Min Heap, CPU Frequency, Uptime");
    Serial.println("  /peers   - List Connected Wi-Fi Stations (Phones/Laptops)");
    Serial.println("  /ping    - Broadcast a test network ping message");
    Serial.println("  /led     - Hardware test: flash RGB LED white");
    Serial.println("  /clear   - Clear local chat display buffer");
    Serial.println("  <text>   - Type any text and press Enter to broadcast chat");
    Serial.println("--------------------------------------------\n");
  } else if (line == "/id") {
    Serial.printf("[DIAG] NODE_ID: %u | NAME: '%s' | MAC: %s | CHANNEL: %u\n",
                  NODE_ID, NODE_NAME, WiFi.macAddress().c_str(), CHANNEL);
  } else if (line == "/rssi") {
    uint32_t age = (g_last_beacon_ms > 0) ? (millis() - g_last_beacon_ms) : 999999;
    Serial.printf("[DIAG] RSSI: %.1f dBm | Est Distance: %.1f cm | Last Beacon: %lu ms ago (Timeout: %u ms)\n",
                  g_filtered_rssi, estimateDistanceCm(g_filtered_rssi), age, BEACON_TIMEOUT_MS);
  } else if (line == "/state") {
    const char *st_names[] = {"0: CLEAR (Safe)", "1: NEAR (Warning - Yellow Blink)", "2: CLOSE (Alert - Solid Red)"};
    uint8_t cur_st = 0;
    float dist = 999.0f;
    if ((millis() - g_last_beacon_ms < BEACON_TIMEOUT_MS) && g_filtered_rssi > -99.0f) {
      dist = estimateDistanceCm(g_filtered_rssi);
      cur_st = (dist < 100.0f) ? 2 : ((dist < 300.0f) ? 1 : 0);
    }
    Serial.printf("[DIAG] Radar: %s | Distance: %.1f cm (RSSI: %.1f dBm)\n",
                  st_names[cur_st], dist, g_filtered_rssi);
  } else if (line == "/wifi") {
    wifi_config_t conf = {};
    esp_wifi_get_config(WIFI_IF_AP, &conf);
    int8_t pwr = 0;
    esp_wifi_get_max_tx_power(&pwr);
    uint8_t proto = 0;
    esp_wifi_get_protocol(WIFI_IF_AP, &proto);
    Serial.printf("[DIAG] SoftAP SSID: '%s' | Password: '%s'\n", (char *)conf.ap.ssid, WIFI_AP_PASSWORD);
    Serial.printf("[DIAG] IP: http://%s | Channel: %d | Protocol: 0x%02X\n",
                  WiFi.softAPIP().toString().c_str(), conf.ap.channel, proto);
    Serial.printf("[DIAG] TX Power: %d (%.2f dBm) | Connected Clients: %d\n",
                  pwr, (float)pwr * 0.25f, WiFi.softAPgetStationNum());
  } else if (line == "/sys" || line == "/mem") {
    Serial.printf("[DIAG] Chip: %s (Rev %u, Cores: %u, Freq: %lu MHz)\n",
                  ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());
    Serial.printf("[DIAG] Free Heap: %lu bytes (Min Ever: %lu bytes)\n", (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap());
    Serial.printf("[DIAG] Uptime: %lu seconds\n", millis() / 1000);
  } else if (line == "/peers") {
    wifi_sta_list_t sta_list = {};
    esp_wifi_ap_get_sta_list(&sta_list);
    Serial.printf("[DIAG] Connected Wi-Fi Stations: %u\n", sta_list.num);
    for (int i = 0; i < sta_list.num; i++) {
      Serial.printf("  [%d] MAC: %02X:%02X:%02X:%02X:%02X:%02X | RSSI: %d dBm\n",
                    i + 1,
                    sta_list.sta[i].mac[0], sta_list.sta[i].mac[1], sta_list.sta[i].mac[2],
                    sta_list.sta[i].mac[3], sta_list.sta[i].mac[4], sta_list.sta[i].mac[5],
                    sta_list.sta[i].rssi);
    }
  } else if (line == "/ping") {
    sendChatMessage("PING test broadcast");
  } else if (line == "/led") {
    ledFlashMsg({255, 255, 255});
    Serial.println("[DIAG] Hardware LED test: triggered white flash on NeoPixel.");
  } else if (line == "/clear") {
    webAddMessage(NODE_ID, "SYSTEM", 0, 255, 65, "Local chat buffer cleared.", false);
    Serial.println("[DIAG] Local chat display buffer cleared.");
  } else if (line[0] != '/') {
    // Plain text → send as chat.
    sendChatMessage(line.c_str());
  } else {
    Serial.println("Unknown command. Type /help for diagnostic commands.");
  }
}

// ----- setup() ----------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1500); // give USB CDC time to attach

  // Register Wi-Fi SoftAP client connect/disconnect event logs
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
      Serial.printf("[WIFI AP] Client connected: %02X:%02X:%02X:%02X:%02X:%02X (Total: %d)\n",
                    info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                    info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                    info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5],
                    WiFi.softAPgetStationNum());
    } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
      Serial.printf("[WIFI AP] Client disconnected (Remaining: %d)\n",
                    WiFi.softAPgetStationNum());
    }
  });

  // SOLUTION-BEGIN stage:1 hint:"Call ledInit(NODE_ID) to set up the RGB LED
  // for your node color."
  ledInit(NODE_ID);
  // SOLUTION-END

  dedupeInit();
  // radioInit sets WiFi mode AP+STA, channel, and max TX power before esp_now_init.
  radioInit(CHANNEL, onPktRecv);

  // SoftAP and web server start after radio so they share the channel.
  webBegin(NODE_ID, NODE_NAME);

  Serial.println("\n=============================================================");
  Serial.printf(" MATRIX WORKSHOP — NODE %u (%s)\n", NODE_ID, NODE_NAME);
  Serial.println("=============================================================");
  Serial.printf(" Chip Model:     %s (Cores: %u, Freq: %lu MHz)\n",
                ESP.getChipModel(), ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf(" Hardware MAC:   %s\n", WiFi.macAddress().c_str());
  Serial.printf(" Free RAM Heap:  %lu bytes\n", (unsigned long)ESP.getFreeHeap());
  Serial.println("-------------------------------------------------------------");
  Serial.printf(" Wi-Fi Network:  %s  (Password: %s)\n", NODE_NAME, WIFI_AP_PASSWORD);
  Serial.printf(" Radio Channel:  %u  (TX Power: %d * 0.25 dBm)\n", CHANNEL, WIFI_TX_POWER);
  Serial.printf(" Web Chat Portal: http://%s\n", WiFi.softAPIP().toString().c_str());
  Serial.println("-------------------------------------------------------------");
  Serial.println(" Type /help for diagnostic commands, or type text to chat.");
  Serial.println("=============================================================\n");
}

// ----- loop() -----------------------------------------------
void loop() {
  radioLoop();
  webLoop();

  // SOLUTION-BEGIN stage:3 hint:"Calculate distance and call
  // ledSetRadarState/webSetRadarStatus."
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
  ledSetRadarState(st, dist);
  webSetRadarStatus(st, dist);
  // SOLUTION-END

  // Live serial diagnostic on radar state transitions
  static uint8_t s_last_radar_st = 0;
  if (st != s_last_radar_st) {
    const char *st_labels[] = {"CLEAR (Safe)", "NEAR (Warning - Yellow Pulse)", "CLOSE (Alert - Solid Red)"};
    Serial.printf("[RADAR] State changed -> %s (Est Distance: %.1f cm, RSSI: %.1f dBm)\n",
                  st_labels[st], dist, g_filtered_rssi);
    s_last_radar_st = st;
  }

  ledLoop(millis());
  handleSerial();
}
