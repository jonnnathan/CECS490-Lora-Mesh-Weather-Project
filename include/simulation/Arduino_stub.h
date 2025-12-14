/**
 * @file Arduino_stub.h
 * @brief Arduino compatibility layer for native (desktop) builds.
 *
 * Provides stubs for Arduino types, functions, and classes commonly used
 * in the mesh networking code. Enables the same code to compile and run
 * on desktop systems for simulation purposes.
 *
 * @note Only included in native builds (when ARDUINO is not defined).
 */

#ifndef ARDUINO_STUB_H
#define ARDUINO_STUB_H

#ifndef ARDUINO  // Only for native builds

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <string>
#include <chrono>

// Platform-specific threading support
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#else
    #include <thread>
    #include <mutex>
#endif

// ============================================================================
// Arduino Type Definitions
// ============================================================================

using byte = uint8_t;

// ============================================================================
// Arduino String Class (Simplified)
// ============================================================================

/**
 * @brief Simplified Arduino String compatible class.
 *
 * Wraps std::string to provide basic Arduino String compatibility.
 */
class String {
public:
    String() : m_str() {}
    String(const char* s) : m_str(s ? s : "") {}
    String(const std::string& s) : m_str(s) {}
    String(int val) : m_str(std::to_string(val)) {}
    String(unsigned int val) : m_str(std::to_string(val)) {}
    String(long val) : m_str(std::to_string(val)) {}
    String(unsigned long val) : m_str(std::to_string(val)) {}
    String(float val, int decimals = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        m_str = buf;
    }
    String(double val, int decimals = 2) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.*f", decimals, val);
        m_str = buf;
    }

    // Access
    const char* c_str() const { return m_str.c_str(); }
    size_t length() const { return m_str.length(); }
    bool isEmpty() const { return m_str.empty(); }

    // Operators
    String& operator=(const char* s) { m_str = s ? s : ""; return *this; }
    String& operator=(const String& s) { m_str = s.m_str; return *this; }
    String operator+(const String& s) const { return String(m_str + s.m_str); }
    String operator+(const char* s) const { return String(m_str + (s ? s : "")); }
    String& operator+=(const String& s) { m_str += s.m_str; return *this; }
    String& operator+=(const char* s) { if (s) m_str += s; return *this; }
    String& operator+=(char c) { m_str += c; return *this; }
    bool operator==(const String& s) const { return m_str == s.m_str; }
    bool operator==(const char* s) const { return m_str == (s ? s : ""); }
    bool operator!=(const String& s) const { return m_str != s.m_str; }
    char operator[](size_t i) const { return m_str[i]; }

    // Methods
    int indexOf(char c) const {
        size_t pos = m_str.find(c);
        return pos == std::string::npos ? -1 : (int)pos;
    }
    String substring(size_t start, size_t end = std::string::npos) const {
        return String(m_str.substr(start, end - start));
    }
    int toInt() const { return std::stoi(m_str); }
    float toFloat() const { return std::stof(m_str); }

private:
    std::string m_str;
};

// ============================================================================
// Time Functions
// ============================================================================

namespace {
    // Start time for millis() calculation
    auto g_startTime = std::chrono::steady_clock::now();
}

/**
 * @brief Get milliseconds since program start.
 * @return Milliseconds elapsed
 */
inline unsigned long millis() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime);
    return static_cast<unsigned long>(duration.count());
}

/**
 * @brief Get microseconds since program start.
 * @return Microseconds elapsed
 */
inline unsigned long micros() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - g_startTime);
    return static_cast<unsigned long>(duration.count());
}

/**
 * @brief Delay execution for specified milliseconds.
 * @param ms Milliseconds to delay
 */
inline void delay(unsigned long ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

/**
 * @brief Delay execution for specified microseconds.
 * @param us Microseconds to delay
 */
inline void delayMicroseconds(unsigned int us) {
#ifdef _WIN32
    // Windows Sleep() only has millisecond resolution
    // For microseconds, we use a busy wait or Sleep(0) for short delays
    if (us >= 1000) {
        Sleep(us / 1000);
    } else if (us > 0) {
        Sleep(0);  // Yield to other threads
    }
#else
    std::this_thread::sleep_for(std::chrono::microseconds(us));
#endif
}

// ============================================================================
// Serial Stub
// ============================================================================

/**
 * @brief Serial port stub that outputs to console.
 */
class SerialClass {
public:
    void begin(unsigned long baud) {
        (void)baud;  // Unused
        m_initialized = true;
    }

    void end() {
        m_initialized = false;
    }

    // Print methods
    size_t print(const char* s) {
        if (s) printf("%s", s);
        return s ? strlen(s) : 0;
    }
    size_t print(const String& s) { return print(s.c_str()); }
    size_t print(char c) { printf("%c", c); return 1; }
    size_t print(int n, int base = 10) {
        if (base == 16) printf("%x", n);
        else if (base == 8) printf("%o", n);
        else if (base == 2) {
            // Binary print
            if (n == 0) { printf("0"); return 1; }
            char buf[33];
            int i = 0;
            unsigned int un = (unsigned int)n;
            while (un > 0) {
                buf[i++] = '0' + (un & 1);
                un >>= 1;
            }
            buf[i] = '\0';
            // Reverse
            for (int j = 0; j < i/2; j++) {
                char t = buf[j];
                buf[j] = buf[i-1-j];
                buf[i-1-j] = t;
            }
            printf("%s", buf);
            return i;
        }
        else printf("%d", n);
        return 1;
    }
    size_t print(unsigned int n, int base = 10) { return print((int)n, base); }
    size_t print(long n, int base = 10) {
        if (base == 16) printf("%lx", n);
        else printf("%ld", n);
        return 1;
    }
    size_t print(unsigned long n, int base = 10) {
        if (base == 16) printf("%lx", n);
        else printf("%lu", n);
        return 1;
    }
    size_t print(float n, int decimals = 2) { printf("%.*f", decimals, n); return 1; }
    size_t print(double n, int decimals = 2) { printf("%.*f", decimals, n); return 1; }

    // Println methods
    size_t println() { printf("\n"); return 1; }
    size_t println(const char* s) { print(s); return println(); }
    size_t println(const String& s) { print(s); return println(); }
    size_t println(char c) { print(c); return println(); }
    size_t println(int n, int base = 10) { print(n, base); return println(); }
    size_t println(unsigned int n, int base = 10) { print(n, base); return println(); }
    size_t println(long n, int base = 10) { print(n, base); return println(); }
    size_t println(unsigned long n, int base = 10) { print(n, base); return println(); }
    size_t println(float n, int decimals = 2) { print(n, decimals); return println(); }
    size_t println(double n, int decimals = 2) { print(n, decimals); return println(); }

    // Printf (not standard Arduino but useful)
    size_t printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        int result = vprintf(format, args);
        va_end(args);
        return result > 0 ? result : 0;
    }

    // Input (stub)
    int available() { return 0; }
    int read() { return -1; }
    int peek() { return -1; }
    void flush() { fflush(stdout); }

    operator bool() const { return m_initialized; }

private:
    bool m_initialized = false;
};

// Global Serial instance
extern SerialClass Serial;

// ============================================================================
// Flash Memory Macros (No-op on native)
// ============================================================================

#define F(x) (x)
#define PROGMEM
#define pgm_read_byte(addr) (*(const unsigned char*)(addr))

// ============================================================================
// GPIO Stubs (No-op on native)
// ============================================================================

#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0

inline void pinMode(uint8_t pin, uint8_t mode) {
    (void)pin; (void)mode;
}

inline void digitalWrite(uint8_t pin, uint8_t val) {
    (void)pin; (void)val;
}

inline int digitalRead(uint8_t pin) {
    (void)pin;
    return LOW;
}

// ============================================================================
// FreeRTOS Stubs (for critical section compatibility)
// ============================================================================

#ifdef _WIN32
// Windows: Use CRITICAL_SECTION
typedef CRITICAL_SECTION portMUX_TYPE;

inline void portENTER_CRITICAL(portMUX_TYPE* mux) {
    EnterCriticalSection(mux);
}

inline void portEXIT_CRITICAL(portMUX_TYPE* mux) {
    LeaveCriticalSection(mux);
}

#define portMUX_INITIALIZER_UNLOCKED {}

inline void spinlock_initialize(portMUX_TYPE* mux) {
    InitializeCriticalSection(mux);
}

#else
// POSIX: Use std::mutex
typedef std::mutex portMUX_TYPE;

inline void portENTER_CRITICAL(portMUX_TYPE* mux) {
    mux->lock();
}

inline void portEXIT_CRITICAL(portMUX_TYPE* mux) {
    mux->unlock();
}

#define portMUX_INITIALIZER_UNLOCKED {}

inline void spinlock_initialize(portMUX_TYPE* mux) {
    (void)mux;  // std::mutex is auto-initialized
}
#endif

// ISR versions (same behavior in simulation)
#define portENTER_CRITICAL_ISR(mux) portENTER_CRITICAL(mux)
#define portEXIT_CRITICAL_ISR(mux) portEXIT_CRITICAL(mux)

// ============================================================================
// ESP32 Specific Stubs
// ============================================================================

inline uint32_t ESP_getFreeHeap() {
    return 200000;  // Fake 200KB free
}

inline uint32_t ESP_getHeapSize() {
    return 320000;  // Fake 320KB total
}

// ============================================================================
// Random Number Generation
// ============================================================================

inline long random(long max) {
    return rand() % max;
}

inline long random(long min, long max) {
    return min + rand() % (max - min);
}

inline void randomSeed(unsigned long seed) {
    srand((unsigned int)seed);
}

// ============================================================================
// Math Helpers
// ============================================================================

#ifndef constrain
#define constrain(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))
#endif

#ifndef map
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#endif

#endif // ARDUINO
#endif // ARDUINO_STUB_H
