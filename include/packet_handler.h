#ifndef PACKET_HANDLER_H
#define PACKET_HANDLER_H

#include <Arduino.h>
#include "lora_comm.h"
#include "mesh_protocol.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         FUNCTION DECLARATIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void initPacketHandler();
void checkForIncomingMessages();

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         PACKET FORWARDING                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool shouldForward(MeshHeader* header);
void scheduleForward(uint8_t* data, uint8_t len, MeshHeader* header);

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STATISTICS ACCESS                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

unsigned long getRxCount();
unsigned long getDuplicateCount();
unsigned long getValidRxCount();
unsigned long getDuplicatesDroppedCount();
unsigned long getPacketsForwardedCount();

#endif // PACKET_HANDLER_H