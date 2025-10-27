#ifndef TDMA_SCHEDULER_H
#define TDMA_SCHEDULER_H

#include <Arduino.h>

// TDMA Configuration
// Each device gets assigned specific minutes within an hour to transmit
// For example: Device 1 transmits on even minutes (0, 2, 4, ..., 58)
//              Device 2 transmits on odd minutes (1, 3, 5, ..., 59)

struct TDMAConfig {
    uint8_t deviceId;                    // Device identifier (1, 2, 3, etc.)
    uint8_t transmissionsPerSlot;        // Number of transmissions per time slot (default: 4)
    uint8_t transmissionSeconds[4];      // Seconds within the minute to transmit (e.g., 0, 15, 30, 45)
    bool useAlternatingMinutes;          // If true, uses even/odd minute allocation
    uint8_t customMinutes[60];           // Custom minute allocation (1 = transmit, 0 = receive)
    uint8_t customMinuteCount;           // Number of custom minutes allocated
};

struct TDMAStatus {
    bool isMyTimeSlot;                   // Current minute belongs to this device
    bool shouldTransmit;                 // Device should transmit now
    uint8_t currentTransmissionIndex;    // Which transmission in the slot (0-3)
    uint8_t nextTransmissionSecond;      // Next scheduled transmission second
    bool gpsTimeSynced;                  // GPS time is valid and synced
    unsigned long lastTransmitTime;      // Last successful transmission timestamp
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

    // Initialize with device configuration
    void init(uint8_t deviceId, uint8_t transmissionsPerSlot = 4);

    // Set custom transmission seconds within a minute
    void setTransmissionSeconds(uint8_t sec1, uint8_t sec2, uint8_t sec3, uint8_t sec4);

    // Set custom minute allocation (for more than 2 devices)
    void setCustomMinutes(const uint8_t* minutes, uint8_t count);

    // Update scheduler with current GPS time
    void update(int gpsHour, int gpsMinute, int gpsSecond, bool gpsValid);

    // Check if device should transmit now
    bool shouldTransmitNow();

    // Check if current minute is this device's time slot
    bool isMyTimeSlot();

    // Get current TDMA status
    TDMAStatus getStatus();

    // Get formatted GPS timestamp for message payload
    String getGPSTimestampString();

    // Get timestamp structure
    GPSTimestamp getGPSTimestamp();

    // Reset transmission tracking for new time slot
    void resetSlot();

    // Mark transmission as complete
    void markTransmissionComplete();

    // Get time until next transmission (in seconds)
    int getTimeUntilNextTransmission();

    // Get device mode (TX or RX)
    String getDeviceMode();

private:
    TDMAConfig config;
    TDMAStatus status;
    GPSTimestamp currentTime;

    uint8_t lastProcessedMinute;
    uint8_t lastProcessedSecond;
    uint8_t transmissionsCompletedThisSlot;

    // Helper functions
    bool isTransmissionSecond(uint8_t second);
    uint8_t findNextTransmissionSecond(uint8_t currentSecond);
    bool checkMinuteAllocation(uint8_t minute);
};

#endif // TDMA_SCHEDULER_H
