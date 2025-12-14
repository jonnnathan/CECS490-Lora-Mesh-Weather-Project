/**
 * @file lora_packet_types.h
 * @brief Portable packet structure definitions for simulation.
 *
 * Provides packet structure definitions that work in both ESP32 and
 * native builds. For native builds, uses simulation's Arduino_stub.h
 * for the String type.
 *
 * @note This file mirrors the structures in lora_comm.h but is
 *       self-contained for native simulation builds.
 */

#ifndef LORA_PACKET_TYPES_H
#define LORA_PACKET_TYPES_H

#ifndef ARDUINO
    #include "simulation/Arduino_stub.h"
#else
    #include <Arduino.h>
#endif

#include <stdint.h>

// ============================================================================
// Constants
// ============================================================================

/// Maximum hop count for forwarded packets
constexpr uint8_t LORA_MAX_HOPS_SIM = 8;

/// Fixed header size: originId(1) + seq(2) + ttl(1) + payloadLen(2) = 6 bytes
constexpr size_t LORA_HEADER_SIZE_SIM = 6;

// ============================================================================
// Packet Header Structure
// ============================================================================

/**
 * @brief LoRa packet header (6 bytes).
 *
 * Used for basic packet routing and identification.
 */
struct LoRaPacketHeader_Sim {
    uint8_t originId;       ///< Original sender's ID
    uint16_t seq;           ///< Sequence number
    uint8_t ttl;            ///< Time-to-live (hop count)
    uint16_t payloadLen;    ///< Payload length in bytes
};

// ============================================================================
// Received Packet Structure
// ============================================================================

/**
 * @brief Complete received packet with metadata.
 *
 * Contains the packet data plus signal quality metrics.
 */
struct LoRaReceivedPacket_Sim {
    LoRaPacketHeader_Sim header;    ///< Packet header (legacy)
    uint8_t payloadBytes[256];      ///< Raw packet data (full packet for simulation)
    uint8_t payloadLen;             ///< Full packet length
    String payload;                 ///< String version (for text messages)
    float rssi;                     ///< Received signal strength (dBm)
    float snr;                      ///< Signal-to-noise ratio (dB)
};

// ============================================================================
// Type Aliases for Compatibility
// ============================================================================

// When building for native simulation, use _Sim versions
// These are only active in native builds

#ifndef ARDUINO

// For the simulation, we use the _Sim structs directly
// The IRadio interface uses forward declarations
struct LoRaPacketHeader : public LoRaPacketHeader_Sim {};
struct LoRaReceivedPacket : public LoRaReceivedPacket_Sim {};

#endif // ARDUINO

#endif // LORA_PACKET_TYPES_H
