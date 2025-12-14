/**
 * @file SensorManager.h
 * @brief Centralized sensor management for ESP32 LoRa Mesh nodes.
 *
 * This module encapsulates all sensor-related functionality including:
 * - SHT30 temperature and humidity sensor
 * - BMP180 pressure and altitude sensor
 * - Automatic barometric pressure calibration using GPS altitude
 *
 * The SensorManager follows a singleton-like pattern where a single instance
 * is created in main.cpp and accessed throughout the application.
 *
 * @section Usage
 * @code
 * SensorManager sensors;
 *
 * void setup() {
 *     sensors.begin();  // Initialize sensors
 * }
 *
 * void loop() {
 *     sensors.update();  // Periodic sensor reads
 *     float temp = sensors.getTemperatureF();
 *     float humidity = sensors.getHumidity();
 * }
 * @endcode
 *
 * @section Dependencies
 * - SHT30 temperature/humidity sensor library
 * - BMP180 pressure sensor library
 * - Wire (I2C) library
 * - GPS module for altitude calibration (optional)
 *
 * @author ESP32 LoRa Mesh Team
 * @version 1.0.0
 * @date 2025
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "sht30.h"
#include "bmp180.h"

/**
 * @brief Manages all environmental sensors on the mesh node.
 *
 * The SensorManager class provides a clean interface to read temperature,
 * humidity, pressure, and altitude from the attached sensors. It handles:
 * - I2C bus initialization on dedicated GPIO pins
 * - Sensor detection and initialization
 * - Periodic sensor readings with caching
 * - Automatic barometric pressure calibration using GPS altitude
 * - Graceful fallback when sensors are unavailable
 *
 * @note All temperature readings are returned in Fahrenheit.
 * @note Pressure readings are in hPa (hectopascals).
 * @note Altitude readings are in meters.
 */
class SensorManager {
public:
    /**
     * @brief Construct a new SensorManager instance.
     *
     * Does not initialize hardware - call begin() to initialize sensors.
     */
    SensorManager();

    /**
     * @brief Initialize the sensor subsystem.
     *
     * This method:
     * - Initializes the dedicated I2C bus for sensors
     * - Detects and initializes the SHT30 sensor (if enabled)
     * - Detects and initializes the BMP180 sensor (if enabled)
     * - Performs initial sensor readings
     *
     * @param sdaPin GPIO pin for I2C SDA (default from config.h)
     * @param sclPin GPIO pin for I2C SCL (default from config.h)
     * @return true if at least one sensor initialized successfully
     * @return false if all sensors failed to initialize
     *
     * @note Should be called once during setup() after Serial.begin()
     */
    bool begin(int sdaPin = -1, int sclPin = -1);

    /**
     * @brief Read all sensors and update cached values.
     *
     * Call this method periodically (e.g., every 5 seconds) to refresh
     * sensor readings. The method handles:
     * - Reading SHT30 temperature and humidity
     * - Reading BMP180 pressure
     * - Auto-calibrating sea level pressure from GPS altitude
     * - Computing barometric altitude
     *
     * @note If a sensor read fails, the cached value is retained.
     * @note GPS calibration requires valid GPS fix with altitude.
     */
    void update();

    /**
     * @brief Update barometric calibration using GPS altitude.
     *
     * This method calibrates the sea level pressure using a known
     * GPS altitude. The formula used is:
     * P0 = P / (1 - altitude/44330)^5.255
     *
     * @param gpsAltitudeMeters Known altitude from GPS in meters
     * @return true if calibration was accepted (valid range)
     * @return false if altitude was out of valid range (-500m to 10000m)
     *
     * @note Calibration uses exponential moving average for smoothing
     */
    bool calibrateWithGPS(float gpsAltitudeMeters);

    // =========================================================================
    // Getter Methods - Return cached sensor values
    // =========================================================================

    /**
     * @brief Get the current temperature in Fahrenheit.
     *
     * @return Temperature in degrees Fahrenheit
     * @note Returns default value (72.5°F) if no sensor available
     */
    float getTemperatureF() const;

    /**
     * @brief Get the current temperature in Celsius.
     *
     * @return Temperature in degrees Celsius
     */
    float getTemperatureC() const;

    /**
     * @brief Get the current relative humidity.
     *
     * @return Humidity percentage (0-100%)
     * @note Returns default value (45%) if no sensor available
     */
    float getHumidity() const;

    /**
     * @brief Get the current atmospheric pressure.
     *
     * @return Pressure in hPa (hectopascals)
     * @note Returns default value (1013.25 hPa) if no sensor available
     */
    float getPressureHPa() const;

    /**
     * @brief Get the current barometric altitude.
     *
     * Altitude is calculated from pressure using the calibrated
     * sea level pressure. More accurate after GPS calibration.
     *
     * @return Altitude in meters
     * @note Returns 0 if no pressure sensor available
     */
    float getAltitudeM() const;

    /**
     * @brief Get the calibrated sea level pressure.
     *
     * @return Sea level pressure in Pascals
     */
    float getSeaLevelPressurePa() const;

    // =========================================================================
    // Status Methods
    // =========================================================================

    /**
     * @brief Check if SHT30 sensor is available.
     *
     * @return true if SHT30 was detected and initialized
     */
    bool isSHT30Available() const;

    /**
     * @brief Check if BMP180 sensor is available.
     *
     * @return true if BMP180 was detected and initialized
     */
    bool isBMP180Available() const;

    /**
     * @brief Check if barometric pressure has been calibrated with GPS.
     *
     * @return true if GPS calibration has been performed
     */
    bool isCalibrated() const;

    /**
     * @brief Check if any sensor is available.
     *
     * @return true if at least one sensor is operational
     */
    bool hasAnySensor() const;

private:
    TwoWire m_sensorWire;           ///< Dedicated I2C bus for sensors

    SHT30 m_sht30;                   ///< SHT30 temperature/humidity sensor
    BMP180 m_bmp180;                 ///< BMP180 pressure/altitude sensor

    bool m_sht30Ok;                  ///< SHT30 initialization status
    bool m_bmp180Ok;                 ///< BMP180 initialization status
    bool m_initialized;              ///< Overall initialization status

    // Cached sensor readings
    float m_tempF;                   ///< Cached temperature in Fahrenheit
    float m_humidity;                ///< Cached humidity percentage
    float m_pressureHPa;             ///< Cached pressure in hPa
    float m_altitudeM;               ///< Cached barometric altitude in meters

    // Calibration state
    float m_calibratedSeaLevelPa;    ///< Calibrated sea level pressure (Pascals)
    bool m_seaLevelCalibrated;       ///< Whether GPS calibration has been done

    /**
     * @brief Read SHT30 sensor and update cached values.
     *
     * @return true if read successful
     */
    bool readSHT30();

    /**
     * @brief Read BMP180 sensor and update cached values.
     *
     * @return true if read successful
     */
    bool readBMP180();
};

#endif // SENSOR_MANAGER_H
