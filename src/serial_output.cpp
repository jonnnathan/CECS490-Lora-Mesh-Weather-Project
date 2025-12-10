#include "serial_output.h"
#include "config.h"
#include "node_store.h"
#include "neo6m.h"
#include "tdma_scheduler.h"
#include "network_time.h"

// External references needed for status printing
extern TDMAScheduler tdmaScheduler;
extern unsigned long totalTxAttempts;
extern unsigned long successfulTx;
extern unsigned long validRxMessages;
extern unsigned long duplicateRxMessages;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         BOX DRAWING FUNCTIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printHeader(const char* title) {
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.print(F("║  "));
    Serial.print(title);
    int len = strlen(title);
    for (int i = 0; i < 61 - len; i++) Serial.print(' ');
    Serial.println(F("║"));
    Serial.println(F("╠═══════════════════════════════════════════════════════════════╣"));
}

void printFooter() {
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
}

void printDivider() {
    Serial.println(F("╟───────────────────────────────────────────────────────────────╢"));
}

void printRow(const char* label, const String& value) {
    Serial.print(F("║  "));
    Serial.print(label);
    int labelLen = strlen(label);
    for (int i = 0; i < 18 - labelLen; i++) Serial.print(' ');
    Serial.print(F(": "));
    Serial.print(value);
    int valueLen = value.length();
    for (int i = 0; i < 40 - valueLen; i++) Serial.print(' ');
    Serial.println(F("║"));
}

void printRow(const char* label, int value) {
    printRow(label, String(value));
}

void printRow(const char* label, float value, int decimals) {
    printRow(label, String(value, decimals));
}

void printBoxLine(const char* text) {
    Serial.print(F("║  "));
    Serial.print(text);
    int len = strlen(text);
    for (int i = 0; i < 61 - len; i++) Serial.print(' ');
    Serial.println(F("║"));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         HELPER FUNCTIONS                                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

String getSignalBars(float rssi) {
    if (rssi > -60) return "████████ Excellent";
    if (rssi > -70) return "██████░░ Great";
    if (rssi > -80) return "█████░░░ Good";
    if (rssi > -90) return "███░░░░░ Fair";
    if (rssi > -100) return "██░░░░░░ Weak";
    return "█░░░░░░░ Poor";
}

String formatUptime(unsigned long uptimeSeconds) {
    unsigned long hours = uptimeSeconds / 3600;
    unsigned long mins = (uptimeSeconds % 3600) / 60;
    unsigned long secs = uptimeSeconds % 60;
    
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%luh %lum %lus", hours, mins, secs);
    return String(buffer);
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STARTUP BANNER                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printStartupBanner() {
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║                                                               ║"));
    Serial.println(F("║     ██╗      ██████╗ ██████╗  █████╗     ███╗   ███╗███████╗  ║"));
    Serial.println(F("║     ██║     ██╔═══██╗██╔══██╗██╔══██╗    ████╗ ████║██╔════╝  ║"));
    Serial.println(F("║     ██║     ██║   ██║██████╔╝███████║    ██╔████╔██║█████╗    ║"));
    Serial.println(F("║     ██║     ██║   ██║██╔══██╗██╔══██║    ██║╚██╔╝██║██╔══╝    ║"));
    Serial.println(F("║     ███████╗╚██████╔╝██║  ██║██║  ██║    ██║ ╚═╝ ██║███████╗  ║"));
    Serial.println(F("║     ╚══════╝ ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝    ╚═╝     ╚═╝╚══════╝  ║"));
    Serial.println(F("║                                                               ║"));
    Serial.println(F("║              GPS-Timed TDMA Mesh Network                      ║"));
    Serial.println(F("║                                                               ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
    Serial.println();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STATUS DISPLAY FUNCTIONS                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printNetworkStatus() {
    printHeader("NETWORK STATUS");
    
    for (uint8_t i = 0; i < MESH_MAX_NODES; i++) {
        uint8_t nodeId = i + 1;
        NodeMessage* node = &nodeStore[i];
        
        String status;
        if (nodeId == DEVICE_ID) {
            status = "THIS DEVICE (self)";
        } else if (!node->hasData) {
            status = "Never heard";
        } else if (node->isOnline) {
            status = "ONLINE - " + String(node->getAgeSeconds()) + "s ago";
        } else {
            status = "OFFLINE - " + String(node->getAgeSeconds()) + "s ago";
        }
        
        String label = "Node " + String(nodeId);
        printRow(label.c_str(), status);
        
        if (nodeId != DEVICE_ID && node->hasData) {
            printRow("  Messages", String(node->messageCount));
            printRow("  Lost", String(node->packetsLost) + " (" + String(node->getPacketLossPercent(), 1) + "%)");
            printRow("  Last RSSI", String(node->lastRssi, 1) + " dBm");
        }
    }
    
    printFooter();
}

void printSystemStats() {
    printHeader("SYSTEM STATISTICS");
    
    printRow("Device", String(DEVICE_NAME) + " (ID: " + String(DEVICE_ID) + ")");
    
    // Convert to 12-hour format
    int hour12 = g_hour;
    const char* ampm = "AM";
    
    if (g_hour == 0) {
        hour12 = 12;
    } else if (g_hour < 12) {
        hour12 = g_hour;
    } else if (g_hour == 12) {
        hour12 = 12;
        ampm = "PM";
    } else {
        hour12 = g_hour - 12;
        ampm = "PM";
    }
    
    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "%2d:%02d:%02d %s", hour12, g_minute, g_second, ampm);
    printRow("Local Time", String(timeStr));
    printRow("GPS Satellites", gps.satellites.isValid() ? String(gps.satellites.value()) : "?");
    
    printDivider();
    
    printRow("TX Attempts", String(totalTxAttempts));
    printRow("TX Success", String(successfulTx));
    if (totalTxAttempts > 0) {
        printRow("TX Rate", String((float)successfulTx / totalTxAttempts * 100.0, 1) + "%");
    }
    
    printDivider();
    
    printRow("RX Total", String(validRxMessages));
    printRow("Duplicates", String(duplicateRxMessages));
    
    printDivider();
    
    printRow("Uptime", formatUptime(millis() / 1000));
    
    printFooter();
}

void printGPSStatusLine() {
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));

    // Get current time source from TDMA scheduler
    TDMAStatus tdmaStatus = tdmaScheduler.getStatus();
    TimeSource timeSource = tdmaStatus.timeSource;

    // Determine what time to display based on source
    uint8_t dispHour = 0, dispMin = 0, dispSec = 0;
    const char* srcStr = "WAIT";

    // GPS time is only valid for display if we have actual satellites
    // This prevents showing stale cached GPS time when satellites are lost
    bool gpsHasSatellites = gps.satellites.isValid() && gps.satellites.value() >= 1;

    if (g_datetime_valid && gpsHasSatellites) {
        // GPS time is available with satellites
        dispHour = g_hour;
        dispMin = g_minute;
        dispSec = g_second;
        srcStr = "GPS";
    } else if (isNetworkTimeValid()) {
        // Using network time (fallback when GPS has no satellites)
        getNetworkTime(dispHour, dispMin, dispSec);
        srcStr = "NET";
    }

    if (timeSource == TIME_SOURCE_NONE) {
        // No valid time source
        char buffer[70];
        snprintf(buffer, sizeof(buffer), "║  Time: WAITING | Sats: %d | Mode: %s",
                 gps.satellites.isValid() ? gps.satellites.value() : 0,
                 tdmaScheduler.getDeviceMode().c_str());
        Serial.print(buffer);
        int len = strlen(buffer);
        for (int i = 0; i < 65 - len; i++) Serial.print(' ');
        Serial.println(F("║"));
    } else {
        // Convert to 12-hour format
        int hour12 = dispHour;
        const char* ampm = "AM";

        if (dispHour == 0) {
            hour12 = 12;
            ampm = "AM";
        } else if (dispHour < 12) {
            hour12 = dispHour;
            ampm = "AM";
        } else if (dispHour == 12) {
            hour12 = 12;
            ampm = "PM";
        } else {
            hour12 = dispHour - 12;
            ampm = "PM";
        }

        char buffer[70];
        snprintf(buffer, sizeof(buffer), "║  %s: %2d:%02d:%02d %s | Sats: %d | Mode: %s",
                 srcStr, hour12, dispMin, dispSec, ampm,
                 gps.satellites.isValid() ? gps.satellites.value() : 0,
                 tdmaScheduler.getDeviceMode().c_str());

        Serial.print(buffer);
        int len = strlen(buffer);
        for (int i = 0; i < 65 - len; i++) Serial.print(' ');
        Serial.println(F("║"));
    }

    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         EVENT NOTIFICATIONS                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printNodeOfflineAlert(uint8_t nodeId, unsigned long lastSeenSeconds) {
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.print(F("║  !! NODE "));
    Serial.print(nodeId);
    Serial.print(F(" OFFLINE - Last seen "));
    Serial.print(lastSeenSeconds);
    Serial.print(F("s ago"));
    int pad = 30 - String(lastSeenSeconds).length();
    for (int p = 0; p < pad; p++) Serial.print(' ');
    Serial.println(F("║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
}

void printSlotEntry() {
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  -> ENTERING TX SLOT                                          ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
}

void printSlotExit(uint8_t packetsSent) {
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.print(F("║  <- EXITING TX SLOT - Transmitted: "));
    Serial.print(packetsSent);
    Serial.println(F(" packet(s)                      ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
}

void printTxResult(bool success) {
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    if (success) {
        Serial.println(F("║  >> TRANSMISSION SUCCESSFUL                                   ║"));
    } else {
        Serial.println(F("║  !! TRANSMISSION FAILED                                       ║"));
    }
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
}

void printTxPacket(const char* deviceName, uint16_t seq, const String& payload) {
    printHeader("TRANSMITTING");
    printRow("Device", String(deviceName));
    printRow("Sequence", String(seq));
    printRow("Payload", payload);
    printRow("Size", String(payload.length()) + " bytes");
    
    // Convert to 12-hour format
    int hour12 = g_hour;
    const char* ampm = "AM";
    
    if (g_hour == 0) {
        hour12 = 12;
        ampm = "AM";
    } else if (g_hour < 12) {
        hour12 = g_hour;
        ampm = "AM";
    } else if (g_hour == 12) {
        hour12 = 12;
        ampm = "PM";
    } else {
        hour12 = g_hour - 12;
        ampm = "PM";
    }
    
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%2d:%02d:%02d %s", hour12, g_minute, g_second, ampm);
    printRow("Time", String(timeStr));
    printFooter();
}

void printRxPacket(const LoRaReceivedPacket& packet, uint16_t gap, unsigned long msgCount, unsigned long lost, float lossPercent) {
    printHeader("PACKET RECEIVED");
    printRow("From Node", String(packet.header.originId));
    printRow("Sequence", String(packet.header.seq));
    printRow("Payload", packet.payload);
    printRow("TTL", String(packet.header.ttl));
    printDivider();
    printRow("RSSI", String(packet.rssi, 1) + " dBm");
    printRow("SNR", String(packet.snr, 1) + " dB");
    printRow("Signal", getSignalBars(packet.rssi));
    printDivider();
    printRow("Total from Node", String(msgCount));
    printRow("Packets Lost", String(lost));
    printRow("Loss Rate", String(lossPercent, 2) + "%");
    
    if (gap > 0) {
        printDivider();
        Serial.print(F("║  !! SEQUENCE GAP: Missing "));
        Serial.print(gap);
        Serial.print(F(" packet(s)!"));
        int pad = 32 - String(gap).length();
        for (int p = 0; p < pad; p++) Serial.print(' ');
        Serial.println(F("║"));
    }
    
    printFooter();
}

void printRxFullReport(const LoRaReceivedPacket& packet, const FullReportMsg& report, uint16_t gap, unsigned long msgCount, unsigned long lost, float lossPercent) {
    printHeader("FULL_REPORT RECEIVED");

    // Mesh routing info
    printRow("Protocol Ver", String(report.meshHeader.version));
    printRow("Source Node", String(report.meshHeader.sourceId));
    printRow("Dest", report.meshHeader.destId == ADDR_BROADCAST ? "BROADCAST" : String(report.meshHeader.destId));
    printRow("Sender", String(report.meshHeader.senderId));
    printRow("Msg ID", String(report.meshHeader.messageId));
    printRow("TTL", String(report.meshHeader.ttl));
    if (report.meshHeader.flags & FLAG_IS_FORWARDED) {
        printRow("Status", "FORWARDED");
    }

    printDivider();
    
    // Environmental data
    printRow("Temperature", String(report.temperatureF_x10 / 10.0, 1) + " F");
    printRow("Humidity", String(report.humidity_x10 / 10.0, 1) + " %");
    printRow("Pressure", String(report.pressure_hPa) + " hPa");
    printRow("Altitude", String(report.altitude_m) + " m");
    
    printDivider();
    
    // GPS data
    if (report.flags & FLAG_GPS_VALID) {
        printRow("GPS", "Valid");
        printRow("Latitude", String(report.latitude_x1e6 / 1000000.0, 6));
        printRow("Longitude", String(report.longitude_x1e6 / 1000000.0, 6));
        printRow("GPS Altitude", String(report.gps_altitude_m) + " m");
        printRow("Satellites", String(report.satellites));
        printRow("HDOP", String(report.hdop_x10 / 10.0, 1));
    } else {
        printRow("GPS", "No Fix");
    }
    
    printDivider();
    
    // Remote system status
    printRow("Remote Uptime", String(report.uptime_sec) + " sec");
    printRow("Remote TX Count", String(report.txCount));
    printRow("Remote RX Count", String(report.rxCount));
    printRow("Remote Battery", String(report.battery_pct) + " %");
    printRow("Neighbor Count", String(report.neighborCount) + " nodes");
    
    printDivider();
    
    // Signal quality
    printRow("RSSI", String(packet.rssi, 1) + " dBm");
    printRow("SNR", String(packet.snr, 1) + " dB");
    printRow("Signal", getSignalBars(packet.rssi));
    
    printDivider();
    
    // Statistics
    printRow("Total from Node", String(msgCount));
    printRow("Packets Lost", String(lost));
    printRow("Loss Rate", String(lossPercent, 2) + "%");
    
    if (gap > 0) {
        printDivider();
        Serial.print(F("║  !! SEQUENCE GAP: Missing "));
        Serial.print(gap);
        Serial.print(F(" packet(s)!"));
        int pad = 32 - String(gap).length();
        for (int p = 0; p < pad; p++) Serial.print(' ');
        Serial.println(F("║"));
    }
    
    printFooter();
}
