// =============================================================
//  radio.h — ESP-NOW broadcast send/receive with FreeRTOS queue.
// =============================================================
#pragma once
#include "protocol.h"

// Called by radioLoop() for every validated incoming packet.
typedef void (*PktRecvCb)(const Pkt* pkt, int8_t rssi);

// Initialise WiFi (AP+STA), set channel, start ESP-NOW.
// Adds broadcast peer on WIFI_IF_AP (falls back to WIFI_IF_STA if needed).
// cb is called from the main-loop context, never from the WiFi task.
void radioInit(uint8_t channel, PktRecvCb cb);

// Schedule a packet for 3 transmissions with 5-20 ms random jitter.
// Non-blocking: returns immediately; radioLoop() does the actual sending.
void radioSend(const Pkt* pkt);

// Drain the receive queue and service the retransmit scheduler.
// Call this every loop iteration.
void radioLoop();
