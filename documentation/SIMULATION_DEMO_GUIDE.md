# ESP32 LoRa Mesh Network - Simulation Demo Guide

## Overview

This guide explains how to run the native desktop simulation of the ESP32 LoRa Mesh Network for demonstration and verification purposes. The simulation allows testing of mesh networking features without physical hardware.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Building the Simulation](#building-the-simulation)
3. [Running the Dashboard](#running-the-dashboard)
4. [Test Scenarios](#test-scenarios)
5. [Understanding the Output](#understanding-the-output)
6. [Demo Script for Video Recording](#demo-script-for-video-recording)
7. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Software Requirements

1. **PlatformIO** - For building the simulation
   - Install via VS Code extension or CLI
   - https://platformio.org/install

2. **Python 3.8+** - For running the dashboard bridge
   ```bash
   python --version
   ```

3. **Python Packages** - Install required packages:
   ```bash
   pip install websockets aiohttp
   ```

### Verify Installation

```bash
# Check PlatformIO
pio --version

# Check Python packages
python -c "import websockets; import aiohttp; print('OK')"
```

---

## Building the Simulation

### Step 1: Navigate to Project Directory

```bash
cd "C:\Users\Jonathan Ramirez\Desktop\CECS490A\Nov10_2025\ESP32-WIP"
```

### Step 2: Build the Native Simulation

```bash
pio run -e native_windows
```

**Expected Output:**
```
Processing native_windows (platform: native)
...
Linking .pio\build\native_windows\program.exe
========================= [SUCCESS] Took X.XX seconds =========================
```

### Step 3: Verify Build

The executable will be located at:
```
.pio\build\native_windows\program.exe
```

---

## Running the Dashboard

### Step 1: Start the Simulation Bridge

Open a terminal and run:

```bash
cd desktop_dashboard
python simulation_bridge.py
```

**Expected Output:**
```
============================================================
   LoRa Mesh Simulation Bridge
============================================================

UDP Listen Port: 8889
HTTP Server:     http://localhost:8080
WebSocket:       ws://localhost:8081

------------------------------------------------------------
Open http://localhost:8080 in your browser
------------------------------------------------------------
```

### Step 2: Open the Dashboard

Open a web browser and navigate to:
```
http://localhost:8080
```

The dashboard will display:
- Network topology visualization
- Node status cards
- Real-time packet events
- Console log output

---

## Test Scenarios

### Scenario 1: Multi-Hop Routing Test

**Purpose:** Demonstrate beacon propagation and route establishment across multiple nodes.

**Setup:** 3 nodes in a linear arrangement

| Node | Role | Position | Command |
|------|------|----------|---------|
| 1 | Gateway | (100, 300) | `program.exe 1 --pos 100 300` |
| 2 | Relay | (300, 300) | `program.exe 2 --pos 300 300` |
| 3 | End Node | (500, 300) | `program.exe 3 --pos 500 300` |

**Commands (run each in a separate terminal):**

```bash
# Terminal 1: Gateway
.pio\build\native_windows\program.exe 1 --pos 100 300

# Terminal 2: Relay Node
.pio\build\native_windows\program.exe 2 --pos 300 300

# Terminal 3: End Node
.pio\build\native_windows\program.exe 3 --pos 500 300
```

**What to Observe:**
1. Gateway sends BEACON packets every 10 seconds
2. Node 2 receives beacon, updates route (distance=1)
3. Node 2 rebroadcasts beacon
4. Node 3 receives beacon, updates route (distance=1 or 2)
5. Nodes send DATA packets to gateway
6. Gateway receives data from all nodes

---

### Scenario 2: Packet Loss Resilience Test

**Purpose:** Demonstrate network resilience under lossy conditions.

**Setup:** 3 nodes with 20% packet loss on the relay

| Node | Role | Packet Loss | Command |
|------|------|-------------|---------|
| 1 | Gateway | 0% | `program.exe 1 --pos 100 300` |
| 2 | Lossy Relay | 20% | `program.exe 2 --pos 300 300 --loss 0.2` |
| 3 | End Node | 0% | `program.exe 3 --pos 500 300` |

**Commands:**

```bash
# Terminal 1: Gateway
.pio\build\native_windows\program.exe 1 --pos 100 300

# Terminal 2: Lossy Relay (20% packet loss)
.pio\build\native_windows\program.exe 2 --pos 300 300 --loss 0.2

# Terminal 3: End Node
.pio\build\native_windows\program.exe 3 --pos 500 300
```

**What to Observe:**
1. Some beacons and data packets are dropped
2. Routes still establish (may take longer)
3. Network adapts and continues functioning
4. Console shows dropped packet messages

---

### Scenario 3: Dynamic Topology Test (Node Join/Leave)

**Purpose:** Demonstrate network adaptation when nodes join or leave.

**Phase 1: Start with 2 nodes**

```bash
# Terminal 1: Gateway
.pio\build\native_windows\program.exe 1 --pos 100 300

# Terminal 2: Node 2
.pio\build\native_windows\program.exe 2 --pos 300 300
```

Wait 30 seconds for routes to establish.

**Phase 2: Add Node 3**

```bash
# Terminal 3: Node 3 joins late
.pio\build\native_windows\program.exe 3 --pos 500 300
```

**What to Observe:**
1. Initial 2-node network establishes routes
2. Node 3 joins and discovers network via beacons
3. Node 3 establishes route and starts sending data
4. Dashboard updates to show new node

**Phase 3: Remove Node 2 (Ctrl+C in Terminal 2)**

**What to Observe:**
1. Node 2 disappears from network
2. After timeout (~30 seconds), Node 2 marked offline
3. If Node 3 was routing through Node 2, route expires
4. Node 3 may re-establish route directly to gateway

---

### Scenario 4: Star Topology (5 Nodes)

**Purpose:** Demonstrate scalability with multiple nodes.

**Setup:** Gateway in center, 4 nodes around it

```bash
# Terminal 1: Gateway (center)
.pio\build\native_windows\program.exe 1 --pos 300 300

# Terminal 2: Node 2 (top-left)
.pio\build\native_windows\program.exe 2 --pos 200 200

# Terminal 3: Node 3 (top-right)
.pio\build\native_windows\program.exe 3 --pos 400 200

# Terminal 4: Node 4 (bottom-left)
.pio\build\native_windows\program.exe 4 --pos 200 400

# Terminal 5: Node 5 (bottom-right)
.pio\build\native_windows\program.exe 5 --pos 400 400
```

**What to Observe:**
1. All nodes establish distance=1 to gateway
2. TDMA scheduling prevents collisions
3. Each node transmits in its assigned time slot
4. Gateway receives data from all 4 nodes

---

## Understanding the Output

### Console Log Messages

| Message | Meaning |
|---------|---------|
| `[TX] Gateway sending BEACON` | Gateway broadcasting routing beacon |
| `[RX] Node X received BEACON from Y` | Node received a beacon packet |
| `[ROUTE] Node X: Updated route` | Node updated its routing table |
| `[TX] Node X sending DATA` | Node transmitting sensor data |
| `[RX] Node X received DATA` | Node received a data packet |
| `[FWD] Node X forwarding DATA` | Node forwarding packet toward gateway |
| `[GATEWAY] Received data from node X` | Gateway successfully received data |
| `Dropped duplicate` | Duplicate packet detection working |
| `Route expired` | Route timeout (no recent beacons) |

### Statistics Output (Every 15 seconds)

```
--- Node X Statistics ---
[STATE] Node X: distance=1, nextHop=1, gateway=1, routeValid=yes
[NEIGHBORS] Node X has 2 neighbors:
  - Node 1: RSSI -95 (min:-100, max:-90), packets:10
  - Node 2: RSSI -85 (min:-90, max:-80), packets:5
  TX: 5, RX: 3, FWD: 2, Beacons: 8
--------------------------
```

| Field | Meaning |
|-------|---------|
| distance | Hop count to gateway (0=gateway) |
| nextHop | Node ID to forward packets to |
| routeValid | Whether route is currently active |
| RSSI | Signal strength in dBm (higher = better) |
| TX | Packets transmitted |
| RX | Data packets received |
| FWD | Packets forwarded |
| Beacons | Beacon packets processed |

### Dashboard Elements

1. **Node Cards** - Show each node's status, temperature, humidity
2. **Topology View** - Visual representation of network connections
3. **Packet Events** - Real-time feed of sent/received packets
4. **Console** - Raw log output from all nodes

---

## Demo Script for Video Recording

### Preparation (Before Recording)

1. Close all unnecessary applications
2. Build the simulation: `pio run -e native_windows`
3. Open 4 terminal windows, arrange them on screen
4. Open browser with dashboard ready (http://localhost:8080)
5. Position windows for visibility

### Recording Script

#### Part 1: Introduction (1 minute)

"This demonstration shows our ESP32 LoRa Mesh Network running in simulation mode. The simulation uses UDP multicast to simulate radio communication between nodes, allowing us to test the mesh networking protocol without physical hardware."

#### Part 2: Start the Dashboard (30 seconds)

1. Show the terminal
2. Run: `cd desktop_dashboard && python simulation_bridge.py`
3. Show the browser dashboard loading
4. Explain: "The dashboard provides real-time visualization of the mesh network"

#### Part 3: Multi-Hop Routing Demo (3 minutes)

1. Start Gateway (Node 1):
   ```bash
   .pio\build\native_windows\program.exe 1 --pos 100 300
   ```
   - Show gateway starting
   - Point out: "Node 1 is the gateway - it has distance 0 to itself"

2. Start Node 2:
   ```bash
   .pio\build\native_windows\program.exe 2 --pos 300 300
   ```
   - Show beacon reception
   - Point out: "Node 2 receives the beacon and establishes a route with distance 1"

3. Start Node 3:
   ```bash
   .pio\build\native_windows\program.exe 3 --pos 500 300
   ```
   - Show route establishment
   - Point out: "Node 3 joins the network and establishes its route"

4. Show the dashboard:
   - Point out the topology visualization
   - Show real-time packet events
   - Highlight data flowing from nodes to gateway

5. Point out key features:
   - "BEACON packets propagate routing information"
   - "DATA packets contain sensor readings"
   - "Duplicate detection prevents packet loops"

#### Part 4: Packet Loss Resilience (2 minutes)

1. Stop all nodes (Ctrl+C)
2. Restart with packet loss:
   ```bash
   .pio\build\native_windows\program.exe 2 --pos 300 300 --loss 0.2
   ```
3. Show: "Even with 20% packet loss, the network still functions"
4. Point out dropped packet messages in console

#### Part 5: Dynamic Topology (2 minutes)

1. With network running, stop Node 2 (Ctrl+C)
2. Show: "Watch how the network detects the node going offline"
3. Wait for timeout message
4. Restart Node 2
5. Show: "The node rejoins and re-establishes its route"

#### Part 6: Conclusion (30 seconds)

"This simulation validates our mesh networking protocol including:
- Gradient routing with beacon propagation
- Multi-hop packet forwarding
- Duplicate detection
- Dynamic topology adaptation
- Packet loss resilience

The same protocol runs on actual ESP32 hardware with LoRa radios."

---

## Command Reference

### Simulation Executable

```
program.exe <node_id> [options]

Arguments:
  node_id          Node ID (1-255, 1 = gateway)

Options:
  --pos X Y        Set virtual position (default: circular layout)
  --loss RATE      Packet loss rate 0.0-1.0 (default: 0.0)
  --latency MS     Transmission latency in ms (default: 0)
  --help           Show help message
```

### Examples

```bash
# Gateway at position (0, 0)
program.exe 1 --pos 0 0

# Node with 10% packet loss
program.exe 2 --loss 0.1

# Node with 50ms latency
program.exe 3 --latency 50

# Combine options
program.exe 4 --pos 200 100 --loss 0.05 --latency 20
```

---

## Troubleshooting

### "Port already in use" Error

```bash
# Windows: Find and kill process using port 8080
netstat -ano | findstr :8080
taskkill /PID <pid> /F
```

### Nodes Not Receiving Packets

1. Check Windows Firewall settings
2. Ensure all nodes are using same multicast group (239.0.0.1:8888)
3. Try restarting all nodes

### Build Errors

```bash
# Clean and rebuild
pio run -e native_windows -t clean
pio run -e native_windows
```

### Dashboard Not Updating

1. Check browser console for WebSocket errors
2. Verify simulation_bridge.py is running
3. Refresh the browser page

---

## Files Reference

| File | Purpose |
|------|---------|
| `.pio/build/native_windows/program.exe` | Simulation executable |
| `desktop_dashboard/simulation_bridge.py` | Dashboard WebSocket server |
| `desktop_dashboard/dashboard.html` | Web dashboard UI |
| `include/simulation/sim_routing.h` | Routing protocol implementation |
| `src/simulation/simulation_main.cpp` | Simulation entry point |
| `src/simulation/SimulatedRadio.cpp` | UDP multicast radio simulation |

---

## Network Protocol Summary

### Packet Types

| Type | Code | Purpose |
|------|------|---------|
| BEACON | 0x0A | Routing information propagation |
| DATA | 0x01 | Sensor data transmission |
| ACK | 0x20 | Acknowledgment (optional) |

### Routing Algorithm

1. **Gateway** broadcasts BEACON with distance=0
2. **Nodes** receive beacon, set distance = sender_distance + 1
3. **Nodes** rebroadcast beacon with updated distance
4. **Nodes** select next-hop based on lowest distance (best RSSI as tiebreaker)
5. **Data packets** forwarded hop-by-hop toward gateway

### Timing Parameters

| Parameter | Value | Purpose |
|-----------|-------|---------|
| Beacon Interval | 10 seconds | Gateway beacon frequency |
| Route Timeout | 30 seconds | Route expiration without beacon |
| Neighbor Timeout | 60 seconds | Stale neighbor pruning |
| TDMA Slot | 6 seconds | Per-node transmission window |

---

*Document created: December 2024*
*Project: ESP32 LoRa Mesh Network - CECS 490A*
