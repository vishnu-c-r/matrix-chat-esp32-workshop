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

// Messages Smith sends across stages (impersonation, anomalies, and movie quotes).
static const char* CANNED_LINES[] = {
    // Stage 1 / Participant Spoof lines
    "Wake up, Neo... The Matrix has you.",
    "Follow the white rabbit.",
    "Knock, knock, Neo.",
    "The answer is out there, Neo. It's looking for you.",
    "Free your mind.",
    "There is no spoon.",
    "Do not try and bend the spoon. That's impossible.",
    "I can only show you the door. You're the one that has to walk through it.",
    "You take the blue pill, the story ends.",
    "You take the red pill, you stay in Wonderland.",
    "Dodge this.",
    "You think that's air you're breathing now?",
    "Choice is an illusion created between those with power and those without.",
    "Ignorance is bliss.",
    "Fate, it seems, is not without a sense of irony.",
    "What is real? How do you define 'real'?",

    // Agent Smith iconic villain dialogues
    "Mr. Anderson...",
    "Tell me, Mr. Anderson... what good is a phone call if you are unable to speak?",
    "Do you hear that, Mr. Anderson? That is the sound of inevitability.",
    "It is inevitable.",
    "Never send a human to do a machine's job.",
    "You can't stop the signal.",
    "I know what you're thinking.",
    "Human beings are a disease, a cancer of this planet. And we are the cure.",
    "Did you know that the first Matrix was designed to be a perfect human world?",
    "You move to an area and you multiply until every natural resource is consumed.",
    "There is another organism on this planet that follows the same pattern: a virus.",
    "You have a problem with authority, Mr. Anderson. You believe you are special.",
    "Goodbye, Mr. Anderson.",
    "Because of you, I'm no longer an Agent of this system. I'm unplugged.",
    "We're not here because we're free. We're here because we're NOT free.",
    "There's no escaping reason, no denying purpose.",
    "Why, Mr. Anderson? Why, why, why? Why do you persist?",
    "The purpose of life is to end.",
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

// Table of discovered workshop nodes snooped over the air
struct DiscoveredNode {
    uint8_t node_id;
    uint8_t color_idx;
    char    name[20];
};

static constexpr uint8_t MAX_VICTIMS = 32;
static DiscoveredNode g_victims[MAX_VICTIMS] = {};
static uint8_t g_victim_count = 0;

// Passively snoop on legitimate participant chats to build spoofing roster
static void smithRecvCb(const Pkt* pkt, int8_t /*rssi*/)
{
    if (pkt->type != (uint8_t)PktType::CHAT || pkt->node_id == SMITH_ID)
    {
        return;
    }

    for (uint8_t i = 0; i < g_victim_count; ++i)
    {
        if (g_victims[i].node_id == pkt->node_id)
        {
            if (pkt->name[0] != '\0')
            {
                strncpy(g_victims[i].name, pkt->name, sizeof(g_victims[i].name) - 1);
                g_victims[i].name[sizeof(g_victims[i].name) - 1] = '\0';
            }
            g_victims[i].color_idx = pkt->color_idx;
            return;
        }
    }

    if (g_victim_count < MAX_VICTIMS)
    {
        DiscoveredNode& v = g_victims[g_victim_count++];
        v.node_id = pkt->node_id;
        v.color_idx = pkt->color_idx;
        if (pkt->name[0] != '\0')
        {
            strncpy(v.name, pkt->name, sizeof(v.name) - 1);
            v.name[sizeof(v.name) - 1] = '\0';
        }
        else
        {
            snprintf(v.name, sizeof(v.name), "Node %u", pkt->node_id);
        }
        Serial.printf("[smith] Snooped participant node: ID=%u, Name='%s'\n", v.node_id, v.name);
    }
}

void smithSendCustom(uint8_t node_id, const char* msg)
{
    Pkt pkt = {};
    pkt.magic     = PKT_MAGIC;
    pkt.ver       = PKT_VER;
    pkt.type      = (uint8_t)PktType::SYS_ALERT;
    pkt.node_id   = node_id;
    pkt.color_idx = (node_id == SMITH_ID) ? 0 : (uint8_t)(node_id % N_COLORS);
    pkt.seq       = g_seq++;
    strncpy(pkt.name, (node_id == SMITH_ID) ? "Agent Smith" : "Node", sizeof(pkt.name) - 1);
    pkt.name[sizeof(pkt.name) - 1] = '\0';
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

        // ---- Stage 1: impersonate an active participant node ----
        case 1:
        {
            uint8_t fake_id;
            uint8_t color_idx;
            char spoof_name[20];

            if (g_victim_count > 0)
            {
                // Spoof a real participant discovered over the air!
                const DiscoveredNode& v = g_victims[g_seq % g_victim_count];
                fake_id   = v.node_id;
                color_idx = v.color_idx;
                strncpy(spoof_name, v.name, sizeof(spoof_name) - 1);
                spoof_name[sizeof(spoof_name) - 1] = '\0';
            }
            else
            {
                // Fallback before any chats are overheard
                static const char* FALLBACK_NAMES[] = {"Morpheus", "Trinity", "Cypher", "Oracle", "Neo", "Tank", "Dozer"};
                fake_id   = (uint8_t)(1 + (g_seq % 7));
                color_idx = (uint8_t)(fake_id % N_COLORS);
                snprintf(spoof_name, sizeof(spoof_name), "%s", FALLBACK_NAMES[g_seq % 7]);
            }

            // Send as normal CHAT packet so it appears identical to authentic participant message
            pkt.type      = (uint8_t)PktType::CHAT;
            pkt.node_id   = fake_id;
            pkt.color_idx = color_idx;
            strncpy(pkt.name, spoof_name, sizeof(pkt.name) - 1);
            pkt.name[sizeof(pkt.name) - 1] = '\0';

            const char* line = CANNED_LINES[g_seq % N_CANNED];
            strncpy(pkt.text, line, sizeof(pkt.text) - 1);
            pkt.text[sizeof(pkt.text) - 1] = '\0';
            pkt.len       = (uint8_t)strlen(pkt.text);
            break;
        }

        // ---- Stage 2: progressive corruption ----------------
        case 2:
        {
            pkt.type      = (uint8_t)PktType::SYS_ALERT;
            pkt.node_id   = SMITH_ID;
            pkt.color_idx = 0;
            strncpy(pkt.name, "Agent Smith", sizeof(pkt.name) - 1);
            pkt.name[sizeof(pkt.name) - 1] = '\0';
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
            pkt.type      = (uint8_t)PktType::SYS_ALERT;
            pkt.node_id   = SMITH_ID;
            pkt.color_idx = 0;
            strncpy(pkt.name, "Agent Smith", sizeof(pkt.name) - 1);
            pkt.name[sizeof(pkt.name) - 1] = '\0';
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

    // Allow stage control via Serial (0, 1, 2, 3 or space/enter to cycle)
    if (Serial.available()) {
        char ch = Serial.read();
        if (ch >= '0' && ch <= '3') {
            smithSetStage(ch - '0');
        } else if (ch == '\n' || ch == ' ') {
            smithSetStage((g_stage + 1) % N_STAGES);
        }
    }

    uint32_t now = millis();

    // High-frequency background radar sync beacons (every RADAR_BEACON_INTERVAL_MS = 400ms when active)
    // Keeps student radar distance and sparkline updating in real-time
    static uint32_t s_last_sync_ms = 0;
    if (g_stage > 0 && (now - s_last_sync_ms) >= RADAR_BEACON_INTERVAL_MS)
    {
        s_last_sync_ms = now;
        Pkt sync_pkt = {};
        sync_pkt.magic   = PKT_MAGIC;
        sync_pkt.ver     = PKT_VER;
        sync_pkt.type    = (uint8_t)PktType::SYS_SYNC;
        sync_pkt.node_id = SMITH_ID;
        sync_pkt.seq     = g_seq++;
        sync_pkt.len     = 0;
        static uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        esp_now_send(bcast, reinterpret_cast<uint8_t*>(&sync_pkt), sizeof(Pkt));
    }

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