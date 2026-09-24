// =============================================================
//  config.h — THE ONLY FILE PARTICIPANTS NEED TO EDIT
//  Change NODE_ID and NODE_NAME before flashing your board.
// =============================================================
#pragma once

// Your team number (1-50).  Must be unique in the room.
#define NODE_ID 1

// Your team handle and Wi-Fi network name (SSID, max 20 chars).
// This is your board's Wi-Fi network name and your handle in chat!
#define NODE_NAME "Neo"

// Wi-Fi channel — set by the workshop instructor for the venue.
// Everyone in the room must use the same channel (default 1).
#define CHANNEL 1

// ---- RSSI proximity thresholds (dBm) -----------------------
// More negative = weaker signal = further away.
// Link proximity thresholds for diagnostic feedback.
#define RSSI_CLOSE_ENTER (-50)
#define RSSI_CLOSE_EXIT (-55)
#define RSSI_NEAR (-85)

// Reset link status after this many ms without beacon.
#define BEACON_TIMEOUT_MS 1500U
#define SMITH_TIMEOUT_MS BEACON_TIMEOUT_MS // Compatibility alias

// EMA smoothing factor for RSSI.  0 = frozen, 1 = raw, 0.2 = recommended.
#define RSSI_EMA_ALPHA 0.2f
