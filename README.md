# Matrix Chat + Agent Smith Detector

> **3-hour engineering workshop** — 30-50 students in groups of 4.
> Each group builds a group chat over ESP-NOW while a rogue "Agent Smith"
> board infiltrates the network. Proximity is detected via RSSI.

---

## Quick start

```
# 1. Edit your identity (ONE file only!)
include/config.h   ←  change NODE_ID and NODE_NAME

# 2. Flash your board
pio run -e esp32s3_node  -t upload --upload-port COMx    # S3
pio run -e esp32dev_node -t upload --upload-port COMx    # WROOM

# 3. Connect phone/laptop to Wi-Fi "NEO-<your id>", password matrix123
# 4. Open http://192.168.4.1
```

---

## Repository layout

```
platformio.ini          Build configuration (all envs)
include/
  config.h              THE ONLY FILE STUDENTS EDIT
src/
  common/               Shared modules (node + smith)
    protocol.h          Wire packet (Pkt struct, magic 0xA5)
    board_pins.h        GPIO assignments per board variant
    palette.h           12-color palette; red reserved for Smith
    radio.h / .cpp      ESP-NOW broadcast, FreeRTOS queue, retransmit
    led.h   / .cpp      LEDC/NeoPixel state machine (gamma-corrected)
    rssi_tracker.h/.cpp EMA filter + CLEAR/NEAR/CLOSE state machine
    dedupe.h  / .cpp    64-entry ring buffer (node_id, seq) filter
    web.h     / .cpp    WebServer + DNSServer captive portal
    smith/              Smith-exclusive modules
      corrupt.h / .cpp  Deterministic text corruptor (5 levels)
  node/
    main.cpp            Human node firmware (reference solution)
  smith/
    main.cpp            Agent Smith firmware (4 escalation stages)
test/
  test_packet.cpp       Protocol pack/unpack
  test_dedupe.cpp       Ring buffer edge cases
  test_rssi.cpp         EMA + hysteresis + timeout
  test_corrupt.cpp      Deterministic corruption
tools/
  make_student.py       Generates student skeleton from reference
```

---

## Wiring

### RGB LED (common-cathode, 3 × 330 Ω resistors to GND)

| Board | R pin | G pin | B pin | Notes |
|-------|-------|-------|-------|-------|
| ESP32-WROOM-32 | GPIO 25 | GPIO 26 | GPIO 27 | Safe; avoids flash (6-11) and strapping pins |
| ESP32-C3 DevKitM-1 | GPIO 3 | GPIO 4 | GPIO 5 | C3 BOOT is GPIO9, not GPIO0 |
| ESP32-S3 DevKitC-1 | GPIO 4 | GPIO 5 | GPIO 6 | Or use onboard NeoPixel (pin 48) |

```
ESP32 pin ──[ 330 Ω ]──┬── R leg (LED)
                        ├── G leg (LED)   } common cathode → GND
                        └── B leg (LED)
```

### NeoPixel (ESP32-S3 DevKitC-1 only)
No external wiring — the onboard RGB LED on pin 48 is used automatically.
`#define HAS_NEOPIXEL true` is set automatically for S3 targets.

---

## Build environments

| Env | Board | Role |
|-----|-------|------|
| `esp32s3_node` | S3 DevKitC-1 | Chat participant |
| `esp32dev_node` | WROOM-32 | Chat participant |
| `esp32c3_node` | C3 DevKitM-1 | Chat participant |
| `esp32s3_smith` | S3 | Agent Smith |
| `esp32dev_smith` | WROOM-32 | Agent Smith |
| `esp32c3_smith` | C3 DevKitM-1 | Agent Smith |
| `esp32dev_student` | WROOM-32 | Skeleton (same as node + `STUDENT_BUILD=1`) |
| `native` | Desktop | Unity unit tests |

---

## Flash steps

```powershell
# List connected boards
pio device list

# Flash node firmware (S3)
pio run -e esp32s3_node -t upload --upload-port COM6

# Flash node firmware (WROOM)
pio run -e esp32dev_node -t upload --upload-port COM7

# Flash Smith firmware (WROOM)
pio run -e esp32dev_smith -t upload --upload-port COM8

# Monitor serial (115200 baud)
pio device monitor --port COM6 --baud 115200
```

> Ports change when boards are unplugged. Re-run `pio device list` if upload fails.

### C3 note
The C3 with USB CDC sometimes needs the BOOT button held during reset to
enter download mode. Hold BOOT, press RST, release RST, release BOOT.

---

## RSSI calibration procedure (`/cal` mode)

1. Flash the node. Open a serial monitor at 115200.
2. Flash Smith onto a second board.
3. Place Smith at a fixed distance (e.g. 1 m) from the node.
4. Type `/rssi` in the serial monitor every few seconds.
5. Note `rssi_f` at 1 m, 2 m, and 5 m in the room.
6. Adjust `RSSI_CLOSE_ENTER` / `RSSI_NEAR` in `include/config.h` so:
   - 1 m ≈ CLOSE  (`rssi_f > RSSI_CLOSE_ENTER`)
   - 2-3 m ≈ NEAR
   - >5 m ≈ CLEAR

Example values from a typical workshop room:

| Distance | Raw RSSI range |
|----------|---------------|
| 0.5 m | -40 to -50 dBm |
| 1–2 m | -55 to -65 dBm |
| 3–5 m | -70 to -80 dBm |
| >5 m   | -80 to -95 dBm |

---

## Venue channel-selection checklist

Before the workshop:

- [ ] Scan the 2.4 GHz spectrum with a Wi-Fi analyser app.
- [ ] Identify channels with the least other AP traffic.
- [ ] Pick a channel **not** used by venue infrastructure (usually 1, 6, 11).
- [ ] Channels 1-13 are valid for ESP-NOW. Channel 1 is the default.
- [ ] Update `#define CHANNEL` in `include/config.h` and flash all boards.
- [ ] Verify all groups can exchange chat messages before handing out Smith.

---

## Airtime budget estimate (10 nodes, 1 Smith)

Each normal node sends at most 1 chat packet × 3 retransmits.
Smith broadcasts a beacon every 100 ms + optional chat at up to 5 msg/s.

| Source | Rate | Pkt size | Approx duty |
|--------|------|----------|-------------|
| 1 node chat (3× retransmit) | ~0.1 msg/s avg | 196 B | < 0.1% |
| 10 nodes combined | 1 msg/s total | 196 B | < 1% |
| Smith beacon (stage 0) | 10/s | 196 B | ~3% |
| Smith flood (stage 3) | 5/s chat + beacon | 196 B | ~4% |
| **Total worst-case** | | | **< 6%** |

ESP-NOW on a 20 MHz channel at 1 Mbit/s can sustain ~50% useful load,
so 10 nodes + Smith is well within budget with minimal collision risk.

---

## Smith escalation stages

Press the **BOOT button** on the Smith board to advance:

| Stage | LED | Behaviour |
|-------|-----|-----------|
| 0 | Off | Silent beacons only (100 ms interval, 2 dBm TX) |
| 1 | Blue | Impersonates a random node (1 of 10 canned lines) |
| 2 | Yellow | Progressive text corruption, level 1→5 over time |
| 3 | Red | Flood 5 msg/s, max corruption |

---

## Student skeleton generation

```powershell
python tools/make_student.py          # outputs to student/
python tools/make_student.py --out dist/student   # custom path
```

The script strips all `// SOLUTION-BEGIN … // SOLUTION-END` blocks and
replaces them with `// TODO(stageN): <hint>` comments.

### Stage guide for students

| Stage | Time | What to do | Visible result |
|-------|------|------------|----------------|
| 1 | 15 min | Call `ledInit(NODE_ID)` in `setup()` | LED glows in your node colour |
| 2 | 40 min | Build `Pkt` fields in `sendChatMessage()`; handle CHAT in `onPktRecv()` | Messages appear in browser |
| 3 | 40 min | Call `rssiTrackerUpdate()` + `rssiTrackerTick()` → `ledSetSmithState()` | LED blinks when Smith approaches |

---

## Unit tests (native)

```powershell
pio test -e native
```

Tests: `test_packet`, `test_dedupe`, `test_rssi`, `test_corrupt`.
No hardware needed — runs on the host machine.

---

## Assumptions recorded

The following were assumed in this implementation and should be
confirmed before the workshop:

1. **Broadcast peer interface**: `WIFI_IF_AP` is used. If TX returns
   `ESP_ERR_ESPNOW_IF`, the code falls back to `WIFI_IF_STA` and prints
   a warning. Which interface works with a SoftAP active on the same
   channel needs hardware validation.

2. **Smith TX power**: `esp_wifi_set_max_tx_power(8)` (2 dBm) is called
   after `esp_now_init()`. Whether this is honoured without a full radio
   calibration cycle on every board variant needs hardware validation.

3. **RSSI API**: `esp_now_recv_info_t->rx_ctrl->rssi` is used (IDF 5.x /
   Arduino-ESP32 core 3.x). Verified available by `static_assert` on
   `ESP_IDF_VERSION_MAJOR >= 5`. Absolute accuracy (±3-5 dBm typical)
   varies by board, antenna orientation, and environment.

4. **Captive portal**: The DNS redirect + OS probe URLs work on most
   Android (Chrome) and iOS (Safari) clients in our testing. Some OS
   versions cache captive-portal decisions. Users may need to forget the
   network and reconnect. Windows NCSI behaviour varies.

5. **ledcAttach / ledcWrite**: The new Arduino-ESP32 3.x LEDC API is
   used. The deprecated `ledcSetup` + `ledcAttachPin` calls are not used.
   If the platform version reverts to an older API, `led.cpp` will need
   updating.

6. **NeoPixel (S3)**: `neopixelWrite()` is available in Arduino-ESP32 3.x
   without any extra library. Behaviour with 3rd-party NeoPixel strips
   on pins other than 48 has not been validated.

---

## Serial commands (node firmware)

| Command | Output |
|---------|--------|
| `/id` | Node ID, name, and MAC address |
| `/rssi` | Current filtered RSSI and raw last value |
| `/state` | Smith state: CLEAR / NEAR / CLOSE |
| *(plain text)* | Sends as a chat message |
