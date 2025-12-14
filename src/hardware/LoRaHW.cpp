/**
 * @file LoRaHW.cpp
 * @brief Hardware LoRa implementation wrapping lora_comm.cpp
 */

#ifdef ARDUINO

#include "hardware/LoRaHW.h"
#include "lora_comm.h"

// Access the volatile flag from lora_comm.cpp
extern volatile bool packetReceived;

LoRaHW::LoRaHW(uint8_t deviceId)
    : m_deviceId(deviceId)
{
}

bool LoRaHW::init() {
    return initLoRa();
}

void LoRaHW::standby() {
    // RadioLib standby mode - for now just ensure we're in receive mode
    // Future: could add explicit standby to lora_comm.cpp if needed
}

bool LoRaHW::isReady() const {
    return isLoRaReady();
}

bool LoRaHW::sendBinary(const uint8_t* data, uint8_t length) {
    return sendBinaryMessage(data, length);
}

bool LoRaHW::receive(LoRaReceivedPacket& packet) {
    return receivePacket(packet);
}

void LoRaHW::startReceive() {
    setLoRaReceiveMode();
}

bool LoRaHW::packetAvailable() const {
    return packetReceived;
}

float LoRaHW::getLastRSSI() const {
    return ::getLastRSSI();
}

float LoRaHW::getLastSNR() const {
    return ::getLastSNR();
}

uint8_t LoRaHW::getDeviceId() const {
    return m_deviceId;
}

#endif // ARDUINO
