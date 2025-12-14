#include "mesh_stats.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL STATISTICS                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static MeshStats stats;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         INITIALIZATION                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void initMeshStats() {
    resetMeshStats();
}

void resetMeshStats() {
    stats.packetsReceived = 0;
    stats.duplicatesDropped = 0;
    stats.packetsSent = 0;
    stats.packetsForwarded = 0;
    stats.ttlExpired = 0;
    stats.queueOverflows = 0;
    stats.ownPacketsIgnored = 0;
    stats.gatewayBroadcastSkips = 0;
    stats.uptimeSeconds = 0;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         ACCESSORS                                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

MeshStats getMeshStats() {
    return stats;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         INCREMENT FUNCTIONS                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void incrementPacketsReceived() {
    stats.packetsReceived++;
}

void incrementPacketsSent() {
    stats.packetsSent++;
}

void incrementPacketsForwarded() {
    stats.packetsForwarded++;
}

void incrementDuplicatesDropped() {
    stats.duplicatesDropped++;
}

void incrementTTLExpired() {
    stats.ttlExpired++;
}

void incrementQueueOverflows() {
    stats.queueOverflows++;
}

void incrementOwnPacketsIgnored() {
    stats.ownPacketsIgnored++;
}

void incrementGatewayBroadcastSkips() {
    stats.gatewayBroadcastSkips++;
}

void updateMeshStatsUptime() {
    stats.uptimeSeconds = millis() / 1000;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         REPORTING FUNCTIONS                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printMeshStats() {
    updateMeshStatsUptime();

    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║                    MESH NETWORK STATISTICS                    ║"));
    Serial.println(F("╠═══════════════════════════════════════════════════════════════╣"));

    // Reception Statistics
    Serial.println(F("║  RECEPTION:                                                   ║"));
    Serial.print(F("║    Packets Received:      "));
    Serial.print(stats.packetsReceived);
    for (int i = String(stats.packetsReceived).length(); i < 34; i++) Serial.print(' ');
    Serial.println(F("║"));

    Serial.print(F("║    Duplicates Dropped:    "));
    Serial.print(stats.duplicatesDropped);
    for (int i = String(stats.duplicatesDropped).length(); i < 34; i++) Serial.print(' ');
    Serial.println(F("║"));

    // Calculate reception percentage
    if (stats.packetsReceived > 0) {
        float dupPercent = (stats.duplicatesDropped * 100.0) / (stats.packetsReceived + stats.duplicatesDropped);
        Serial.print(F("║    Duplicate Rate:        "));
        Serial.print(dupPercent, 1);
        Serial.print(F("%"));
        for (int i = String(dupPercent, 1).length() + 1; i < 34; i++) Serial.print(' ');
        Serial.println(F("║"));
    }

    Serial.println(F("║                                                               ║"));

    // Transmission Statistics
    Serial.println(F("║  TRANSMISSION:                                                ║"));
    Serial.print(F("║    Own Packets Sent:      "));
    Serial.print(stats.packetsSent);
    for (int i = String(stats.packetsSent).length(); i < 34; i++) Serial.print(' ');
    Serial.println(F("║"));

    Serial.print(F("║    Packets Forwarded:     "));
    Serial.print(stats.packetsForwarded);
    for (int i = String(stats.packetsForwarded).length(); i < 34; i++) Serial.print(' ');
    Serial.println(F("║"));

    uint32_t totalTx = stats.packetsSent + stats.packetsForwarded;
    Serial.print(F("║    Total Transmitted:     "));
    Serial.print(totalTx);
    for (int i = String(totalTx).length(); i < 34; i++) Serial.print(' ');
    Serial.println(F("║"));

    Serial.println(F("║                                                               ║"));

    // Drop/Skip Statistics
    Serial.println(F("║  DROPS & SKIPS:                                               ║"));
    Serial.print(F("║    TTL Expired:           "));
    Serial.print(stats.ttlExpired);
    for (int i = String(stats.ttlExpired).length(); i < 34; i++) Serial.print(' ');
    Serial.println(F("║"));

    Serial.print(F("║    Queue Overflows:       "));
    Serial.print(stats.queueOverflows);
    for (int i = String(stats.queueOverflows).length(); i < 34; i++) Serial.print(' ');
    Serial.println(F("║"));

    Serial.print(F("║    Own Packets Ignored:   "));
    Serial.print(stats.ownPacketsIgnored);
    for (int i = String(stats.ownPacketsIgnored).length(); i < 34; i++) Serial.print(' ');
    Serial.println(F("║"));

    Serial.print(F("║    Gateway BC Skips:      "));
    Serial.print(stats.gatewayBroadcastSkips);
    for (int i = String(stats.gatewayBroadcastSkips).length(); i < 34; i++) Serial.print(' ');
    Serial.println(F("║"));

    Serial.println(F("║                                                               ║"));

    // Uptime
    Serial.print(F("║  Uptime: "));
    uint32_t hours = stats.uptimeSeconds / 3600;
    uint32_t minutes = (stats.uptimeSeconds % 3600) / 60;
    uint32_t seconds = stats.uptimeSeconds % 60;
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%02luh %02lum %02lus", hours, minutes, seconds);
    Serial.print(timeStr);
    for (int i = strlen(timeStr); i < 52; i++) Serial.print(' ');
    Serial.println(F("║"));

    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
    Serial.println();
}

String getMeshStatsString() {
    updateMeshStatsUptime();

    String result = "RX:" + String(stats.packetsReceived);
    result += " TX:" + String(stats.packetsSent);
    result += " FWD:" + String(stats.packetsForwarded);
    result += " DUP:" + String(stats.duplicatesDropped);
    result += " TTL:" + String(stats.ttlExpired);
    result += " QOVF:" + String(stats.queueOverflows);

    return result;
}
