#ifndef NETWORK_TIME_H
#define NETWORK_TIME_H

#include <Arduino.h>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NETWORK TIME SYNCHRONIZATION                       ║
// ║                                                                           ║
// ║  Provides fallback time synchronization for nodes without GPS lock.       ║
// ║  Time is extracted from gateway beacon messages and used for TDMA         ║
// ║  scheduling when local GPS is unavailable.                                ║
// ║                                                                           ║
// ║  Priority:                                                                ║
// ║    1. Own GPS time (most accurate)                                        ║
// ║    2. Network time from beacon (fallback)                                 ║
// ║    3. No transmission (safety mode)                                       ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIME SOURCE ENUM                                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * TimeSource - Indicates where the current time reference comes from
 */
enum TimeSource : uint8_t {
    TIME_SOURCE_NONE = 0,       // No valid time available - cannot transmit
    TIME_SOURCE_GPS = 1,        // Using own GPS (best accuracy, ~1μs)
    TIME_SOURCE_NETWORK = 2,    // Using gateway beacon time (fallback, ~200-500ms)
    TIME_SOURCE_MANUAL = 3      // Manually set via web interface (for testing)
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NETWORK TIME STATE                                ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * NetworkTimeState - Stores the latest time received from beacon
 *
 * Multi-hop time relay: Time can come from gateway directly or via
 * intermediate nodes. Lower hop count = more accurate time.
 */
struct NetworkTimeState {
    uint8_t  hour;                  // Received hour (0-23)
    uint8_t  minute;                // Received minute (0-59)
    uint8_t  second;                // Received second (0-59)
    unsigned long receivedAtMillis; // Local millis() when beacon was received
    unsigned long lastUpdateTime;   // millis() of last update
    bool     valid;                 // Is network time currently valid?
    uint8_t  sourceNodeId;          // Which node provided the time
    uint8_t  hopCount;              // Hops from GPS source (0=GPS, 1=gateway, 2+=relay)
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         FUNCTION DECLARATIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Initialize the network time module
 * Call once during setup()
 */
void initNetworkTime();

/**
 * Update network time from a received beacon
 *
 * Multi-hop support: Only updates if the new time source has equal or lower
 * hop count than current source, ensuring we prefer more accurate time.
 *
 * @param hour      Hour from beacon (0-23)
 * @param minute    Minute from beacon (0-59)
 * @param second    Second from beacon (0-59)
 * @param sourceNode Node ID that sent the beacon
 * @param hopCount  Number of hops from GPS source (1=from gateway, 2+=relayed)
 */
void updateNetworkTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t sourceNode, uint8_t hopCount);

/**
 * Get current hop count of network time
 *
 * @return Hop count (1=gateway, 2+=relay), or 255 if no time available
 */
uint8_t getNetworkTimeHopCount();

/**
 * Get the current network time (extrapolated from last beacon)
 *
 * @param hour      Output: current hour
 * @param minute    Output: current minute
 * @param second    Output: current second
 * @return true if network time is valid, false otherwise
 */
bool getNetworkTime(uint8_t &hour, uint8_t &minute, uint8_t &second);

/**
 * Check if network time is currently valid
 * Network time becomes invalid if no beacon received within NETWORK_TIME_MAX_AGE_MS
 *
 * @return true if network time can be used
 */
bool isNetworkTimeValid();

/**
 * Invalidate network time (e.g., on error or manual reset)
 */
void invalidateNetworkTime();

/**
 * Get age of network time in seconds
 *
 * @return Seconds since last beacon, or ULONG_MAX if never received
 */
unsigned long getNetworkTimeAge();

/**
 * Get the raw network time state (for debugging)
 *
 * @return Reference to internal NetworkTimeState
 */
const NetworkTimeState& getNetworkTimeState();

/**
 * Get a string representation of the time source
 *
 * @param source The TimeSource enum value
 * @return String like "GPS", "NET", or "NONE"
 */
const char* getTimeSourceString(TimeSource source);

/**
 * Print network time status to Serial (for debugging)
 */
void printNetworkTimeStatus();

/**
 * Set time manually (for testing without GPS)
 * This bypasses normal time source priority and forces the time.
 *
 * @param hour   Hour (0-23)
 * @param minute Minute (0-59)
 * @param second Second (0-59)
 */
void setManualTime(uint8_t hour, uint8_t minute, uint8_t second);

#endif // NETWORK_TIME_H
