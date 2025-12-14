/**
 * @file secrets.h
 * @brief Sensitive credentials and API keys for the ESP32 LoRa Mesh project.
 *
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                              WARNING                                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  This file contains SENSITIVE CREDENTIALS and should NOT be committed     ║
 * ║  to version control. It is listed in .gitignore.                          ║
 * ║                                                                           ║
 * ║  If you are setting up a new development environment:                     ║
 * ║  1. Copy secrets_example.h to secrets.h                                   ║
 * ║  2. Fill in your actual credentials                                       ║
 * ║  3. Never commit secrets.h to the repository                              ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 *
 * @note This file is automatically included by config.cpp
 * @see secrets_example.h for a template with placeholder values
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
#define SECRET_WIFI_AP_PASSWORD "mesh1234"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         WIFI STATION MODE CREDENTIALS                     ║
// ║  Used when gateway joins an existing WiFi network                        ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief WiFi network name to connect to in station mode.
 *
 * Leave empty to use RuntimeConfig NVS values.
 */
#define SECRET_WIFI_STA_SSID ""

/**
 * @brief WiFi password for station mode.
 *
 * Leave empty to use RuntimeConfig NVS values.
 */
#define SECRET_WIFI_STA_PASSWORD ""

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         ENTERPRISE WIFI CREDENTIALS                       ║
// ║  For eduroam and corporate WPA2-Enterprise networks                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief Enterprise WiFi network name (e.g., "eduroam").
 */
#define SECRET_WIFI_ENTERPRISE_SSID ""

/**
 * @brief Anonymous outer identity for EAP.
 */
#define SECRET_WIFI_ENTERPRISE_ANONYMOUS_ID ""

/**
 * @brief EAP identity (usually email address).
 */
#define SECRET_WIFI_ENTERPRISE_IDENTITY ""

/**
 * @brief EAP username (often same as identity).
 */
#define SECRET_WIFI_ENTERPRISE_USERNAME ""

/**
 * @brief EAP password.
 */
#define SECRET_WIFI_ENTERPRISE_PASSWORD ""

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         THINGSPEAK API KEYS                               ║
// ║  Write API keys for each node's ThingSpeak channel                       ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * @brief ThingSpeak Write API Keys (indexed by node ID - 1).
 *
 * Index 0 = Node 1 (gateway, typically not used)
 * Index 1 = Node 2
 * Index 2 = Node 3
 * etc.
 *
 * Get your API keys from: https://thingspeak.com/channels/YOUR_CHANNEL/api_keys
 */
#define SECRET_THINGSPEAK_API_KEY_NODE1 ""                // Gateway - usually not needed
#define SECRET_THINGSPEAK_API_KEY_NODE2 "8LBHS85TKG8AS2U0"
#define SECRET_THINGSPEAK_API_KEY_NODE3 "D35GBS8QVSC0BVQJ"
#define SECRET_THINGSPEAK_API_KEY_NODE4 "I1MFITOW8JNJJWD1"
#define SECRET_THINGSPEAK_API_KEY_NODE5 "KS12LH06QHZU8D8J"

/**
 * @brief ThingSpeak Read API Keys (for dashboard data retrieval).
 */
#define SECRET_THINGSPEAK_READ_KEY_NODE1 ""
#define SECRET_THINGSPEAK_READ_KEY_NODE2 "DZ7L3266JBJ0TITC"
#define SECRET_THINGSPEAK_READ_KEY_NODE3 "HZFT8OH0W6CI6BXJ"
#define SECRET_THINGSPEAK_READ_KEY_NODE4 "3LOL0G23XL9SYF6F"
#define SECRET_THINGSPEAK_READ_KEY_NODE5 "UEF28CAKQ0OUGYX8"

/**
 * @brief ThingSpeak Channel IDs.
 *
 * Create channels at: https://thingspeak.com/channels/new
 */
#define SECRET_THINGSPEAK_CHANNEL_NODE1 0        // Gateway - not used
#define SECRET_THINGSPEAK_CHANNEL_NODE2 3194362
#define SECRET_THINGSPEAK_CHANNEL_NODE3 3194371
#define SECRET_THINGSPEAK_CHANNEL_NODE4 3194372
#define SECRET_THINGSPEAK_CHANNEL_NODE5 3194374

#endif // SECRETS_H
