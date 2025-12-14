#include "config.h"
#include "secrets.h" // Sensitive credentials (git-ignored)

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GATEWAY CONFIGURATION                             ║
// ║  Set GATEWAY_NODE_ID to the DEVICE_ID of the node that hosts dashboard   ║
// ║  Example: If GATEWAY_NODE_ID = 1, then Device 1 becomes the gateway      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

const uint8_t GATEWAY_NODE_ID = 1; // Which node ID is the gateway?

// ⚠️  CRITICAL: IS_GATEWAY depends on DEVICE_ID initialization
// These MUST remain in the same compilation unit (config.cpp) to ensure
// proper initialization order. Do not move IS_GATEWAY to a different file.
// Static initialization order fiasco prevention: both constants are in same TU.
const bool IS_GATEWAY = (DEVICE_ID == GATEWAY_NODE_ID);
const char *WIFI_AP_SSID = "LoRa_Mesh";                 // Network name
const char *WIFI_AP_PASSWORD = SECRET_WIFI_AP_PASSWORD; // From secrets.h

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         WIFI MODE SELECTION                               ║
// ║  Set WIFI_USE_STATION_MODE to true to join existing WiFi                 ║
// ║  Set to false to create Access Point (LoRa_Mesh)                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

const bool WIFI_USE_STATION_MODE =
    false; // false = Access Point (works offline), true = join WiFi (needs
           // internet for map)

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                    ENTERPRISE WIFI (eduroam) SUPPORT                      ║
// ║  Set WIFI_USE_ENTERPRISE to true for WPA2-Enterprise networks           ║
// ║  Examples: eduroam, corporate networks with username/password           ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

const bool WIFI_USE_ENTERPRISE =
    false; // false = Use standard WiFi (phone hotspot)

// Enterprise WiFi credentials (for eduroam, etc.) - from secrets.h
const char *WIFI_ENTERPRISE_SSID = SECRET_WIFI_ENTERPRISE_SSID;
const char *WIFI_ENTERPRISE_ANONYMOUS_IDENTITY =
    SECRET_WIFI_ENTERPRISE_ANONYMOUS_ID;
const char *WIFI_ENTERPRISE_IDENTITY = SECRET_WIFI_ENTERPRISE_IDENTITY;
const char *WIFI_ENTERPRISE_USERNAME = SECRET_WIFI_ENTERPRISE_USERNAME;
const char *WIFI_ENTERPRISE_PASSWORD = SECRET_WIFI_ENTERPRISE_PASSWORD;

// Standard WiFi credentials (for home/simple networks) - from secrets.h
const char *WIFI_STA_SSID = SECRET_WIFI_STA_SSID;
const char *WIFI_STA_PASSWORD = SECRET_WIFI_STA_PASSWORD;
// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DEVICE CONFIGURATION                              ║
// ║  Change DEVICE_ID and DEVICE_NAME for each node                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

const uint8_t DEVICE_ID = 3;
const char *const DEVICE_NAME = "DEV3";

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIMEZONE CONFIGURATION                            ║
// ║  Change this for your timezone                                           ║
// ║    PST = -8, MST = -7, CST = -6, EST = -5                                ║
// ║    For daylight saving: PDT = -7, MDT = -6, CDT = -5, EDT = -4           ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

const int8_t UTC_OFFSET_HOURS = -8; // Pacific Standard Time

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIMING CONFIGURATION                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Core timing
const unsigned long RX_CHECK_INTERVAL_MS = 50;
const unsigned long DISPLAY_TIME_MS = 3000;
const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 250;

// Node health timing
const unsigned long NODE_TIMEOUT_MS = 90000;

// Status print intervals
const unsigned long GPS_STATUS_INTERVAL_MS = 5000;
const unsigned long NODE_CHECK_INTERVAL_MS = 10000;
const unsigned long STATS_PRINT_INTERVAL_MS = 60000;

// Duplicate detection
const unsigned long DUPLICATE_TIMEOUT_MS = 60000;

// Neighbor table maintenance
const unsigned long NEIGHBOR_PRUNE_INTERVAL_MS =
    60000; // Prune expired neighbors every 60 seconds

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GRADIENT ROUTING CONFIGURATION                    ║
// ║  Gradient routing reduces bandwidth by ~64% compared to flooding          ║
// ║  Set USE_GRADIENT_ROUTING = false to fall back to pure flooding          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

const bool USE_GRADIENT_ROUTING =
    true; // Enable gradient routing (false = pure flooding)
const unsigned long BEACON_INTERVAL_MS =
    30000; // Gateway beacon interval (30 seconds)
const unsigned long ROUTE_TIMEOUT_MS =
    60000; // Route expires after 60 seconds without beacon
const unsigned long BEACON_REBROADCAST_MIN_MS =
    100; // Min random delay before beacon rebroadcast
const unsigned long BEACON_REBROADCAST_MAX_MS =
    500; // Max random delay before beacon rebroadcast

// ThingSpeak Configuration - API keys from secrets.h
const char *THINGSPEAK_API_KEYS[] = {
    SECRET_THINGSPEAK_API_KEY_NODE1, // Gateway
    SECRET_THINGSPEAK_API_KEY_NODE2, // Node 2
    SECRET_THINGSPEAK_API_KEY_NODE3, // Node 3
    SECRET_THINGSPEAK_API_KEY_NODE4, // Node 4
    SECRET_THINGSPEAK_API_KEY_NODE5  // Node 5
};
// ThingSpeak Channel IDs (index 0 = Node 1, index 1 = Node 2, etc.)
const unsigned long THINGSPEAK_CHANNEL_IDS[] = {
    SECRET_THINGSPEAK_CHANNEL_NODE1, // Node 1 (gateway) - not used
    SECRET_THINGSPEAK_CHANNEL_NODE2, // Node 2
    SECRET_THINGSPEAK_CHANNEL_NODE3, // Node 3
    SECRET_THINGSPEAK_CHANNEL_NODE4, // Node 4
    SECRET_THINGSPEAK_CHANNEL_NODE5  // Node 5
};
const char *THINGSPEAK_READ_KEYS[] = {
    SECRET_THINGSPEAK_READ_KEY_NODE1, // Node 1 (gateway) - not used
    SECRET_THINGSPEAK_READ_KEY_NODE2, // Node 2
    SECRET_THINGSPEAK_READ_KEY_NODE3, // Node 3
    SECRET_THINGSPEAK_READ_KEY_NODE4, // Node 4
    SECRET_THINGSPEAK_READ_KEY_NODE5  // Node 5
};
const bool THINGSPEAK_ENABLED = true;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         SENSOR CONFIGURATION                              ║
// ║  Enable/disable SHT30 and BMP180 sensors                                  ║
// ║  Sensors use I2C bus on GPIO7 (SDA) and GPIO20 (SCL)                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

const bool SENSOR_SHT30_ENABLED = true;  // Temperature/humidity sensor
const bool SENSOR_BMP180_ENABLED = true; // Pressure/altitude sensor
const unsigned long SENSOR_READ_INTERVAL_MS =
    5000; // Read sensors every 5 seconds
const float SEA_LEVEL_PRESSURE_PA =
    102000.0; // Adjusted for Long Beach ~62ft elevation + current weather
              // (~1020 hPa)

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIME HELPER FUNCTIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void getLocalTime(int utcHour, int utcMin, int utcSec, int &localHour,
                  int &localMin, int &localSec) {
  localMin = utcMin;
  localSec = utcSec;

  localHour = utcHour + UTC_OFFSET_HOURS;

  // Handle day wraparound
  if (localHour < 0) {
    localHour += 24;
  } else if (localHour >= 24) {
    localHour -= 24;
  }
}

String formatTime12Hr(int hour, int minute, int second) {
  const char *ampm = "AM";
  int hour12 = hour;

  if (hour == 0) {
    hour12 = 12;
    ampm = "AM";
  } else if (hour == 12) {
    hour12 = 12;
    ampm = "PM";
  } else if (hour > 12) {
    hour12 = hour - 12;
    ampm = "PM";
  } else {
    ampm = "AM";
  }

  char buffer[15];
  snprintf(buffer, sizeof(buffer), "%2d:%02d:%02d %s", hour12, minute, second,
           ampm);
  return String(buffer);
}