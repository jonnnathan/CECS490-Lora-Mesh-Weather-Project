#include "memory_monitor.h"
#include "neighbor_table.h"
#include "duplicate_cache.h"
#include "transmit_queue.h"
#include "node_store.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MEMORY TRACKING                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static uint32_t minFreeHeapEverSeen = 0xFFFFFFFF;
static unsigned long lastMemoryCheck = 0;
static const unsigned long MEMORY_CHECK_INTERVAL_MS = 60000;  // Check every 60 seconds

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         INITIALIZATION                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void initMemoryMonitor() {
    minFreeHeapEverSeen = ESP.getFreeHeap();
    lastMemoryCheck = millis();

    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  MEMORY MONITOR INITIALIZED                                   ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
    Serial.print(F("Initial free heap: "));
    Serial.print(minFreeHeapEverSeen);
    Serial.println(F(" bytes"));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MEMORY ESTIMATION                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Estimate memory used by neighbor table
uint32_t estimateNeighborTableMemory() {
    // Each neighbor entry: nodeId (1) + rssi (2) + minRssi (2) + maxRssi (2) +
    //                      packetCount (4) + lastSeen (4) = 15 bytes
    // Plus array overhead and structure padding
    const uint32_t bytesPerEntry = 16;  // Rounded up for alignment

    uint8_t activeNeighbors = neighborTable.getActiveCount();
    uint32_t baseSize = sizeof(neighborTable);  // Static structure size
    uint32_t dynamicSize = activeNeighbors * bytesPerEntry;

    return baseSize + dynamicSize;
}

// Estimate memory used by duplicate cache
uint32_t estimateDuplicateCacheMemory() {
    // Each cache entry: sourceId (1) + messageId (2) + timestamp (4) = 7 bytes
    // Plus array overhead
    const uint32_t bytesPerEntry = 8;  // Rounded up for alignment

    uint8_t entryCount = duplicateCache.getCount();
    uint32_t baseSize = sizeof(duplicateCache);
    uint32_t dynamicSize = entryCount * bytesPerEntry;

    return baseSize + dynamicSize;
}

// Estimate memory used by transmit queue
uint32_t estimateTransmitQueueMemory() {
    // Each queue entry: length (1) + data[64] = 65 bytes
    // Plus ring buffer overhead
    const uint32_t bytesPerEntry = 68;  // Rounded up for alignment

    uint32_t baseSize = sizeof(transmitQueue);
    uint32_t capacity = TX_QUEUE_SIZE;  // From transmit_queue.h
    uint32_t dynamicSize = capacity * bytesPerEntry;

    return baseSize + dynamicSize;
}

// Estimate memory used by node store
uint32_t estimateNodeStoreMemory() {
    // Each NodeMessage: ~150 bytes (includes FullReportMsg + tracking data)
    const uint32_t bytesPerNode = 150;

    uint8_t nodeCount = getNodeCount();
    uint32_t baseSize = 64;  // Node store array overhead
    uint32_t dynamicSize = nodeCount * bytesPerNode;

    return baseSize + dynamicSize;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MEMORY STATISTICS                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

MemoryStats getMemoryStats() {
    MemoryStats stats;

    // Get current heap status
    stats.freeHeap = ESP.getFreeHeap();
    stats.minFreeHeap = ESP.getMinFreeHeap();

    // Update our running minimum
    if (stats.freeHeap < minFreeHeapEverSeen) {
        minFreeHeapEverSeen = stats.freeHeap;
    }

    // Use the more conservative minimum
    if (stats.minFreeHeap < minFreeHeapEverSeen) {
        stats.minFreeHeap = stats.minFreeHeap;
    } else {
        stats.minFreeHeap = minFreeHeapEverSeen;
    }

    // Estimate mesh subsystem memory usage
    stats.neighborTableBytes = estimateNeighborTableMemory();
    stats.duplicateCacheBytes = estimateDuplicateCacheMemory();
    stats.transmitQueueBytes = estimateTransmitQueueMemory();
    stats.nodeStoreBytes = estimateNodeStoreMemory();

    stats.totalMeshBytes = stats.neighborTableBytes +
                          stats.duplicateCacheBytes +
                          stats.transmitQueueBytes +
                          stats.nodeStoreBytes;

    // Calculate usage percentage (ESP32 typically has ~300KB total heap)
    uint32_t totalHeap = stats.freeHeap + stats.totalMeshBytes + 50000;  // Rough estimate
    stats.usagePercent = ((float)(totalHeap - stats.freeHeap) / totalHeap) * 100.0f;

    return stats;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MEMORY REPORTING                                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printMemoryReport() {
    MemoryStats stats = getMemoryStats();

    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║                    MEMORY STATUS REPORT                       ║"));
    Serial.println(F("╠═══════════════════════════════════════════════════════════════╣"));

    // Heap status
    Serial.println(F("║  HEAP STATUS:                                                 ║"));
    Serial.print(F("║    Free Heap:          "));
    Serial.print(stats.freeHeap);
    Serial.print(F(" bytes ("));
    Serial.print(stats.freeHeap / 1024.0f, 2);
    Serial.println(F(" KB)"));

    Serial.print(F("║    Min Free Heap:      "));
    Serial.print(stats.minFreeHeap);
    Serial.print(F(" bytes ("));
    Serial.print(stats.minFreeHeap / 1024.0f, 2);
    Serial.println(F(" KB)"));

    Serial.println(F("║                                                               ║"));

    // Mesh subsystem memory
    Serial.println(F("║  MESH SUBSYSTEM MEMORY:                                       ║"));
    Serial.print(F("║    Neighbor Table:     "));
    Serial.print(stats.neighborTableBytes);
    Serial.println(F(" bytes"));

    Serial.print(F("║    Duplicate Cache:    "));
    Serial.print(stats.duplicateCacheBytes);
    Serial.println(F(" bytes"));

    Serial.print(F("║    Transmit Queue:     "));
    Serial.print(stats.transmitQueueBytes);
    Serial.println(F(" bytes"));

    Serial.print(F("║    Node Store:         "));
    Serial.print(stats.nodeStoreBytes);
    Serial.println(F(" bytes"));

    Serial.print(F("║    Total Mesh:         "));
    Serial.print(stats.totalMeshBytes);
    Serial.print(F(" bytes ("));
    Serial.print(stats.totalMeshBytes / 1024.0f, 2);
    Serial.println(F(" KB)"));

    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MEMORY HEALTH MONITORING                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool checkMemoryHealth() {
    MemoryStats stats = getMemoryStats();

    // Check for critical memory condition
    if (stats.freeHeap < MEMORY_CRITICAL_THRESHOLD_BYTES) {
        Serial.println(F(""));
        Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
        Serial.println(F("║  ⚠️  CRITICAL MEMORY WARNING ⚠️                               ║"));
        Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
        Serial.print(F("🚨 Free heap critically low: "));
        Serial.print(stats.freeHeap);
        Serial.print(F(" bytes ("));
        Serial.print(stats.freeHeap / 1024.0f, 2);
        Serial.println(F(" KB)"));
        Serial.println(F(""));
        Serial.println(F("⚠️  SYSTEM MAY BECOME UNSTABLE!"));
        Serial.println(F("Consider:"));
        Serial.println(F("  - Reducing TX_QUEUE_SIZE"));
        Serial.println(F("  - Reducing DUPLICATE_CACHE_SIZE"));
        Serial.println(F("  - Reducing MAX_NEIGHBORS"));
        Serial.println(F("  - Reducing MAX_NODES"));
        Serial.println(F(""));

        return true;  // Critical condition
    }

    // Check for warning condition
    if (stats.freeHeap < MEMORY_WARNING_THRESHOLD_BYTES) {
        Serial.println(F(""));
        Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
        Serial.println(F("║  ⚠️  MEMORY WARNING                                           ║"));
        Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
        Serial.print(F("⚠️  Free heap running low: "));
        Serial.print(stats.freeHeap);
        Serial.print(F(" bytes ("));
        Serial.print(stats.freeHeap / 1024.0f, 2);
        Serial.println(F(" KB)"));
        Serial.print(F("Mesh subsystems using: "));
        Serial.print(stats.totalMeshBytes);
        Serial.println(F(" bytes"));
        Serial.println(F(""));

        return false;  // Warning but not critical
    }

    // Memory is healthy
    return false;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         PERIODIC MEMORY UPDATE                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void updateMemoryStats() {
    unsigned long now = millis();

    // Only check periodically
    if (now - lastMemoryCheck < MEMORY_CHECK_INTERVAL_MS) {
        return;
    }

    lastMemoryCheck = now;

    // Update minimum free heap tracking
    uint32_t currentFree = ESP.getFreeHeap();
    if (currentFree < minFreeHeapEverSeen) {
        minFreeHeapEverSeen = currentFree;
    }

    // Check for low memory conditions
    checkMemoryHealth();
}
