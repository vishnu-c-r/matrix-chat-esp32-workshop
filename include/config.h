// =============================================================
//  config.h — ALL WORKSHOP CONFIGURATION & PARAMETERS
//
//  Students edit:
//    - NODE_ID: unique team number (1-50)
//    - NODE_NAME: team display handle and SoftAP Wi-Fi name (SSID)
//
//  Instructors / Admins edit:
//    - Venue channel, Wi-Fi password, radar smoothing, etc.
// =============================================================
#pragma once
#include <stdint.h>

// =============================================================
//  1. PARTICIPANT IDENTITY (Students edit these)
// =============================================================

// Your team number (1-50). Must be unique in the room.
#define NODE_ID 1

// Your team handle and Wi-Fi network name (SSID, max 20 chars).
// This is your board's Wi-Fi network name and your handle in chat!
#define NODE_NAME "Neo"


// =============================================================
//  2. VENUE RADIO & WI-FI SETTINGS
// =============================================================

// Wi-Fi channel: 1, 6, or 11 recommended (standard non-overlapping channels).
// NOTE: Channels 12-14 are NOT supported by many iPhones/laptops.
// Everyone in the room (all nodes) MUST use the exact same channel!
#define CHANNEL 1

// Wi-Fi SoftAP password for smartphones connecting to nodes
// (Must be 8-63 ASCII characters for WPA2-PSK)
#define WIFI_AP_PASSWORD "matrix123"

// Wi-Fi transmit power in units of 0.25 dBm:
// 8 = 2 dBm (calibrated for room-scale proximity and ESP32-C3 stability)
// 40 = 10 dBm, 80 = 20 dBm (max)
#define WIFI_TX_POWER 8


// =============================================================
//  3. CHAT & WEB PORTAL CONFIGURATION
// =============================================================

// Number of previous messages stored in RAM ring buffer.
// Newly connected devices receive this history snapshot on load.
#define CHAT_HISTORY_SIZE 24

// Rate limit: minimum milliseconds between outgoing messages per client
#define CHAT_RATE_LIMIT_MS 1000U

// Maximum chat message length (characters)
#define CHAT_MAX_MSG_LEN 150


// =============================================================
//  4. RADAR & PROXIMITY TRACKING TUNING
// =============================================================

// Link proximity thresholds (dBm). More negative = weaker = further away.
#define RSSI_CLOSE_ENTER (-50)
#define RSSI_CLOSE_EXIT  (-55)
#define RSSI_NEAR        (-85)

// Reset radar status after this many ms without receiving a beacon
#define BEACON_TIMEOUT_MS 2500U

// Fast-attack, smooth-decay EMA filter smoothing factors:
// RSSI_EMA_ALPHA_FAST is used when signal rises (threat approaching, fast response)
// RSSI_EMA_ALPHA is used when signal drops or holds (steady smoothing)
#define RSSI_EMA_ALPHA      0.25f
#define RSSI_EMA_ALPHA_FAST 0.50f

// Indoor FSPL distance estimation model:
// dist_cm = 100 * 10^((REF_1M - RSSI) / PATH_LOSS_EXP)
#define RSSI_REF_1M        (-45.0f) // Calibrated RSSI at 1 meter distance
#define RSSI_PATH_LOSS_EXP (30.0f)  // 10 * N, where N=3.0 (indoor office/hall)

// Frequency of background radar synchronization beacons (ms)
#define RADAR_BEACON_INTERVAL_MS 400U
