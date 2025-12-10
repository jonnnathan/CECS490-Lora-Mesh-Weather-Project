# Mesh Network Serial Commands Guide

## Overview

A comprehensive serial command interface has been added for testing, debugging, and managing the mesh network. All commands are entered via the Serial Monitor (115200 baud).

## Available Commands

### 1. `mesh status`

**Purpose:** Display current network status including neighbor table, queue depth, and cache configuration.

**Usage:**
```
mesh status
```

**Output Example:**
```
╔═══════════════════════════════════════════════════════════════╗
║  NEIGHBOR TABLE                                               ║
╚═══════════════════════════════════════════════════════════════╝
Active Neighbors: 3 / 10

┌──────┬─────────┬─────────┬─────────┬──────────┬──────────┐
│ Node │  RSSI   │   Min   │   Max   │ Packets  │ Last Heard│
├──────┼─────────┼─────────┼─────────┼──────────┼──────────┤
│   1  │ -68 dBm │ -75 dBm │ -62 dBm │   42     │   5s ago │
│   3  │ -72 dBm │ -80 dBm │ -65 dBm │   38     │   3s ago │
│   4  │ -85 dBm │ -92 dBm │ -78 dBm │   15     │  12s ago │
└──────┴─────────┴─────────┴─────────┴──────────┴──────────┘

╔═══════════════════════════════════════════════════════════════╗
║  TRANSMIT QUEUE STATUS                                        ║
╚═══════════════════════════════════════════════════════════════╝
Queue Depth: 2 / 8 (25% full)

Queue: [██░░░░░░]

⚠️  2 packet(s) queued for forwarding

╔═══════════════════════════════════════════════════════════════╗
║  DUPLICATE DETECTION CACHE                                    ║
╚═══════════════════════════════════════════════════════════════╝
Cache Configuration:
  Max Entries: 32
  Timeout: 120 seconds

Cache automatically prunes expired entries every 60 seconds.
```

**Information Provided:**
- **Neighbor Table:** All active neighbors with RSSI statistics and packet counts
- **Queue Status:** Number of packets waiting to be forwarded
- **Cache Configuration:** Duplicate detection cache size and timeout

---

### 2. `mesh stats`

**Purpose:** Display detailed mesh network statistics.

**Usage:**
```
mesh stats
```

**Output Example:**
```
╔═══════════════════════════════════════════════════════════════╗
║                    MESH NETWORK STATISTICS                    ║
╠═══════════════════════════════════════════════════════════════╣
║  RECEPTION:                                                   ║
║    Packets Received:      156                                 ║
║    Duplicates Dropped:    12                                  ║
║    Duplicate Rate:        7.1%                                ║
║                                                               ║
║  TRANSMISSION:                                                ║
║    Own Packets Sent:      145                                 ║
║    Packets Forwarded:     23                                  ║
║    Total Transmitted:     168                                 ║
║                                                               ║
║  DROPS & SKIPS:                                               ║
║    TTL Expired:           3                                   ║
║    Queue Overflows:       0                                   ║
║    Own Packets Ignored:   145                                 ║
║    Gateway BC Skips:      0                                   ║
║                                                               ║
║  Uptime: 02h 15m 42s                                          ║
╚═══════════════════════════════════════════════════════════════╝
```

**Metrics Explained:**
- **Packets Received:** Total valid packets received from other nodes
- **Duplicates Dropped:** Packets rejected due to duplicate detection
- **Duplicate Rate:** Percentage of received packets that were duplicates
- **Own Packets Sent:** Number of packets originated by this node
- **Packets Forwarded:** Number of packets relayed for other nodes
- **TTL Expired:** Packets not forwarded because TTL reached 0
- **Queue Overflows:** Packets dropped because forward queue was full
- **Own Packets Ignored:** Own packets correctly not forwarded (expected behavior)
- **Gateway BC Skips:** Gateway correctly didn't forward broadcasts (expected behavior)

---

### 3. `mesh reset`

**Purpose:** Clear all caches and reset statistics. Useful for starting fresh tests.

**Usage:**
```
mesh reset
```

**Output Example:**
```
╔═══════════════════════════════════════════════════════════════╗
║  RESETTING MESH SUBSYSTEMS                                    ║
╚═══════════════════════════════════════════════════════════════╝
Clearing duplicate cache...
✅ Duplicate cache cleared

Clearing neighbor table...
✅ Neighbor table cleared

Clearing transmit queue...
✅ Transmit queue cleared

Resetting mesh statistics...
✅ Statistics reset

🔄 All mesh subsystems have been reset!
```

**What Gets Reset:**
- **Duplicate Cache:** All seen message records cleared
- **Neighbor Table:** All neighbor entries removed (will rebuild from next received packets)
- **Transmit Queue:** All queued forwards discarded
- **Statistics:** All counters reset to zero

**Warning:** After a reset, the node will temporarily forget all neighbors until it receives packets from them again. This is normal and expected.

---

### 4. `mesh test`

**Purpose:** Send a test message with configurable destination, TTL, and data payload.

**Usage:**
```
mesh test [destId] [ttl] [message]
```

**Parameters:**
- `destId` - Destination node ID (0-255)
  - `0` or `0x00` = Gateway
  - `1-254` = Specific node
  - `255` or `0xFF` = Broadcast to all nodes
- `ttl` - Time To Live (1-255)
  - Number of hops the message can travel
  - Decremented at each relay
  - Typical values: 2-3 for multi-hop networks
- `message` - Optional test message text (any string)

**Examples:**

#### Example 1: Basic Test (Uses Defaults)
```
mesh test
```
Sends broadcast message with TTL=3 and message="TEST"

#### Example 2: Send to Specific Node
```
mesh test 3 2 Hello
```
Sends "Hello" to Node 3 with TTL=2 (can hop through 1 intermediate node)

#### Example 3: Broadcast with Custom TTL
```
mesh test 255 3 NetworkTest
```
Broadcasts "NetworkTest" to all nodes with TTL=3 (can hop through 2 intermediate nodes)

#### Example 4: Direct to Gateway
```
mesh test 0 1 GatewayTest
```
Sends "GatewayTest" directly to gateway with TTL=1 (no forwarding allowed)

**Output Example:**
```
╔═══════════════════════════════════════════════════════════════╗
║  SENDING TEST MESSAGE                                         ║
╚═══════════════════════════════════════════════════════════════╝
Destination: Node 3
TTL: 2
Test Data: "Hello"

Encoded 39 bytes
✅ Test message transmitted successfully
```

**Test Message Format:**
- Test messages use the `FULL_REPORT` message type
- The `humidity` field is set to 99.0% to indicate it's a test message
- The `temperature` field encodes the test message length
- Message ID starts at 200 (vs normal messages starting at 1) for easy identification

**Using Test Messages for Network Validation:**

1. **Direct Connectivity Test:**
   ```
   mesh test 3 1 DirectTest
   ```
   TTL=1 means no forwarding. If Node 3 receives this, you have direct connectivity.

2. **2-Hop Test:**
   ```
   mesh test 4 2 TwoHopTest
   ```
   TTL=2 allows one relay. If Node 4 receives this via Node 2, your mesh is working.

3. **Maximum Range Test:**
   ```
   mesh test 255 5 MaxRange
   ```
   TTL=5 allows 4 relays. Broadcast to see how far the message propagates.

4. **Forwarding Queue Test:**
   ```
   mesh test 255 3 Test1
   mesh test 255 3 Test2
   mesh test 255 3 Test3
   ```
   Send multiple test messages rapidly to test queue handling on relay nodes.

---

### 5. `mesh memory`

**Purpose:** Display detailed memory usage report including heap status and mesh subsystem memory consumption.

**Usage:**
```
mesh memory
```

**Output Example:**
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

**Information Provided:**
- **Free Heap:** Current available memory in bytes and kilobytes
- **Min Free Heap:** Lowest memory point since boot (helps identify memory leaks)
- **Neighbor Table:** Memory used by neighbor tracking system
- **Duplicate Cache:** Memory used by duplicate detection cache
- **Transmit Queue:** Memory used by forwarding packet queue
- **Node Store:** Memory used by per-node message storage
- **Total Mesh:** Combined memory usage of all mesh subsystems

**When to Use:**
- Investigating memory issues
- Monitoring long-term memory trends
- Debugging system instability
- Verifying memory impact of configuration changes

**Memory Health Indicators:**

Normal:
```
Free Heap: > 10 KB - System operating normally
```

Warning:
```
╔═══════════════════════════════════════════════════════════════╗
║  ⚠️  MEMORY WARNING                                           ║
╚═══════════════════════════════════════════════════════════════╝
⚠️  Free heap running low: 8456 bytes (8.26 KB)
```
Free Heap: 5-10 KB - Monitor closely, consider reducing configuration

Critical:
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
Free Heap: < 5 KB - System unstable, immediate action required

**Note:** Memory health is automatically checked every 60 seconds during periodic maintenance. Warnings appear automatically if thresholds are crossed.

---

### 6. `mesh help`

**Purpose:** Display help information for all mesh commands.

**Usage:**
```
mesh help
```
or
```
help
```

**Output:** Shows complete command reference with examples.

---

## Command Interface Features

### Character Echo
All typed characters are echoed back for user feedback.

### Backspace Support
Press backspace (or delete) to correct typing mistakes.

### Command Buffer
Commands up to 128 characters are supported.

### Case Insensitive
Commands work in any case: `MESH STATUS`, `mesh status`, or `Mesh Status`.

### Non-Blocking
The command processor does not block the main loop. Network operations continue while you type.

---

## Usage Scenarios

### Scenario 1: Initial Network Setup

When setting up a new mesh network:

1. **Start each node and check status:**
   ```
   mesh status
   ```

2. **Wait for neighbors to appear** (may take 1-2 minutes as nodes transmit)

3. **Verify neighbor table shows expected nodes** with good RSSI values

4. **Check statistics are reasonable:**
   ```
   mesh stats
   ```

### Scenario 2: Testing 2-Hop Forwarding

To verify Node 3 can reach Node 1 through Node 2:

1. **On Node 3, send test message to Node 1:**
   ```
   mesh test 1 2 ForwardTest
   ```

2. **On Node 2, check stats to see if it forwarded:**
   ```
   mesh stats
   ```
   Look for "Packets Forwarded" counter incrementing

3. **On Node 1, verify it received the packet** (check serial output for RX message)

### Scenario 3: Debugging Network Issues

If packets aren't being forwarded:

1. **Check neighbor tables on all nodes:**
   ```
   mesh status
   ```
   Verify intermediate nodes can see both source and destination

2. **Check for queue overflows:**
   ```
   mesh stats
   ```
   If "Queue Overflows" is high, reduce traffic or increase queue size

3. **Check for high duplicate rates:**
   ```
   mesh stats
   ```
   High duplicate rates (>20%) may indicate network loops or broadcast storms

4. **Reset and observe from clean state:**
   ```
   mesh reset
   mesh status
   ```

### Scenario 4: Performance Testing

To measure network performance:

1. **Reset statistics:**
   ```
   mesh reset
   ```

2. **Run test for 5 minutes** (let normal traffic flow)

3. **Check statistics:**
   ```
   mesh stats
   ```

4. **Calculate metrics:**
   - Forwarding rate = Packets Forwarded / Packets Received
   - Duplicate rate = Duplicates Dropped / (Packets Received + Duplicates Dropped)
   - Queue efficiency = 1 - (Queue Overflows / Packets Forwarded)

### Scenario 5: Range Testing

To find maximum communication range:

1. **Start with nodes close together, verify connectivity:**
   ```
   mesh status
   ```

2. **Move target node progressively farther**

3. **Send test packets at each distance:**
   ```
   mesh test 3 1 RangeTest
   ```

4. **On receiving node, check RSSI values:**
   ```
   mesh status
   ```
   Note the RSSI at each distance

5. **Find maximum range** where RSSI is still above -100 dBm

---

## Troubleshooting

### Command Not Working

**Problem:** Typed command but nothing happens

**Solutions:**
- Make sure to press Enter after typing the command
- Check Serial Monitor is set to 115200 baud
- Verify line ending is set to "Newline" or "Both NL & CR"
- Try typing just `help` to test the interface

### Neighbor Table Empty

**Problem:** `mesh status` shows no neighbors

**Solutions:**
- Wait 1-2 minutes for nodes to transmit in their TDMA slots
- Verify other nodes are powered on and running
- Check LoRa frequency, bandwidth, and spreading factor match on all nodes
- Verify nodes are within communication range (try <100m initially)

### Test Messages Not Received

**Problem:** `mesh test` sends successfully but target doesn't receive

**Solutions:**
- Check destination node ID is correct
- Verify TTL is sufficient for number of hops needed
- Use `mesh status` on all nodes to verify neighbor relationships
- Check for LoRa configuration mismatches
- Verify target node is in receive mode (check its serial output)

### Queue Always Full

**Problem:** `mesh status` shows queue at 8/8 constantly

**Solutions:**
- Too much traffic for queue size - this is normal in very dense networks
- Check `mesh stats` for "Queue Overflows" - if >10%, consider:
  - Reducing TDMA transmission frequency
  - Increasing TX_QUEUE_SIZE in transmit_queue.h
  - Reducing number of hops (lower default TTL)

### High Duplicate Rate

**Problem:** `mesh stats` shows >20% duplicate rate

**Solutions:**
- This is expected in dense mesh networks with multiple paths
- If >50%, check for:
  - Broadcast storms (gateway forwarding broadcasts)
  - Multiple nodes forwarding same packets simultaneously
  - TDMA slots overlapping (check GPS synchronization)

---

## Advanced Usage

### Automated Testing Script

You can send commands programmatically via serial:

```python
import serial
import time

ser = serial.Serial('COM3', 115200, timeout=1)

# Reset stats
ser.write(b'mesh reset\n')
time.sleep(2)

# Send 10 test messages
for i in range(10):
    ser.write(f'mesh test 3 2 Test{i}\n'.encode())
    time.sleep(5)  # Wait for TDMA cycle

# Check stats
ser.write(b'mesh stats\n')
time.sleep(1)

# Read response
print(ser.read_all().decode())
```

### Integration with Test Scenarios

Combine serial commands with the test scenarios in `TEST_SCENARIO_2HOP_FORWARDING.md`:

1. Before each test: `mesh reset`
2. After each test: `mesh stats` to verify expected behavior
3. Use `mesh test` to trigger specific forwarding scenarios
4. Use `mesh status` to verify neighbor table state

---

## Command Summary Table

| Command | Arguments | Purpose | Output |
|---------|-----------|---------|--------|
| `mesh status` | None | Show network status | Neighbor table, queue, cache info |
| `mesh stats` | None | Show statistics | Detailed mesh metrics |
| `mesh reset` | None | Clear all caches | Reset confirmation |
| `mesh test` | [dest] [ttl] [msg] | Send test message | Transmission result |
| `mesh memory` | None | Show memory usage | Heap status, subsystem memory |
| `mesh help` | None | Show command help | Full command reference |

---

## Integration Notes

### Adding to Existing Code

The mesh command system is integrated into [main.cpp](src/main.cpp:464):

```cpp
// Serial Command Processing
processMeshCommands();
```

This is called every loop iteration and is non-blocking.

### Required Files

- [include/mesh_commands.h](include/mesh_commands.h) - Command interface declarations
- [src/mesh_commands.cpp](src/mesh_commands.cpp) - Command implementation

### Dependencies

The command system uses these existing modules:
- `neighbor_table` - For neighbor information
- `duplicate_cache` - For cache management
- `transmit_queue` - For queue status
- `mesh_stats` - For statistics display
- `lora_comm` - For sending test messages

---

## Future Enhancements

Potential additions for future development:

1. **`mesh route [destId]`** - Show best route to destination
2. **`mesh flood [count]`** - Flood network with test packets
3. **`mesh log [on|off]`** - Enable/disable verbose logging
4. **`mesh ttl [value]`** - Change default TTL
5. **`mesh freq [value]`** - Change LoRa frequency (requires radio reconfiguration)

---

## See Also

- [DEBUG_LOGGING_INTEGRATION.md](DEBUG_LOGGING_INTEGRATION.md) - Conditional debug logging system
- [TEST_SCENARIO_2HOP_FORWARDING.md](TEST_SCENARIO_2HOP_FORWARDING.md) - Comprehensive test scenarios
- [MESH_TOPOLOGY_VISUALIZATION.js](MESH_TOPOLOGY_VISUALIZATION.js) - Dashboard visualization code
- [mesh_debug.h](include/mesh_debug.h) - Debug logging configuration
