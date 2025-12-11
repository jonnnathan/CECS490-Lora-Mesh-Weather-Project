#include "serial_json.h"
#include "config.h"
#include "mesh_stats.h"
#include "gradient_routing.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         SERIAL JSON OUTPUT                                ║
// ║  Outputs mesh data as JSON for desktop dashboard bridge                   ║
// ║                                                                           ║
// ║  JSON is output on a single line for easy parsing                         ║
// ║  The Python serial_bridge.py script captures these and forwards           ║
// ║  to the web dashboard via WebSocket                                       ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void outputNodeDataJson(uint8_t nodeId, const FullReportMsg& report, float rssi, float snr) {
    // Calculate hop distance from TTL
    uint8_t hopDistance = (nodeId == GATEWAY_NODE_ID) ? 0 : (MESH_DEFAULT_TTL - report.meshHeader.ttl);

    // Build JSON string
    // Note: Using print statements to avoid String memory fragmentation
    Serial.print(F("{\"type\":\"node_data\","));
    Serial.print(F("\"nodeId\":"));
    Serial.print(nodeId);
    Serial.print(F(",\"temp\":"));
    Serial.print(report.temperatureF_x10 / 10.0, 1);
    Serial.print(F(",\"humidity\":"));
    Serial.print(report.humidity_x10 / 10.0, 1);
    Serial.print(F(",\"pressure\":"));
    Serial.print(report.pressure_hPa);
    Serial.print(F(",\"altitude\":"));
    Serial.print(report.altitude_m);

    // Sensor status (check FLAGS_SENSORS_OK for now, individual flags in future)
    Serial.print(F(",\"sensorsOk\":"));
    Serial.print((report.flags & FLAG_SENSORS_OK) ? F("true") : F("false"));

    Serial.print(F(",\"lat\":"));
    Serial.print(report.latitude_x1e6 / 1000000.0, 6);
    Serial.print(F(",\"lng\":"));
    Serial.print(report.longitude_x1e6 / 1000000.0, 6);
    Serial.print(F(",\"satellites\":"));
    Serial.print(report.satellites);
    Serial.print(F(",\"rssi\":"));
    Serial.print(rssi, 0);
    Serial.print(F(",\"snr\":"));
    Serial.print(snr, 1);
    Serial.print(F(",\"hopDistance\":"));
    Serial.print(hopDistance);
    Serial.print(F(",\"meshMsgId\":"));
    Serial.print(report.meshHeader.messageId);
    Serial.print(F(",\"meshTtl\":"));
    Serial.print(report.meshHeader.ttl);
    Serial.print(F(",\"meshSenderId\":"));
    Serial.print(report.meshHeader.senderId);
    Serial.print(F(",\"neighborCount\":"));
    Serial.print(report.neighborCount);
    Serial.print(F(",\"uptime_sec\":"));
    Serial.print(report.uptime_sec);
    Serial.print(F(",\"online\":true"));

    // Extract time source from flags (bits 4-5)
    uint8_t timeSrcFlags = report.flags & FLAG_TIME_SRC_MASK;
    Serial.print(F(",\"timeSource\":\""));
    if (timeSrcFlags == FLAG_TIME_SRC_GPS) {
        Serial.print(F("GPS"));
    } else if (timeSrcFlags == FLAG_TIME_SRC_NET) {
        Serial.print(F("NET"));
    } else {
        Serial.print(F("NONE"));
    }
    Serial.print(F("\""));

    Serial.println(F("}"));
}

void outputGatewayStatusJson() {
    Serial.print(F("{\"type\":\"gateway_status\","));
    Serial.print(F("\"nodeId\":"));
    Serial.print(DEVICE_ID);
    Serial.print(F(",\"uptime\":"));
    Serial.print(millis() / 1000);
    Serial.print(F(",\"freeHeap\":"));
    Serial.print(ESP.getFreeHeap());
    Serial.print(F(",\"isGateway\":"));
    Serial.print(IS_GATEWAY ? F("true") : F("false"));

    // Add routing state if available
    if (USE_GRADIENT_ROUTING) {
        RoutingState state = getRoutingState();
        Serial.print(F(",\"routeValid\":"));
        Serial.print(state.routeValid ? F("true") : F("false"));
        Serial.print(F(",\"distanceToGateway\":"));
        Serial.print(state.distanceToGateway);
        Serial.print(F(",\"nextHop\":"));
        Serial.print(state.nextHop);
    }

    Serial.println(F("}"));
}

void outputMeshStatsJson() {
    MeshStats stats = getMeshStats();

    Serial.print(F("{\"type\":\"mesh_stats\","));
    Serial.print(F("\"packetsReceived\":"));
    Serial.print(stats.packetsReceived);
    Serial.print(F(",\"packetsSent\":"));
    Serial.print(stats.packetsSent);
    Serial.print(F(",\"packetsForwarded\":"));
    Serial.print(stats.packetsForwarded);
    Serial.print(F(",\"duplicatesDropped\":"));
    Serial.print(stats.duplicatesDropped);
    Serial.print(F(",\"ttlExpired\":"));
    Serial.print(stats.ttlExpired);
    Serial.print(F(",\"queueOverflows\":"));
    Serial.print(stats.queueOverflows);

    // Add routing stats if gradient routing is enabled
    if (USE_GRADIENT_ROUTING) {
        RoutingStats routeStats = getRoutingStats();
        Serial.print(F(",\"beaconsReceived\":"));
        Serial.print(routeStats.beaconsReceived);
        Serial.print(F(",\"beaconsSent\":"));
        Serial.print(routeStats.beaconsSent);
        Serial.print(F(",\"unicastForwards\":"));
        Serial.print(routeStats.unicastForwards);
        Serial.print(F(",\"floodingFallbacks\":"));
        Serial.print(routeStats.floodingFallbacks);
    }

    Serial.println(F("}"));
}

void outputBeaconJson(uint8_t senderId, uint8_t distance, int16_t rssi) {
    Serial.print(F("{\"type\":\"beacon\","));
    Serial.print(F("\"senderId\":"));
    Serial.print(senderId);
    Serial.print(F(",\"distance\":"));
    Serial.print(distance);
    Serial.print(F(",\"rssi\":"));
    Serial.print(rssi);
    Serial.println(F("}"));
}
