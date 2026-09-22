// =============================================================
//  web.h — Captive-portal web chat server (Arduino WebServer).
//  Serves an inline HTML page; polls /api every second.
// =============================================================
#pragma once
#include <stdint.h>

// Start the web server and captive-portal DNS.
//   node_id   : 1-50 (used to name the SoftAP and colour messages)
//   node_name : human-readable name shown in chat
void webBegin(uint8_t node_id, const char* node_name);

// Call every loop iteration to service HTTP requests.
void webLoop();

// Add an incoming (or outgoing) chat message to the display buffer.
//   r,g,b    : sender colour (from palette)
//   is_smith : true = show with Smith glitch styling
void webAddMessage(uint8_t node_id, const char* name,
                   uint8_t r, uint8_t g, uint8_t b,
                   const char* text, bool is_smith);

// Update the Smith proximity indicator shown in the status bar.
//   state  : 0=CLEAR 1=NEAR 2=CLOSE  (SmithState cast to uint8_t)
//   rssi_f : filtered RSSI value
void webSetSmithStatus(uint8_t state, float rssi_f);

// Implemented in node/main.cpp — called when a browser sends a message.
void sendChatMessage(const char* text);
