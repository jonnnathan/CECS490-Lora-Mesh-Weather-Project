#ifndef SERIAL_JSON_H
#define SERIAL_JSON_H

#include <Arduino.h>
#include "lora_comm.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         SERIAL JSON OUTPUT                                ║
// ║  Outputs mesh data as JSON for desktop dashboard bridge                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Output node data as JSON to Serial
 * Format matches what the Python serial_bridge.py expects
 *
 * @param nodeId Node ID (1-5)
 * @param report Full report message from the node
 * @param rssi Received signal strength
 * @param snr Signal-to-noise ratio
 */
void outputNodeDataJson(uint8_t nodeId, const FullReportMsg& report, float rssi, float snr);

/**
 * Output gateway status as JSON
 * Called periodically from gateway to update dashboard
 */
void outputGatewayStatusJson();

/**
 * Output mesh statistics as JSON
 */
void outputMeshStatsJson();

/**
 * Output beacon info as JSON (for routing visualization)
 *
 * @param senderId Who sent the beacon
 * @param distance Distance to gateway from sender
 * @param rssi RSSI of received beacon
 */
void outputBeaconJson(uint8_t senderId, uint8_t distance, int16_t rssi);

#endif // SERIAL_JSON_H
