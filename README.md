# 🛰️ Dual mmWave Radar Fusion System (HLK-LD2452 + HLK-LD6003D)
## Multi-Target Human Positioning, Trajectory Tracking & Spatial Geofence Platform
**Engineering College Capstone / Mini-Project Reference Implementation**  
**Core Controller:** ESP32 (Xtensa Dual-Core 32-bit @ 240MHz) via **PlatformIO**  
**Sensor Array:** 
- **24 GHz FMCW Radar:** Hi-Link HLK-LD2452 (Hardware UART1 @ 256,000 bps)
- **60 GHz FMCW Radar:** Hi-Link HLK-LD6003D / ADT6101P (Hardware UART2 @ 115,200 bps)  
**Display Unit:** 3.5-inch SPI TFT (ILI9488 480×320 Landscape) + XPT2046 Resistive Touch Controller  
**Web Console:** Real-Time WebSocket HTML5/CSS3 Cyberpunk Radar Dashboard with Interactive Geofencing & Live Strip Charts  

---

## 📑 Table of Contents
1. [Project Overview & Scientific Significance](#1-project-overview--scientific-significance)
2. [Complete Hardware Pinout & Wiring Manual](#2-complete-hardware-pinout--wiring-manual)
   - [ESP32 DevKit 30-Pin vs 38-Pin Compatibility](#esp32-devkit-30-pin-vs-38-pin-compatibility)
   - [Master Wiring Table](#master-wiring-table)
   - [Wire-by-Wire Color-Coded Hookup Guide](#wire-by-wire-color-coded-hookup-guide)
   - [Power Distribution & Decoupling Capacitor Design](#power-distribution--decoupling-capacitor-design)
3. [Sensor Physics & Comparison (24GHz vs 60GHz)](#3-sensor-physics--comparison-24ghz-vs-60ghz)
4. [Sensor Serial Communication Protocols](#4-sensor-serial-communication-protocols)
   - [HLK-LD2452 (24GHz) Frame Format](#hlk-ld2452-24ghz-frame-format)
   - [HLK-LD6003D (60GHz) Frame Format](#hlk-ld6003d-60ghz-frame-format)
   - [Multi-Frequency Spatial Fusion Algorithm](#multi-frequency-spatial-fusion-algorithm)
5. [TFT Touch Display UI Design & On-Screen Menu](#5-tft-touch-display-ui-design--on-screen-menu)
6. [Interactive Web Interface User Guide](#6-interactive-web-interface-user-guide)
   - [Embedded Web Server vs Standalone HTML](#embedded-web-server-vs-standalone-html)
   - [Interactive Click-and-Drag Geofence Drawing](#interactive-click-and-drag-geofence-drawing)
   - [Real-Time Dual Strip Charts & Web Audio Alarm](#real-time-dual-strip-charts--web-audio-alarm)
   - [CSV Telemetry Data Export for Reports](#csv-telemetry-data-export-for-reports)
7. [PlatformIO Build, Flash & Deployment Guide](#7-platformio-build-flash--deployment-guide)
8. [Comprehensive Troubleshooting & Diagnostics](#8-comprehensive-troubleshooting--diagnostics)
9. [College Viva / Defense Key Talking Points](#9-college-viva--defense-key-talking-points)

---

## 1. Project Overview & Scientific Significance

Traditional human motion sensors (such as Passive Infrared / PIR) suffer from significant limitations: they only detect lateral motion, fail when an individual stands motionless, are blinded by temperature variations, and cannot measure distance or exact Cartesian coordinates.

This project implements an **enterprise-grade Multi-Frequency Radar Fusion System** combining:
1. **24 GHz FMCW Millimeter-Wave Radar (HLK-LD2452):** Excellent signal penetration through lightweight obstacles, broad beam coverage, high sensitivity to bulk radial movement, tracking up to 3 targets with 10 Hz refresh.
2. **60 GHz FMCW Millimeter-Wave Radar (HLK-LD6003D):** Ultra-wide bandwidth and short wavelength ($\lambda \approx 5\text{ mm}$) delivering fine angular resolution, high trajectory precision, and micro-motion discrimination.
3. **Sensor Fusion Engine:** Cross-correlates spatial coordinates from both distinct RF bands in real time. Fused targets confirmed by both sensors achieve **99% confidence**, eliminating phantom radar reflections and environmental clutter.
4. **Interactive Dual-HUD Visualization:** An onboard 3.5" color SPI TFT display with full touch control plus an asynchronous WebSocket Web Dashboard accessible across any computer or mobile browser.

---

## 2. Complete Hardware Pinout & Wiring Manual

### ESP32 DevKit 30-Pin vs 38-Pin Compatibility
The code uses GPIO assignments that are **100% available and identical on both 30-pin and 38-pin ESP32 DevKit boards**:
- **Hardware UART0 (GPIO 1, 3):** Dedicated to USB Programming and Serial Monitor.
- **Hardware UART1 (GPIO 16, 17):** Assigned to the 24 GHz HLK-LD2452 sensor.
- **Hardware UART2 (GPIO 25, 26):** Assigned to the 60 GHz HLK-LD6003D sensor.
- **Hardware VSPI Bus (GPIO 18, 19, 23):** Shared high-speed bus for both the ILI9488 TFT Display and XPT2046 Touch Screen.
- **Dedicated Chip Selects (CS):** GPIO 15 for TFT Display, GPIO 21 for Touch Controller.

---

### Master Wiring Table

| Peripheral Module | Module Header Pin | ESP32 Board GPIO | Wire Type / Signal | Recommended Wire Color | Functional Description |
|:---|:---|:---|:---|:---|:---|
| **Power Supply** | External 5V Source | **VIN (5V)** | Power (+5V DC) | 🔴 Red | Main system 5V input rail |
| **Power Supply** | Common Ground | **GND** | Ground (0V) | ⚫ Black | Shared ground plane for all modules |
| **3.5" TFT (ILI9488)** | **VCC** | **VIN (5V)** | Power (+5V) | 🔴 Red | Display logic & onboard 3.3V LDO |
| **3.5" TFT (ILI9488)** | **GND** | **GND** | Ground | ⚫ Black | Ground return |
| **3.5" TFT (ILI9488)** | **CS** | **GPIO 15** | Digital Output | 🟠 Orange | TFT Chip Select (Active LOW) |
| **3.5" TFT (ILI9488)** | **RESET / RST** | **GPIO 4** | Digital Output | 🟡 Yellow | TFT Hardware Reset |
| **3.5" TFT (ILI9488)** | **DC / RS** | **GPIO 2** | Digital Output | 🟢 Green | Data / Command Selection |
| **3.5" TFT (ILI9488)** | **SDI / MOSI** | **GPIO 23** | Hardware VSPI | 🔵 Blue | SPI Master-Out Slave-In |
| **3.5" TFT (ILI9488)** | **SCK / SCLK** | **GPIO 18** | Hardware VSPI | 🟣 Purple | SPI Master Clock (27 MHz) |
| **3.5" TFT (ILI9488)** | **LED / BL** | **GPIO 27** | PWM / High | ⚪ White | Display Backlight Control |
| **3.5" TFT (ILI9488)** | **SDO / MISO** | **GPIO 19** | Hardware VSPI | 🟤 Brown | SPI Master-In Slave-Out |
| **Touch (XPT2046)** | **T_CLK** | **GPIO 18** | Hardware VSPI | 🟣 Purple (Jumper) | Shared SPI Clock with TFT |
| **Touch (XPT2046)** | **T_CS** | **GPIO 21** | Digital Output | 🟠 Orange/White | Touch Controller Chip Select |
| **Touch (XPT2046)** | **T_DIN / MOSI** | **GPIO 23** | Hardware VSPI | 🔵 Blue (Jumper) | Shared SPI Data In with TFT |
| **Touch (XPT2046)** | **T_DO / MISO** | **GPIO 19** | Hardware VSPI | 🟤 Brown (Jumper) | Shared SPI Data Out with TFT |
| **Touch (XPT2046)** | **T_IRQ** | **GPIO 22** | Digital Input | 🟡 Yellow/White | Touch Pen Down Interrupt |
| **Radar 1: HLK-LD2452** | **VCC** | **VIN (5V)** | Power (+5V) | 🔴 Red | 5V DC supply (~126 mA operating) |
| **Radar 1: HLK-LD2452** | **GND** | **GND** | Ground | ⚫ Black | Ground |
| **Radar 1: HLK-LD2452** | **TX** | **GPIO 16 (Silkscreen: RX2)** | UART1 RX | 🔵 Blue | Radar TX -> ESP32 RX (**256,000 baud**) |
| **Radar 1: HLK-LD2452** | **RX** | **GPIO 17 (Silkscreen: TX2)** | UART1 TX | 🟢 Green | Radar RX <- ESP32 TX |
| **Radar 2: HLK-LD6003D** | **VCC** | **3.3V or 5V** | Power (VCC) | 🔴 Red | 3.3V (raw chip) or 5V (carrier PCB) |
| **Radar 2: HLK-LD6003D** | **GND** | **GND** | Ground | ⚫ Black | Ground |
| **Radar 2: HLK-LD6003D** | **TX0 (UART0)** | **GPIO 25 (Silkscreen: D25)** | UART2 RX | 🔵 Blue | **USE THIS!** Radar Target Data Output -> ESP32 RX2 |
| **Radar 2: HLK-LD6003D** | **RX0 (UART0)** | **GPIO 26 (Silkscreen: D26)** | UART2 TX | 🟢 Green | **USE THIS!** Radar Command Input <- ESP32 TX2 |
| **Radar 2: HLK-LD6003D** | **TX1 / RX1** | *DO NOT CONNECT* | UART1 Debug | - | Internal SoC Debug Console (leave floating) |
| **Radar 2: HLK-LD6003D** | **TX2 / RX2** | *DO NOT CONNECT* | UART2 Flash | - | Factory Firmware Flash / ISP (leave floating) |
| **Radar 2: HLK-LD6003D** | **BOOT / P19 (if present)** | **GND** | Boot Config | ⚫ Black | Pull LOW/GND for normal internal flash boot |
| **Alert Buzzer (Opt.)** | **Buzzer (+)** | **GPIO 32 (Silkscreen: D32)** | Digital Output | 🔴 Red | Active 5V/3.3V Piezo Buzzer |
| **Alert Buzzer (Opt.)** | **Buzzer (-)** | **GND** | Ground | ⚫ Black | Ground |

---

### 🔍 Important: Understanding ESP32 30-Pin Silkscreen Labels & GPIO 25/26 UART

> [!NOTE]
> **Why are GPIO 16 and 17 labeled "RX2" and "TX2" on your ESP32 board, while GPIO 25 and 26 are labeled "D25" and "D26"?**
> 
> 1. **The ESP32 GPIO Matrix (Flexible Hardware Pin Routing):**
>    Unlike older microcontrollers (like Arduino Uno/Mega) where UART pins are hardwired to fixed physical legs, the ESP32 contains an internal **crossbar switch called the GPIO Matrix (IO MUX)**. This architecture allows the ESP32's hardware UART controllers to be mapped to **virtually any standard GPIO pin** without any performance penalty!
>    - When our code executes `Serial2.begin(115200, SERIAL_8N1, 25, 26)`, the internal hardware UART2 controller is dynamically connected directly to **GPIO 25 and GPIO 26**.
>    - This is **genuine, full-speed Hardware UART with 128-byte hardware FIFO buffers and DMA/interrupts** (not software emulation / SoftwareSerial).
>
> 2. **Why the board manufacturer printed "RX2" and "TX2" on GPIO 16 & 17:**
>    - In Espressif's default silicon datasheet, UART2's default power-on reset pins happen to be GPIO 16 & 17. Board makers therefore printed `RX2` and `TX2` next to them on the PCB.
>    - However, UART1's silicon default pins were GPIO 9 & 10, which are **permanently connected to the internal SPI Flash memory chip under the ESP32 metal shield**. If any board attempts to use GPIO 9 & 10, the ESP32 crashes immediately.
>    - Therefore, to run **two separate hardware UART sensors simultaneously** on ESP32:
>      - **Sensor 1 (HLK-LD2452)** is wired to the pins stamped **`RX2` (GPIO 16)** and **`TX2` (GPIO 17)**.
>      - **Sensor 2 (HLK-LD6003D)** is wired to the general-purpose pins stamped **`D25` (GPIO 25)** and **`D26` (GPIO 26)**, which the ESP32's GPIO Matrix transforms into full Hardware UART pins!

---

### Wire-by-Wire Color-Coded Hookup Guide

#### Step 1: Display & Touch Bus (Shared SPI)
1. Connect **SCK** on the TFT board and **T_CLK** on the Touch header together and wire both to **GPIO 18** (labeled `D18` or `CLK`).
2. Connect **SDI (MOSI)** on the TFT board and **T_DIN** on the Touch header together and wire both to **GPIO 23** (labeled `D23` or `MOSI`).
3. Connect **SDO (MISO)** on the TFT board and **T_DO** on the Touch header together and wire both to **GPIO 19** (labeled `D19` or `MISO`).
4. Wire **TFT_CS** to **GPIO 15** (labeled `D15`), and wire **T_CS** to **GPIO 21** (labeled `D21`).
5. Wire **DC / RS** to **GPIO 2** (labeled `D2`), **RST** to **GPIO 4** (labeled `D4`), **LED (BL)** to **GPIO 27** (labeled `D27`), and **T_IRQ** to **GPIO 22** (labeled `D22`).

#### Step 2: HLK-LD2452 (24 GHz Sensor)
1. Wire **VCC** to **VIN (5V)**.
2. Wire **GND** to **GND**.
3. Wire **TX** on LD2452 to the ESP32 pin labeled **`RX2`** (which is **GPIO 16**).
4. Wire **RX** on LD2452 to the ESP32 pin labeled **`TX2`** (which is **GPIO 17**).

#### Step 3: HLK-LD6003D (60 GHz Sensor)
The HLK-LD6003 module breaks out multiple UART ports from its internal ARM SoC:
- **`TX0` and `RX0` (UART0) -> USE THESE!**:
  - Wire **`TX0`** on LD6003D to the ESP32 pin labeled **`D25`** (which is **GPIO 25**).
  - Wire **`RX0`** on LD6003D to the ESP32 pin labeled **`D26`** (which is **GPIO 26**).
- **`TX1` and `RX1`**: Leave disconnected (internal debug logs).
- **`TX2` and `RX2`**: Leave disconnected (factory flashing).
- **`VCC`**: Check your module label (3.3V for raw module, 5V if carrier board with LDO).
- **`GND`**: Wire to ESP32 **GND**.
- **`BOOT / P19` (if exposed on your pin header)**: Connect to **GND** for normal flash boot.

---

### Power Distribution & Decoupling Capacitor Design

> [!CAUTION]
> **Total Peak Current Budget:**
> - ESP32 during Wi-Fi transmission bursts: ~240 mA peak
> - 3.5" TFT Display LED Backlight: ~150–180 mA
> - HLK-LD2452 FMCW RF chirping: ~130 mA
> - HLK-LD6003D 60GHz RF transmitter: ~150 mA
> **Total Peak Demand = ~670 mA – 750 mA**

**Engineering Best Practices:**
1. **Never power the system from a weak laptop USB 2.0 port (capped at 500 mA)** during radar scanning. Use a dedicated 5V 2A USB wall adapter or powered USB 3.0 hub.
2. **Decoupling Capacitors:** Place a **100µF to 470µF 10V electrolytic capacitor** in parallel with a **0.1µF (100nF) ceramic capacitor** directly across the breadboard's 5V and GND power rails. This absorbs millimeter-wave RF switching transients and prevents spurious ESP32 brownout resets (`Brownout detector was triggered`).
3. **Common Ground:** Every component (ESP32, TFT, Touch, LD2452, LD6003D) **must share a common ground plane**. A floating ground causes UART bit framing errors and touch SPI noise.

---

## 3. Sensor Physics & Comparison (24GHz vs 60GHz)

| Parameter | HLK-LD2452 (24 GHz) | HLK-LD6003D (60 GHz) | Fusion Engine Advantage |
|:---|:---|:---|:---|
| **RF Frequency Band** | 24.00 – 24.25 GHz (ISM Band) | 57.00 – 64.00 GHz (V-Band) | Dual-spectrum RF coverage |
| **Wavelength ($\lambda$)** | $\approx 12.5\text{ mm}$ | $\approx 5.0\text{ mm}$ | Fine vs bulk motion complementarity |
| **Antenna Architecture** | 1 Transmit, 2 Receive (1T2R) | 2 Transmit, 2 Receive (2T2R) | Enhanced azimuth angle resolution |
| **Max Detection Range** | Up to 6.0 meters | Up to 6.0 meters | Synchronized 6m tracking envelope |
| **Angular Coverage (FOV)** | Horizontal: ±60° (120°) \| Vertical: ±30° | Horizontal: ±60° (120°) \| Vertical: ±30° | Matched spatial conical overlap |
| **Penetration Properties** | Strong penetration through drywall/clothing | High surface reflection, micro-Doppler | 24G penetrates; 60G profiles surface |
| **Target Capacity** | 3 Targets simultaneously | 3–4 Targets simultaneously | Scalable multi-individual tracking |
| **Refresh Rate** | 10 Hz (every 100 ms) | 10 Hz (every 100 ms) | Zero-latency real-time fusion loop |

---

## 4. Sensor Serial Communication Protocols

### HLK-LD2452 (24GHz) Frame Format
- **Baud Rate:** `256000 bps` | **Data Bits:** 8 | **Stop Bits:** 1 | **Parity:** None
- **Total Frame Size:** Fixed 30 Bytes
```
+---------------+---------------+---------------+---------------+---------------+
| Header (4B)   | Target 1 (8B) | Target 2 (8B) | Target 3 (8B) | Tail (2B)     |
| AA FF 03 00   | X, Y, Spd, Res| X, Y, Spd, Res| X, Y, Spd, Res| 55 CC         |
+---------------+---------------+---------------+---------------+---------------+
```

#### Target Sub-Block Byte Mapping (8 Bytes each):
- **Byte 0–1 (X-Coordinate):** Signed `int16_t` in millimeters. Bit 15 is sign bit (`1 = negative`, `0 = positive`).
- **Byte 2–3 (Y-Coordinate):** Signed `int16_t` in millimeters (forward depth from sensor).
- **Byte 4–5 (Radial Speed):** Signed `int16_t` in cm/s. Bit 15 is sign (`1 = negative/approaching`, `0 = positive/receding`).
- **Byte 6–7 (Distance Gate):** `uint16_t` distance gate resolution in mm.

---

### HLK-LD6003D (60GHz) Frame Format
- **Baud Rate:** `115200 bps` | **Data Bits:** 8 | **Stop Bits:** 1 | **Parity:** None
- **Processor:** ADT6101P integrated SoC (ARM Cortex-M3 core).
- **Parser Design:** Driver contains an adaptive frame decoder that supports both Hi-Link binary coordinate packets (`AA FF ... 55 CC`) and SY coordinate packets (`53 59 ... 54 43`). Coordinates are normalized into standard meters and m/s.

---

### Multi-Frequency Spatial Fusion Algorithm

In **DUAL FUSION** mode:
1. Every 100ms, candidate coordinates are captured from both sensors:
   $$\vec{P}_{24, i} = (X_{24, i}, Y_{24, i}), \quad \vec{P}_{60, j} = (X_{60, j}, Y_{60, j})$$
2. The spatial Euclidean distance delta is calculated:
   $$\Delta d = \sqrt{(X_{24, i} - X_{60, j})^2 + (Y_{24, i} - Y_{60, j})^2}$$
3. If $\Delta d \le 0.50\text{ m}$ (50 cm clustering threshold):
   - **Target is Confirmed:** Confidence is boosted to **99%**.
   - **Fused Position Calculation:**
     $$X_{fused} = (0.45 \cdot X_{24}) + (0.55 \cdot X_{60})$$
     $$Y_{fused} = (0.50 \cdot Y_{24}) + (0.50 \cdot Y_{60})$$
   - Displayed with a distinctive **Glowing Magenta/White Halo** and marked as `[DUAL FUSED]`.
4. If a target is seen by only one sensor, it is retained with a **75% single-sensor confidence score** and color-coded accordingly.

---

## 5. TFT Touch Display UI Design & On-Screen Menu

The 3.5" TFT display uses a modern **Cyberpunk Radar HUD** layout:

```
+-------------------------------------------------------------------------------+
| [24G] [60G] [DUAL] |       RADAR FUSION OS       | TRK:2 18FPS | IP:192.168.1.150 |
+-------------------------------------------------------+-----------------------+
|                                                       | [ TRACK: ALL TARGETS ]|
|                    (0 deg)                            | DIST : 2.45 m         |
|                     / | \                             | ANG  : +14.2 deg      |
|                   /   |   \                           | SPD  : +0.32 m/s      |
|                 /     |     \                         | POS  : X:+0.6 Y:2.4m  |
|               /   *T1 |       \                       | CONF : 99% [DUAL]     |
|             /     (2.4m)        \                     +-----------------------+
|           /           |           \                   | [ ZOOM - ] [ ZOOM + ] |
|         /             |             \                 +-----------------------+
|       /               |               \               | [  GRID  ] [  ZONE  ] |
|     /                 |                 \             +-----------------------+
|   /                   |                   \           | [  VIEW: RAW HEX   ]  |
| (-60 deg)           (0,0)             (+60 deg)       | RANGE: 0 - 6 m        |
+-------------------------------------------------------+-----------------------+
```

### Touch Button Reference:
- **`[24G]` / `[60G]` / `[DUAL]`:** Switches active sensor modes with zero mechanical buttons.
- **`[TRACK: ALL / T1 / T2 / T3]`:** Cycles target tracking focus. Tap to isolate a single person's telemetry.
- **`[ZOOM -]` / `[ZOOM +]`:** Dynamically scales radar range between 2m, 4m, 6m, and 8m.
- **`[GRID]`:** Toggles Cartesian measurement grid lines.
- **`[ZONE]`:** Toggles the Geofence Alert Zone. When breached:
  - Top banner flashes bright red (`! INTRUSION ALERT !`).
  - Screen border pulses.
  - Active buzzer sounds on GPIO 32.
- **`[VIEW: RAW HEX]`:** Toggles live serial packet sniffer to inspect raw bytes directly on screen.

---

## 6. Interactive Web Interface User Guide

### Access Options:
1. **Embedded Web Server:**
   - The ESP32 serves the dashboard directly on port 80.
   - When connected to local Wi-Fi: Navigate to `http://<ESP32_IP>` (e.g. `http://192.168.1.150`).
   - If local Wi-Fi is unavailable, the ESP32 launches a SoftAP hotspot:
     - **SSID:** `RadarFusion-AP` | **Password:** `12345678` | **URL:** `http://192.168.4.1`
2. **Standalone HTML Application:**
   - Located at [standalone_web/RadarDashboard.html](file:///c:/Users/Vinay%20Singh/Desktop/MiniProject/standalone_web/RadarDashboard.html).
   - Double-click to open in any web browser on your PC, laptop, or smartphone.
   - Type the ESP32 IP address and click **Connect**.

### Key Web Features:
- **Interactive Geofence Drawing:** Click the **`+ Draw Zone`** button, then click and drag directly across the radar canvas to define custom intrusion perimeters!
- **Real-Time Dual Strip Charts:** Hardware-accelerated canvas charts plot primary target distance (m) and radial velocity (m/s) over time.
- **Web Audio Perimeter Alarm:** Generates synthetic acoustic alert sweeps using the Web Audio API on geofence breaches.
- **CSV Telemetry Data Export:** Click **Export CSV** to download a complete timestamped log of all tracking and intrusion events for your college project report.
- **Color Theme Switcher:** Select between **Cyberpunk Dark**, **Military HUD**, and **Deep Void** themes.

---

## 7. PlatformIO Build, Flash & Deployment Guide

### Prerequisites
- Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE** extension.
- Alternatively, use the PlatformIO Core CLI already present on your system.

### Build and Upload Steps:
1. Open the project directory `c:\Users\Vinay Singh\Desktop\MiniProject` in VS Code.
2. Edit [Config.h](file:///c:/Users/Vinay%20Singh/Desktop/MiniProject/include/Config.h) to configure your Wi-Fi SSID and password:
   ```cpp
   #define DEFAULT_WIFI_SSID     "YourCollegeOrHomeWiFi"
   #define DEFAULT_WIFI_PASS     "YourWiFiPassword"
   ```
3. Connect your ESP32 board via USB.
4. Build and flash the firmware:
   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
   ```
5. Launch the Serial Monitor to observe boot diagnostics:
   ```powershell
   & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor
   ```

---

## 8. Comprehensive Troubleshooting & Diagnostics

### Issue 1: TFT Display Shows White Screen
- **Cause:** SPI bus contention or incorrect driver initialization.
- **Solution:** Verify that all SPI build flags in `platformio.ini` are loaded. Check that `TFT_CS` is connected to GPIO 15 and `TFT_DC` is connected to GPIO 2. Ensure your 5V power supply provides adequate current.

### Issue 2: Radar Sensor Status Shows "Disconnected" / No Targets
- **Cause:** Reversed TX/RX lines or baud rate mismatch.
- **Solution:**
  - Remember that sensor **TX connects to ESP32 RX**, and sensor **RX connects to ESP32 TX**.
  - HLK-LD2452 default baud rate is **256,000 bps**.
  - HLK-LD6003D default baud rate is **115,200 bps**.
  - Tap `[VIEW: RAW HEX]` on the TFT screen to inspect incoming raw bytes.

### Issue 3: Touch Screen Not Responding or Coordinates Inverted
- **Cause:** MISO/MOSI wiring issue on the touch header or calibration drift.
- **Solution:** Verify that `T_CLK`, `T_DIN`, and `T_DO` share the VSPI bus (GPIO 18, 23, 19) and `T_CS` is on GPIO 21. Touch calibration constants can be adjusted in [Config.h](file:///c:/Users/Vinay%20Singh/Desktop/MiniProject/include/Config.h).

---

## 9. College Viva / Defense Key Talking Points

When presenting this project to evaluators and professors, highlight these key engineering decisions:
1. **Why FMCW Radar Over Ultrasonic/PIR?**
   - FMCW radars transmit continuous frequency sweeps (chirps). By measuring the frequency difference (beat frequency $f_b$) between the transmitted and received signals, the system calculates exact radial distance ($R = \frac{c \cdot T_c \cdot f_b}{2B}$) and Doppler velocity simultaneously.
2. **Why Fuse 24 GHz and 60 GHz Radars?**
   - A single radar sensor is prone to multipath reflections and environmental noise. Operating two sensors at different frequency bands (24 GHz and 60 GHz) provides **spectral diversity**: 24 GHz penetrates lighter obstacles, while 60 GHz provides superior angular resolution ($\lambda = 5\text{ mm}$). Fusing both sensors eliminates false positives.
3. **Why Dual-Core ESP32 with Non-Blocking State Machines?**
   - Millimeter-wave radars transmit data packets at high baud rates (256,000 bps). Using ESP32's dual hardware UARTs with interrupt-driven ring buffers ensures zero dropped packets while the display renders smooth 60 FPS graphics.