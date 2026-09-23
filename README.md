# The Matrix: Decentralised ESP-NOW Mesh Chat

> **3-Hour Engineering Workshop**
> Build an autonomous, serverless peer-to-peer chat network using ESP32 microcontrollers and the ESP-NOW 2.4 GHz protocol. Each participant's board hosts its own captive-portal web interface and uses addressable RGB hardware indicators to signal network activity.

---

## ⚡ Quick Start

```powershell
# 1. Edit your node identity (The ONLY file you need to modify!)
# Open include/config.h and set your unique NODE_ID (1-50) and NODE_NAME
include/config.h

# 2. Flash your board via PlatformIO (replace COMx with your port)
pio run -e esp32c3_node  -t upload --upload-port COMx    # ESP32-C3 SuperMini (Recommended)
pio run -e esp32s3_node  -t upload --upload-port COMx    # ESP32-S3 DevKitC-1
pio run -e esp32dev_node -t upload --upload-port COMx    # ESP32-WROOM-32

# 3. Connect your phone or laptop to your board's Wi-Fi network:
# SSID:     NEO-<your_node_id>   (e.g., NEO-14)
# Password: matrix123

# 4. Open your browser and navigate to:
http://192.168.4.1
```

---

## 📁 Repository Layout

```
platformio.ini          PlatformIO build configurations for all targets
include/
  config.h              THE ONLY FILE YOU NEED TO EDIT (ID & Handle)
src/
  common/               Shared network & hardware drivers
    protocol.h          Wire packet definition (Pkt struct, magic 0xA5)
    board_pins.h        Pinouts for ESP32-C3, S3, and WROOM
    palette.h           12-color unique node identity palette
    radio.h / .cpp      ESP-NOW broadcast engine & queue manager
    led.h   / .cpp      NeoPixel / RGB LED state animation controller
    dedupe.h  / .cpp    64-entry ring buffer packet deduplicator
    web.h     / .cpp    Lightweight WebServer + DNSServer captive portal
  node/
    main.cpp            Node chat firmware
presentation.html       Interactive workshop slide deck
```

---

## 🔌 Hardware & Wiring

### 1. ESP32-C3 SuperMini / DevKitM-1 (Primary Workshop Board)
The ESP32-C3 uses an external WS2812 addressable NeoPixel LED connected to **GPIO 2**:

| NeoPixel Pin | ESP32-C3 Pin | Notes |
|:-------------|:-------------|:------|
| **VCC / 5V** | 5V / VBUS | Power line |
| **GND**      | GND | Ground line |
| **DIN (Data)**| **GPIO 2** | High-speed single-wire data |

> **Note on C3 Uploads**: If the C3 board does not enter bootloader mode automatically, hold down the **BOOT** button (GPIO 9), tap the **RST** button, and then release BOOT.

### 2. ESP32-S3 DevKitC-1
No external wiring required. The onboard addressable RGB LED on **GPIO 48** is used automatically (`HAS_NEOPIXEL = true`).

### 3. ESP32-WROOM-32 (esp32dev)
Uses an external common-cathode RGB LED with 3 × 330 Ω resistors:
- **Red**: GPIO 25
- **Green**: GPIO 26
- **Blue**: GPIO 27
- **Cathode**: GND

---

## 📡 How the Chat System Works

1. **Peer-to-Peer Radio Layer**:
   - Messages bypass Wi-Fi routers, the internet, and servers entirely.
   - Nodes transmit 2.4 GHz raw vendor-specific action frames directly over **ESP-NOW** in under 1 millisecond.
   - All boards in the room listen on the same radio channel (`CHANNEL 1`).

2. **The Wire Packet (`Pkt`)**:
   - Every transmission is packed into a compact binary `Pkt` struct (188 bytes):
     - `magic` (0xA5): Network signature byte.
     - `ver`: Protocol version.
     - `node_id`: Sender's unique ID number.
     - `color_idx`: Determines avatar and LED color.
     - `seq`: Monotonically incrementing sequence ID for deduplication.
     - `text`: Null-terminated chat string (up to 180 characters).

3. **Captive Portal Web Interface**:
   - Each ESP32 acts as a SoftAP (`NEO-<id>`).
   - A built-in DNS server captures all requests and routes them to `192.168.4.1`.
   - The web UI polls `/api` once per second to pull recent messages from the ring buffer.

4. **NeoPixel Hardware Status**:
   - **IDLE**: Smooth breathing pulse in your node's assigned color.
   - **TRANSMIT (TX)**: Quick blue pulse when you send a message.
   - **RECEIVE (RX)**: Immediate bright flash in the sender's color whenever a packet arrives.
   - **ACTIVE**: Radio link synchronization indicator.

---

## 🛠️ Build Environments

| Environment | Board Target | Description |
|:------------|:-------------|:------------|
| `esp32c3_node` | ESP32-C3 | Final workshop student chat firmware |
| `esp32s3_node` | ESP32-S3 | S3 DevKitC-1 student chat firmware |
| `esp32dev_node` | ESP32-WROOM | Classic ESP32 DevKit chat firmware |

---

## 💻 Serial Monitor Commands

Open a serial monitor at **115200 baud** to interact directly with your board:

| Command | Action |
|:--------|:-------|
| `/id` | Prints your `NODE_ID`, handle, and hardware MAC address |
| `/rssi` | Displays filtered RSSI signal strength from incoming transmissions |
| `/state` | Displays current network link status & proximity distance |
| *(any text)* | Transmits the entered text as a chat message over ESP-NOW |

---

## 🖥️ Workshop Presentation

Open `presentation.html` in Google Chrome or any modern browser for the interactive slide deck.
- Use **Left / Right arrow keys** or the on-screen **◄ Prev / Next ►** buttons to navigate.
- Use **`+`** / **`-`** to scale presentation contents to match your screen or projector.
- Press **`F`** to toggle fullscreen mode.
