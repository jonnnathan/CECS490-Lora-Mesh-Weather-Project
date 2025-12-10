#ifndef MESH_STATS_H
#define MESH_STATS_H

#include <Arduino.h>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MESH STATISTICS STRUCTURE                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

struct MeshStats {
    // Reception statistics
    uint32_t packetsReceived;       // Total packets received
    uint32_t duplicatesDropped;     // Packets dropped due to duplicate detection

    // Transmission statistics
    uint32_t packetsSent;           // Our own packets transmitted
    uint32_t packetsForwarded;      // Packets forwarded for other nodes

    // Error/drop statistics
    uint32_t ttlExpired;            // Packets not forwarded due to TTL <= 1
    uint32_t queueOverflows;        // Packets dropped due to full queue
    uint32_t ownPacketsIgnored;     // Own packets not forwarded (expected)
    uint32_t gatewayBroadcastSkips; // Gateway broadcast forwards skipped (expected)

    // Uptime
    uint32_t uptimeSeconds;         // Total system uptime in seconds
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         FUNCTION DECLARATIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Initialize mesh statistics
void initMeshStats();

// Reset all statistics to zero
void resetMeshStats();

// Get current statistics
MeshStats getMeshStats();

// Increment counters
void incrementPacketsReceived();
void incrementPacketsSent();
void incrementPacketsForwarded();
void incrementDuplicatesDropped();
void incrementTTLExpired();
void incrementQueueOverflows();
void incrementOwnPacketsIgnored();
void incrementGatewayBroadcastSkips();

// Update uptime
void updateMeshStatsUptime();

// Print statistics to serial
void printMeshStats();

// Get formatted statistics string
String getMeshStatsString();

#endif // MESH_STATS_H
