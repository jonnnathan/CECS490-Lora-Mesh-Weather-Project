#ifndef NODE_STORE_H
#define NODE_STORE_H

#include <Arduino.h>
#include "config.h"
#include "lora_comm.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NODE MESSAGE STRUCTURE                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

struct NodeMessage {
    bool hasData;
    bool isOnline;
    String payload;
    uint8_t originId;
    uint16_t lastSeq;
    uint16_t expectedNextSeq;
    float lastRssi;
    float lastSnr;
    unsigned long lastHeardTime;
    unsigned long messageCount;
    unsigned long packetsLost;
    FullReportMsg lastReport;
    NodeMessage();
    void clear();
    bool hasTimedOut(unsigned long timeoutMs) const;
    unsigned long getAgeSeconds() const;
    float getPacketLossPercent() const;

    // Update from mesh packet - uses mesh messageId for gap detection
    // Returns number of packets lost (gap size), 0 if no gap
    uint16_t updateFromMeshPacket(const LoRaReceivedPacket& packet, uint8_t meshMessageId);

    // Update from legacy packet - uses LoRa seq for gap detection
    // Returns number of packets lost (gap size), 0 if no gap
    uint16_t updateFromPacket(const LoRaReceivedPacket& packet);
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL NODE STORE                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

extern NodeMessage nodeStore[MESH_MAX_NODES];

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         FUNCTION DECLARATIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void initNodeStore();
NodeMessage* getNodeMessage(uint8_t nodeId);
void checkNodeTimeouts();
String getNodeStatusIcon(uint8_t nodeId);
uint8_t getNodeCount();

#endif // NODE_STORE_H