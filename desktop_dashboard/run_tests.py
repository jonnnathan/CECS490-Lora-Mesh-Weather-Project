#!/usr/bin/env python3
"""
Mesh Network Verification Test Runner
=====================================
Launches simulation scenarios for testing multi-hop routing, packet loss
resilience, and dynamic topology changes.

Usage:
    python run_tests.py                    # Interactive menu
    python run_tests.py --test multihop    # Run specific test
    python run_tests.py --test loss        # Packet loss test
    python run_tests.py --test dynamic     # Dynamic topology test

Requirements:
    - Built simulation: pio run -e native_windows
    - Python packages: pip install websockets aiohttp
"""

import argparse
import asyncio
import subprocess
import sys
import time
import os
from pathlib import Path

# Configuration
SCRIPT_DIR = Path(__file__).parent
PROJECT_DIR = SCRIPT_DIR.parent
PROGRAM_EXE = PROJECT_DIR / ".pio" / "build" / "native_windows" / "program.exe"
BRIDGE_SCRIPT = SCRIPT_DIR / "simulation_bridge.py"

# Test scenarios
TESTS = {
    "multihop": {
        "name": "Multi-Hop Routing Test",
        "description": "Tests beacon propagation and packet forwarding through 3 nodes in a chain",
        "nodes": [
            {"id": 1, "pos": (100, 300), "loss": 0.0, "role": "Gateway"},
            {"id": 2, "pos": (300, 300), "loss": 0.0, "role": "Relay"},
            {"id": 3, "pos": (500, 300), "loss": 0.0, "role": "End Node"},
        ],
        "duration": 60,
        "expected": [
            "Node 2 should have distance=1, nextHop=1",
            "Node 3 should have distance=2, nextHop=2",
            "Data from Node 3 should reach Gateway via Node 2",
        ]
    },
    "loss": {
        "name": "Packet Loss Resilience Test",
        "description": "Tests network behavior with 20% packet loss on relay node",
        "nodes": [
            {"id": 1, "pos": (100, 300), "loss": 0.0, "role": "Gateway"},
            {"id": 2, "pos": (300, 300), "loss": 0.2, "role": "Lossy Relay"},
            {"id": 3, "pos": (500, 300), "loss": 0.0, "role": "End Node"},
        ],
        "duration": 90,
        "expected": [
            "Routes should still be established despite packet loss",
            "Some data packets may be lost, but most should get through",
            "Beacons should eventually propagate to all nodes",
        ]
    },
    "dynamic": {
        "name": "Dynamic Topology Test",
        "description": "Tests node join/leave behavior - Node 3 joins after 30 seconds",
        "nodes": [
            {"id": 1, "pos": (100, 300), "loss": 0.0, "role": "Gateway", "delay": 0},
            {"id": 2, "pos": (300, 300), "loss": 0.0, "role": "Relay", "delay": 0},
            {"id": 3, "pos": (500, 300), "loss": 0.0, "role": "Late Joiner", "delay": 30},
        ],
        "duration": 90,
        "expected": [
            "Node 2 should establish route within 15 seconds",
            "Node 3 should join network after 30 seconds",
            "Node 3 should learn route from Node 2's rebroadcast",
        ]
    },
    "star": {
        "name": "Star Topology Test",
        "description": "Tests 5 nodes all within direct range of gateway",
        "nodes": [
            {"id": 1, "pos": (300, 300), "loss": 0.0, "role": "Gateway"},
            {"id": 2, "pos": (200, 200), "loss": 0.0, "role": "Node"},
            {"id": 3, "pos": (400, 200), "loss": 0.0, "role": "Node"},
            {"id": 4, "pos": (200, 400), "loss": 0.0, "role": "Node"},
            {"id": 5, "pos": (400, 400), "loss": 0.0, "role": "Node"},
        ],
        "duration": 60,
        "expected": [
            "All nodes should have distance=1, nextHop=1",
            "Data from all nodes should reach gateway directly",
        ]
    },
    "severe_loss": {
        "name": "Severe Packet Loss Test",
        "description": "Tests network with 50% packet loss - stress test",
        "nodes": [
            {"id": 1, "pos": (100, 300), "loss": 0.0, "role": "Gateway"},
            {"id": 2, "pos": (300, 300), "loss": 0.5, "role": "Very Lossy"},
            {"id": 3, "pos": (500, 300), "loss": 0.5, "role": "Very Lossy"},
        ],
        "duration": 120,
        "expected": [
            "Network may be unstable",
            "Some routes may not establish",
            "Tests network resilience limits",
        ]
    }
}


def print_banner():
    """Print welcome banner."""
    print("\n" + "=" * 70)
    print("  ESP32 LoRa Mesh Network - Verification Test Runner")
    print("=" * 70 + "\n")


def print_test_info(test_key):
    """Print information about a test."""
    test = TESTS[test_key]
    print(f"\n{'=' * 60}")
    print(f"  {test['name']}")
    print(f"{'=' * 60}")
    print(f"\n{test['description']}\n")
    print("Nodes:")
    for node in test["nodes"]:
        delay_str = f", delay={node.get('delay', 0)}s" if node.get('delay', 0) > 0 else ""
        print(f"  Node {node['id']}: pos={node['pos']}, loss={node['loss']*100:.0f}%, role={node['role']}{delay_str}")
    print(f"\nDuration: {test['duration']} seconds")
    print("\nExpected Results:")
    for exp in test["expected"]:
        print(f"  - {exp}")
    print()


def list_tests():
    """List all available tests."""
    print("Available Tests:")
    print("-" * 60)
    for key, test in TESTS.items():
        print(f"  {key:15} - {test['name']}")
    print()


async def run_bridge():
    """Start the simulation bridge."""
    print("[BRIDGE] Starting simulation bridge...")
    proc = await asyncio.create_subprocess_exec(
        sys.executable, str(BRIDGE_SCRIPT),
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
        cwd=str(SCRIPT_DIR)
    )
    # Wait for bridge to start
    await asyncio.sleep(2)
    print("[BRIDGE] Bridge started on http://localhost:8080")
    return proc


async def run_node(node_id, pos, loss, delay=0):
    """Start a simulation node."""
    if delay > 0:
        print(f"[NODE {node_id}] Will start in {delay} seconds...")
        await asyncio.sleep(delay)

    args = [str(PROGRAM_EXE), str(node_id), "--pos", str(pos[0]), str(pos[1])]
    if loss > 0:
        args.extend(["--loss", str(loss)])

    print(f"[NODE {node_id}] Starting at pos={pos}, loss={loss*100:.0f}%")

    proc = await asyncio.create_subprocess_exec(
        *args,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.STDOUT,
        cwd=str(PROJECT_DIR)
    )
    return proc


async def monitor_output(proc, name, duration):
    """Monitor process output for a duration."""
    try:
        end_time = time.time() + duration
        while time.time() < end_time:
            try:
                line = await asyncio.wait_for(proc.stdout.readline(), timeout=1.0)
                if line:
                    text = line.decode('utf-8', errors='ignore').strip()
                    # Filter interesting lines
                    if any(kw in text for kw in ['[ROUTE]', '[GATEWAY]', '[FWD]', 'Statistics', 'distance=']):
                        print(f"[{name}] {text}")
            except asyncio.TimeoutError:
                continue
            except Exception:
                break
    except Exception as e:
        print(f"[{name}] Monitor error: {e}")


async def run_test(test_key):
    """Run a specific test scenario."""
    if test_key not in TESTS:
        print(f"Unknown test: {test_key}")
        list_tests()
        return

    test = TESTS[test_key]
    print_test_info(test_key)

    # Check if program exists
    if not PROGRAM_EXE.exists():
        print(f"ERROR: Simulation not built. Run: pio run -e native_windows")
        return

    input("Press Enter to start the test (open http://localhost:8080 to watch)...")

    # Start bridge
    bridge_proc = await run_bridge()

    # Start nodes
    node_procs = []
    monitor_tasks = []

    for node in test["nodes"]:
        delay = node.get("delay", 0)
        proc = await run_node(node["id"], node["pos"], node["loss"], delay)
        node_procs.append(proc)

        # Create monitor task
        task = asyncio.create_task(
            monitor_output(proc, f"Node {node['id']}", test["duration"] + delay)
        )
        monitor_tasks.append(task)

    print(f"\n[TEST] Running for {test['duration']} seconds...")
    print("[TEST] Watch the dashboard at http://localhost:8080")
    print("[TEST] Press Ctrl+C to stop early\n")

    try:
        # Wait for test duration
        await asyncio.sleep(test["duration"])
    except asyncio.CancelledError:
        print("\n[TEST] Cancelled by user")

    # Cleanup
    print("\n[TEST] Stopping nodes...")
    for proc in node_procs:
        proc.terminate()
        try:
            await asyncio.wait_for(proc.wait(), timeout=5)
        except asyncio.TimeoutError:
            proc.kill()

    bridge_proc.terminate()
    try:
        await asyncio.wait_for(bridge_proc.wait(), timeout=5)
    except asyncio.TimeoutError:
        bridge_proc.kill()

    # Cancel monitor tasks
    for task in monitor_tasks:
        task.cancel()

    print("\n[TEST] Test completed!")
    print("\nExpected Results:")
    for exp in test["expected"]:
        print(f"  - {exp}")
    print()


def interactive_menu():
    """Show interactive test selection menu."""
    print_banner()
    list_tests()

    while True:
        choice = input("Enter test name (or 'q' to quit): ").strip().lower()
        if choice == 'q':
            break
        if choice in TESTS:
            asyncio.run(run_test(choice))
            print()
        else:
            print(f"Unknown test: {choice}")
            list_tests()


def main():
    parser = argparse.ArgumentParser(description="Mesh Network Verification Test Runner")
    parser.add_argument("--test", "-t", choices=list(TESTS.keys()),
                       help="Run specific test")
    parser.add_argument("--list", "-l", action="store_true",
                       help="List available tests")
    args = parser.parse_args()

    if args.list:
        print_banner()
        list_tests()
    elif args.test:
        print_banner()
        asyncio.run(run_test(args.test))
    else:
        interactive_menu()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nExiting...")
