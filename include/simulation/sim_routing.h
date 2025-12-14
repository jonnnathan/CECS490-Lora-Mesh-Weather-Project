/**
 * @file sim_routing.h
 * @brief Gradient routing implementation for native simulation.
 *
 * Implements the same routing logic as the ESP32 firmware for accurate
 * multi-hop routing verification.
 */

#ifndef SIM_ROUTING_H
#define SIM_ROUTING_H

#ifndef ARDUINO

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================================
// Configuration (matching ESP32 firmware)
// ============================================================================

constexpr uint8_t DISTANCE_UNKNOWN = 255;
constexpr uint8_t MAX_TTL = 8;
constexpr uint8_t DEFAULT_TTL = 3;

// Timing (faster for simulation)
constexpr uint32_t SIM_BEACON_INTERVAL_MS = 10000;      // 10 seconds (faster than 30s for testing)
constexpr uint32_t SIM_ROUTE_TIMEOUT_MS = 30000;        // 30 seconds
constexpr uint32_t SIM_BEACON_REBROADCAST_MIN_MS = 50;  // 50ms min delay
constexpr uint32_t SIM_BEACON_REBROADCAST_MAX_MS = 200; // 200ms max delay
constexpr uint32_t SIM_NEIGHBOR_TIMEOUT_MS = 60000;     // 60 seconds

// Neighbor table
constexpr uint8_t MAX_NEIGHBORS = 10;

// Duplicate cache
constexpr uint8_t SEEN_CACHE_SIZE = 32;
constexpr uint32_t DUPLICATE_WINDOW_MS = 60000;  // 60 seconds

// ============================================================================
// Message Types (matching mesh_protocol.h)
// ============================================================================

enum MessageType : uint8_t {
    MSG_BEACON = 0x0A,
    MSG_FULL_REPORT = 0x10,
    MSG_ACK = 0x20,
    MSG_DATA = 0x01
};

// ============================================================================
// Packet Structures
// ============================================================================

#pragma pack(push, 1)

/**
 * @brief Mesh packet header (8 bytes)
 */
struct SimMeshHeader {
    uint8_t version;      // Protocol version
    uint8_t messageType;  // MSG_BEACON, MSG_FULL_REPORT, etc.
    uint8_t sourceId;     // Original sender (never changes)
    uint8_t destId;       // Final destination (0 = broadcast)
    uint8_t senderId;     // Immediate sender (changes each hop)
    uint8_t messageId;    // Sequence for duplicate detection
    uint8_t ttl;          // Hop limit
    uint8_t flags;        // Routing flags
};

/**
 * @brief Beacon message structure
 */
struct SimBeaconMsg {
    SimMeshHeader header;
    uint8_t distanceToGateway;
    uint8_t gatewayId;
    uint16_t sequenceNumber;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t timeValid;
};

#pragma pack(pop)

constexpr uint8_t FLAG_IS_FORWARDED = 0x01;
constexpr uint8_t FLAG_NEEDS_ACK = 0x02;

// ============================================================================
// Neighbor Entry
// ============================================================================

struct SimNeighbor {
    uint8_t nodeId;
    int16_t rssi;
    int16_t rssiMin;
    int16_t rssiMax;
    uint32_t lastHeardMs;
    uint8_t packetsReceived;
    bool active;
};

// ============================================================================
// Routing State
// ============================================================================

struct SimRoutingState {
    uint8_t distanceToGateway;
    uint8_t nextHop;
    uint8_t gatewayId;
    int16_t bestRssi;
    uint16_t lastBeaconSeq;
    uint32_t lastBeaconTimeMs;
    bool routeValid;
};

// ============================================================================
// Seen Message (for duplicate detection)
// ============================================================================

struct SimSeenMessage {
    uint8_t sourceId;
    uint8_t messageId;
    uint32_t timestampMs;
    bool valid;
};

// ============================================================================
// Pending Beacon (for rebroadcast scheduling)
// ============================================================================

struct SimPendingBeacon {
    SimBeaconMsg beacon;
    uint32_t scheduledTimeMs;
    bool pending;
};

// ============================================================================
// Packet Delivery Tracking (for verification)
// ============================================================================

struct PacketDeliveryRecord {
    uint8_t sourceId;
    uint8_t messageId;
    uint8_t originHops;      // Hops at origin (for tracking path length)
    uint8_t receivedHops;    // Hops when received at gateway
    uint32_t sentTimeMs;
    uint32_t receivedTimeMs;
    bool delivered;
};

constexpr uint8_t MAX_DELIVERY_RECORDS = 64;

// ============================================================================
// SimRouter Class
// ============================================================================

class SimRouter {
public:
    SimRouter(uint8_t deviceId, bool isGateway);

    // Core routing
    void processBeacon(const SimBeaconMsg& beacon, int16_t rssi, uint32_t nowMs);
    bool shouldForward(const SimMeshHeader& header);
    void prepareForward(SimMeshHeader& header);

    // Beacon management
    void checkBeaconTransmit(uint32_t nowMs);
    bool getPendingBeacon(SimBeaconMsg& outBeacon);
    void createGatewayBeacon(SimBeaconMsg& beacon, uint32_t nowMs);

    // Neighbor management
    void updateNeighbor(uint8_t nodeId, int16_t rssi, uint32_t nowMs);
    void pruneNeighbors(uint32_t nowMs);
    uint8_t getNeighborCount() const;

    // Duplicate detection
    bool isDuplicate(uint8_t sourceId, uint8_t messageId, uint32_t nowMs);
    void markSeen(uint8_t sourceId, uint8_t messageId, uint32_t nowMs);

    // State queries
    uint8_t getDistance() const { return m_state.distanceToGateway; }
    uint8_t getNextHop() const { return m_state.nextHop; }
    bool hasValidRoute() const { return m_state.routeValid; }
    uint8_t getGatewayId() const { return m_state.gatewayId; }

    // Packet tracking (for verification)
    void recordPacketSent(uint8_t messageId, uint32_t nowMs);
    void recordPacketReceived(uint8_t sourceId, uint8_t messageId, uint8_t hops, uint32_t nowMs);
    float getDeliveryRate() const;
    uint32_t getPacketsSent() const { return m_packetsSent; }
    uint32_t getPacketsReceived() const { return m_packetsReceived; }
    uint32_t getPacketsForwarded() const { return m_packetsForwarded; }
    uint32_t getBeaconsReceived() const { return m_beaconsReceived; }

    // Debug
    void printState() const;
    void printNeighbors() const;

private:
    uint8_t m_deviceId;
    bool m_isGateway;
    uint8_t m_messageSeq;

    SimRoutingState m_state;
    SimNeighbor m_neighbors[MAX_NEIGHBORS];
    SimSeenMessage m_seenCache[SEEN_CACHE_SIZE];
    uint8_t m_seenCacheIndex;
    SimPendingBeacon m_pendingBeacon;

    uint32_t m_lastBeaconSentMs;
    uint16_t m_beaconSeq;

    // Statistics
    uint32_t m_packetsSent;
    uint32_t m_packetsReceived;
    uint32_t m_packetsForwarded;
    uint32_t m_beaconsReceived;
    uint32_t m_duplicatesDropped;

    // Delivery tracking
    PacketDeliveryRecord m_deliveryRecords[MAX_DELIVERY_RECORDS];
    uint8_t m_deliveryIndex;
    uint32_t m_deliveredCount;
    uint32_t m_sentCount;
};

// ============================================================================
// Implementation
// ============================================================================

inline SimRouter::SimRouter(uint8_t deviceId, bool isGateway)
    : m_deviceId(deviceId)
    , m_isGateway(isGateway)
    , m_messageSeq(0)
    , m_seenCacheIndex(0)
    , m_lastBeaconSentMs(0)
    , m_beaconSeq(0)
    , m_packetsSent(0)
    , m_packetsReceived(0)
    , m_packetsForwarded(0)
    , m_beaconsReceived(0)
    , m_duplicatesDropped(0)
    , m_deliveryIndex(0)
    , m_deliveredCount(0)
    , m_sentCount(0)
{
    // Initialize routing state
    m_state.distanceToGateway = isGateway ? 0 : DISTANCE_UNKNOWN;
    m_state.nextHop = 0;
    m_state.gatewayId = isGateway ? deviceId : 0;
    m_state.bestRssi = -127;
    m_state.lastBeaconSeq = 0;
    m_state.lastBeaconTimeMs = 0;
    m_state.routeValid = isGateway;  // Gateway always has valid route

    // Clear neighbors
    memset(m_neighbors, 0, sizeof(m_neighbors));

    // Clear seen cache
    memset(m_seenCache, 0, sizeof(m_seenCache));

    // Clear pending beacon
    m_pendingBeacon.pending = false;

    // Clear delivery records
    memset(m_deliveryRecords, 0, sizeof(m_deliveryRecords));
}

inline void SimRouter::processBeacon(const SimBeaconMsg& beacon, int16_t rssi, uint32_t nowMs) {
    m_beaconsReceived++;

    uint8_t newDistance = beacon.distanceToGateway + 1;

    // Overflow check
    if (newDistance < beacon.distanceToGateway) {
        newDistance = DISTANCE_UNKNOWN;
    }

    // Update neighbor table
    updateNeighbor(beacon.header.senderId, rssi, nowMs);

    // Should we update our route?
    bool updateRoute = false;

    if (!m_state.routeValid) {
        // No route - accept any valid beacon
        updateRoute = true;
    } else if (newDistance < m_state.distanceToGateway) {
        // Better (shorter) route found
        updateRoute = true;
    } else if (newDistance == m_state.distanceToGateway && rssi > m_state.bestRssi) {
        // Same distance but better signal
        updateRoute = true;
    } else if (beacon.header.senderId == m_state.nextHop) {
        // Refresh from current next-hop (keep route alive)
        m_state.lastBeaconTimeMs = nowMs;
        m_state.lastBeaconSeq = beacon.sequenceNumber;
    }

    if (updateRoute && !m_isGateway) {
        m_state.distanceToGateway = newDistance;
        m_state.nextHop = beacon.header.senderId;
        m_state.gatewayId = beacon.gatewayId;
        m_state.bestRssi = rssi;
        m_state.lastBeaconSeq = beacon.sequenceNumber;
        m_state.lastBeaconTimeMs = nowMs;
        m_state.routeValid = true;

        printf("[ROUTE] Node %d: Updated route - distance=%d, nextHop=%d, rssi=%d\n",
               m_deviceId, newDistance, beacon.header.senderId, rssi);

        // Schedule beacon rebroadcast
        uint32_t delay = SIM_BEACON_REBROADCAST_MIN_MS +
                        (rand() % (SIM_BEACON_REBROADCAST_MAX_MS - SIM_BEACON_REBROADCAST_MIN_MS));

        m_pendingBeacon.beacon = beacon;
        m_pendingBeacon.beacon.header.senderId = m_deviceId;
        m_pendingBeacon.beacon.header.ttl--;
        m_pendingBeacon.beacon.distanceToGateway = newDistance;
        m_pendingBeacon.scheduledTimeMs = nowMs + delay;
        m_pendingBeacon.pending = (beacon.header.ttl > 1);

        if (m_pendingBeacon.pending) {
            printf("[BEACON] Node %d: Scheduled rebroadcast in %u ms (distance=%d)\n",
                   m_deviceId, delay, newDistance);
        }
    }
}

inline bool SimRouter::shouldForward(const SimMeshHeader& header) {
    // Don't forward own packets
    if (header.sourceId == m_deviceId) {
        return false;
    }

    // Check TTL
    if (header.ttl <= 1) {
        return false;
    }

    // Gateway doesn't forward broadcasts (it's the destination)
    if (m_isGateway && header.destId == 0) {
        return false;
    }

    // Check if packet came from our next-hop (would go backward)
    if (m_state.routeValid && header.senderId == m_state.nextHop) {
        return false;
    }

    return true;
}

inline void SimRouter::prepareForward(SimMeshHeader& header) {
    header.ttl--;
    header.senderId = m_deviceId;
    header.flags |= FLAG_IS_FORWARDED;
    m_packetsForwarded++;
}

inline void SimRouter::checkBeaconTransmit(uint32_t nowMs) {
    // Check route timeout
    if (!m_isGateway && m_state.routeValid) {
        if (nowMs - m_state.lastBeaconTimeMs > SIM_ROUTE_TIMEOUT_MS) {
            printf("[ROUTE] Node %d: Route expired (no beacon for %u ms)\n",
                   m_deviceId, nowMs - m_state.lastBeaconTimeMs);
            m_state.routeValid = false;
            m_state.distanceToGateway = DISTANCE_UNKNOWN;
        }
    }

    // Gateway sends periodic beacons
    if (m_isGateway && (nowMs - m_lastBeaconSentMs >= SIM_BEACON_INTERVAL_MS || m_lastBeaconSentMs == 0)) {
        m_lastBeaconSentMs = nowMs;
    }
}

inline bool SimRouter::getPendingBeacon(SimBeaconMsg& outBeacon) {
    if (!m_pendingBeacon.pending) {
        return false;
    }

    outBeacon = m_pendingBeacon.beacon;
    m_pendingBeacon.pending = false;
    return true;
}

inline void SimRouter::createGatewayBeacon(SimBeaconMsg& beacon, uint32_t nowMs) {
    memset(&beacon, 0, sizeof(beacon));

    beacon.header.version = 1;
    beacon.header.messageType = MSG_BEACON;
    beacon.header.sourceId = m_deviceId;
    beacon.header.destId = 0;  // Broadcast
    beacon.header.senderId = m_deviceId;
    beacon.header.messageId = m_beaconSeq++;
    beacon.header.ttl = DEFAULT_TTL;
    beacon.header.flags = 0;

    beacon.distanceToGateway = 0;
    beacon.gatewayId = m_deviceId;
    beacon.sequenceNumber = m_beaconSeq;

    // Simulated time
    time_t rawTime;
    time(&rawTime);
    struct tm* timeInfo = localtime(&rawTime);
    beacon.hour = timeInfo->tm_hour;
    beacon.minute = timeInfo->tm_min;
    beacon.second = timeInfo->tm_sec;
    beacon.timeValid = 1;
}

inline void SimRouter::updateNeighbor(uint8_t nodeId, int16_t rssi, uint32_t nowMs) {
    if (nodeId == 0) return;

    // Find existing or empty slot
    int emptySlot = -1;
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (m_neighbors[i].active && m_neighbors[i].nodeId == nodeId) {
            // Update existing
            m_neighbors[i].rssi = rssi;
            if (rssi < m_neighbors[i].rssiMin) m_neighbors[i].rssiMin = rssi;
            if (rssi > m_neighbors[i].rssiMax) m_neighbors[i].rssiMax = rssi;
            m_neighbors[i].lastHeardMs = nowMs;
            m_neighbors[i].packetsReceived++;
            return;
        }
        if (!m_neighbors[i].active && emptySlot < 0) {
            emptySlot = i;
        }
    }

    // Add new neighbor
    if (emptySlot >= 0) {
        m_neighbors[emptySlot].nodeId = nodeId;
        m_neighbors[emptySlot].rssi = rssi;
        m_neighbors[emptySlot].rssiMin = rssi;
        m_neighbors[emptySlot].rssiMax = rssi;
        m_neighbors[emptySlot].lastHeardMs = nowMs;
        m_neighbors[emptySlot].packetsReceived = 1;
        m_neighbors[emptySlot].active = true;

        printf("[NEIGHBOR] Node %d: Added neighbor %d (RSSI: %d)\n",
               m_deviceId, nodeId, rssi);
    }
}

inline void SimRouter::pruneNeighbors(uint32_t nowMs) {
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (m_neighbors[i].active) {
            if (nowMs - m_neighbors[i].lastHeardMs > SIM_NEIGHBOR_TIMEOUT_MS) {
                printf("[NEIGHBOR] Node %d: Pruned stale neighbor %d\n",
                       m_deviceId, m_neighbors[i].nodeId);
                m_neighbors[i].active = false;
            }
        }
    }
}

inline uint8_t SimRouter::getNeighborCount() const {
    uint8_t count = 0;
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (m_neighbors[i].active) count++;
    }
    return count;
}

inline bool SimRouter::isDuplicate(uint8_t sourceId, uint8_t messageId, uint32_t nowMs) {
    for (int i = 0; i < SEEN_CACHE_SIZE; i++) {
        if (m_seenCache[i].valid &&
            m_seenCache[i].sourceId == sourceId &&
            m_seenCache[i].messageId == messageId) {
            if (nowMs - m_seenCache[i].timestampMs < DUPLICATE_WINDOW_MS) {
                m_duplicatesDropped++;
                return true;
            }
        }
    }
    return false;
}

inline void SimRouter::markSeen(uint8_t sourceId, uint8_t messageId, uint32_t nowMs) {
    m_seenCache[m_seenCacheIndex].sourceId = sourceId;
    m_seenCache[m_seenCacheIndex].messageId = messageId;
    m_seenCache[m_seenCacheIndex].timestampMs = nowMs;
    m_seenCache[m_seenCacheIndex].valid = true;
    m_seenCacheIndex = (m_seenCacheIndex + 1) % SEEN_CACHE_SIZE;
}

inline void SimRouter::recordPacketSent(uint8_t messageId, uint32_t nowMs) {
    m_packetsSent++;
    m_sentCount++;

    m_deliveryRecords[m_deliveryIndex].sourceId = m_deviceId;
    m_deliveryRecords[m_deliveryIndex].messageId = messageId;
    m_deliveryRecords[m_deliveryIndex].originHops = m_state.distanceToGateway;
    m_deliveryRecords[m_deliveryIndex].sentTimeMs = nowMs;
    m_deliveryRecords[m_deliveryIndex].delivered = false;
    m_deliveryIndex = (m_deliveryIndex + 1) % MAX_DELIVERY_RECORDS;
}

inline void SimRouter::recordPacketReceived(uint8_t sourceId, uint8_t messageId, uint8_t hops, uint32_t nowMs) {
    m_packetsReceived++;

    // If we're gateway, mark delivery
    if (m_isGateway) {
        for (int i = 0; i < MAX_DELIVERY_RECORDS; i++) {
            if (m_deliveryRecords[i].sourceId == sourceId &&
                m_deliveryRecords[i].messageId == messageId &&
                !m_deliveryRecords[i].delivered) {
                m_deliveryRecords[i].delivered = true;
                m_deliveryRecords[i].receivedTimeMs = nowMs;
                m_deliveryRecords[i].receivedHops = hops;
                m_deliveredCount++;
                break;
            }
        }
    }
}

inline float SimRouter::getDeliveryRate() const {
    if (m_sentCount == 0) return 0.0f;
    return (float)m_deliveredCount / (float)m_sentCount * 100.0f;
}

inline void SimRouter::printState() const {
    printf("[STATE] Node %d: distance=%d, nextHop=%d, gateway=%d, routeValid=%s\n",
           m_deviceId,
           m_state.distanceToGateway,
           m_state.nextHop,
           m_state.gatewayId,
           m_state.routeValid ? "yes" : "no");
}

inline void SimRouter::printNeighbors() const {
    printf("[NEIGHBORS] Node %d has %d neighbors:\n", m_deviceId, getNeighborCount());
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        if (m_neighbors[i].active) {
            printf("  - Node %d: RSSI %d (min:%d, max:%d), packets:%d\n",
                   m_neighbors[i].nodeId,
                   m_neighbors[i].rssi,
                   m_neighbors[i].rssiMin,
                   m_neighbors[i].rssiMax,
                   m_neighbors[i].packetsReceived);
        }
    }
}

#endif // ARDUINO
#endif // SIM_ROUTING_H
