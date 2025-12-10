# Gradient Routing Implementation Plan

## Overview
Migrate from flooding-based mesh to gradient-based routing to reduce bandwidth usage by ~64% while maintaining simplicity and reliability.

---

## Progress Tracker

- [x] **Phase 1**: Understanding Current Architecture
- [x] **Phase 2**: Design New Packet Types (MSG_BEACON, BeaconMsg structure)
- [ ] **Phase 3**: Add Routing State to Each Node
- [ ] **Phase 4**: Gateway Beacon Broadcasting
- [ ] **Phase 5**: Node Beacon Reception & Route Update
- [ ] **Phase 6**: Modify Data Forwarding Logic
- [ ] **Phase 7**: Packet Reception Filter
- [ ] **Phase 8**: Add Configuration Options
- [ ] **Phase 9**: Testing & Validation
- [ ] **Phase 10**: Performance Monitoring
- [ ] **Phase 11**: Documentation

---

## Phase 3: Add Routing State to Each Node

### Create New Files:

**include/gradient_routing.h**
```cpp
#ifndef GRADIENT_ROUTING_H
#define GRADIENT_ROUTING_H

#include <Arduino.h>
#include "mesh_protocol.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         ROUTING STATE                                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * RoutingState - Stores gradient routing information for this node
 *
 * Each node maintains its distance to the gateway and the best
 * next-hop neighbor for forwarding data toward the gateway.
 */
struct RoutingState {
    uint8_t  distanceToGateway;    // Hop count (255 = unknown/no route)
    uint8_t  nextHop;              // Node ID to forward to (for gateway-bound traffic)
    uint8_t  gatewayId;            // Which gateway we're routing toward
    int16_t  bestRssi;             // RSSI of best path (for tiebreaking)
    uint16_t lastBeaconSeq;        // Last beacon sequence received
    unsigned long lastBeaconTime;  // When last beacon was received (millis)
    bool     routeValid;           // Is route still fresh?
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         ROUTING STATISTICS                                ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

struct RoutingStats {
    unsigned long beaconsReceived;     // Total beacons received
    unsigned long beaconsSent;         // Beacons sent (gateway) or relayed (nodes)
    unsigned long routeUpdates;        // Times route was updated
    unsigned long unicastForwards;     // Packets forwarded via gradient routing
    unsigned long floodingFallbacks;   // Times we fell back to flooding
    unsigned long routeExpirations;    // Times route expired
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         FUNCTION DECLARATIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Initialization
void initGradientRouting();

// Route management
void updateRoutingState(uint8_t senderDistance, uint8_t senderId, uint8_t gatewayId,
                        uint16_t beaconSeq, int16_t rssi);
void checkRouteExpiration();
void invalidateRoute();

// Route queries
bool hasValidRoute();
uint8_t getNextHop();
uint8_t getDistanceToGateway();
bool isGateway();

// Beacon handling
void scheduleBeaconRebroadcast(const BeaconMsg& receivedBeacon, int16_t rssi);
bool hasPendingBeacon();
bool getPendingBeacon(BeaconMsg& beacon);

// Statistics and debugging
void printRoutingTable();
void printRoutingStats();
RoutingStats getRoutingStats();

#endif // GRADIENT_ROUTING_H
```

**src/gradient_routing.cpp**
```cpp
#include "gradient_routing.h"
#include "config.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL STATE                                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static RoutingState routingState;
static RoutingStats routingStats;

// Pending beacon for rebroadcast (with random delay)
static BeaconMsg pendingBeacon;
static bool beaconPending = false;
static unsigned long beaconScheduledTime = 0;

// Configuration constants (can be moved to config.h later)
static const unsigned long ROUTE_TIMEOUT_MS = 60000;      // Route expires after 60 seconds
static const unsigned long BEACON_REBROADCAST_MIN_MS = 100;  // Min delay before rebroadcast
static const unsigned long BEACON_REBROADCAST_MAX_MS = 500;  // Max delay before rebroadcast

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         INITIALIZATION                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void initGradientRouting() {
    // Initialize routing state
    routingState.distanceToGateway = 255;  // Unknown
    routingState.nextHop = 0;
    routingState.gatewayId = ADDR_GATEWAY;
    routingState.bestRssi = -127;          // Worst possible
    routingState.lastBeaconSeq = 0;
    routingState.lastBeaconTime = 0;
    routingState.routeValid = false;

    // Gateway always has distance 0 to itself
    if (DEVICE_ID == ADDR_GATEWAY) {
        routingState.distanceToGateway = 0;
        routingState.nextHop = DEVICE_ID;
        routingState.routeValid = true;
    }

    // Clear statistics
    memset(&routingStats, 0, sizeof(routingStats));

    // Clear pending beacon
    beaconPending = false;

    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║           GRADIENT ROUTING INITIALIZED                    ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));

    if (isGateway()) {
        Serial.println(F("  Mode: GATEWAY (distance = 0)"));
    } else {
        Serial.println(F("  Mode: NODE (waiting for beacon)"));
    }
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

    // Determine if this is a better route
    bool shouldUpdate = false;

    // Case 1: No valid route - accept any route
    if (!routingState.routeValid) {
        shouldUpdate = true;
    }
    // Case 2: Shorter path - always prefer
    else if (newDistance < routingState.distanceToGateway) {
        shouldUpdate = true;
    }
    // Case 3: Same distance but better RSSI - prefer stronger signal
    else if (newDistance == routingState.distanceToGateway && rssi > routingState.bestRssi) {
        shouldUpdate = true;
    }
    // Case 4: Same sender with newer beacon - refresh route
    else if (senderId == routingState.nextHop && beaconSeq > routingState.lastBeaconSeq) {
        shouldUpdate = true;
    }

    if (shouldUpdate) {
        uint8_t oldDistance = routingState.distanceToGateway;
        uint8_t oldNextHop = routingState.nextHop;

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
        Serial.print(F("  Distance: "));
        Serial.print(oldDistance);
        Serial.print(F(" → "));
        Serial.println(newDistance);
        Serial.print(F("  Next hop: Node "));
        Serial.print(oldNextHop);
        Serial.print(F(" → Node "));
        Serial.println(senderId);
        Serial.print(F("  RSSI: "));
        Serial.print(rssi);
        Serial.println(F(" dBm"));
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
            Serial.println(F("⚠️ Route expired - falling back to flooding"));
        }
    }
}

void invalidateRoute() {
    if (isGateway()) return;  // Gateway route never invalidates

    routingState.routeValid = false;
    routingState.distanceToGateway = 255;
    routingState.bestRssi = -127;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         ROUTE QUERIES                                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool hasValidRoute() {
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
    return (DEVICE_ID == ADDR_GATEWAY);
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         BEACON HANDLING                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void scheduleBeaconRebroadcast(const BeaconMsg& receivedBeacon, int16_t rssi) {
    // Gateway doesn't rebroadcast beacons (it originates them)
    if (isGateway()) return;

    // Don't rebroadcast if TTL exhausted
    if (receivedBeacon.meshHeader.ttl <= 1) return;

    // Schedule beacon with random delay to prevent collisions
    unsigned long delay = random(BEACON_REBROADCAST_MIN_MS, BEACON_REBROADCAST_MAX_MS);
    beaconScheduledTime = millis() + delay;

    // Prepare beacon for rebroadcast
    pendingBeacon = receivedBeacon;
    pendingBeacon.distanceToGateway = routingState.distanceToGateway;  // Our distance
    pendingBeacon.meshHeader.senderId = DEVICE_ID;  // We're now the sender
    pendingBeacon.meshHeader.ttl--;  // Decrement TTL

    beaconPending = true;

    Serial.print(F("📡 Beacon scheduled for rebroadcast in "));
    Serial.print(delay);
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
    if (routingState.distanceToGateway == 255) {
        Serial.println(F("UNKNOWN"));
    } else {
        Serial.print(routingState.distanceToGateway);
        Serial.println(F(" hops"));
    }

    Serial.print(F("  Next Hop: Node "));
    Serial.println(routingState.nextHop);

    Serial.print(F("  Best RSSI: "));
    Serial.print(routingState.bestRssi);
    Serial.println(F(" dBm"));

    if (routingState.routeValid) {
        unsigned long age = (millis() - routingState.lastBeaconTime) / 1000;
        Serial.print(F("  Route Age: "));
        Serial.print(age);
        Serial.println(F(" sec"));
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

    Serial.println(F("─────────────────────────────────────────────────────────────"));
}

RoutingStats getRoutingStats() {
    return routingStats;
}
```

---

## Phase 4: Gateway Beacon Broadcasting

### Modify main.cpp

Add beacon timer and sending logic for gateway node:

```cpp
// Add to includes
#include "gradient_routing.h"

// Add global variables
static unsigned long lastBeaconTime = 0;
const unsigned long BEACON_INTERVAL_MS = 30000;  // 30 seconds

// Add to setup()
initGradientRouting();

// Add to loop() - Gateway beacon sending
if (IS_GATEWAY) {
    unsigned long currentTime = millis();
    if (currentTime - lastBeaconTime >= BEACON_INTERVAL_MS) {
        sendGatewayBeacon();
        lastBeaconTime = currentTime;
    }
}

// Add new function
void sendGatewayBeacon() {
    BeaconMsg beacon;
    beacon.distanceToGateway = 0;  // Gateway is distance 0
    beacon.gatewayId = DEVICE_ID;
    beacon.sequenceNumber = millis() / 1000;  // Use uptime as sequence

    uint8_t buffer[16];
    uint8_t len = encodeBeacon(buffer, beacon);

    if (sendBinaryMessage(buffer, len)) {
        Serial.println(F("📡 Gateway beacon sent"));
    }
}
```

---

## Phase 5: Node Beacon Reception & Route Update

### Modify packet_handler.cpp

Add beacon handling to checkForIncomingMessages():

```cpp
// Add to includes
#include "gradient_routing.h"

// In checkForIncomingMessages(), add beacon handling:
MessageType msgType = getMessageType(packet.payloadBytes, packet.payloadLen);

if (msgType == MSG_BEACON) {
    // Handle beacon packet
    BeaconMsg beacon;
    if (decodeBeacon(packet.payloadBytes, packet.payloadLen, beacon)) {
        // Update routing state
        updateRoutingState(
            beacon.distanceToGateway,
            beacon.meshHeader.senderId,
            beacon.gatewayId,
            beacon.sequenceNumber,
            packet.rssi
        );

        // Schedule rebroadcast (non-gateway nodes only)
        scheduleBeaconRebroadcast(beacon, packet.rssi);

        Serial.print(F("📡 Beacon from Node "));
        Serial.print(beacon.meshHeader.senderId);
        Serial.print(F(" (distance="));
        Serial.print(beacon.distanceToGateway);
        Serial.println(F(")"));
    }
    continue;  // Don't process beacon as data
}

// Existing MSG_FULL_REPORT handling continues below...
```

### Add beacon rebroadcast in loop()

```cpp
// In loop(), add beacon rebroadcast check
if (hasPendingBeacon()) {
    BeaconMsg beacon;
    if (getPendingBeacon(beacon)) {
        uint8_t buffer[16];
        uint8_t len = encodeBeacon(buffer, beacon);
        sendBinaryMessage(buffer, len);
        Serial.println(F("📡 Beacon rebroadcasted"));
    }
}
```

---

## Phase 6: Modify Data Forwarding Logic

### Modify scheduleForward() in packet_handler.cpp

Replace flooding with gradient routing (with fallback):

```cpp
void scheduleForward(uint8_t* data, uint8_t len, MeshHeader* header) {
    // Create a copy of the packet for modification
    uint8_t forwardBuffer[64];

    if (len == 0 || len > 64) {
        Serial.println(F("⚠️ Forward failed: invalid packet length"));
        return;
    }

    memcpy(forwardBuffer, data, len);
    MeshHeader* forwardHeader = (MeshHeader*)forwardBuffer;

    // ═══════════════════════════════════════════════════════════════
    // GRADIENT ROUTING DECISION
    // ═══════════════════════════════════════════════════════════════

    if (USE_GRADIENT_ROUTING && hasValidRoute()) {
        // Use gradient routing - forward toward gateway
        uint8_t nextHop = getNextHop();

        forwardHeader->senderId = DEVICE_ID;
        forwardHeader->ttl--;
        forwardHeader->flags |= FLAG_IS_FORWARDED;

        // Note: LoRa is broadcast, but conceptually this is "toward" nextHop
        // The nextHop node will continue forwarding toward gateway

        if (transmitQueue.enqueue(forwardBuffer, len)) {
            incrementUnicastForwards();  // Track gradient routing usage
            Serial.print(F("📤 Gradient forward via Node "));
            Serial.println(nextHop);
        }
    } else {
        // Fallback to flooding (no valid route)
        incrementFloodingFallbacks();

        forwardHeader->ttl--;
        forwardHeader->senderId = DEVICE_ID;
        forwardHeader->flags |= FLAG_IS_FORWARDED;

        if (transmitQueue.enqueue(forwardBuffer, len)) {
            Serial.println(F("📤 Flooding forward (no gradient route)"));
        }
    }
}
```

---

## Phase 7: Packet Reception Filter (Optional Optimization)

### Add intelligent relay decision

In nodes (non-gateway), we can optimize by only forwarding if we're on the path toward gateway:

```cpp
bool shouldForward(MeshHeader* header) {
    // Existing checks...
    if (header->ttl <= 1) return false;
    if (header->sourceId == DEVICE_ID) return false;
    if (DEVICE_ID == ADDR_GATEWAY && header->destId == ADDR_BROADCAST) return false;

    // ═══════════════════════════════════════════════════════════════
    // GRADIENT ROUTING OPTIMIZATION
    // ═══════════════════════════════════════════════════════════════
    // Only forward if we're "closer" to gateway than the sender
    // This prevents unnecessary rebroadcasts from nodes further from gateway

    if (USE_GRADIENT_ROUTING && hasValidRoute()) {
        uint8_t senderDistance = getSenderDistance(header->senderId);
        uint8_t ourDistance = getDistanceToGateway();

        // Only forward if we're closer to gateway
        if (ourDistance >= senderDistance && !isGateway()) {
            Serial.println(F("🚫 Not forwarding - not on optimal path"));
            return false;
        }
    }

    return true;
}
```

---

## Phase 8: Add Configuration Options

### Add to config.h

```cpp
// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GRADIENT ROUTING CONFIGURATION                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

extern const bool USE_GRADIENT_ROUTING;       // Enable/disable gradient routing
extern const unsigned long BEACON_INTERVAL_MS; // How often gateway sends beacons
extern const unsigned long ROUTE_TIMEOUT_MS;   // Route expiration time
extern const bool FALLBACK_TO_FLOODING;        // Fall back if no route
```

### Add to config.cpp

```cpp
// Gradient Routing Configuration
const bool USE_GRADIENT_ROUTING = true;           // Enable gradient routing
const unsigned long BEACON_INTERVAL_MS = 30000;   // 30 seconds between beacons
const unsigned long ROUTE_TIMEOUT_MS = 60000;     // Routes expire after 60 seconds
const bool FALLBACK_TO_FLOODING = true;           // Fall back to flooding if no route
```

---

## Phase 9: Testing & Validation

### Test Cases:

1. **Gateway beacon propagation**
   - Gateway sends beacon every 30 seconds
   - Verify all nodes receive and update distance
   - Check Serial Monitor for routing table updates

2. **Route establishment**
   - Power on gateway first
   - Power on nodes one by one
   - Verify each learns correct distance
   - Run `printRoutingTable()` on each node

3. **Data transmission efficiency**
   - Send data from furthest node (Node 5)
   - Count transmissions in Serial Monitor
   - Compare to flooding baseline

4. **Route expiration & recovery**
   - Turn off relay node (Node 2)
   - Verify route expires after 60 seconds
   - Check if fallback to flooding works
   - Turn Node 2 back on, verify route re-established

5. **Multi-hop verification**
   - Ensure packets traverse correct path
   - Verify `printPacketRoute()` shows gradient paths

### Expected Serial Output:

```
╔═══════════════════════════════════════════════════════════╗
║               GRADIENT ROUTING TABLE                      ║
╚═══════════════════════════════════════════════════════════╝
  This Node: 5
  Mode: NODE
  Route Valid: YES
  Distance to Gateway: 3 hops
  Next Hop: Node 4
  Best RSSI: -72 dBm
  Route Age: 15 sec
─────────────────────────────────────────────────────────────
```

---

## Phase 10: Performance Monitoring

### Add statistics tracking

Functions already defined in gradient_routing.h:
- `printRoutingStats()` - Print routing statistics
- `getRoutingStats()` - Get stats struct for dashboard

### Add to web dashboard (optional)

Display routing information on web dashboard:
- Current distance to gateway
- Next hop node
- Route validity
- Beacon statistics

---

## Phase 11: Documentation

### Create GRADIENT_ROUTING.md

Document:
- How gradient routing works
- Configuration options
- Troubleshooting guide
- Performance comparison vs flooding
- Visual diagrams

---

## Rollback Plan

### If Issues Occur:

1. **Keep flooding logic intact** - Don't delete old code
2. **Config flag** - Set `USE_GRADIENT_ROUTING = false` to revert
3. **Graceful degradation** - If no route, fall back to flooding
4. **Git commit** - Commit before starting each phase

### Risk Mitigation:

- Nodes still receive broadcasts (LoRa limitation)
- Filtering happens at application layer
- TTL still prevents infinite loops
- Duplicate detection still works

---

## Expected Outcomes

### Before (Flooding):
- 5 nodes send 10 packets each: **~250 transmissions**
- 70% wasted bandwidth

### After (Gradient Routing):
- Setup: 6 beacons (one-time per 30 seconds)
- 5 nodes send 10 packets each: **~90 useful transmissions**
- **64% reduction in transmissions**

---

## Files to Create/Modify

### New Files:
- `include/gradient_routing.h` - Routing state and functions
- `src/gradient_routing.cpp` - Implementation

### Modified Files:
- `include/mesh_protocol.h` - Add MSG_BEACON, BeaconMsg (DONE)
- `include/lora_comm.h` - Add beacon encode/decode declarations (DONE)
- `src/lora_comm.cpp` - Add beacon encode/decode implementation (DONE)
- `src/packet_handler.cpp` - Add beacon handling, modify forwarding
- `src/main.cpp` - Add beacon timer, init gradient routing
- `include/config.h` - Add gradient routing config
- `src/config.cpp` - Add gradient routing config values

---

## Questions Answered (From Planning)

1. **Keep flooding as fallback?** YES - for robustness
2. **Beacon interval?** 30 seconds
3. **Route timeout?** 60 seconds (2x beacon interval)
4. **RSSI-based path selection?** Hop-count primary, RSSI tiebreaker
5. **Beacon rebroadcast delay?** Random 100-500ms to reduce collisions
