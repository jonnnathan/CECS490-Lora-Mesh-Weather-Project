#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

#include <Arduino.h>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MESH PROTOCOL DEFINITIONS                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Protocol Version
 * Increment this when making breaking changes to the protocol structure
 */
#define MESH_PROTOCOL_VERSION 1

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MESSAGE TYPES                                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Message Type Identifiers
 * Used in MeshHeader.messageType to indicate the type of message being sent
 */
enum MessageType : uint8_t {
    MSG_FULL_REPORT = 0x01,  // Complete sensor/GPS/status report (38 bytes total)
    MSG_ROUTED_DATA = 0x02,  // General routed data packet (variable payload)
    MSG_ACK         = 0x03,  // Acknowledgment message (confirms receipt)
    MSG_BEACON      = 0x0A,  // Gradient routing beacon (gateway distance advertisement)

    // Legacy message types (for backward compatibility)
    MSG_HEARTBEAT   = 0x04,  // Simple heartbeat/keepalive
    MSG_SENSOR_DATA = 0x05,  // Legacy sensor data format
    MSG_GPS_DATA    = 0x06,  // Legacy GPS data format
    MSG_STATUS      = 0x07,  // Legacy status message
    MSG_TEXT        = 0x08,  // Text message
    MSG_ALERT       = 0x09   // Alert/warning message
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         SPECIAL ADDRESSES                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Special Address Constants
 * These reserved addresses have special meaning in the mesh network
 */
#define ADDR_BROADCAST  0xFF  // Broadcast to all nodes (destId = 0xFF)
#define ADDR_GATEWAY    0x00  // Gateway node address (node with internet access)

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         FLAG DEFINITIONS                                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Message Flag Bits
 * Bit flags that can be combined in the 'flags' field of MeshHeader
 */
#define FLAG_NEEDS_ACK      0x01  // Sender expects an ACK response (bit 0)
#define FLAG_IS_FORWARDED   0x02  // Message has been forwarded at least once (bit 1)

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TTL CONSTANT                                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Default Time To Live
 * Maximum number of hops a message can take before being dropped
 * Each forwarding node decrements TTL; message is discarded when TTL reaches 0
 */
#define MESH_DEFAULT_TTL    3

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MESH HEADER STRUCTURE                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * MeshHeader - 8-byte header for all mesh protocol messages
 *
 * This header provides routing information for multi-hop mesh networking.
 * Total size: 8 bytes (packed structure with no padding)
 *
 * Field Descriptions:
 * -------------------
 * version    - Protocol version (currently MESH_PROTOCOL_VERSION = 1)
 *              Allows nodes to reject incompatible protocol versions
 *
 * messageType - Type of message (see MessageType enum)
 *              Determines how to interpret the payload that follows this header
 *
 * sourceId   - Original sender's node ID (never changes during forwarding)
 *              This is the node that created the message
 *
 * destId     - Final destination node ID (never changes during forwarding)
 *              Can be a specific node ID, ADDR_BROADCAST, or ADDR_GATEWAY
 *
 * senderId   - Immediate sender's node ID (changes each hop)
 *              This is the last node that transmitted this packet
 *              Used for tracking the forwarding path and ACK routing
 *
 * messageId  - Sequence number from the original source
 *              Combined with sourceId to create unique packet identifier
 *              Used for duplicate detection and ACK matching
 *
 * ttl        - Time To Live / hop count remaining
 *              Decremented by each forwarding node; packet dropped when 0
 *              Prevents infinite routing loops
 *
 * flags      - Bit flags for message options (see FLAG_* definitions)
 *              bit 0: FLAG_NEEDS_ACK - sender requests acknowledgment
 *              bit 1: FLAG_IS_FORWARDED - packet has been relayed
 *              bits 2-7: reserved for future use
 *
 * Routing Logic:
 * --------------
 * - When creating a message: set sourceId and senderId to own ID
 * - When forwarding: keep sourceId/destId/messageId unchanged, update senderId to own ID
 * - When receiving: check destId to see if message is for you or needs forwarding
 */
struct MeshHeader {
    uint8_t version;      // Protocol version (MESH_PROTOCOL_VERSION)
    uint8_t messageType;  // Message type (MessageType enum)
    uint8_t sourceId;     // Original sender node ID
    uint8_t destId;       // Final destination node ID
    uint8_t senderId;     // Immediate sender node ID (current hop)
    uint8_t messageId;    // Sequence number for duplicate detection
    uint8_t ttl;          // Time to live (hop limit)
    uint8_t flags;        // Message flags (see FLAG_* definitions)
} __attribute__((packed));  // Ensure no padding - exactly 8 bytes

// Compile-time assertion to verify header size
static_assert(sizeof(MeshHeader) == 8, "MeshHeader must be exactly 8 bytes");

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         BEACON MESSAGE STRUCTURE                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * BeaconMsg - Gradient Routing Beacon with Time Synchronization
 *
 * Gateway periodically broadcasts beacons to establish routing gradients.
 * Nodes use beacons to learn their distance (hop count) to the gateway
 * and determine the best next-hop for forwarding data.
 *
 * NEW: Beacons now include GPS timestamp for network time synchronization.
 * Nodes without GPS lock can use this time to participate in TDMA scheduling.
 *
 * Total size: 8 bytes (MeshHeader) + 8 bytes (payload) = 16 bytes
 *
 * Beacon Propagation:
 * -------------------
 * 1. Gateway sends beacon with distance=0 and current GPS time
 * 2. Nodes receive beacon, calculate distance = received_distance + 1
 * 3. Nodes store best route (lowest distance, or best RSSI as tiebreaker)
 * 4. Nodes WITHOUT GPS lock extract time for TDMA scheduling
 * 5. Nodes rebroadcast beacon with incremented distance after random delay
 * 6. Process repeats until all reachable nodes have routes
 *
 * Route Selection:
 * ----------------
 * - Primary: Choose lowest hop count to gateway
 * - Tiebreaker: If equal hop count, choose strongest RSSI
 * - Expiration: Routes expire after ROUTE_TIMEOUT_MS (default 60s)
 *
 * Time Sync Priority:
 * -------------------
 * 1. Own GPS time (most accurate, ~1μs)
 * 2. Network time from beacon (fallback, ~200-500ms accuracy)
 * 3. No transmission (if neither available)
 */
struct BeaconMsg {
    // Mesh routing header (8 bytes)
    MeshHeader meshHeader;          // Standard routing header

    // Beacon payload - routing info (4 bytes)
    uint8_t  distanceToGateway;     // Hop count from sender to gateway
    uint8_t  gatewayId;             // Which gateway this beacon came from
    uint16_t sequenceNumber;        // Beacon sequence (for freshness check)

    // Beacon payload - time sync info (4 bytes) - NEW
    uint8_t  gpsHour;               // Gateway's GPS hour (0-23)
    uint8_t  gpsMinute;             // Gateway's GPS minute (0-59)
    uint8_t  gpsSecond;             // Gateway's GPS second (0-59)
    uint8_t  gpsValid;              // 1 = GPS time valid, 0 = invalid
} __attribute__((packed));

// Compile-time assertion to verify beacon size
static_assert(sizeof(BeaconMsg) == 16, "BeaconMsg must be exactly 16 bytes");

#endif // MESH_PROTOCOL_H
