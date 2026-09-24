// =============================================================
//  src/smith/main.cpp — Agent Smith rogue beacon firmware.
//
//  Press the BOOT button to advance through four stages:
//    0: Silent beacons only (SMITH_BEACON every 100 ms)
//    1: Impersonate a random node with a canned chat line
//    2: Progressive text corruption (levels 1-5 as time passes)
//    3: Flood at 5 msg/s using Smith red color
//
//  LED (or NeoPixel) shows current stage: off / blue / yellow / red.
//  TX power is set low (~2 dBm) so distance affects RSSI meaningfully.
//
//  No web server, no serial input needed — battery-friendly.
// =============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

#include "config.h"
#include "common/protocol.h"
#include "common/board_pins.h"
#include "common/palette.h"
#include "corrupt.h"
#include "common/radio.h"
#include "common/led.h"
#include "smith_web.h"

// ----- Stage configuration ----------------------------------
static constexpr uint8_t N_STAGES = 4;

// Smith's node ID — well outside the 1-50 student range.
static constexpr uint8_t SMITH_ID = 99;

// Beacon interval per stage (ms).
static const uint32_t STAGE_INTERVAL_MS[N_STAGES] = {
    500,   // 0: beacon only
    2000,  // 1: impersonation chat
    1500,  // 2: corrupted chat
    1000,  // 3: flood (slowed down)
};

// Messages Smith sends in stage 1 (impersonation).
static const char* CANNED_LINES[] = {
    "I'm inevitable.",
    "Mr. Anderson...",
    "Never send a human to do a machine's job.",
    "You can't stop the signal.",
    "I know what you're thinking.",
    "The answer is out there, Neo.",
    "Free your mind.",
    "There is no spoon.",
    "Dodge this.",
    "You think that's air you're breathing?",
};
static constexpr uint8_t N_CANNED = sizeof(CANNED_LINES) / sizeof(CANNED_LINES[0]);

// ----- State -------------------------------------------------
static uint8_t   g_stage     = 0;
static uint16_t  g_seq       = 0;
static uint32_t  g_last_ms   = 0;
static uint32_t  g_stage_start_ms = 0;

// Stage indicator colors (shown on the Smith board's LED).
static const Color STAGE_COLORS[N_STAGES] = {
    {0,   0,   0},    // 0: off
    {0,   0, 255},    // 1: blue
    {255, 200,  0},   // 2: yellow
    {255,   0,  0},   // 3: red
};

// ----- BOOT button debounce ---------------------------------
static bool     g_btn_prev   = true;
static uint32_t g_btn_ms     = 0;

static void checkButton()
{
    bool pressed = (digitalRead(PIN_BOOT) == LOW);
    uint32_t now = millis();
    if (pressed && !g_btn_prev && (now - g_btn_ms) > 200)
    {
        g_stage          = (g_stage + 1) % N_STAGES;
        g_stage_start_ms = now;
        g_btn_ms         = now;

        // Show stage color briefly at full brightness.
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
        if (HAS_NEOPIXEL) {
            const Color& c = STAGE_COLORS[g_stage];
            rgbLedWrite(PIN_NEOPIXEL, c.r, c.g, c.b);
        }
#endif
        Serial.printf("[smith] stage -> %u\n", g_stage);
    }
    g_btn_prev = pressed;
}

// ----- Web Server API ---------------------------------------
uint8_t smithGetStage()
{
    return g_stage;
}

void smithSetStage(uint8_t stage)
{
    if (stage < N_STAGES)
    {
        g_stage = stage;
        g_stage_start_ms = millis();
        Serial.printf("[smith] Web changed stage -> %u\n", g_stage);
        
        const Color& c = STAGE_COLORS[g_stage];
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
        rgbLedWrite(PIN_NEOPIXEL, c.r / 3, c.g / 3, c.b / 3);
#else
        ledcWrite(PIN_LED_R, c.r / 4);
        ledcWrite(PIN_LED_G, c.g / 4);
        ledcWrite(PIN_LED_B, c.b / 4);
#endif
    }
}

void smithSendCustom(uint8_t node_id, const char* msg)
{
    Pkt pkt = {};
    pkt.magic     = PKT_MAGIC;
    pkt.ver       = PKT_VER;
    pkt.type      = (uint8_t)PktType::SMITH_CHAT;
    pkt.node_id   = node_id;
    pkt.color_idx = (node_id == SMITH_ID) ? 0 : (uint8_t)(node_id % N_COLORS);
    pkt.seq       = g_seq++;
    strncpy(pkt.text, msg, sizeof(pkt.text) - 1);
    pkt.text[sizeof(pkt.text) - 1] = '\0';
    pkt.len       = (uint8_t)strlen(pkt.text);

    static uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(bcast, reinterpret_cast<uint8_t*>(&pkt), sizeof(Pkt));
    Serial.printf("[smith] Injected custom message as %u: %s\n", node_id, msg);
}

// ----- Build and send a beacon / chat packet ----------------
static void sendBeacon()
{
    Pkt pkt = {};
    pkt.magic  = PKT_MAGIC;
    pkt.ver    = PKT_VER;
    pkt.seq    = g_seq++;

    switch (g_stage)
    {
        // ---- Stage 0: completely silent (undetectable) ------
        case 0:
            return; // Don't send any packets at all!

        // ---- Stage 1: impersonate a random node -------------
        case 1:
        {
            uint8_t fake_id  = (uint8_t)(1 + (g_seq % 10));   // pretend to be node 1-10
            pkt.type         = (uint8_t)PktType::SMITH_CHAT;
            pkt.node_id      = fake_id;
            pkt.color_idx    = (uint8_t)(fake_id % N_COLORS);
            const char* line = CANNED_LINES[g_seq % N_CANNED];
            strncpy(pkt.text, line, sizeof(pkt.text) - 1);
            pkt.text[sizeof(pkt.text) - 1] = '\0';
            pkt.len          = (uint8_t)strlen(pkt.text);
            break;
        }

        // ---- Stage 2: progressive corruption ----------------
        case 2:
        {
            pkt.type      = (uint8_t)PktType::SMITH_CHAT;
            pkt.node_id   = SMITH_ID;
            pkt.color_idx = 0;
            // Corruption level rises from 1 to 5 over the first 50 packets.
            int level = 1 + (int)((millis() - g_stage_start_ms) / 8000);
            if (level > 5) level = 5;
            const char* base = CANNED_LINES[g_seq % N_CANNED];
            strncpy(pkt.text, base, sizeof(pkt.text) - 1);
            pkt.text[sizeof(pkt.text) - 1] = '\0';
            corrupt(pkt.text, level, (uint32_t)g_seq);
            pkt.len = (uint8_t)strlen(pkt.text);
            break;
        }

        // ---- Stage 3: flood with Smith red ------------------
        case 3:
            pkt.type      = (uint8_t)PktType::SMITH_CHAT;
            pkt.node_id   = SMITH_ID;
            pkt.color_idx = 0;
            {
                const char* base = CANNED_LINES[g_seq % N_CANNED];
                strncpy(pkt.text, base, sizeof(pkt.text) - 1);
                pkt.text[sizeof(pkt.text) - 1] = '\0';
                corrupt(pkt.text, 5, (uint32_t)g_seq);
                pkt.len = (uint8_t)strlen(pkt.text);
            }
            break;
    }

    // For Smith we call esp_now_send directly (bypass retransmit scheduler).
    static uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(bcast, reinterpret_cast<uint8_t*>(&pkt), sizeof(Pkt));
}

// ----- LED update for Smith board ---------------------------
static void updateStageLed()
{
    const Color& c = STAGE_COLORS[g_stage];
    // Smith has no node LED state machine — just show stage color directly.
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    rgbLedWrite(PIN_NEOPIXEL, c.r / 3, c.g / 3, c.b / 3);  // dim
#else
    // LEDC direct write for WROOM/C3 stage indicator.
    ledcWrite(PIN_LED_R, c.r / 4);
    ledcWrite(PIN_LED_G, c.g / 4);
    ledcWrite(PIN_LED_B, c.b / 4);
#endif
}

// ----- Null receive callback (Smith doesn't read chat) ------
static void smithRecvCb(const Pkt*, int8_t) {}

// ----- setup() ----------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Agent Smith firmware ===");
    Serial.println("Press BOOT to advance stages (0→1→2→3→0)");

    pinMode(PIN_BOOT, INPUT_PULLUP);

#if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3)
    ledcAttach(PIN_LED_R, 5000, 8);
    ledcAttach(PIN_LED_G, 5000, 8);
    ledcAttach(PIN_LED_B, 5000, 8);
#endif

    // Low TX power (~2 dBm) makes RSSI distance-sensitive in the workshop.
    // esp_wifi_set_max_tx_power takes units of 0.25 dBm; 8 = 2 dBm.
    // MUST be called after WiFi.mode() but we set it again after init.
    radioInit(CHANNEL, smithRecvCb);
    esp_wifi_set_max_tx_power(8);  // 2 dBm — hardware validation needed

    smithWebBegin();

    Serial.printf("MAC: %s  channel: %u\n", WiFi.macAddress().c_str(), CHANNEL);
    g_stage_start_ms = millis();
}

// ----- loop() -----------------------------------------------
void loop()
{
    checkButton();

    uint32_t now = millis();
    if ((now - g_last_ms) >= STAGE_INTERVAL_MS[g_stage])
    {
        g_last_ms = now;
        sendBeacon();
        updateStageLed();

        if (g_stage > 0)
        {
            Serial.printf("[smith] stage=%u seq=%u\n", g_stage, g_seq);
        }
    }

    // radioLoop drains the (ignored) receive queue.
    radioLoop();
    smithWebLoop();
}