/**
 * @file secrets_example.h
 * @brief Template for sensitive credentials - copy to secrets.h and fill in.
 *
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                         SETUP INSTRUCTIONS                                ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  1. Copy this file to secrets.h in the same directory                    ║
 * ║  2. Replace all placeholder values with your actual credentials          ║
 * ║  3. Never commit secrets.h to version control                            ║
 * ║                                                                           ║
 * ║  The secrets.h file is listed in .gitignore to prevent accidental        ║
 * ║  exposure of credentials.                                                 ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * @note This file serves as documentation and template only.
 * @see secrets.h (your local copy with real credentials)
 */

#ifndef SECRETS_H
#define SECRETS_H

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         WIFI ACCESS POINT CREDENTIALS                     ║
// ║  Used when gateway creates its own WiFi network (AP mode)                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief Password for the mesh WiFi access point.
 *
 * Must be at least 8 characters for WPA2.
 * Change this from the default for production deployments.
 */
#define SECRET_WIFI_AP_PASSWORD "your_ap_password_here"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         WIFI STATION MODE CREDENTIALS                     ║
// ║  Used when gateway joins an existing WiFi network                        ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief WiFi network name to connect to in station mode.
 */
#define SECRET_WIFI_STA_SSID "your_wifi_ssid"

/**
 * @brief WiFi password for station mode.
 */
#define SECRET_WIFI_STA_PASSWORD "your_wifi_password"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         ENTERPRISE WIFI CREDENTIALS                       ║
// ║  For eduroam and corporate WPA2-Enterprise networks                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief Enterprise WiFi network name (e.g., "eduroam").
 */
#define SECRET_WIFI_ENTERPRISE_SSID "eduroam"

/**
 * @brief Anonymous outer identity for EAP.
 */
#define SECRET_WIFI_ENTERPRISE_ANONYMOUS_ID "anonymous@university.edu"

/**
 * @brief EAP identity (usually email address).
 */
#define SECRET_WIFI_ENTERPRISE_IDENTITY "user@university.edu"

/**
 * @brief EAP username (often same as identity).
 */
#define SECRET_WIFI_ENTERPRISE_USERNAME "user@university.edu"

/**
 * @brief EAP password.
 */
#define SECRET_WIFI_ENTERPRISE_PASSWORD "your_password"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         THINGSPEAK API KEYS                               ║
// ║  Write API keys for each node's ThingSpeak channel                       ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief ThingSpeak Write API Keys (indexed by node ID - 1).
 *
 * Get your API keys from: https://thingspeak.com/channels/YOUR_CHANNEL/api_keys
 */
#define SECRET_THINGSPEAK_API_KEY_NODE1 ""                    // Gateway - usually not needed
#define SECRET_THINGSPEAK_API_KEY_NODE2 "YOUR_WRITE_KEY_2"
#define SECRET_THINGSPEAK_API_KEY_NODE3 "YOUR_WRITE_KEY_3"
#define SECRET_THINGSPEAK_API_KEY_NODE4 "YOUR_WRITE_KEY_4"
#define SECRET_THINGSPEAK_API_KEY_NODE5 "YOUR_WRITE_KEY_5"

/**
 * @brief ThingSpeak Read API Keys (for dashboard data retrieval).
 */
#define SECRET_THINGSPEAK_READ_KEY_NODE1 ""
#define SECRET_THINGSPEAK_READ_KEY_NODE2 "YOUR_READ_KEY_2"
#define SECRET_THINGSPEAK_READ_KEY_NODE3 "YOUR_READ_KEY_3"
#define SECRET_THINGSPEAK_READ_KEY_NODE4 "YOUR_READ_KEY_4"
#define SECRET_THINGSPEAK_READ_KEY_NODE5 "YOUR_READ_KEY_5"

/**
 * @brief ThingSpeak Channel IDs.
 *
 * Create channels at: https://thingspeak.com/channels/new
 */
#define SECRET_THINGSPEAK_CHANNEL_NODE1 0        // Gateway - not used
#define SECRET_THINGSPEAK_CHANNEL_NODE2 1234567  // Replace with your channel ID
#define SECRET_THINGSPEAK_CHANNEL_NODE3 1234568
#define SECRET_THINGSPEAK_CHANNEL_NODE4 1234569
#define SECRET_THINGSPEAK_CHANNEL_NODE5 1234570

#endif // SECRETS_H
