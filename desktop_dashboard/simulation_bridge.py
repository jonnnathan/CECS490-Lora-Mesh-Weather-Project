#!/usr/bin/env python3
"""
LoRa Mesh Simulation Bridge
============================
Bridges UDP events from native mesh simulation to web dashboard via WebSocket.

Features:
- Receives JSON events from simulated nodes via UDP (port 8889)
- Serves web dashboard on HTTP (default: port 8080)
- Streams real-time updates via WebSocket (default: port 8081)
- Visualizes multi-node simulation topology

Usage:
    python simulation_bridge.py
    python simulation_bridge.py --http-port 9000 --ws-port 9001

Requirements:
    pip install websockets aiohttp
"""

import argparse
import asyncio
import json
import logging
import socket
import struct
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Dict, Set

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

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger(__name__)

# ============================================================================
#                         CONFIGURATION
# ============================================================================

# UDP port for receiving simulation events
SIM_UDP_PORT = 8889

# Node offline detection
NODE_OFFLINE_TIMEOUT = 10  # seconds (shorter for simulation)
OFFLINE_CHECK_INTERVAL = 2  # seconds

# Gateway node ID
GATEWAY_NODE_ID = 1

# ============================================================================
#                         GLOBAL STATE
# ============================================================================

# Store latest data from each node
nodes_data: Dict[int, dict] = {}
gateway_data: dict = {}
mesh_stats: dict = {
    'packetsSent': 0,
    'packetsReceived': 0,
    'packetsDropped': 0
}

# Connected WebSocket clients
websocket_clients: Set = set()

# Track last time each node was seen
node_last_seen: Dict[int, float] = {}

# Track message count per node
node_message_count: Dict[int, int] = {}

# Network topology
network_topology: Dict[int, dict] = {}
topology_cache: dict = None
topology_cache_dirty: bool = True

# Packet events history (for animation)
packet_events: list = []
MAX_PACKET_EVENTS = 100

# Console log buffer
console_lines: list = []
MAX_CONSOLE_LINES = 500

# ============================================================================
#                         TOPOLOGY TRACKING
# ============================================================================

def update_topology(node_id: int, data: dict):
    """Update network topology from node data."""
    global network_topology, topology_cache_dirty
    topology_cache_dirty = True

    now = time.time()

    # Position from simulation - support both formats
    # Format 1: posX, posY (from simulation_main.cpp)
    # Format 2: position.x, position.y
    if 'posX' in data:
        x = data.get('posX', node_id * 100)
        y = data.get('posY', 0)
    else:
        pos = data.get('position', {})
        x = pos.get('x', node_id * 100)
        y = pos.get('y', 0)

    # Routing info
    hop_distance = data.get('hopDistance', 0)
    parent_node = data.get('parentNode', data.get('meshSenderId', 0))

    # If no parent specified, estimate based on hop distance
    if parent_node == 0 and hop_distance > 0 and node_id != GATEWAY_NODE_ID:
        # Find closest node with lower hop distance as parent
        parent_node = GATEWAY_NODE_ID

    network_topology[node_id] = {
        'nodeId': node_id,
        'parentNode': parent_node,
        'hopDistance': hop_distance,
        'rssi': data.get('rssi', -50),
        'snr': data.get('snr', 10),
        'timeSource': data.get('timeSource', 'SIM'),
        'online': True,
        'lastSeen': now,
        'x': x,
        'y': y,
        'lat': data.get('lat', 0),
        'lng': data.get('lng', 0),
        'temp': data.get('temp', data.get('sensors', {}).get('temperature') if isinstance(data.get('sensors'), dict) else None),
        'humidity': data.get('humidity', data.get('sensors', {}).get('humidity') if isinstance(data.get('sensors'), dict) else None),
        'neighborCount': data.get('neighborCount', 0)
    }


def get_topology_data():
    """Get current topology data for dashboard."""
    global topology_cache, topology_cache_dirty

    if not topology_cache_dirty and topology_cache is not None:
        return topology_cache

    # Build edges from parent relationships
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
        'packetEvents': packet_events[-20:]  # Last 20 packet events
    }
    topology_cache_dirty = False

    return topology_cache


# ============================================================================
#                         UDP EVENT RECEIVER
# ============================================================================

async def receive_udp_events():
    """Receive JSON events from simulation nodes via UDP."""
    global nodes_data, mesh_stats, node_last_seen, node_message_count

    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', SIM_UDP_PORT))
    sock.setblocking(False)

    logger.info(f"Listening for simulation events on UDP port {SIM_UDP_PORT}")

    loop = asyncio.get_event_loop()

    while True:
        try:
            # Non-blocking receive
            try:
                data, addr = await loop.run_in_executor(None, lambda: sock.recvfrom(4096))
            except BlockingIOError:
                await asyncio.sleep(0.01)
                continue
            except Exception:
                await asyncio.sleep(0.01)
                continue

            # Parse JSON
            try:
                event = json.loads(data.decode('utf-8'))
            except json.JSONDecodeError as e:
                logger.warning(f"Invalid JSON from {addr}: {e}")
                continue

            await process_simulation_event(event)

        except Exception as e:
            logger.error(f"Error receiving UDP: {e}")
            await asyncio.sleep(0.1)


async def process_simulation_event(event: dict):
    """Process incoming simulation event."""
    global nodes_data, mesh_stats, node_last_seen, node_message_count, packet_events, console_lines

    event_type = event.get('type', 'unknown')
    now = time.time()

    # Add to console log
    log_line = f"[{datetime.now().strftime('%H:%M:%S')}] {json.dumps(event)}"
    console_lines.append(log_line)
    if len(console_lines) > MAX_CONSOLE_LINES:
        console_lines = console_lines[-MAX_CONSOLE_LINES:]

    if event_type in ('node_status', 'node_data'):
        # Node status update - accept both type names
        node_id = event.get('nodeId')
        if node_id:
            node_last_seen[node_id] = now

            if node_id not in node_message_count:
                node_message_count[node_id] = 0
            node_message_count[node_id] += 1

            # Handle both position formats
            if 'posX' in event:
                pos_x = event.get('posX', 0)
                pos_y = event.get('posY', 0)
            else:
                pos_x = event.get('position', {}).get('x', 0)
                pos_y = event.get('position', {}).get('y', 0)

            nodes_data[node_id] = {
                'nodeId': node_id,
                'type': 'node_data',
                'temp': event.get('temp', event.get('sensors', {}).get('temperature') if isinstance(event.get('sensors'), dict) else None),
                'humidity': event.get('humidity', event.get('sensors', {}).get('humidity') if isinstance(event.get('sensors'), dict) else None),
                'pressure': event.get('pressure', event.get('sensors', {}).get('pressure') if isinstance(event.get('sensors'), dict) else None),
                'rssi': event.get('rssi', -50),
                'snr': event.get('snr', 10),
                'hopDistance': event.get('hopDistance', 0),
                'lat': event.get('lat', 0),
                'lng': event.get('lng', 0),
                'satellites': event.get('satellites', 0),
                'lastUpdate': now,
                'online': True,
                'messageCount': node_message_count[node_id],
                'position': {'x': pos_x, 'y': pos_y},
                'neighborCount': event.get('neighborCount', 0),
                'isGateway': event.get('isGateway', node_id == 1),
                'txCount': event.get('txCount', 0),
                'rxCount': event.get('rxCount', 0)
            }

            update_topology(node_id, event)

            logger.info(f"Node {node_id}: pos=({pos_x:.0f}, {pos_y:.0f}), "
                       f"hop={event.get('hopDistance', 0)}, temp={event.get('temp', 'N/A')}")

    elif event_type == 'packet_sent':
        mesh_stats['packetsSent'] = mesh_stats.get('packetsSent', 0) + 1

        # Get node info - simulation sends nodeId as the sender
        from_node = event.get('fromNode', event.get('nodeId', 0))
        pkt_type = event.get('packetType', event.get('event', 'UNKNOWN'))

        # Add packet event for visualization
        packet_events.append({
            'type': 'sent',
            'timestamp': now,
            'fromNode': from_node,
            'packetType': pkt_type,
            'rssi': event.get('rssi', -50),
            'seq': event.get('seq', 0)
        })
        if len(packet_events) > MAX_PACKET_EVENTS:
            packet_events = packet_events[-MAX_PACKET_EVENTS:]

        logger.info(f"[TX] Node {from_node} sent {pkt_type}")

    elif event_type == 'packet_received':
        mesh_stats['packetsReceived'] = mesh_stats.get('packetsReceived', 0) + 1

        # Get node info - simulation sends different field names
        from_node = event.get('fromNode', event.get('from', 0))
        to_node = event.get('toNode', event.get('nodeId', 0))
        pkt_type = event.get('packetType', event.get('event', 'UNKNOWN'))

        # Add packet event for visualization
        packet_events.append({
            'type': 'received',
            'timestamp': now,
            'fromNode': from_node,
            'toNode': to_node,
            'packetType': pkt_type,
            'rssi': event.get('rssi', -50),
            'snr': event.get('snr', 0),
            'seq': event.get('seq', 0)
        })
        if len(packet_events) > MAX_PACKET_EVENTS:
            packet_events = packet_events[-MAX_PACKET_EVENTS:]

        logger.info(f"[RX] Node {to_node} received {pkt_type} from {from_node} (RSSI: {event.get('rssi', 'N/A')})")

    elif event_type == 'packet_dropped':
        mesh_stats['packetsDropped'] = mesh_stats.get('packetsDropped', 0) + 1
        logger.debug(f"Packet dropped: {event.get('reason', 'unknown')}")

    # Broadcast update to WebSocket clients
    await broadcast_update()
    await broadcast_serial_line(log_line)


# ============================================================================
#                         WEBSOCKET BROADCASTING
# ============================================================================

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

    disconnected = set()
    for ws in websocket_clients:
        try:
            await ws.send(message)
        except websockets.exceptions.ConnectionClosed:
            disconnected.add(ws)

    websocket_clients.difference_update(disconnected)


async def broadcast_serial_line(line: str):
    """Send a log line to all WebSocket clients for console display."""
    if not websocket_clients:
        return

    message = json.dumps({
        'type': 'serial',
        'line': line,
        'timestamp': datetime.now().isoformat()
    })

    disconnected = set()
    for ws in websocket_clients:
        try:
            await ws.send(message)
        except websockets.exceptions.ConnectionClosed:
            disconnected.add(ws)

    websocket_clients.difference_update(disconnected)


async def check_offline_nodes():
    """Check for nodes that haven't reported recently."""
    global nodes_data, node_last_seen, topology_cache_dirty

    while True:
        await asyncio.sleep(OFFLINE_CHECK_INTERVAL)

        now = time.time()
        status_changed = False

        for node_id, last_seen in list(node_last_seen.items()):
            time_since_seen = now - last_seen
            was_online = nodes_data.get(node_id, {}).get('online', True)

            if time_since_seen > NODE_OFFLINE_TIMEOUT:
                if was_online and node_id in nodes_data:
                    nodes_data[node_id]['online'] = False
                    if node_id in network_topology:
                        network_topology[node_id]['online'] = False
                    topology_cache_dirty = True
                    logger.warning(f"Node {node_id} marked OFFLINE (no data for {int(time_since_seen)}s)")
                    status_changed = True
            else:
                if not was_online and node_id in nodes_data:
                    nodes_data[node_id]['online'] = True
                    if node_id in network_topology:
                        network_topology[node_id]['online'] = True
                    topology_cache_dirty = True
                    logger.info(f"Node {node_id} is back ONLINE")
                    status_changed = True

        if status_changed:
            await broadcast_update()


# ============================================================================
#                         WEBSOCKET SERVER
# ============================================================================

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

        # Send recent console lines
        for line in console_lines[-50:]:
            await websocket.send(json.dumps({
                'type': 'serial',
                'line': line,
                'timestamp': datetime.now().isoformat()
            }))

        # Keep connection alive
        async for message in websocket:
            try:
                data = json.loads(message)
                logger.debug(f"Received from client: {data}")
                # Handle any commands if needed
            except json.JSONDecodeError:
                pass

    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        websocket_clients.discard(websocket)
        logger.info(f"WebSocket client disconnected")


# ============================================================================
#                         HTTP SERVER
# ============================================================================

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


def create_http_app():
    """Create the HTTP application."""
    app = web.Application()
    app.router.add_get('/', handle_index)
    app.router.add_get('/api/data', handle_api_data)
    app.router.add_get('/api/nodes', handle_api_nodes)
    app.router.add_get('/api/topology', handle_api_topology)

    # Serve static files
    static_path = Path(__file__).parent
    app.router.add_static('/static/', static_path, show_index=True)

    return app


# ============================================================================
#                         MAIN
# ============================================================================

async def main(args):
    """Main entry point."""
    print("\n" + "="*60)
    print("   LoRa Mesh Simulation Bridge")
    print("="*60 + "\n")

    print(f"UDP Listen Port: {SIM_UDP_PORT}")
    print(f"HTTP Server:     http://localhost:{args.http_port}")
    print(f"WebSocket:       ws://localhost:{args.ws_port}")
    print("\n" + "-"*60)
    print(f"Open http://localhost:{args.http_port} in your browser")
    print("-"*60)
    print("\nStart simulation nodes with:")
    print("  .pio/build/native_windows/program.exe 1  (gateway)")
    print("  .pio/build/native_windows/program.exe 2")
    print("  .pio/build/native_windows/program.exe 3")
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

    # Start UDP receiver
    udp_task = asyncio.create_task(receive_udp_events())

    # Start offline checker
    offline_task = asyncio.create_task(check_offline_nodes())

    # Run forever
    try:
        await asyncio.gather(
            udp_task,
            offline_task,
            asyncio.Future()
        )
    except KeyboardInterrupt:
        logger.info("Shutting down...")
    finally:
        ws_server.close()
        await runner.cleanup()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='LoRa Mesh Simulation Bridge')
    parser.add_argument('--http-port', type=int, default=8080, help='HTTP server port (default: 8080)')
    parser.add_argument('--ws-port', type=int, default=8081, help='WebSocket port (default: 8081)')

    args = parser.parse_args()

    try:
        asyncio.run(main(args))
    except KeyboardInterrupt:
        print("\nExiting...")
