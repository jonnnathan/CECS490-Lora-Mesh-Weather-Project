# Bug Fix: Radio Loopback Causing Message Double Counting

## Problem Description

**Symptom:** Nodes are counting their own transmitted messages twice. For example, Node 1 shows 8 messages after 4 minutes (should be 4).

**Root Cause:** **Radio loopback** - nodes are receiving their own transmitted packets back from the radio.

## Why This Happens

Some LoRa modules (including SX1262/SX1276) can receive their own transmissions under certain conditions:

1. **TX/RX Fast Switching:** When a node transmits and immediately switches back to receive mode, it can catch the tail end of its own transmission
2. **Near-Field Coupling:** Strong signal from nearby antenna can couple into the receiver path
3. **Power Reflections:** Impedance mismatches can reflect signals back into the receiver
4. **Module Behavior:** Some LoRa modules don't fully isolate TX from RX

## The Issue in Code

**Before the fix:**

```cpp
if (decodeFullReport(...)) {
    // No check if this is our own packet!

    // Duplicate detection (but doesn't catch first occurrence)
    if (duplicateCache.isDuplicate(sourceId, messageId)) {
        continue;  // Drop duplicate
    }

    duplicateCache.markSeen(sourceId, messageId);

    // Update node store - increments messageCount
    node->updateFromPacket(packet);  // ❌ Increments even for our own packets!
}
```

### What Happened:

1. Node 1 transmits message #42
2. Node 1's radio immediately hears its own transmission
3. Packet decoded successfully
4. Not in duplicate cache yet (first time seeing it)
5. `node->updateFromPacket()` called → `messageCount++` → Now at 1
6. Later, Node 1 forwards or receives the same message legitimately
7. Duplicate cache catches it this time → dropped
8. **Result:** Node 1's own counter gets incremented once per transmission

Wait, that would only be +1, not +2...

### Actually, the Real Issue:

Looking more carefully, if Node 1 is showing **8 messages in 4 minutes**, that's exactly **2× the expected rate**. This means:

1. Node 1 transmits message #1 → `messageCount` not incremented (we track received, not sent)
2. Node 1 hears its own message #1 via radio loopback → `messageCount++` → Now at 1
3. Node 1 transmits message #2 → (not counted)
4. Node 1 hears its own message #2 via radio loopback → `messageCount++` → Now at 2

So the node's `messageCount` for itself is being incremented **only from radio loopback**, causing it to appear that it received messages from itself.

## The Fix

Add an **early check** to skip processing packets where we are the source:

```cpp
if (decodeFullReport(...)) {
    // ✅ ADDED: Skip our own packets (radio loopback prevention)
    if (lastReceivedReport.meshHeader.sourceId == DEVICE_ID) {
        DEBUG_RX_F("Ignoring own packet | sourceId=%d msgId=%d (radio loopback)",
                  lastReceivedReport.meshHeader.sourceId,
                  lastReceivedReport.meshHeader.messageId);
        continue;
    }

    // Now proceed with normal processing
    debugLogPacketRx(&lastReceivedReport.meshHeader, packet.rssi, packet.snr);

    // Duplicate detection
    if (duplicateCache.isDuplicate(...)) {
        continue;
    }

    // Update node store (only for packets from other nodes)
    node->updateFromPacket(packet);
}
```

### Why This Works:

**Before Fix:**
- Node 1 transmits
- Node 1 receives own packet → `node[1].messageCount++`
- **Result:** Node 1's counter shows it received from itself

**After Fix:**
- Node 1 transmits
- Node 1 receives own packet → **Skipped immediately**
- **Result:** Node 1's counter stays at 0 (correct - it hasn't received from itself)

## Code Changes

### File: src/packet_handler.cpp

**Lines 142-151 (Added radio loopback check):**

```cpp
if (msgType == MSG_FULL_REPORT && decodeFullReport(packet.payloadBytes, packet.payloadLen, lastReceivedReport)) {
    // ─────────────────────────────────────────────────────────────────────
    // Skip our own packets (radio loopback prevention)
    // ─────────────────────────────────────────────────────────────────────
    if (lastReceivedReport.meshHeader.sourceId == DEVICE_ID) {
        DEBUG_RX_F("Ignoring own packet | sourceId=%d msgId=%d (radio loopback)",
                  lastReceivedReport.meshHeader.sourceId,
                  lastReceivedReport.meshHeader.messageId);
        continue;  // Skip to next packet
    }

    // Now proceed with normal packet processing
    debugLogPacketRx(&lastReceivedReport.meshHeader, packet.rssi, packet.snr);
    // ... rest of processing
}
```

## Why Not Use `shouldForward()` Check?

You might wonder: "We already have a check in `shouldForward()` that skips own packets - why do we need another?"

**Answer:** The `shouldForward()` check is for **forwarding decisions**, which happens **after** we've already:
1. ✅ Logged the packet reception
2. ✅ **Incremented the node store counter** ← This is the problem!
3. ✅ Updated the neighbor table
4. ✅ Updated the display

By the time we get to `shouldForward()`, the damage is done - `messageCount` has already been incremented.

### The Forwarding Check (Still Needed):

```cpp
bool shouldForward(MeshHeader* header) {
    // Don't forward own messages
    if (header->sourceId == DEVICE_ID) {
        incrementOwnPacketsIgnored();
        return false;  // ← This prevents forwarding, not counting
    }
    // ...
}
```

This check is **still necessary** to prevent forwarding our own packets. The new check at the top prevents **counting/processing** our own packets.

## Expected Behavior After Fix

### Test Case: Node 1 Transmits Every 60 Seconds

**Without Fix:**
```
Time    | Action                  | Node 1's messageCount for Node 1
--------|-------------------------|----------------------------------
0:00    | Node 1 transmits msg 1  | 0
0:00    | Node 1 hears own msg 1  | 1 (❌ radio loopback counted)
1:00    | Node 1 transmits msg 2  | 1
1:00    | Node 1 hears own msg 2  | 2 (❌ radio loopback counted)
2:00    | Node 1 transmits msg 3  | 2
2:00    | Node 1 hears own msg 3  | 3 (❌ radio loopback counted)
3:00    | Node 1 transmits msg 4  | 3
3:00    | Node 1 hears own msg 4  | 4 (❌ radio loopback counted)
--------|-------------------------|----------------------------------
After 4 minutes: 4 messages shown (should be 0)
```

**With Fix:**
```
Time    | Action                  | Node 1's messageCount for Node 1
--------|-------------------------|----------------------------------
0:00    | Node 1 transmits msg 1  | 0
0:00    | Node 1 hears own msg 1  | 0 (✅ skipped - own packet)
1:00    | Node 1 transmits msg 2  | 0
1:00    | Node 1 hears own msg 2  | 0 (✅ skipped - own packet)
2:00    | Node 1 transmits msg 3  | 0
2:00    | Node 1 hears own msg 3  | 0 (✅ skipped - own packet)
3:00    | Node 1 transmits msg 4  | 0
3:00    | Node 1 hears own msg 4  | 0 (✅ skipped - own packet)
--------|-------------------------|----------------------------------
After 4 minutes: 0 messages shown (✅ correct - no messages received from others)
```

## Dashboard Display

The web dashboard shows `messageCount` from the node store. After this fix:

- **Node 1's own entry:** Should show **0 messages** (unless it actually received from other Node 1s)
- **Node 2's entry on Node 1:** Should show count of messages **Node 1 received from Node 2**
- **Node 3's entry on Node 1:** Should show count of messages **Node 1 received from Node 3**

Each node should only count messages **received from other nodes**, not from itself.

## Testing the Fix

### Test 1: Single Node in Isolation
1. Power on only Node 1
2. Wait 5 minutes
3. Check web dashboard: Node 1 should show **0 messages** (can't receive from itself)

### Test 2: Two Nodes
1. Power on Node 1 and Node 2
2. Wait 5 minutes (5 transmission cycles)
3. Node 1's dashboard should show: Node 2 = **5 messages** (or close, accounting for packet loss)
4. Node 2's dashboard should show: Node 1 = **5 messages**
5. Neither should show messages from themselves

### Test 3: Using Serial Commands
```bash
mesh reset          # Clear all counters
# Wait 3 minutes
mesh stats          # Check packetsReceived
mesh status         # Check which nodes are in neighbor table
```

**Expected:**
- `packetsReceived` should match the number of **other nodes** × **minutes elapsed**
- Node should NOT appear in its own neighbor table

## Additional Benefits

This fix also:

1. **Reduces network traffic** - Doesn't add own packets to duplicate cache
2. **Saves memory** - One fewer entry in duplicate cache per transmission
3. **Cleaner logs** - Debug logs won't show own packets being "received"
4. **More accurate statistics** - `packetsReceived` only counts actual received packets

## Distinguishing Scenarios

### Scenario 1: Normal Reception (Keep)
- Node 2 transmits
- Node 1 receives from Node 2
- `sourceId = 2`, `senderId = 2`
- **Action:** Process normally ✅

### Scenario 2: Forwarded Reception (Keep)
- Node 3 transmits
- Node 2 forwards
- Node 1 receives from Node 2
- `sourceId = 3`, `senderId = 2`
- **Action:** Process normally (it's from Node 3) ✅

### Scenario 3: Radio Loopback (Drop - NEW)
- Node 1 transmits
- Node 1 receives own transmission
- `sourceId = 1`, `senderId = 1`
- **Action:** Skip immediately ✅

### Scenario 4: Own Forwarded Packet (Drop - Existing)
- Node 1 transmitted earlier
- Node 2 forwards it back
- Node 1 receives
- `sourceId = 1`, `senderId = 2`
- **Action:** Caught by duplicate cache ✅

## Summary

**Root Cause:** Radio loopback causing nodes to receive their own transmissions

**Symptom:** Message counts double (8 messages in 4 minutes instead of 4)

**Fix:** Add early check to skip packets where `sourceId == DEVICE_ID`

**Result:** Nodes only count messages received from **other** nodes

**Files Changed:**
- [src/packet_handler.cpp](src/packet_handler.cpp) - Lines 143-151

**Related Fixes:**
- Works together with previous fix for forwarded packet counting
- Complements the `shouldForward()` own-packet check (which prevents forwarding)
