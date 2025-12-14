/**
 * @file SensorManager.cpp
 * @brief Implementation of the SensorManager class.
 *
 * @see SensorManager.h for detailed documentation.
 */

#include "SensorManager.h"
#include "config.h"
#include "Logger.h"
#include <math.h>

// ============================================================================
// Constructor
// ============================================================================

SensorManager::SensorManager()
    : m_sensorWire(1)  // Use I2C bus 1 (separate from OLED on bus 0)
    , m_sht30Ok(false)
    , m_bmp180Ok(false)
    , m_initialized(false)
    , m_tempF(72.5f)                    // Default temperature
    , m_humidity(45.0f)                  // Default humidity
    , m_pressureHPa(1013.25f)           // Standard sea level pressure
    , m_altitudeM(0.0f)
    , m_calibratedSeaLevelPa(SEA_LEVEL_PRESSURE_PA)
    , m_seaLevelCalibrated(false)
{
}

// ============================================================================
// Initialization
// ============================================================================

bool SensorManager::begin(int sdaPin, int sclPin) {
    if (m_initialized) {
        LOG_WARN_F("SensorManager already initialized");
        return true;
    }

    // Use config defaults if not specified
    if (sdaPin < 0) sdaPin = SENSOR_I2C_SDA;
    if (sclPin < 0) sclPin = SENSOR_I2C_SCL;

    LOG_INFO_F("SensorManager: Initializing I2C on SDA=%d, SCL=%d", sdaPin, sclPin);

    // Initialize dedicated I2C bus for sensors
    m_sensorWire.begin(sdaPin, sclPin);

    // Initialize SHT30 (temperature/humidity)
    if (SENSOR_SHT30_ENABLED) {
        m_sht30Ok = m_sht30.begin(&m_sensorWire);
        if (m_sht30Ok) {
            LOG_INFO_F("SHT30 sensor initialized @ 0x44");
            // Perform initial read
            readSHT30();
        } else {
            LOG_WARN_F("SHT30 sensor not found");
        }
    } else {
        LOG_INFO_F("SHT30 sensor disabled in config");
    }

    // Initialize BMP180 (pressure/altitude)
    if (SENSOR_BMP180_ENABLED) {
        m_bmp180Ok = m_bmp180.begin(&m_sensorWire);
        if (m_bmp180Ok) {
            LOG_INFO_F("BMP180 sensor initialized @ 0x77");
            // Perform initial read
            readBMP180();
        } else {
            LOG_WARN_F("BMP180 sensor not found");
        }
    } else {
        LOG_INFO_F("BMP180 sensor disabled in config");
    }

    m_initialized = true;

    if (!hasAnySensor()) {
        LOG_ERROR_F("No sensors available!");
        return false;
    }

    LOG_INFO_F("SensorManager: Ready (SHT30=%s, BMP180=%s)",
               m_sht30Ok ? "OK" : "N/A",
               m_bmp180Ok ? "OK" : "N/A");

    return true;
}

// ============================================================================
// Update (Periodic Read)
// ============================================================================

void SensorManager::update() {
    if (!m_initialized) {
        LOG_WARN_F("SensorManager::update called before begin()");
        return;
    }

    // Read SHT30
    if (SENSOR_SHT30_ENABLED && m_sht30Ok) {
        if (!readSHT30()) {
            LOG_WARN_F("SHT30 read failed");
        }
    }

    // Read BMP180
    if (SENSOR_BMP180_ENABLED && m_bmp180Ok) {
        if (!readBMP180()) {
            LOG_WARN_F("BMP180 read failed");
        }
    }
}

// ============================================================================
// GPS Calibration
// ============================================================================

bool SensorManager::calibrateWithGPS(float gpsAltitudeMeters) {
    // Only calibrate if GPS altitude is reasonable (-500m to 10000m)
    if (gpsAltitudeMeters < -500.0f || gpsAltitudeMeters > 10000.0f) {
        return false;
    }

    // Need a current pressure reading
    if (!m_bmp180Ok) {
        return false;
    }

    // Get current pressure in Pascals
    float pressure_pa = m_pressureHPa * 100.0f;
    if (pressure_pa <= 0) {
        return false;
    }

    // Calculate sea level pressure using barometric formula
    // P0 = P / (1 - altitude/44330)^5.255
    float ratio = 1.0f - (gpsAltitudeMeters / 44330.0f);
    if (ratio <= 0.0f) {
        return false;
    }

    float newSeaLevel = pressure_pa / powf(ratio, 5.255f);

    // Sanity check: sea level pressure should be 950-1050 hPa
    if (newSeaLevel < 95000.0f || newSeaLevel > 105000.0f) {
        return false;
    }

    // Apply calibration
    if (!m_seaLevelCalibrated) {
        // First calibration - use value directly
        m_calibratedSeaLevelPa = newSeaLevel;
        m_seaLevelCalibrated = true;
        LOG_INFO_F("Sea level pressure calibrated from GPS: %.1f hPa",
                   newSeaLevel / 100.0f);
    } else {
        // Subsequent calibrations - use exponential moving average for smoothing
        m_calibratedSeaLevelPa = m_calibratedSeaLevelPa * 0.95f + newSeaLevel * 0.05f;
    }

    // Recalculate altitude with new calibration
    m_altitudeM = m_bmp180.readAltitude(m_calibratedSeaLevelPa);

    return true;
}

// ============================================================================
// Getter Methods
// ============================================================================

float SensorManager::getTemperatureF() const {
    return m_tempF;
}

float SensorManager::getTemperatureC() const {
    return (m_tempF - 32.0f) / 1.8f;
}

float SensorManager::getHumidity() const {
    return m_humidity;
}

float SensorManager::getPressureHPa() const {
    return m_pressureHPa;
}

float SensorManager::getAltitudeM() const {
    return m_altitudeM;
}

float SensorManager::getSeaLevelPressurePa() const {
    return m_calibratedSeaLevelPa;
}

// ============================================================================
// Status Methods
// ============================================================================

bool SensorManager::isSHT30Available() const {
    return m_sht30Ok;
}

bool SensorManager::isBMP180Available() const {
    return m_bmp180Ok;
}

bool SensorManager::isCalibrated() const {
    return m_seaLevelCalibrated;
}

bool SensorManager::hasAnySensor() const {
    return m_sht30Ok || m_bmp180Ok;
}

// ============================================================================
// Private Methods
// ============================================================================

bool SensorManager::readSHT30() {
    if (!m_sht30.read()) {
        return false;
    }

    // Convert Celsius to Fahrenheit
    float tempC = m_sht30.getTemperature();
    m_tempF = tempC * 1.8f + 32.0f;
    m_humidity = m_sht30.getHumidity();

    return true;
}

bool SensorManager::readBMP180() {
    float pressure_pa = m_bmp180.readPressure();
    if (pressure_pa <= 0) {
        return false;
    }

    m_pressureHPa = pressure_pa / 100.0f;  // Convert Pa to hPa

    // Calculate altitude using calibrated sea level pressure
    m_altitudeM = m_bmp180.readAltitude(m_calibratedSeaLevelPa);

    // If SHT30 is not available, use BMP180 temperature
    if (!SENSOR_SHT30_ENABLED || !m_sht30Ok) {
        float tempC = m_bmp180.readTemperature();
        m_tempF = tempC * 1.8f + 32.0f;
    }

    return true;
}
