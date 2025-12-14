#include "packet_handler.h"
#include "config.h"
#include "node_store.h"
#include "serial_output.h"
#include "serial_json.h"
#include "display_manager.h"
#include "thingspeak.h"
#include "neighbor_table.h"
#include "duplicate_cache.h"
#include "transmit_queue.h"
#include "mesh_protocol.h"
#include "mesh_stats.h"
#include "mesh_debug.h"
#include "network_topology.h"
#include "gradient_routing.h"
#include "network_time.h"
#include "Logger.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STATISTICS                                        ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static unsigned long rxCount = 0;
static unsigned long duplicateRxMessages = 0;
static unsigned long validRxMessages = 0;
static unsigned long duplicatesDropped = 0;  // Mesh-level duplicates (from duplicate cache)
static unsigned long packetsForwarded = 0;   // Packets scheduled for forwarding

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         LAST RECEIVED REPORT                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static FullReportMsg lastReceivedReport;
static bool lastReportValid = false;
static uint8_t lastReportOrigin = 0;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         INITIALIZATION                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void initPacketHandler() {
    duplicateCache.clear();
    rxCount = 0;
    duplicateRxMessages = 0;
    validRxMessages = 0;
    duplicatesDropped = 0;
    packetsForwarded = 0;
    lastReportValid = false;
    lastReportOrigin = 0;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         PACKET FORWARDING                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool shouldForward(MeshHeader* header) {
    // Don't forward if TTL is too low (1 or less)
    if (header->ttl <= 1) {
        incrementTTLExpired();
        debugLogForwardDecision(false, "TTL <= 1", header);
        return false;
    }

    // Don't forward own messages
    if (header->sourceId == DEVICE_ID) {
        incrementOwnPacketsIgnored();
        debugLogForwardDecision(false, "Own packet", header);
        return false;
    }

    // Gateway doesn't forward broadcasts (to prevent loops)
    if (IS_GATEWAY && header->destId == ADDR_BROADCAST) {
        incrementGatewayBroadcastSkips();
        debugLogForwardDecision(false, "Gateway broadcast loop prevention", header);
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // GRADIENT ROUTING FILTER
    // Only forward if we're "closer" to gateway than the sender
    // This prevents unnecessary rebroadcasts from nodes further from gateway
    // ═══════════════════════════════════════════════════════════════════════════
    if (hasValidRoute()) {
        // Gateway always accepts packets (it's the destination)
        if (IS_GATEWAY) {
            debugLogForwardDecision(true, "Gateway receiving packet", header);
            return true;
        }

        // Check if sender included hop count info (for intelligent filtering)
        // The senderId tells us who we heard this from directly
        // If the immediate sender is our next-hop toward gateway, don't forward
        // (that would send it backwards away from gateway)
        uint8_t nextHop = getNextHop();
        if (header->senderId == nextHop) {
            // Packet came FROM our route toward gateway - don't send it back
            debugLogForwardDecision(false, "Packet from next-hop (wrong direction)", header);
            return false;
        }

        // Forward toward gateway (we're a valid relay)
        debugLogForwardDecision(true, "Gradient relay toward gateway", header);
        return true;
    }

    // No valid route - fall back to flooding behavior (forward everything)
    debugLogForwardDecision(true, "Flooding fallback (no route)", header);
    return true;
}

void scheduleForward(uint8_t* data, uint8_t len, MeshHeader* header) {
    // Create a copy of the packet for modification
    uint8_t forwardBuffer[64];

    // Validate length
    if (len == 0 || len > 64) {
        LOG_WARN_F("Forward failed: invalid packet length (%d)", len);
        return;
    }

    // Copy packet data to local buffer
    memcpy(forwardBuffer, data, len);

    // Get pointer to MeshHeader in the copied buffer
    MeshHeader* forwardHeader = (MeshHeader*)forwardBuffer;

    // Decrement TTL
    forwardHeader->ttl--;

    // Update senderId to our node ID (we're now the sender)
    forwardHeader->senderId = DEVICE_ID;

    // Set forwarded flag
    forwardHeader->flags |= FLAG_IS_FORWARDED;

    // ═══════════════════════════════════════════════════════════════════════
    // GRADIENT ROUTING DECISION
    // ═══════════════════════════════════════════════════════════════════════
    bool useGradientRouting = hasValidRoute();
    uint8_t nextHop = 0;

    if (useGradientRouting) {
        nextHop = getNextHop();
        incrementUnicastForwards();

        Serial.println(F(""));
        Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
        Serial.println(F("║           GRADIENT ROUTING FORWARD                        ║"));
        Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));
        Serial.print(F("  Next Hop: Node "));
        Serial.print(nextHop);
        Serial.print(F("  |  Distance: "));
        Serial.print(getDistanceToGateway());
        Serial.println(F(" hops to gateway"));
    } else {
        incrementFloodingFallbacks();

        Serial.println(F(""));
        Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
        Serial.println(F("║           FLOODING FALLBACK (No Route)                    ║"));
        Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));
        Serial.println(F("  No valid gradient route - using flooding"));
    }

    // Try to enqueue for transmission
    // Note: LoRa is broadcast, but gradient routing means only the intended
    // next-hop should continue forwarding toward the gateway
    if (transmitQueue.enqueue(forwardBuffer, len)) {
        // Success - log the forward action
        debugLogQueueOp("Enqueue success", transmitQueue.depth(), TX_QUEUE_SIZE);

        Serial.print(F("  Source: Node "));
        Serial.print(forwardHeader->sourceId);
        Serial.print(F("  |  MsgID: "));
        Serial.print(forwardHeader->messageId);
        Serial.print(F("  |  TTL: "));
        Serial.println(forwardHeader->ttl);
        Serial.print(F("  Mode: "));
        Serial.print(useGradientRouting ? F("GRADIENT") : F("FLOODING"));
        Serial.print(F("  |  Queue: "));
        Serial.println(transmitQueue.depth());
        Serial.println(F("─────────────────────────────────────────────────────────────"));
    } else {
        // Queue full - log warning and increment overflow counter
        incrementQueueOverflows();
        debugLogQueueOp("Enqueue FAILED - queue full", transmitQueue.depth(), TX_QUEUE_SIZE);
        LOG_WARN_F("Forward queue FULL - dropped: src=%d msgId=%d",
                   forwardHeader->sourceId, forwardHeader->messageId);
        Serial.println(F("─────────────────────────────────────────────────────────────"));
    }
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         PACKET PROCESSING                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void checkForIncomingMessages() {
    LoRaReceivedPacket packet;

    while (receivePacket(packet)) {
        // Update statistics
        rxCount++;

        // Get message type from raw bytes
        MessageType msgType = getMessageType(packet.payloadBytes, packet.payloadLen);

        // ═══════════════════════════════════════════════════════════════════════
        // BEACON MESSAGE HANDLING (Gradient Routing)
        // ═══════════════════════════════════════════════════════════════════════
        if (msgType == MSG_BEACON) {
            BeaconMsg beacon;
            if (decodeBeacon(packet.payloadBytes, packet.payloadLen, beacon)) {
                // Skip our own beacons (radio loopback)
                if (beacon.meshHeader.sourceId == DEVICE_ID) {
                    continue;
                }

                // Log beacon reception
                Serial.println(F(""));
                Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
                Serial.println(F("║               BEACON RECEIVED                             ║"));
                Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));
                Serial.print(F("  From Node: "));
                Serial.print(beacon.meshHeader.senderId);
                Serial.print(F("  |  Distance: "));
                Serial.print(beacon.distanceToGateway);
                Serial.print(F(" hops  |  TTL: "));
                Serial.println(beacon.meshHeader.ttl);
                Serial.print(F("  Gateway: "));
                Serial.print(beacon.gatewayId);
                Serial.print(F("  |  Seq: "));
                Serial.print(beacon.sequenceNumber);
                Serial.print(F("  |  RSSI: "));
                Serial.print(packet.rssi);
                Serial.println(F(" dBm"));
                Serial.println(F("─────────────────────────────────────────────────────────────"));

                // Update routing state with beacon info
                updateRoutingState(
                    beacon.distanceToGateway,
                    beacon.meshHeader.senderId,
                    beacon.gatewayId,
                    beacon.sequenceNumber,
                    (int16_t)packet.rssi
                );

                // ─────────────────────────────────────────────────────────────────
                // NETWORK TIME SYNC: Extract GPS time from beacon
                // Hop count = distanceToGateway + 1 (gateway is distance 0, so
                // receiving from gateway = 1 hop from GPS source)
                // ─────────────────────────────────────────────────────────────────
                if (beacon.gpsValid) {
                    uint8_t timeHopCount = beacon.distanceToGateway + 1;
                    updateNetworkTime(beacon.gpsHour, beacon.gpsMinute,
                                     beacon.gpsSecond, beacon.meshHeader.senderId,
                                     timeHopCount);
                    Serial.print(F("  Time Sync: "));
                    Serial.print(beacon.gpsHour);
                    Serial.print(F(":"));
                    if (beacon.gpsMinute < 10) Serial.print(F("0"));
                    Serial.print(beacon.gpsMinute);
                    Serial.print(F(":"));
                    if (beacon.gpsSecond < 10) Serial.print(F("0"));
                    Serial.print(beacon.gpsSecond);
                    Serial.print(F(" (hop "));
                    Serial.print(timeHopCount);
                    Serial.println(F(")"));
                }

                // Schedule beacon rebroadcast (non-gateway nodes only)
                scheduleBeaconRebroadcast(beacon, (int16_t)packet.rssi);

                // Update neighbor table with beacon sender
                neighborTable.update(beacon.meshHeader.senderId, packet.rssi);
            }
            continue;  // Don't process beacon as data packet
        }

        // ═══════════════════════════════════════════════════════════════════════
        // FULL_REPORT MESSAGE HANDLING
        // ═══════════════════════════════════════════════════════════════════════
        if (msgType == MSG_FULL_REPORT && decodeFullReport(packet.payloadBytes, packet.payloadLen, lastReceivedReport)) {
            // ─────────────────────────────────────────────────────────────────────
            // Skip our own packets (radio loopback prevention)
            // ─────────────────────────────────────────────────────────────────────
            if (lastReceivedReport.meshHeader.sourceId == DEVICE_ID) {
                DEBUG_RX_F("Ignoring own packet | sourceId=%d msgId=%d (radio loopback)",
                          lastReceivedReport.meshHeader.sourceId,
                          lastReceivedReport.meshHeader.messageId);
                continue;
            }

            // Log packet reception details
            debugLogPacketRx(&lastReceivedReport.meshHeader, packet.rssi, packet.snr);

            // ─────────────────────────────────────────────────────────────────────
            // Mesh-level duplicate detection using sourceId + messageId
            // ─────────────────────────────────────────────────────────────────────
            if (duplicateCache.isDuplicate(
                lastReceivedReport.meshHeader.sourceId,
                lastReceivedReport.meshHeader.messageId)) {
                // This is a duplicate from mesh forwarding - skip processing
                duplicatesDropped++;
                incrementDuplicatesDropped();
                debugLogDuplicate(lastReceivedReport.meshHeader.sourceId,
                                 lastReceivedReport.meshHeader.messageId, true);
                LOG_DEBUG_F("Duplicate mesh message from Node %d msg #%d (dropped, total: %lu)",
                            lastReceivedReport.meshHeader.sourceId,
                            lastReceivedReport.meshHeader.messageId,
                            duplicatesDropped);
                continue;
            }

            // Mark as seen for future duplicate detection
            duplicateCache.markSeen(
                lastReceivedReport.meshHeader.sourceId,
                lastReceivedReport.meshHeader.messageId);
            debugLogDuplicate(lastReceivedReport.meshHeader.sourceId,
                             lastReceivedReport.meshHeader.messageId, false);

            // ═══════════════════════════════════════════════════════════════
            // NETWORK TOPOLOGY VISUALIZATION
            // ═══════════════════════════════════════════════════════════════
            // Show visual route of this packet
            printPacketRoute(packet, lastReceivedReport);
            // Add to route history
            addPacketRoute(lastReceivedReport);

            // Update valid message counter
            validRxMessages++;
            incrementPacketsReceived();

            lastReportValid = true;
            // Use sourceId from MeshHeader (original sender, not immediate sender)
            lastReportOrigin = lastReceivedReport.meshHeader.sourceId;

            // Update neighbor table with immediate sender's RSSI (who we heard directly)
            neighborTable.update(lastReceivedReport.meshHeader.senderId, packet.rssi);

            // Update node store for the ORIGINAL SOURCE (not the forwarder)
            NodeMessage* node = getNodeMessage(lastReceivedReport.meshHeader.sourceId);
            uint16_t gap = 0;
            unsigned long msgCount = 0;
            unsigned long lost = 0;
            float lossPercent = 0.0;

            if (node != nullptr) {
                // Update packet tracking stats (only for the original source)
                // Use mesh messageId for gap detection (not LoRa seq)
                gap = node->updateFromMeshPacket(packet, lastReceivedReport.meshHeader.messageId);
                msgCount = node->messageCount;
                lost = node->packetsLost;
                lossPercent = node->getPacketLossPercent();

                // Store the decoded report
                node->lastReport = lastReceivedReport;
            }

            // Print fancy FULL_REPORT output
            printRxFullReport(packet, lastReceivedReport, gap, msgCount, lost, lossPercent);

            // Update display with decoded data
            updateRxDisplayFullReport(packet, lastReceivedReport);
            // Send to ThingSpeak cloud (gateway only)
            if (IS_GATEWAY) {
                sendToThingSpeak(lastReceivedReport.meshHeader.sourceId, lastReceivedReport, packet.rssi);
            }

            // Output JSON for desktop dashboard (serial bridge)
            outputNodeDataJson(lastReceivedReport.meshHeader.sourceId, lastReceivedReport, packet.rssi, packet.snr);

            // ─────────────────────────────────────────────────────────────────────
            // Check if packet should be forwarded to other nodes
            // ─────────────────────────────────────────────────────────────────────
            if (shouldForward(&lastReceivedReport.meshHeader)) {
                scheduleForward(packet.payloadBytes, packet.payloadLen, &lastReceivedReport.meshHeader);
                packetsForwarded++;
            }
        } else {
            // Legacy string message or decode failure
            duplicateRxMessages++;  // Use old counter for non-mesh messages
            lastReportValid = false;

            // For legacy messages, use LoRa header originId to track stats
            NodeMessage* legacyNode = getNodeMessage(packet.header.originId);
            uint16_t gap = 0;
            unsigned long msgCount = 0;
            unsigned long lost = 0;
            float lossPercent = 0.0;

            if (legacyNode) {
                gap = legacyNode->updateFromPacket(packet);
                msgCount = legacyNode->messageCount;
                lost = legacyNode->packetsLost;
                lossPercent = legacyNode->getPacketLossPercent();
            }

            printRxPacket(packet, gap, msgCount, lost, lossPercent);
            updateRxDisplay(packet);
        }
    }
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STATISTICS ACCESS                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

unsigned long getRxCount() {
    return rxCount;
}

unsigned long getDuplicateCount() {
    return duplicateRxMessages;
}

unsigned long getValidRxCount() {
    return validRxMessages;
}

unsigned long getDuplicatesDroppedCount() {
    return duplicatesDropped;
}

unsigned long getPacketsForwardedCount() {
    return packetsForwarded;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         LAST REPORT ACCESS                                ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool getLastReceivedReport(FullReportMsg& report, uint8_t& originId) {
    if (!lastReportValid) return false;
    report = lastReceivedReport;
    originId = lastReportOrigin;
    return true;
}