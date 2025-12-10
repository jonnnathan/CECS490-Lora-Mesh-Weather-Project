# Memory Monitoring System

## Overview

The memory monitoring system tracks ESP32 heap usage and provides detailed reports on memory consumption by mesh network subsystems. This helps identify memory leaks, optimize memory usage, and prevent system instability due to low memory conditions.

## Features

1. **Real-time Heap Tracking**
   - Current free heap
   - Minimum free heap since boot
   - Automatic tracking of lowest memory point

2. **Subsystem Memory Estimation**
   - Neighbor table memory usage
   - Duplicate cache memory usage
   - Transmit queue memory usage
   - Node store memory usage
   - Total mesh subsystem memory

3. **Automatic Health Monitoring**
   - Periodic memory checks (every 60 seconds)
   - Warning alerts when memory is low
   - Critical alerts when memory is dangerously low
   - Integrated into maintenance routine

4. **Serial Command Interface**
   - `mesh memory` - Display detailed memory report

## Memory Thresholds

### Warning Level
- **Threshold:** Free heap < 10 KB (10,240 bytes)
- **Action:** Warning message printed to serial
- **Severity:** Low - system should continue operating normally

### Critical Level
- **Threshold:** Free heap < 5 KB (5,120 bytes)
- **Action:** Critical warning with recommendations
- **Severity:** High - system may become unstable

## Usage

### Manual Memory Check

To check current memory status at any time:

```
mesh memory
```

**Example Output:**
```
╔═══════════════════════════════════════════════════════════════╗
║                    MEMORY STATUS REPORT                       ║
╠═══════════════════════════════════════════════════════════════╣
║  HEAP STATUS:                                                 ║
║    Free Heap:          45672 bytes (44.60 KB)
║    Min Free Heap:      38912 bytes (38.00 KB)
║                                                               ║
║  MESH SUBSYSTEM MEMORY:                                       ║
║    Neighbor Table:     256 bytes
║    Duplicate Cache:    384 bytes
║    Transmit Queue:     576 bytes
║    Node Store:         1500 bytes
║    Total Mesh:         2716 bytes (2.65 KB)
╚═══════════════════════════════════════════════════════════════╝
```

### Automatic Monitoring

The system automatically checks memory health every 60 seconds during the periodic maintenance routine. No user action required.

**Warning Example:**
```
╔═══════════════════════════════════════════════════════════════╗
║  ⚠️  MEMORY WARNING                                           ║
╚═══════════════════════════════════════════════════════════════╝
⚠️  Free heap running low: 8456 bytes (8.26 KB)
Mesh subsystems using: 2716 bytes
```

**Critical Warning Example:**
```
╔═══════════════════════════════════════════════════════════════╗
║  ⚠️  CRITICAL MEMORY WARNING ⚠️                               ║
╚═══════════════════════════════════════════════════════════════╝
🚨 Free heap critically low: 4128 bytes (4.03 KB)

⚠️  SYSTEM MAY BECOME UNSTABLE!
Consider:
  - Reducing TX_QUEUE_SIZE
  - Reducing DUPLICATE_CACHE_SIZE
  - Reducing MAX_NEIGHBORS
  - Reducing MAX_NODES
```

## Memory Estimation Details

### Neighbor Table Memory

**Formula:**
```
Base size + (Active neighbors × 16 bytes)
```

**Per-neighbor storage:**
- Node ID: 1 byte
- RSSI current: 2 bytes
- RSSI min: 2 bytes
- RSSI max: 2 bytes
- Packet count: 4 bytes
- Last seen timestamp: 4 bytes
- Padding: ~1 byte
- **Total: 16 bytes per neighbor**

### Duplicate Cache Memory

**Formula:**
```
Base size + (Valid entries × 8 bytes)
```

**Per-entry storage:**
- Source ID: 1 byte
- Message ID: 2 bytes
- Timestamp: 4 bytes
- Valid flag: 1 byte
- **Total: 8 bytes per entry**

### Transmit Queue Memory

**Formula:**
```
Base size + (Queue capacity × 68 bytes)
```

**Per-packet storage:**
- Length: 1 byte
- Data buffer: 64 bytes
- Timestamp: 4 bytes (for age tracking)
- **Total: 68 bytes per queued packet**

### Node Store Memory

**Formula:**
```
Base overhead + (Active nodes × 150 bytes)
```

**Per-node storage:**
- NodeMessage structure: ~150 bytes
  - FullReportMsg: 80 bytes
  - Tracking data: 70 bytes (payload string, timestamps, counters)

## Configuration

### Adjusting Thresholds

Edit [include/memory_monitor.h](include/memory_monitor.h):

```cpp
// Default: warn at 10KB, critical at 5KB
#define MEMORY_WARNING_THRESHOLD_BYTES   10240
#define MEMORY_CRITICAL_THRESHOLD_BYTES  5120
```

### Check Interval

The memory check interval is defined in [src/memory_monitor.cpp](src/memory_monitor.cpp):

```cpp
// Default: check every 60 seconds
static const unsigned long MEMORY_CHECK_INTERVAL_MS = 60000;
```

## Integration

### Initialization

Memory monitoring is initialized during system startup in [src/main.cpp](src/main.cpp:284):

```cpp
void setup() {
    // ... other initialization
    initMemoryMonitor();
    printRow("Memory Monitor", "OK");
}
```

### Periodic Maintenance

Automatic memory checks run during the maintenance routine in [src/main.cpp](src/main.cpp:456):

```cpp
void loop() {
    // Periodic maintenance
    if (now - lastNeighborPrune >= NEIGHBOR_PRUNE_INTERVAL_MS) {
        // ... other maintenance tasks

        // Check memory health
        updateMemoryStats();

        lastNeighborPrune = now;
    }
}
```

## Troubleshooting

### Memory Warnings Appearing Frequently

**Possible Causes:**
1. Too many active neighbors tracked
2. Duplicate cache too large for traffic volume
3. Transmit queue filling up due to high forwarding rate
4. Node store tracking too many nodes

**Solutions:**

1. **Reduce neighbor table size** in [include/neighbor_table.h](include/neighbor_table.h):
   ```cpp
   #define MAX_NEIGHBORS 10  // Reduce from default 10 to 5
   ```

2. **Reduce duplicate cache size** in [include/duplicate_cache.h](include/duplicate_cache.h):
   ```cpp
   #define SEEN_CACHE_SIZE 32  // Reduce from 32 to 16
   ```

3. **Reduce transmit queue size** in [include/transmit_queue.h](include/transmit_queue.h):
   ```cpp
   #define TX_QUEUE_SIZE 8  // Reduce from 8 to 4
   ```

4. **Reduce max tracked nodes** in [include/config.h](include/config.h):
   ```cpp
   #define MESH_MAX_NODES 10  // Reduce from 10 to 5
   ```

### Critical Memory Warnings

If you see critical memory warnings:

1. **Immediately check memory report:**
   ```
   mesh memory
   ```

2. **Reset mesh subsystems to free memory:**
   ```
   mesh reset
   ```

3. **Check for memory leaks:**
   - Monitor if minimum free heap continues to drop over time
   - Use `mesh memory` periodically to track trends

4. **Reduce configuration values** as described above

### ESP32 Memory Specifications

**ESP32 Typical Heap:**
- Total RAM: ~300 KB
- Available heap: ~150-250 KB (depends on WiFi/BLE usage)
- Stack usage: ~30-50 KB
- **Working heap for application:** ~100-200 KB

**Memory Distribution in This Project:**
- Mesh subsystems: 2-5 KB
- Web server (if enabled): 20-40 KB
- WiFi stack: 40-60 KB
- LoRa buffers: 2-4 KB
- Display buffers: 1-2 KB
- **Remaining free heap:** 40-80 KB (typical)

## API Reference

### Functions

#### `initMemoryMonitor()`
Initialize memory monitoring system. Call once during setup.

**Returns:** void

---

#### `getMemoryStats()`
Get current memory statistics structure.

**Returns:** `MemoryStats` structure containing:
- `freeHeap` - Current free heap (bytes)
- `minFreeHeap` - Minimum free heap since boot (bytes)
- `neighborTableBytes` - Estimated neighbor table memory
- `duplicateCacheBytes` - Estimated duplicate cache memory
- `transmitQueueBytes` - Estimated transmit queue memory
- `nodeStoreBytes` - Estimated node store memory
- `totalMeshBytes` - Total mesh subsystem memory
- `usagePercent` - Heap usage percentage

**Example:**
```cpp
MemoryStats stats = getMemoryStats();
Serial.print("Free: ");
Serial.println(stats.freeHeap);
```

---

#### `printMemoryReport()`
Print formatted memory report to serial console.

**Returns:** void

**Example:**
```cpp
printMemoryReport();
```

---

#### `checkMemoryHealth()`
Check if memory is below warning/critical thresholds and print alerts.

**Returns:** `bool` - true if memory is critically low, false otherwise

**Example:**
```cpp
if (checkMemoryHealth()) {
    // Memory is critically low - take action
    duplicateCache.clear();
}
```

---

#### `updateMemoryStats()`
Periodic memory check called from maintenance routine. Automatically prints warnings if needed.

**Returns:** void

**Note:** This function rate-limits itself to run every 60 seconds.

## Files

### Header File
- **Path:** [include/memory_monitor.h](include/memory_monitor.h)
- **Purpose:** Function declarations, structure definitions, configuration

### Implementation File
- **Path:** [src/memory_monitor.cpp](src/memory_monitor.cpp)
- **Purpose:** Memory tracking logic, estimation algorithms, reporting

### Integration Points
- [src/main.cpp:284](src/main.cpp) - Initialization
- [src/main.cpp:456](src/main.cpp) - Periodic checks
- [src/mesh_commands.cpp:406](src/mesh_commands.cpp) - Serial command handler

## Performance Impact

### Initialization
- **Time:** < 1ms
- **Memory:** 16 bytes (static variables)

### Memory Check (`updateMemoryStats()`)
- **Frequency:** Every 60 seconds
- **CPU time:** < 2ms
- **Impact:** Negligible

### Memory Report (`printMemoryReport()`)
- **Trigger:** User command or critical condition
- **CPU time:** 10-20ms (mostly serial output)
- **Impact:** Only when explicitly called

## Best Practices

1. **Monitor Early and Often**
   - Check `mesh memory` when first deploying
   - Monitor minimum free heap over hours/days
   - Look for downward trends indicating leaks

2. **Use Warnings as Early Indicators**
   - Don't wait for critical warnings
   - Investigate when warnings first appear
   - Adjust configuration before problems occur

3. **Test Under Load**
   - Run network with maximum expected nodes
   - Generate heavy forwarding traffic
   - Monitor memory during peak usage

4. **Log Memory Stats**
   - Periodically save memory reports
   - Track minimum free heap over time
   - Identify patterns and trends

5. **Leave Safety Margin**
   - Don't configure system to use all available memory
   - Leave 30-50% free heap as safety margin
   - Account for temporary allocations

## See Also

- [MESH_COMMANDS_GUIDE.md](MESH_COMMANDS_GUIDE.md) - Serial command interface
- [DEBUG_LOGGING_INTEGRATION.md](DEBUG_LOGGING_INTEGRATION.md) - Debug logging system
- [neighbor_table.h](include/neighbor_table.h) - Neighbor tracking
- [duplicate_cache.h](include/duplicate_cache.h) - Duplicate detection
- [transmit_queue.h](include/transmit_queue.h) - Packet queue
- [node_store.h](include/node_store.h) - Node storage
