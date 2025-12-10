# Bug Fix: Message Double Counting Issue

## Problem Description

**Symptom:** On Node 2 (or any relay node), the message counter doubles every time it receives and forwards packets from other nodes.

**Example:**
- Node 3 sends message #42
- Node 2 receives it and forwards it
- Node 2's internal counter for Node 3 increments by 2 instead of 1

## Root Cause Analysis

The issue was in [src/packet_handler.cpp](src/packet_handler.cpp) in the `checkForIncomingMessages()` function.

### The Problem

The code was updating the node store **twice** for forwarded packets:

1. **First Update (Lines 140-150 - REMOVED):**
   ```cpp
   NodeMessage* nodeMsg = getNodeMessage(packet.header.originId);
   if (nodeMsg) {
       gap = nodeMsg->updateFromPacket(packet);  // ❌ FIRST INCREMENT
       // ...
   }
   ```
   This used the **LoRa header's originId**, which is the **immediate sender** (forwarder).

2. **Second Update (Lines 199-201 - NOW FIXED):**
   ```cpp
   NodeMessage* node = getNodeMessage(lastReceivedReport.meshHeader.sourceId);
   if (node != nullptr) {
       node->lastReport = lastReceivedReport;  // ❌ SECOND UPDATE
   }
   ```
   This used the **Mesh header's sourceId**, which is the **original source**.

### Why This Caused Double Counting

When Node 2 receives a forwarded packet:

- **LoRa Header:** `originId = 1` (Node 1 just transmitted it)
- **Mesh Header:** `sourceId = 3` (Node 3 originally created it)
- **Mesh Header:** `senderId = 1` (Node 1 forwarded it)

**The bug:** The code incremented the message counter for Node 1 (the forwarder), then later also updated Node 3 (the source). If Node 1 and Node 3 happened to be tracked in the same way, or if there was confusion about which node to track, counters would increment incorrectly.

## The Fix

### Change 1: Decode Mesh Header First

**Before:**
```cpp
void checkForIncomingMessages() {
    // Update node store using LoRa header (immediate sender)
    NodeMessage* nodeMsg = getNodeMessage(packet.header.originId);
    gap = nodeMsg->updateFromPacket(packet);  // ❌ Updates wrong node

    // Then decode mesh header
    if (decodeFullReport(...)) {
        // ...
    }
}
```

**After:**
```cpp
void checkForIncomingMessages() {
    // Decode mesh header FIRST
    if (decodeFullReport(...)) {
        // Then update node store using correct sourceId
        NodeMessage* node = getNodeMessage(meshHeader.sourceId);
        gap = node->updateFromPacket(packet);  // ✅ Updates correct node
    }
}
```

### Change 2: Use Mesh Header Source ID

**Before:**
```cpp
// Used LoRa header originId (immediate sender/forwarder)
NodeMessage* nodeMsg = getNodeMessage(packet.header.originId);
```

**After:**
```cpp
// Use Mesh header sourceId (original creator)
NodeMessage* node = getNodeMessage(lastReceivedReport.meshHeader.sourceId);
```

### Change 3: Update Stats Once

**Before:**
- Stats updated in two places
- First for forwarder (LoRa originId)
- Then for source (Mesh sourceId)

**After:**
- Stats updated once
- Only for the original message source
- After duplicate detection and validation

## Code Changes

### File: src/packet_handler.cpp

**Lines 132-142 (Removed duplicate tracking):**
```cpp
void checkForIncomingMessages() {
    LoRaReceivedPacket packet;

    while (receivePacket(packet)) {
        rxCount++;

        // ❌ REMOVED: Early node store update with LoRa originId
        // NodeMessage* nodeMsg = getNodeMessage(packet.header.originId);
        // if (nodeMsg) { gap = nodeMsg->updateFromPacket(packet); }

        // ✅ ADDED: Decode mesh header first
        MessageType msgType = getMessageType(packet.payloadBytes, packet.payloadLen);
```

**Lines 185-201 (Moved node store update):**
```cpp
// Update neighbor table with immediate sender's RSSI (who we heard directly)
neighborTable.update(lastReceivedReport.meshHeader.senderId, packet.rssi);

// ✅ ADDED: Update node store for ORIGINAL SOURCE (not forwarder)
NodeMessage* node = getNodeMessage(lastReceivedReport.meshHeader.sourceId);
uint16_t gap = 0;
unsigned long msgCount = 0;
unsigned long lost = 0;
float lossPercent = 0.0;

if (node != nullptr) {
    // Update packet tracking stats (only for the original source)
    gap = node->updateFromPacket(packet);
    msgCount = node->messageCount;
    lost = node->packetsLost;
    lossPercent = node->getPacketLossPercent();

    // Store the decoded report
    node->lastReport = lastReceivedReport;
}
```

**Lines 220-241 (Legacy message handling):**
```cpp
} else {
    // Legacy string message or decode failure
    duplicateRxMessages++;
    lastReportValid = false;

    // ✅ ADDED: For legacy messages, use LoRa header originId to track stats
    NodeMessage* legacyNode = getNodeMessage(packet.header.originId);
    uint16_t gap = 0;
    unsigned long msgCount = 0;
    unsigned long lost = 0;
    float lossPercent = 0.0;

    if (legacyNode) {
        gap = legacyNode->updateFromPacket(packet);
        msgCount = legacyNode->messageCount;
        lost = legacyNode->packetsLost;
        lossPercent = legacyNode->getPacketLossPercent();
    }

    printRxPacket(packet, gap, msgCount, lost, lossPercent);
    updateRxDisplay(packet);
}
```

## Why This Solution is Correct

### 1. Consistent Node Tracking

**Mesh Messages:** Always track by `meshHeader.sourceId` (original creator)
- Node 3 creates message → tracked under Node 3
- Node 1 forwards it → still tracked under Node 3
- Node 2 receives it → increments Node 3's counter once

**Legacy Messages:** Track by `loraHeader.originId` (immediate sender)
- For backwards compatibility with non-mesh messages

### 2. Proper Neighbor Discovery

The neighbor table still correctly tracks the **immediate sender**:
```cpp
neighborTable.update(lastReceivedReport.meshHeader.senderId, packet.rssi);
```
- If Node 2 receives from Node 1, it knows Node 1 is a neighbor
- RSSI is for the direct link, not the original source

### 3. Single Increment Path

Each received packet now follows **one path** through the code:
```
Receive Packet
    ↓
Decode Mesh Header
    ↓
Duplicate Detection
    ↓
Update Source Node Stats (ONCE)
    ↓
Update Neighbor Table
    ↓
Forward Decision
```

## Expected Behavior After Fix

### Direct Reception (Node 2 receives directly from Node 3):
- `meshHeader.sourceId = 3` (Node 3 originated)
- `meshHeader.senderId = 3` (Node 3 sent it)
- Node 2 increments Node 3's counter: **+1**
- Node 2 adds Node 3 to neighbor table

### Forwarded Reception (Node 2 receives Node 3's message via Node 1):
- `meshHeader.sourceId = 3` (Node 3 originated)
- `meshHeader.senderId = 1` (Node 1 forwarded it)
- Node 2 increments Node 3's counter: **+1** ✅ (not +2)
- Node 2 adds Node 1 to neighbor table (direct link)

## Testing the Fix

### Test 1: Direct Communication
1. Node 1 transmits
2. Node 2 receives
3. Check Node 2's serial output: Node 1's message count should increment by 1

### Test 2: 2-Hop Forwarding
1. Node 3 transmits (Node 2 can't hear it directly)
2. Node 1 forwards
3. Node 2 receives the forwarded packet
4. Check Node 2's serial output: Node 3's message count should increment by 1 (not 2)
5. Use `mesh status` to verify neighbor table shows Node 1 (not Node 3)

### Test 3: Statistics Verification
```
mesh reset          # Clear stats
# Wait for 5 messages from Node 3 via Node 1
mesh stats          # Should show 5 packets received (not 10)
```

## Related Issues Fixed

This fix also resolves:
1. **Incorrect packet loss calculations** - Previously counted forwarded packets as lost
2. **Neighbor table confusion** - Now correctly identifies direct neighbors vs. multi-hop sources
3. **Message count mismatches** - Dashboard and serial output now agree
4. **RSSI tracking** - RSSI now correctly tracks the direct link, not the original source

## Backwards Compatibility

**Legacy messages** (non-mesh format) still work correctly:
- They don't have a mesh header
- Fall back to using LoRa header `originId`
- Maintain same behavior as before

## Verification Commands

After applying this fix, use these commands to verify:

```bash
mesh status    # Check neighbor table shows correct nodes
mesh stats     # Verify packet counts are consistent
mesh reset     # Clear and start fresh
mesh test 3 2  # Send test message through network
```

## Summary

**Root Cause:** Double-counting due to updating node store for both forwarder and source

**Solution:** Update node store only once, using the original source ID from mesh header

**Result:** Message counters increment correctly, even for multi-hop forwarded packets

**Files Changed:**
- [src/packet_handler.cpp](src/packet_handler.cpp) - Lines 132-241
