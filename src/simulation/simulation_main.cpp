/**
 * @file simulation_main.cpp
 * @brief Enhanced mesh network simulation with multi-hop routing verification.
 *
 * Features:
 * - Gradient routing with beacon propagation
 * - Multi-hop packet forwarding
 * - Neighbor table tracking
 * - Duplicate detection
 * - Packet delivery verification
 * - Dynamic topology (node join/leave)
 *
 * @section Usage
 * Build:   pio run -e native_windows
 * Run:     .pio/build/native_windows/program.exe <node_id> [options]
 *
 * Examples:
 *   .pio/build/native_windows/program.exe 1          # Gateway
 *   .pio/build/native_windows/program.exe 2          # Node 2
 *   .pio/build/native_windows/program.exe 3 --loss 0.2  # 20% packet loss
 */

#ifndef ARDUINO

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <ctime>
#include <cmath>

#include "simulation/Arduino_stub.h"
#include "simulation/SimulatedRadio.h"
#include "simulation/lora_packet_types.h"
#include "simulation/platform_socket.h"
#include "simulation/sim_routing.h"

// ============================================================================
// Configuration
// ============================================================================

constexpr uint8_t DEFAULT_DEVICE_ID = 1;
constexpr uint32_t TICK_INTERVAL_MS = 50;
constexpr uint32_t DATA_INTERVAL_MS = 8000;       // Send data every 8 seconds
constexpr uint32_t STATUS_INTERVAL_MS = 2000;     // Dashboard update every 2 seconds
constexpr uint32_t STATS_INTERVAL_MS = 15000;     // Print stats every 15 seconds
constexpr uint8_t SLOT_DURATION_SEC = 6;

// Dashboard
constexpr uint16_t DASHBOARD_PORT = 8889;
constexpr const char* DASHBOARD_ADDR = "127.0.0.1";

// ============================================================================
// Global State
// ============================================================================

static volatile bool g_running = true;
static SimulatedRadio* g_radio = nullptr;
static SimRouter* g_router = nullptr;
static uint8_t g_deviceId = DEFAULT_DEVICE_ID;
static bool g_isGateway = false;
static float g_posX = 0;
static float g_posY = 0;

// Dashboard socket
static socket_t g_dashboardSocket = SOCKET_INVALID;
static struct sockaddr_in g_dashboardAddr;

// Simulated sensors
static float g_temperature = 72.0f;
static float g_humidity = 45.0f;

// ============================================================================
// Dashboard Event Output
// ============================================================================

bool initDashboardSocket() {
    g_dashboardSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_dashboardSocket == SOCKET_INVALID) {
        return false;
    }
    memset(&g_dashboardAddr, 0, sizeof(g_dashboardAddr));
    g_dashboardAddr.sin_family = AF_INET;
    g_dashboardAddr.sin_addr.s_addr = inet_addr(DASHBOARD_ADDR);
    g_dashboardAddr.sin_port = htons(DASHBOARD_PORT);
    return true;
}

void sendDashboardEvent(const char* jsonEvent) {
    if (g_dashboardSocket == SOCKET_INVALID) return;
    sendto(g_dashboardSocket, jsonEvent, (int)strlen(jsonEvent), 0,
           (struct sockaddr*)&g_dashboardAddr, sizeof(g_dashboardAddr));
}

void sendNodeStatus() {
    g_temperature = 70.0f + (rand() % 100) / 10.0f;
    g_humidity = 40.0f + (rand() % 200) / 10.0f;

    char json[512];
    snprintf(json, sizeof(json),
        "{\"type\":\"node_data\",\"nodeId\":%d,\"isGateway\":%s,"
        "\"posX\":%.1f,\"posY\":%.1f,"
        "\"temp\":%.1f,\"humidity\":%.1f,"
        "\"hopDistance\":%d,\"nextHop\":%d,\"routeValid\":%s,"
        "\"neighborCount\":%d,"
        "\"txCount\":%u,\"rxCount\":%u,\"fwdCount\":%u,"
        "\"rssi\":%.1f,\"meshSenderId\":%d,"
        "\"timeSource\":\"SIM\",\"online\":true}",
        g_deviceId,
        g_isGateway ? "true" : "false",
        g_posX, g_posY,
        g_temperature, g_humidity,
        g_router->getDistance(),
        g_router->getNextHop(),
        g_router->hasValidRoute() ? "true" : "false",
        g_router->getNeighborCount(),
        g_router->getPacketsSent(),
        g_router->getPacketsReceived(),
        g_router->getPacketsForwarded(),
        g_radio->getLastRSSI(),
        g_router->getNextHop()
    );
    sendDashboardEvent(json);
}

void sendPacketEvent(const char* eventType, const char* packetType,
                     uint8_t fromNode, uint8_t toNode, uint8_t hops) {
    char json[256];
    snprintf(json, sizeof(json),
        "{\"type\":\"%s\",\"nodeId\":%d,\"packetType\":\"%s\","
        "\"fromNode\":%d,\"toNode\":%d,\"hops\":%d,"
        "\"rssi\":%.1f,\"timestamp\":%lu}",
        eventType, g_deviceId, packetType,
        fromNode, toNode, hops,
        g_radio->getLastRSSI(), (unsigned long)millis()
    );
    sendDashboardEvent(json);
}

void sendRoutingEvent(const char* event, uint8_t distance, uint8_t nextHop) {
    char json[256];
    snprintf(json, sizeof(json),
        "{\"type\":\"routing\",\"nodeId\":%d,\"event\":\"%s\","
        "\"distance\":%d,\"nextHop\":%d,\"timestamp\":%lu}",
        g_deviceId, event, distance, nextHop, (unsigned long)millis()
    );
    sendDashboardEvent(json);
}

// ============================================================================
// Signal Handler
// ============================================================================

void signalHandler(int signum) {
    printf("\n[SIM] Caught signal %d, shutting down...\n", signum);
    g_running = false;
}

// ============================================================================
// TDMA Slot Calculation
// ============================================================================

bool isMyTdmaSlot() {
    time_t rawTime;
    time(&rawTime);
    struct tm* timeInfo = localtime(&rawTime);
    uint8_t currentSecond = timeInfo->tm_sec;

    uint8_t slotStart = ((g_deviceId - 1) * SLOT_DURATION_SEC) % 60;
    uint8_t slotEnd = (slotStart + SLOT_DURATION_SEC) % 60;

    if (slotEnd > slotStart) {
        return currentSecond >= slotStart && currentSecond < slotEnd;
    } else {
        return currentSecond >= slotStart || currentSecond < slotEnd;
    }
}

// ============================================================================
// Packet Building
// ============================================================================

uint8_t buildDataPacket(uint8_t* buffer, size_t maxLen, uint8_t messageId) {
    if (maxLen < sizeof(SimMeshHeader) + 8) return 0;

    SimMeshHeader* header = (SimMeshHeader*)buffer;
    header->version = 1;
    header->messageType = MSG_DATA;
    header->sourceId = g_deviceId;
    header->destId = 1;  // Send to gateway
    header->senderId = g_deviceId;
    header->messageId = messageId;
    header->ttl = DEFAULT_TTL;
    header->flags = 0;

    // Payload: sensor data
    uint8_t* payload = buffer + sizeof(SimMeshHeader);
    payload[0] = (uint8_t)g_temperature;
    payload[1] = (uint8_t)g_humidity;
    payload[2] = g_router->getDistance();
    payload[3] = g_router->getNextHop();
    uint32_t timestamp = millis();
    memcpy(&payload[4], &timestamp, 4);

    return sizeof(SimMeshHeader) + 8;
}

// ============================================================================
// Packet Processing
// ============================================================================

void processReceivedPacket(const uint8_t* data, uint8_t length, int16_t rssi, float snr) {
    if (length < sizeof(SimMeshHeader)) return;

    const SimMeshHeader* header = (const SimMeshHeader*)data;
    uint32_t nowMs = millis();

    // Update neighbor table for immediate sender
    g_router->updateNeighbor(header->senderId, rssi, nowMs);

    // Handle by message type
    if (header->messageType == MSG_BEACON) {
        if (length < sizeof(SimBeaconMsg)) return;

        const SimBeaconMsg* beacon = (const SimBeaconMsg*)data;

        printf("[RX] Node %d received BEACON from node %d (distance=%d, TTL=%d, RSSI=%d)\n",
               g_deviceId, header->senderId, beacon->distanceToGateway, header->ttl, rssi);

        sendPacketEvent("packet_received", "BEACON", header->senderId, g_deviceId, beacon->distanceToGateway);

        // Process beacon for routing
        uint8_t oldDistance = g_router->getDistance();
        g_router->processBeacon(*beacon, rssi, nowMs);

        // Notify dashboard of routing change
        if (g_router->getDistance() != oldDistance) {
            sendRoutingEvent("route_updated", g_router->getDistance(), g_router->getNextHop());
        }

    } else if (header->messageType == MSG_DATA) {
        // Check for duplicates
        if (g_router->isDuplicate(header->sourceId, header->messageId, nowMs)) {
            printf("[RX] Node %d: Dropped duplicate DATA from node %d (msgId=%d)\n",
                   g_deviceId, header->sourceId, header->messageId);
            return;
        }
        g_router->markSeen(header->sourceId, header->messageId, nowMs);

        printf("[RX] Node %d received DATA from node %d via %d (msgId=%d, TTL=%d, RSSI=%d)\n",
               g_deviceId, header->sourceId, header->senderId, header->messageId, header->ttl, rssi);

        sendPacketEvent("packet_received", "DATA", header->sourceId, g_deviceId, header->ttl);

        // Record reception at gateway
        if (g_isGateway) {
            g_router->recordPacketReceived(header->sourceId, header->messageId, header->ttl, nowMs);
            printf("[GATEWAY] Received data from node %d (originated %d hops away)\n",
                   header->sourceId, DEFAULT_TTL - header->ttl);
        }

        // Forward if needed
        if (g_router->shouldForward(*header)) {
            // Copy and prepare for forwarding
            uint8_t forwardBuffer[256];
            memcpy(forwardBuffer, data, length);
            SimMeshHeader* fwdHeader = (SimMeshHeader*)forwardBuffer;
            g_router->prepareForward(*fwdHeader);

            printf("[FWD] Node %d forwarding DATA from %d toward gateway (TTL=%d)\n",
                   g_deviceId, header->sourceId, fwdHeader->ttl);

            sendPacketEvent("packet_forwarded", "DATA", header->sourceId, g_router->getNextHop(), fwdHeader->ttl);

            // Send with small delay to avoid collision
            delay(10 + rand() % 50);
            g_radio->sendBinary(forwardBuffer, length);
        }
    }
}

// ============================================================================
// Command Line Parsing
// ============================================================================

void printUsage(const char* progName) {
    printf("Usage: %s <node_id> [options]\n\n", progName);
    printf("Multi-Hop Routing Verification Simulation\n\n");
    printf("Arguments:\n");
    printf("  node_id          Node ID (1-255, 1 = gateway)\n\n");
    printf("Options:\n");
    printf("  --pos X Y        Set position (default: circular layout)\n");
    printf("  --loss RATE      Packet loss rate 0.0-1.0 (default: 0.0)\n");
    printf("  --latency MS     Transmission latency in ms (default: 0)\n\n");
    printf("Test Scenarios:\n");
    printf("  Linear chain:    Start nodes 1,2,3 with --pos to create chain\n");
    printf("  Packet loss:     Use --loss 0.2 for 20%% loss resilience test\n");
    printf("  Dynamic:         Start/stop nodes to test join/leave\n\n");
    printf("Examples:\n");
    printf("  %s 1 --pos 0 300           # Gateway at left\n", progName);
    printf("  %s 2 --pos 200 300         # Node 2 in middle\n", progName);
    printf("  %s 3 --pos 400 300         # Node 3 at right (2-hop from gateway)\n", progName);
}

bool parseArgs(int argc, char* argv[], uint8_t& deviceId, float& posX, float& posY,
               float& lossRate, uint32_t& latencyMs) {
    if (argc < 2) {
        printUsage(argv[0]);
        return false;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printUsage(argv[0]);
        return false;
    }

    deviceId = (uint8_t)atoi(argv[1]);
    if (deviceId == 0) {
        printf("Error: Invalid node ID\n");
        return false;
    }

    // Default circular layout
    float angle = (deviceId - 1) * (3.14159f * 2.0f / 5.0f);
    posX = 300.0f + 150.0f * cosf(angle);
    posY = 300.0f + 150.0f * sinf(angle);
    lossRate = 0.0f;
    latencyMs = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--pos") == 0 && i + 2 < argc) {
            posX = (float)atof(argv[i + 1]);
            posY = (float)atof(argv[i + 2]);
            i += 2;
        } else if (strcmp(argv[i], "--loss") == 0 && i + 1 < argc) {
            lossRate = (float)atof(argv[i + 1]);
            i += 1;
        } else if (strcmp(argv[i], "--latency") == 0 && i + 1 < argc) {
            latencyMs = (uint32_t)atoi(argv[i + 1]);
            i += 1;
        }
    }

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    printf("\n");
    printf("================================================================\n");
    printf("  ESP32 LoRa Mesh - Multi-Hop Routing Verification\n");
    printf("================================================================\n\n");

    float lossRate;
    uint32_t latencyMs;
    if (!parseArgs(argc, argv, g_deviceId, g_posX, g_posY, lossRate, latencyMs)) {
        return 1;
    }

    g_isGateway = (g_deviceId == 1);

    printf("Configuration:\n");
    printf("  Node ID:      %d %s\n", g_deviceId, g_isGateway ? "(GATEWAY)" : "");
    printf("  Position:     (%.0f, %.0f)\n", g_posX, g_posY);
    printf("  Packet Loss:  %.0f%%\n", lossRate * 100.0f);
    printf("  Latency:      %u ms\n", latencyMs);
    printf("\n");

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    Serial.begin(115200);
    platform::initSockets();
    initDashboardSocket();

    // Create router
    g_router = new SimRouter(g_deviceId, g_isGateway);

    // Create radio
    g_radio = new SimulatedRadio(g_deviceId);
    g_radio->setPosition(g_posX, g_posY);
    g_radio->setPacketLossRate(lossRate);
    g_radio->setLatencyMs(latencyMs);

    if (!g_radio->init()) {
        printf("Failed to initialize radio!\n");
        delete g_router;
        delete g_radio;
        return 1;
    }

    printf("[SIM] Simulation started. Press Ctrl+C to exit.\n");
    printf("[SIM] Dashboard: python simulation_bridge.py -> http://localhost:8080\n\n");

    // Timing
    uint32_t lastTick = millis();
    uint32_t lastBeaconCheck = 0;
    uint32_t lastDataSend = 0;
    uint32_t lastStatus = 0;
    uint32_t lastStats = 0;
    uint8_t messageSeq = 0;
    bool transmittedThisSlot = false;
    uint8_t lastSlotSecond = 255;

    // Initial status
    sendNodeStatus();
    if (g_isGateway) {
        sendRoutingEvent("gateway_start", 0, 0);
    }

    // Main loop
    while (g_running) {
        uint32_t nowMs = millis();

        // Get current time for TDMA
        time_t rawTime;
        time(&rawTime);
        struct tm* timeInfo = localtime(&rawTime);
        uint8_t currentSecond = timeInfo->tm_sec;

        // Reset slot flag
        uint8_t slotSecond = currentSecond / SLOT_DURATION_SEC;
        if (slotSecond != lastSlotSecond) {
            transmittedThisSlot = false;
            lastSlotSecond = slotSecond;
        }

        // Poll radio
        g_radio->pollNetwork();

        // Process received packets
        LoRaReceivedPacket packet;
        while (g_radio->receive(packet)) {
            processReceivedPacket(packet.payloadBytes, packet.payloadLen,
                                 (int16_t)packet.rssi, packet.snr);
        }

        // Check for beacon transmission/timeout
        g_router->checkBeaconTransmit(nowMs);

        // Gateway sends beacons
        if (g_isGateway && (nowMs - lastBeaconCheck >= SIM_BEACON_INTERVAL_MS || lastBeaconCheck == 0)) {
            SimBeaconMsg beacon;
            g_router->createGatewayBeacon(beacon, nowMs);

            printf("[TX] Gateway sending BEACON (seq=%d)\n", beacon.sequenceNumber);
            sendPacketEvent("packet_sent", "BEACON", g_deviceId, 0, 0);

            g_radio->sendBinary((uint8_t*)&beacon, sizeof(beacon));
            lastBeaconCheck = nowMs;
        }

        // Non-gateway: send pending beacon rebroadcast
        if (!g_isGateway) {
            SimBeaconMsg pendingBeacon;
            if (g_router->getPendingBeacon(pendingBeacon)) {
                printf("[TX] Node %d rebroadcasting BEACON (distance=%d)\n",
                       g_deviceId, pendingBeacon.distanceToGateway);
                sendPacketEvent("packet_sent", "BEACON", g_deviceId, 0, pendingBeacon.distanceToGateway);

                g_radio->sendBinary((uint8_t*)&pendingBeacon, sizeof(pendingBeacon));
            }
        }

        // Send data in TDMA slot
        if (!g_isGateway && isMyTdmaSlot() && !transmittedThisSlot && g_router->hasValidRoute()) {
            if (nowMs - lastDataSend >= DATA_INTERVAL_MS || lastDataSend == 0) {
                uint8_t buffer[64];
                uint8_t len = buildDataPacket(buffer, sizeof(buffer), messageSeq);

                if (len > 0) {
                    g_router->recordPacketSent(messageSeq, nowMs);
                    messageSeq++;

                    printf("[TX] Node %d sending DATA (msgId=%d) via nextHop=%d\n",
                           g_deviceId, messageSeq - 1, g_router->getNextHop());
                    sendPacketEvent("packet_sent", "DATA", g_deviceId, g_router->getNextHop(), g_router->getDistance());

                    g_radio->sendBinary(buffer, len);
                    lastDataSend = nowMs;
                    transmittedThisSlot = true;
                }
            }
        }

        // Periodic neighbor pruning
        g_router->pruneNeighbors(nowMs);

        // Dashboard status update
        if (nowMs - lastStatus >= STATUS_INTERVAL_MS) {
            sendNodeStatus();
            lastStatus = nowMs;
        }

        // Print statistics periodically
        if (nowMs - lastStats >= STATS_INTERVAL_MS) {
            printf("\n--- Node %d Statistics ---\n", g_deviceId);
            g_router->printState();
            g_router->printNeighbors();
            printf("  TX: %u, RX: %u, FWD: %u, Beacons: %u\n",
                   g_router->getPacketsSent(),
                   g_router->getPacketsReceived(),
                   g_router->getPacketsForwarded(),
                   g_router->getBeaconsReceived());
            if (g_isGateway) {
                printf("  Delivery Rate: %.1f%%\n", g_router->getDeliveryRate());
            }
            printf("--------------------------\n\n");
            lastStats = nowMs;
        }

        // Tick
        uint32_t elapsed = millis() - lastTick;
        if (elapsed < TICK_INTERVAL_MS) {
            delay(TICK_INTERVAL_MS - elapsed);
        }
        lastTick = millis();
    }

    // Cleanup
    printf("\n[SIM] Simulation ended.\n");
    printf("Final Statistics:\n");
    printf("  TX: %u, RX: %u, FWD: %u\n",
           g_router->getPacketsSent(),
           g_router->getPacketsReceived(),
           g_router->getPacketsForwarded());
    if (g_isGateway) {
        printf("  Delivery Rate: %.1f%%\n", g_router->getDeliveryRate());
    }

    if (g_dashboardSocket != SOCKET_INVALID) {
        CLOSE_SOCKET(g_dashboardSocket);
    }
    delete g_router;
    delete g_radio;
    return 0;
}

#endif // ARDUINO
