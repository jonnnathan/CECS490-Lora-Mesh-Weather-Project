#ifndef TDMA_SCHEDULER_H
#define TDMA_SCHEDULER_H

#include <Arduino.h>
#include "network_time.h"

// ==================== TDMA CONFIGURATION ====================
// Five-node schedule with 12-second slots inside a 60-second minute.
// Each node owns a 12-second window (10s TX + 2s guard).

// Slot timing (seconds within each minute):
//   Node 1: 0-11   (TX at 6)
//   Node 2: 12-23  (TX at 18)
//   Node 3: 24-35  (TX at 30)
//   Node 4: 36-47  (TX at 42)
//   Node 5: 48-59  (TX at 54)

struct TDMAConfig {
    uint8_t deviceId;                    // Device identifier (1-5)
    uint8_t transmissionsPerSlot;        // Number of transmissions per slot (default: 1)
    uint8_t transmissionOffset;          // Offset from slot start in seconds (e.g., 6)
};

struct TDMAStatus {
    bool isMyTimeSlot;                   // Current second falls within this device's slot
    bool shouldTransmit;                 // Device should transmit now
    uint8_t currentTransmissionIndex;    // Which transmission in the slot (always 0 now)
    uint8_t nextTransmissionSecond;      // Next scheduled transmission second (absolute)
    uint8_t slotStartSecond;             // When this node's slot starts
    uint8_t slotEndSecond;               // When this node's slot ends
    bool gpsTimeSynced;                  // GPS time is valid and synced (legacy compatibility)
    bool timeSynced;                     // Time is synced (GPS or network) - NEW
    TimeSource timeSource;               // Current time source (GPS, NETWORK, NONE) - NEW
    unsigned long lastTransmitTime;      // Last successful transmission timestamp (millis)
};

struct GPSTimestamp {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    bool valid;
};

// TDMA Scheduler Class
class TDMAScheduler {
public:
    TDMAScheduler();

    // TDMA timing constants - 5 nodes, 12 seconds each
    static constexpr uint8_t MAX_NODES = 5;
    static constexpr uint8_t SLOT_DURATION_SEC = 12;    // 60s / 5 nodes = 12s per node
    static constexpr uint8_t TX_WINDOW_SEC = 10;        // Active transmit window (with 2s guard)
    static constexpr uint8_t TX_PER_SLOT = 1;           // One transmission per window
    static constexpr uint8_t DEFAULT_TX_OFFSET = 6;     // TX at middle of slot (6 seconds in)

    // Initialize with device ID (1-5)
    void init(uint8_t deviceId);

    // Set custom transmission offset within slot (default: 6)
    void setTransmissionOffset(uint8_t offset);

    // Update scheduler with current GPS time (legacy method)
    void update(int gpsHour, int gpsMinute, int gpsSecond, bool gpsValid);

    // Update scheduler with automatic time source selection (GPS or Network fallback)
    // Returns the time source that was used
    TimeSource updateWithFallback(int gpsHour, int gpsMinute, int gpsSecond, bool gpsValid);

    // Check if device should transmit now
    bool shouldTransmitNow();

    // Check if current second is within this device's slot
    bool isMyTimeSlot();

    // Get current TDMA status
    TDMAStatus getStatus();

    // Get formatted GPS timestamp for message payload
    String getGPSTimestampString();

    // Get timestamp structure
    GPSTimestamp getGPSTimestamp();

    // Reset transmission tracking for new slot
    void resetSlot();

    // Mark transmission as complete
    void markTransmissionComplete();

    // Get time until next transmission (in seconds)
    int getTimeUntilNextTransmission();

    // Get device mode string (TX_MODE, RX_MODE, WAIT_GPS)
    String getDeviceMode();

    // Get configured transmissions per slot
    uint8_t getTransmissionsPerSlot();

    // Get slot boundaries for this device
    uint8_t getSlotStart();
    uint8_t getSlotEnd();

private:
    TDMAConfig config;
    TDMAStatus status;
    GPSTimestamp currentTime;

    uint8_t lastProcessedSecond;
    uint8_t transmissionsCompletedThisSlot;
    bool slotActiveThisMinute;

    // Helper functions
    uint8_t calculateSlotStart(uint8_t deviceId);
    uint8_t calculateSlotEnd(uint8_t deviceId);
    bool isWithinMySlot(uint8_t second);
    bool isTransmissionSecond(uint8_t second);
    uint8_t getAbsoluteTransmissionSecond();
};

#endif // TDMA_SCHEDULER_H