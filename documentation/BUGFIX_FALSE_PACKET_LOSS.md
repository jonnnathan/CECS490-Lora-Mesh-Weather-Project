# Bug Fix: False Packet Loss on Forwarded Messages

## Problem Description

**Symptom:** Node 2 shows high packet loss rates (e.g., 30-50%) even though all messages are being received successfully.

**Root Cause:** Gap detection is using the wrong sequence number for forwarded mesh packets.

## Why This Happens

### The Two Sequence Numbers

The system has **two different sequence numbers**:

1. **LoRa Header `seq`** - Increments for every transmission from a specific radio
   - Node 1 transmits → `seq = 1, 2, 3, 4...`
   - Node 1 forwards packets → `seq = 5, 6, 7, 8...` (continues incrementing)
   - This tracks **radio transmissions**, not message origination

2. **Mesh Header `messageId`** - Message ID from the original source node
   - Node 3 creates message → `messageId = 1`
   - Node 1 forwards it → still `messageId = 1` (unchanged)
   - This tracks **original messages** from the source

### The Bug

In [node_store.cpp:48-78](src/node_store.cpp#L48-L78), gap detection uses `packet.header.seq`:

```cpp
uint16_t NodeMessage::updateFromPacket(const LoRaReceivedPacket& packet) {
    uint16_t gap = 0;

    if (hasData && packet.header.seq != expectedNextSeq) {
        // ❌ WRONG: Using LoRa seq for gap detection
        if (packet.header.seq > expectedNextSeq) {
            gap = packet.header.seq - expectedNextSeq;
        }
        // ...
        packetsLost += gap;
    }

    lastSeq = packet.header.seq;           // ❌ Storing forwarder's seq
    expectedNextSeq = packet.header.seq + 1;  // ❌ Expecting forwarder's next seq
    messageCount++;

    return gap;
}
```

## Failure Scenario

### Network Topology:
```
Node 3 -----> Node 1 -----> Node 2
(source)    (forwarder)   (receiver)
```

### Transmission Sequence:

**Node 3 transmits message #1:**
- Mesh: `sourceId=3, messageId=1`
- LoRa: `originId=3, seq=100`

**Node 1 receives and forwards:**
- Mesh: `sourceId=3, messageId=1` (unchanged)
- LoRa: `originId=1, seq=42` (Node 1's own LoRa seq)

**Node 2 receives:**
- Reads: `packet.header.seq = 42` (Node 1's LoRa seq)
- Stores: `lastSeq = 42`, `expectedNextSeq = 43`
- Tracking Node 3, but using Node 1's sequence numbers!

**Node 3 transmits message #2:**
- Mesh: `sourceId=3, messageId=2`
- LoRa: `originId=3, seq=101`

**Node 1 receives and forwards:**
- Mesh: `sourceId=3, messageId=2` (unchanged)
- LoRa: `originId=1, seq=45` (Node 1 transmitted 3 own packets in between)

**Node 2 receives:**
- Expected: `seq = 43`
- Got: `seq = 45`
- **Gap detected: 2 packets "lost"** ❌

**But in reality:** Node 3 sent messages 1 and 2 consecutively. Node 2 received both. **Zero packets actually lost.**

The "lost" packets are actually **Node 1's own transmissions** (not messages from Node 3).

## The Fix

### Solution: Use Mesh Header Message ID for Gap Detection

For mesh messages, track gaps using `meshHeader.messageId` instead of `packet.header.seq`.

### Required Changes

We need to modify the gap detection to be **context-aware**:
- For **mesh messages** (FULL_REPORT): Use `meshHeader.messageId` from the decoded message
- For **legacy messages**: Continue using `packet.header.seq`

This requires changes in how `updateFromPacket()` is called.

## Implementation

### Change 1: Add Mesh-Aware Gap Detection

Modify [src/node_store.cpp](src/node_store.cpp) to accept mesh message ID:

**New function signature:**
```cpp
// For mesh messages - pass the mesh message ID
uint16_t NodeMessage::updateFromMeshPacket(const LoRaReceivedPacket& packet, uint8_t meshMessageId);

// For legacy messages - continue using LoRa seq
uint16_t NodeMessage::updateFromPacket(const LoRaReceivedPacket& packet);
```

**Implementation:**
```cpp
uint16_t NodeMessage::updateFromMeshPacket(const LoRaReceivedPacket& packet, uint8_t meshMessageId) {
    uint16_t gap = 0;

    // Use mesh messageId for gap detection (wraps at 255)
    if (hasData && meshMessageId != expectedNextSeq) {
        // Calculate gap (handle sequence wraparound at 255)
        if (meshMessageId > expectedNextSeq) {
            gap = meshMessageId - expectedNextSeq;
        } else if (meshMessageId < lastSeq) {
            // Sequence wrapped around (8-bit max = 255)
            gap = (255 - expectedNextSeq) + meshMessageId + 1;
        }

        // Sanity check - ignore huge gaps (likely node restart)
        if (gap > 0 && gap < 100) {  // Reduced threshold for 8-bit seq
            packetsLost += gap;
        }
    }

    hasData = true;
    isOnline = true;
    payload = packet.payload;
    originId = packet.header.originId;
    lastSeq = meshMessageId;              // ✅ Store mesh message ID
    expectedNextSeq = meshMessageId + 1;  // ✅ Expect next mesh message ID
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
        if (packet.header.seq > expectedNextSeq) {
            gap = packet.header.seq - expectedNextSeq;
        } else if (packet.header.seq < lastSeq) {
            gap = (65535 - expectedNextSeq) + packet.header.seq + 1;
        }

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
```

### Change 2: Update packet_handler.cpp to Use Mesh Message ID

In [src/packet_handler.cpp:203](src/packet_handler.cpp#L203), change from:

```cpp
// ❌ OLD: Uses LoRa seq (wrong for forwarded packets)
gap = node->updateFromPacket(packet);
```

To:

```cpp
// ✅ NEW: Uses mesh messageId for gap detection
gap = node->updateFromMeshPacket(packet, lastReceivedReport.meshHeader.messageId);
```

### Change 3: Update node_store.h Header

Add the new function declaration to [include/node_store.h](include/node_store.h):

```cpp
struct NodeMessage {
    // ... existing fields ...

    // Update from mesh packet (uses mesh messageId for gap detection)
    uint16_t updateFromMeshPacket(const LoRaReceivedPacket& packet, uint8_t meshMessageId);

    // Update from legacy packet (uses LoRa seq for gap detection)
    uint16_t updateFromPacket(const LoRaReceivedPacket& packet);
};
```

## Expected Behavior After Fix

### Test Case: 2-Hop Forwarding

**Setup:**
- Node 3 transmits messages every 60 seconds
- Node 1 forwards to Node 2
- Node 1 also transmits own messages

**Without Fix:**
```
Time    | Action                              | Node 2's Loss from Node 3
--------|-------------------------------------|-------------------------
0:00    | Node 3 msg #1 → Node 1 → Node 2    | 0 lost
0:20    | Node 1 own msg (LoRa seq++++)      | --
0:40    | Node 1 own msg (LoRa seq++++)      | --
1:00    | Node 3 msg #2 → Node 1 → Node 2    | 2 lost ❌ (false positive)
2:00    | Node 3 msg #3 → Node 1 → Node 2    | 2 lost ❌ (false positive)
3:00    | Node 3 msg #4 → Node 1 → Node 2    | 2 lost ❌ (false positive)
--------|-------------------------------------|-------------------------
After 4 minutes: 6 packets "lost" (all false positives)
Loss rate: 6/(4+6) = 60% ❌
```

**With Fix:**
```
Time    | Action                              | Node 2's Loss from Node 3
--------|-------------------------------------|-------------------------
0:00    | Node 3 msg #1 → Node 1 → Node 2    | 0 lost
0:20    | Node 1 own msg (ignored)           | --
0:40    | Node 1 own msg (ignored)           | --
1:00    | Node 3 msg #2 → Node 1 → Node 2    | 0 lost ✅
2:00    | Node 3 msg #3 → Node 1 → Node 2    | 0 lost ✅
3:00    | Node 3 msg #4 → Node 1 → Node 2    | 0 lost ✅
--------|-------------------------------------|-------------------------
After 4 minutes: 0 packets lost (correct)
Loss rate: 0/(4+0) = 0% ✅
```

## Why This Fix is Correct

### Direct Reception
When Node 2 receives **directly** from Node 3:
- `packet.header.originId = 3` (Node 3 transmitted)
- `meshHeader.sourceId = 3` (Node 3 created message)
- `meshHeader.messageId = 1, 2, 3, ...` (Node 3's message sequence)
- Gap detection uses Node 3's `messageId` ✅ Correct

### Forwarded Reception
When Node 2 receives **via Node 1**:
- `packet.header.originId = 1` (Node 1 transmitted)
- `meshHeader.sourceId = 3` (Node 3 created message)
- `meshHeader.messageId = 1, 2, 3, ...` (Node 3's message sequence)
- Gap detection uses Node 3's `messageId` ✅ Correct

### Legacy Messages
Non-mesh messages:
- No mesh header, falls back to `updateFromPacket()`
- Uses LoRa `seq` as before ✅ Backward compatible

## Sequence Number Comparison

| Sequence Type | Bits | Max Value | Usage | Tracked Per |
|---------------|------|-----------|-------|-------------|
| `packet.header.seq` | 16 | 65535 | LoRa transmission count | Radio (transmitter) |
| `meshHeader.messageId` | 8 | 255 | Message origination count | Source node |

**Key Difference:**
- **LoRa seq** = How many times **this radio** transmitted (including forwards)
- **Mesh messageId** = Which message **from the source node** this is

For tracking **source node's** message sequence, we must use **mesh messageId**.

## Implementation Files Changed

1. **[include/node_store.h](include/node_store.h)** - Add `updateFromMeshPacket()` declaration
2. **[src/node_store.cpp](src/node_store.cpp)** - Implement both functions (mesh + legacy)
3. **[src/packet_handler.cpp](src/packet_handler.cpp)** - Call `updateFromMeshPacket()` for mesh messages

## Verification

After applying fix, test with:

```
mesh reset
# Let network run for 5 minutes with forwarding
mesh stats
```

**Expected:**
- Packet loss should be 0% (or very low, only actual RF losses)
- No false positives from forwarder's LoRa sequence gaps

**Check specific node:**
Via web dashboard or serial output, verify:
- Node 3's packet loss on Node 2 = near 0%
- Message count matches expected (1 per minute ×  5 = 5 messages)

## Related Issues

This fix also resolves:
1. **Inflated loss percentages** on relay nodes
2. **Incorrect statistics** in web dashboard
3. **Gap detection triggering on forwarder transmissions** instead of source message gaps

## Summary

**Root Cause:** Gap detection used LoRa radio sequence number instead of mesh message ID

**Problem:** Forwarder's own transmissions counted as "lost" packets from source

**Solution:** Use mesh `messageId` for gap detection, not LoRa `seq`

**Result:** Accurate packet loss tracking for mesh-forwarded messages

**Files Changed:**
- [include/node_store.h](include/node_store.h) - Add new function
- [src/node_store.cpp](src/node_store.cpp) - Implement mesh-aware gap detection
- [src/packet_handler.cpp](src/packet_handler.cpp) - Use mesh message ID
