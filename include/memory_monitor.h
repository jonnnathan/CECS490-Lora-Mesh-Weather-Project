#ifndef MEMORY_MONITOR_H
#define MEMORY_MONITOR_H

#include <Arduino.h>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MEMORY MONITORING                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Memory warning thresholds
#define MEMORY_WARNING_THRESHOLD_BYTES   10240  // Warn if free heap < 10KB
#define MEMORY_CRITICAL_THRESHOLD_BYTES  5120   // Critical if free heap < 5KB

// ─────────────────────────────────────────────────────────────────────────────
// Structure to hold memory usage statistics
// ─────────────────────────────────────────────────────────────────────────────
struct MemoryStats {
    uint32_t freeHeap;              // Current free heap in bytes
    uint32_t minFreeHeap;           // Minimum free heap since boot
    uint32_t neighborTableBytes;    // Estimated memory used by neighbor table
    uint32_t duplicateCacheBytes;   // Estimated memory used by duplicate cache
    uint32_t transmitQueueBytes;    // Estimated memory used by transmit queue
    uint32_t nodeStoreBytes;        // Estimated memory used by node store
    uint32_t totalMeshBytes;        // Total mesh subsystem memory
    float usagePercent;             // Percentage of total heap used
};

// ─────────────────────────────────────────────────────────────────────────────
// Function declarations
// ─────────────────────────────────────────────────────────────────────────────

// Get current memory statistics
MemoryStats getMemoryStats();

// Print formatted memory report to serial
void printMemoryReport();

// Check if memory is low and print warning if needed
// Returns true if memory is critically low
bool checkMemoryHealth();

// Initialize memory monitoring (call once at startup)
void initMemoryMonitor();

// Periodic memory check (call from maintenance routine)
void updateMemoryStats();

#endif // MEMORY_MONITOR_H
