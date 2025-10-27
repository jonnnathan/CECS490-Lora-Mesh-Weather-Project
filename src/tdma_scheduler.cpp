#include "tdma_scheduler.h"

TDMAScheduler::TDMAScheduler() {
    // Initialize with defaults
    config.deviceId = 1;
    config.transmissionsPerSlot = 4;
    config.useAlternatingMinutes = true;
    config.customMinuteCount = 0;

    // Default transmission seconds: 0, 15, 30, 45 (evenly distributed)
    config.transmissionSeconds[0] = 0;
    config.transmissionSeconds[1] = 15;
    config.transmissionSeconds[2] = 30;
    config.transmissionSeconds[3] = 45;

    // Initialize status
    status.isMyTimeSlot = false;
    status.shouldTransmit = false;
    status.currentTransmissionIndex = 0;
    status.nextTransmissionSecond = 0;
    status.gpsTimeSynced = false;
    status.lastTransmitTime = 0;

    // Initialize time tracking
    lastProcessedMinute = 255;
    lastProcessedSecond = 255;
    transmissionsCompletedThisSlot = 0;

    // Initialize GPS timestamp
    currentTime.hour = 0;
    currentTime.minute = 0;
    currentTime.second = 0;
    currentTime.day = 0;
    currentTime.month = 0;
    currentTime.year = 0;
    currentTime.valid = false;
}

void TDMAScheduler::init(uint8_t deviceId, uint8_t transmissionsPerSlot) {
    config.deviceId = deviceId;
    config.transmissionsPerSlot = transmissionsPerSlot;

    Serial.print("TDMA Scheduler initialized for Device ");
    Serial.print(deviceId);
    Serial.print(" with ");
    Serial.print(transmissionsPerSlot);
    Serial.println(" transmissions per slot");
}

void TDMAScheduler::setTransmissionSeconds(uint8_t sec1, uint8_t sec2, uint8_t sec3, uint8_t sec4) {
    config.transmissionSeconds[0] = sec1;
    config.transmissionSeconds[1] = sec2;
    config.transmissionSeconds[2] = sec3;
    config.transmissionSeconds[3] = sec4;

    Serial.print("Transmission seconds set to: ");
    for (int i = 0; i < 4; i++) {
        Serial.print(config.transmissionSeconds[i]);
        if (i < 3) Serial.print(", ");
    }
    Serial.println();
}

void TDMAScheduler::setCustomMinutes(const uint8_t* minutes, uint8_t count) {
    config.useAlternatingMinutes = false;
    config.customMinuteCount = count;

    for (uint8_t i = 0; i < 60; i++) {
        config.customMinutes[i] = 0;
    }

    for (uint8_t i = 0; i < count && i < 60; i++) {
        if (minutes[i] < 60) {
            config.customMinutes[minutes[i]] = 1;
        }
    }

    Serial.print("Custom minutes set (count: ");
    Serial.print(count);
    Serial.print("): ");
    for (uint8_t i = 0; i < count && i < 10; i++) {
        Serial.print(minutes[i]);
        Serial.print(" ");
    }
    if (count > 10) Serial.print("...");
    Serial.println();
}

void TDMAScheduler::update(int gpsHour, int gpsMinute, int gpsSecond, bool gpsValid) {
    // Update GPS timestamp (need to include neo6m.h globals for date)
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

    // Check if we entered a new minute
    if (gpsMinute != lastProcessedMinute) {
        lastProcessedMinute = gpsMinute;
        transmissionsCompletedThisSlot = 0;

        // Check if this is our time slot
        status.isMyTimeSlot = checkMinuteAllocation(gpsMinute);

        if (status.isMyTimeSlot) {
            Serial.print("Entered TX time slot at ");
            Serial.print(gpsHour);
            Serial.print(":");
            if (gpsMinute < 10) Serial.print("0");
            Serial.println(gpsMinute);
        }
    }

    // Update transmission status
    if (status.isMyTimeSlot) {
        // Check if current second is a transmission second
        bool isTransmitSecond = isTransmissionSecond(gpsSecond);

        // Only transmit if:
        // 1. Current second is a transmission second
        // 2. We haven't transmitted at this second yet
        // 3. We haven't exceeded our transmissions per slot
        if (isTransmitSecond &&
            gpsSecond != lastProcessedSecond &&
            transmissionsCompletedThisSlot < config.transmissionsPerSlot) {

            status.shouldTransmit = true;
            lastProcessedSecond = gpsSecond;

            // Find which transmission index this is
            for (uint8_t i = 0; i < config.transmissionsPerSlot; i++) {
                if (config.transmissionSeconds[i] == gpsSecond) {
                    status.currentTransmissionIndex = i;
                    break;
                }
            }
        } else {
            status.shouldTransmit = false;
        }

        // Calculate next transmission second
        status.nextTransmissionSecond = findNextTransmissionSecond(gpsSecond);
    } else {
        // Not our time slot - receive mode
        status.shouldTransmit = false;
        status.currentTransmissionIndex = 0;
        status.nextTransmissionSecond = 0;
    }
}

bool TDMAScheduler::shouldTransmitNow() {
    return status.shouldTransmit && status.gpsTimeSynced;
}

bool TDMAScheduler::isMyTimeSlot() {
    return status.isMyTimeSlot && status.gpsTimeSynced;
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

    Serial.print("TX complete (");
    Serial.print(transmissionsCompletedThisSlot);
    Serial.print("/");
    Serial.print(config.transmissionsPerSlot);
    Serial.println(")");
}

int TDMAScheduler::getTimeUntilNextTransmission() {
    if (!status.gpsTimeSynced || !status.isMyTimeSlot) {
        return -1;  // Not applicable
    }

    uint8_t nextSec = status.nextTransmissionSecond;
    uint8_t currentSec = currentTime.second;

    if (nextSec > currentSec) {
        return nextSec - currentSec;
    } else if (nextSec == 0 && currentSec > config.transmissionSeconds[config.transmissionsPerSlot - 1]) {
        // Next transmission is in the next minute
        return 60 - currentSec;
    }

    return 0;
}

String TDMAScheduler::getDeviceMode() {
    if (!status.gpsTimeSynced) {
        return "WAIT_GPS";
    }

    if (status.isMyTimeSlot) {
        if (transmissionsCompletedThisSlot >= config.transmissionsPerSlot) {
            return "TX_DONE";
        }
        return "TX_MODE";
    }

    return "RX_MODE";
}

// Private helper functions

bool TDMAScheduler::isTransmissionSecond(uint8_t second) {
    for (uint8_t i = 0; i < config.transmissionsPerSlot; i++) {
        if (config.transmissionSeconds[i] == second) {
            return true;
        }
    }
    return false;
}

uint8_t TDMAScheduler::findNextTransmissionSecond(uint8_t currentSecond) {
    // Find the next scheduled transmission second
    for (uint8_t i = 0; i < config.transmissionsPerSlot; i++) {
        if (config.transmissionSeconds[i] > currentSecond) {
            return config.transmissionSeconds[i];
        }
    }

    // If no more transmissions this minute, return the first one
    return config.transmissionSeconds[0];
}

bool TDMAScheduler::checkMinuteAllocation(uint8_t minute) {
    if (config.useAlternatingMinutes) {
        // Device 1 gets even minutes (0, 2, 4, ..., 58)
        // Device 2 gets odd minutes (1, 3, 5, ..., 59)
        if (config.deviceId == 1) {
            return (minute % 2) == 0;
        } else if (config.deviceId == 2) {
            return (minute % 2) == 1;
        } else {
            // For device IDs > 2, use modulo allocation
            return (minute % 10) == (config.deviceId - 1);
        }
    } else {
        // Use custom minute allocation
        return config.customMinutes[minute] == 1;
    }
}
