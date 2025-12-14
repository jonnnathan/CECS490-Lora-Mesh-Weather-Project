/**
 * @file RuntimeConfig.h
 * @brief Runtime configuration management using ESP32 NVS (Non-Volatile Storage).
 *
 * This module provides persistent configuration storage that survives reboots,
 * allowing field configuration changes without re-flashing firmware.
 *
 * @section Overview
 * RuntimeConfig uses the ESP32 Preferences library (backed by NVS) to store
 * key configuration values. On startup, values are loaded from NVS with
 * fallback to hardcoded defaults if not found.
 *
 * @section Stored Configuration
 * The following values can be configured at runtime:
 * - Device ID (1-255)
 * - Device Name (up to 8 characters)
 * - WiFi SSID and Password
 * - Timezone UTC offset
 *
 * @section Usage
 * @code
 * // In setup()
 * RuntimeConfig& config = RuntimeConfig::getInstance();
 * config.begin();
 *
 * // Read values
 * uint8_t deviceId = config.getDeviceId();
 * String wifiSsid = config.getWifiSsid();
 *
 * // Change values (persisted to NVS)
 * config.setDeviceId(5);           // Takes effect immediately
 * config.setWifiSsid("MyNetwork"); // May require reboot for WiFi
 *
 * // Reset to defaults
 * config.resetToDefaults();
 * @endcode
 *
 * @section NVS_Keys
 * Configuration is stored in NVS namespace "mesh_cfg" with these keys:
 * | Key         | Type   | Description                    |
 * |-------------|--------|--------------------------------|
 * | device_id   | uint8  | Node ID (1-255)               |
 * | device_name | string | Human-readable name            |
 * | wifi_ssid   | string | WiFi network name              |
 * | wifi_pass   | string | WiFi password                  |
 * | utc_offset  | int8   | Hours offset from UTC          |
 *
 * @author ESP32 LoRa Mesh Team
 * @version 1.0.0
 * @date 2025
 */

#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * @brief NVS namespace for mesh configuration storage.
 *
 * All configuration values are stored under this namespace in NVS.
 */
#define CONFIG_NVS_NAMESPACE "mesh_cfg"

/**
 * @defgroup ConfigKeys NVS Configuration Keys
 * @brief String keys used for NVS storage.
 * @{
 */
#define CONFIG_KEY_DEVICE_ID    "device_id"    ///< Node ID (uint8_t)
#define CONFIG_KEY_DEVICE_NAME  "device_name"  ///< Node name (String)
#define CONFIG_KEY_WIFI_SSID    "wifi_ssid"    ///< WiFi SSID (String)
#define CONFIG_KEY_WIFI_PASS    "wifi_pass"    ///< WiFi password (String)
#define CONFIG_KEY_UTC_OFFSET   "utc_offset"   ///< Timezone offset (int8_t)
/** @} */

/**
 * @brief Maximum length for string configuration values.
 */
#define CONFIG_MAX_STRING_LEN 32

/**
 * @brief Manages runtime-configurable parameters with NVS persistence.
 *
 * RuntimeConfig implements the Singleton pattern, ensuring a single point
 * of access to configuration throughout the application. Configuration
 * changes are immediately persisted to NVS.
 *
 * @note Getter methods are cheap (return cached values).
 * @note Setter methods write to NVS (relatively slow, ~10-50ms).
 */
class RuntimeConfig {
public:
    /**
     * @brief Get the singleton RuntimeConfig instance.
     *
     * @return Reference to the global RuntimeConfig instance
     */
    static RuntimeConfig& getInstance();

    /**
     * @brief Initialize the configuration system.
     *
     * Loads all configuration values from NVS. If a value is not found
     * in NVS, the hardcoded default from config.cpp is used.
     *
     * @return true if NVS was opened successfully
     * @return false if NVS initialization failed
     *
     * @note Must be called once during setup() before accessing config values.
     */
    bool begin();

    /**
     * @brief Check if the configuration system is initialized.
     *
     * @return true if begin() has been called successfully
     */
    bool isInitialized() const;

    // =========================================================================
    // Device Configuration Getters
    // =========================================================================

    /**
     * @brief Get the device ID.
     *
     * The device ID is a unique 8-bit identifier for this node in the mesh.
     * Value 1 is conventionally reserved for the gateway node.
     *
     * @return Device ID (1-255)
     * @note Default: Value from config.cpp DEVICE_ID
     */
    uint8_t getDeviceId() const;

    /**
     * @brief Get the device name.
     *
     * Human-readable name for this node, displayed on dashboard and logs.
     *
     * @return Device name string (e.g., "DEV3")
     * @note Default: Value from config.cpp DEVICE_NAME
     */
    String getDeviceName() const;

    /**
     * @brief Check if this node is the gateway.
     *
     * Computed from device ID - returns true if device ID matches GATEWAY_NODE_ID.
     *
     * @return true if this is the gateway node
     */
    bool isGateway() const;

    // =========================================================================
    // WiFi Configuration Getters
    // =========================================================================

    /**
     * @brief Get the WiFi SSID for station mode.
     *
     * @return WiFi network name to connect to
     * @note Default: Value from config.cpp WIFI_STA_SSID
     */
    String getWifiSsid() const;

    /**
     * @brief Get the WiFi password for station mode.
     *
     * @return WiFi password
     * @note Default: Value from config.cpp WIFI_STA_PASSWORD
     */
    String getWifiPassword() const;

    // =========================================================================
    // Timezone Configuration
    // =========================================================================

    /**
     * @brief Get the UTC timezone offset in hours.
     *
     * Used to convert GPS UTC time to local time for display and TDMA scheduling.
     *
     * @return Hours offset from UTC (e.g., -8 for PST, -5 for EST)
     * @note Default: Value from config.cpp UTC_OFFSET_HOURS
     */
    int8_t getUtcOffset() const;

    // =========================================================================
    // Configuration Setters (Persist to NVS)
    // =========================================================================

    /**
     * @brief Set the device ID.
     *
     * @param id New device ID (1-255)
     * @return true if saved successfully
     *
     * @warning Changing device ID affects mesh routing. Reboot recommended.
     */
    bool setDeviceId(uint8_t id);

    /**
     * @brief Set the device name.
     *
     * @param name New device name (max 8 characters)
     * @return true if saved successfully
     */
    bool setDeviceName(const String& name);

    /**
     * @brief Set the WiFi SSID.
     *
     * @param ssid New WiFi network name
     * @return true if saved successfully
     *
     * @note Changes take effect after reboot.
     */
    bool setWifiSsid(const String& ssid);

    /**
     * @brief Set the WiFi password.
     *
     * @param password New WiFi password
     * @return true if saved successfully
     *
     * @note Changes take effect after reboot.
     */
    bool setWifiPassword(const String& password);

    /**
     * @brief Set the UTC timezone offset.
     *
     * @param offset Hours offset from UTC (-12 to +14)
     * @return true if saved successfully
     */
    bool setUtcOffset(int8_t offset);

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Reset all configuration to hardcoded defaults.
     *
     * Clears all stored NVS values. On next begin(), defaults will be loaded.
     *
     * @return true if NVS was cleared successfully
     *
     * @note Requires reboot to take full effect.
     */
    bool resetToDefaults();

    /**
     * @brief Trigger a system reboot.
     *
     * Use after configuration changes that require restart.
     *
     * @param delayMs Milliseconds to wait before reboot (default 1000)
     */
    void reboot(uint32_t delayMs = 1000);

    /**
     * @brief Print current configuration to Serial.
     *
     * Useful for debugging and verification.
     */
    void printConfig() const;

private:
    RuntimeConfig();  // Private constructor for singleton
    RuntimeConfig(const RuntimeConfig&) = delete;
    RuntimeConfig& operator=(const RuntimeConfig&) = delete;

    Preferences m_prefs;       ///< ESP32 Preferences (NVS) handle
    bool m_initialized;        ///< Whether begin() has been called

    // Cached configuration values (loaded from NVS or defaults)
    uint8_t m_deviceId;        ///< Cached device ID
    String m_deviceName;       ///< Cached device name
    String m_wifiSsid;         ///< Cached WiFi SSID
    String m_wifiPassword;     ///< Cached WiFi password
    int8_t m_utcOffset;        ///< Cached UTC offset

    /**
     * @brief Load a string value from NVS with default fallback.
     *
     * @param key NVS key to read
     * @param defaultValue Value to use if key not found
     * @return Loaded value or default
     */
    String loadString(const char* key, const char* defaultValue);

    /**
     * @brief Save a string value to NVS.
     *
     * @param key NVS key to write
     * @param value Value to store
     * @return true if write successful
     */
    bool saveString(const char* key, const String& value);
};

#endif // RUNTIME_CONFIG_H
