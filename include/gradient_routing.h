#ifndef GRADIENT_ROUTING_H
#define GRADIENT_ROUTING_H

#include <Arduino.h>
#include "mesh_protocol.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GRADIENT ROUTING                                  ║
// ║  Efficient routing for gateway-centric mesh networks                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         CONFIGURATION                                     ║
// ║  Main settings are in config.h/config.cpp:                                ║
// ║    - USE_GRADIENT_ROUTING                                                 ║
// ║    - BEACON_INTERVAL_MS                                                   ║
// ║    - ROUTE_TIMEOUT_MS                                                     ║
// ║    - BEACON_REBROADCAST_MIN_MS / MAX_MS                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Unknown distance value (no route established)
#define DISTANCE_UNKNOWN            255

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

/**
 * RoutingStats - Track gradient routing performance
 */
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

// ─────────────────────────────────────────────────────────────────────────────
// Initialization
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Initialize gradient routing state
 * Call this once in setup() before using routing functions
 */
void initGradientRouting();

// ─────────────────────────────────────────────────────────────────────────────
// Route Management
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Update routing state when a beacon is received
 *
 * @param senderDistance  Distance reported by beacon sender
 * @param senderId        Node ID of beacon sender (immediate hop)
 * @param gatewayId       Gateway ID this beacon originated from
 * @param beaconSeq       Beacon sequence number (for freshness)
 * @param rssi            RSSI of received beacon (for tiebreaking)
 */
void updateRoutingState(uint8_t senderDistance, uint8_t senderId, uint8_t gatewayId,
                        uint16_t beaconSeq, int16_t rssi);

/**
 * Check if current route has expired and invalidate if necessary
 * Called automatically by hasValidRoute()
 */
void checkRouteExpiration();

/**
 * Manually invalidate the current route
 * Forces fallback to flooding until new beacon received
 */
void invalidateRoute();

// ─────────────────────────────────────────────────────────────────────────────
// Route Queries
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Check if we have a valid route to the gateway
 * @return true if route is valid and not expired
 */
bool hasValidRoute();

/**
 * Get the next hop node ID for gateway-bound traffic
 * @return Node ID of next hop, or 0 if no route
 */
uint8_t getNextHop();

/**
 * Get our current distance to the gateway
 * @return Hop count (255 = unknown)
 */
uint8_t getDistanceToGateway();

/**
 * Check if this node is the gateway
 * @return true if DEVICE_ID == ADDR_GATEWAY
 */
bool isGateway();

/**
 * Get full routing state (for debugging/dashboard)
 * @return Copy of current routing state
 */
RoutingState getRoutingState();

// ─────────────────────────────────────────────────────────────────────────────
// Beacon Handling
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Schedule a beacon for rebroadcast after random delay
 * Only non-gateway nodes should call this
 *
 * @param receivedBeacon  The beacon we received
 * @param rssi            RSSI of received beacon
 */
void scheduleBeaconRebroadcast(const BeaconMsg& receivedBeacon, int16_t rssi);

/**
 * Check if there's a pending beacon ready to send
 * @return true if beacon is ready (delay elapsed)
 */
bool hasPendingBeacon();

/**
 * Get the pending beacon for transmission
 * Clears the pending state after retrieval
 *
 * @param beacon  Output: beacon to transmit
 * @return true if beacon was retrieved, false if none pending
 */
bool getPendingBeacon(BeaconMsg& beacon);

// ─────────────────────────────────────────────────────────────────────────────
// Statistics and Debugging
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Print current routing table to Serial
 */
void printRoutingTable();

/**
 * Print routing statistics to Serial
 */
void printRoutingStats();

/**
 * Get routing statistics struct
 * @return Copy of current statistics
 */
RoutingStats getRoutingStats();

/**
 * Increment unicast forward counter
 * Called when packet is forwarded via gradient routing
 */
void incrementUnicastForwards();

/**
 * Increment flooding fallback counter
 * Called when packet is forwarded via flooding (no route)
 */
void incrementFloodingFallbacks();

#endif // GRADIENT_ROUTING_H
