/**
 * @file Logger.h
 * @brief Centralized logging system with log levels, timestamps, and multiple output sinks.
 *
 * This module provides a professional logging infrastructure for the ESP32 LoRa Mesh
 * system, replacing scattered Serial.print() calls with a structured, configurable
 * logging approach.
 *
 * @section Features
 * - Four log levels: ERROR, WARN, INFO, DEBUG
 * - Automatic timestamp prepending (millis or RTC-based)
 * - Compile-time log level filtering to save flash/RAM
 * - Multiple output sinks (Serial, SD Card, UDP - extensible)
 * - Flash-string support (F() macro) for memory efficiency
 * - Thread-safe logging with mutex protection
 *
 * @section Usage
 * @code
 * LOG_INFO("System initialized");
 * LOG_WARN_F("Low memory: %d bytes free", freeHeap);
 * LOG_ERROR_F("LoRa init failed with code %d", errorCode);
 * LOG_DEBUG_F("Packet received: size=%d rssi=%d", size, rssi);
 * @endcode
 *
 * @section LogFormat
 * Output format: [MM:SS.mmm][LEVEL] Message
 * Example: [12:45.123][INFO] System initialized
 *
 * @author ESP32 LoRa Mesh Team
 * @version 1.0.0
 * @date 2025
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <Print.h>

// ============================================================================
// LOG LEVEL CONFIGURATION
// ============================================================================

/**
 * @brief Enumeration of available log levels.
 *
 * Log levels are hierarchical - setting a level enables all levels at or
 * above that severity. For example, LOG_LEVEL_WARN enables WARN and ERROR.
 *
 * @note Level values are ordered from most severe (0) to least severe (4).
 */
enum LogLevel : uint8_t {
    LOG_LEVEL_NONE  = 0,  ///< Logging completely disabled
    LOG_LEVEL_ERROR = 1,  ///< Critical errors requiring immediate attention
    LOG_LEVEL_WARN  = 2,  ///< Warning conditions that may indicate problems
    LOG_LEVEL_INFO  = 3,  ///< Informational messages about normal operation
    LOG_LEVEL_DEBUG = 4   ///< Detailed debug information for troubleshooting
};

/**
 * @brief Compile-time log level threshold.
 *
 * Messages with severity below this level are compiled out entirely,
 * saving flash space and execution time in production builds.
 *
 * @note Change this value to control verbosity:
 * - LOG_LEVEL_ERROR: Production (minimal output)
 * - LOG_LEVEL_WARN:  Testing (warnings + errors)
 * - LOG_LEVEL_INFO:  Normal development
 * - LOG_LEVEL_DEBUG: Full debugging
 */
#ifndef LOG_LEVEL_ACTIVE
#define LOG_LEVEL_ACTIVE LOG_LEVEL_INFO
#endif

/**
 * @brief Master enable switch for all logging.
 *
 * Set to false to completely disable logging (saves ~2KB flash).
 */
#ifndef LOG_ENABLED
#define LOG_ENABLED true
#endif

/**
 * @brief Enable/disable timestamp prepending in log output.
 *
 * When enabled, logs include [MM:SS.mmm] prefix.
 * Disable to save ~500 bytes RAM and reduce output length.
 */
#ifndef LOG_TIMESTAMPS_ENABLED
#define LOG_TIMESTAMPS_ENABLED true
#endif

/**
 * @brief Maximum length of a single log message.
 *
 * Messages exceeding this length are truncated.
 * Increase if you need longer messages (costs RAM).
 */
#ifndef LOG_MAX_MESSAGE_LENGTH
#define LOG_MAX_MESSAGE_LENGTH 256
#endif

// ============================================================================
// LOG SINK INTERFACE
// ============================================================================

/**
 * @brief Abstract interface for log output destinations.
 *
 * Implement this interface to create custom log sinks such as
 * SD card logging, UDP network logging, or MQTT publishing.
 *
 * @note Sinks are called synchronously - keep implementations fast.
 */
class ILogSink {
public:
    virtual ~ILogSink() = default;

    /**
     * @brief Write a log message to this sink.
     *
     * @param level The severity level of the message
     * @param timestamp Milliseconds since boot (for ordering)
     * @param message The formatted log message (null-terminated)
     */
    virtual void write(LogLevel level, uint32_t timestamp, const char* message) = 0;

    /**
     * @brief Flush any buffered output.
     *
     * Called periodically and before system reset.
     */
    virtual void flush() = 0;

    /**
     * @brief Check if this sink is ready to receive messages.
     *
     * @return true if sink is initialized and operational
     */
    virtual bool isReady() const = 0;
};

/**
 * @brief Serial port log sink implementation.
 *
 * Default sink that outputs to the hardware Serial port.
 * Formats output with timestamps and level indicators.
 */
class SerialLogSink : public ILogSink {
public:
    /**
     * @brief Construct a Serial log sink.
     *
     * @param serial Reference to the Serial port to use (e.g., Serial, Serial1)
     */
    explicit SerialLogSink(Print& serial = Serial);

    void write(LogLevel level, uint32_t timestamp, const char* message) override;
    void flush() override;
    bool isReady() const override;

private:
    Print& m_serial;

    /**
     * @brief Get the level prefix string.
     *
     * @param level Log level to convert
     * @return Short string like "ERR", "WRN", "INF", "DBG"
     */
    const char* getLevelString(LogLevel level) const;
};

// ============================================================================
// LOGGER CLASS
// ============================================================================

/**
 * @brief Central logging manager (Singleton pattern).
 *
 * The Logger class manages log level filtering, message formatting,
 * and distribution to registered output sinks.
 *
 * @section Thread-Safety
 * All logging methods are protected by a FreeRTOS mutex, making them
 * safe to call from multiple tasks or ISR contexts.
 *
 * @section Initialization
 * Call Logger::getInstance().begin() in setup() before any logging.
 *
 * @code
 * void setup() {
 *     Serial.begin(115200);
 *     Logger::getInstance().begin();
 *     LOG_INFO("System starting...");
 * }
 * @endcode
 */
class Logger {
public:
    /**
     * @brief Get the singleton Logger instance.
     *
     * @return Reference to the global Logger instance
     */
    static Logger& getInstance();

    /**
     * @brief Initialize the logging system.
     *
     * Sets up default Serial sink and prepares internal buffers.
     * Must be called after Serial.begin().
     *
     * @param defaultLevel Initial runtime log level (can be changed later)
     */
    void begin(LogLevel defaultLevel = LOG_LEVEL_INFO);

    /**
     * @brief Set the runtime log level filter.
     *
     * Messages below this level are suppressed at runtime.
     * Note: Compile-time filtering (LOG_LEVEL_ACTIVE) takes precedence.
     *
     * @param level New minimum log level to display
     */
    void setLevel(LogLevel level);

    /**
     * @brief Get the current runtime log level.
     *
     * @return Current minimum log level
     */
    LogLevel getLevel() const;

    /**
     * @brief Add a custom log sink.
     *
     * Up to 4 sinks can be registered simultaneously.
     * The Serial sink is added automatically by begin().
     *
     * @param sink Pointer to the sink (must remain valid for Logger lifetime)
     * @return true if sink was added, false if sink array is full
     */
    bool addSink(ILogSink* sink);

    /**
     * @brief Remove a previously added sink.
     *
     * @param sink Pointer to the sink to remove
     * @return true if sink was found and removed
     */
    bool removeSink(ILogSink* sink);

    /**
     * @brief Log a message at the specified level.
     *
     * @param level Severity level of the message
     * @param format Printf-style format string
     * @param ... Format arguments
     */
    void log(LogLevel level, const char* format, ...);

    /**
     * @brief Log a message from flash memory (F() macro).
     *
     * @param level Severity level of the message
     * @param format Printf-style format string in PROGMEM
     * @param ... Format arguments
     */
    void logF(LogLevel level, const __FlashStringHelper* format, ...);

    /**
     * @brief Flush all sinks.
     *
     * Ensures all buffered log data is written to outputs.
     */
    void flush();

private:
    Logger();  // Private constructor for singleton
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static constexpr size_t MAX_SINKS = 4;

    LogLevel m_runtimeLevel;                    ///< Current runtime filter level
    ILogSink* m_sinks[MAX_SINKS];              ///< Registered output sinks
    uint8_t m_sinkCount;                        ///< Number of active sinks
    SerialLogSink m_defaultSerialSink;         ///< Built-in Serial sink
    char m_buffer[LOG_MAX_MESSAGE_LENGTH];     ///< Formatting buffer
    bool m_initialized;                         ///< Begin() called flag
    SemaphoreHandle_t m_mutex;                 ///< Thread safety mutex

    /**
     * @brief Internal logging implementation.
     *
     * @param level Message severity
     * @param message Formatted message to distribute
     */
    void logInternal(LogLevel level, const char* message);

    /**
     * @brief Check if a message at this level should be logged.
     *
     * @param level Level to check
     * @return true if message should be output
     */
    bool shouldLog(LogLevel level) const;
};

// ============================================================================
// LOGGING MACROS
// ============================================================================

/**
 * @defgroup LogMacros Logging Macros
 * @brief Convenient macros for logging at different levels.
 *
 * These macros provide:
 * - Compile-time filtering (messages below LOG_LEVEL_ACTIVE are removed)
 * - Automatic level specification
 * - Support for both RAM and flash (F()) format strings
 *
 * @{
 */

#if LOG_ENABLED

// Compile-time level check macro
#define LOG_LEVEL_CHECK(level) ((level) <= LOG_LEVEL_ACTIVE)

// Base logging macros
#define LOG(level, fmt, ...) \
    do { \
        if (LOG_LEVEL_CHECK(level)) { \
            Logger::getInstance().log(level, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#define LOG_F(level, fmt, ...) \
    do { \
        if (LOG_LEVEL_CHECK(level)) { \
            Logger::getInstance().logF(level, F(fmt), ##__VA_ARGS__); \
        } \
    } while(0)

// ERROR level - always included unless logging disabled
#if LOG_LEVEL_ACTIVE >= LOG_LEVEL_ERROR
    /**
     * @brief Log an error message (RAM string).
     * @param fmt Printf-style format string
     */
    #define LOG_ERROR(fmt, ...) LOG(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

    /**
     * @brief Log an error message (flash string - saves RAM).
     * @param fmt Printf-style format string (stored in flash)
     */
    #define LOG_ERROR_F(fmt, ...) LOG_F(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#else
    #define LOG_ERROR(fmt, ...)
    #define LOG_ERROR_F(fmt, ...)
#endif

// WARN level
#if LOG_LEVEL_ACTIVE >= LOG_LEVEL_WARN
    /**
     * @brief Log a warning message (RAM string).
     * @param fmt Printf-style format string
     */
    #define LOG_WARN(fmt, ...) LOG(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)

    /**
     * @brief Log a warning message (flash string - saves RAM).
     * @param fmt Printf-style format string (stored in flash)
     */
    #define LOG_WARN_F(fmt, ...) LOG_F(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#else
    #define LOG_WARN(fmt, ...)
    #define LOG_WARN_F(fmt, ...)
#endif

// INFO level
#if LOG_LEVEL_ACTIVE >= LOG_LEVEL_INFO
    /**
     * @brief Log an informational message (RAM string).
     * @param fmt Printf-style format string
     */
    #define LOG_INFO(fmt, ...) LOG(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)

    /**
     * @brief Log an informational message (flash string - saves RAM).
     * @param fmt Printf-style format string (stored in flash)
     */
    #define LOG_INFO_F(fmt, ...) LOG_F(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#else
    #define LOG_INFO(fmt, ...)
    #define LOG_INFO_F(fmt, ...)
#endif

// DEBUG level
#if LOG_LEVEL_ACTIVE >= LOG_LEVEL_DEBUG
    /**
     * @brief Log a debug message (RAM string).
     * @param fmt Printf-style format string
     */
    #define LOG_DEBUG(fmt, ...) LOG(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

    /**
     * @brief Log a debug message (flash string - saves RAM).
     * @param fmt Printf-style format string (stored in flash)
     */
    #define LOG_DEBUG_F(fmt, ...) LOG_F(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...)
    #define LOG_DEBUG_F(fmt, ...)
#endif

/** @} */ // end of LogMacros group

#else // LOG_ENABLED == false

// All logging disabled - empty macros
#define LOG(level, fmt, ...)
#define LOG_F(level, fmt, ...)
#define LOG_ERROR(fmt, ...)
#define LOG_ERROR_F(fmt, ...)
#define LOG_WARN(fmt, ...)
#define LOG_WARN_F(fmt, ...)
#define LOG_INFO(fmt, ...)
#define LOG_INFO_F(fmt, ...)
#define LOG_DEBUG(fmt, ...)
#define LOG_DEBUG_F(fmt, ...)

#endif // LOG_ENABLED

// ============================================================================
// CATEGORY-SPECIFIC LOGGING (Integration with mesh_debug.h)
// ============================================================================

/**
 * @defgroup CategoryLogs Category-Specific Log Macros
 * @brief Macros for logging with specific subsystem categories.
 *
 * These integrate with the existing mesh_debug.h categories while
 * using the new Logger infrastructure.
 *
 * @{
 */

/** @brief Log a sensor-related message */
#define LOG_SENSOR(fmt, ...)  LOG_INFO_F("[SENSOR] " fmt, ##__VA_ARGS__)

/** @brief Log a LoRa radio-related message */
#define LOG_LORA(fmt, ...)    LOG_INFO_F("[LORA] " fmt, ##__VA_ARGS__)

/** @brief Log a GPS-related message */
#define LOG_GPS(fmt, ...)     LOG_INFO_F("[GPS] " fmt, ##__VA_ARGS__)

/** @brief Log a WiFi-related message */
#define LOG_WIFI(fmt, ...)    LOG_INFO_F("[WIFI] " fmt, ##__VA_ARGS__)

/** @brief Log a display-related message */
#define LOG_DISPLAY(fmt, ...) LOG_DEBUG_F("[DISPLAY] " fmt, ##__VA_ARGS__)

/** @brief Log a TDMA timing-related message */
#define LOG_TDMA(fmt, ...)    LOG_DEBUG_F("[TDMA] " fmt, ##__VA_ARGS__)

/** @brief Log a memory-related message */
#define LOG_MEMORY(fmt, ...)  LOG_DEBUG_F("[MEM] " fmt, ##__VA_ARGS__)

/** @brief Log a configuration-related message */
#define LOG_CONFIG(fmt, ...)  LOG_INFO_F("[CONFIG] " fmt, ##__VA_ARGS__)

/** @} */ // end of CategoryLogs group

#endif // LOGGER_H
