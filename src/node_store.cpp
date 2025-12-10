#include "node_store.h"
#include "serial_output.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL NODE STORE                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

NodeMessage nodeStore[MESH_MAX_NODES];

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NODE MESSAGE METHODS                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

NodeMessage::NodeMessage() {
    clear();
}

void NodeMessage::clear() {
    hasData = false;
    isOnline = false;
    payload = "";
    originId = 0;
    lastSeq = 0;
    expectedNextSeq = 0;
    lastRssi = 0.0;
    lastSnr = 0.0;
    lastHeardTime = 0;
    messageCount = 0;
    packetsLost = 0;
}

bool NodeMessage::hasTimedOut(unsigned long timeoutMs) const {
    if (!hasData) return true;
    return (millis() - lastHeardTime) > timeoutMs;
}

unsigned long NodeMessage::getAgeSeconds() const {
    if (!hasData) return 0;
    return (millis() - lastHeardTime) / 1000;
}

float NodeMessage::getPacketLossPercent() const {
    if (messageCount == 0) return 0.0;
    unsigned long totalExpected = messageCount + packetsLost;
    return (float)packetsLost / totalExpected * 100.0;
}

uint16_t NodeMessage::updateFromMeshPacket(const LoRaReceivedPacket& packet, uint8_t meshMessageId) {
    uint16_t gap = 0;

    // Use mesh messageId for gap detection (8-bit, wraps at 255)
    if (hasData && meshMessageId != (uint8_t)expectedNextSeq) {
        // Calculate gap (handle sequence wraparound at 255)
        if (meshMessageId > (uint8_t)expectedNextSeq) {
            gap = meshMessageId - (uint8_t)expectedNextSeq;
        } else if (meshMessageId < (uint8_t)lastSeq) {
            // Sequence wrapped around (8-bit max = 255)
            gap = (255 - (uint8_t)expectedNextSeq) + meshMessageId + 1;
        }

        // Sanity check - ignore huge gaps (likely node restart)
        // Reduced threshold for 8-bit sequence numbers
        if (gap > 0 && gap < 100) {
            packetsLost += gap;
        }
    }

    hasData = true;
    isOnline = true;
    payload = packet.payload;
    originId = packet.header.originId;
    lastSeq = meshMessageId;              // Store mesh message ID
    expectedNextSeq = meshMessageId + 1;  // Expect next mesh message ID
    lastRssi = packet.rssi;
    lastSnr = packet.snr;
    lastHeardTime = millis();
    messageCount++;

    return gap;
}

uint16_t NodeMessage::updateFromPacket(const LoRaReceivedPacket& packet) {
    // Legacy version - uses LoRa header seq (for non-mesh messages)
    uint16_t gap = 0;

    if (hasData && packet.header.seq != expectedNextSeq) {
        // Calculate gap (handle sequence wraparound)
        if (packet.header.seq > expectedNextSeq) {
            gap = packet.header.seq - expectedNextSeq;
        } else if (packet.header.seq < lastSeq) {
            // Sequence wrapped around (16-bit max = 65535)
            gap = (65535 - expectedNextSeq) + packet.header.seq + 1;
        }

        // Sanity check - ignore huge gaps (likely node restart)
        if (gap > 0 && gap < 1000) {
            packetsLost += gap;
        }
    }

    hasData = true;
    isOnline = true;
    payload = packet.payload;
    originId = packet.header.originId;
    lastSeq = packet.header.seq;
    expectedNextSeq = packet.header.seq + 1;
    lastRssi = packet.rssi;
    lastSnr = packet.snr;
    lastHeardTime = millis();
    messageCount++;

    return gap;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NODE STORE FUNCTIONS                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void initNodeStore() {
    for (uint8_t i = 0; i < MESH_MAX_NODES; i++) {
        nodeStore[i].clear();
    }
}

NodeMessage* getNodeMessage(uint8_t nodeId) {
    if (nodeId < 1 || nodeId > MESH_MAX_NODES) return nullptr;
    return &nodeStore[nodeId - 1];
}

void checkNodeTimeouts() {
    for (uint8_t i = 0; i < MESH_MAX_NODES; i++) {
        uint8_t nodeId = i + 1;
        if (nodeId == DEVICE_ID) continue;
        
        NodeMessage* node = &nodeStore[i];
        if (node->hasData && node->isOnline && node->hasTimedOut(NODE_TIMEOUT_MS)) {
            printNodeOfflineAlert(nodeId, node->getAgeSeconds());
            node->isOnline = false;
        }
    }
}

String getNodeStatusIcon(uint8_t nodeId) {
    if (nodeId == DEVICE_ID) return "[*]";
    if (nodeId < 1 || nodeId > MESH_MAX_NODES) return "[?]";

    NodeMessage* node = &nodeStore[nodeId - 1];
    if (!node->hasData) return "[ ]";
    if (node->isOnline) return "[O]";
    return "[x]";
}

uint8_t getNodeCount() {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MESH_MAX_NODES; i++) {
        if (nodeStore[i].hasData) {
            count++;
        }
    }
    return count;
}