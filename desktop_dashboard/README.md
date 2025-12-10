# LoRa Mesh Desktop Dashboard

A Python-based serial bridge that reads mesh data from the ESP32 gateway via USB/Serial and serves a web dashboard on your local machine.

## Why Use This?

- **No WiFi Required on ESP32**: The gateway doesn't need to connect to WiFi
- **Works with Enterprise WiFi**: Uses your PC's internet connection (eduroam, corporate networks work fine)
- **Full-Featured Dashboard**: No ESP32 memory constraints
- **ThingSpeak Integration**: Uploads data to ThingSpeak from your PC

## Architecture

```
┌──────────────┐     USB/Serial      ┌──────────────────────────┐
│  ESP32       │ ──────────────────> │  Python Bridge           │
│  Gateway     │   JSON data stream  │  (serial_bridge.py)      │
│              │                     │                          │
│  - LoRa RX   │                     │  - Reads serial port     │
│  - Mesh data │                     │  - WebSocket server      │
│  - Sensors   │                     │  - HTTP server           │
└──────────────┘                     │  - ThingSpeak uploads    │
                                     └────────────┬─────────────┘
                                                  │
                                     HTTP :8080   │  WebSocket :8081
                                                  ▼
                                     ┌──────────────────────────┐
                                     │  Web Browser             │
                                     │  http://localhost:8080   │
                                     │                          │
                                     │  - Interactive map       │
                                     │  - Real-time updates     │
                                     │  - Node status cards     │
                                     └──────────────────────────┘
```

## Quick Start

### 1. Install Python Dependencies

```bash
cd desktop_dashboard
pip install -r requirements.txt
```

### 2. Flash ESP32 with JSON Output Enabled

The ESP32 firmware outputs JSON data over Serial. No code changes needed if you're using the latest firmware.

### 3. Run the Serial Bridge

```bash
# Auto-detect ESP32 port
python serial_bridge.py

# Or specify port manually
python serial_bridge.py --port COM3        # Windows
python serial_bridge.py --port /dev/ttyUSB0  # Linux
python serial_bridge.py --port /dev/cu.usbserial-*  # macOS
```

### 4. Open Dashboard

Open your browser to: **http://localhost:8080**

## Command Line Options

```
python serial_bridge.py [options]

Options:
  --port, -p      Serial port (auto-detected if not specified)
  --baud, -b      Baud rate (default: 115200)
  --http-port     HTTP server port (default: 8080)
  --ws-port       WebSocket port (default: 8081)
```

## Features

### Dashboard
- **Map View**: Interactive map showing node locations with connection lines
- **Node Cards**: Detailed status for each node (temp, humidity, GPS, RSSI)
- **Statistics**: Mesh network statistics (packets, forwards, etc.)
- **Console**: Real-time log of all received data

### Serial Bridge
- Auto-detects ESP32 USB serial port
- Parses JSON data from ESP32
- Serves WebSocket for real-time updates
- REST API at `/api/data` for polling
- ThingSpeak integration (uploads every 20 seconds)

## Troubleshooting

### "No ESP32 detected"
- Make sure ESP32 is connected via USB
- Check that drivers are installed (CP210x or CH340)
- Specify port manually: `--port COM3`

### Dashboard shows "Disconnected"
- Make sure serial_bridge.py is running
- Check that no other program is using the serial port
- Try restarting the bridge

### No data appearing
- Check ESP32 Serial Monitor for JSON output
- Verify baud rate matches (default: 115200)
- Make sure nodes are transmitting

## ESP32 JSON Output Format

The ESP32 outputs JSON messages on separate lines:

```json
{"type":"node_data","nodeId":3,"temp":72.5,"humidity":45.2,"lat":33.783,"lng":-118.114,"rssi":-65}
{"type":"mesh_stats","packetsReceived":150,"packetsSent":45,"packetsForwarded":30}
{"type":"gateway_status","nodeId":1,"uptime":3600,"freeHeap":150000}
{"type":"beacon","senderId":2,"distance":1,"rssi":-55}
```

## ThingSpeak Configuration

Edit `serial_bridge.py` to update your ThingSpeak API keys:

```python
THINGSPEAK_API_KEYS = {
    2: "YOUR_API_KEY_NODE_2",
    3: "YOUR_API_KEY_NODE_3",
    # ...
}
```
