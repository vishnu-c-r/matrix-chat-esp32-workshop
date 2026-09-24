// =============================================================
//  led.cpp — Non-blocking LED driver (LEDC + NeoPixel).
//
//  NOTE: ledcSetup/ledcAttachPin are deprecated in Arduino-ESP32 3.x.
//  We use the new ledcAttach(pin, freq, bits) + ledcWrite(pin, duty) API.
// =============================================================
#include "led.h"
#include "board_pins.h"
#include <Arduino.h>
#include <math.h>

// ---- Gamma correction table (γ ≈ 2.0) ----------------------
static uint8_t gamma8(uint8_t v)
{
    return (uint8_t)((uint32_t)v * v / 255u);
}

// ---- Low-level output ---------------------------------------
static void setRgb(uint8_t r, uint8_t g, uint8_t b)
{
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    // Onboard WS2812 — gamma applied inside rgbLedWrite is vendor-specific;
    // we apply our own gamma before calling so output is consistent.
    rgbLedWrite(PIN_NEOPIXEL, gamma8(r), gamma8(g), gamma8(b));
#else
    // External common-cathode RGB via LEDC (Arduino-ESP32 3.x API).
    ledcWrite(PIN_LED_R, gamma8(r));
    ledcWrite(PIN_LED_G, gamma8(g));
    ledcWrite(PIN_LED_B, gamma8(b));
#endif
}

// ---- State machine ------------------------------------------
// ---- State machine ------------------------------------------
enum class LedState : uint8_t { IDLE, MSG_FLASH, RADAR_NEAR, RADAR_CLOSE };

static LedState g_state      = LedState::IDLE;
static Color    g_node_color = {0, 255, 128};  // default: mint

static uint32_t g_flash_start  = 0;
static Color    g_flash_color  = {0, 255, 128};

static uint32_t g_blink_last   = 0;
static bool     g_blink_on     = false;
static uint32_t g_blink_half   = 600;  // half-period in ms

// Map filtered distance to blink half-period: 300 cm → 600 ms, 100 cm → 50 ms.
static uint32_t distToHalfPeriod(float dist_cm)
{
    float t = (300.0f - dist_cm) / (300.0f - 100.0f);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (uint32_t)(600.0f - t * 550.0f);  // 600 ms → 50 ms
}

// ---- Public API ---------------------------------------------
void ledInit(uint8_t node_id)
{
    g_node_color = nodeColor(node_id % N_COLORS);

#if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3)
    // Arduino-ESP32 3.x LEDC API: attach pin, then write duty by pin.
    ledcAttach(PIN_LED_R, 5000, 8);
    ledcAttach(PIN_LED_G, 5000, 8);
    ledcAttach(PIN_LED_B, 5000, 8);
#endif

    setRgb(0, 0, 0);  // off until first loop tick
}

void ledFlashMsg(Color c)
{
    // Only override if not currently showing a higher-priority state.
    if (g_state == LedState::IDLE || g_state == LedState::MSG_FLASH)
    {
        g_flash_color = c;
        g_flash_start = millis();
        g_state       = LedState::MSG_FLASH;
    }
}

void ledSetRadarState(uint8_t state, float dist_cm)
{
    if (state == 2)
    {
        g_state = LedState::RADAR_CLOSE;
    }
    else if (state == 1)
    {
        if (g_state != LedState::RADAR_NEAR)
        {
            g_blink_last = millis();
            g_blink_on   = true;
            g_state      = LedState::RADAR_NEAR;
        }
        g_blink_half = distToHalfPeriod(dist_cm);
    }
    else
    {
        // CLEAR — fall back to idle unless a flash is in progress.
        if (g_state == LedState::RADAR_NEAR || g_state == LedState::RADAR_CLOSE)
        {
            g_state = LedState::IDLE;
        }
    }
}

void ledLoop(uint32_t now_ms)
{
    switch (g_state)
    {
        // ---- Threat very close: solid red -------------------
        case LedState::RADAR_CLOSE:
            setRgb(COLOR_ALERT.r, COLOR_ALERT.g, COLOR_ALERT.b);
            break;

        // ---- Threat nearby: blink alert red at mapped period ------
        case LedState::RADAR_NEAR:
            if ((now_ms - g_blink_last) >= g_blink_half)
            {
                g_blink_on   = !g_blink_on;
                g_blink_last = now_ms;
            }
            if (g_blink_on)
            {
                setRgb(COLOR_ALERT.r, COLOR_ALERT.g, COLOR_ALERT.b);
            }
            else
            {
                setRgb(0, 0, 0);
            }
            break;

        // ---- 300 ms flash on message receive ---------------
        case LedState::MSG_FLASH:
            if ((now_ms - g_flash_start) >= 300u)
            {
                g_state = LedState::IDLE;
            }
            else
            {
                setRgb(g_flash_color.r, g_flash_color.g, g_flash_color.b);
            }
            break;

        // ---- Idle: slow breathing in own color -------------
        case LedState::IDLE:
        {
            // Brightness oscillates between 5% and 25%, period ~3 s.
            float phase = (float)(now_ms % 3000u) / 3000.0f * 6.2832f;
            float br    = 0.05f + 0.20f * (0.5f + 0.5f * sinf(phase));
            setRgb(
                (uint8_t)((float)g_node_color.r * br),
                (uint8_t)((float)g_node_color.g * br),
                (uint8_t)((float)g_node_color.b * br)
            );
            break;
        }
    }
}
