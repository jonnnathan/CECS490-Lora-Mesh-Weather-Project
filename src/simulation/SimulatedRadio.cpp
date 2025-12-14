/**
 * @file SimulatedRadio.cpp
 * @brief UDP multicast-based radio simulation implementation.
 */

#ifndef ARDUINO  // Only for native builds

#include "simulation/SimulatedRadio.h"
#include "simulation/lora_packet_types.h"
#include "simulation/Arduino_stub.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>

// ============================================================================
// Platform-Specific Lock Guard
// ============================================================================

#ifdef _WIN32
// Simple RAII wrapper for Windows CRITICAL_SECTION
class ScopedLock {
public:
    explicit ScopedLock(CRITICAL_SECTION& cs) : m_cs(cs) {
        EnterCriticalSection(&m_cs);
    }
    ~ScopedLock() {
        LeaveCriticalSection(&m_cs);
    }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
private:
    CRITICAL_SECTION& m_cs;
};
#else
// Use std::lock_guard on POSIX
template<typename T>
using ScopedLock = std::lock_guard<T>;
#endif

// ============================================================================
// Wire Protocol
// ============================================================================

// UDP packet format:
// Byte 0:    Sender ID (1 byte)
// Bytes 1-4: Sender X position (float, 4 bytes)
// Bytes 5-8: Sender Y position (float, 4 bytes)
// Bytes 9+:  Actual LoRa packet data

constexpr size_t WIRE_HEADER_SIZE = 9;  // 1 + 4 + 4

// ============================================================================
// SimulatedRadio Implementation
// ============================================================================

SimulatedRadio::SimulatedRadio(uint8_t deviceId)
    : m_deviceId(deviceId)
    , m_posX(deviceId * 100.0f)  // Default: spread nodes by ID
    , m_posY(0.0f)
    , m_packetLossRate(0.0f)
    , m_latencyMs(0)
    , m_baseRSSI(-40.0f)         // -40 dBm at 1 meter
    , m_pathLossExponent(2.5f)   // Indoor environment
    , m_socket(SOCKET_INVALID)
    , m_initialized(false)
    , m_receiving(false)
    , m_lastRSSI(-100.0f)
    , m_lastSNR(0.0f)
    , m_droppedCount(0)
    , m_receivedCount(0)
    , m_sentCount(0)
{
    memset(&m_multicastAddr, 0, sizeof(m_multicastAddr));
#ifdef _WIN32
    InitializeCriticalSection(&m_queueMutex);
#endif
}

SimulatedRadio::~SimulatedRadio() {
    if (m_socket != SOCKET_INVALID) {
        platform::leaveMulticastGroup(m_socket, SIM_MULTICAST_GROUP);
        CLOSE_SOCKET(m_socket);
    }
    platform::cleanupSockets();
#ifdef _WIN32
    DeleteCriticalSection(&m_queueMutex);
#endif
}

bool SimulatedRadio::init() {
    // Initialize platform sockets
    if (!platform::initSockets()) {
        printf("[SIM] Failed to initialize socket subsystem\n");
        return false;
    }

    // Setup the socket
    if (!setupSocket()) {
        return false;
    }

    // Join multicast group
    if (!joinMulticast()) {
        CLOSE_SOCKET(m_socket);
        m_socket = SOCKET_INVALID;
        return false;
    }

    m_initialized = true;
    m_receiving = true;

    printf("[SIM] Radio initialized for node %d at position (%.1f, %.1f)\n",
           m_deviceId, m_posX, m_posY);
    printf("[SIM] Multicast group: %s:%d\n", SIM_MULTICAST_GROUP, SIM_MULTICAST_PORT);

    // Seed random number generator for packet loss simulation
    srand((unsigned int)time(nullptr) + m_deviceId);

    return true;
}

bool SimulatedRadio::setupSocket() {
    // Create UDP socket
    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == SOCKET_INVALID) {
        char errMsg[256];
        platform::getErrorMessage(errMsg, sizeof(errMsg));
        printf("[SIM] Failed to create socket: %s\n", errMsg);
        return false;
    }

    // Allow address reuse (important for running multiple instances)
    if (!platform::setReuseAddr(m_socket)) {
        printf("[SIM] Warning: Failed to set SO_REUSEADDR\n");
    }

    // Bind to multicast port
    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(SIM_MULTICAST_PORT);

    if (bind(m_socket, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        char errMsg[256];
        platform::getErrorMessage(errMsg, sizeof(errMsg));
        printf("[SIM] Failed to bind socket: %s\n", errMsg);
        CLOSE_SOCKET(m_socket);
        m_socket = SOCKET_INVALID;
        return false;
    }

    // Set non-blocking mode
    if (!platform::setNonBlocking(m_socket)) {
        printf("[SIM] Warning: Failed to set non-blocking mode\n");
    }

    // Setup multicast destination address
    memset(&m_multicastAddr, 0, sizeof(m_multicastAddr));
    m_multicastAddr.sin_family = AF_INET;
    m_multicastAddr.sin_addr.s_addr = inet_addr(SIM_MULTICAST_GROUP);
    m_multicastAddr.sin_port = htons(SIM_MULTICAST_PORT);

    return true;
}

bool SimulatedRadio::joinMulticast() {
    // Join multicast group
    if (!platform::joinMulticastGroup(m_socket, SIM_MULTICAST_GROUP)) {
        char errMsg[256];
        platform::getErrorMessage(errMsg, sizeof(errMsg));
        printf("[SIM] Failed to join multicast group: %s\n", errMsg);
        return false;
    }

    // Set multicast TTL (1 = local network only)
    platform::setMulticastTTL(m_socket, 1);

    // Enable multicast loopback (so we can test on single machine)
    platform::setMulticastLoopback(m_socket, true);

    return true;
}

void SimulatedRadio::standby() {
    m_receiving = false;
}

bool SimulatedRadio::isReady() const {
    return m_initialized && m_socket != SOCKET_INVALID;
}

bool SimulatedRadio::sendBinary(const uint8_t* data, uint8_t length) {
    if (!m_initialized || m_socket == SOCKET_INVALID) {
        return false;
    }

    // Build wire packet: header + data
    uint8_t wirePacket[WIRE_HEADER_SIZE + SIM_MAX_PACKET_SIZE];

    // Wire header
    wirePacket[0] = m_deviceId;
    memcpy(&wirePacket[1], &m_posX, sizeof(float));
    memcpy(&wirePacket[5], &m_posY, sizeof(float));

    // Copy payload
    size_t totalLen = WIRE_HEADER_SIZE + length;
    if (totalLen > sizeof(wirePacket)) {
        printf("[SIM] Packet too large: %zu bytes\n", totalLen);
        return false;
    }
    memcpy(&wirePacket[WIRE_HEADER_SIZE], data, length);

    // Apply latency if configured
    if (m_latencyMs > 0) {
        delay(m_latencyMs);
    }

    // Send via UDP multicast
    int sent = sendto(m_socket, (const char*)wirePacket, (int)totalLen, 0,
                      (struct sockaddr*)&m_multicastAddr, sizeof(m_multicastAddr));

    if (sent < 0) {
        char errMsg[256];
        platform::getErrorMessage(errMsg, sizeof(errMsg));
        printf("[SIM] Send failed: %s\n", errMsg);
        return false;
    }

    m_sentCount++;
    printf("[SIM] Node %d sent %d bytes\n", m_deviceId, length);

    return true;
}

void SimulatedRadio::pollNetwork() {
    if (!m_initialized || !m_receiving || m_socket == SOCKET_INVALID) {
        return;
    }

    // Try to receive packets (non-blocking)
    uint8_t buffer[WIRE_HEADER_SIZE + SIM_MAX_PACKET_SIZE];
    struct sockaddr_in senderAddr;
    socklen_t addrLen = sizeof(senderAddr);

    while (true) {
        int received = recvfrom(m_socket, (char*)buffer, sizeof(buffer), 0,
                                (struct sockaddr*)&senderAddr, &addrLen);

        if (received <= 0) {
            if (!platform::wouldBlock()) {
                // Real error, not just "no data"
                char errMsg[256];
                platform::getErrorMessage(errMsg, sizeof(errMsg));
                // Don't spam errors for common "would block" case
            }
            break;  // No more packets
        }

        // Validate minimum size
        if ((size_t)received < WIRE_HEADER_SIZE) {
            continue;  // Invalid packet
        }

        // Extract wire header
        uint8_t senderId = buffer[0];
        float senderX, senderY;
        memcpy(&senderX, &buffer[1], sizeof(float));
        memcpy(&senderY, &buffer[5], sizeof(float));

        // Skip own packets
        if (senderId == m_deviceId) {
            continue;
        }

        // Check for packet loss simulation
        if (shouldDropPacket()) {
            m_droppedCount++;
            printf("[SIM] Node %d dropped packet from node %d (simulated loss)\n",
                   m_deviceId, senderId);
            continue;
        }

        // Calculate signal quality based on virtual distance
        float rssi = calculateRSSI(senderX, senderY);
        float snr = calculateSNR(rssi);

        // Create simulated packet
        SimulatedPacket pkt;
        pkt.length = received - WIRE_HEADER_SIZE;
        memcpy(pkt.data, &buffer[WIRE_HEADER_SIZE], pkt.length);
        pkt.senderId = senderId;
        pkt.senderX = senderX;
        pkt.senderY = senderY;
        pkt.timestamp = millis();
        pkt.calculatedRSSI = rssi;
        pkt.calculatedSNR = snr;

        // Queue packet
        {
            ScopedLock lock(m_queueMutex);
            if (m_rxQueue.size() < SIM_RX_QUEUE_SIZE) {
                m_rxQueue.push(pkt);
                m_receivedCount++;
            } else {
                m_droppedCount++;
                printf("[SIM] RX queue full, dropping packet\n");
            }
        }

        printf("[SIM] Node %d received %d bytes from node %d (RSSI: %.1f, SNR: %.1f)\n",
               m_deviceId, pkt.length, senderId, rssi, snr);
    }
}

bool SimulatedRadio::receive(LoRaReceivedPacket& packet) {
    ScopedLock lock(m_queueMutex);

    if (m_rxQueue.empty()) {
        return false;
    }

    // Dequeue oldest packet
    SimulatedPacket simPkt = m_rxQueue.front();
    m_rxQueue.pop();

    // Copy raw packet data - let caller parse according to their format
    // This supports both legacy 6-byte headers and new 8-byte mesh headers
    size_t copyLen = simPkt.length;
    if (copyLen > sizeof(packet.payloadBytes)) {
        copyLen = sizeof(packet.payloadBytes);
    }
    memcpy(packet.payloadBytes, simPkt.data, copyLen);
    packet.payloadLen = copyLen;

    // Also populate legacy header fields for backward compatibility
    if (simPkt.length >= LORA_HEADER_SIZE_SIM) {
        packet.header.originId = simPkt.data[0];
        memcpy(&packet.header.seq, &simPkt.data[1], sizeof(uint16_t));
        packet.header.ttl = simPkt.data[3];
        memcpy(&packet.header.payloadLen, &simPkt.data[4], sizeof(uint16_t));
    } else {
        packet.header.originId = simPkt.senderId;
        packet.header.seq = 0;
        packet.header.ttl = 1;
        packet.header.payloadLen = simPkt.length;
    }

    packet.payload = "";
    packet.rssi = simPkt.calculatedRSSI;
    packet.snr = simPkt.calculatedSNR;

    // Update last known signal quality
    m_lastRSSI = simPkt.calculatedRSSI;
    m_lastSNR = simPkt.calculatedSNR;

    return true;
}

void SimulatedRadio::startReceive() {
    m_receiving = true;
}

bool SimulatedRadio::packetAvailable() const {
    ScopedLock lock(m_queueMutex);
    return !m_rxQueue.empty();
}

float SimulatedRadio::getLastRSSI() const {
    return m_lastRSSI;
}

float SimulatedRadio::getLastSNR() const {
    return m_lastSNR;
}

uint8_t SimulatedRadio::getDeviceId() const {
    return m_deviceId;
}

// ============================================================================
// Simulation Configuration
// ============================================================================

void SimulatedRadio::setPosition(float x, float y) {
    m_posX = x;
    m_posY = y;
    printf("[SIM] Node %d position set to (%.1f, %.1f)\n", m_deviceId, x, y);
}

void SimulatedRadio::setPacketLossRate(float rate) {
    m_packetLossRate = (rate < 0.0f) ? 0.0f : (rate > 1.0f) ? 1.0f : rate;
    printf("[SIM] Node %d packet loss rate set to %.1f%%\n",
           m_deviceId, m_packetLossRate * 100.0f);
}

void SimulatedRadio::setLatencyMs(uint32_t ms) {
    m_latencyMs = ms;
    printf("[SIM] Node %d latency set to %u ms\n", m_deviceId, ms);
}

void SimulatedRadio::setBaseRSSI(float rssi) {
    m_baseRSSI = rssi;
}

void SimulatedRadio::setPathLossExponent(float exponent) {
    m_pathLossExponent = exponent;
}

// ============================================================================
// Internal Helpers
// ============================================================================

bool SimulatedRadio::shouldDropPacket() const {
    if (m_packetLossRate <= 0.0f) {
        return false;
    }
    float r = (float)rand() / (float)RAND_MAX;
    return r < m_packetLossRate;
}

float SimulatedRadio::getDistance(float senderX, float senderY) const {
    float dx = m_posX - senderX;
    float dy = m_posY - senderY;
    float dist = std::sqrt(dx * dx + dy * dy);
    return dist < 1.0f ? 1.0f : dist;  // Minimum 1 meter
}

float SimulatedRadio::calculateRSSI(float senderX, float senderY) const {
    // Log-distance path loss model:
    // RSSI = RSSI_0 - 10 * n * log10(d)
    // Where:
    //   RSSI_0 = signal strength at 1 meter
    //   n = path loss exponent
    //   d = distance in meters

    float distance = getDistance(senderX, senderY);
    float pathLoss = 10.0f * m_pathLossExponent * std::log10(distance);
    float rssi = m_baseRSSI - pathLoss;

    // Add some random variation (+/- 3 dB)
    float variation = ((float)rand() / (float)RAND_MAX - 0.5f) * 6.0f;
    rssi += variation;

    // Clamp to realistic range
    if (rssi > -30.0f) rssi = -30.0f;    // Very close
    if (rssi < -120.0f) rssi = -120.0f;  // At noise floor

    return rssi;
}

float SimulatedRadio::calculateSNR(float rssi) const {
    // Simple SNR model based on RSSI
    // Assume noise floor around -120 dBm
    const float noiseFloor = -120.0f;

    float snr = rssi - noiseFloor;

    // Add some variation
    float variation = ((float)rand() / (float)RAND_MAX - 0.5f) * 4.0f;
    snr += variation;

    // LoRa can decode at negative SNR (down to about -20 dB)
    if (snr < -20.0f) snr = -20.0f;
    if (snr > 20.0f) snr = 20.0f;

    return snr;
}

#endif // ARDUINO
