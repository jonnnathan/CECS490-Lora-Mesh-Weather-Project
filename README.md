# ESP32 LoRa Mesh Network

> **A multi-hop wireless sensor network using LoRa radio technology, GPS synchronization, and gradient-based routing**

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Radio](https://img.shields.io/badge/Radio-LoRa%20SX1262-green.svg)](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

---

## Table of Contents

1. [Overview](#1-overview)
   - [What is this project?](#what-is-this-project)
   - [Key Features](#key-features)
   - [System Architecture](#system-architecture)
2. [Hardware Requirements](#2-hardware-requirements)
   - [Components List](#components-list)
   - [Wiring Diagrams](#wiring-diagrams)
   - [Pin Assignments](#pin-assignments)
3. [Software Setup](#3-software-setup)
   - [Prerequisites](#prerequisites)
   - [Installation](#installation)
   - [Configuration](#configuration)
4. [Network Architecture](#4-network-architecture)
   - [Mesh Topology](#mesh-topology)
   - [Message Protocol](#message-protocol)
   - [TDMA Scheduling](#tdma-scheduling)
   - [Network Time Synchronization](#network-time-synchronization)
   - [Gradient Routing](#gradient-routing)
5. [Dashboards](#5-dashboards)
   - [ESP32 Web Dashboard](#esp32-web-dashboard)
   - [Desktop Dashboard](#desktop-dashboard)
   - [WiFi Modes](#wifi-modes)
6. [Serial Commands](#6-serial-commands)
7. [Testing Guide](#7-testing-guide)
8. [Troubleshooting](#8-troubleshooting)
9. [API Reference](#9-api-reference)
10. [Contributing](#10-contributing)
11. [License](#11-license)

---

## 1. Overview

### What is this project?

This project implements a **self-organizing wireless mesh network** using ESP32 microcontrollers with LoRa radios. Multiple sensor nodes collect environmental data (temperature, humidity, pressure, GPS location) and transmit it across the network to a gateway node, which displays the data on a web dashboard and optionally uploads it to ThingSpeak.

```
                           ┌─────────────┐
                           │   Gateway   │
                           │   (Node 1)  │
                           │  Dashboard  │
                           └──────┬──────┘
                                  │
              ┌───────────────────┼───────────────────┐
              │                   │                   │
       ┌──────┴──────┐     ┌──────┴──────┐     ┌──────┴──────┐
       │   Node 2    │     │   Node 3    │     │   Node 4    │
       │   Relay     │     │   Sensor    │     │   Sensor    │
       │  + Sensor   │     │    Only     │     │    Only     │
       └──────┬──────┘     └─────────────┘     └─────────────┘
              │
       ┌──────┴──────┐
       │   Node 5    │
       │   Remote    │
       │   Sensor    │
       └─────────────┘
```

### Key Features

| Feature | Description |
|---------|-------------|
| **Multi-hop Mesh** | Messages automatically route through intermediate nodes to reach the gateway |
| **GPS Time Sync** | All nodes synchronize using GPS for collision-free TDMA transmission |
| **Network Time Sync** | Nodes without GPS can use beacon time from gateway/relays for TDMA |
| **Gradient Routing** | Efficient routing using hop-count gradients (64% less bandwidth than flooding) |
| **Web Dashboard** | Real-time visualization of node data, maps, and signal strength |
| **Duplicate Detection** | Prevents message loops with intelligent caching |
| **Thread-Safe** | ISR-protected packet reception with proper mutex handling |
| **Offline Operation** | Access Point mode works without internet |

### System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              APPLICATION LAYER                               │
├─────────────────────────────────────────────────────────────────────────────┤
│  main.cpp          │  TDMA Scheduler    │  Web Dashboard    │  ThingSpeak   │
│  - Sensor reading  │  - GPS time sync   │  - HTTP server    │  - Cloud data │
│  - Display update  │  - Slot allocation │  - WebSocket      │  - History    │
│  - Beacon TX       │  - Network time    │  - Status API     │  - Charts     │
└────────────────────┴────────────────────┴───────────────────┴───────────────┘
                                    │
┌─────────────────────────────────────────────────────────────────────────────┐
│                               MESH LAYER                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│  packet_handler    │  gradient_routing  │  neighbor_table   │  network_time │
│  - RX processing   │  - Beacon handling │  - RSSI tracking  │  - Time sync  │
│  - TX scheduling   │  - Route selection │  - Node discovery │  - Multi-hop  │
│  - Forward logic   │  - Time relay      │  - Link quality   │  - Fallback   │
└────────────────────┴────────────────────┴───────────────────┴───────────────┘
                                    │
┌─────────────────────────────────────────────────────────────────────────────┐
│                               RADIO LAYER                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│  lora_comm.cpp     │  SX1262 Driver     │  mesh_protocol.h                  │
│  - Packet TX/RX    │  - RadioLib        │  - Header format                  │
│  - ISR handling    │  - 915 MHz config  │  - Message types                  │
│  - RSSI/SNR        │  - Spreading Factor│  - TTL management                 │
└────────────────────┴────────────────────┴───────────────────────────────────┘
```

---

## 2. Hardware Requirements

### Components List

| Component | Model | Quantity | Purpose |
|-----------|-------|----------|---------|
| **Microcontroller** | Heltec WiFi Kit 32 V3 | 3-5 | Main processing, LoRa, OLED |
| **GPS Module** | NEO-6M | 3-5 | Time synchronization, location |
| **Sensor** | SHT30 (I2C) | 3-5 | Temperature & humidity |
| **Antenna** | 915 MHz LoRa | 3-5 | Extended range communication |
| **Power** | USB-C or LiPo | 3-5 | Power supply |

### Wiring Diagrams

#### GPS Module Connection (NEO-6M)

```
NEO-6M GPS          Heltec ESP32
┌─────────┐         ┌─────────┐
│   VCC   │─────────│   3.3V  │
│   GND   │─────────│   GND   │
│   TX    │─────────│  GPIO46 │ (RX2)
│   RX    │─────────│  GPIO45 │ (TX2) [Optional]
└─────────┘         └─────────┘
```

#### SHT30 Sensor Connection (I2C)

```
SHT30 Sensor        Heltec ESP32
┌─────────┐         ┌─────────┐
│   VCC   │─────────│   3.3V  │
│   GND   │─────────│   GND   │
│   SDA   │─────────│  GPIO41 │ (SDA)
│   SCL   │─────────│  GPIO42 │ (SCL)
└─────────┘         └─────────┘
```

### Pin Assignments

| Function | GPIO | Notes |
|----------|------|-------|
| **LoRa NSS** | 8 | SPI Chip Select |
| **LoRa SCK** | 9 | SPI Clock |
| **LoRa MOSI** | 10 | SPI Data Out |
| **LoRa MISO** | 11 | SPI Data In |
| **LoRa RST** | 12 | Radio Reset |
| **LoRa DIO1** | 14 | Radio Interrupt |
| **LoRa BUSY** | 13 | Radio Busy |
| **GPS RX** | 46 | GPS TX → ESP32 RX |
| **GPS TX** | 45 | ESP32 TX → GPS RX (optional) |
| **I2C SDA** | 41 | Sensor Data |
| **I2C SCL** | 42 | Sensor Clock |
| **OLED SDA** | 17 | Display Data (built-in) |
| **OLED SCL** | 18 | Display Clock (built-in) |
| **OLED RST** | 21 | Display Reset (built-in) |

---

## 3. Software Setup

### Prerequisites

1. **PlatformIO** - Install via VS Code extension or CLI
2. **Python 3.8+** - For desktop dashboard (optional)
3. **Git** - For cloning and version control

### Installation

```bash
# Clone the repository
git clone https://github.com/yourusername/ESP32-LoRa-Mesh.git
cd ESP32-LoRa-Mesh

# Install PlatformIO dependencies
pio pkg install

# Build the project
pio run

# Upload to ESP32 (connect via USB)
pio run --target upload
```

### Configuration

Edit `src/config.cpp` for each node:

```cpp
// ═══════════════════════════════════════════════════════════════════════════
//                         CHANGE THESE FOR EACH NODE
// ═══════════════════════════════════════════════════════════════════════════

const uint8_t DEVICE_ID = 1;           // Unique ID: 1, 2, 3, 4, or 5
const char* const DEVICE_NAME = "DEV1"; // Display name

// Gateway Configuration
const uint8_t GATEWAY_NODE_ID = 1;     // Which node hosts the dashboard
// IS_GATEWAY is automatically set: true if DEVICE_ID == GATEWAY_NODE_ID

// Timezone
const int8_t UTC_OFFSET_HOURS = -8;    // PST = -8, EST = -5
```

#### WiFi Configuration

```cpp
// ═══════════════════════════════════════════════════════════════════════════
//                         WIFI CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

// Option 1: Access Point Mode (works offline)
const bool WIFI_USE_STATION_MODE = false;
// Creates network: "LoRa_Mesh" with password "mesh1234"
// Access at: http://192.168.4.1

// Option 2: Station Mode (join existing WiFi)
const bool WIFI_USE_STATION_MODE = true;
const char* WIFI_STA_SSID = "YourWiFiName";
const char* WIFI_STA_PASSWORD = "YourPassword";
// Access at: IP shown in Serial Monitor
```

---

## 4. Network Architecture

### Mesh Topology

The network uses a **flooding with gradient routing** approach:

```
                    GATEWAY (distance=0)
                          │
           ┌──────────────┼──────────────┐
           │              │              │
        Node 2         Node 3         Node 4
      (distance=1)   (distance=1)   (distance=1)
           │              │
        Node 5         Node 6
      (distance=2)   (distance=2)
```

**How it works:**
1. Gateway broadcasts **beacons** every 30 seconds with distance=0
2. Nodes receive beacons and calculate their distance (received_distance + 1)
3. Nodes rebroadcast beacons with their new distance
4. Each node tracks the **best route** (lowest hops, best RSSI as tiebreaker)
5. Data packets flow **upstream** toward the gateway using stored routes

### Message Protocol

All messages use an 8-byte header:

```
┌────────┬────────┬────────┬────────┬────────┬────────┬────────┬────────┐
│ Version│ MsgType│ SourceID│ DestID │ SenderID│ MsgID  │  TTL   │ Flags  │
│ 1 byte │ 1 byte │ 1 byte │ 1 byte │ 1 byte │ 1 byte │ 1 byte │ 1 byte │
└────────┴────────┴────────┴────────┴────────┴────────┴────────┴────────┘
                              MESH HEADER (8 bytes)
```

**Field Descriptions:**

| Field | Size | Description |
|-------|------|-------------|
| `version` | 1B | Protocol version (currently 1) |
| `messageType` | 1B | Type: FULL_REPORT (0x01), BEACON (0x0A), etc. |
| `sourceId` | 1B | Original creator's node ID (never changes) |
| `destId` | 1B | Final destination (0xFF = broadcast, 0x00 = gateway) |
| `senderId` | 1B | Current transmitter's ID (changes each hop) |
| `messageId` | 1B | Sequence number for duplicate detection |
| `ttl` | 1B | Time-to-live (hops remaining, default=3) |
| `flags` | 1B | Bit 0: needs ACK, Bit 1: is forwarded |

### TDMA Scheduling

GPS-synchronized Time Division Multiple Access prevents collisions:

```
Time (seconds)     0    6   12   18   24   30   36   42   48   54
                   │    │    │    │    │    │    │    │    │    │
Node 1 (Gateway)   │████│    │    │    │    │    │████│    │    │
Node 2             │    │████│    │    │    │    │    │████│    │
Node 3             │    │    │████│    │    │    │    │    │████│
Node 4             │    │    │    │████│    │    │    │    │    │
Node 5             │    │    │    │    │████│    │    │    │    │
                   └────────────────────────────────────────────┘
                              One TDMA Cycle (60 seconds)
```

**Slot Allocation:**
- Each node gets a 12-second time slot
- Transmissions occur at specific seconds within the slot
- Forward transmissions happen 100ms after primary transmission
- GPS time ensures all nodes are synchronized (< 1 second drift)
- Nodes without GPS use **network time** from beacons as fallback (see [Network Time Synchronization](#network-time-synchronization))

### Network Time Synchronization

Nodes without GPS lock can still participate in TDMA using **network time synchronization**:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          TIME SOURCE PRIORITY                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   Priority 1: Own GPS      (accuracy: ~1μs)      ← Best, used if available  │
│   Priority 2: Network Time (accuracy: ~200-500ms) ← Fallback from beacons   │
│   Priority 3: None         (cannot transmit)      ← Safety mode             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**How it works:**

1. **Gateway broadcasts time** - The gateway includes GPS time (hour, minute, second) in every beacon message
2. **Nodes extract time** - When a node receives a beacon, it extracts the time and stores it
3. **Time extrapolation** - The node tracks when the beacon was received and calculates current time from elapsed milliseconds
4. **TDMA fallback** - Nodes without GPS use network time for slot calculation

#### Multi-hop Time Relay

Nodes can receive time from intermediate relays, not just directly from the gateway:

```
Gateway (GPS)  ──beacon──>  Node 2  ──rebroadcast──>  Node 3
  hop=0                     hop=1                      hop=2
  (GPS time)               (relays time)            (uses time)
```

**Hop Count Preference:**
- Lower hop count = more accurate time (less propagation delay)
- Nodes prefer time from sources with fewer hops
- If current time is >30 seconds old, any fresh source is accepted

```
Time Source Selection Logic:
┌────────────────────────────────────────────────────────────┐
│ Accept new time if:                                        │
│   1. No valid time currently, OR                           │
│   2. New source has lower/equal hop count, OR              │
│   3. Current time is >30 seconds old                       │
└────────────────────────────────────────────────────────────┘
```

**Beacon Message with Time (16 bytes):**

```
┌────────┬────────┬────────┬────────┬────────┬────────┬────────┬────────┐
│ Header │distance│  Node  │  Temp  │ Humid  │ GPS    │ GPS    │ GPS    │
│(8 byte)│ 1 byte │ Count  │ 2 bytes│ 2 bytes│ Hour   │ Minute │ Second │
│        │        │ 1 byte │        │        │ 1 byte │ 1 byte │ 1 byte │
└────────┴────────┴────────┴────────┴────────┴────────┴────────┴────────┘
                    BEACON MESSAGE (16 bytes total)

+ gpsValid (1 byte) - indicates if time fields contain valid GPS time
```

**Serial Output Example:**

```
[NET-TIME] Time updated: 23:50:15 from Node 1 (hop 1)
[NET-TIME] Switching from 2-hop to 1-hop source
```

### Gradient Routing

Traditional flooding vs gradient routing:

```
FLOODING (before):                    GRADIENT (after):

    N3──┐      ┌──N4                     N3       N4
        │      │                          \       /
    N2──┴──GW──┴──N5                   N2──┴─GW─┴──N5
        │      │                              │
    N6──┘      └──N7                         N6
                                              │
Each packet: 7 transmissions              N7 (routes via N6)

                                         Each packet: ~3 transmissions
                                         64% bandwidth reduction!
```

**Benefits:**
- 64% fewer transmissions
- Lower power consumption
- Reduced channel congestion
- Predictable routing paths

---

## 5. Dashboards

### ESP32 Web Dashboard

The gateway node hosts a web server with two modes:

#### Lite Dashboard (Access Point Mode)

**Best for:** Field deployment, outdoor testing, no internet required

```
Connect to WiFi: "LoRa_Mesh"
Password: "mesh1234"
Open: http://192.168.4.1
```

**Features:**
- Ultra-lightweight (~5KB HTML)
- Works 100% offline
- Auto-refresh every 3 seconds
- Node status cards
- Temperature, humidity, GPS data
- RSSI signal strength

#### Full Dashboard (Station Mode)

**Best for:** Indoor demos, full visualization

```
Configure WiFi in config.cpp
Check Serial Monitor for IP
Open: http://[IP_ADDRESS]
```

**Features:**
- Interactive map with node markers
- Signal strength heatmap
- Mesh topology visualization
- ThingSpeak history charts
- Real-time WebSocket updates

### Desktop Dashboard

A Python-based alternative that runs on your PC:

```
┌──────────────┐     USB/Serial      ┌──────────────────────────┐
│  ESP32       │ ──────────────────> │  Python Bridge           │
│  Gateway     │   JSON data stream  │  serial_bridge.py        │
│              │                     │                          │
│  - LoRa RX   │                     │  - WebSocket server:8081 │
│  - No WiFi!  │                     │  - HTTP dashboard :8080  │
└──────────────┘                     │  - ThingSpeak upload     │
                                     └────────────┬─────────────┘
                                                  │
                                     ┌────────────┴─────────────┐
                                     │  Browser: localhost:8080 │
                                     └──────────────────────────┘
```

**Installation:**

```bash
cd desktop_dashboard
pip install -r requirements.txt
python serial_bridge.py --port COM3    # Windows
python serial_bridge.py --port /dev/ttyUSB0  # Linux
```

**Features:**
- Real-time node data cards with sensor readings
- Interactive map with GPS markers
- **Network Topology Visualization** (vis.js)
- RSSI signal quality indicators
- ThingSpeak integration

**Benefits:**
- No WiFi configuration on ESP32
- Works with enterprise WiFi (eduroam)
- No memory constraints
- Full-featured dashboard

#### Topology Visualization Node Colors

The network topology view uses colors to indicate each node's time synchronization source:

| Color | Meaning |
|-------|---------|
| 🟢 **Green** | Gateway node |
| 🔵 **Blue** | GPS time source (direct satellite lock) |
| 🟠 **Orange** | Network time source (synced via beacon) |
| ⚫ **Gray** | No time source available |
| 🔴 **Red** | Node offline (no data received recently) |

Edge labels display RSSI values between connected nodes.

### WiFi Modes Comparison

| Feature | Access Point | Station Mode | Desktop |
|---------|--------------|--------------|---------|
| **Internet Required** | No | Yes | Your PC's |
| **Works Outdoors** | Yes | Phone hotspot | No |
| **Setup Complexity** | Simple | Medium | Medium |
| **Map Features** | No | Yes | Yes |
| **Memory Usage** | Low | High | N/A |
| **Enterprise WiFi** | N/A | Complex | Works |

---

## 6. Serial Commands

Connect via Serial Monitor at 115200 baud:

### `mesh status`

Display network status:

```
╔═══════════════════════════════════════════════════════════════╗
║  NEIGHBOR TABLE                                               ║
╚═══════════════════════════════════════════════════════════════╝
Active Neighbors: 3 / 10

┌──────┬─────────┬─────────┬─────────┬──────────┬──────────┐
│ Node │  RSSI   │   Min   │   Max   │ Packets  │ Last Seen│
├──────┼─────────┼─────────┼─────────┼──────────┼──────────┤
│   2  │ -68 dBm │ -75 dBm │ -62 dBm │   42     │   5s ago │
│   3  │ -72 dBm │ -80 dBm │ -65 dBm │   38     │   3s ago │
└──────┴─────────┴─────────┴─────────┴──────────┴──────────┘
```

### `mesh stats`

Display statistics:

```
╔═══════════════════════════════════════════════════════════════╗
║                    MESH NETWORK STATISTICS                    ║
╠═══════════════════════════════════════════════════════════════╣
║  RECEPTION:                                                   ║
║    Packets Received:      156                                 ║
║    Duplicates Dropped:    12  (7.1%)                          ║
║                                                               ║
║  TRANSMISSION:                                                ║
║    Own Packets Sent:      145                                 ║
║    Packets Forwarded:     23                                  ║
║                                                               ║
║  Uptime: 02h 15m 42s                                          ║
╚═══════════════════════════════════════════════════════════════╝
```

### `mesh test [dest] [ttl] [msg]`

Send a test message:

```bash
mesh test           # Broadcast with TTL=3
mesh test 3 2       # Send to Node 3 with TTL=2
mesh test 255 3 Hi  # Broadcast "Hi" with TTL=3
```

### `mesh memory`

Display memory status:

```
╔═══════════════════════════════════════════════════════════════╗
║                    MEMORY STATUS REPORT                       ║
╠═══════════════════════════════════════════════════════════════╣
║  HEAP STATUS:                                                 ║
║    Free Heap:          45672 bytes (44.60 KB)                 ║
║    Min Free Heap:      38912 bytes (38.00 KB)                 ║
║                                                               ║
║  MESH SUBSYSTEM MEMORY:                                       ║
║    Neighbor Table:     256 bytes                              ║
║    Duplicate Cache:    384 bytes                              ║
║    Transmit Queue:     576 bytes                              ║
║    Total Mesh:         2716 bytes (2.65 KB)                   ║
╚═══════════════════════════════════════════════════════════════╝
```

### `mesh reset`

Clear all caches and reset statistics.

### `mesh help`

Display command reference.

---

## 7. Testing Guide

### Test 1: Basic Communication

1. Configure two nodes (IDs 1 and 2)
2. Power on both nodes, wait for GPS lock
3. Check Serial Monitor for received packets

**Expected:** Node 1 receives Node 2's data and vice versa.

### Test 2: 2-Hop Forwarding

```
Node 3 ←──→ Node 2 ←──→ Node 1 (Gateway)
 Source       Relay       Destination
```

1. Position Node 3 out of range of Node 1
2. Position Node 2 between them
3. Send from Node 3: `mesh test 1 3 ForwardTest`

**Expected Output on Node 2:**
```
[RX] Packet from Node 3 via Node 3 | msgId=42 ttl=3
[FWD] FORWARD: Node 3 msg=42 ttl=3 -> Enqueuing
```

**Expected Output on Node 1:**
```
[RX] Packet from Node 3 via Node 2 | msgId=42 ttl=2 (FORWARDED)
```

### Test 3: Duplicate Detection

1. Create a network with multiple paths
2. Send a broadcast from any node
3. Verify duplicates are dropped

**Expected:**
```
🚫 Duplicate mesh message from Node 3 msg #42 (dropped)
```

### Test 4: Gradient Routing

1. Power on gateway first
2. Check other nodes receive beacons
3. Run `mesh status` to verify route established

**Expected:**
```
╔═══════════════════════════════════════════════════════════════╗
║               GRADIENT ROUTING TABLE                          ║
╚═══════════════════════════════════════════════════════════════╝
  Route Valid: YES
  Distance to Gateway: 2 hops
  Next Hop: Node 2
  Best RSSI: -72 dBm
```

---

## 8. Troubleshooting

### No GPS Fix

**Symptoms:** Display shows "GPS?" or "Wait GPS"

**Solutions:**
- Move device outdoors or near window
- Wait 30-60 seconds for initial acquisition
- Check GPS TX wire connects to ESP32 GPIO 46
- Verify GPS module LED blinks

### No Radio Communication

**Symptoms:** Nodes don't see each other

**Solutions:**
- Verify antenna is connected
- Check LoRa frequency matches (915 MHz)
- Reduce distance between nodes
- Check RSSI values (`mesh status`)

### Dashboard Not Loading

**Access Point Mode:**
- Connect to "LoRa_Mesh" WiFi
- Use `http://192.168.4.1` (not HTTPS)
- Check Serial Monitor for "AP Started"

**Station Mode:**
- Verify WiFi credentials
- ESP32 only supports 2.4 GHz
- Check Serial Monitor for assigned IP

### High Duplicate Rate

**Symptoms:** `mesh stats` shows >20% duplicates

**Normal in dense networks.** Consider:
- Reducing TTL
- Using gradient routing
- Spacing nodes further apart

### Memory Issues

**Symptoms:** Random crashes, "Free heap low" warnings

**Solutions:**
- Use Lite Dashboard (AP mode)
- Reduce `MAX_NEIGHBORS`, `TX_QUEUE_SIZE`
- Run `mesh memory` to monitor

### Node Not Transmitting (No Time Source)

**Symptoms:** Display shows "SRC: NONE" or node never transmits

**Possible Causes:**
1. **No GPS satellites** - GPS cached time but no actual lock
2. **No beacon received** - Network time unavailable

**Solutions:**
- Check GPS has satellites: Display should show "SRC: GPS"
- Ensure gateway is running and broadcasting beacons
- Move node within range of gateway or relay node
- Check Serial Monitor for `[NET-TIME]` messages
- Verify beacon contains valid time: `gpsValid=1`

**Diagnosis via Serial:**
```
[NET-TIME] Time updated: 23:50:15 from Node 1 (hop 1)  ← Good
[NET-TIME] Network time expired (no recent beacon)     ← No time source
```

---

## 9. API Reference

### Message Types

```cpp
enum MessageType : uint8_t {
    MSG_FULL_REPORT = 0x01,  // Sensor + GPS data (38 bytes)
    MSG_BEACON      = 0x0A,  // Routing beacon with time sync (16 bytes)
    MSG_ACK         = 0x03,  // Acknowledgment
    MSG_TEXT        = 0x08,  // Text message
};
```

### Configuration Constants

```cpp
// Network
#define MESH_MAX_NODES              5
#define MESH_DEFAULT_TTL            3
#define MESH_PROTOCOL_VERSION       1

// Routing
const bool USE_GRADIENT_ROUTING = true;
const unsigned long BEACON_INTERVAL_MS = 30000;
const unsigned long ROUTE_TIMEOUT_MS = 60000;

// Timing
const unsigned long NODE_TIMEOUT_MS = 60000;
const unsigned long DUPLICATE_TIMEOUT_MS = 120000;
```

### JSON Output Format (Serial)

```json
{"type":"node_data","nodeId":3,"temp":72.5,"humidity":45.2,
 "lat":33.783,"lng":-118.114,"rssi":-65,"hopDistance":2,
 "timeSource":"GPS"}

{"type":"mesh_stats","packetsReceived":150,"packetsSent":45,
 "packetsForwarded":30,"duplicatesDropped":5}

{"type":"gateway_status","nodeId":1,"uptime":3600,"freeHeap":150000}

{"type":"beacon","senderId":2,"distance":1,"rssi":-55}
```

**Time Source Values:**
| Value | Description |
|-------|-------------|
| `"GPS"` | Node has direct GPS time lock |
| `"NET"` | Node using network time from beacon |
| `"NONE"` | No time source available |

---

## 10. Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Style

- Use descriptive variable names
- Add comments for complex logic
- Follow existing formatting
- Test on hardware before submitting

---

## 11. License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Appendix A: Signal Strength Reference

| RSSI (dBm) | Quality | Description |
|------------|---------|-------------|
| > -60 | Excellent | Very strong signal, reliable |
| -60 to -70 | Good | Strong signal |
| -70 to -80 | Fair | Moderate signal, may see occasional issues |
| -80 to -90 | Weak | Poor signal, expect packet loss |
| < -90 | Very Weak | Unreliable, at edge of range |

## Appendix B: Debug Logging

Enable/disable debug categories in `include/mesh_debug.h`:

```cpp
#define DEBUG_MESH_ENABLED  true   // Master switch
#define DEBUG_MESH_RX       true   // Packet reception
#define DEBUG_MESH_TX       true   // Packet transmission
#define DEBUG_MESH_FORWARD  true   // Forwarding decisions
#define DEBUG_MESH_DUPLICATE true  // Duplicate detection
#define DEBUG_MESH_NEIGHBOR true   // Neighbor table
#define DEBUG_MESH_QUEUE    true   // Queue operations
```

**Log Format:**
```
[CATEGORY] Description | key1=value1 key2=value2
```

**Example:**
```
[RX] Packet from Node 3 via Node 2 | msgId=42 ttl=2 rssi=-75
[FWD] FORWARD: Node 3 msg=42 ttl=2 -> Enqueuing
[QUE] Enqueue success | depth=1/8
```

## Appendix C: File Structure

```
ESP32-WIP/
├── include/
│   ├── config.h              # Configuration declarations
│   ├── mesh_protocol.h       # Protocol definitions
│   ├── mesh_debug.h          # Debug logging macros
│   ├── lora_comm.h           # Radio communication
│   ├── packet_handler.h      # Packet processing
│   ├── gradient_routing.h    # Routing algorithm
│   ├── neighbor_table.h      # Neighbor tracking
│   ├── duplicate_cache.h     # Duplicate detection
│   ├── transmit_queue.h      # TX queue management
│   ├── tdma_scheduler.h      # Time slot scheduling
│   ├── network_time.h        # Network time synchronization
│   ├── neo6m.h               # GPS module interface
│   ├── web_dashboard.h       # Full web dashboard
│   └── display_manager.h     # OLED display
├── src/
│   ├── main.cpp              # Main application
│   ├── config.cpp            # Configuration values
│   ├── lora_comm.cpp         # Radio implementation
│   ├── packet_handler.cpp    # Packet processing
│   ├── gradient_routing.cpp  # Routing implementation
│   ├── neighbor_table.cpp    # Neighbor tracking
│   ├── duplicate_cache.cpp   # Duplicate detection
│   ├── transmit_queue.cpp    # TX queue
│   ├── tdma_scheduler.cpp    # TDMA scheduling
│   ├── network_time.cpp      # Network time sync implementation
│   ├── web_dashboard.cpp     # Full dashboard
│   ├── web_dashboard_lite.cpp# Lite dashboard
│   ├── neo6m.cpp             # GPS module
│   └── mesh_commands.cpp     # Serial commands
├── desktop_dashboard/
│   ├── serial_bridge.py      # Python WebSocket bridge
│   ├── dashboard.html        # Desktop web interface
│   └── requirements.txt      # Python dependencies
├── platformio.ini            # Build configuration
└── README.md                 # This file
```

---

## Quick Reference Card

| Action | Command / Location |
|--------|-------------------|
| **Build** | `pio run` |
| **Upload** | `pio run --target upload` |
| **Monitor** | `pio device monitor` |
| **Change Node ID** | `src/config.cpp` → `DEVICE_ID` |
| **Enable WiFi** | `src/config.cpp` → `WIFI_USE_STATION_MODE` |
| **AP Dashboard** | http://192.168.4.1 |
| **View Stats** | Serial: `mesh stats` |
| **Test Network** | Serial: `mesh test 255 3 Hello` |
| **Check Memory** | Serial: `mesh memory` |
| **Reset Network** | Serial: `mesh reset` |

---

**Project:** ESP32 LoRa Mesh Network
**Version:** 1.2
**Last Updated:** December 2025

### Roadmap / Upcoming Features

**v1.3 - Environmental Sensor Integration (Planned)**
- **SHT30** - High-accuracy temperature and humidity sensor
  - I2C address: 0x44 (default) or 0x45
  - Temperature accuracy: ±0.2°C
  - Humidity accuracy: ±2% RH
  - Replaces basic DHT sensors for improved reliability
- **BMP180** - Barometric pressure and altitude sensor
  - I2C address: 0x77
  - Pressure range: 300-1100 hPa
  - Altitude calculation from pressure
  - Temperature compensation built-in

**Sensor Wiring (Heltec ESP32 LoRa v3):**
| Sensor | VCC | GND | SDA | SCL |
|--------|-----|-----|-----|-----|
| SHT30  | 3.3V | GND | GPIO7 | GPIO20 |
| BMP180 | 3.3V | GND | GPIO7 | GPIO20 |

*Note: Separate I2C bus from OLED display (which uses GPIO17/18)*

**Required Libraries:**
```ini
lib_deps =
    closedcube/ClosedCube SHT31D@^1.5.1
    adafruit/Adafruit BMP085 Library@^1.2.4
```

**Files to be Modified/Created:**
| File | Changes |
|------|---------|
| `include/sensors.h` | New - Sensor structs and function declarations |
| `src/sensors.cpp` | New - SHT30/BMP180 initialization and reading |
| `include/config.h` | Add `SENSOR_SHT30_ENABLED`, `SENSOR_BMP180_ENABLED` flags |
| `src/main.cpp` | Add sensor init in `setup()`, readings in `loop()` |
| `include/lora_comm.h` | Update `FullReportMsg` with pressure/altitude fields |
| `src/lora_comm.cpp` | Update encode/decode for new sensor fields |
| `src/serial_json.cpp` | Add pressure/altitude to JSON output |
| `platformio.ini` | Add sensor library dependencies |

**New JSON Fields:**
```json
{
  "pressure": 1013.25,
  "altitude": 45.2,
  "tempSource": "SHT30"
}
```

---

### Changelog

**v1.2 - Time Source Reporting & Dashboard Improvements**
- Added explicit time source field (`timeSource`) to JSON output
- Time source now encoded in FullReportMsg flags (bits 4-5)
- Desktop dashboard topology visualization improvements:
  - Node colors indicate time source (GPS=blue, NET=orange, NONE=gray)
  - Gateway node displayed in green
  - Offline nodes displayed in red
  - RSSI labels on edges with improved visibility
- Fixed WebSocket/HTTP port handling in dashboard

**v1.1 - Network Time Synchronization**
- Added network time sync for nodes without GPS lock
- Gateway beacons now include GPS time (hour, minute, second)
- Implemented multi-hop time relay with hop count preference
- Nodes can receive time from intermediate relays
- Added satellite count check to prevent false GPS valid state
- BeaconMsg structure expanded to 16 bytes with time fields
