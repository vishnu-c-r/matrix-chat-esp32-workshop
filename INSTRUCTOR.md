# Instructor Guide: Operation Agent Smith

> **CLASSIFIED / INSTRUCTOR ONLY**
> Do not share this document or branch with workshop students. Keep the rogue transmitter attack a surprise until the dramatic reveal during the workshop!

---

## 🎭 The Secret Scenario

Students believe they are simply participating in a 3-hour workshop on building decentralized ESP-NOW chat networks. However, hidden inside their firmware is an **active RF proximity radar** calibrated to track rogue beacons.

Midway through the workshop, you will activate **Agent Smith**—a rogue ESP-NOW transmitter running on an ESP32-C3 in your pocket. As you walk past student desks, their NeoPixels will turn red and pulse like Geiger counters, while spoofed messages and corrupt text infiltrate their captive-portal screens.

---

## ⚡ Quick Start for Instructors

### 1. Flash the Smith Firmware
Flash your rogue ESP32-C3 board:
```powershell
pio run -e esp32c3_smith -t upload --upload-port COMx
```
*(Or use `esp32s3_smith` / `esp32dev_smith` if using an S3 or WROOM board).*

### 2. Connect to the Hacker Web UI
1. Power on your Smith board (connect NeoPixel to **GPIO 2** for stage status feedback).
2. Connect your phone or laptop to the rogue Wi-Fi access point:
   - **SSID**: `Matrix-Admin`
   - **Password**: `admin123`
3. Open your browser and navigate to: `http://192.168.4.1`

---

## 🎛️ Hacker Control Panel Features

From your phone's browser, you can control the attack live without touching the board:

### 1. Stage Slider
- **Stage 0: Stealth (Off)** — Silent. Zero RF transmissions. Students can chat normally while you prepare.
- **Stage 1: Ghost Proximity** — Emits `SYS_SYNC` beacons every 500 ms. Front-row students' LEDs will pulse red as you walk near them!
- **Stage 2: Identity Theft** — Automatically impersonates student Node IDs (1–10) with canned quotes.
- **Stage 3: Total Takeover** — High-frequency packet flood with level 5 ASCII text corruption and red chat bubbles.

### 2. Live Custom Message Injection
- Type custom text (e.g. Manglish jokes, local venue references).
- Select any student's `NODE_ID` (1–50) to spoof them, or leave as `99` for Agent Smith.
- Hit **Send** to broadcast instantly into the student mesh.

---

## ⏱️ 3-Hour Workshop Orchestration Timeline

| Time | Phase | Instructor Action |
|:-----|:------|:------------------|
| **0:00 – 1:30** | **Peaceful Building** | Guide students through `presentation.html`. Have them set their `NODE_ID` in `config.h`, flash their C3 boards, and chat. Smith stays in **Stage 0**. |
| **1:30 – 1:45** | **The First Glitch** | Switch Smith to **Stage 1**. Put the board in your pocket and walk down the classroom aisles. Student NeoPixels will begin blinking red! |
| **1:45 – 2:15** | **The Infiltration** | Switch to **Stage 2** or inject custom Manglish messages. Students will start asking each other why they are sending strange texts! |
| **2:15 – 2:30** | **The Takeover** | Switch to **Stage 3** (Total Takeover). The captive chat locks into glitched red text. Open `presentation_smith.html` on the projector! |
| **2:30 – 3:00** | **The Physical Hunt** | Challenge students to stand up and follow their NeoPixel blink frequency (Geiger counter mode) to hunt down the rogue device in the room. |

---

## 🖥️ Presentations

- **Student Slides**: `presentation.html` (Spoiler-free, clean Matrix theme, full code & architecture explanations)
- **Smith Slides**: `presentation_smith.html` (Classified red Matrix theme for instructor briefing or the Stage 3 reveal)
