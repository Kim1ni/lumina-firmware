# 🔆 Lumina - AI-Integrated Smart Lamp

A professional-grade IoT smart lamp built with ESP8266, featuring AI mood lighting, robust state management, and seamless Android integration.

## 📋 Table of Contents

- [Overview](#overview)
- [Hardware Requirements](#hardware-requirements)
- [Software Architecture](#software-architecture)
- [Getting Started](#getting-started)
- [State Diagram](#state-diagram)
- [Protocol Specification](#protocol-specification)
- [Android App Integration](#android-app-integration)
- [Troubleshooting](#troubleshooting)
- [Future Enhancements](#future-enhancements)

---

## 🎯 Overview

Lumina transforms a simple LED ring into an intelligent lighting system that responds to your mood. Built from a "breadboard mess" into a production-ready device, it showcases professional software engineering patterns applied to embedded systems.

### Key Features

- **🎨 AI Mood Lighting**: Gemini API integration for natural language color control
- **🔋 Portable Design**: 18650 battery with voltage monitoring and low-power warnings
- **📡 Wireless Control**: UDP-based communication with Android app
- **🔄 OTA Updates**: Over-the-air firmware updates with rollback on failure
- **🧠 Smart State Management**: Memory-safe State Pattern prevents crashes
- **💪 Production Ready**: Full error handling, diagnostics, and recovery

---

## 🛠️ Hardware Requirements

### Core Components

| Component | Specification | Purpose |
|-----------|--------------|---------|
| **ESP8266** | NodeMCU or ESP-12E | Main controller |
| **LED Ring** | WS2812B, 16 LEDs | RGB lighting |
| **Battery** | 18650 Li-ion (3.7V, 2600mAh+) | Portable power |
| **Charging Module** | TP4056 | Battery charging circuit |
| **Boost Converter** | MT3608 | 3.7V → 5V for LEDs |
| **Resistors** | 330kΩ + 100kΩ | Voltage divider for battery monitoring |

### Circuit Diagram

```
18650 Battery (3.0V - 4.2V)
    ├─→ TP4056 (Charging)
    └─→ MT3608 (Boost to 5V)
           ├─→ WS2812B Ring (Data: GPIO12/D6, Power: 5V)
           └─→ ESP8266 (Power: 3.3V via onboard regulator)
           
Battery Voltage Monitor:
    Battery+ ─[330kΩ]─┬─[100kΩ]─ GND
                       └─→ ESP8266 A0 (ADC)
```

### Power Calculations

- **LED Ring**: ~16 LEDs × 60mA = 960mA max (at full white)
- **ESP8266**: ~80mA active, ~15mA in light sleep
- **Total**: ~1A peak, ~500mA typical
- **Battery Life**: 2600mAh / 500mA ≈ 5 hours (typical usage)

---

## 🏗️ Software Architecture

### Design Patterns

#### 1. **State Pattern** (Core Architecture)

Prevents memory fragmentation by ensuring only one state's logic exists in RAM at a time.

```
LuminaState (Abstract)
    ├─ SearchingState      → Connecting to WiFi
    ├─ ProvisioningState   → AP mode for setup
    ├─ ConnectedState      → Normal operation
    └─ UpdatingState       → OTA firmware update
```

**Benefits:**
- Zero memory leaks (states deleted on transition)
- Clear separation of concerns
- Easy to add new operational modes

#### 2. **Observer Pattern** (Communication)

Device broadcasts "heartbeat" packets; Android app observes via UDP.

```kotlin
// Android side (Kotlin)
val deviceStatus: StateFlow<LuminaStatus> = udpReceiver
    .observeHeartbeats()
    .stateIn(viewModelScope)
```

#### 3. **Strategy Pattern** (Lighting)

Interchangeable lighting algorithms selected at runtime.

```
LightingStrategy (Abstract)
    ├─ SolidColorStrategy      → Static color
    ├─ CalmBreathingStrategy   → Slow sine wave
    ├─ FocusStrategy           → Steady with subtle pulse
    └─ PartyStrategy           → Rotating rainbow segments
```

---

## 🚀 Getting Started

### Prerequisites

1. **Arduino IDE** (1.8.19 or newer)
2. **ESP8266 Board Package** (3.0.2+)
   - Add to Board Manager: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. **Libraries** (Install via Library Manager):
   - `Adafruit NeoPixel` (1.10.0+)
   - `ArduinoOTA` (Included with ESP8266 package)

### Installation

1. **Clone or download** this repository
2. **Open** `Lumina.ino` in Arduino IDE
3. **Configure** your board:
   - Board: "NodeMCU 1.0 (ESP-12E Module)"
   - CPU Frequency: "80 MHz"
   - Flash Size: "4MB (FS:2MB OTA:~1019KB)"
   - Upload Speed: "115200"

4. **Adjust Config.h** (if needed):
   ```cpp
   #define LED_COUNT 16        // Match your LED ring
   #define AP_SSID "Lumina-Setup"
   #define AP_PASSWORD "lumina2026"
   ```

5. **Upload** the sketch via USB

### First Boot

1. **Power on** the device
2. **Watch Serial Monitor** (115200 baud) for debug output
3. Device will enter **SearchingState** (blue pulse)
4. If no saved WiFi, it transitions to **ProvisioningState** (orange animation)

---

## 🔄 State Diagram

```
         [Power On]
              ↓
      ┌─ SearchingState  ─┐
      │  (Blue Pulse)     │
      │  Credentials?     │
      └────────┬──────────┘
               │
        ┌──────┴──────┐
        │ No          │ Yes
        ↓             ↓
ProvisioningState  [WiFi Connect]
(Orange Rotating)       │
Setup AP for       ┌────┴────┐
credential         │ Success │ Timeout
download           ↓         ↓
    │       ConnectedState  [Retry]
    └──→    (Green Calm)      ↑
               │              │
          ┌────┴────┐         │
          │ OTA Cmd │ WiFi Lost
          ↓         └─────────┘
    UpdatingState
    (Yellow Pulse)
    OTA Download
          │
     ┌────┴────┐
     │Success  │Failure
     ↓         ↓
  [Reboot]  [Rollback]
```

---

## 📡 Protocol Specification

### Command Structure

All commands sent as UDP packets to port **4210**:

```
[Byte 0] = Command Code
[Byte 1..N] = Data
```

### Command Codes

| Code | Name | Data Format | Description |
|------|------|-------------|-------------|
| `0x01` | `CMD_SET_COLOR` | `[R, G, B]` | Set solid color |
| `0x02` | `CMD_SET_MOOD` | `[Type, R, G, B, ...]` | AI mood lighting |
| `0x03` | `CMD_SET_BRIGHTNESS` | `[Brightness]` | 0-255 brightness |
| `0x04` | `CMD_GET_STATUS` | - | Request device status |
| `0x05` | `CMD_PROVISION` | `[SSIDLen, SSID..., PassLen, Pass...]` | Send WiFi credentials |
| `0x06` | `CMD_OTA_START` | - | Begin OTA update |
| `0xFF` | `CMD_RESET` | - | Factory reset |

### Status Packets

Device broadcasts status every 5 seconds:

```
[0] = 0x10 (STATUS_HEARTBEAT)
[1] = Current state code
[2] = Battery percent (0-100)
[3] = WiFi RSSI (0-128, offset by +128)
[4-7] = Battery voltage (float)
[8-11] = Free heap (uint32)
[12] = Strategy name length
[13..] = Strategy name (ASCII)
```

---

## 📱 Android App Integration

### Kotlin Flow Example

```kotlin
data class LuminaStatus(
    val state: DeviceState,
    val batteryPercent: Int,
    val batteryVoltage: Float,
    val wifiRssi: Int,
    val freeHeap: Int,
    val lightingMode: String
)

class LuminaViewModel : ViewModel() {
    private val udpSocket = DatagramSocket(4210)
    
    val deviceStatus = flow {
        while (true) {
            val packet = DatagramPacket(ByteArray(256), 256)
            udpSocket.receive(packet)
            
            if (packet.data[0] == 0x10.toByte()) {
                emit(parseLuminaStatus(packet.data))
            }
        }
    }.stateIn(viewModelScope, SharingStarted.Lazily, null)
    
    fun setMood(mood: String) = viewModelScope.launch {
        val rgb = geminiApi.moodToRGB(mood) // Call Gemini
        sendCommand(CMD_SET_MOOD, byteArrayOf(
            0x00, // Calm mode
            rgb.r.toByte(),
            rgb.g.toByte(),
            rgb.b.toByte()
        ))
    }
}
```

### Jetpack Compose UI

```kotlin
@Composable
fun LuminaControlScreen(viewModel: LuminaViewModel) {
    val status by viewModel.deviceStatus.collectAsState()
    
    Column {
        ProductHealthCard(
            battery = status?.batteryPercent ?: 0,
            signal = status?.wifiRssi ?: 0,
            memory = status?.freeHeap ?: 0
        )
        
        MoodInput(onMoodSubmit = { mood ->
            viewModel.setMood(mood)
        })
    }
}
```

---

## 🐛 Troubleshooting

### Memory Issues

**Symptom**: Device crashes with `0KB Free Memory` error

**Solutions**:
- Check that states are deleted properly in `transitionTo()`
- Reduce `LED_COUNT` if using more LEDs than 16
- Disable debug output: `#define DEBUG_MODE false`
- Use `F()` macro for strings: `DEBUG_PRINTLN(F("Text"));`

### LED Flickering

**Symptom**: LEDs flicker or show wrong colors

**Solutions**:
- Verify MT3608 output is stable 5V
- Check WS2812B data wire is on correct pin (D6/GPIO12)
- Add 470µF capacitor across LED power and ground
- Reduce brightness: `leds.setBrightness(100);`

### WiFi Won't Connect

**Symptom**: Stuck in SearchingState forever

**Solutions**:
- Check Serial Monitor for actual error
- Verify SSID/password in EEPROM (use `CMD_PROVISION`)
- Try 2.4GHz WiFi only (ESP8266 doesn't support 5GHz)
- Move closer to router during initial setup

### OTA Update Fails

**Symptom**: Update starts but doesn't complete

**Solutions**:
- Ensure strong WiFi signal (RSSI > -70dBm)
- Don't move device during update
- Check that compiled binary is < 1MB
- Use Ethernet cable for ThinkPad during development

---

## 🚀 Future Enhancements

### Hardware
- [ ] Add physical button for manual state control
- [ ] Implement deep sleep for "off" mode (days of battery life)
- [ ] Use Li-Po battery with fuel gauge IC for accurate %
- [ ] Design 3D-printed enclosure (puck style, 80mm diameter)

### Software
- [ ] Implement Smooth LED transitions between colors
- [ ] Add scheduling (turn on/off at specific times)
- [ ] Music reactive mode (using FFT on ESP8266)
- [ ] Multi-device sync (control multiple Luminas as one)

### App Features
- [ ] Voice control via Google Assistant
- [ ] Preset scenes (Movie, Reading, Sleep, etc.)
- [ ] Historical battery usage graphs
- [ ] Firmware update directly from app

---

## 📄 License

MIT License - See LICENSE file for details

## 🙏 Acknowledgments

- **Adafruit** for the NeoPixel library
- **ESP8266 Community** for the Arduino core
- **Marvel Cinematic Universe & Star Wars** for inspiration ✨

---

## 📞 Contact

**Project Maintainer**: Gabriel Kimani  
**Email**: mathengegabriel9@gmail.com  
**LinkedIn**: [Your LinkedIn]

**Note**: This project is designed to showcase professional software engineering principles for portfolio purposes, particularly for FAANG-level interviews.
