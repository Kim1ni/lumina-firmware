#ifndef PROVISIONING_STATE_H
#define PROVISIONING_STATE_H

#include "States.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "Config.h"
#include "SearchingState.h"

// ============================================================================
// PROVISIONING STATE - Device acts as Access Point for setup
// ============================================================================
class ProvisioningState : public LuminaState {
private:
    WiFiUDP udp;
    unsigned long lastBroadcast;
    uint8_t orangePhase;
    unsigned long lastAnimUpdate;
    
    void updateOrangeAnimation() {
        if (millis() - lastAnimUpdate < 100) return;
        lastAnimUpdate = millis();
        
        // Rotating orange segments
        Adafruit_NeoPixel* leds = manager->getLEDs();
        leds->clear();
        
        for (int i = 0; i < 4; i++) {
            int pos = (orangePhase + i * 4) % LED_COUNT;
            leds->setPixelColor(pos, Colors::PROVISIONING.to32bit());
        }
        
        leds->show();
        orangePhase = (orangePhase + 1) % LED_COUNT;
    }
    
    void broadcastPresence() {
        if (millis() - lastBroadcast < 2000) return; // Every 2 seconds
        lastBroadcast = millis();
        
        // Broadcast announcement packet so app can discover us
        uint8_t packet[64];
        packet[0] = STATUS_STATE;
        packet[1] = STATE_PROVISIONING;
        packet[2] = strlen(DEVICE_NAME);
        memcpy(&packet[3], DEVICE_NAME, packet[2]);
        
        // Broadcast to 255.255.255.255
        IPAddress broadcast(255, 255, 255, 255);
        udp.beginPacket(broadcast, UDP_PORT);
        udp.write(packet, 3 + packet[2]);
        udp.endPacket();
        
        DEBUG_PRINTLN("[PROVISION] Broadcasting presence...");
    }

public:
    explicit ProvisioningState(StateManager* mgr) 
        : LuminaState(mgr),
          lastBroadcast(0),
          orangePhase(0),
          lastAnimUpdate(0) {}
    
    ~ProvisioningState() override
    {
        udp.stop();
    }
    
    void onEnter() override {
        stateStartTime = millis();
        DEBUG_PRINTLN("\n[PROVISION] Entering Provisioning State");
        
        // Stop any existing Wi-Fi connection
        WiFi.disconnect();
        delay(100);
        
        // Create Access Point
        DEBUG_PRINTF("[PROVISION] Creating AP '%s'...\n", AP_SSID);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        
        IPAddress apIP = WiFi.softAPIP();
        DEBUG_PRINTF("[PROVISION] AP IP: %s\n", apIP.toString().c_str());
        
        // Start UDP listener
        if (udp.begin(UDP_PORT)) {
            DEBUG_PRINTF("[PROVISION] UDP listening on port %d\n", UDP_PORT);
        } else {
            DEBUG_PRINTLN("[PROVISION] ✗ Failed to start UDP");
        }
        
        // Initialize animation
        orangePhase = 0;
        lastAnimUpdate = millis();
        lastBroadcast = 0;
    }
    
    void onExit() override {
        DEBUG_PRINTLN("[PROVISION] Exiting Provisioning State");
        udp.stop();
        WiFi.softAPdisconnect(true);
        
        Adafruit_NeoPixel* leds = manager->getLEDs();
        leds->clear();
        leds->show();
    }
    
    void update() override {
        updateOrangeAnimation();
        broadcastPresence();
        
        // Check for incoming UDP packets
        int packetSize = udp.parsePacket();
        if (packetSize > 0) {
            uint8_t buffer[256];
            int len = udp.read(buffer, sizeof(buffer));
            
            if (len > 0) {
                handleCommand(buffer[0], &buffer[1], len - 1);
            }
        }
        
        // Timeout check (return to searching after 5 minutes)
        if (hasTimedOut(PROVISION_TIMEOUT)) {
            handleTimeout();
        }
    }
    
    void handleTimeout() override {
        DEBUG_PRINTLN("[PROVISION] ✗ Provisioning timeout, returning to search");
        manager->transitionTo(createSearchingState(manager));
    }
    
    void handleCommand(uint8_t cmd, uint8_t* data, size_t len) override {
        DEBUG_PRINTF("[PROVISION] Received command: 0x%02X\n", cmd);
        
        switch (cmd) {
            case CMD_PROVISION: {
                if (len < 2) {
                    DEBUG_PRINTLN("[PROVISION] ✗ Invalid provision data");
                    break;
                }
                
                // Parse SSID
                uint8_t ssidLen = data[0];
                if (ssidLen > 32 || len < ssidLen + 2) {
                    DEBUG_PRINTLN("[PROVISION] ✗ Invalid SSID length");
                    break;
                }
                
                String ssid = "";
                for (int i = 0; i < ssidLen; i++) {
                    ssid += static_cast<char>(data[1 + i]);
                }
                
                // Parse Password
                uint8_t passLen = data[1 + ssidLen];
                if (passLen > 64 || len < ssidLen + passLen + 2) {
                    DEBUG_PRINTLN("[PROVISION] ✗ Invalid password length");
                    break;
                }
                
                String password = "";
                for (int i = 0; i < passLen; i++) {
                    password += static_cast<char>(data[2 + ssidLen + i]);
                }
                
                DEBUG_PRINTF("[PROVISION] Received credentials:\n  SSID: %s\n  Pass: %s\n", 
                           ssid.c_str(), 
                           password.length() > 0 ? "***" : "(empty)");
                
                // Save credentials
                if (manager->saveCredentials(ssid, password)) {
                    // Send success response
                    uint8_t response[2] = {STATUS_STATE, STATE_SEARCHING};
                    udp.beginPacket(udp.remoteIP(), udp.remotePort());
                    udp.write(response, 2);
                    udp.endPacket();
                    
                    DEBUG_PRINTLN("[PROVISION] ✓ Credentials saved, rebooting...");
                    
                    // Flash green to indicate success
                    Adafruit_NeoPixel* leds = manager->getLEDs();
                    for (int i = 0; i < LED_COUNT; i++) {
                        leds->setPixelColor(i, Colors::CONNECTED.to32bit());
                    }
                    leds->show();
                    delay(2000);
                    
                    manager->reboot();
                }
                break;
            }
            
            case CMD_GET_STATUS: {
                // Send device info
                uint8_t response[32];
                response[0] = STATUS_STATE;
                response[1] = STATE_PROVISIONING;
                response[2] = manager->getBatteryPercent();
                response[3] = strlen(FIRMWARE_VERSION);
                memcpy(&response[4], FIRMWARE_VERSION, response[3]);
                
                udp.beginPacket(udp.remoteIP(), udp.remotePort());
                udp.write(response, 4 + response[3]);
                udp.endPacket();
                
                DEBUG_PRINTLN("[PROVISION] Sent status response");
                break;
            }
            
            case CMD_RESET: {
                DEBUG_PRINTLN("[PROVISION] Factory reset requested");
                manager->clearCredentials();
                
                // Send acknowledgment
                uint8_t response[1] = {STATUS_STATE};
                udp.beginPacket(udp.remoteIP(), udp.remotePort());
                udp.write(response, 1);
                udp.endPacket();
                
                delay(1000);
                manager->reboot();
                break;
            }
            
            default:
                DEBUG_PRINTF("[PROVISION] Unknown command: 0x%02X\n", cmd);
                break;
        }
    }
    
    const char* getName() const override { return "Provisioning"; }
    uint8_t getStateCode() const override { return STATE_PROVISIONING; }
};

// Factory function
inline LuminaState* createProvisioningState(StateManager* manager) {
    return new ProvisioningState(manager);
}

#endif // PROVISIONING_STATE_H
