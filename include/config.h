#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>




// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GATEWAY CONFIGURATION                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

extern const uint8_t GATEWAY_NODE_ID;   // Which node ID hosts the dashboard
extern const bool IS_GATEWAY;           // Auto-set: true if DEVICE_ID == GATEWAY_NODE_ID
extern const char* WIFI_AP_SSID;        // WiFi network name
extern const char* WIFI_AP_PASSWORD;    // WiFi password (min 8 characters)
// WiFi Mode: AP (Access Point) or STA (Station - join existing network)
extern const bool WIFI_USE_STATION_MODE;  // true = join existing WiFi, false = create AP

// Enterprise WiFi (WPA2-Enterprise for eduroam, corporate networks)
extern const bool WIFI_USE_ENTERPRISE;                  // true = WPA2-Enterprise
extern const char* WIFI_ENTERPRISE_SSID;                // Enterprise network SSID (e.g., "eduroam")
extern const char* WIFI_ENTERPRISE_ANONYMOUS_IDENTITY;  // Anonymous outer identity (e.g., "anonymous@csulb.edu")
extern const char* WIFI_ENTERPRISE_IDENTITY;            // Your identity (usually email)
extern const char* WIFI_ENTERPRISE_USERNAME;            // Your username (usually same as identity)
extern const char* WIFI_ENTERPRISE_PASSWORD;            // Your password

// Standard WiFi (WPA2-Personal for home/simple networks)
extern const char* WIFI_STA_SSID;         // Your home/school WiFi name
extern const char* WIFI_STA_PASSWORD;     // Your home/school WiFi password
// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DEVICE CONFIGURATION                              ║
// ║  Change DEVICE_ID and DEVICE_NAME in config.cpp for each node            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

extern const uint8_t DEVICE_ID;
extern const char* const DEVICE_NAME;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIMEZONE CONFIGURATION                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

extern const int8_t UTC_OFFSET_HOURS;  // e.g., -8 for PST, -5 for EST

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         SENSOR CONFIGURATION                              ║
// ║  SHT30 and BMP180 on separate I2C bus from OLED                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Sensor enable flags (set to false to disable individual sensors)
extern const bool SENSOR_SHT30_ENABLED;
extern const bool SENSOR_BMP180_ENABLED;

// Sensor I2C bus pins (separate from OLED which uses GPIO17/18)
#define SENSOR_I2C_SDA      7
#define SENSOR_I2C_SCL      20

// Sensor reading interval (milliseconds)
extern const unsigned long SENSOR_READ_INTERVAL_MS;

// Sea level pressure for altitude calculation (Pa) - adjust for local conditions
extern const float SEA_LEVEL_PRESSURE_PA;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NETWORK CONFIGURATION                             ║
// ║  These must be #define because they're used as array sizes               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#define MESH_MAX_NODES              5
#define MESH_MAX_HOPS               8
#define RECENT_PACKET_CACHE_SIZE    32

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIMING CONFIGURATION                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Core timing
extern const unsigned long RX_CHECK_INTERVAL_MS;
extern const unsigned long DISPLAY_TIME_MS;
extern const unsigned long DISPLAY_UPDATE_INTERVAL_MS;

// Node health timing
extern const unsigned long NODE_TIMEOUT_MS;

// Status print intervals
extern const unsigned long GPS_STATUS_INTERVAL_MS;
extern const unsigned long NODE_CHECK_INTERVAL_MS;
extern const unsigned long STATS_PRINT_INTERVAL_MS;

// Duplicate detection
extern const unsigned long DUPLICATE_TIMEOUT_MS;

// Neighbor table maintenance
extern const unsigned long NEIGHBOR_PRUNE_INTERVAL_MS;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GRADIENT ROUTING CONFIGURATION                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

extern const bool USE_GRADIENT_ROUTING;           // Enable/disable gradient routing (false = pure flooding)
extern const unsigned long BEACON_INTERVAL_MS;    // How often gateway sends beacons (ms)
extern const unsigned long ROUTE_TIMEOUT_MS;      // Route expiration time (ms)
extern const unsigned long BEACON_REBROADCAST_MIN_MS;  // Min delay before beacon rebroadcast
extern const unsigned long BEACON_REBROADCAST_MAX_MS;  // Max delay before beacon rebroadcast

// ThingSpeak Configuration
extern const char* THINGSPEAK_API_KEYS[];
extern const bool THINGSPEAK_ENABLED;
extern const unsigned long THINGSPEAK_CHANNEL_IDS[];
extern const char* THINGSPEAK_READ_KEYS[];

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIME HELPER FUNCTIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void getLocalTime(int utcHour, int utcMin, int utcSec, int &localHour, int &localMin, int &localSec);
String formatTime12Hr(int hour, int minute, int second);



#endif // CONFIG_H