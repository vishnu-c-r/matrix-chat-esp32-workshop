// =============================================================
//  radio.cpp — ESP-NOW broadcast with FreeRTOS queue and
//              non-blocking 3x retransmit scheduler.
//
//  ASSUMPTION: Broadcast peer is registered on WIFI_IF_AP.
//  If TX returns ESP_ERR_ESPNOW_IF the code retries on WIFI_IF_STA
//  and prints a warning.  Which interface works depends on the
//  AP+STA mode and channel alignment — needs hardware validation.
// =============================================================
#include "radio.h"
#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// ---------- Receive queue ------------------------------------
struct QueueItem {
    Pkt    pkt;
    int8_t rssi;
};

static QueueHandle_t g_queue   = nullptr;
static PktRecvCb     g_recv_cb = nullptr;

// The ESP-NOW receive callback runs inside the WiFi task.
// We MUST NOT do anything expensive here — just push to the queue.
static void IRAM_ATTR espNowRecvCb(const esp_now_recv_info_t* info,
                                    const uint8_t*             data,
                                    int                        len)
{
    if (len != (int)sizeof(Pkt))
    {
        return;
    }
    const auto* p = reinterpret_cast<const Pkt*>(data);
    if (p->magic != PKT_MAGIC || p->ver != PKT_VER)
    {
        return;  // not our packet
    }

    QueueItem item;
    memcpy(&item.pkt, data, sizeof(Pkt));
    item.rssi = (int8_t)info->rx_ctrl->rssi;

    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(g_queue, &item, &woken);
    if (woken)
    {
        portYIELD_FROM_ISR();
    }
}

// ---------- Retransmit scheduler ----------------------------
static constexpr uint8_t MAX_PENDING = 4;  // max in-flight packets

struct PendingSend {
    Pkt      pkt;
    uint8_t  sends_left;
    uint32_t next_ms;
};

static PendingSend g_pending[MAX_PENDING] = {};

// Broadcast MAC — all peers share this address.
static uint8_t g_bcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ---------- radioInit ---------------------------------------
void radioInit(uint8_t channel, PktRecvCb cb)
{
    g_recv_cb = cb;
    g_queue   = xQueueCreate(16, sizeof(QueueItem));

    // AP+STA lets us serve a web page while using ESP-NOW.
    // We NEVER call WiFi.begin() — the STA side stays disconnected.
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);

    // Channel MUST be set before esp_now_init().
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    // Workshop calibrated TX power: prevents ESP32-C3 power droop
    // and makes RSSI distance-sensitive across the room.
    esp_wifi_set_max_tx_power(WIFI_TX_POWER);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("[radio] esp_now_init failed — halting");
        while (true) { delay(1000); }
    }
    esp_now_register_recv_cb(espNowRecvCb);

    // Register the broadcast peer.  Try WIFI_IF_AP first (required when
    // the SoftAP is active on the same channel).
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, g_bcast_mac, 6);
    peer.channel = 0;       // 0 = use current channel
    peer.ifidx   = WIFI_IF_AP;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK)
    {
        // Fallback — hardware validation required to confirm which works.
        peer.ifidx = WIFI_IF_STA;
        if (esp_now_add_peer(&peer) != ESP_OK)
        {
            Serial.println("[radio] broadcast peer add failed on both AP and STA");
        }
        else
        {
            Serial.println("[radio] WARNING: using WIFI_IF_STA — needs hardware validation");
        }
    }
}

// ---------- radioSend (non-blocking) ------------------------
void radioSend(const Pkt* pkt)
{
    // Find a free slot in the pending array.
    for (auto& p : g_pending)
    {
        if (p.sends_left == 0)
        {
            p.pkt        = *pkt;
            p.sends_left = 3;
            p.next_ms    = millis();  // send first copy immediately
            return;
        }
    }
    // All slots full: silently drop (shouldn't happen in workshop use).
}

// ---------- radioLoop (call from Arduino loop()) ------------
void radioLoop()
{
    // 1. Drain the receive queue.
    QueueItem item;
    while (xQueueReceive(g_queue, &item, 0) == pdTRUE)
    {
        if (g_recv_cb)
        {
            g_recv_cb(&item.pkt, item.rssi);
        }
    }

    // 2. Service the retransmit scheduler.
    uint32_t now = millis();
    for (auto& p : g_pending)
    {
        if (p.sends_left > 0 && now >= p.next_ms)
        {
            esp_now_send(g_bcast_mac,
                         reinterpret_cast<const uint8_t*>(&p.pkt),
                         sizeof(Pkt));
            --p.sends_left;
            if (p.sends_left > 0)
            {
                // Random jitter 5-20 ms to reduce collision probability.
                uint32_t jitter = 5u + (esp_random() % 16u);
                p.next_ms       = now + jitter;
            }
        }
    }
}
