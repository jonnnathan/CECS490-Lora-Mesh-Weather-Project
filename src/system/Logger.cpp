/**
 * @file Logger.cpp
 * @brief Implementation of the centralized logging system.
 *
 * @see Logger.h for detailed documentation and usage examples.
 */

#include "Logger.h"
#include <stdarg.h>
#include <stdio.h>

// ============================================================================
// SerialLogSink Implementation
// ============================================================================

SerialLogSink::SerialLogSink(Print& serial)
    : m_serial(serial)
{
}

void SerialLogSink::write(LogLevel level, uint32_t timestamp, const char* message) {
    if (!isReady()) {
        return;
    }

#if LOG_TIMESTAMPS_ENABLED
    // Format: [MM:SS.mmm]
    uint32_t totalSeconds = timestamp / 1000;
    uint32_t millisPart = timestamp % 1000;
    uint32_t minutes = (totalSeconds / 60) % 60;
    uint32_t seconds = totalSeconds % 60;

    char timeBuffer[16];
    snprintf(timeBuffer, sizeof(timeBuffer), "[%02lu:%02lu.%03lu]",
             (unsigned long)minutes, (unsigned long)seconds, (unsigned long)millisPart);
    m_serial.print(timeBuffer);
#endif

    // Print level indicator
    m_serial.print('[');
    m_serial.print(getLevelString(level));
    m_serial.print("] ");

    // Print the actual message
    m_serial.println(message);
}

void SerialLogSink::flush() {
    // Serial has auto-flush, but we call it explicitly for completeness
    // Note: Serial.flush() waits for TX buffer to empty
    if (isReady()) {
        Serial.flush();
    }
}

bool SerialLogSink::isReady() const {
    // Check if Serial is initialized by attempting a simple operation
    // Serial is ready if it's been begin()'d
    return true;  // Assume ready - Serial.begin() called in setup()
}

const char* SerialLogSink::getLevelString(LogLevel level) const {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERR ";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DBG ";
        default:              return "??? ";
    }
}

// ============================================================================
// Logger Implementation
// ============================================================================

Logger& Logger::getInstance() {
    // Meyer's Singleton - thread-safe initialization in C++11
    static Logger instance;
    return instance;
}

Logger::Logger()
    : m_runtimeLevel(LOG_LEVEL_INFO)
    , m_sinkCount(0)
    , m_defaultSerialSink(Serial)
    , m_initialized(false)
    , m_mutex(nullptr)
{
    // Initialize sink array to nullptr
    for (size_t i = 0; i < MAX_SINKS; ++i) {
        m_sinks[i] = nullptr;
    }
    m_buffer[0] = '\0';
}

void Logger::begin(LogLevel defaultLevel) {
    if (m_initialized) {
        return;  // Already initialized
    }

    // Create mutex for thread safety
    m_mutex = xSemaphoreCreateMutex();

    // Set initial level
    m_runtimeLevel = defaultLevel;

    // Add default Serial sink
    addSink(&m_defaultSerialSink);

    m_initialized = true;

    // Log startup message
    LOG_INFO_F("Logger initialized (level=%d)", (int)defaultLevel);
}

void Logger::setLevel(LogLevel level) {
    if (m_mutex && xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        m_runtimeLevel = level;
        xSemaphoreGive(m_mutex);
    } else {
        m_runtimeLevel = level;
    }
}

LogLevel Logger::getLevel() const {
    return m_runtimeLevel;
}

bool Logger::addSink(ILogSink* sink) {
    if (sink == nullptr || m_sinkCount >= MAX_SINKS) {
        return false;
    }

    // Check for duplicate
    for (size_t i = 0; i < m_sinkCount; ++i) {
        if (m_sinks[i] == sink) {
            return false;  // Already added
        }
    }

    if (m_mutex && xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        m_sinks[m_sinkCount++] = sink;
        xSemaphoreGive(m_mutex);
    } else {
        m_sinks[m_sinkCount++] = sink;
    }

    return true;
}

bool Logger::removeSink(ILogSink* sink) {
    if (sink == nullptr) {
        return false;
    }

    bool found = false;

    if (m_mutex && xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < m_sinkCount; ++i) {
            if (m_sinks[i] == sink) {
                // Shift remaining sinks down
                for (size_t j = i; j < m_sinkCount - 1; ++j) {
                    m_sinks[j] = m_sinks[j + 1];
                }
                m_sinks[--m_sinkCount] = nullptr;
                found = true;
                break;
            }
        }
        xSemaphoreGive(m_mutex);
    }

    return found;
}

void Logger::log(LogLevel level, const char* format, ...) {
    if (!shouldLog(level)) {
        return;
    }

    va_list args;
    va_start(args, format);

    // Take mutex for buffer access
    if (m_mutex && xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        vsnprintf(m_buffer, sizeof(m_buffer), format, args);
        logInternal(level, m_buffer);
        xSemaphoreGive(m_mutex);
    } else {
        // Fallback without mutex (during early init)
        vsnprintf(m_buffer, sizeof(m_buffer), format, args);
        logInternal(level, m_buffer);
    }

    va_end(args);
}

void Logger::logF(LogLevel level, const __FlashStringHelper* format, ...) {
    if (!shouldLog(level)) {
        return;
    }

    va_list args;
    va_start(args, format);

    // Copy flash string to RAM buffer first
    // Using PGM_P for flash pointer
    const char* flashStr = reinterpret_cast<const char*>(format);

    // Take mutex for buffer access
    if (m_mutex && xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        // On ESP32, PROGMEM strings are accessible like regular RAM
        // but we use vsnprintf which works with both
        vsnprintf(m_buffer, sizeof(m_buffer), flashStr, args);
        logInternal(level, m_buffer);
        xSemaphoreGive(m_mutex);
    } else {
        vsnprintf(m_buffer, sizeof(m_buffer), flashStr, args);
        logInternal(level, m_buffer);
    }

    va_end(args);
}

void Logger::flush() {
    if (m_mutex && xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < m_sinkCount; ++i) {
            if (m_sinks[i] && m_sinks[i]->isReady()) {
                m_sinks[i]->flush();
            }
        }
        xSemaphoreGive(m_mutex);
    }
}

void Logger::logInternal(LogLevel level, const char* message) {
    uint32_t timestamp = millis();

    // Distribute to all sinks
    for (size_t i = 0; i < m_sinkCount; ++i) {
        if (m_sinks[i] && m_sinks[i]->isReady()) {
            m_sinks[i]->write(level, timestamp, message);
        }
    }
}

bool Logger::shouldLog(LogLevel level) const {
    // Check both compile-time and runtime levels
    if (level == LOG_LEVEL_NONE) {
        return false;
    }
    if (level > LOG_LEVEL_ACTIVE) {
        return false;  // Compile-time filter
    }
    if (level > m_runtimeLevel) {
        return false;  // Runtime filter
    }
    return true;
}
