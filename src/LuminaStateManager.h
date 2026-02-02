#ifndef LUMINA_STATE_MANAGER_H
#define LUMINA_STATE_MANAGER_H

#include "States.h"
#include "SearchingState.h"
#include <Adafruit_NeoPixel.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

/**
** ============================================================================
** CONCRETE STATE MANAGER IMPLEMENTATION
** ============================================================================
**/
class LuminaStateManager : public StateManager {
private:
    LuminaState* currentState;
    Adafruit_NeoPixel leds;
    WiFiUDP udp;
    unsigned long lastBatteryRead;
    float lastBatteryVoltage;
    uint8_t lastBatteryPercent;
    
    // Voltage divider calculation (adjust R1/R2 for your circuit)
    // ESP8266 ADC: 0-1V = 0-1023
    // 18650: 3.0-4.2V needs voltage divider
    // Example: R1=330k, R2=100k -> 4.2V becomes 0.977V
    static float readBatteryVoltage() {
        constexpr float R1 = 330000.0; // Resistor to battery (ohms)
        constexpr float R2 = 100000.0; // Resistor to ground (ohms)
        constexpr float ADC_REF = 1.0; // ESP8266 ADC reference voltage
        
        int rawADC = analogRead(BATTERY_PIN);
        float adcVoltage = static_cast<float>(rawADC / 1023.0) * ADC_REF;
        float batteryVoltage = adcVoltage * ((R1 + R2) / R2);
        
        // Simple averaging to smooth readings
        static float readings[4] = {0, 0, 0, 0};
        static int index = 0;
        readings[index] = batteryVoltage;
        index = (index + 1) % 4;
        
        float sum = 0;
        for (float reading : readings) sum += reading;
        
        return static_cast<float>(sum / 4.0);
    }
    
    static uint8_t voltageToPercent(float voltage) {
        // Linear approximation: 3.0V = 0%, 4.2V = 100%
        // For more accuracy, use a lookup table with a real discharge curve
        if (voltage >= BATTERY_FULL) return 100;
        if (voltage <= BATTERY_EMPTY) return 0;
        
        return static_cast<uint8_t>((voltage - BATTERY_EMPTY) / (BATTERY_FULL - BATTERY_EMPTY) * 100.0);
    }

public:
    LuminaStateManager() 
        : currentState(nullptr),
          leds(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800),
          lastBatteryRead(0),
          lastBatteryVoltage(0),
          lastBatteryPercent(0) {}
    
    ~LuminaStateManager() override {
        if (currentState) {
            currentState->onExit();
            delete currentState;
        }
    }
    
    void begin() {
        // Initialize LEDs
        leds.begin();
        leds.setBrightness(BRIGHTNESS_MAX);
        leds.clear();
        leds.show();
        
        // Initialize EEPROM
        EEPROM.begin(EEPROM_SIZE);
        
        // Initial battery reading
        lastBatteryVoltage = readBatteryVoltage();
        lastBatteryPercent = voltageToPercent(lastBatteryVoltage);
        lastBatteryRead = millis();
        
        DEBUG_PRINTLN("\n========================================");
        DEBUG_PRINTLN("       LUMINA SMART LAMP v" FIRMWARE_VERSION);
        DEBUG_PRINTLN("========================================");
        DEBUG_PRINTF("Chip ID: %08X\n", ESP.getChipId());
        DEBUG_PRINTF("Flash: %d KB\n", ESP.getFlashChipSize() / 1024);
        DEBUG_PRINTF("Free Heap: %d bytes\n", ESP.getFreeHeap());
        DEBUG_PRINTF("Battery: %.2fV (%d%%)\n", lastBatteryVoltage, lastBatteryPercent);
        DEBUG_PRINTLN("========================================\n");
        
        // Start in SearchingState
        transitionTo(createSearchingState(this));
    }
    
    void update() {
        if (currentState) {
            currentState->update();
        }
        
        // Update battery reading every 10 seconds
        if (millis() - lastBatteryRead > 10000) {
            lastBatteryRead = millis();
            lastBatteryVoltage = readBatteryVoltage();
            lastBatteryPercent = voltageToPercent(lastBatteryVoltage);
        }
        
        // Memory leak detection
        static uint32_t lastHeap = EspClass::getFreeHeap();
        static unsigned long lastHeapCheck = millis();
        
        if (millis() - lastHeapCheck > 30000) { // Check every 30s
            uint32_t currentHeap = EspClass::getFreeHeap();
            
            if (lastHeap - currentHeap > 1024) { // Lost more than 1KB
                DEBUG_PRINTF("⚠ Memory leak detected! Lost %d bytes\n", 
                           lastHeap - currentHeap);
            }
            
            lastHeap = currentHeap;
            lastHeapCheck = millis();
        }
    }
    
    // ========================================================================
    // STATE TRANSITION
    // ========================================================================
    void transitionTo(LuminaState* newState) override {
        if (!newState) {
            DEBUG_PRINTLN("✗ ERROR: Attempted transition to null state");
            return;
        }
        
        // Exit current state
        if (currentState) {
            DEBUG_PRINTF("→ Transitioning from %s to %s\n", 
                       currentState->getName(), 
                       newState->getName());
            currentState->onExit();
            delete currentState; // Free memory!
        } else {
            DEBUG_PRINTF("→ Initial state: %s\n", newState->getName());
        }
        
        // Enter a new state
        currentState = newState;
        currentState->onEnter();
        
        DEBUG_PRINTF("✓ Free Heap after transition: %d bytes\n", ESP.getFreeHeap());
    }
    
    [[nodiscard]] LuminaState* getCurrentState() const override {
        return currentState;
    }
    
    // ========================================================================
    // HARDWARE ACCESS
    // ========================================================================
    Adafruit_NeoPixel* getLEDs() override {
        return &leds;
    }
    
    float getBatteryVoltage() override {
        return lastBatteryVoltage;
    }
    
    uint8_t getBatteryPercent() override {
        return lastBatteryPercent;
    }
    
    // ========================================================================
    // NETWORK ACCESS
    // ========================================================================
    bool sendUDP(const uint8_t* data, size_t len) override {
        if (!isWiFiConnected()) return false;
        
        IPAddress broadcast = WiFi.localIP();
        broadcast[3] = 255;
        
        udp.beginPacket(broadcast, UDP_PORT);
        udp.write(data, len);
        return udp.endPacket() == 1;
    }
    
    bool isWiFiConnected() override {
        return WiFi.status() == WL_CONNECTED;
    }
    
    String getLocalIP() override {
        return WiFi.localIP().toString();
    }
    
    // ========================================================================
    // CREDENTIAL MANAGEMENT (EEPROM)
    // ========================================================================
    bool saveCredentials(const String& ssid, const String& password) override {
        DEBUG_PRINTLN("[EEPROM] Saving credentials...");
        
        if (ssid.length() > 32 || password.length() > 64) {
            DEBUG_PRINTLN("[EEPROM] ✗ Credentials too long");
            return false;
        }
        
        // Write magic byte
        EEPROM.write(ADDR_MAGIC, EEPROM_MAGIC);
        
        // Write SSID
        EEPROM.write(ADDR_SSID_LEN, ssid.length());
        for (size_t i = 0; i < ssid.length(); i++) {
            EEPROM.write(ADDR_SSID + i, ssid[i]);
        }
        
        // Write Password
        EEPROM.write(ADDR_PASS_LEN, password.length());
        for (size_t i = 0; i < password.length(); i++) {
            EEPROM.write(ADDR_PASS + i, password[i]);
        }
        
        // Commit to flash
        if (EEPROM.commit()) {
            DEBUG_PRINTLN("[EEPROM] ✓ Credentials saved");
            return true;
        } else {
            DEBUG_PRINTLN("[EEPROM] ✗ Failed to commit");
            return false;
        }
    }
    
    bool loadCredentials(String& ssid, String& password) override {
        DEBUG_PRINTLN("[EEPROM] Loading credentials...");
        
        // Check magic byte
        if (EEPROM.read(ADDR_MAGIC) != EEPROM_MAGIC) {
            DEBUG_PRINTLN("[EEPROM] ✗ No valid data (bad magic)");
            return false;
        }
        
        // Read SSID
        uint8_t ssidLen = EEPROM.read(ADDR_SSID_LEN);
        if (ssidLen > 32) {
            DEBUG_PRINTLN("[EEPROM] ✗ Invalid SSID length");
            return false;
        }
        
        ssid = "";
        for (int i = 0; i < ssidLen; i++) {
            ssid += static_cast<char>(EEPROM.read(ADDR_SSID + i));
        }
        
        // Read Password
        uint8_t passLen = EEPROM.read(ADDR_PASS_LEN);
        if (passLen > 64) {
            DEBUG_PRINTLN("[EEPROM] ✗ Invalid password length");
            return false;
        }
        
        password = "";
        for (int i = 0; i < passLen; i++) {
            password += static_cast<char>(EEPROM.read(ADDR_PASS + i));
        }
        
        DEBUG_PRINTF("[EEPROM] ✓ Loaded SSID: '%s'\n", ssid.c_str());
        return true;
    }
    
    void clearCredentials() override {
        DEBUG_PRINTLN("[EEPROM] Clearing credentials...");
        
        // Clear magic byte
        EEPROM.write(ADDR_MAGIC, 0x00);
        EEPROM.commit();
        
        DEBUG_PRINTLN("[EEPROM] ✓ Credentials cleared");
    }
    
    // ========================================================================
    // SYSTEM CONTROL
    // ========================================================================
    void reboot() override {
        DEBUG_PRINTLN("\n[SYSTEM] Rebooting in 2 seconds...");
        leds.clear();
        leds.show();
        delay(2000);
        EspClass::restart();
    }
    
    uint32_t getFreeHeap() override {
        return EspClass::getFreeHeap();
    }
};

#endif // LUMINA_STATE_MANAGER_H
