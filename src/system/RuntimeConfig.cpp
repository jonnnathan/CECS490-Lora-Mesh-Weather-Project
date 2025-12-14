/**
 * @file RuntimeConfig.cpp
 * @brief Implementation of RuntimeConfig NVS-backed configuration.
 *
 * @see RuntimeConfig.h for detailed documentation.
 */

#include "RuntimeConfig.h"
#include "config.h"  // For default values
#include "Logger.h"

// ============================================================================
// Singleton Instance
// ============================================================================

RuntimeConfig& RuntimeConfig::getInstance() {
    static RuntimeConfig instance;
    return instance;
}

// ============================================================================
// Constructor
// ============================================================================

RuntimeConfig::RuntimeConfig()
    : m_initialized(false)
    , m_deviceId(0)
    , m_deviceName("")
    , m_wifiSsid("")
    , m_wifiPassword("")
    , m_utcOffset(0)
{
}

// ============================================================================
// Initialization
// ============================================================================

bool RuntimeConfig::begin() {
    if (m_initialized) {
        LOG_WARN_F("RuntimeConfig already initialized");
        return true;
    }

    // Open NVS namespace (read-write mode)
    if (!m_prefs.begin(CONFIG_NVS_NAMESPACE, false)) {
        LOG_ERROR_F("Failed to open NVS namespace: %s", CONFIG_NVS_NAMESPACE);
        return false;
    }

    LOG_INFO_F("RuntimeConfig: Loading from NVS namespace '%s'", CONFIG_NVS_NAMESPACE);

    // Load device ID with default from config.cpp
    m_deviceId = m_prefs.getUChar(CONFIG_KEY_DEVICE_ID, DEVICE_ID);

    // Load device name with default from config.cpp
    m_deviceName = loadString(CONFIG_KEY_DEVICE_NAME, DEVICE_NAME);

    // Load WiFi credentials with defaults from config.cpp
    m_wifiSsid = loadString(CONFIG_KEY_WIFI_SSID, WIFI_STA_SSID);
    m_wifiPassword = loadString(CONFIG_KEY_WIFI_PASS, WIFI_STA_PASSWORD);

    // Load timezone offset with default from config.cpp
    m_utcOffset = m_prefs.getChar(CONFIG_KEY_UTC_OFFSET, UTC_OFFSET_HOURS);

    m_initialized = true;

    LOG_INFO_F("RuntimeConfig loaded: deviceId=%d, name=%s, utcOffset=%d",
               m_deviceId, m_deviceName.c_str(), m_utcOffset);

    return true;
}

bool RuntimeConfig::isInitialized() const {
    return m_initialized;
}

// ============================================================================
// Getters
// ============================================================================

uint8_t RuntimeConfig::getDeviceId() const {
    if (!m_initialized) {
        return DEVICE_ID;  // Return compile-time default
    }
    return m_deviceId;
}

String RuntimeConfig::getDeviceName() const {
    if (!m_initialized) {
        return String(DEVICE_NAME);
    }
    return m_deviceName;
}

bool RuntimeConfig::isGateway() const {
    return getDeviceId() == GATEWAY_NODE_ID;
}

String RuntimeConfig::getWifiSsid() const {
    if (!m_initialized) {
        return String(WIFI_STA_SSID);
    }
    return m_wifiSsid;
}

String RuntimeConfig::getWifiPassword() const {
    if (!m_initialized) {
        return String(WIFI_STA_PASSWORD);
    }
    return m_wifiPassword;
}

int8_t RuntimeConfig::getUtcOffset() const {
    if (!m_initialized) {
        return UTC_OFFSET_HOURS;
    }
    return m_utcOffset;
}

// ============================================================================
// Setters (Persist to NVS)
// ============================================================================

bool RuntimeConfig::setDeviceId(uint8_t id) {
    if (!m_initialized) {
        LOG_ERROR_F("RuntimeConfig not initialized");
        return false;
    }

    if (id == 0) {
        LOG_WARN_F("Device ID 0 is invalid");
        return false;
    }

    if (m_prefs.putUChar(CONFIG_KEY_DEVICE_ID, id) == 0) {
        LOG_ERROR_F("Failed to save device ID to NVS");
        return false;
    }

    m_deviceId = id;
    LOG_INFO_F("Device ID set to %d (reboot recommended)", id);
    return true;
}

bool RuntimeConfig::setDeviceName(const String& name) {
    if (!m_initialized) {
        LOG_ERROR_F("RuntimeConfig not initialized");
        return false;
    }

    if (name.length() == 0 || name.length() > CONFIG_MAX_STRING_LEN) {
        LOG_WARN_F("Device name invalid (length=%d)", name.length());
        return false;
    }

    if (!saveString(CONFIG_KEY_DEVICE_NAME, name)) {
        return false;
    }

    m_deviceName = name;
    LOG_INFO_F("Device name set to '%s'", name.c_str());
    return true;
}

bool RuntimeConfig::setWifiSsid(const String& ssid) {
    if (!m_initialized) {
        LOG_ERROR_F("RuntimeConfig not initialized");
        return false;
    }

    if (!saveString(CONFIG_KEY_WIFI_SSID, ssid)) {
        return false;
    }

    m_wifiSsid = ssid;
    LOG_INFO_F("WiFi SSID set (reboot required)");
    return true;
}

bool RuntimeConfig::setWifiPassword(const String& password) {
    if (!m_initialized) {
        LOG_ERROR_F("RuntimeConfig not initialized");
        return false;
    }

    if (!saveString(CONFIG_KEY_WIFI_PASS, password)) {
        return false;
    }

    m_wifiPassword = password;
    LOG_INFO_F("WiFi password set (reboot required)");
    return true;
}

bool RuntimeConfig::setUtcOffset(int8_t offset) {
    if (!m_initialized) {
        LOG_ERROR_F("RuntimeConfig not initialized");
        return false;
    }

    if (offset < -12 || offset > 14) {
        LOG_WARN_F("UTC offset out of range: %d", offset);
        return false;
    }

    if (m_prefs.putChar(CONFIG_KEY_UTC_OFFSET, offset) == 0) {
        LOG_ERROR_F("Failed to save UTC offset to NVS");
        return false;
    }

    m_utcOffset = offset;
    LOG_INFO_F("UTC offset set to %d hours", offset);
    return true;
}

// ============================================================================
// Utility Methods
// ============================================================================

bool RuntimeConfig::resetToDefaults() {
    if (!m_initialized) {
        LOG_ERROR_F("RuntimeConfig not initialized");
        return false;
    }

    LOG_WARN_F("Resetting configuration to defaults...");

    // Clear all keys in our namespace
    bool success = m_prefs.clear();

    if (success) {
        // Reload defaults
        m_deviceId = DEVICE_ID;
        m_deviceName = String(DEVICE_NAME);
        m_wifiSsid = String(WIFI_STA_SSID);
        m_wifiPassword = String(WIFI_STA_PASSWORD);
        m_utcOffset = UTC_OFFSET_HOURS;

        LOG_INFO_F("Configuration reset to defaults (reboot recommended)");
    } else {
        LOG_ERROR_F("Failed to clear NVS");
    }

    return success;
}

void RuntimeConfig::reboot(uint32_t delayMs) {
    LOG_WARN_F("Rebooting in %lu ms...", delayMs);
    delay(delayMs);
    ESP.restart();
}

void RuntimeConfig::printConfig() const {
    Serial.println(F("\n╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║                   RUNTIME CONFIGURATION                       ║"));
    Serial.println(F("╠═══════════════════════════════════════════════════════════════╣"));
    Serial.print(F("║  Device ID:     "));
    Serial.println(m_deviceId);
    Serial.print(F("║  Device Name:   "));
    Serial.println(m_deviceName);
    Serial.print(F("║  Is Gateway:    "));
    Serial.println(isGateway() ? "Yes" : "No");
    Serial.print(F("║  WiFi SSID:     "));
    Serial.println(m_wifiSsid.length() > 0 ? m_wifiSsid : "(not set)");
    Serial.print(F("║  WiFi Password: "));
    Serial.println(m_wifiPassword.length() > 0 ? "****" : "(not set)");
    Serial.print(F("║  UTC Offset:    "));
    Serial.print(m_utcOffset);
    Serial.println(F(" hours"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝\n"));
}

// ============================================================================
// Private Methods
// ============================================================================

String RuntimeConfig::loadString(const char* key, const char* defaultValue) {
    String value = m_prefs.getString(key, defaultValue);
    return value;
}

bool RuntimeConfig::saveString(const char* key, const String& value) {
    size_t written = m_prefs.putString(key, value);
    if (written == 0 && value.length() > 0) {
        LOG_ERROR_F("Failed to save string to NVS key: %s", key);
        return false;
    }
    return true;
}
