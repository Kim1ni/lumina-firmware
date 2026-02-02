#ifndef SEARCHING_STATE_H
#define SEARCHING_STATE_H

#include "States.h"
#include <ESP8266WiFi.h>

#include "Config.h"
#include "ProvisioningState.h"
#include "ConnectedState.h"

// ============================================================================
// SEARCHING STATE - Attempting to connect to saved WiFi
// ============================================================================
class SearchingState : public LuminaState {
private:
    uint8_t pulseValue;
    bool pulseDirection;
    unsigned long lastPulseUpdate;
    String ssid;
    String password;
    unsigned long lastConnectionAttempt;
    
    void updatePulseAnimation() {
        if (millis() - lastPulseUpdate < PULSE_SPEED) return;
        lastPulseUpdate = millis();
        
        // Smooth sine-wave pulse
        if (pulseDirection) {
            pulseValue += 5;
            if (pulseValue >= BRIGHTNESS_MAX) {
                pulseValue = BRIGHTNESS_MAX;
                pulseDirection = false;
            }
        } else {
            pulseValue -= 5;
            if (pulseValue <= BRIGHTNESS_MIN) {
                pulseValue = BRIGHTNESS_MIN;
                pulseDirection = true;
            }
        }
        
        // Apply blue color with current pulse brightness
        Adafruit_NeoPixel* leds = manager->getLEDs();
        uint8_t scaledBlue = map(pulseValue, 0, 255, 0, Colors::SEARCHING.b);
        
        for (int i = 0; i < LED_COUNT; i++) {
            leds->setPixelColor(i, 0, 0, scaledBlue);
        }
        leds->show();
    }
    
    void attemptConnection() {
        if (millis() - lastConnectionAttempt < 5000) return; // Try every 5s
        lastConnectionAttempt = millis();
        
        DEBUG_PRINTF("[SEARCHING] Attempting to connect to '%s'...\n", ssid.c_str());
        
        if (WiFi.status() == WL_CONNECTED) {
            DEBUG_PRINTLN("[SEARCHING] ✓ WiFi connected!");
            DEBUG_PRINTF("[SEARCHING] IP: %s\n", WiFi.localIP().toString().c_str());
            
            // Success! Transition to ConnectedState
            manager->transitionTo(createConnectedState(manager));
        }
    }

public:
    explicit SearchingState(StateManager* mgr) 
        : LuminaState(mgr), 
          pulseValue(BRIGHTNESS_MIN), 
          pulseDirection(true),
          lastPulseUpdate(0),
          lastConnectionAttempt(0) {}
    
    void onEnter() override {
        stateStartTime = millis();
        DEBUG_PRINTLN("\n[SEARCHING] Entering Searching State");
        
        // Load saved credentials
        if (!manager->loadCredentials(ssid, password)) {
            DEBUG_PRINTLN("[SEARCHING] ✗ No saved credentials, entering provisioning");
            manager->transitionTo(createProvisioningState(manager));
            return;
        }
        
        DEBUG_PRINTF("[SEARCHING] Found credentials for '%s'\n", ssid.c_str());
        
        // Begin Wi-Fi connection
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), password.c_str());
        
        // Initialize pulse animation
        pulseValue = BRIGHTNESS_MIN;
        pulseDirection = true;
        lastPulseUpdate = millis();
        lastConnectionAttempt = millis();
    }
    
    void onExit() override {
        DEBUG_PRINTLN("[SEARCHING] Exiting Searching State");
        // Turn off LEDs
        Adafruit_NeoPixel* leds = manager->getLEDs();
        leds->clear();
        leds->show();
    }
    
    void update() override {
        updatePulseAnimation();
        attemptConnection();
        
        // Check for timeout
        if (hasTimedOut(WIFI_TIMEOUT)) {
            handleTimeout();
        }
        
        // Memory safety check
        if (manager->getFreeHeap() < MIN_FREE_HEAP) {
            DEBUG_PRINTF("[SEARCHING] ⚠ Low memory: %d bytes\n", manager->getFreeHeap());
        }
    }
    
    void handleTimeout() override {
        DEBUG_PRINTLN("[SEARCHING] ✗ Connection timeout, entering provisioning mode");
        
        // Failed to connect, enter provisioning
        manager->transitionTo(createProvisioningState(manager));
    }
    
    void handleCommand(uint8_t cmd, uint8_t* data, size_t len) override {
        // In searching state, only respond to the provision command
        if (cmd == CMD_PROVISION && len >= 2) {
            DEBUG_PRINTLN("[SEARCHING] Received provision command");
            
            // Extract SSID and password from data
            uint8_t ssidLen = data[0];
            if (ssidLen > 32 || len < ssidLen + 2) return; // Invalid
            
            String newSSID = "";
            for (int i = 0; i < ssidLen; i++) {
                newSSID += static_cast<char>(data[1 + i]);
            }
            
            uint8_t passLen = data[1 + ssidLen];
            if (passLen > 64 || len < ssidLen + passLen + 2) return; // Invalid
            
            String newPassword = "";
            for (int i = 0; i < passLen; i++) {
                newPassword += static_cast<char>(data[2 + ssidLen + i]);
            }
            
            // Save and reconnect
            if (manager->saveCredentials(newSSID, newPassword)) {
                DEBUG_PRINTLN("[SEARCHING] New credentials saved, rebooting...");
                delay(1000);
                manager->reboot();
            }
        }
        else if (cmd == CMD_RESET) {
            DEBUG_PRINTLN("[SEARCHING] Factory reset requested");
            manager->clearCredentials();
            manager->reboot();
        }
    }
    
    [[nodiscard]] const char* getName() const override { return "Searching"; }
    [[nodiscard]] uint8_t getStateCode() const override { return STATE_SEARCHING; }
};

// Factory function
inline LuminaState* createSearchingState(StateManager* manager) {
    return new SearchingState(manager);
}

#endif // SEARCHING_STATE_H
