#ifndef CONFIG_H
#define CONFIG_H

/**
 * Hardware Configuration
 */
#define LED_PIN            D6        // WS2812B data pin
#define LED_COUNT          16        // Number of LEDs in the ring
#define BATTERY_PIN        A0        // ADC for battery voltage monitoring

// Power thresholds (18650 Li-ion: 4.2V full, 3.0V empty)
#define BATTERY_FULL       4.2f
#define BATTERY_EMPTY      3.0f
#define BATTERY_WARNING    3.3f


/**
 * Network Configuration
 */
#define UDP_PORT           4210      // Device listens on this port
#define HEARTBEAT_INTERVAL 5000      // Send status every 5 seconds (ms)
#define WIFI_TIMEOUT       30000     // 30 seconds to connect
#define AP_SSID            "Lumina-Setup"
#define AP_PASSWORD        "lumina2026"

/**
 * State Timing Configuration
 */
#define SEARCHING_DURATION 30000     // Pulse blue for 30 seconds
#define PROVISION_TIMEOUT  300000    // 5 minutes in AP mode
#define HEARTBEAT_TIMEOUT  10000     // Consider disconnected after 10 seconds


/**
 * Memory Management
 */
#define MIN_FREE_HEAP      8192      // Minimum free heap before warning
#define EEPROM_SIZE        512       // EEPROM allocation for credentials

// EEPROM Memory Map
#define EEPROM_MAGIC       0xA5      // Magic byte to verify valid data
#define ADDR_MAGIC         0         // Magic byte address
#define ADDR_SSID_LEN      1         // SSID length (1 byte)
#define ADDR_SSID          2         // SSID start (max 32 bytes)
#define ADDR_PASS_LEN      34        // Password length (1 byte)
#define ADDR_PASS          35        // Password start (max 64 bytes)

/**
 * LED Animation Settings
 */
#define PULSE_SPEED        50        // Animation update interval (ms)
#define FADE_SPEED         20        // Color transition speed (ms)
#define BRIGHTNESS_MAX     200       // Max brightness (0-255)
#define BRIGHTNESS_MIN     10        // Min brightness for pulse

/**
 * Communication Protocol
 */
// Command Types (1 byte)
#define CMD_SET_COLOR      0x01      // Set solid color
#define CMD_SET_MOOD       0x02      // AI-generated mood lighting
#define CMD_SET_BRIGHTNESS 0x03      // Adjust brightness
#define CMD_GET_STATUS     0x04      // Request device status
#define CMD_PROVISION      0x05      // Send Wi-Fi credentials
#define CMD_OTA_START      0x06      // Begin OTA update
#define CMD_RESET          0xFF      // Factory reset

// Status Types (1 byte)
#define STATUS_HEARTBEAT   0x10      // Regular status broadcast
#define STATUS_BATTERY     0x11      // Battery level update
#define STATUS_ERROR       0x12      // Error notification
#define STATUS_STATE       0x13      // State change notification


/**
 * Debug Settings
 */
#define DEBUG_MODE         true      // Enable serial debugging
#define SERIAL_BAUD        115200    // Serial monitor baud rate

#if DEBUG_MODE
  #define DEBUG_PRINT(x)   Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...)Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

/**
 * Color Presets(RGB Values)
 */
struct Color {
    uint8_t r, g, b;

    explicit Color(const uint8_t red = 0, const uint8_t green = 0, const uint8_t blue = 0)
        : r(red), g(green), b(blue) {}
        
    [[nodiscard]] uint32_t to32bit() const {
        return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
    }
};

namespace Colors {
    const Color OFF(0, 0, 0);
    const Color SEARCHING(0, 50, 255);      // Blue pulse
    const Color PROVISIONING(255, 165, 0);  // Orange
    const Color CONNECTED(0, 255, 0);       // Green
    const Color UPDATING(255, 255, 0);      // Yellow
    const Color ERROR_COLOR(255, 0, 0);     // Red
    const Color LOW_BATTERY(255, 100, 0);   // Orange-Red
}


/**
 * Device Information
 */
#define FIRMWARE_VERSION   "1.0.0"
#define DEVICE_NAME        "Lumina"
#define MANUFACTURER       "Gabriel Kimani"

#endif // CONFIG_H
