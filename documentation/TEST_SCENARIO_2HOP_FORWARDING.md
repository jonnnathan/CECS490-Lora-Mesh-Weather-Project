# Phase 4: 2-Hop Forwarding Test Scenarios

## Test Environment Setup

### Hardware Configuration
```
Node 3 (DEV3) ←--→ Node 2 (DEV2) ←--→ Node 1 (DEV1/Gateway)
   Source            Relay              Destination
```

### Node Configurations

| Node | ID | Name | Role | Slot Time | TX Second |
|------|----|----|------|-----------|-----------|
| Node 1 | 1 | DEV1 | Gateway | 0-11s | 6s |
| Node 2 | 2 | DEV2 | Relay | 12-23s | 18s |
| Node 3 | 3 | DEV3 | Source | 24-35s | 30s |

### Expected Behavior Summary

**Second 30:** Node 3 transmits its FULL_REPORT
- sourceId = 3, senderId = 3, messageId = X, ttl = 3

**Second 30-31:** Node 2 receives and processes
- Checks duplicate cache → NEW
- Marks as seen
- Processes locally (prints, displays, updates neighbor table)
- Checks shouldForward() → TRUE
- Calls scheduleForward()
- Decrements ttl to 2
- Updates senderId to 2
- Enqueues for transmission

**Second 18 (next minute):** Node 2's TDMA slot
- Transmits own FULL_REPORT first
- After 100ms delay, calls transmitQueuedForwards()
- Transmits Node 3's packet (modified: sourceId=3, senderId=2, ttl=2)

**Second 19:** Node 1 receives forwarded packet
- sourceId = 3, senderId = 2, messageId = X, ttl = 2
- Processes locally
- Checks shouldForward() → FALSE (gateway doesn't forward broadcasts)
- Does NOT enqueue

---

## Test Scenario 1: Basic 2-Hop Forward (Node 3 → Node 2 → Node 1)

### Objective
Verify that a packet from Node 3 is correctly forwarded by Node 2 and received by Node 1 (gateway).

### Prerequisites
- All three nodes powered on and synchronized to GPS
- Node 1 configured as gateway (DEVICE_ID = 1)
- All nodes have valid GPS fix
- Nodes physically arranged: Node 3 ←--→ Node 2 ←--→ Node 1
- Node 3 cannot directly reach Node 1 (verify with RSSI)

### Test Steps

#### Step 1: Monitor Node 3 Transmission (Second 30)
**Expected Output on Node 3 Serial:**
```
╔═══════════════════════════════════════════════════════════════╗
║                  TRANSMITTING FULL_REPORT                     ║
╠═══════════════════════════════════════════════════════════════╣
║  Sequence:        42                                          ║
║  Temp:            72.5 F                                      ║
║  Humidity:        45.0 %                                      ║
║  Pressure:        1013 hPa                                    ║
║  GPS Valid:       Yes                                         ║
║  Satellites:      8                                           ║
║  Uptime:          180 sec                                     ║
║  Payload Size:    39 bytes                                    ║
╚═══════════════════════════════════════════════════════════════╝
✅ TX SUCCESS
```

**Verify:**
- [ ] Transmission occurs at exactly second 30
- [ ] Payload size is 39 bytes (8-byte header + 31-byte report)
- [ ] TX SUCCESS printed

#### Step 2: Monitor Node 2 Reception (Second 30-31)
**Expected Output on Node 2 Serial:**
```
╔═══════════════════════════════════════════════════════════════╗
║                  RECEIVED: FULL_REPORT                        ║
╠═══════════════════════════════════════════════════════════════╣
║  From Node:       3 (DEV3)                                    ║
║  Via (Sender):    3                                           ║
║  Message ID:      42                                          ║
║  TTL:             3                                           ║
║  Flags:           0x00                                        ║
║                                                               ║
║  RSSI:            -75 dBm                                     ║
║  SNR:             8.5 dB                                      ║
║  Temp:            72.5 F                                      ║
║  GPS Valid:       Yes                                         ║
║  Neighbor Count:  2 nodes                                     ║
╚═══════════════════════════════════════════════════════════════╝

📤 Scheduled forward: src=3 dest=255 msgId=42 ttl=2 queueDepth=1
```

**Verify:**
- [ ] Packet received and decoded successfully
- [ ] "Via (Sender)" shows 3 (direct from Node 3)
- [ ] TTL in received packet is 3
- [ ] Forward scheduled with TTL=2 (decremented)
- [ ] Queue depth shows 1

#### Step 3: Monitor Node 2 Forwarding (Second 18, next minute)
**Expected Output on Node 2 Serial:**
```
╔═══════════════════════════════════════════════════════════════╗
║                  TRANSMITTING FULL_REPORT                     ║
║  (Node 2's own report)                                        ║
╚═══════════════════════════════════════════════════════════════╝
✅ TX SUCCESS

🔄 Forwarding queued packet (1/1) size=39 bytes
✅ Forward transmitted
📊 Forwarded 1 packet(s) this slot. Queue remaining: 0
```

**Verify:**
- [ ] Node 2's own report transmitted first at second 18
- [ ] Forward transmission occurs ~100ms after primary
- [ ] Forward successful
- [ ] Queue emptied (remaining: 0)

#### Step 4: Monitor Node 1 Reception (Second 19)
**Expected Output on Node 1 Serial:**
```
╔═══════════════════════════════════════════════════════════════╗
║                  RECEIVED: FULL_REPORT                        ║
╠═══════════════════════════════════════════════════════════════╣
║  From Node:       3 (DEV3)                                    ║
║  Via (Sender):    2                                           ║
║  Message ID:      42                                          ║
║  TTL:             2                                           ║
║  Flags:           0x02 (FORWARDED)                            ║
║                                                               ║
║  RSSI:            -68 dBm                                     ║
║  SNR:             9.2 dB                                      ║
║  Temp:            72.5 F                                      ║
╚═══════════════════════════════════════════════════════════════╝

(No forwarding message - gateway doesn't forward broadcasts)
```

**Verify:**
- [ ] Packet shows sourceId = 3 (original sender)
- [ ] "Via (Sender)" shows 2 (immediate sender/forwarder)
- [ ] TTL is 2 (decremented from 3)
- [ ] Flags include FLAG_IS_FORWARDED (0x02)
- [ ] NO forward scheduled (gateway rule)
- [ ] Data from Node 3 appears in ThingSpeak

---

## Test Scenario 2: Duplicate Detection Across 2 Hops

### Objective
Verify that if Node 1 somehow receives the same packet twice (once forwarded by Node 2, once by another path), it detects and drops the duplicate.

### Prerequisites
- Same as Scenario 1
- Node 4 also in range of Node 3 and Node 1

### Test Setup
```
        Node 3
       /      \
   Node 2    Node 4
       \      /
        Node 1
```

### Expected Behavior
1. Node 3 transmits (second 30)
2. Both Node 2 and Node 4 receive and schedule forwards
3. Node 2 forwards first (second 18)
4. Node 1 receives and processes
5. Node 4 forwards later (second 42)
6. Node 1 detects duplicate and drops

**Expected Output on Node 1 (second 42):**
```
🚫 Duplicate mesh message from Node 3 msg #42 (dropped, total: 1)
```

**Verify:**
- [ ] First packet processed normally
- [ ] Second packet detected as duplicate
- [ ] Duplicate counter incremented
- [ ] No double processing

---

## Test Scenario 3: TTL Expiration

### Objective
Verify that packets with TTL=1 are NOT forwarded.

### Test Setup
Modify Node 3's transmission to use TTL=1:
```cpp
// In buildFullReport() or transmit()
report.meshHeader.ttl = 1;  // Test value
```

### Expected Behavior
1. Node 3 transmits with TTL=1
2. Node 2 receives, checks shouldForward()
3. shouldForward() returns FALSE (TTL <= 1)
4. Packet NOT queued for forwarding

**Expected Output on Node 2:**
```
╔═══════════════════════════════════════════════════════════════╗
║                  RECEIVED: FULL_REPORT                        ║
║  TTL:             1                                           ║
╚═══════════════════════════════════════════════════════════════╝

(No "Scheduled forward" message)
```

**Verify:**
- [ ] Packet received and processed locally
- [ ] NO forward scheduled
- [ ] ttlExpired counter incremented
- [ ] Mesh stats show ttlExpired > 0

---

## Test Scenario 4: Queue Overflow

### Objective
Verify proper handling when forward queue is full.

### Test Setup
1. Reduce TX_QUEUE_SIZE to 2 in transmit_queue.h
2. Have 3+ nodes all transmit in rapid succession
3. Monitor Node 2 (middle node)

### Expected Behavior
1. First 2 forwards queued successfully
2. 3rd forward attempt fails
3. Queue overflow logged and counted

**Expected Output on Node 2:**
```
📤 Scheduled forward: src=3 dest=255 msgId=42 ttl=2 queueDepth=1
📤 Scheduled forward: src=4 dest=255 msgId=15 ttl=2 queueDepth=2
⚠️ Forward queue FULL - dropped: src=5 msgId=8
```

**Verify:**
- [ ] First packets queued successfully
- [ ] Queue full warning displayed
- [ ] queueOverflows counter incremented
- [ ] Mesh stats show queueOverflows > 0
- [ ] Source and message ID logged for dropped packet

---

## Test Scenario 5: Slot Timing Safety

### Objective
Verify that forwarding stops before slot end to prevent overrun.

### Test Setup
1. Queue 3-4 packets for forwarding
2. Delay Node 2's primary transmission to later in slot (second 22)
3. Monitor timing

### Expected Behavior
1. Primary transmission at second 22
2. Start forwarding at second 22-23
3. Time check detects approaching slot end (second 23)
4. Forwarding stops with warning

**Expected Output on Node 2:**
```
✅ TX SUCCESS (primary report)

🔄 Forwarding queued packet (1/4) size=39 bytes
✅ Forward transmitted
⏱️ Slot time ending - stopping forwards
📊 Forwarded 1 packet(s) this slot. Queue remaining: 3
```

**Verify:**
- [ ] Forwarding stops before slot end
- [ ] Time warning printed
- [ ] Remaining packets stay in queue
- [ ] No transmission during Node 3's slot

---

## Test Scenario 6: Gateway Broadcast Prevention

### Objective
Verify that gateway (Node 1) never forwards broadcast packets.

### Test Setup
1. Configure Node 1 as gateway (DEVICE_ID = 1)
2. Node 2 transmits broadcast (destId = 0xFF)
3. Monitor Node 1

### Expected Behavior
1. Node 1 receives broadcast packet
2. shouldForward() checks gateway rule
3. Returns FALSE
4. No forward scheduled

**Expected Output on Node 1:**
```
╔═══════════════════════════════════════════════════════════════╗
║                  RECEIVED: FULL_REPORT                        ║
║  Dest:            255 (BROADCAST)                             ║
╚═══════════════════════════════════════════════════════════════╝

(No "Scheduled forward" message)
```

**Verify:**
- [ ] Packet received and processed locally
- [ ] NO forward scheduled
- [ ] gatewayBroadcastSkips counter incremented
- [ ] Mesh stats show gatewayBroadcastSkips > 0

---

## Test Scenario 7: Own Message Prevention

### Objective
Verify that nodes never forward their own messages.

### Test Setup
1. Node 2 transmits a packet
2. Due to multipath or reflection, Node 2 receives its own packet
3. Monitor Node 2

### Expected Behavior
1. Node 2 receives packet with sourceId = 2 (own ID)
2. shouldForward() checks sourceId
3. Returns FALSE
4. No forward scheduled

**Expected Output on Node 2:**
```
╔═══════════════════════════════════════════════════════════════╗
║                  RECEIVED: FULL_REPORT                        ║
║  From Node:       2 (own message)                             ║
╚═══════════════════════════════════════════════════════════════╝

(No "Scheduled forward" message)
```

**Verify:**
- [ ] Packet received but not forwarded
- [ ] ownPacketsIgnored counter incremented
- [ ] Mesh stats show ownPacketsIgnored > 0

---

## Verification Checklist

### Physical Layer
- [ ] Node 3 cannot directly reach Node 1 (RSSI check)
- [ ] Node 2 can reach both Node 3 and Node 1
- [ ] GPS lock on all nodes
- [ ] All nodes synchronized to GPS time

### Packet Structure
- [ ] sourceId remains unchanged through hops
- [ ] senderId updated at each hop
- [ ] TTL decremented at each hop
- [ ] FLAG_IS_FORWARDED set on forwarded packets
- [ ] messageId unchanged (for duplicate detection)

### Timing
- [ ] Node 3 transmits at second 30
- [ ] Node 2 receives within 1 second
- [ ] Node 2 forwards at second 18 (next minute)
- [ ] Node 1 receives within 1 second

### Statistics
- [ ] Node 2: packetsReceived incremented
- [ ] Node 2: packetsForwarded incremented
- [ ] Node 1: packetsReceived incremented
- [ ] No duplicatesDropped (first time through)
- [ ] No queueOverflows (normal operation)
- [ ] No ttlExpired (TTL=3 initially)

### Logs and Display
- [ ] All nodes show detailed reception info
- [ ] Forward scheduling logged with details
- [ ] Forward transmission success/failure logged
- [ ] Queue depth displayed
- [ ] Mesh statistics print every 60 seconds

---

## Expected Mesh Statistics Output

### Node 3 (Source)
```
╔═══════════════════════════════════════════════════════════════╗
║                    MESH NETWORK STATISTICS                    ║
╠═══════════════════════════════════════════════════════════════╣
║  RECEPTION:                                                   ║
║    Packets Received:      0                                   ║
║    Duplicates Dropped:    0                                   ║
║                                                               ║
║  TRANSMISSION:                                                ║
║    Own Packets Sent:      5                                   ║
║    Packets Forwarded:     0                                   ║
║    Total Transmitted:     5                                   ║
║                                                               ║
║  DROPS & SKIPS:                                               ║
║    TTL Expired:           0                                   ║
║    Queue Overflows:       0                                   ║
║    Own Packets Ignored:   0                                   ║
║    Gateway BC Skips:      0                                   ║
╚═══════════════════════════════════════════════════════════════╝
```

### Node 2 (Relay)
```
╔═══════════════════════════════════════════════════════════════╗
║                    MESH NETWORK STATISTICS                    ║
╠═══════════════════════════════════════════════════════════════╣
║  RECEPTION:                                                   ║
║    Packets Received:      10                                  ║
║    Duplicates Dropped:    0                                   ║
║                                                               ║
║  TRANSMISSION:                                                ║
║    Own Packets Sent:      5                                   ║
║    Packets Forwarded:     5                                   ║
║    Total Transmitted:     10                                  ║
║                                                               ║
║  DROPS & SKIPS:                                               ║
║    TTL Expired:           0                                   ║
║    Queue Overflows:       0                                   ║
║    Own Packets Ignored:   0                                   ║
║    Gateway BC Skips:      0                                   ║
╚═══════════════════════════════════════════════════════════════╝
```

### Node 1 (Gateway)
```
╔═══════════════════════════════════════════════════════════════╗
║                    MESH NETWORK STATISTICS                    ║
╠═══════════════════════════════════════════════════════════════╣
║  RECEPTION:                                                   ║
║    Packets Received:      10                                  ║
║    Duplicates Dropped:    0                                   ║
║                                                               ║
║  TRANSMISSION:                                                ║
║    Own Packets Sent:      5                                   ║
║    Packets Forwarded:     0                                   ║
║    Total Transmitted:     5                                   ║
║                                                               ║
║  DROPS & SKIPS:                                               ║
║    TTL Expired:           0                                   ║
║    Queue Overflows:       0                                   ║
║    Own Packets Ignored:   0                                   ║
║    Gateway BC Skips:      5                                   ║
╚═══════════════════════════════════════════════════════════════╝
```

---

## Troubleshooting Guide

### Issue: Node 2 doesn't forward
**Possible Causes:**
1. shouldForward() returning false
2. Queue enqueue failing
3. Slot timing issue

**Debug Steps:**
1. Check TTL value in received packet
2. Check sourceId != DEVICE_ID
3. Check queue depth before enqueue
4. Verify transmitQueuedForwards() is called

### Issue: Node 1 doesn't receive
**Possible Causes:**
1. Node 2 not transmitting forward
2. Radio timing issue
3. Node 1 out of range

**Debug Steps:**
1. Check Node 2 logs for "Forward transmitted"
2. Check Node 2 slot timing (should be second 18-19)
3. Verify RSSI between Node 2 and Node 1

### Issue: Duplicate not detected
**Possible Causes:**
1. messageId different in packets
2. Cache expired
3. Cache not marking seen

**Debug Steps:**
1. Compare messageId in both packets
2. Check cache timeout (120 seconds)
3. Verify duplicateCache.markSeen() called

---

## Success Criteria

Phase 4 is considered successful when:

✅ **2-Hop Communication:**
- [ ] Packets successfully forwarded across 2 hops
- [ ] Data from Node 3 appears on Node 1
- [ ] ThingSpeak shows Node 3 data (uploaded by Node 1)

✅ **Packet Integrity:**
- [ ] sourceId unchanged through forwarding
- [ ] senderId updated at each hop
- [ ] TTL decremented properly
- [ ] Data payload unchanged

✅ **Loop Prevention:**
- [ ] Duplicates detected and dropped
- [ ] Own messages not forwarded
- [ ] Gateway doesn't rebroadcast
- [ ] TTL prevents infinite loops

✅ **Timing & Resource Management:**
- [ ] No slot overruns
- [ ] Queue overflows handled gracefully
- [ ] Old messages pruned from queue
- [ ] Statistics tracked accurately

✅ **Logging & Monitoring:**
- [ ] All events clearly logged
- [ ] Statistics available and accurate
- [ ] Errors visible and actionable
- [ ] Queue depth monitored

---

## Phase 4 Complete!

Once all test scenarios pass, the mesh network supports multi-hop forwarding with:
- Automatic packet relay
- Duplicate detection
- Loop prevention
- Resource management
- Comprehensive monitoring

**Next Steps:** Test 3-hop scenarios, stress testing, and production deployment.
