#include "tdma_scheduler.h"

TDMAScheduler::TDMAScheduler() {
    // Initialize with defaults
    config.deviceId = 1;
    config.transmissionsPerSlot = TX_PER_SLOT;
    config.transmissionOffset = DEFAULT_TX_OFFSET;

    // Initialize status
    status.isMyTimeSlot = false;
    status.shouldTransmit = false;
    status.currentTransmissionIndex = 0;
    status.nextTransmissionSecond = 0;
    status.slotStartSecond = 0;
    status.slotEndSecond = 0;
    status.gpsTimeSynced = false;
    status.timeSynced = false;           // NEW
    status.timeSource = TIME_SOURCE_NONE; // NEW
    status.lastTransmitTime = 0;

    // Initialize tracking
    lastProcessedSecond = 255;
    transmissionsCompletedThisSlot = 0;
    slotActiveThisMinute = false;

    // Initialize GPS timestamp
    currentTime.hour = 0;
    currentTime.minute = 0;
    currentTime.second = 0;
    currentTime.day = 0;
    currentTime.month = 0;
    currentTime.year = 0;
    currentTime.valid = false;
}

void TDMAScheduler::init(uint8_t deviceId) {
    // Validate device ID (1-5)
    if (deviceId < 1 || deviceId > MAX_NODES) {
        Serial.print("[TDMA] WARNING: Invalid device ID ");
        Serial.print(deviceId);
        Serial.print(", must be 1-");
        Serial.println(MAX_NODES);
        config.deviceId = 1;
    } else {
        config.deviceId = deviceId;
    }

    // Calculate slot boundaries
    status.slotStartSecond = calculateSlotStart(config.deviceId);
    status.slotEndSecond = calculateSlotEnd(config.deviceId);

    Serial.print("[TDMA] Initialized for Device ");
    Serial.println(config.deviceId);
    Serial.print("[TDMA] Slot window: seconds ");
    Serial.print(status.slotStartSecond);
    Serial.print(" - ");
    Serial.println(status.slotEndSecond);
    Serial.print("[TDMA] TX at second: ");
    Serial.println(getAbsoluteTransmissionSecond());
}

void TDMAScheduler::setTransmissionOffset(uint8_t offset) {
    // Validate offset is within the active TX window
    if (offset >= TX_WINDOW_SEC) {
        Serial.print("[TDMA] WARNING: Offset must be < ");
        Serial.println(TX_WINDOW_SEC);
        return;
    }

    config.transmissionOffset = offset;

    Serial.print("[TDMA] TX offset set to: ");
    Serial.println(offset);
}

uint8_t TDMAScheduler::calculateSlotStart(uint8_t deviceId) {
    // Slot start = (deviceId - 1) * 12 seconds
    // Node 1: 0, Node 2: 12, Node 3: 24, Node 4: 36, Node 5: 48
    return (uint8_t)((deviceId - 1) * SLOT_DURATION_SEC);
}

uint8_t TDMAScheduler::calculateSlotEnd(uint8_t deviceId) {
    // Slot end = slotStart + 11 (gives 12 seconds: 0-11, 12-23, 24-35, 36-47, 48-59)
    uint8_t slotStart = calculateSlotStart(deviceId);
    uint8_t slotEnd = slotStart + (SLOT_DURATION_SEC - 1);
    
    // Cap at 59 for last slot
    if (slotEnd > 59) {
        slotEnd = 59;
    }
    
    return slotEnd;
}

bool TDMAScheduler::isWithinMySlot(uint8_t second) {
    return (second >= status.slotStartSecond) && (second <= status.slotEndSecond);
}

uint8_t TDMAScheduler::getAbsoluteTransmissionSecond() {
    return status.slotStartSecond + config.transmissionOffset;
}

bool TDMAScheduler::isTransmissionSecond(uint8_t second) {
    return (getAbsoluteTransmissionSecond() == second);
}

void TDMAScheduler::update(int gpsHour, int gpsMinute, int gpsSecond, bool gpsValid) {
    // Update GPS timestamp
    extern int g_year, g_month, g_day;

    currentTime.hour = (uint8_t)gpsHour;
    currentTime.minute = (uint8_t)gpsMinute;
    currentTime.second = (uint8_t)gpsSecond;
    currentTime.day = (uint8_t)g_day;
    currentTime.month = (uint8_t)g_month;
    currentTime.year = (uint16_t)g_year;
    currentTime.valid = gpsValid;

    status.gpsTimeSynced = gpsValid;

    if (!gpsValid) {
        status.isMyTimeSlot = false;
        status.shouldTransmit = false;
        return;
    }

    uint8_t currentSec = (uint8_t)gpsSecond;

    // Check if we're in our slot
    bool wasInSlot = status.isMyTimeSlot;
    status.isMyTimeSlot = isWithinMySlot(currentSec);

    // Detect slot entry (reset counters)
    if (status.isMyTimeSlot && !wasInSlot) {
        transmissionsCompletedThisSlot = 0;
        lastProcessedSecond = 255;
        slotActiveThisMinute = true;
        
        // Convert to 12-hour format for display
        int hour12 = gpsHour;
        const char* ampm = "AM";
        
        if (gpsHour == 0) {
            hour12 = 12;
        } else if (gpsHour < 12) {
            hour12 = gpsHour;
        } else if (gpsHour == 12) {
            hour12 = 12;
            ampm = "PM";
        } else {
            hour12 = gpsHour - 12;
            ampm = "PM";
        }
        
        Serial.print("[TDMA] Entering TX slot at ");
        Serial.print(hour12);
        Serial.print(":");
        if (gpsMinute < 10) Serial.print("0");
        Serial.print(gpsMinute);
        Serial.print(":");
        if (gpsSecond < 10) Serial.print("0");
        Serial.print(gpsSecond);
        Serial.print(" ");
        Serial.println(ampm);
    }

    // Detect slot exit
    if (!status.isMyTimeSlot && wasInSlot) {
        slotActiveThisMinute = false;
        Serial.print("[TDMA] Exiting TX slot, completed ");
        Serial.print(transmissionsCompletedThisSlot);
        Serial.print("/");
        Serial.print(config.transmissionsPerSlot);
        Serial.println(" transmissions");
    }

    // Determine if we should transmit
    if (status.isMyTimeSlot) {
        bool isTxSecond = isTransmissionSecond(currentSec);
        bool notProcessedYet = (currentSec != lastProcessedSecond);
        bool slotsRemaining = (transmissionsCompletedThisSlot < config.transmissionsPerSlot);

        if (isTxSecond && notProcessedYet && slotsRemaining) {
            status.shouldTransmit = true;
            lastProcessedSecond = currentSec;
            status.currentTransmissionIndex = 0;
        } else {
            status.shouldTransmit = false;
        }

        // Set next TX second
        if (transmissionsCompletedThisSlot < config.transmissionsPerSlot) {
            status.nextTransmissionSecond = getAbsoluteTransmissionSecond();
        } else {
            // Done for this slot, next TX is in next minute
            status.nextTransmissionSecond = getAbsoluteTransmissionSecond();
        }
    } else {
        status.shouldTransmit = false;
        status.currentTransmissionIndex = 0;
        status.nextTransmissionSecond = getAbsoluteTransmissionSecond();
    }
}

TimeSource TDMAScheduler::updateWithFallback(int gpsHour, int gpsMinute, int gpsSecond, bool gpsValid) {
    // ─────────────────────────────────────────────────────────────────────────
    // Determine time source: GPS (priority 1) or Network (priority 2)
    // ─────────────────────────────────────────────────────────────────────────

    uint8_t hour, minute, second;
    TimeSource source = TIME_SOURCE_NONE;

    if (gpsValid) {
        // Priority 1: Use GPS time (most accurate)
        hour = (uint8_t)gpsHour;
        minute = (uint8_t)gpsMinute;
        second = (uint8_t)gpsSecond;
        source = TIME_SOURCE_GPS;
    } else if (isNetworkTimeValid()) {
        // Priority 2: Use network time from beacon (fallback)
        if (getNetworkTime(hour, minute, second)) {
            source = TIME_SOURCE_NETWORK;
        }
    }

    // Update status with time source info
    status.timeSource = source;
    status.timeSynced = (source != TIME_SOURCE_NONE);
    status.gpsTimeSynced = gpsValid;  // Legacy compatibility

    // If no valid time source, disable transmission
    if (source == TIME_SOURCE_NONE) {
        status.isMyTimeSlot = false;
        status.shouldTransmit = false;
        return source;
    }

    // Call the main update logic with the determined time
    update((int)hour, (int)minute, (int)second, true);

    // Override the legacy gpsTimeSynced to reflect actual source
    status.gpsTimeSynced = gpsValid;

    return source;
}

bool TDMAScheduler::shouldTransmitNow() {
    // Use timeSynced which covers both GPS and network time
    return status.shouldTransmit && status.timeSynced;
}

bool TDMAScheduler::isMyTimeSlot() {
    // Use timeSynced which covers both GPS and network time
    return status.isMyTimeSlot && status.timeSynced;
}

TDMAStatus TDMAScheduler::getStatus() {
    return status;
}

String TDMAScheduler::getGPSTimestampString() {
    if (!currentTime.valid) {
        return "NO_GPS";
    }

    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             currentTime.year, currentTime.month, currentTime.day,
             currentTime.hour, currentTime.minute, currentTime.second);
    return String(timestamp);
}

GPSTimestamp TDMAScheduler::getGPSTimestamp() {
    return currentTime;
}

void TDMAScheduler::resetSlot() {
    transmissionsCompletedThisSlot = 0;
    status.currentTransmissionIndex = 0;
    lastProcessedSecond = 255;
}

void TDMAScheduler::markTransmissionComplete() {
    transmissionsCompletedThisSlot++;
    status.shouldTransmit = false;
    status.lastTransmitTime = millis();

    Serial.print("[TDMA] TX complete (");
    Serial.print(transmissionsCompletedThisSlot);
    Serial.print("/");
    Serial.print(config.transmissionsPerSlot);
    Serial.println(")");
}

int TDMAScheduler::getTimeUntilNextTransmission() {
    if (!status.gpsTimeSynced) {
        return -1;
    }

    uint8_t currentSec = currentTime.second;
    uint8_t nextTx = status.nextTransmissionSecond;

    if (status.isMyTimeSlot && nextTx > currentSec) {
        return nextTx - currentSec;
    } else if (!status.isMyTimeSlot) {
        // Calculate seconds until our slot starts
        if (status.slotStartSecond > currentSec) {
            return status.slotStartSecond - currentSec;
        } else {
            // Slot is in next minute
            return (60 - currentSec) + status.slotStartSecond;
        }
    }

    return 0;
}

String TDMAScheduler::getDeviceMode() {
    // Check if we have any valid time source
    if (!status.timeSynced) {
        return "WAIT_TIME";  // Waiting for GPS or network time
    }

    // Build mode string with time source indicator
    String mode;

    if (status.isMyTimeSlot) {
        if (transmissionsCompletedThisSlot >= config.transmissionsPerSlot) {
            mode = "TX_DONE";
        } else {
            mode = "TX_MODE";
        }
    } else {
        mode = "RX_MODE";
    }

    return mode;
}

uint8_t TDMAScheduler::getTransmissionsPerSlot() {
    return config.transmissionsPerSlot;
}

uint8_t TDMAScheduler::getSlotStart() {
    return status.slotStartSecond;
}

uint8_t TDMAScheduler::getSlotEnd() {
    return status.slotEndSecond;
}