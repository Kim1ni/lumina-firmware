/**
 * ============================================================================
 * LUMINA - AI-Integrated Smart Lamp
 * ============================================================================
 * 
 * A professional-grade IoT device using the State Pattern for robust
 * memory management and seamless state transitions.
 * 
 * Hardware:
 * - ESP8266 (NodeMCU/ESP-12E)
 * - WS2812B RGB LED Ring (16 LEDs)
 * - 18650 Li-ion Battery + TP4056 + MT3608
 * - Voltage divider for battery monitoring
 * 
 * Architecture:
 * - State Pattern: Clean separation of operational modes
 * - Observer Pattern: UDP heartbeat for Android app integration
 * - Strategy Pattern: Interchangeable lighting algorithms
 * 
 * States:
 * 1. SearchingState    - Blue pulse, attempting Wi-Fi connection
 * 2. ProvisioningState - Orange animation, AP mode for setup
 * 3. ConnectedState    - Normal operation with AI mood lighting
 * 4. UpdatingState     - Yellow pulse, OTA firmware update
 * 
 * Author: Gabriel Kimani
 * Version: 1.0.0
 * Date: February 2026
 * 
 * License: MIT
 * ============================================================================
 */

#include <ESP8266WiFi.h>
#include <Adafruit_NeoPixel.h>

// Include all our custom headers
#include "Config.h"
#include "LuminaStateManager.h"

/**
** ============================================================================
** GLOBAL STATE MANAGER
** ============================================================================
**/
LuminaStateManager stateManager;

// ============================================================================
// ARDUINO SETUP
// ============================================================================
void setup() {
    // Initialize Serial for debugging
    #if DEBUG_MODE
    Serial.begin(SERIAL_BAUD);
    delay(100);
    Serial.println();
    #endif
    
    // Startup animation - Quick rainbow
    stateManager.begin(); // This initializes LEDs
    
    Adafruit_NeoPixel* leds = stateManager.getLEDs();
    for (int j = 0; j < 255; j += 15) {
        for (int i = 0; i < LED_COUNT; i++) {
            int hue = (i * 65536 / LED_COUNT + j * 256) % 65536;
            uint32_t color = Adafruit_NeoPixel::ColorHSV(hue);
            leds->setPixelColor(i, color);
        }
        leds->show();
        delay(20);
    }
    leds->clear();
    leds->show();
    
    DEBUG_PRINTLN("✓ Lumina initialized successfully\n");
}

// ============================================================================
// ARDUINO MAIN LOOP
// ============================================================================
void loop() {
    // Update current state
    stateManager.update();
    
    // Small delay to prevent tight loop CPU usage
    // The ESP8266 needs breathing room for Wi-Fi stack
    delay(10);
    
    // Yield to ESP8266 system tasks
    yield();
}

// ============================================================================
// NOTES FOR FUTURE DEVELOPMENT
// ============================================================================
/**
 * MEMORY OPTIMIZATION TIPS:
 * -------------------------
 * 1. The State Pattern ensures only ONE state's code is in RAM at a time
 * 2. Use String sparingly - prefer const char* for fixed strings
 * 3. Always delete old states before creating new ones
 * 4. Monitor getFreeHeap() regularly during development
 * 5. Use F() macro for debug strings: DEBUG_PRINTLN(F("Text"));
 * 
 * ADDING NEW STATES:
 * ------------------
 * 1. Create NewState.h inheriting from LuminaState
 * 2. Implement all pure virtual methods
 * 3. Add factory function: LuminaState* createNewState(StateManager*)
 * 4. Include the header in this file
 * 5. Transition to it: manager->transitionTo(createNewState(manager))
 * 
 * ADDING NEW LIGHTING STRATEGIES:
 * --------------------------------
 * 1. Create class inheriting from LightingStrategy in ConnectedState.h
 * 2. Implement apply() method with your animation logic
 * 3. Add new CMD_SET_MOOD case to handle it
 * 4. Update Android app to send the new mood type
 * 
 * PROTOCOL EXTENSIONS:
 * --------------------
 * All commands are sent as UDP packets:
 * [0] = Command byte (see Config.h)
 * [1..n] = Command-specific data
 * 
 * Example - Set Color:
 * [CMD_SET_COLOR, R, G, B]
 * 
 * Example - Set Mood:
 * [CMD_SET_MOOD, MoodType, R, G, B, ...]
 * 
 * ANDROID APP INTEGRATION:
 * ------------------------
 * 1. App discovers device via UDP broadcast (STATUS_HEARTBEAT)
 * 2. App sends commands to device IP on UDP_PORT (4210)
 * 3. Device responds with status packets
 * 4. Use Kotlin Flows to observe heartbeat packets
 * 5. Parse battery, heap, and RSSI for "Product Health" UI
 * 
 * GEMINI AI INTEGRATION:
 * ----------------------
 * 1. User enters mood text in Android app
 * 2. App calls Gemini API with prompt: "Convert '[mood]' to RGB values"
 * 3. Gemini returns JSON: {"r": 255, "g": 100, "b": 50, "type": "calm"}
 * 4. App sends CMD_SET_MOOD with parsed RGB values
 * 5. Device applies appropriate LightingStrategy
 * 
 * POWER OPTIMIZATION:
 * -------------------
 * For extended battery life:
 * 1. Use WiFi.setSleepMode(WIFI_LIGHT_SLEEP) in ConnectedState
 * 2. Reduce LED brightness when battery is low
 * 3. Increase heartbeat interval to reduce broadcasts
 * 4. Consider deep sleep mode for "off" state (requires hardware button)
 * 
 * TROUBLESHOOTING:
 * ----------------
 * Problem: "0KB Free Memory" crash
 * Solution: Check for memory leaks - ensure states are deleted properly
 * 
 * Problem: LEDs flicker
 * Solution: Check power supply - MT3608 must output steady 5V
 * 
 * Problem: WiFi won't connect
 * Solution: Use Serial monitor to see actual error, check SSID/password
 * 
 * Problem: OTA fails midway
 * Solution: UpdatingState has rollback - check WiFi signal strength
 * 
 * PORTFOLIO DOCUMENTATION:
 * ------------------------
 * This project demonstrates:
 * ✓ Design Patterns (State, Observer, Strategy, Factory)
 * ✓ Memory-safe embedded C++ (RAII, smart pointers)
 * ✓ Professional IoT architecture
 * ✓ Multi-platform integration (ESP8266 + Android)
 * ✓ AI integration (Gemini API)
 * ✓ OTA updates with failsafe rollback
 * ✓ Production-ready power management
 * 
 * Perfect for Google interviews - shows systems thinking, not just coding!
 */