// =============================================================
//  config.h — THE ONLY FILE STUDENTS NEED TO EDIT
//  Change NODE_ID and NODE_NAME before flashing your board.
// =============================================================
#pragma once

// Your team number (1-50).  Must be unique in the room.
#define NODE_ID   1

// Your handle in the chat (max 20 chars).
#define NODE_NAME "Neo"

// Wi-Fi channel — set by the workshop instructor for the venue.
// Everyone in the room must use the same channel (default 1).
#define CHANNEL   1

// ---- RSSI proximity thresholds (dBm) -----------------------
// More negative = weaker signal = further away.
// Smith is CLOSE when filtered RSSI rises above RSSI_CLOSE_ENTER.
// It leaves CLOSE only when RSSI drops below RSSI_CLOSE_EXIT (hysteresis).
#define RSSI_CLOSE_ENTER  (-50)
#define RSSI_CLOSE_EXIT   (-55)
// Smith is NEAR when filtered RSSI is above RSSI_NEAR.
#define RSSI_NEAR         (-85)

// Declare Smith gone after this many ms without a beacon.
#define SMITH_TIMEOUT_MS  1500U

// EMA smoothing factor for RSSI.  0 = frozen, 1 = raw, 0.2 = recommended.
#define RSSI_EMA_ALPHA    0.2f
