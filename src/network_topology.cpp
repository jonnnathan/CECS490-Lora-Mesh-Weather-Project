/*
 * network_topology.cpp
 * Network Topology Visualization Implementation
 */

#include "network_topology.h"
#include "config.h"
#include "serial_output.h"

// History of recent packet routes (circular buffer)
#define MAX_ROUTE_HISTORY 10
static PacketRoute routeHistory[MAX_ROUTE_HISTORY];
static uint8_t routeHistoryIndex = 0;
static uint8_t routeHistoryCount = 0;

void printPacketRoute(const LoRaReceivedPacket& packet, const FullReportMsg& report) {
    uint8_t originId = report.meshHeader.sourceId;
    uint8_t senderId = report.meshHeader.senderId;
    uint8_t ttl = report.meshHeader.ttl;
    uint8_t hops = MESH_MAX_HOPS - ttl;

    Serial.println(F(""));
    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║               PACKET ROUTE VISUALIZATION                 ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));

    // Show message ID and TTL
    Serial.print(F("  Message ID: #"));
    Serial.print(report.meshHeader.messageId);
    Serial.print(F(" | TTL: "));
    Serial.print(ttl);
    Serial.print(F("/"));
    Serial.print(MESH_MAX_HOPS);
    Serial.print(F(" | Hops: "));
    Serial.println(hops);
    Serial.println();

    // Visual route display
    if (originId == senderId) {
        // Direct transmission (no relay)
        Serial.println(F("  Route: DIRECT TRANSMISSION"));
        Serial.println();
        Serial.print(F("     [Node "));
        Serial.print(originId);
        Serial.print(F("]  ━━━━━━━>  [Gateway "));
        Serial.print(DEVICE_ID);
        Serial.println(F("]"));
        Serial.println(F("      ORIGIN              DESTINATION"));
    } else {
        // Relayed transmission
        Serial.println(F("  Route: RELAYED TRANSMISSION"));
        Serial.println();
        Serial.print(F("     [Node "));
        Serial.print(originId);
        Serial.print(F("]  ━━>  [Node "));
        Serial.print(senderId);
        Serial.print(F("]  ━━>  [Gateway "));
        Serial.print(DEVICE_ID);
        Serial.println(F("]"));
        Serial.println(F("      ORIGIN       RELAY       DESTINATION"));
        Serial.println();
        Serial.print(F("     └─ Original sender: Node "));
        Serial.println(originId);
        Serial.print(F("     └─ Last hop: Node "));
        Serial.print(senderId);
        Serial.println(F(" (relay)"));
    }

    Serial.println();
    Serial.print(F("  Signal: "));
    Serial.print(packet.rssi, 0);
    Serial.print(F(" dBm | SNR: "));
    Serial.print(packet.snr, 1);
    Serial.println(F(" dB"));
    Serial.println(F("─────────────────────────────────────────────────────────────"));
    Serial.println();
}

void addPacketRoute(const FullReportMsg& report) {
    // Add to circular buffer
    PacketRoute& route = routeHistory[routeHistoryIndex];

    route.originId = report.meshHeader.sourceId;
    route.senderId = report.meshHeader.senderId;
    route.receiverId = DEVICE_ID;
    route.ttl = report.meshHeader.ttl;
    route.hops = MESH_MAX_HOPS - report.meshHeader.ttl;
    route.messageId = report.meshHeader.messageId;
    route.timestamp = millis();
    route.isValid = true;

    routeHistoryIndex = (routeHistoryIndex + 1) % MAX_ROUTE_HISTORY;
    if (routeHistoryCount < MAX_ROUTE_HISTORY) {
        routeHistoryCount++;
    }
}

void printNetworkTopology() {
    Serial.println(F(""));
    Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
    Serial.println(F("║            NETWORK TOPOLOGY - RECENT ROUTES               ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));

    if (routeHistoryCount == 0) {
        Serial.println(F("  No packets received yet."));
        Serial.println();
        return;
    }

    Serial.print(F("  Gateway: Node "));
    Serial.println(DEVICE_ID);
    Serial.print(F("  Recent packets: "));
    Serial.println(routeHistoryCount);
    Serial.println();

    // Count direct vs relayed
    uint8_t directCount = 0;
    uint8_t relayedCount = 0;

    for (uint8_t i = 0; i < routeHistoryCount; i++) {
        const PacketRoute& route = routeHistory[i];
        if (route.isValid) {
            if (route.originId == route.senderId) {
                directCount++;
            } else {
                relayedCount++;
            }
        }
    }

    Serial.print(F("  Direct transmissions: "));
    Serial.println(directCount);
    Serial.print(F("  Relayed transmissions: "));
    Serial.println(relayedCount);
    Serial.println();

    // Show unique routes
    Serial.println(F("  Active Routes:"));
    Serial.println(F("  ──────────────"));

    bool routes[MESH_MAX_NODES + 1][MESH_MAX_NODES + 1] = {false};

    for (uint8_t i = 0; i < routeHistoryCount; i++) {
        const PacketRoute& route = routeHistory[i];
        if (route.isValid && !routes[route.originId][route.senderId]) {
            routes[route.originId][route.senderId] = true;

            Serial.print(F("    "));
            if (route.originId == route.senderId) {
                // Direct
                Serial.print(F("Node "));
                Serial.print(route.originId);
                Serial.print(F(" ━━━> Gateway "));
                Serial.print(DEVICE_ID);
                Serial.println(F("  (direct)"));
            } else {
                // Relayed
                Serial.print(F("Node "));
                Serial.print(route.originId);
                Serial.print(F(" ━━> Node "));
                Serial.print(route.senderId);
                Serial.print(F(" ━━> Gateway "));
                Serial.print(DEVICE_ID);
                Serial.println(F("  (relay)"));
            }
        }
    }

    Serial.println(F("─────────────────────────────────────────────────────────────"));
    Serial.println();
}
