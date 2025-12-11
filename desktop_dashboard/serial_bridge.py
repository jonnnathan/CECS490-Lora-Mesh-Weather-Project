#!/usr/bin/env python3
"""
LoRa Mesh Serial Bridge
=======================
Bridges ESP32 serial output to a web dashboard via WebSocket.

Features:
- Reads JSON data from ESP32 gateway via serial port
- Serves web dashboard on HTTP (default: port 8080)
- Streams real-time updates via WebSocket (default: port 8081)
- Uploads data to ThingSpeak (optional)
- Works with any WiFi (uses PC's internet connection)

Usage:
    python serial_bridge.py --port COM11
    python serial_bridge.py --port /dev/ttyUSB0 --baud 115200

Requirements:
    pip install pyserial websockets aiohttp requests
"""

import argparse
import asyncio
import json
import logging
import os
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Dict, Optional, Set

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

try:
    import websockets
except ImportError:
    print("ERROR: websockets not installed. Run: pip install websockets")
    sys.exit(1)

try:
    from aiohttp import web
except ImportError:
    print("ERROR: aiohttp not installed. Run: pip install aiohttp")
    sys.exit(1)

try:
    import requests
except ImportError:
    requests = None
    print("WARNING: requests not installed. ThingSpeak uploads disabled.")

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger(__name__)

# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                         CONFIGURATION                                     ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

# ThingSpeak configuration (same as ESP32 config.cpp)
THINGSPEAK_ENABLED = True
THINGSPEAK_API_KEYS = {
    2: "8LBHS85TKG8AS2U0",
    3: "D35GBS8QVSC0BVQJ",
    4: "I1MFITOW8JNJJWD1",
    5: "KS12LH06QHZU8D8J"
}
THINGSPEAK_CHANNEL_IDS = {
    2: 3194362,
    3: 3194371,
    4: 3194372,
    5: 3194374
}
THINGSPEAK_UPDATE_INTERVAL = 20  # seconds between uploads

# Node offline detection
NODE_OFFLINE_TIMEOUT = 90  # seconds without data before marking offline
OFFLINE_CHECK_INTERVAL = 5  # seconds between offline checks

# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                         GLOBAL STATE                                      ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

# Store latest data from each node
nodes_data: Dict[int, dict] = {}
gateway_data: dict = {}
mesh_stats: dict = {}

# Connected WebSocket clients
websocket_clients: Set = set()

# Global serial port for sending commands
serial_port: Optional[serial.Serial] = None

# Last ThingSpeak upload time per node
last_thingspeak_upload: Dict[int, float] = {}

# Track last time each node was seen (for offline detection)
node_last_seen: Dict[int, float] = {}

# Track message count per node (how many messages received from each)
node_message_count: Dict[int, int] = {}

# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                         NETWORK TOPOLOGY TRACKING                         ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

# Gateway node ID (should match config)
GATEWAY_NODE_ID = 1

# Track network topology for visualization
# Format: {nodeId: {parentNode, hopDistance, rssi, timeSource, lastSeen}}
network_topology: Dict[int, dict] = {}

# Topology cache - avoid rebuilding on every request
topology_cache: dict = None
topology_cache_dirty: bool = True

# Topology history for animation/replay
# Format: [{timestamp, topology_snapshot}]
topology_history: list = []
MAX_TOPOLOGY_HISTORY = 100  # Keep last 100 topology snapshots
last_topology_snapshot_time: float = 0


def update_topology(node_id: int, data: dict):
    """Update network topology from node data."""
    global network_topology, topology_cache_dirty
    topology_cache_dirty = True  # Invalidate cache

    now = time.time()

    # Extract routing info
    parent_node = data.get('meshSenderId', 0)
    hop_distance = data.get('hopDistance', 0)
    rssi = data.get('rssi', -100)

    # Time source (from explicit timeSource field in JSON)
    # ESP32 now sends this field directly, no inference needed
    time_source = data.get('timeSource', 'UNKNOWN')
    if time_source == 'UNKNOWN':
        # Fallback inference only for old firmware without timeSource field
        # Note: satellites count alone doesn't indicate time source, so default to NONE
        time_source = 'NONE'

    # If parent is self or 0, this node is directly connected to gateway
    if parent_node == 0 or parent_node == node_id:
        parent_node = GATEWAY_NODE_ID if node_id != GATEWAY_NODE_ID else 0

    network_topology[node_id] = {
        'nodeId': node_id,
        'parentNode': parent_node,
        'hopDistance': hop_distance,
        'rssi': rssi,
        'timeSource': time_source,
        'online': True,
        'lastSeen': now,
        'lat': data.get('lat', 0),
        'lng': data.get('lng', 0),
        'temp': data.get('temp'),
        'humidity': data.get('humidity'),
        'satellites': data.get('satellites', 0),
        'neighborCount': data.get('neighborCount', 0)
    }


def save_topology_snapshot():
    """Save current topology state to history (rate limited)."""
    global topology_history, last_topology_snapshot_time

    now = time.time()

    # Only save if enough time has passed (every 30 seconds)
    if (now - last_topology_snapshot_time) < 30:
        return

    last_topology_snapshot_time = now

    snapshot = {
        'timestamp': now,
        'datetime': datetime.now().isoformat(),
        'nodes': {k: v.copy() for k, v in network_topology.items()}
    }

    topology_history.append(snapshot)

    # Trim to max size
    if len(topology_history) > MAX_TOPOLOGY_HISTORY:
        topology_history = topology_history[-MAX_TOPOLOGY_HISTORY:]

    logger.debug(f"Topology snapshot saved ({len(topology_history)} in history)")


def get_topology_data():
    """Get current topology data for dashboard (with caching)."""
    global topology_cache, topology_cache_dirty

    # Return cached data if still valid
    if not topology_cache_dirty and topology_cache is not None:
        # Update history count (changes independently)
        topology_cache['historyCount'] = len(topology_history)
        return topology_cache

    # Rebuild cache
    edges = []
    for node_id, node_data in network_topology.items():
        parent = node_data.get('parentNode', 0)
        if parent > 0 and parent != node_id:
            edges.append({
                'from': node_id,
                'to': parent,
                'rssi': node_data.get('rssi', -100)
            })

    topology_cache = {
        'nodes': list(network_topology.values()),
        'edges': edges,
        'historyCount': len(topology_history)
    }
    topology_cache_dirty = False

    return topology_cache


# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                         SERIAL PORT HANDLING                              ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

def list_serial_ports():
    """List available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found!")
        return []

    print("\nAvailable serial ports:")
    for i, port in enumerate(ports):
        print(f"  {i+1}. {port.device} - {port.description}")
    return ports


def find_esp32_port() -> Optional[str]:
    """Try to auto-detect ESP32 port."""
    ports = serial.tools.list_ports.comports()

    for port in ports:
        desc_lower = port.description.lower()
        # Common ESP32 USB-Serial chips
        if any(chip in desc_lower for chip in ['cp210', 'ch340', 'ftdi', 'silicon labs', 'usb serial']):
            logger.info(f"Auto-detected ESP32 on {port.device}")
            return port.device

    return None


async def read_serial(port: str, baud: int):
    """Read data from serial port, forward all lines to console, and process JSON."""
    global nodes_data, gateway_data, mesh_stats, serial_port

    logger.info(f"Opening serial port {port} at {baud} baud...")

    try:
        ser = serial.Serial(port, baud, timeout=0.1)
        serial_port = ser  # Store globally for sending commands
        logger.info(f"Serial port opened successfully")
    except serial.SerialException as e:
        logger.error(f"Failed to open serial port: {e}")
        return

    line_buffer = ""

    while True:
        try:
            # Read available data
            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                line_buffer += data

                # Process complete lines
                while '\n' in line_buffer:
                    line, line_buffer = line_buffer.split('\n', 1)
                    line = line.rstrip('\r')  # Remove carriage return if present

                    if not line:
                        continue

                    # Broadcast every line to the console
                    await broadcast_serial_line(line)

                    # Check if line is JSON and process it
                    if line.startswith('{') and line.endswith('}'):
                        try:
                            data = json.loads(line)
                            await process_mesh_data(data)
                        except json.JSONDecodeError:
                            # Not valid JSON, already sent to console
                            pass

                # Prevent buffer from growing too large
                if len(line_buffer) > 10000:
                    line_buffer = line_buffer[-1000:]

            await asyncio.sleep(0.01)  # Small delay to prevent busy loop

        except serial.SerialException as e:
            logger.error(f"Serial error: {e}")
            await broadcast_serial_line(f"[ERROR] Serial error: {e}")
            await asyncio.sleep(1)
        except Exception as e:
            logger.error(f"Error reading serial: {e}")
            await asyncio.sleep(0.1)


async def process_mesh_data(data: dict):
    """Process incoming mesh data and broadcast to WebSocket clients."""
    global nodes_data, gateway_data, mesh_stats, node_last_seen, node_message_count

    # Check message type
    msg_type = data.get('type', 'unknown')

    if msg_type == 'node_data':
        # Node sensor/GPS data
        node_id = data.get('nodeId')
        if node_id:
            now = time.time()
            node_last_seen[node_id] = now  # Track when we last heard from this node

            # Increment message count for this node
            if node_id not in node_message_count:
                node_message_count[node_id] = 0
            node_message_count[node_id] += 1

            nodes_data[node_id] = {
                **data,
                'lastUpdate': now,
                'online': True,  # Mark as online since we just received data
                'messageCount': node_message_count[node_id]  # Add message count
            }
            logger.info(f"Node {node_id}: temp={data.get('temp')}°F, "
                       f"humidity={data.get('humidity')}%, "
                       f"lat={data.get('lat')}, lng={data.get('lng')}, "
                       f"messages={node_message_count[node_id]}")

            # Update network topology
            update_topology(node_id, data)
            save_topology_snapshot()

            # Upload to ThingSpeak if enabled
            if THINGSPEAK_ENABLED and requests:
                await upload_to_thingspeak(node_id, data)

    elif msg_type == 'gateway_status':
        gateway_data = data
        logger.debug(f"Gateway status updated")

    elif msg_type == 'mesh_stats':
        mesh_stats = data
        logger.debug(f"Mesh stats updated")

    elif msg_type == 'beacon':
        # Routing beacon info
        logger.debug(f"Beacon from node {data.get('senderId')}, distance={data.get('distance')}")

    # Broadcast to all connected WebSocket clients
    await broadcast_update()


async def broadcast_update():
    """Send current state to all WebSocket clients."""
    if not websocket_clients:
        return

    message = json.dumps({
        'type': 'update',
        'timestamp': datetime.now().isoformat(),
        'gateway': gateway_data,
        'nodes': {str(k): v for k, v in nodes_data.items()},
        'meshStats': mesh_stats,
        'topology': get_topology_data()
    })

    # Send to all connected clients
    disconnected = set()
    for ws in websocket_clients:
        try:
            await ws.send(message)
        except websockets.exceptions.ConnectionClosed:
            disconnected.add(ws)

    # Remove disconnected clients
    websocket_clients.difference_update(disconnected)


async def broadcast_serial_line(line: str):
    """Send a raw serial line to all WebSocket clients for console display."""
    if not websocket_clients:
        return

    message = json.dumps({
        'type': 'serial',
        'line': line,
        'timestamp': datetime.now().isoformat()
    })

    # Send to all connected clients
    disconnected = set()
    for ws in websocket_clients:
        try:
            await ws.send(message)
        except websockets.exceptions.ConnectionClosed:
            disconnected.add(ws)

    # Remove disconnected clients
    websocket_clients.difference_update(disconnected)


async def check_offline_nodes():
    """Background task to check for nodes that haven't reported recently."""
    global nodes_data, node_last_seen

    while True:
        await asyncio.sleep(OFFLINE_CHECK_INTERVAL)

        now = time.time()
        status_changed = False

        for node_id, last_seen in list(node_last_seen.items()):
            time_since_seen = now - last_seen
            was_online = nodes_data.get(node_id, {}).get('online', True)

            if time_since_seen > NODE_OFFLINE_TIMEOUT:
                # Node hasn't been seen recently - mark offline
                if was_online and node_id in nodes_data:
                    nodes_data[node_id]['online'] = False
                    nodes_data[node_id]['offlineSince'] = last_seen
                    logger.warning(f"Node {node_id} marked OFFLINE (no data for {int(time_since_seen)}s)")
                    status_changed = True
            else:
                # Node is online
                if not was_online and node_id in nodes_data:
                    nodes_data[node_id]['online'] = True
                    if 'offlineSince' in nodes_data[node_id]:
                        del nodes_data[node_id]['offlineSince']
                    logger.info(f"Node {node_id} is back ONLINE")
                    status_changed = True

        # Broadcast update if any status changed
        if status_changed:
            await broadcast_update()


async def upload_to_thingspeak(node_id: int, data: dict):
    """Upload node data to ThingSpeak."""
    if node_id not in THINGSPEAK_API_KEYS:
        return

    # Rate limit uploads
    now = time.time()
    last_upload = last_thingspeak_upload.get(node_id, 0)
    if now - last_upload < THINGSPEAK_UPDATE_INTERVAL:
        return

    api_key = THINGSPEAK_API_KEYS[node_id]

    # Map data to ThingSpeak fields
    params = {
        'api_key': api_key,
        'field1': data.get('temp'),        # Temperature
        'field2': data.get('humidity'),    # Humidity
        'field3': data.get('pressure'),    # Pressure
        'field4': data.get('lat'),         # Latitude
        'field5': data.get('lng'),         # Longitude
        'field6': data.get('rssi'),        # RSSI
        'field7': data.get('satellites'),  # Satellites
        'field8': data.get('hopDistance')  # Hop distance
    }

    # Remove None values
    params = {k: v for k, v in params.items() if v is not None}

    try:
        response = requests.get(
            'https://api.thingspeak.com/update',
            params=params,
            timeout=5
        )
        if response.status_code == 200 and response.text != '0':
            last_thingspeak_upload[node_id] = now
            logger.info(f"ThingSpeak upload for node {node_id}: entry {response.text}")
        else:
            logger.warning(f"ThingSpeak upload failed for node {node_id}")
    except Exception as e:
        logger.error(f"ThingSpeak error: {e}")


# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                         COMMAND HANDLING                                  ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

async def handle_client_command(data: dict, websocket):
    """Handle commands from WebSocket clients."""
    command = data.get('command')

    if command == 'settime':
        # Set time manually on ESP32 (for testing without GPS)
        hour = data.get('hour', 0)
        minute = data.get('minute', 0)
        second = data.get('second', 0)

        # Send command to ESP32 via serial
        if serial_port and serial_port.is_open:
            cmd = f"SETTIME {hour:02d}:{minute:02d}:{second:02d}\n"
            try:
                serial_port.write(cmd.encode('utf-8'))
                logger.info(f"Sent time command to ESP32: {hour:02d}:{minute:02d}:{second:02d}")
                await websocket.send(json.dumps({
                    'type': 'command_response',
                    'command': 'settime',
                    'success': True,
                    'message': f'Time set to {hour:02d}:{minute:02d}:{second:02d} UTC'
                }))
            except Exception as e:
                logger.error(f"Failed to send time command: {e}")
                await websocket.send(json.dumps({
                    'type': 'command_response',
                    'command': 'settime',
                    'success': False,
                    'error': str(e)
                }))
        else:
            logger.warning("Serial port not available for command")
            await websocket.send(json.dumps({
                'type': 'command_response',
                'command': 'settime',
                'success': False,
                'error': 'Serial port not connected'
            }))
    else:
        logger.warning(f"Unknown command: {command}")


# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                         WEBSOCKET SERVER                                  ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

async def websocket_handler(websocket):
    """Handle WebSocket connections."""
    logger.info(f"WebSocket client connected")
    websocket_clients.add(websocket)

    try:
        # Send current state immediately
        await websocket.send(json.dumps({
            'type': 'init',
            'timestamp': datetime.now().isoformat(),
            'gateway': gateway_data,
            'nodes': {str(k): v for k, v in nodes_data.items()},
            'meshStats': mesh_stats,
            'topology': get_topology_data()
        }))

        # Keep connection alive and handle commands
        async for message in websocket:
            # Handle any client messages (e.g., commands)
            try:
                data = json.loads(message)
                logger.debug(f"Received from client: {data}")

                # Handle commands
                if data.get('type') == 'command':
                    await handle_client_command(data, websocket)
            except json.JSONDecodeError:
                pass

    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        websocket_clients.discard(websocket)
        logger.info(f"WebSocket client disconnected")


# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                         HTTP SERVER                                       ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

async def handle_index(request):
    """Serve the dashboard HTML."""
    dashboard_path = Path(__file__).parent / 'dashboard.html'
    if dashboard_path.exists():
        return web.FileResponse(dashboard_path)
    else:
        return web.Response(text="Dashboard not found. Please create dashboard.html", status=404)


async def handle_api_data(request):
    """REST API endpoint for current data."""
    return web.json_response({
        'timestamp': datetime.now().isoformat(),
        'gateway': gateway_data,
        'nodes': {str(k): v for k, v in nodes_data.items()},
        'meshStats': mesh_stats
    })


async def handle_api_nodes(request):
    """REST API endpoint for nodes only."""
    return web.json_response({
        'nodes': {str(k): v for k, v in nodes_data.items()}
    })


async def handle_api_topology(request):
    """REST API endpoint for network topology."""
    return web.json_response({
        'timestamp': datetime.now().isoformat(),
        'topology': get_topology_data()
    })


async def handle_api_topology_history(request):
    """REST API endpoint for topology history."""
    return web.json_response({
        'timestamp': datetime.now().isoformat(),
        'history': topology_history
    })


def create_http_app():
    """Create the HTTP application."""
    app = web.Application()
    app.router.add_get('/', handle_index)
    app.router.add_get('/api/data', handle_api_data)
    app.router.add_get('/api/nodes', handle_api_nodes)
    app.router.add_get('/api/topology', handle_api_topology)
    app.router.add_get('/api/topology/history', handle_api_topology_history)

    # Serve static files from current directory
    static_path = Path(__file__).parent
    app.router.add_static('/static/', static_path, show_index=True)

    return app


# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║                         MAIN                                              ║
# ╚═══════════════════════════════════════════════════════════════════════════╝

async def main(args):
    """Main entry point."""
    print("\n" + "="*60)
    print("   LoRa Mesh Serial Bridge")
    print("="*60 + "\n")

    # Find serial port
    port = args.port
    if not port:
        port = find_esp32_port()
        if not port:
            print("\nNo ESP32 detected. Please specify port manually.")
            list_serial_ports()
            print("\nUsage: python serial_bridge.py --port COM11")
            return

    print(f"Serial Port:    {port}")
    print(f"Baud Rate:      {args.baud}")
    print(f"HTTP Server:    http://localhost:{args.http_port}")
    print(f"WebSocket:      ws://localhost:{args.ws_port}")
    print(f"ThingSpeak:     {'Enabled' if THINGSPEAK_ENABLED and requests else 'Disabled'}")
    print("\n" + "-"*60)
    print("Open http://localhost:{} in your browser".format(args.http_port))
    print("-"*60 + "\n")

    # Create HTTP app
    http_app = create_http_app()
    runner = web.AppRunner(http_app)
    await runner.setup()
    site = web.TCPSite(runner, 'localhost', args.http_port)
    await site.start()
    logger.info(f"HTTP server started on port {args.http_port}")

    # Start WebSocket server
    ws_server = await websockets.serve(
        websocket_handler,
        'localhost',
        args.ws_port
    )
    logger.info(f"WebSocket server started on port {args.ws_port}")

    # Start serial reader
    serial_task = asyncio.create_task(read_serial(port, args.baud))

    # Start offline node checker
    offline_checker_task = asyncio.create_task(check_offline_nodes())
    logger.info(f"Offline detection enabled (timeout: {NODE_OFFLINE_TIMEOUT}s)")

    # Run forever
    try:
        await asyncio.gather(
            serial_task,
            offline_checker_task,
            asyncio.Future()  # Run forever
        )
    except KeyboardInterrupt:
        logger.info("Shutting down...")
    finally:
        ws_server.close()
        await runner.cleanup()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='LoRa Mesh Serial Bridge')
    parser.add_argument('--port', '-p', help='Serial port (e.g., COM11 or /dev/ttyUSB0)')
    parser.add_argument('--baud', '-b', type=int, default=115200, help='Baud rate (default: 115200)')
    parser.add_argument('--http-port', type=int, default=8080, help='HTTP server port (default: 8080)')
    parser.add_argument('--ws-port', type=int, default=8081, help='WebSocket port (default: 8081)')

    args = parser.parse_args()

    try:
        asyncio.run(main(args))
    except KeyboardInterrupt:
        print("\nExiting...")
