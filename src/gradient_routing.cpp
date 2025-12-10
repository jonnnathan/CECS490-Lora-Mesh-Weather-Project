#include "gradient_routing.h"
#include "config.h"
#include "mesh_protocol.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL STATE                                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static RoutingState routingState;
static RoutingStats routingStats;

// Pending beacon for rebroadcast (with random delay)
static BeaconMsg pendingBeacon;
static bool beaconPending = false;
static unsigned long beaconScheduledTime = 0;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         INITIALIZATION                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void initGradientRouting() {
    // Initialize routing state
    routingState.distanceToGateway = DISTANCE_UNKNOWN;
    routingState.nextHop = 0;
    routingState.gatewayId = ADDR_GATEWAY;
    routingState.bestRssi = -127;  // Worst possible RSSI
    routingState.lastBeaconSeq = 0;
    routingState.lastBeaconTime = 0;
    routingState.routeValid = false;

    // Gateway always has distance 0 to itself
    if (IS_GATEWAY) {
        routingState.distanceToGateway = 0;
        routingState.nextHop = DEVICE_ID;
        routingState.routeValid = true;
    }

    // Clear statistics
    memset(&routingStats, 0, sizeof(routingStats));

    // Clear pending beacon
    beaconPending = false;

    Serial.println(F(""));
    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║           GRADIENT ROUTING INITIALIZED                    ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));

    if (isGateway()) {
        Serial.println(F("  Mode: GATEWAY (distance = 0)"));
        Serial.println(F("  Will broadcast beacons periodically"));
    } else {
        Serial.println(F("  Mode: NODE (waiting for beacon)"));
        Serial.print(F("  Route timeout: "));
        Serial.print(ROUTE_TIMEOUT_MS / 1000);
        Serial.println(F(" seconds"));
    }
    Serial.println(F("─────────────────────────────────────────────────────────────"));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         ROUTE MANAGEMENT                                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void updateRoutingState(uint8_t senderDistance, uint8_t senderId, uint8_t gatewayId,
                        uint16_t beaconSeq, int16_t rssi) {
    // Gateway doesn't update its route (always distance 0)
    if (isGateway()) {
        routingStats.beaconsReceived++;
        return;
    }

    routingStats.beaconsReceived++;

    // Calculate our distance through this sender
    uint8_t newDistance = senderDistance + 1;

    // Overflow protection
    if (newDistance < senderDistance) {
        newDistance = DISTANCE_UNKNOWN;
    }

    // Determine if this is a better route
    bool shouldUpdate = false;
    const char* updateReason = "";

    // Case 1: No valid route - accept any route
    if (!routingState.routeValid) {
        shouldUpdate = true;
        updateReason = "First route";
    }
    // Case 2: Shorter path - always prefer
    else if (newDistance < routingState.distanceToGateway) {
        shouldUpdate = true;
        updateReason = "Shorter path";
    }
    // Case 3: Same distance but better RSSI - prefer stronger signal
    else if (newDistance == routingState.distanceToGateway && rssi > routingState.bestRssi) {
        shouldUpdate = true;
        updateReason = "Better RSSI";
    }
    // Case 4: Same sender with newer beacon - refresh route
    else if (senderId == routingState.nextHop) {
        shouldUpdate = true;
        updateReason = "Route refresh";
    }

    if (shouldUpdate) {
        uint8_t oldDistance = routingState.distanceToGateway;
        uint8_t oldNextHop = routingState.nextHop;
        int16_t oldRssi = routingState.bestRssi;

        routingState.distanceToGateway = newDistance;
        routingState.nextHop = senderId;
        routingState.gatewayId = gatewayId;
        routingState.bestRssi = rssi;
        routingState.lastBeaconSeq = beaconSeq;
        routingState.lastBeaconTime = millis();
        routingState.routeValid = true;

        routingStats.routeUpdates++;

        // Log route update
        Serial.println(F(""));
        Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
        Serial.println(F("║               ROUTE UPDATED                               ║"));
        Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));
        Serial.print(F("  Reason: "));
        Serial.println(updateReason);
        Serial.print(F("  Distance: "));
        if (oldDistance == DISTANCE_UNKNOWN) {
            Serial.print(F("UNKNOWN"));
        } else {
            Serial.print(oldDistance);
        }
        Serial.print(F(" -> "));
        Serial.print(newDistance);
        Serial.println(F(" hops"));
        Serial.print(F("  Next hop: Node "));
        Serial.print(oldNextHop);
        Serial.print(F(" -> Node "));
        Serial.println(senderId);
        Serial.print(F("  RSSI: "));
        Serial.print(oldRssi);
        Serial.print(F(" -> "));
        Serial.print(rssi);
        Serial.println(F(" dBm"));
        Serial.print(F("  Beacon seq: "));
        Serial.println(beaconSeq);
        Serial.println(F("─────────────────────────────────────────────────────────────"));
    }
}

void checkRouteExpiration() {
    // Gateway route never expires
    if (isGateway()) return;

    if (routingState.routeValid) {
        unsigned long elapsed = millis() - routingState.lastBeaconTime;
        if (elapsed > ROUTE_TIMEOUT_MS) {
            invalidateRoute();
            routingStats.routeExpirations++;

            Serial.println(F(""));
            Serial.println(F("⚠️ ═══════════════════════════════════════════════════════"));
            Serial.println(F("   ROUTE EXPIRED - Falling back to flooding"));
            Serial.print(F("   No beacon received for "));
            Serial.print(elapsed / 1000);
            Serial.println(F(" seconds"));
            Serial.println(F("═══════════════════════════════════════════════════════════"));
        }
    }
}

void invalidateRoute() {
    if (isGateway()) return;  // Gateway route never invalidates

    routingState.routeValid = false;
    routingState.distanceToGateway = DISTANCE_UNKNOWN;
    routingState.bestRssi = -127;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         ROUTE QUERIES                                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool hasValidRoute() {
    // If gradient routing is disabled, always return false (use flooding)
    if (!USE_GRADIENT_ROUTING) {
        return false;
    }
    checkRouteExpiration();
    return routingState.routeValid;
}

uint8_t getNextHop() {
    return routingState.nextHop;
}

uint8_t getDistanceToGateway() {
    return routingState.distanceToGateway;
}

bool isGateway() {
    return IS_GATEWAY;
}

RoutingState getRoutingState() {
    return routingState;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         BEACON HANDLING                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void scheduleBeaconRebroadcast(const BeaconMsg& receivedBeacon, int16_t rssi) {
    // Gateway doesn't rebroadcast beacons (it originates them)
    if (isGateway()) return;

    // Don't rebroadcast if TTL exhausted
    if (receivedBeacon.meshHeader.ttl <= 1) {
        Serial.println(F("  Beacon TTL exhausted, not rebroadcasting"));
        return;
    }

    // Don't rebroadcast our own beacons
    if (receivedBeacon.meshHeader.sourceId == DEVICE_ID) {
        return;
    }

    // Schedule beacon with random delay to prevent collisions
    unsigned long delayMs = random(BEACON_REBROADCAST_MIN_MS, BEACON_REBROADCAST_MAX_MS);
    beaconScheduledTime = millis() + delayMs;

    // Prepare beacon for rebroadcast
    pendingBeacon = receivedBeacon;
    pendingBeacon.distanceToGateway = routingState.distanceToGateway;  // Our distance
    pendingBeacon.meshHeader.senderId = DEVICE_ID;  // We're now the sender
    pendingBeacon.meshHeader.ttl--;  // Decrement TTL

    beaconPending = true;

    Serial.print(F("  Beacon scheduled for rebroadcast in "));
    Serial.print(delayMs);
    Serial.println(F(" ms"));
}

bool hasPendingBeacon() {
    if (!beaconPending) return false;
    return (millis() >= beaconScheduledTime);
}

bool getPendingBeacon(BeaconMsg& beacon) {
    if (!hasPendingBeacon()) return false;

    beacon = pendingBeacon;
    beaconPending = false;
    routingStats.beaconsSent++;

    return true;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STATISTICS AND DEBUGGING                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printRoutingTable() {
    Serial.println(F(""));
    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║               GRADIENT ROUTING TABLE                      ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));

    Serial.print(F("  This Node: "));
    Serial.println(DEVICE_ID);

    Serial.print(F("  Mode: "));
    Serial.println(isGateway() ? F("GATEWAY") : F("NODE"));

    Serial.print(F("  Route Valid: "));
    Serial.println(routingState.routeValid ? F("YES") : F("NO"));

    Serial.print(F("  Distance to Gateway: "));
    if (routingState.distanceToGateway == DISTANCE_UNKNOWN) {
        Serial.println(F("UNKNOWN"));
    } else {
        Serial.print(routingState.distanceToGateway);
        Serial.println(F(" hops"));
    }

    if (!isGateway()) {
        Serial.print(F("  Next Hop: Node "));
        Serial.println(routingState.nextHop);

        Serial.print(F("  Best RSSI: "));
        Serial.print(routingState.bestRssi);
        Serial.println(F(" dBm"));

        Serial.print(F("  Last Beacon Seq: "));
        Serial.println(routingState.lastBeaconSeq);

        if (routingState.routeValid) {
            unsigned long age = (millis() - routingState.lastBeaconTime) / 1000;
            Serial.print(F("  Route Age: "));
            Serial.print(age);
            Serial.print(F(" sec (expires in "));
            unsigned long remaining = (ROUTE_TIMEOUT_MS - (millis() - routingState.lastBeaconTime)) / 1000;
            Serial.print(remaining);
            Serial.println(F(" sec)"));
        }
    }

    Serial.println(F("─────────────────────────────────────────────────────────────"));
}

void printRoutingStats() {
    Serial.println(F(""));
    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║             GRADIENT ROUTING STATISTICS                   ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));

    Serial.print(F("  Beacons Received: "));
    Serial.println(routingStats.beaconsReceived);

    Serial.print(F("  Beacons Sent/Relayed: "));
    Serial.println(routingStats.beaconsSent);

    Serial.print(F("  Route Updates: "));
    Serial.println(routingStats.routeUpdates);

    Serial.print(F("  Unicast Forwards: "));
    Serial.println(routingStats.unicastForwards);

    Serial.print(F("  Flooding Fallbacks: "));
    Serial.println(routingStats.floodingFallbacks);

    Serial.print(F("  Route Expirations: "));
    Serial.println(routingStats.routeExpirations);

    // Calculate efficiency
    unsigned long totalForwards = routingStats.unicastForwards + routingStats.floodingFallbacks;
    if (totalForwards > 0) {
        float efficiency = (float)routingStats.unicastForwards / totalForwards * 100.0f;
        Serial.print(F("  Routing Efficiency: "));
        Serial.print(efficiency, 1);
        Serial.println(F("%"));
    }

    Serial.println(F("─────────────────────────────────────────────────────────────"));
}

RoutingStats getRoutingStats() {
    return routingStats;
}

void incrementUnicastForwards() {
    routingStats.unicastForwards++;
}

void incrementFloodingFallbacks() {
    routingStats.floodingFallbacks++;
}
