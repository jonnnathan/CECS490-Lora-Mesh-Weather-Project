#include "lora_comm.h"
#include "config.h"  // For DEVICE_ID constant
#include <cstring>

// Heltec WiFi LoRa 32 V3 pin definitions
#define LORA_SCK     9
#define LORA_MISO    11
#define LORA_MOSI    10
#define LORA_CS      8
#define LORA_RST     12
#define LORA_IRQ     14

// LoRa globals
SPIClass spi(HSPI);
SPISettings spiSettings(2000000, MSBFIRST, SPI_MODE0);
SX1262 radio = new Module(LORA_CS, LORA_IRQ, LORA_RST, RADIOLIB_NC, spi, spiSettings);

constexpr size_t LORA_MAX_PACKET_SIZE = 255;
constexpr size_t LORA_MAX_PAYLOAD_SIZE = LORA_MAX_PACKET_SIZE - LORA_HEADER_SIZE;

bool loraReady = false;
volatile float lastRSSI = 0.0;  // Volatile: written in main loop, could be read during ISR context
volatile float lastSNR = 0.0;   // Volatile: written in main loop, could be read during ISR context
uint16_t loraSeq = 0;

// *** KEY FIX: Interrupt-driven packet detection ***
// This volatile flag is set by the ISR when a NEW packet arrives
volatile bool packetReceived = false;

// Spinlock mutex for protecting packetReceived flag (ISR-safe)
static portMUX_TYPE radioMux = portMUX_INITIALIZER_UNLOCKED;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STATIC BUFFERS (STACK OPTIMIZATION)               ║
// ║  Shared TX/RX buffers to save stack space (~1KB saved)                   ║
// ║  Safe because LoRa operations are single-threaded and non-reentrant      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static uint8_t txBuffer[LORA_MAX_PACKET_SIZE];  // 255 bytes - shared TX buffer
static uint8_t rxBuffer[LORA_MAX_PACKET_SIZE];  // 255 bytes - shared RX buffer

// Interrupt Service Routine - called by radio when packet received
#if defined(ESP8266) || defined(ESP32)
ICACHE_RAM_ATTR
#endif
void setPacketReceivedFlag(void) {
    portENTER_CRITICAL_ISR(&radioMux);
    packetReceived = true;
    portEXIT_CRITICAL_ISR(&radioMux);
}

bool initLoRa() {
    Serial.println(F("Initializing LoRa..."));

    spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    
    int state = radio.begin(915.0);
    if (state == RADIOLIB_ERR_NONE) {



        Serial.println(F("LoRa initialization successful"));
        
        radio.setBandwidth(125.0);
        radio.setSpreadingFactor(7);
        radio.setCodingRate(5);
        radio.setOutputPower(14);

        // Set up interrupt on DIO1 for packet reception
        radio.setDio1Action(setPacketReceivedFlag);

        loraReady = true;
        
        // Start receiving (interrupt mode)
        packetReceived = false;
        radio.startReceive();
        
        return true;
    } else {
        Serial.print(F("LoRa initialization failed, code: "));
        Serial.println(state);
        loraReady = false;
        return false;
    }
}

bool sendSensorData(float tempF, float pressureHPa, float altitudeM, String gpsData) {
    if (!loraReady) return false;
    
    String message = "SENSOR:";
    message += String(tempF, 1) + "F,";
    message += String(pressureHPa, 1) + "hPa,";
    message += String(altitudeM, 1) + "m,";
    message += gpsData;
    
    return sendMessage(message);
}

bool sendMessage(String message) {
    if (!loraReady) return false;

    if (message.length() > LORA_MAX_PAYLOAD_SIZE) {
        Serial.println(F("LoRa TX failed: payload too large"));
        return false;
    }

    LoRaPacketHeader header;
    header.originId = DEVICE_ID;
    header.seq = loraSeq++;
    header.ttl = LORA_MAX_HOPS;
    header.payloadLen = message.length();

    const size_t packetLen = LORA_HEADER_SIZE + header.payloadLen;
    if (packetLen > LORA_MAX_PACKET_SIZE) {
        Serial.println(F("LoRa TX failed: packet too large"));
        return false;
    }

    // Use static txBuffer to save stack space
    size_t idx = 0;
    txBuffer[idx++] = header.originId;
    txBuffer[idx++] = (header.seq >> 8) & 0xFF;
    txBuffer[idx++] = header.seq & 0xFF;
    txBuffer[idx++] = header.ttl;
    txBuffer[idx++] = (header.payloadLen >> 8) & 0xFF;
    txBuffer[idx++] = header.payloadLen & 0xFF;
    memcpy(&txBuffer[idx], message.c_str(), header.payloadLen);

    Serial.print(F("LoRa TX: "));
    Serial.println(message);

    int state = radio.transmit(txBuffer, packetLen);

    delay(10);
    
    // Reset flag and restart receive mode after transmit
    packetReceived = false;
    radio.startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("LoRa TX successful"));
        return true;
    } else {
        Serial.print(F("LoRa TX failed, code: "));
        Serial.println(state);
        return false;
    }
}

bool sendBinaryMessage(const uint8_t* data, uint8_t length) {
    if (!loraReady) return false;

    if (length > LORA_MAX_PAYLOAD_SIZE) {
        Serial.println(F("LoRa TX failed: payload too large"));
        return false;
    }

    LoRaPacketHeader header;
    header.originId = DEVICE_ID;
    header.seq = loraSeq++;
    header.ttl = LORA_MAX_HOPS;
    header.payloadLen = length;

    const size_t packetLen = LORA_HEADER_SIZE + header.payloadLen;
    if (packetLen > LORA_MAX_PACKET_SIZE) {
        Serial.println(F("LoRa TX failed: packet too large"));
        return false;
    }

    // Use static txBuffer to save stack space
    size_t idx = 0;
    txBuffer[idx++] = header.originId;
    txBuffer[idx++] = (header.seq >> 8) & 0xFF;
    txBuffer[idx++] = header.seq & 0xFF;
    txBuffer[idx++] = header.ttl;
    txBuffer[idx++] = (header.payloadLen >> 8) & 0xFF;
    txBuffer[idx++] = header.payloadLen & 0xFF;
    memcpy(&txBuffer[idx], data, length);

    Serial.print(F("LoRa TX Binary: "));
    Serial.print(length);
    Serial.println(F(" bytes"));

    int state = radio.transmit(txBuffer, packetLen);

    delay(10);
    
    packetReceived = false;
    radio.startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("LoRa TX successful"));
        return true;
    } else {
        Serial.print(F("LoRa TX failed, code: "));
        Serial.println(state);
        return false;
    }
}

bool forwardPacket(const LoRaPacketHeader &header, const String &payload) {
    if (!loraReady) return false;

    if (payload.length() > LORA_MAX_PAYLOAD_SIZE) {
        Serial.println(F("LoRa relay failed: payload too large"));
        return false;
    }

    LoRaPacketHeader outgoing = header;
    outgoing.payloadLen = payload.length();

    const size_t packetLen = LORA_HEADER_SIZE + outgoing.payloadLen;
    if (packetLen > LORA_MAX_PACKET_SIZE) {
        Serial.println(F("LoRa relay failed: packet too large"));
        return false;
    }

    // Use static txBuffer to save stack space
    size_t idx = 0;
    txBuffer[idx++] = outgoing.originId;
    txBuffer[idx++] = (outgoing.seq >> 8) & 0xFF;
    txBuffer[idx++] = outgoing.seq & 0xFF;
    txBuffer[idx++] = outgoing.ttl;
    txBuffer[idx++] = (outgoing.payloadLen >> 8) & 0xFF;
    txBuffer[idx++] = outgoing.payloadLen & 0xFF;
    memcpy(&txBuffer[idx], payload.c_str(), outgoing.payloadLen);

    Serial.print(F("LoRa relay: origin="));
    Serial.print(outgoing.originId);
    Serial.print(F(" seq="));
    Serial.print(outgoing.seq);
    Serial.print(F(" ttl="));
    Serial.println(outgoing.ttl);

    int state = radio.transmit(txBuffer, packetLen);

    delay(10);
    
    // Reset flag and restart receive mode after transmit
    packetReceived = false;
    radio.startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("LoRa relay successful"));
        return true;
    }

    Serial.print(F("LoRa relay failed, code: "));
    Serial.println(state);
    return false;
}

void setLoRaReceiveMode() {
    if (loraReady) {
        packetReceived = false;
        radio.startReceive();
    }
}

bool receivePacket(LoRaReceivedPacket &packet) {
    if (!loraReady) return false;

    // *** KEY FIX: Only process if the interrupt flag indicates a NEW packet ***
    // This prevents re-reading stale data from the radio buffer
    // Use critical section to safely read and clear the flag
    bool hasPacket;
    portENTER_CRITICAL(&radioMux);
    hasPacket = packetReceived;
    if (hasPacket) {
        packetReceived = false;  // Clear flag atomically
    }
    portEXIT_CRITICAL(&radioMux);

    if (!hasPacket) {
        return false;
    }

    size_t packetLen = radio.getPacketLength(true);
    
    if (packetLen == 0) {
        radio.startReceive();
        return false;
    }

    if (packetLen < LORA_HEADER_SIZE || packetLen > LORA_MAX_PACKET_SIZE) {
        Serial.print(F("LoRa RX: Invalid length: "));
        Serial.println(packetLen);
        // Use rxBuffer for discarding invalid packets too
        radio.readData(rxBuffer, min(packetLen, LORA_MAX_PACKET_SIZE));
        radio.startReceive();
        return false;
    }

    // Use static rxBuffer to save stack space
    int state = radio.readData(rxBuffer, packetLen);

    if (state == RADIOLIB_ERR_NONE) {
        // DEBUG - print both methods
        Serial.print(F("[DEBUG] getRSSI(): "));
        Serial.println(radio.getRSSI());
        Serial.print(F("[DEBUG] getRSSI(true): "));
        Serial.println(radio.getRSSI(true));
        Serial.print(F("[DEBUG] getSNR(): "));
        Serial.println(radio.getSNR());

        LoRaPacketHeader header;
        size_t idx = 0;
        header.originId = rxBuffer[idx++];
        header.seq = (static_cast<uint16_t>(rxBuffer[idx++]) << 8);
        header.seq |= rxBuffer[idx++];
        header.ttl = rxBuffer[idx++];
        header.payloadLen = (static_cast<uint16_t>(rxBuffer[idx++]) << 8);
        header.payloadLen |= rxBuffer[idx++];

        const size_t expectedLen = LORA_HEADER_SIZE + header.payloadLen;
        
        if (header.payloadLen > LORA_MAX_PAYLOAD_SIZE) {
            Serial.println(F("LoRa RX: Payload too large"));
            radio.startReceive();
            return false;
        }
        
        if (expectedLen != packetLen) {
            Serial.print(F("LoRa RX: Length mismatch "));
            Serial.print(expectedLen);
            Serial.print(F(" vs "));
            Serial.println(packetLen);
            radio.startReceive();
            return false;
        }
        
        // Only reject own packets - let main.cpp handle duplicates and TTL
        if (header.originId == DEVICE_ID) {
            Serial.println(F("LoRa RX: Ignoring own packet"));
            radio.startReceive();
            return false;
        }

        // Store RSSI/SNR with critical section protection (volatile floats)
        portENTER_CRITICAL(&radioMux);
        lastRSSI = radio.getRSSI(true);
        lastSNR = radio.getSNR();
        portEXIT_CRITICAL(&radioMux);

        packet.header = header;

        // Store raw bytes
        packet.payloadLen = header.payloadLen;
        if (packet.payloadLen > 64) packet.payloadLen = 64;
        memcpy(packet.payloadBytes, &rxBuffer[idx], packet.payloadLen);

        // Also create string version for legacy/text messages
        packet.payload = String((char*)&rxBuffer[idx], header.payloadLen);
        
        packet.rssi = lastRSSI;
        packet.snr = lastSNR;

        Serial.print(F("LoRa RX: "));
        Serial.print(packet.payloadLen);
        Serial.print(F(" bytes, origin="));
        Serial.print(header.originId);
        Serial.print(F(" seq="));
        Serial.print(header.seq);
        Serial.print(F(" RSSI:"));
        Serial.print(lastRSSI);
        Serial.print(F(" SNR:"));
        Serial.println(lastSNR);

        radio.startReceive();
        return true;
    }

    if (state != RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.print(F("LoRa RX error: "));
        Serial.println(state);
    }
    
    radio.startReceive();
    return false;
}

String receiveMessage() {
    LoRaReceivedPacket packet;
    if (receivePacket(packet)) {
        return packet.payload;
    }
    return "";
}

bool isLoRaReady() {
    return loraReady;
}

float getLastRSSI() {
    float rssi;
    portENTER_CRITICAL(&radioMux);
    rssi = lastRSSI;
    portEXIT_CRITICAL(&radioMux);
    return rssi;
}

float getLastSNR() {
    float snr;
    portENTER_CRITICAL(&radioMux);
    snr = lastSNR;
    portEXIT_CRITICAL(&radioMux);
    return snr;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         FULL_REPORT ENCODING                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Static sequence counter for message IDs (wraps at 255)
static uint8_t meshMessageSeq = 0;

uint8_t encodeFullReport(uint8_t* buffer, const FullReportMsg& report) {
    uint8_t idx = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // MeshHeader (8 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    buffer[idx++] = MESH_PROTOCOL_VERSION;          // version
    buffer[idx++] = MSG_FULL_REPORT;                // messageType
    buffer[idx++] = DEVICE_ID;                      // sourceId
    buffer[idx++] = ADDR_BROADCAST;                 // destId (broadcast to all)
    buffer[idx++] = DEVICE_ID;                      // senderId (same as source initially)
    buffer[idx++] = meshMessageSeq++;               // messageId (auto-increment, wraps at 255)
    buffer[idx++] = MESH_DEFAULT_TTL;               // ttl
    buffer[idx++] = 0;                              // flags (no special flags for broadcast)

    // ─────────────────────────────────────────────────────────────────────────
    // Environmental sensors (8 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    buffer[idx++] = report.temperatureF_x10 & 0xFF;
    buffer[idx++] = (report.temperatureF_x10 >> 8) & 0xFF;
    buffer[idx++] = report.humidity_x10 & 0xFF;
    buffer[idx++] = (report.humidity_x10 >> 8) & 0xFF;
    buffer[idx++] = report.pressure_hPa & 0xFF;
    buffer[idx++] = (report.pressure_hPa >> 8) & 0xFF;
    buffer[idx++] = report.altitude_m & 0xFF;
    buffer[idx++] = (report.altitude_m >> 8) & 0xFF;

    // ─────────────────────────────────────────────────────────────────────────
    // GPS data (12 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    buffer[idx++] = report.latitude_x1e6 & 0xFF;
    buffer[idx++] = (report.latitude_x1e6 >> 8) & 0xFF;
    buffer[idx++] = (report.latitude_x1e6 >> 16) & 0xFF;
    buffer[idx++] = (report.latitude_x1e6 >> 24) & 0xFF;
    buffer[idx++] = report.longitude_x1e6 & 0xFF;
    buffer[idx++] = (report.longitude_x1e6 >> 8) & 0xFF;
    buffer[idx++] = (report.longitude_x1e6 >> 16) & 0xFF;
    buffer[idx++] = (report.longitude_x1e6 >> 24) & 0xFF;
    buffer[idx++] = report.gps_altitude_m & 0xFF;
    buffer[idx++] = (report.gps_altitude_m >> 8) & 0xFF;
    buffer[idx++] = report.satellites;
    buffer[idx++] = report.hdop_x10;

    // ─────────────────────────────────────────────────────────────────────────
    // System status (10 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    buffer[idx++] = report.uptime_sec & 0xFF;
    buffer[idx++] = (report.uptime_sec >> 8) & 0xFF;
    buffer[idx++] = (report.uptime_sec >> 16) & 0xFF;
    buffer[idx++] = (report.uptime_sec >> 24) & 0xFF;
    buffer[idx++] = report.txCount & 0xFF;
    buffer[idx++] = (report.txCount >> 8) & 0xFF;
    buffer[idx++] = report.rxCount & 0xFF;
    buffer[idx++] = (report.rxCount >> 8) & 0xFF;
    buffer[idx++] = report.battery_pct;
    buffer[idx++] = report.neighborCount;

    // ─────────────────────────────────────────────────────────────────────────
    // Flags (1 byte)
    // ─────────────────────────────────────────────────────────────────────────
    buffer[idx++] = report.flags;

    return idx;  // Should be 39 bytes (8-byte header + 31-byte payload)
}

bool decodeFullReport(const uint8_t* buffer, uint8_t length, FullReportMsg& report) {

    // Need at least 39 bytes (8 MeshHeader + 31 payload)
    if (length < 39) {
        Serial.print(F("decodeFullReport: Buffer too short ("));
        Serial.print(length);
        Serial.println(F(" bytes, need 39)"));
        return false;
    }

    uint8_t idx = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // MeshHeader (8 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    report.meshHeader.version = buffer[idx++];
    report.meshHeader.messageType = buffer[idx++];
    report.meshHeader.sourceId = buffer[idx++];
    report.meshHeader.destId = buffer[idx++];
    report.meshHeader.senderId = buffer[idx++];
    report.meshHeader.messageId = buffer[idx++];
    report.meshHeader.ttl = buffer[idx++];
    report.meshHeader.flags = buffer[idx++];

    // Validate protocol version
    if (report.meshHeader.version != MESH_PROTOCOL_VERSION) {
        Serial.print(F("⚠ WARNING: Protocol version mismatch! Got v"));
        Serial.print(report.meshHeader.version);
        Serial.print(F(", expected v"));
        Serial.println(MESH_PROTOCOL_VERSION);
        // Continue anyway - might still be parseable
    }

    // Validate message type
    if (report.meshHeader.messageType != MSG_FULL_REPORT) {
        Serial.print(F("decodeFullReport: Wrong message type (0x"));
        Serial.print(report.meshHeader.messageType, HEX);
        Serial.println(F(", expected MSG_FULL_REPORT)"));
        return false;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Environmental sensors (8 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    report.temperatureF_x10 = buffer[idx] | (buffer[idx+1] << 8);
    idx += 2;
    report.humidity_x10 = buffer[idx] | (buffer[idx+1] << 8);
    idx += 2;
    report.pressure_hPa = buffer[idx] | (buffer[idx+1] << 8);
    idx += 2;
    report.altitude_m = buffer[idx] | (buffer[idx+1] << 8);
    idx += 2;

    // ─────────────────────────────────────────────────────────────────────────
    // GPS data (12 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    report.latitude_x1e6 = buffer[idx] | (buffer[idx+1] << 8) |
                           (buffer[idx+2] << 16) | (buffer[idx+3] << 24);
    idx += 4;
    report.longitude_x1e6 = buffer[idx] | (buffer[idx+1] << 8) |
                            (buffer[idx+2] << 16) | (buffer[idx+3] << 24);
    idx += 4;
    report.gps_altitude_m = buffer[idx] | (buffer[idx+1] << 8);
    idx += 2;
    report.satellites = buffer[idx++];
    report.hdop_x10 = buffer[idx++];

    // ─────────────────────────────────────────────────────────────────────────
    // System status (10 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    report.uptime_sec = buffer[idx] | (buffer[idx+1] << 8) |
                        (buffer[idx+2] << 16) | (buffer[idx+3] << 24);
    idx += 4;
    report.txCount = buffer[idx] | (buffer[idx+1] << 8);
    idx += 2;
    report.rxCount = buffer[idx] | (buffer[idx+1] << 8);
    idx += 2;
    report.battery_pct = buffer[idx++];
    report.neighborCount = buffer[idx++];

    // ─────────────────────────────────────────────────────────────────────────
    // Flags (1 byte)
    // ─────────────────────────────────────────────────────────────────────────
    report.flags = buffer[idx++];

    return true;
}

MessageType getMessageType(const uint8_t* buffer, uint8_t length) {
    if (length < 2) return MSG_FULL_REPORT;  // Default fallback to most common type
    // MeshHeader: byte 0 = version, byte 1 = messageType
    return static_cast<MessageType>(buffer[1]);
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         BEACON ENCODING                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Static sequence counter for beacon sequence numbers
static uint16_t beaconSeq = 0;

uint8_t encodeBeacon(uint8_t* buffer, const BeaconMsg& beacon) {
    uint8_t idx = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // MeshHeader (8 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    buffer[idx++] = MESH_PROTOCOL_VERSION;          // version
    buffer[idx++] = MSG_BEACON;                     // messageType
    buffer[idx++] = DEVICE_ID;                      // sourceId
    buffer[idx++] = ADDR_BROADCAST;                 // destId (broadcast to all)
    buffer[idx++] = DEVICE_ID;                      // senderId (same as source initially)
    buffer[idx++] = beaconSeq & 0xFF;               // messageId (beacon seq, lower byte)
    buffer[idx++] = MESH_MAX_HOPS;                  // ttl (beacons propagate far)
    buffer[idx++] = 0;                              // flags (no special flags for beacons)

    // ─────────────────────────────────────────────────────────────────────────
    // Beacon payload - routing info (4 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    buffer[idx++] = beacon.distanceToGateway;       // distance (hop count)
    buffer[idx++] = beacon.gatewayId;               // which gateway
    buffer[idx++] = beacon.sequenceNumber & 0xFF;   // sequence (lower byte)
    buffer[idx++] = (beacon.sequenceNumber >> 8) & 0xFF;  // sequence (upper byte)

    // ─────────────────────────────────────────────────────────────────────────
    // Beacon payload - time sync info (4 bytes) - NEW
    // ─────────────────────────────────────────────────────────────────────────
    buffer[idx++] = beacon.gpsHour;                 // GPS hour (0-23)
    buffer[idx++] = beacon.gpsMinute;               // GPS minute (0-59)
    buffer[idx++] = beacon.gpsSecond;               // GPS second (0-59)
    buffer[idx++] = beacon.gpsValid;                // GPS valid flag (0 or 1)

    beaconSeq++;  // Increment for next beacon

    return idx;  // Should be 16 bytes (8-byte header + 8-byte payload)
}

bool decodeBeacon(const uint8_t* buffer, uint8_t length, BeaconMsg& beacon) {
    // Need at least 12 bytes for backwards compatibility (old beacons without time)
    // New beacons are 16 bytes (8 MeshHeader + 4 routing + 4 time sync)
    if (length < 12) {
        Serial.print(F("decodeBeacon: Buffer too short ("));
        Serial.print(length);
        Serial.println(F(" bytes, need at least 12)"));
        return false;
    }

    uint8_t idx = 0;

    // ─────────────────────────────────────────────────────────────────────────
    // MeshHeader (8 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    beacon.meshHeader.version = buffer[idx++];
    beacon.meshHeader.messageType = buffer[idx++];
    beacon.meshHeader.sourceId = buffer[idx++];
    beacon.meshHeader.destId = buffer[idx++];
    beacon.meshHeader.senderId = buffer[idx++];
    beacon.meshHeader.messageId = buffer[idx++];
    beacon.meshHeader.ttl = buffer[idx++];
    beacon.meshHeader.flags = buffer[idx++];

    // Validate protocol version
    if (beacon.meshHeader.version != MESH_PROTOCOL_VERSION) {
        Serial.print(F("⚠ WARNING: Beacon version mismatch! Got v"));
        Serial.print(beacon.meshHeader.version);
        Serial.print(F(", expected v"));
        Serial.println(MESH_PROTOCOL_VERSION);
        // Continue anyway - might still be parseable
    }

    // Validate message type
    if (beacon.meshHeader.messageType != MSG_BEACON) {
        Serial.print(F("decodeBeacon: Wrong message type (0x"));
        Serial.print(beacon.meshHeader.messageType, HEX);
        Serial.println(F(", expected MSG_BEACON)"));
        return false;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Beacon payload - routing info (4 bytes)
    // ─────────────────────────────────────────────────────────────────────────
    beacon.distanceToGateway = buffer[idx++];
    beacon.gatewayId = buffer[idx++];
    beacon.sequenceNumber = buffer[idx] | (buffer[idx+1] << 8);
    idx += 2;

    // ─────────────────────────────────────────────────────────────────────────
    // Beacon payload - time sync info (4 bytes) - NEW
    // Handle backwards compatibility with old 12-byte beacons
    // ─────────────────────────────────────────────────────────────────────────
    if (length >= 16) {
        // New beacon format with time sync
        beacon.gpsHour = buffer[idx++];
        beacon.gpsMinute = buffer[idx++];
        beacon.gpsSecond = buffer[idx++];
        beacon.gpsValid = buffer[idx++];
    } else {
        // Old beacon format without time sync - mark as invalid
        beacon.gpsHour = 0;
        beacon.gpsMinute = 0;
        beacon.gpsSecond = 0;
        beacon.gpsValid = 0;  // No time available in old format
    }

    return true;
}