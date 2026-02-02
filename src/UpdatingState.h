#ifndef UPDATING_STATE_H
#define UPDATING_STATE_H

#include "States.h"
#include <ArduinoOTA.h>
#include "Config.h"
#include "ConnectedState.h"
#include "SearchingState.h"

// ============================================================================
// UPDATING STATE - Handles OTA firmware updates
// ============================================================================
/**
 * Updating State - Handles OTA updates
 */
class UpdatingState : public LuminaState {
private:
    unsigned long lastPulse;
    uint8_t yellowBrightness;
    bool pulseDirection;
    bool otaConfigured;
    uint8_t lastProgress;
    LuminaState* previousState; // For rollback on failure
    
    void updateYellowPulse() {
        if (millis() - lastPulse < 30) return; // Fast pulse during update
        lastPulse = millis();
        
        if (pulseDirection) {
            yellowBrightness += 10;
            if (yellowBrightness >= 200) {
                yellowBrightness = 200;
                pulseDirection = false;
            }
        } else {
            yellowBrightness -= 10;
            if (yellowBrightness <= 20) {
                yellowBrightness = 20;
                pulseDirection = true;
            }
        }
        
        Adafruit_NeoPixel* leds = manager->getLEDs();
        for (int i = 0; i < LED_COUNT; i++) {
            leds->setPixelColor(i, yellowBrightness, yellowBrightness, 0);
        }
        leds->show();
    }
    
    void showProgress(uint8_t percent) {
        if (percent == lastProgress) return;
        lastProgress = percent;
        
        Adafruit_NeoPixel* leds = manager->getLEDs();
        leds->clear();
        
        // Fill LEDs proportionally to progress
        int ledsToLight = (percent * LED_COUNT) / 100;
        for (int i = 0; i < ledsToLight; i++) {
            leds->setPixelColor(i, Colors::UPDATING.to32bit());
        }
        leds->show();
        
        DEBUG_PRINTF("[UPDATE] Progress: %d%%\n", percent);
    }
    
    void setupOTA() {
        if (otaConfigured) return;
        
        ArduinoOTA.setHostname(DEVICE_NAME);
        ArduinoOTA.setPassword("lumina-ota-2026"); // OTA password
        
        ArduinoOTA.onStart([this]() {
            String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
            DEBUG_PRINTF("[UPDATE] Starting OTA: %s\n", type.c_str());
            
            // Turn off LEDs during flash
            Adafruit_NeoPixel* leds = manager->getLEDs();
            leds->clear();
            leds->show();
        });
        
        ArduinoOTA.onEnd([this]() {
            DEBUG_PRINTLN("\n[UPDATE] ✓ OTA Complete!");
            
            // Success animation - green sweep
            Adafruit_NeoPixel* leds = manager->getLEDs();
            for (int i = 0; i < LED_COUNT; i++) {
                leds->setPixelColor(i, Colors::CONNECTED.to32bit());
                leds->show();
                delay(50);
            }
        });
        
        ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
            uint8_t percent = (progress * 100) / total;
            showProgress(percent);
        });
        
        ArduinoOTA.onError([this](ota_error_t error) {
            DEBUG_PRINTF("\n[UPDATE] ✗ Error[%u]: ", error);
            
            const char* errorMsg = "Unknown";
            switch (error) {
                case OTA_AUTH_ERROR: errorMsg = "Auth Failed"; break;
                case OTA_BEGIN_ERROR: errorMsg = "Begin Failed"; break;
                case OTA_CONNECT_ERROR: errorMsg = "Connect Failed"; break;
                case OTA_RECEIVE_ERROR: errorMsg = "Receive Failed"; break;
                case OTA_END_ERROR: errorMsg = "End Failed"; break;
            }
            DEBUG_PRINTLN(errorMsg);
            
            // Error animation - red flash
            Adafruit_NeoPixel* leds = manager->getLEDs();
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < LED_COUNT; j++) {
                    leds->setPixelColor(j, Colors::ERROR_COLOR.to32bit());
                }
                leds->show();
                delay(200);
                leds->clear();
                leds->show();
                delay(200);
            }
            
            // Rollback to the previous state
            DEBUG_PRINTLN("[UPDATE] Rolling back to previous state...");
            delay(1000);
            
            manager->transitionTo(createConnectedState(manager));
        });
        
        ArduinoOTA.begin();
        otaConfigured = true;
        DEBUG_PRINTLN("[UPDATE] OTA configured and ready");
    }

public:
    explicit UpdatingState(StateManager* mgr) 
        : LuminaState(mgr),
          lastPulse(0),
          yellowBrightness(20),
          pulseDirection(true),
          otaConfigured(false),
          lastProgress(0),
          previousState(nullptr) {}
    
    void onEnter() override {
        stateStartTime = millis();
        DEBUG_PRINTLN("\n[UPDATE] Entering Updating State");
        DEBUG_PRINTLN("[UPDATE] ⚠ Device locked during update");
        
        // Check if Wi-Fi is connected
        if (WiFi.status() != WL_CONNECTED) {
            DEBUG_PRINTLN("[UPDATE] ✗ No WiFi, cannot update");
            manager->transitionTo(createSearchingState(manager));
            return;
        }
        
        setupOTA();
        
        lastPulse = millis();
        yellowBrightness = 20;
        pulseDirection = true;
        lastProgress = 0;
    }
    
    void onExit() override {
        DEBUG_PRINTLN("[UPDATE] Exiting Updating State");
        ArduinoOTA.end();
        otaConfigured = false;
        
        Adafruit_NeoPixel* leds = manager->getLEDs();
        leds->clear();
        leds->show();
    }
    
    void update() override {
        // Handle OTA
        ArduinoOTA.handle();
        
        // Update visual feedback
        updateYellowPulse();
        
        // Timeout after 10 minutes of no activity
        if (hasTimedOut(600000)) {
            DEBUG_PRINTLN("[UPDATE] Update timeout, returning to connected");
            manager->transitionTo(createConnectedState(manager));
        }
    }
    
    void handleCommand(uint8_t cmd, uint8_t* data, size_t len) override {
        // During update, only respond to status requests
        if (cmd == CMD_GET_STATUS) {
            DEBUG_PRINTLN("[UPDATE] Status requested during update");
            // Send update-in-progress status
            uint8_t response[3] = {STATUS_STATE, STATE_UPDATING, lastProgress};
            manager->sendUDP(response, 3);
        }
        // Ignore all other commands during update for safety
    }
    
    const char* getName() const override { return "Updating"; }
    uint8_t getStateCode() const override { return STATE_UPDATING; }
};

// Factory function
inline LuminaState* createUpdatingState(StateManager* manager) {
    return new UpdatingState(manager);
}

#endif // UPDATING_STATE_H
