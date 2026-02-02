#ifndef CONNECTED_STATE_H
#define CONNECTED_STATE_H

#include "States.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "Config.h"


/**
 * Lighting Strategies (Strategy Pattern)
 */
class LightingStrategy {
public:
    virtual ~LightingStrategy() = default;
    virtual void apply(Adafruit_NeoPixel* leds, unsigned long time) = 0;
    [[nodiscard]] virtual const char* getName() const = 0;
};

class SolidColorStrategy : public LightingStrategy {
private:
    Color color;
    
public:
    explicit SolidColorStrategy(Color c) : color(c) {}
    
    void apply(Adafruit_NeoPixel* leds, unsigned long time) override {
        for (int i = 0; i < LED_COUNT; i++) {
            leds->setPixelColor(i, color.to32bit());
        }
        leds->show();
    }
    
    [[nodiscard]] const char* getName() const override { return "Solid"; }
};

class CalmBreathingStrategy : public LightingStrategy {
private:
    Color baseColor;
    
public:
    explicit CalmBreathingStrategy(Color c) : baseColor(c) {}
    
    void apply(Adafruit_NeoPixel* leds, unsigned long time) override {
        // Slow sine wave breathing (4-second cycle)
        auto phase = static_cast<float>((time % 4000) / 4000.0 * 2.0 * PI);
        auto brightness = static_cast<float>((sin(phase) + 1.0) / 2.0); // 0.0 to 1.0
        
        auto r = static_cast<uint8_t>(static_cast<float>(baseColor.r) * brightness);
        auto g = static_cast<uint8_t>(static_cast<float>(baseColor.g) * brightness);
        auto b = static_cast<uint8_t>(static_cast<float>(baseColor.b) * brightness);
        
        for (int i = 0; i < LED_COUNT; i++) {
            leds->setPixelColor(i, r, g, b);
        }
        leds->show();
    }
    
    [[nodiscard]] const char* getName() const override { return "Calm"; }
};

class FocusStrategy : public LightingStrategy {
private:
    Color color;
    
public:
    explicit FocusStrategy(Color c) : color(c) {}
    
    void apply(Adafruit_NeoPixel* leds, unsigned long time) override {
        // Steady light with subtle pulsing (slower than calm)
        auto phase = static_cast<float>((time % 8000) / 8000.0 * 2.0 * PI);
        auto brightness = static_cast<float>(0.7 + 0.3 * ((sin(phase) + 1.0) / 2.0)); // 0.7 to 1.0

        auto r = static_cast<uint8_t>(static_cast<float>(color.r) * brightness);
        auto g = static_cast<uint8_t>(static_cast<float>(color.g) * brightness);
        auto b = static_cast<uint8_t>(static_cast<float>(color.b) * brightness);

        for (int i = 0; i < LED_COUNT; i++) {
            leds->setPixelColor(i, r, g, b);
        }
        leds->show();
    }
    
    [[nodiscard]] const char* getName() const override { return "Focus"; }
};

class PartyStrategy : public LightingStrategy {
private:
    Color color1, color2, color3;
    
public:
    PartyStrategy(const Color c1, const Color c2, const Color c3)
        : color1(c1), color2(c2), color3(c3) {}
    
    void apply(Adafruit_NeoPixel* leds, unsigned long time) override {
        // Rotating rainbow segments
        int offset = static_cast<int>((time / 50) % LED_COUNT);
        
        for (int i = 0; i < LED_COUNT; i++) {
            int pos = (i + offset) % LED_COUNT;
            Color c;
            
            if (pos < LED_COUNT / 3) c = color1;
            else if (pos < 2 * LED_COUNT / 3) c = color2;
            else c = color3;
            
            leds->setPixelColor(i, c.to32bit());
        }
        leds->show();
    }
    
    [[nodiscard]] const char* getName() const override { return "Party"; }
};

// ============================================================================
// CONNECTED STATE - Normal operation mode
// ============================================================================
class ConnectedState : public LuminaState {
private:
    WiFiUDP udp;
    LightingStrategy* currentStrategy;
    unsigned long lastHeartbeat;
    unsigned long lastStrategyUpdate;
    unsigned long connectionCheckTime;
    
    void sendHeartbeat() {
        if (millis() - lastHeartbeat < HEARTBEAT_INTERVAL) return;
        lastHeartbeat = millis();
        
        // Build status packet
        uint8_t packet[32];
        packet[0] = STATUS_HEARTBEAT;
        packet[1] = STATE_CONNECTED;
        packet[2] = manager->getBatteryPercent();
        packet[3] = static_cast<uint8_t>(WiFi.RSSI() + 128); // Convert -128..0 to 0..128
        
        float voltage = manager->getBatteryVoltage();
        memcpy(&packet[4], &voltage, sizeof(float));
        
        uint32_t heap = manager->getFreeHeap();
        memcpy(&packet[8], &heap, sizeof(uint32_t));
        
        // Strategy name
        const char* name = currentStrategy->getName();
        packet[12] = strlen(name);
        memcpy(&packet[13], name, packet[12]);
        
        // Broadcast heartbeat
        IPAddress broadcast = WiFi.localIP();
        broadcast[3] = 255; // Make it broadcast
        
        udp.beginPacket(broadcast, UDP_PORT);
        udp.write(packet, 13 + packet[12]);
        udp.endPacket();
        
        DEBUG_PRINTF("[CONNECTED] ♥ Heartbeat | Battery: %d%% | Heap: %d | RSSI: %d\n", 
                    packet[2], heap, WiFi.RSSI());
    }
    
    void checkConnection() {
        if (millis() - connectionCheckTime < 5000) return;
        connectionCheckTime = millis();
        
        if (WiFi.status() != WL_CONNECTED) {
            DEBUG_PRINTLN("[CONNECTED] ✗ WiFi lost, returning to search");
            manager->transitionTo(createSearchingState(manager));
        }
    }

public:
    explicit ConnectedState(StateManager* mgr) 
        : LuminaState(mgr),
          currentStrategy(nullptr),
          lastHeartbeat(0),
          lastStrategyUpdate(0),
          connectionCheckTime(0) {
        // Default to calm green
        currentStrategy = new CalmBreathingStrategy(Colors::CONNECTED);
    }
    
    ~ConnectedState() override
    {
        if (currentStrategy) delete currentStrategy;
        udp.stop();
    }
    
    void onEnter() override {
        stateStartTime = millis();
        DEBUG_PRINTLN("\n[CONNECTED] Entering Connected State");
        DEBUG_PRINTF("[CONNECTED] IP: %s\n", WiFi.localIP().toString().c_str());
        
        // Start UDP listener
        if (udp.begin(UDP_PORT)) {
            DEBUG_PRINTF("[CONNECTED] UDP listening on port %d\n", UDP_PORT);
        }
        
        lastHeartbeat = 0;
        lastStrategyUpdate = millis();
        connectionCheckTime = millis();
        
        // Brief green flash to indicate connection
        Adafruit_NeoPixel* leds = manager->getLEDs();
        for (int i = 0; i < LED_COUNT; i++) {
            leds->setPixelColor(i, Colors::CONNECTED.to32bit());
        }
        leds->show();
        delay(500);
    }
    
    void onExit() override {
        DEBUG_PRINTLN("[CONNECTED] Exiting Connected State");
        udp.stop();
        
        Adafruit_NeoPixel* leds = manager->getLEDs();
        leds->clear();
        leds->show();
    }
    
    void update() override {
        checkConnection();
        sendHeartbeat();
        
        // Update lighting strategy
        if (millis() - lastStrategyUpdate >= FADE_SPEED) {
            lastStrategyUpdate = millis();
            if (currentStrategy) {
                currentStrategy->apply(manager->getLEDs(), millis());
            }
        }
        
        // Check for incoming commands
        int packetSize = udp.parsePacket();
        if (packetSize > 0) {
            uint8_t buffer[256];
            int len = udp.read(buffer, sizeof(buffer));
            
            if (len > 0) {
                handleCommand(buffer[0], &buffer[1], len - 1);
            }
        }
        
        // Battery warning
        float voltage = manager->getBatteryVoltage();
        if (voltage < BATTERY_WARNING && voltage > BATTERY_EMPTY) {
            static unsigned long lastWarning = 0;
            if (millis() - lastWarning > 30000) { // Every 30s
                lastWarning = millis();
                DEBUG_PRINTF("[CONNECTED] ⚠ Low battery: %.2fV\n", voltage);
            }
        }
    }
    
    void handleCommand(uint8_t cmd, uint8_t* data, size_t len) override {
        DEBUG_PRINTF("[CONNECTED] Command: 0x%02X\n", cmd);
        
        switch (cmd) {
            case CMD_SET_COLOR: {
                if (len < 3) break;
                
                Color newColor(data[0], data[1], data[2]);
                
                if (currentStrategy) delete currentStrategy;
                currentStrategy = new SolidColorStrategy(newColor);
                
                DEBUG_PRINTF("[CONNECTED] Set color: RGB(%d,%d,%d)\n", 
                           data[0], data[1], data[2]);
                break;
            }
            
            case CMD_SET_MOOD: {
                if (len < 4) break;
                
                uint8_t moodType = data[0];
                Color color(data[1], data[2], data[3]);
                
                if (currentStrategy) delete currentStrategy;
                
                switch (moodType) {
                    case 0: // Calm
                        currentStrategy = new CalmBreathingStrategy(color);
                        DEBUG_PRINTLN("[CONNECTED] Mood: Calm");
                        break;
                    case 1: // Focus
                        currentStrategy = new FocusStrategy(color);
                        DEBUG_PRINTLN("[CONNECTED] Mood: Focus");
                        break;
                    case 2: // Party
                        if (len >= 10) {
                            Color c2(data[4], data[5], data[6]);
                            Color c3(data[7], data[8], data[9]);
                            currentStrategy = new PartyStrategy(color, c2, c3);
                        } else {
                            currentStrategy = new PartyStrategy(color, Colors::CONNECTED, Colors::SEARCHING);
                        }
                        DEBUG_PRINTLN("[CONNECTED] Mood: Party");
                        break;
                    default:
                        currentStrategy = new SolidColorStrategy(color);
                        break;
                }
                break;
            }
            
            case CMD_SET_BRIGHTNESS: {
                if (len < 1) break;
                
                uint8_t brightness = data[0];
                Adafruit_NeoPixel* leds = manager->getLEDs();
                leds->setBrightness(brightness);
                leds->show();
                
                DEBUG_PRINTF("[CONNECTED] Brightness: %d\n", brightness);
                break;
            }
            
            case CMD_GET_STATUS: {
                sendHeartbeat(); // Send immediate status
                break;
            }
            
            case CMD_OTA_START: {
                DEBUG_PRINTLN("[CONNECTED] OTA update requested");
                manager->transitionTo(createUpdatingState(manager));
                break;
            }
            
            case CMD_RESET: {
                DEBUG_PRINTLN("[CONNECTED] Reset requested");
                manager->clearCredentials();
                delay(500);
                manager->reboot();
                break;
            }
            
            default:
                DEBUG_PRINTF("[CONNECTED] Unknown command: 0x%02X\n", cmd);
                break;
        }
    }
    
    [[nodiscard]] const char* getName() const override { return "Connected"; }
    [[nodiscard]] uint8_t getStateCode() const override { return STATE_CONNECTED; }
};

// Factory function
inline LuminaState* createConnectedState(StateManager* manager) {
    return new ConnectedState(manager);
}

#endif // CONNECTED_STATE_H
