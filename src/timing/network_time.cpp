#include "network_time.h"
#include "config.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         CONFIGURATION                                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Maximum age of network time before considered stale (milliseconds)
// If no beacon received within this time, network time becomes invalid
static const unsigned long NETWORK_TIME_MAX_AGE_MS = 120000;  // 2 minutes

// Minimum interval between time updates (prevents rapid updates)
static const unsigned long NETWORK_TIME_MIN_UPDATE_MS = 1000;  // 1 second

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL STATE                                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static NetworkTimeState networkTime;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         INITIALIZATION                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void initNetworkTime() {
    networkTime.hour = 0;
    networkTime.minute = 0;
    networkTime.second = 0;
    networkTime.receivedAtMillis = 0;
    networkTime.lastUpdateTime = 0;
    networkTime.valid = false;
    networkTime.sourceNodeId = 0;
    networkTime.hopCount = 255;  // Max value = no time source

    Serial.println(F("[NET-TIME] Network time sync initialized (multi-hop enabled)"));
    Serial.println(F("[NET-TIME] Waiting for beacon with GPS time..."));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIME UPDATE                                       ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void updateNetworkTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t sourceNode, uint8_t hopCount) {
    unsigned long now = millis();

    // Rate limit updates (prevent rapid beacon storms from causing issues)
    if (networkTime.valid && (now - networkTime.lastUpdateTime) < NETWORK_TIME_MIN_UPDATE_MS) {
        // Exception: Always accept better (lower hop count) sources
        if (hopCount >= networkTime.hopCount) {
            return;
        }
    }

    // Only update if:
    // 1. We have no valid time, OR
    // 2. New source has equal or lower hop count (more accurate), OR
    // 3. Current time is getting old (>30 seconds) - accept any fresh time
    bool shouldUpdate = !networkTime.valid ||
                        hopCount <= networkTime.hopCount ||
                        (now - networkTime.receivedAtMillis) > 30000;

    if (!shouldUpdate) {
        return;
    }

    // Log if we're switching to a different hop count source
    if (networkTime.valid && hopCount != networkTime.hopCount) {
        Serial.printf("[NET-TIME] Switching from %d-hop to %d-hop source\n",
                      networkTime.hopCount, hopCount);
    }

    // Store the received time
    networkTime.hour = hour;
    networkTime.minute = minute;
    networkTime.second = second;
    networkTime.receivedAtMillis = now;
    networkTime.lastUpdateTime = now;
    networkTime.sourceNodeId = sourceNode;
    networkTime.hopCount = hopCount;
    networkTime.valid = true;

    // Log the update
    Serial.printf("[NET-TIME] Time updated: %02d:%02d:%02d from Node %d (hop %d)\n",
                  hour, minute, second, sourceNode, hopCount);
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIME RETRIEVAL                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool getNetworkTime(uint8_t &hour, uint8_t &minute, uint8_t &second) {
    // Check if network time is still valid
    if (!isNetworkTimeValid()) {
        return false;
    }

    // Calculate elapsed time since beacon was received
    unsigned long now = millis();
    unsigned long elapsedMs = now - networkTime.receivedAtMillis;
    unsigned long elapsedSeconds = elapsedMs / 1000;

    // Start from the time we received
    uint32_t totalSeconds = networkTime.hour * 3600UL +
                            networkTime.minute * 60UL +
                            networkTime.second +
                            elapsedSeconds;

    // Handle day wraparound (86400 seconds in a day)
    totalSeconds = totalSeconds % 86400UL;

    // Extract hour, minute, second
    hour = (uint8_t)(totalSeconds / 3600);
    minute = (uint8_t)((totalSeconds % 3600) / 60);
    second = (uint8_t)(totalSeconds % 60);

    return true;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         VALIDITY CHECK                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool isNetworkTimeValid() {
    // Never received a beacon
    if (!networkTime.valid) {
        return false;
    }

    // Check if network time has expired
    unsigned long now = millis();
    unsigned long age = now - networkTime.receivedAtMillis;

    if (age > NETWORK_TIME_MAX_AGE_MS) {
        // Time has expired
        networkTime.valid = false;
        Serial.println(F("[NET-TIME] Network time expired (no recent beacon)"));
        return false;
    }

    return true;
}

void invalidateNetworkTime() {
    networkTime.valid = false;
    Serial.println(F("[NET-TIME] Network time invalidated"));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         UTILITY FUNCTIONS                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

unsigned long getNetworkTimeAge() {
    if (!networkTime.valid || networkTime.receivedAtMillis == 0) {
        return ULONG_MAX;
    }

    return (millis() - networkTime.receivedAtMillis) / 1000;  // Return in seconds
}

uint8_t getNetworkTimeHopCount() {
    if (!networkTime.valid) {
        return 255;  // No valid time
    }
    return networkTime.hopCount;
}

const NetworkTimeState& getNetworkTimeState() {
    return networkTime;
}

const char* getTimeSourceString(TimeSource source) {
    switch (source) {
        case TIME_SOURCE_GPS:     return "GPS";
        case TIME_SOURCE_NETWORK: return "NET";
        case TIME_SOURCE_MANUAL:  return "MANUAL";
        case TIME_SOURCE_NONE:
        default:                  return "NONE";
    }
}

void printNetworkTimeStatus() {
    Serial.println(F(""));
    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║               NETWORK TIME STATUS                         ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));

    Serial.print(F("  Valid: "));
    Serial.println(networkTime.valid ? F("YES") : F("NO"));

    if (networkTime.valid) {
        uint8_t h, m, s;
        if (getNetworkTime(h, m, s)) {
            Serial.printf("  Current Time: %02d:%02d:%02d\n", h, m, s);
        }

        Serial.printf("  Received Time: %02d:%02d:%02d\n",
                      networkTime.hour, networkTime.minute, networkTime.second);

        Serial.print(F("  Source Node: "));
        Serial.println(networkTime.sourceNodeId);

        Serial.print(F("  Hop Count: "));
        Serial.print(networkTime.hopCount);
        if (networkTime.hopCount == 1) {
            Serial.println(F(" (direct from gateway)"));
        } else {
            Serial.println(F(" (relayed)"));
        }

        unsigned long age = getNetworkTimeAge();
        Serial.print(F("  Age: "));
        Serial.print(age);
        Serial.println(F(" seconds"));

        Serial.print(F("  Max Age: "));
        Serial.print(NETWORK_TIME_MAX_AGE_MS / 1000);
        Serial.println(F(" seconds"));
    } else {
        Serial.println(F("  Waiting for beacon with GPS time..."));
    }

    Serial.println(F("─────────────────────────────────────────────────────────────"));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MANUAL TIME SETTING                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void setManualTime(uint8_t hour, uint8_t minute, uint8_t second) {
    unsigned long now = millis();

    // Validate input
    if (hour > 23 || minute > 59 || second > 59) {
        Serial.println(F("[NET-TIME] Invalid manual time - out of range"));
        return;
    }

    // Store the manual time
    networkTime.hour = hour;
    networkTime.minute = minute;
    networkTime.second = second;
    networkTime.receivedAtMillis = now;
    networkTime.lastUpdateTime = now;
    networkTime.sourceNodeId = 0;      // 0 = manual/local
    networkTime.hopCount = 0;          // 0 = highest priority (direct source)
    networkTime.valid = true;

    Serial.println(F(""));
    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║               MANUAL TIME SET (TESTING MODE)              ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));
    Serial.printf("  Time set to: %02d:%02d:%02d UTC\n", hour, minute, second);
    Serial.println(F("  TDMA scheduling is now enabled!"));
    Serial.println(F("  Note: This time will drift - GPS sync is more accurate"));
    Serial.println(F("─────────────────────────────────────────────────────────────"));
}
