/*
 * =========================================================================
 * MATRIX WORKSHOP — EXAMPLE 03: FREERTOS QUEUE & MULTITASKING ON ESP32
 * =========================================================================
 * Purpose: Learn how FreeRTOS Queues allow high-speed radio packet processing
 * without freezing the web server or dropping messages.
 *
 * Concepts:
 *   - xQueueCreate: Creates a FIFO thread-safe ring buffer in RAM
 *   - Task Producer: Fast interrupt/sensor pushes data into queue
 *   - Task Consumer: Worker task pulls items and processes them
 * =========================================================================
 */

#include <Arduino.h>

// Data structure passed through the FreeRTOS Queue
struct EventMsg {
  uint32_t timestamp;
  uint32_t sequence;
  char     text[32];
};

// Queue handle
static QueueHandle_t g_event_queue = NULL;

// -------------------------------------------------------------
// Task 1: Producer Task (Simulates high-speed incoming packets)
// -------------------------------------------------------------
void producerTask(void* pvParameters) {
  uint32_t count = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1500)); // Every 1.5 seconds

    EventMsg msg;
    msg.timestamp = millis();
    msg.sequence = ++count;
    snprintf(msg.text, sizeof(msg.text), "Radio Packet #%u", count);

    // Push item to queue. If queue is full, wait max 10 ticks.
    if (xQueueSend(g_event_queue, &msg, pdMS_TO_TICKS(10)) == pdPASS) {
      Serial.printf("[PRODUCER] Queued: %s (Queue capacity: 10)\n", msg.text);
    } else {
      Serial.println("[PRODUCER WARNING] Queue is full! Packet dropped.");
    }
  }
}

// -------------------------------------------------------------
// Task 2: Consumer Task (Simulates UI & LED processing)
// -------------------------------------------------------------
void consumerTask(void* pvParameters) {
  EventMsg received;
  for (;;) {
    // Block and wait indefinitely until an item arrives in the queue:
    // (Zero CPU wasted while waiting!)
    if (xQueueReceive(g_event_queue, &received, portMAX_DELAY) == pdPASS) {
      Serial.printf("  --> [CONSUMER PROCESSED] Seq: %u | Text: \"%s\" (Latency: %u ms)\n",
                    received.sequence, received.text, (uint32_t)(millis() - received.timestamp));
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n============================================");
  Serial.println("  FreeRTOS Queue & Multitasking Demo ESP32  ");
  Serial.println("============================================\n");

  // Create a Queue that holds up to 10 EventMsg structures
  g_event_queue = xQueueCreate(10, sizeof(EventMsg));
  if (g_event_queue == NULL) {
    Serial.println("[ERROR] Failed to allocate FreeRTOS Queue!");
    return;
  }

  // Pin Producer Task to Core 0 (or allow OS to schedule)
  xTaskCreatePinnedToCore(
    producerTask,   // Task function
    "Producer",     // Name
    3072,           // Stack size (bytes)
    NULL,           // Parameters
    2,              // Priority (higher = 2)
    NULL,           // Task handle
    0               // Core 0 (Protocol / Radio core)
  );

  // Pin Consumer Task to Core 1 (Application core)
  xTaskCreatePinnedToCore(
    consumerTask,   // Task function
    "Consumer",     // Name
    3072,           // Stack size (bytes)
    NULL,           // Parameters
    1,              // Priority
    NULL,           // Task handle
    1               // Core 1 (App / UI core)
  );

  Serial.println("[System] FreeRTOS tasks launched on dual cores!");
}

void loop() {
  // Main loop remains free and responsive for other lightweight duties!
  delay(1000);
}
