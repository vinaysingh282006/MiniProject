# 🛰️ Dual mmWave Radar Fusion System (HLK-LD2452 + HLK-LD6003D)
### College Capstone / Mini-Project: Advanced Spatial Intelligence & Multi-Target Radar Tracking
**Platform:** ESP32 (DevKit V1 / ESP32-WROOM-32) in **PlatformIO**  
**Sensors:** HLK-LD2452 (24 GHz FMCW) & HLK-LD6003D (60 GHz FMCW)  
**Display:** 3.5-inch SPI TFT (ILI9488 480×320) with XPT2046 Touch Screen  
**Web Dashboard:** Real-Time WebSocket HTML5/CSS3 Cyberpunk Radar Dashboard  

---

## 📋 Table of Contents
1. [System Architecture](#system-architecture)
2. [Detailed Wiring & Pinout Tables](#detailed-wiring--pinout-tables)
3. [Sensor Protocols & Specifications](#sensor-protocols--specifications)
4. [TFT Touch Display UI Design](#tft-touch-display-ui-design)
5. [Web Interface (Embedded & Standalone)](#web-interface)
6. [PlatformIO Build & Upload Instructions](#platformio-build--upload-instructions)
7. [Operational Controls](#operational-controls)

---

## 🏗️ System Architecture

```
                       +-----------------------------------+
                       |       ESP32 Microcontroller       |
                       |         (Xtensa @ 240MHz)         |
                       +-----------------------------------+
                               /           |           \
                 Hardware UART1 /            | Hardware  \  Hardware VSPI
                              /             | UART2     \
                             v              v            v
                 +---------------+  +---------------+  +---------------------+
                 |  HLK-LD2452   |  |  HLK-LD6003D  |  | 3.5" ILI9488 SPI    |
                 |  24GHz Radar  |  |  60GHz Radar  |  | TFT + XPT2046 Touch |
                 | (256,000 baud)|  | (115,200 baud)|  | (480x320 Landscape)|
                 +---------------+  +---------------+  +---------------------+
                             \              /                    ^
                              \            /                     |
                               v          v                      |
                        [ SENSOR FUSION ENGINE ] ----------------+
                                   |
                             Async WebSocket
                                   |
                                   v
                      +--------------------------+
                      | Real-Time Web Dashboard  |
                      | (Canvas Radar + Charts)  |
                      +--------------------------+
```

---

## 🔌 Detailed Wiring & Pinout Tables

### 1. ESP32 to 3.5" SPI TFT (ILI9488) Pinout
| 3.5" TFT Pin | ESP32 GPIO | Description / Notes |
|:---|:---|:---|
| **VCC** | **VIN (5V)** | Power Supply (most red modules have onboard 3.3V LDO) |
| **GND** | **GND** | Common Ground |
| **CS** | **GPIO 15** | TFT Chip Select (VSPI) |
| **RESET / RST** | **GPIO 4** | TFT Hardware Reset |
| **DC / RS** | **GPIO 2** | TFT Data / Command Selection |
| **SDI / MOSI** | **GPIO 23** | SPI Master-Out-Slave-In |
| **SCK / SCLK** | **GPIO 18** | SPI Clock |
| **LED / BL** | **GPIO 27** | Backlight Control (or connect directly to 3.3V) |
| **SDO / MISO** | **GPIO 19** | SPI Master-In-Slave-Out |

### 2. ESP32 to XPT2046 Touch Screen Pinout
*(Usually on the same header as the 3.5" TFT display module)*
| Touch Pin | ESP32 GPIO | Description / Notes |
|:---|:---|:---|
| **T_CLK** | **GPIO 18** | Shared SPI Clock with TFT |
| **T_CS** | **GPIO 21** | Touch Controller Chip Select |
| **T_DIN / MOSI** | **GPIO 23** | Shared SPI MOSI with TFT |
| **T_DO / MISO** | **GPIO 19** | Shared SPI MISO with TFT |
| **T_IRQ** | **GPIO 22** | Touch Pen Interrupt |

### 3. ESP32 to HLK-LD2452 (24 GHz Sensor) Pinout
| HLK-LD2452 Pin | ESP32 Pin | Description / Notes |
|:---|:---|:---|
| **VCC** | **5V (VIN)** | 5V DC supply (sensor operating current ~126mA) |
| **GND** | **GND** | Common Ground with ESP32 |
| **TX** | **GPIO 16 (RX1)** | Sensor TX -> ESP32 Hardware UART1 RX |
| **RX** | **GPIO 17 (TX1)** | Sensor RX <- ESP32 Hardware UART1 TX |

### 4. ESP32 to HLK-LD6003D (60 GHz Sensor) Pinout
| HLK-LD6003D Pin | ESP32 Pin | Description / Notes |
|:---|:---|:---|
| **VCC** | **3.3V / 5V** | Check sub-board label: 3.3V for raw module, 5V if carrier board |
| **GND** | **GND** | Common Ground with ESP32 |
| **TX** | **GPIO 25 (RX2)** | Sensor TX -> ESP32 Hardware UART2 RX |
| **RX** | **GPIO 26 (TX2)** | Sensor RX <- ESP32 Hardware UART2 TX |

### 5. Optional Peripherals
| Component | ESP32 GPIO | Description |
|:---|:---|:---|
| **Buzzer (+)** | **GPIO 32** | Active Buzzer for Geofence Breach Alert |
| **Buzzer (-)** | **GND** | Ground |

> [!IMPORTANT]
> **Power Recommendation:** Millimeter-wave sensors and 3.5" TFT displays can draw significant current spikes during RF chirping (upwards of 500-800mA combined). Ensure your ESP32 is powered via a high-quality USB 5V 2A power source, and connect **all GND pins together** to establish a solid ground plane.

---

## 📡 Sensor Protocols & Specifications

### 1. HLK-LD2452 (24 GHz Multi-Target FMCW Radar)
- **Default Baud Rate:** `256000 bps` (8-N-1)
- **Reporting Frequency:** 10 Hz
- **Packet Structure (30 Bytes):**
  ```
  [Header: AA FF 03 00] [Target 1 (8B)] [Target 2 (8B)] [Target 3 (8B)] [Tail: 55 CC]
  ```
- **Target Sub-block (8 Bytes):**
  - Bytes 0–1: Target X-coordinate in mm (`int16_t`, bit 15 is sign bit)
  - Bytes 2–3: Target Y-coordinate in mm (`int16_t`, bit 15 is sign bit)
  - Bytes 4–5: Target Speed in cm/s (`int16_t`, bit 15 is sign bit)
  - Bytes 6–7: Distance Resolution in mm (`uint16_t`)

### 2. HLK-LD6003D (60 GHz Multi-Target Trajectory Radar)
- **Default Baud Rate:** `115200 bps` (8-N-1)
- **SoC Core:** ADT6101P (ARM Cortex-M3 core)
- **Features:** High angular resolution, lateral & longitudinal trajectory tracking.
- **Parser Architecture:** Implements auto-detecting parsing supporting both HiLink multi-target frames (`0xAA 0xFF` ... `0x55 0xCC`) and SY coordinate frames (`0x53 0x59` ... `0x54 0x43`).

### 3. Multi-Sensor Spatial Fusion Engine
When **DUAL FUSION** mode is active:
- Correlates target clusters from both sensors within a 50cm Euclidean threshold:
  $$\Delta d = \sqrt{(X_{24} - X_{60})^2 + (Y_{24} - Y_{60})^2} \le 0.50\text{ m}$$
- If confirmed by both radars:
  - Coordinate: $X_{fused} = 0.45 X_{24} + 0.55 X_{60}$, $Y_{fused} = 0.50 Y_{24} + 0.50 Y_{60}$
  - Confidence: Boosted to **99%**
  - Displayed on TFT and Web with a distinctive **Glowing Magenta/White Halo** and marked as `[DUAL FUSED]`.

---

## 🖥️ TFT Touch Display UI Design

The 3.5" TFT display uses a modern **Cyberpunk Glassmorphism Radar HUD** aesthetic:
1. **Startup Animation:**
   - Animated circular sweep beam rotating through all quadrants.
   - Self-test diagnostic checklist validating ESP32 CPU, LD2452, LD6003D, and WiFi status.
   - Glowing progress bar transitioning into the dashboard.
2. **Top Navigation Tabs:**
   - Touch `[24G]`, `[60G]`, or `[DUAL]` to switch active sensors instantaneously with zero physical buttons.
   - Flashing red banner on top if a geofence intrusion occurs (`! INTRUSION ALERT !`).
3. **Radar Viewport:**
   - Concentric distance range rings with labeled intervals (1m, 2m, 3m...).
   - Animated sweeping beam that moves continuously across the FOV.
   - Velocity vector arrows indicating movement direction.
   - Breadcrumb trajectory trails showing where the target walked over time.
   - Color-coded targets:
     - 🟦 **Cyber Cyan**: HLK-LD2452 (24G)
     - 🟧 **Solar Amber**: HLK-LD6003D (60G)
     - 🟪 **Electric Magenta**: Dual Fused
     - 🟥 **Flashing Red**: Target inside Geofence Alert Zone
4. **Touch Control Panel:**
   - `[ZOOM -]` and `[ZOOM +]`: Scale range dynamically between 2m, 4m, 6m, and 8m.
   - `[GRID]`: Toggle Cartesian grid lines.
   - `[ZONE]`: Toggle geofence perimeter detection.
   - `[VIEW: RAW HEX]`: Live UART packet sniffer to inspect raw sensor data streams.
   - `[TRACK: ALL / T1 / T2 / T3]`: Focus and lock on individual persons.

---

## 🌐 Web Interface (Embedded & Standalone)

The project includes two ways to access the web interface:

### 1. Embedded Web Server
- When the ESP32 starts, it connects to your local Wi-Fi or launches an access point:
  - **SSID:** `RadarFusion-AP`
  - **Password:** `12345678`
  - **IP Address:** `http://192.168.4.1`
- Open any web browser on your phone, tablet, or laptop and visit the IP address. The full HTML5 dashboard is served directly from the ESP32!

### 2. Standalone Dashboard File (`standalone_web/RadarDashboard.html`)
- Located in [RadarDashboard.html](file:///c:/Users/Vinay%20Singh/Desktop/MiniProject/standalone_web/RadarDashboard.html).
- Double-click to open in Google Chrome, Microsoft Edge, or Safari on your computer.
- Enter the IP address shown on your TFT display and click **Connect**.
- Features:
  - 60 FPS HTML5 Canvas Radar with glowing sweep line and trails.
  - Interactive Geofence Editor: customize $X_{min}, X_{max}, Y_{min}, Y_{max}$ boundaries.
  - Web Audio API synthesizer for acoustic alarms on intrusion.
  - Real-time distance vs. time chart for primary targets.
  - Live table showing Distance, Angle, Radial Speed, Lateral X, and Longitudinal Y.
  - Raw Hex Telemetry monitor for protocol inspection.

---

## 🚀 PlatformIO Build & Upload Instructions

### Prerequisites
- Install **VS Code** with the **PlatformIO IDE** extension (or PlatformIO Core CLI).

### Step-by-Step Build:
1. Open the project folder `c:\Users\Vinay Singh\Desktop\MiniProject` in PlatformIO.
2. Edit [Config.h](file:///c:/Users/Vinay%20Singh/Desktop/MiniProject/include/Config.h) to set your Wi-Fi credentials:
   ```cpp
   #define DEFAULT_WIFI_SSID     "YourWiFiSSID"
   #define DEFAULT_WIFI_PASS     "YourWiFiPassword"
   ```
3. Connect your ESP32 board via USB.
4. Build and upload firmware:
   ```powershell
   pio run -t upload
   ```
5. Open Serial Monitor:
   ```powershell
   pio device monitor
   ```

---

## 🎛️ Operational Controls Summary

| Action | TFT Touch Action | Web Dashboard Action |
|:---|:---|:---|
| **Switch to 24G Sensor** | Tap `[24G]` tab at top | Click `24GHz (LD2452)` button |
| **Switch to 60G Sensor** | Tap `[60G]` tab at top | Click `60GHz (LD6003D)` button |
| **Switch to Dual Fusion** | Tap `[DUAL]` tab at top | Click `DUAL FUSION` button |
| **Focus on Single Person** | Tap top right card (`TRACK: ALL / T1`) | Click `Target 1`, `Target 2`, etc. |
| **Zoom Radar Range** | Tap `[ZOOM -]` or `[ZOOM +]` | Adjust `Max Zoom Range` slider |
| **Toggle Grid** | Tap `[GRID]` | Check/uncheck `Cartesian Grid` |
| **Toggle Geofence Zone** | Tap `[ZONE]` | Check/uncheck `Geofence Alert Zone` |
| **Inspect Raw Hex Bytes** | Tap `[VIEW: RAW HEX]` | View `Raw Telemetry Sniffer` box |