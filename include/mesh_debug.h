#ifndef MESH_DEBUG_H
#define MESH_DEBUG_H

#include <Arduino.h>
#include "mesh_protocol.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DEBUG CONFIGURATION                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Master Debug Flag
 * Set to true to enable all mesh debugging output
 * Set to false to disable (saves memory and improves performance)
 */
#define DEBUG_MESH_ENABLED true

/**
 * Individual Debug Categories
 * Enable/disable specific debug output categories
 */
#define DEBUG_MESH_RX           true   // Packet reception details
#define DEBUG_MESH_TX           true   // Packet transmission details
#define DEBUG_MESH_FORWARD      true   // Forwarding decisions
#define DEBUG_MESH_DUPLICATE    true   // Duplicate detection
#define DEBUG_MESH_NEIGHBOR     true   // Neighbor table updates
#define DEBUG_MESH_QUEUE        true   // Queue operations
#define DEBUG_MESH_TTL          true   // TTL expiration events
#define DEBUG_MESH_STATS        false  // Statistics updates (verbose)
#define DEBUG_MESH_TIMING       false  // TDMA timing details (very verbose)

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DEBUG MACROS                                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#if DEBUG_MESH_ENABLED

// Packet Reception
#if DEBUG_MESH_RX
    #define DEBUG_RX(msg) Serial.print(F("[RX] ")); Serial.println(msg)
    #define DEBUG_RX_F(fmt, ...) Serial.printf("[RX] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_RX(msg)
    #define DEBUG_RX_F(fmt, ...)
#endif

// Packet Transmission
#if DEBUG_MESH_TX
    #define DEBUG_TX(msg) Serial.print(F("[TX] ")); Serial.println(msg)
    #define DEBUG_TX_F(fmt, ...) Serial.printf("[TX] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_TX(msg)
    #define DEBUG_TX_F(fmt, ...)
#endif

// Forwarding
#if DEBUG_MESH_FORWARD
    #define DEBUG_FWD(msg) Serial.print(F("[FWD] ")); Serial.println(msg)
    #define DEBUG_FWD_F(fmt, ...) Serial.printf("[FWD] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_FWD(msg)
    #define DEBUG_FWD_F(fmt, ...)
#endif

// Duplicate Detection
#if DEBUG_MESH_DUPLICATE
    #define DEBUG_DUP(msg) Serial.print(F("[DUP] ")); Serial.println(msg)
    #define DEBUG_DUP_F(fmt, ...) Serial.printf("[DUP] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_DUP(msg)
    #define DEBUG_DUP_F(fmt, ...)
#endif

// Neighbor Table
#if DEBUG_MESH_NEIGHBOR
    #define DEBUG_NBR(msg) Serial.print(F("[NBR] ")); Serial.println(msg)
    #define DEBUG_NBR_F(fmt, ...) Serial.printf("[NBR] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_NBR(msg)
    #define DEBUG_NBR_F(fmt, ...)
#endif

// Queue Operations
#if DEBUG_MESH_QUEUE
    #define DEBUG_QUE(msg) Serial.print(F("[QUE] ")); Serial.println(msg)
    #define DEBUG_QUE_F(fmt, ...) Serial.printf("[QUE] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_QUE(msg)
    #define DEBUG_QUE_F(fmt, ...)
#endif

// TTL Expiration
#if DEBUG_MESH_TTL
    #define DEBUG_TTL(msg) Serial.print(F("[TTL] ")); Serial.println(msg)
    #define DEBUG_TTL_F(fmt, ...) Serial.printf("[TTL] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_TTL(msg)
    #define DEBUG_TTL_F(fmt, ...)
#endif

// Statistics
#if DEBUG_MESH_STATS
    #define DEBUG_STAT(msg) Serial.print(F("[STAT] ")); Serial.println(msg)
    #define DEBUG_STAT_F(fmt, ...) Serial.printf("[STAT] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_STAT(msg)
    #define DEBUG_STAT_F(fmt, ...)
#endif

// Timing
#if DEBUG_MESH_TIMING
    #define DEBUG_TIME(msg) Serial.print(F("[TIME] ")); Serial.println(msg)
    #define DEBUG_TIME_F(fmt, ...) Serial.printf("[TIME] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_TIME(msg)
    #define DEBUG_TIME_F(fmt, ...)
#endif

#else
// All debugging disabled
#define DEBUG_RX(msg)
#define DEBUG_RX_F(fmt, ...)
#define DEBUG_TX(msg)
#define DEBUG_TX_F(fmt, ...)
#define DEBUG_FWD(msg)
#define DEBUG_FWD_F(fmt, ...)
#define DEBUG_DUP(msg)
#define DEBUG_DUP_F(fmt, ...)
#define DEBUG_NBR(msg)
#define DEBUG_NBR_F(fmt, ...)
#define DEBUG_QUE(msg)
#define DEBUG_QUE_F(fmt, ...)
#define DEBUG_TTL(msg)
#define DEBUG_TTL_F(fmt, ...)
#define DEBUG_STAT(msg)
#define DEBUG_STAT_F(fmt, ...)
#define DEBUG_TIME(msg)
#define DEBUG_TIME_F(fmt, ...)
#endif

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         HELPER FUNCTIONS                                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Print MeshHeader in a formatted way
 */
inline void debugPrintMeshHeader(const char* prefix, const MeshHeader* header) {
#if DEBUG_MESH_ENABLED
    Serial.print(prefix);
    Serial.print(F(" [src="));
    Serial.print(header->sourceId);
    Serial.print(F(" dst="));
    Serial.print(header->destId);
    Serial.print(F(" via="));
    Serial.print(header->senderId);
    Serial.print(F(" msg="));
    Serial.print(header->messageId);
    Serial.print(F(" ttl="));
    Serial.print(header->ttl);
    Serial.print(F(" flg=0x"));
    Serial.print(header->flags, HEX);
    Serial.println(F("]"));
#endif
}

/**
 * Print packet reception details
 */
inline void debugLogPacketRx(const MeshHeader* header, int16_t rssi, float snr) {
#if DEBUG_MESH_RX
    DEBUG_RX_F("Packet from Node %d via Node %d | msgId=%d ttl=%d rssi=%d snr=%.1f",
               header->sourceId, header->senderId, header->messageId,
               header->ttl, rssi, snr);
#endif
}

/**
 * Print forwarding decision with reason
 */
inline void debugLogForwardDecision(bool shouldForward, const char* reason, const MeshHeader* header) {
#if DEBUG_MESH_FORWARD
    if (shouldForward) {
        DEBUG_FWD_F("FORWARD: Node %d msg=%d ttl=%d -> Enqueuing",
                    header->sourceId, header->messageId, header->ttl);
    } else {
        DEBUG_FWD_F("DROP: Node %d msg=%d ttl=%d | Reason: %s",
                    header->sourceId, header->messageId, header->ttl, reason);
    }
#endif
}

/**
 * Print neighbor table update
 */
inline void debugLogNeighborUpdate(uint8_t nodeId, int16_t rssi, uint8_t packets) {
#if DEBUG_MESH_NEIGHBOR
    DEBUG_NBR_F("Update: Node %d rssi=%d packets=%d", nodeId, rssi, packets);
#endif
}

/**
 * Print queue operation
 */
inline void debugLogQueueOp(const char* operation, uint8_t depth, uint8_t maxDepth) {
#if DEBUG_MESH_QUEUE
    DEBUG_QUE_F("%s | depth=%d/%d", operation, depth, maxDepth);
#endif
}

/**
 * Print duplicate detection
 */
inline void debugLogDuplicate(uint8_t sourceId, uint8_t messageId, bool isDup) {
#if DEBUG_MESH_DUPLICATE
    if (isDup) {
        DEBUG_DUP_F("DUPLICATE: Node %d msg=%d DROPPED", sourceId, messageId);
    } else {
        DEBUG_DUP_F("NEW: Node %d msg=%d marked as seen", sourceId, messageId);
    }
#endif
}

/**
 * Print TDMA slot timing
 */
inline void debugLogSlotTiming(const char* event, uint8_t currentSecond, uint8_t slotStart, uint8_t slotEnd) {
#if DEBUG_MESH_TIMING
    DEBUG_TIME_F("%s | sec=%d slot=[%d-%d]", event, currentSecond, slotStart, slotEnd);
#endif
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         USAGE EXAMPLES                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/*
EXAMPLE 1: Simple message
    DEBUG_RX("Packet received");

EXAMPLE 2: Formatted message with variables
    DEBUG_FWD_F("Forwarding packet from Node %d with TTL %d", sourceId, ttl);

EXAMPLE 3: Conditional logging
    if (queueFull) {
        DEBUG_QUE("Queue overflow - packet dropped");
    }

EXAMPLE 4: Helper function
    debugPrintMeshHeader("[HEADER]", &header);
    debugLogPacketRx(&header, rssi, snr);

EXAMPLE 5: Multi-category logging
    DEBUG_RX("Received packet");
    debugLogDuplicate(sourceId, messageId, isDuplicate);
    if (!isDuplicate) {
        debugLogNeighborUpdate(senderId, rssi, packetCount);
    }
*/

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         LOG FORMAT SPECIFICATION                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/*
Consistent log format for easy parsing:

[CATEGORY] Description | key1=value1 key2=value2 ...

Categories:
  [RX]    - Packet reception
  [TX]    - Packet transmission
  [FWD]   - Forwarding decisions
  [DUP]   - Duplicate detection
  [NBR]   - Neighbor table operations
  [QUE]   - Queue operations
  [TTL]   - TTL expiration
  [STAT]  - Statistics updates
  [TIME]  - TDMA timing

Example logs:
  [RX] Packet from Node 3 via Node 2 | msgId=42 ttl=2 rssi=-75 snr=8.5
  [FWD] FORWARD: Node 3 msg=42 ttl=2 -> Enqueuing
  [DUP] DUPLICATE: Node 3 msg=42 DROPPED
  [NBR] Update: Node 2 rssi=-75 packets=10
  [QUE] Enqueue success | depth=2/8
  [TTL] EXPIRED: Node 5 msg=99 ttl=1
  [TX] Transmitting own report | seq=156 size=39
*/

#endif // MESH_DEBUG_H
