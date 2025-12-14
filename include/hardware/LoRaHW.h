/**
 * @file LoRaHW.h
 * @brief Hardware LoRa implementation using SX1262 via RadioLib.
 *
 * Wraps existing lora_comm.cpp global functions to provide the IRadio
 * interface for ESP32 hardware builds.
 *
 * @note Only compiled when ARDUINO is defined (ESP32 builds).
 */

#ifndef LORA_HW_H
#define LORA_HW_H

#ifdef ARDUINO  // Only compile on ESP32

#include "interfaces/IRadio.h"
#include "lora_comm.h"

/**
 * @brief Hardware LoRa implementation wrapping lora_comm.cpp
 *
 * This class provides the IRadio interface by delegating to the
 * existing global functions in lora_comm.cpp. This approach:
 * - Maintains backward compatibility with existing code
 * - Enables gradual migration to the interface
 * - Allows simulation without hardware changes
 */
class LoRaHW : public IRadio {
public:
    /**
     * @brief Construct LoRaHW with device ID.
     * @param deviceId This node's unique identifier (1 = gateway)
     */
    explicit LoRaHW(uint8_t deviceId);

    // =========================================================================
    // IRadio Interface Implementation
    // =========================================================================

    /**
     * @brief Initialize SX1262 radio.
     *
     * Delegates to initLoRa() which configures:
     * - Frequency: 915 MHz
     * - Bandwidth: 125 kHz
     * - Spreading Factor: 7
     * - Output Power: 14 dBm
     *
     * @return true if radio initialized successfully
     */
    bool init() override;

    /**
     * @brief Put radio in standby mode.
     */
    void standby() override;

    /**
     * @brief Check if radio is ready.
     * @return true if initialized and operational
     */
    bool isReady() const override;

    /**
     * @brief Transmit binary data.
     *
     * Delegates to sendBinaryMessage() which:
     * - Prepends 6-byte LoRa header
     * - Transmits via RadioLib
     * - Automatically returns to receive mode
     *
     * @param data Pointer to data buffer
     * @param length Data length (max 249 bytes after header)
     * @return true if transmission succeeded
     */
    bool sendBinary(const uint8_t* data, uint8_t length) override;

    /**
     * @brief Receive packet from radio.
     *
     * Delegates to receivePacket() which uses interrupt-driven reception.
     *
     * @param packet [out] Structure to fill with packet data
     * @return true if packet received
     */
    bool receive(LoRaReceivedPacket& packet) override;

    /**
     * @brief Start continuous receive mode.
     *
     * Delegates to setLoRaReceiveMode().
     */
    void startReceive() override;

    /**
     * @brief Check if packet is available.
     *
     * Checks the volatile packetReceived flag set by ISR.
     *
     * @return true if packet waiting
     */
    bool packetAvailable() const override;

    /**
     * @brief Get RSSI of last received packet.
     * @return RSSI in dBm
     */
    float getLastRSSI() const override;

    /**
     * @brief Get SNR of last received packet.
     * @return SNR in dB
     */
    float getLastSNR() const override;

    /**
     * @brief Get device ID.
     * @return This node's ID
     */
    uint8_t getDeviceId() const override;

    /**
     * @brief Poll for packets (no-op for hardware).
     *
     * Hardware uses interrupt-driven reception, so this is a no-op.
     */
    void pollNetwork() override {}

private:
    uint8_t m_deviceId;
};

#endif // ARDUINO
#endif // LORA_HW_H
