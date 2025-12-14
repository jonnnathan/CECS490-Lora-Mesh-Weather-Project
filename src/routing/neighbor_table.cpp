#include "neighbor_table.h"
#include "mesh_debug.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL INSTANCE                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

NeighborTable neighborTable;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NEIGHBOR TABLE IMPLEMENTATION                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

NeighborTable::NeighborTable() : count(0) {
    // Initialize all entries to inactive
    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        neighbors[i].isActive = false;
    }
}

void NeighborTable::update(uint8_t nodeId, int16_t rssi) {
    // Don't track nodeId 0 (reserved for gateway/broadcast)
    if (nodeId == 0) return;

    unsigned long now = millis();

    // First, try to find existing entry
    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors[i].isActive && neighbors[i].nodeId == nodeId) {
            // Update existing neighbor
            neighbors[i].rssi = rssi;
            neighbors[i].lastHeardMs = now;
            neighbors[i].packetsReceived++;

            // Update min/max RSSI
            if (rssi < neighbors[i].rssiMin) {
                neighbors[i].rssiMin = rssi;
            }
            if (rssi > neighbors[i].rssiMax) {
                neighbors[i].rssiMax = rssi;
            }

            debugLogNeighborUpdate(nodeId, rssi, neighbors[i].packetsReceived);
            return;
        }
    }

    // Not found - try to add new entry
    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        if (!neighbors[i].isActive) {
            // Found empty slot - create new entry
            neighbors[i].nodeId = nodeId;
            neighbors[i].rssi = rssi;
            neighbors[i].rssiMin = rssi;
            neighbors[i].rssiMax = rssi;
            neighbors[i].lastHeardMs = now;
            neighbors[i].packetsReceived = 1;
            neighbors[i].isActive = true;
            count++;

            DEBUG_NBR_F("NEW neighbor: Node %d rssi=%d [slot %d/%d]",
                       nodeId, rssi, count, MAX_NEIGHBORS);
            return;
        }
    }

    // Table is full - could implement LRU replacement here if needed
    // For now, just ignore the new neighbor
}

Neighbor* NeighborTable::get(uint8_t nodeId) {
    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors[i].isActive && neighbors[i].nodeId == nodeId) {
            return &neighbors[i];
        }
    }
    return nullptr;
}

uint8_t NeighborTable::pruneExpired(uint32_t timeoutMs) {
    unsigned long now = millis();
    uint8_t pruned = 0;

    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        if (neighbors[i].isActive) {
            // Check if neighbor has expired
            if (now - neighbors[i].lastHeardMs > timeoutMs) {
                neighbors[i].isActive = false;
                count--;
                pruned++;
            }
        }
    }

    return pruned;
}

uint8_t NeighborTable::getActiveCount() const {
    return count;
}

uint8_t NeighborTable::getActiveNeighbors(Neighbor** outNeighbors, uint8_t maxCount) {
    uint8_t found = 0;

    for (uint8_t i = 0; i < MAX_NEIGHBORS && found < maxCount; i++) {
        if (neighbors[i].isActive) {
            outNeighbors[found++] = &neighbors[i];
        }
    }

    return found;
}

void NeighborTable::clear() {
    for (uint8_t i = 0; i < MAX_NEIGHBORS; i++) {
        neighbors[i].isActive = false;
    }
    count = 0;
}

int16_t NeighborTable::getAverageRSSI(uint8_t nodeId) {
    Neighbor* n = get(nodeId);
    if (n == nullptr) {
        return -120;  // Return weakest possible signal if not found
    }

    // Return average of min and max observed RSSI
    return (n->rssiMin + n->rssiMax) / 2;
}
