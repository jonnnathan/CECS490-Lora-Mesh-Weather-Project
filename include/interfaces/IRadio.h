/**
 * @file IRadio.h
 * @brief Abstract interface for radio operations.
 *
 * Provides platform-agnostic radio communication abstraction.
 * Implementations:
 * - LoRaHW: Real SX1262 hardware via RadioLib (ESP32)
 * - SimulatedRadio: UDP multicast for desktop simulation (native)
 *
 * @section Usage
 * @code
 * // Platform-specific instantiation in main.cpp
 * #ifdef ARDUINO
 *     static LoRaHW radio(DEVICE_ID);
 * #else
 *     static SimulatedRadio radio(DEVICE_ID);
 * #endif
 * g_meshContext.radio = &radio;
 *
 * // Use via interface
 * if (g_meshContext.radio->packetAvailable()) {
 *     LoRaReceivedPacket pkt;
 *     if (g_meshContext.radio->receive(pkt)) {
 *         // Process packet
 *     }
 * }
 * @endcode
 */

#ifndef IRADIO_H
#define IRADIO_H

#include <stdint.h>
#include <stddef.h>

// Forward declaration to avoid circular dependency with lora_comm.h
struct LoRaReceivedPacket;

/**
 * @brief Radio operation status codes
 */
enum class RadioStatus : int8_t {
    RADIO_OK = 0,
    RADIO_ERROR_INIT = -1,
    RADIO_ERROR_SEND = -2,
    RADIO_ERROR_RECEIVE = -3,
    RADIO_ERROR_TIMEOUT = -4,
    RADIO_ERROR_CRC = -5,
    RADIO_NO_PACKET = -6
};

/**
 * @brief Abstract interface for radio operations.
 *
 * All radio communication in the mesh network goes through this interface,
 * enabling hardware abstraction and simulation capabilities.
 */
class IRadio {
public:
    virtual ~IRadio() = default;

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /**
     * @brief Initialize the radio hardware/simulation.
     * @return true if initialization succeeded
     */
    virtual bool init() = 0;

    /**
     * @brief Put radio into standby/low-power mode.
     */
    virtual void standby() = 0;

    /**
     * @brief Check if radio is ready for operations.
     * @return true if radio is initialized and ready
     */
    virtual bool isReady() const = 0;

    // =========================================================================
    // Transmission
    // =========================================================================

    /**
     * @brief Send binary data over the radio.
     *
     * @param data Pointer to data buffer
     * @param length Length of data in bytes (max 255)
     * @return true if transmission succeeded
     *
     * @note After transmission, radio automatically returns to receive mode.
     */
    virtual bool sendBinary(const uint8_t* data, uint8_t length) = 0;

    // =========================================================================
    // Reception
    // =========================================================================

    /**
     * @brief Receive a packet from the radio.
     *
     * @param packet [out] Structure to fill with received packet data
     * @return true if a packet was received, false if no packet available
     *
     * @note Non-blocking. Returns immediately if no packet is available.
     */
    virtual bool receive(LoRaReceivedPacket& packet) = 0;

    /**
     * @brief Start/restart receive mode.
     *
     * For hardware: Enables the radio receiver and interrupt.
     * For simulation: Ensures socket is listening.
     */
    virtual void startReceive() = 0;

    /**
     * @brief Check if a packet is available without consuming it.
     * @return true if at least one packet is ready to be received
     */
    virtual bool packetAvailable() const = 0;

    // =========================================================================
    // Signal Quality
    // =========================================================================

    /**
     * @brief Get RSSI of last received packet.
     * @return RSSI in dBm (typical range: -120 to -30)
     *
     * @note For simulation, this may be calculated from virtual distance.
     */
    virtual float getLastRSSI() const = 0;

    /**
     * @brief Get SNR of last received packet.
     * @return Signal-to-Noise Ratio in dB
     *
     * @note For simulation, this is derived from simulated conditions.
     */
    virtual float getLastSNR() const = 0;

    // =========================================================================
    // Identity
    // =========================================================================

    /**
     * @brief Get the device ID associated with this radio.
     * @return 8-bit device identifier (1 = gateway)
     */
    virtual uint8_t getDeviceId() const = 0;

    // =========================================================================
    // Simulation Support (optional, default no-op for hardware)
    // =========================================================================

    /**
     * @brief Poll for incoming network data.
     *
     * For simulation: Must be called periodically in main loop to check
     * for incoming UDP packets. Replaces hardware interrupt functionality.
     *
     * For hardware: No-op (uses interrupt-driven reception).
     */
    virtual void pollNetwork() {}
};

#endif // IRADIO_H
