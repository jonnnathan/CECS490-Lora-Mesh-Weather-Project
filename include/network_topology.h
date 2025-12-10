/*
 * network_topology.h
 * Network Topology Visualization for Mesh Network
 *
 * Shows packet routing paths:
 * - Origin Node (where packet started)
 * - Relay Nodes (intermediate hops)
 * - Destination (this gateway)
 */

#ifndef NETWORK_TOPOLOGY_H
#define NETWORK_TOPOLOGY_H

#include <Arduino.h>
#include "lora_comm.h"
#include "mesh_protocol.h"

// Structure to track a packet route
struct PacketRoute {
    uint8_t originId;       // Source node
    uint8_t senderId;       // Last hop (who sent it to us)
    uint8_t receiverId;     // Us (the gateway)
    uint8_t ttl;            // TTL remaining
    uint8_t hops;           // Number of hops taken (8 - TTL)
    uint16_t messageId;     // Unique message ID
    unsigned long timestamp; // When received
    bool isValid;
};

// Print a visual representation of the packet route
void printPacketRoute(const LoRaReceivedPacket& packet, const FullReportMsg& report);

// Print network topology summary
void printNetworkTopology();

// Track packet route history (last N packets)
void addPacketRoute(const FullReportMsg& report);

#endif // NETWORK_TOPOLOGY_H
