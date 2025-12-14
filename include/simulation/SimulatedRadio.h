/**
 * @file SimulatedRadio.h
 * @brief UDP multicast-based radio simulation for native builds.
 *
 * Simulates LoRa communication using UDP multicast on localhost.
 * Multiple instances can run as separate processes, each representing
 * a node in the simulated mesh network.
 *
 * @section Features
 * - UDP Multicast for broadcast simulation
 * - Configurable packet loss rate
 * - Configurable transmission latency
 * - Distance-based RSSI calculation
 * - SNR simulation with noise floor
 *
 * @section Usage
 * @code
 * SimulatedRadio radio(deviceId);
 * radio.init();
 * radio.setPosition(100.0f, 200.0f);  // Virtual coordinates
 * radio.setPacketLossRate(0.1f);      // 10% packet loss
 *
 * // In main loop
 * radio.pollNetwork();  // Must be called periodically
 *
 * LoRaReceivedPacket pkt;
 * if (radio.receive(pkt)) {
 *     // Process packet
 * }
 * @endcode
 *
 * @note Only compiled when ARDUINO is not defined (native builds).
 */

#ifndef SIMULATED_RADIO_H
#define SIMULATED_RADIO_H

#ifndef ARDUINO  // Only for native builds

#include "interfaces/IRadio.h"
#include "simulation/platform_socket.h"
#include <queue>
#include <cstdint>
#include <cmath>

// Platform-specific mutex
#ifdef _WIN32
    // CRITICAL_SECTION is already included via platform_socket.h -> winsock2.h
#else
    #include <mutex>
#endif

// ============================================================================
// Simulation Configuration
// ============================================================================

/// Multicast group address for simulated radio network
constexpr const char* SIM_MULTICAST_GROUP = "239.0.0.1";

/// UDP port for simulation
constexpr uint16_t SIM_MULTICAST_PORT = 8888;

/// Maximum packet size (matches LoRa max)
constexpr size_t SIM_MAX_PACKET_SIZE = 256;

/// Maximum packets in receive queue
constexpr size_t SIM_RX_QUEUE_SIZE = 32;

// ============================================================================
// Simulated Packet Structure
// ============================================================================

/**
 * @brief Internal packet structure with simulation metadata.
 *
 * Wraps raw packet data with simulated signal characteristics.
 */
struct SimulatedPacket {
    uint8_t data[SIM_MAX_PACKET_SIZE];  ///< Raw packet data
    uint8_t length;                      ///< Packet length in bytes
    uint8_t senderId;                    ///< ID of sending node
    float senderX;                       ///< Sender's X position
    float senderY;                       ///< Sender's Y position
    uint64_t timestamp;                  ///< Receive timestamp (ms)
    float calculatedRSSI;                ///< RSSI based on distance
    float calculatedSNR;                 ///< SNR based on conditions
};

// ============================================================================
// SimulatedRadio Class
// ============================================================================

/**
 * @brief UDP multicast-based radio simulation.
 *
 * Provides the IRadio interface using UDP multicast for inter-process
 * communication on the local machine.
 */
class SimulatedRadio : public IRadio {
public:
    /**
     * @brief Construct simulated radio with device ID.
     * @param deviceId This node's unique identifier (1 = gateway)
     */
    explicit SimulatedRadio(uint8_t deviceId);

    /**
     * @brief Destructor - cleanup sockets.
     */
    ~SimulatedRadio();

    // =========================================================================
    // IRadio Interface Implementation
    // =========================================================================

    bool init() override;
    void standby() override;
    bool isReady() const override;

    bool sendBinary(const uint8_t* data, uint8_t length) override;

    bool receive(LoRaReceivedPacket& packet) override;
    void startReceive() override;
    bool packetAvailable() const override;

    float getLastRSSI() const override;
    float getLastSNR() const override;

    uint8_t getDeviceId() const override;

    /**
     * @brief Poll network for incoming packets.
     *
     * Must be called periodically in the main loop. Checks for
     * incoming UDP packets and queues them for later retrieval.
     */
    void pollNetwork() override;

    // =========================================================================
    // Simulation Configuration
    // =========================================================================

    /**
     * @brief Set virtual position for distance-based RSSI.
     *
     * @param x X coordinate in virtual space
     * @param y Y coordinate in virtual space
     */
    void setPosition(float x, float y);

    /**
     * @brief Get current X position.
     */
    float getPositionX() const { return m_posX; }

    /**
     * @brief Get current Y position.
     */
    float getPositionY() const { return m_posY; }

    /**
     * @brief Set packet loss rate.
     *
     * @param rate Loss rate (0.0 = no loss, 1.0 = all lost)
     */
    void setPacketLossRate(float rate);

    /**
     * @brief Set transmission latency.
     *
     * @param ms Latency in milliseconds (added to all transmissions)
     */
    void setLatencyMs(uint32_t ms);

    /**
     * @brief Set base RSSI (signal strength at 1 meter).
     *
     * @param rssi Base RSSI in dBm (typical: -40 to -50)
     */
    void setBaseRSSI(float rssi);

    /**
     * @brief Set path loss exponent for RSSI calculation.
     *
     * @param exponent Path loss exponent (typical: 2.0 to 4.0)
     *                 2.0 = free space, 4.0 = indoor/obstacles
     */
    void setPathLossExponent(float exponent);

    /**
     * @brief Get statistics about dropped packets.
     */
    uint32_t getDroppedPacketCount() const { return m_droppedCount; }

    /**
     * @brief Get total received packet count.
     */
    uint32_t getReceivedPacketCount() const { return m_receivedCount; }

private:
    // Configuration
    uint8_t m_deviceId;
    float m_posX;
    float m_posY;
    float m_packetLossRate;
    uint32_t m_latencyMs;
    float m_baseRSSI;
    float m_pathLossExponent;

    // State
    socket_t m_socket;
    struct sockaddr_in m_multicastAddr;
    bool m_initialized;
    bool m_receiving;

    // Signal quality (last received packet)
    float m_lastRSSI;
    float m_lastSNR;

    // Receive queue
    std::queue<SimulatedPacket> m_rxQueue;
#ifdef _WIN32
    mutable CRITICAL_SECTION m_queueMutex;
#else
    mutable std::mutex m_queueMutex;
#endif

    // Statistics
    uint32_t m_droppedCount;
    uint32_t m_receivedCount;
    uint32_t m_sentCount;

    // Private methods
    bool setupSocket();
    bool joinMulticast();
    bool shouldDropPacket() const;
    float calculateRSSI(float senderX, float senderY) const;
    float calculateSNR(float rssi) const;
    float getDistance(float senderX, float senderY) const;
};

#endif // ARDUINO
#endif // SIMULATED_RADIO_H
