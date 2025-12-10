#ifndef LORA_COMM_H
#define LORA_COMM_H

#include <RadioLib.h>
#include <Arduino.h>
#include "mesh_protocol.h"

// Maximum hop count for forwarded packets
const uint8_t LORA_MAX_HOPS = 8;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STATUS FLAGS                                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Status flags for FULL_REPORT (used in FullReportMsg.flags field)
enum StatusFlags : uint8_t {
    FLAG_GPS_VALID   = 0x01,
    FLAG_SENSORS_OK  = 0x02,
    FLAG_LOW_BATTERY = 0x04,
    FLAG_ALERT       = 0x08,
    // Time source (bits 4-5): 00=NONE, 01=GPS, 10=NET
    FLAG_TIME_SRC_MASK = 0x30,  // Mask for time source bits
    FLAG_TIME_SRC_NONE = 0x00,  // No valid time source
    FLAG_TIME_SRC_GPS  = 0x10,  // Using GPS time
    FLAG_TIME_SRC_NET  = 0x20   // Using network time
};

// FULL_REPORT message structure with mesh routing header
// Total size: 8 bytes (MeshHeader) + 31 bytes (payload) = 39 bytes
struct FullReportMsg {
    // Mesh routing header (8 bytes)
    MeshHeader meshHeader;      // Routing information for multi-hop delivery

    // Environmental sensors (8 bytes)
    int16_t  temperatureF_x10;  // Temperature in °F × 10 (e.g., 725 = 72.5°F)
    uint16_t humidity_x10;      // Humidity % × 10
    uint16_t pressure_hPa;      // Pressure in hPa
    int16_t  altitude_m;        // Barometric altitude in meters

    // GPS data (12 bytes)
    int32_t  latitude_x1e6;     // Latitude × 1,000,000
    int32_t  longitude_x1e6;    // Longitude × 1,000,000
    int16_t  gps_altitude_m;    // GPS altitude in meters
    uint8_t  satellites;        // Number of satellites
    uint8_t  hdop_x10;          // HDOP × 10

    // System status (10 bytes)
    uint32_t uptime_sec;        // Uptime in seconds
    uint16_t txCount;           // Packets transmitted
    uint16_t rxCount;           // Packets received
    uint8_t  battery_pct;       // Battery percentage
    uint8_t  neighborCount;     // Number of active neighbors

    // Flags (1 byte)
    uint8_t  flags;             // StatusFlags
} __attribute__((packed));

// Fixed header size constant (avoids struct padding issues)
// originId(1) + seq(2) + ttl(1) + payloadLen(2) = 6 bytes
const size_t LORA_HEADER_SIZE = 6;

struct LoRaPacketHeader {
    uint8_t originId;
    uint16_t seq;
    uint8_t ttl;
    uint16_t payloadLen;
};

struct LoRaReceivedPacket {
    LoRaPacketHeader header;
    uint8_t payloadBytes[64];    // Raw binary payload
    uint8_t payloadLen;          // Actual payload length
    String payload;              // String version (for legacy/text messages)
    float rssi;
    float snr;
};

// LoRa communication functions
bool initLoRa();
bool sendSensorData(float tempF, float pressureHPa, float altitudeM, String gpsData);
bool sendMessage(String message);
bool sendBinaryMessage(const uint8_t* data, uint8_t length);
bool forwardPacket(const LoRaPacketHeader &header, const String &payload);
void setLoRaReceiveMode();
String receiveMessage();
bool receivePacket(LoRaReceivedPacket &packet);
bool isLoRaReady();

// LoRa status functions
float getLastRSSI();
float getLastSNR();
// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         FULL_REPORT ENCODING                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Encode a FULL_REPORT message into buffer
// Returns: number of bytes written (38 bytes: 8-byte MeshHeader + 30-byte payload)
uint8_t encodeFullReport(uint8_t* buffer, const FullReportMsg& report);

// Decode a FULL_REPORT message from buffer
// Returns: true if valid FULL_REPORT, false otherwise
bool decodeFullReport(const uint8_t* buffer, uint8_t length, FullReportMsg& report);

// Get message type from payload
MessageType getMessageType(const uint8_t* buffer, uint8_t length);

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         BEACON ENCODING                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Encode a BEACON message into buffer
// Returns: number of bytes written (12 bytes: 8-byte MeshHeader + 4-byte payload)
uint8_t encodeBeacon(uint8_t* buffer, const BeaconMsg& beacon);

// Decode a BEACON message from buffer
// Returns: true if valid BEACON, false otherwise
bool decodeBeacon(const uint8_t* buffer, uint8_t length, BeaconMsg& beacon);

#endif