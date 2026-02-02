#ifndef STATES_H
#define STATES_H

#include <Adafruit_NeoPixel.h>

// Forward declaration to avoid circular dependency
class StateManager;

// ============================================================================
// ABSTRACT STATE BASE CLASS
// ============================================================================
class LuminaState {
public:
    virtual ~LuminaState() = default;
    
    // State lifecycle methods
    virtual void onEnter() = 0;           // Called when entering this state
    virtual void onExit() = 0;            // Called when leaving this state
    virtual void update() = 0;            // Called every loop iteration
    
    // Event handlers
    virtual void handleCommand(uint8_t cmd, uint8_t* data, size_t len) = 0;
    virtual void handleTimeout() {}       // Optional timeout handling
    
    // State identification
    [[nodiscard]] virtual const char* getName() const = 0;
    [[nodiscard]] virtual uint8_t getStateCode() const = 0;
    
protected:
    StateManager* manager;                // Reference to state manager
    unsigned long stateStartTime;         // Timestamp when state started
    
    explicit LuminaState(StateManager* mgr) : manager(mgr), stateStartTime(0) {}
    
    // Helper to check if the state has been active too long
    [[nodiscard]] bool hasTimedOut(unsigned long timeout) const {
        return (millis() - stateStartTime) > timeout;
    }
};

// ============================================================================
// STATE MANAGER INTERFACE
// ============================================================================
class StateManager {
public:
    virtual ~StateManager() = default;
    
    // State transitions
    virtual void transitionTo(LuminaState* newState) = 0;
    [[nodiscard]] virtual LuminaState* getCurrentState() const = 0;
    
    // Hardware access (safely shared across states)
    virtual Adafruit_NeoPixel* getLEDs() = 0;
    virtual float getBatteryVoltage() = 0;
    virtual uint8_t getBatteryPercent() = 0;
    
    // Network access
    virtual bool sendUDP(const uint8_t* data, size_t len) = 0;
    virtual bool isWiFiConnected() = 0;
    virtual String getLocalIP() = 0;
    
    // Credential management
    virtual bool saveCredentials(const String& ssid, const String& password) = 0;
    virtual bool loadCredentials(String& ssid, String& password) = 0;
    virtual void clearCredentials() = 0;
    
    // System control
    virtual void reboot() = 0;
    virtual uint32_t getFreeHeap() = 0;
};

// ============================================================================
// STATE CODE DEFINITIONS (for protocol communication)
// ============================================================================
#define STATE_SEARCHING     0x01
#define STATE_PROVISIONING  0x02
#define STATE_CONNECTED     0x03
#define STATE_UPDATING      0x04
#define STATE_ERROR         0xFF

// Forward declarations for state creation functions
extern LuminaState* createSearchingState(StateManager* manager);
extern LuminaState* createProvisioningState(StateManager* manager);
extern LuminaState* createConnectedState(StateManager* manager);
extern LuminaState* createUpdatingState(StateManager* manager);

#endif // STATES_H
