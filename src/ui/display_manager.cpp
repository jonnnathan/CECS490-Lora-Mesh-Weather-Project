#include "display_manager.h"
#include "config.h"
#include "node_store.h"
#include "neo6m.h"
#include "tdma_scheduler.h"
#include "packet_handler.h"

// External references
extern TDMAScheduler tdmaScheduler;
extern unsigned long txSeq;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL DISPLAY OBJECTS                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

OLED display;
DisplayMessage rxMessage;
DisplayMessage txMessage;
DisplayState currentDisplay = DISPLAY_WAITING;
unsigned long displayStateStart = 0;

static unsigned long lastDisplayUpdate = 0;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DISPLAY MESSAGE METHODS                           ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

DisplayMessage::DisplayMessage() 
    : payload(""), meta(""), originId(0), seq(0),
      rssi(0.0), snr(0.0), timestamp(0), isNew(false), isValid(false) {}

void DisplayMessage::clear() {
    payload = "";
    meta = "";
    originId = 0;
    seq = 0;
    rssi = 0.0;
    snr = 0.0;
    timestamp = 0;
    isNew = false;
    isValid = false;
}

void DisplayMessage::updateFromPacket(const LoRaReceivedPacket& packet) {
    payload = packet.payload;
    originId = packet.header.originId;
    seq = packet.header.seq;
    rssi = packet.rssi;
    snr = packet.snr;
    timestamp = millis();
    isNew = true;
    isValid = true;

    // Use snprintf instead of String concatenation
    char metaBuf[32];
    snprintf(metaBuf, sizeof(metaBuf), "Node %d | Seq #%d", originId, seq);
    meta = metaBuf;
}

void DisplayMessage::updateFromTx(const String& msg, uint16_t seqNum) {
    payload = msg;
    originId = DEVICE_ID;
    seq = seqNum;
    rssi = 0.0;
    snr = 0.0;
    timestamp = millis();
    isNew = true;
    isValid = true;

    // Use snprintf instead of String concatenation
    char metaBuf[16];
    snprintf(metaBuf, sizeof(metaBuf), "TX #%d", seq);
    meta = metaBuf;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         INITIALIZATION                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool initDisplay() {
    rxMessage.clear();
    txMessage.clear();
    currentDisplay = DISPLAY_WAITING;

    if (display.init()) {
        display.clearDisplay();

        // Use snprintf instead of String concatenation
        char titleBuf[32];
        snprintf(titleBuf, sizeof(titleBuf), "LoRa %s", DEVICE_NAME);
        display.drawString(0, 0, titleBuf);

        display.drawString(0, 10, "Starting...");
        display.updateDisplay();
        return true;
    }
    return false;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DISPLAY UPDATE                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void displayWaiting() {
    // Line 0: Device name and mode
    String mode = tdmaScheduler.getDeviceMode();
    const char* modeIcon = "";
    if (mode == "TX_MODE") modeIcon = ">>TX";
    else if (mode == "RX_MODE") modeIcon = "..RX";
    else if (mode == "TX_DONE") modeIcon = "okTX";
    else modeIcon = "?GPS";

    char line0[32];
    snprintf(line0, sizeof(line0), "%s %s", DEVICE_NAME, modeIcon);
    display.drawString(0, 0, line0);
    
    // Line 1: Time (if valid)
    if (g_datetime_valid) {
        int hour12 = g_hour;
        const char* ampm = "A";
        
        if (g_hour == 0) { hour12 = 12; ampm = "A"; }
        else if (g_hour < 12) { hour12 = g_hour; ampm = "A"; }
        else if (g_hour == 12) { hour12 = 12; ampm = "P"; }
        else { hour12 = g_hour - 12; ampm = "P"; }
        
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "%2d:%02d:%02d%s", hour12, g_minute, g_second, ampm);
        display.drawString(0, 10, timeStr);

        // Satellites on same line
        if (gps.satellites.isValid()) {
            char satStr[16];
            snprintf(satStr, sizeof(satStr), "Sat:%d", gps.satellites.value());
            display.drawString(78, 10, satStr);
        }
    } else {
        display.drawString(0, 10, "Waiting for GPS...");
    }

    // Line 2: TX/RX counters with labels
    char countersStr[32];
    snprintf(countersStr, sizeof(countersStr), "Tx:%lu Rx:%lu", txSeq, getRxCount());
    display.drawString(0, 20, countersStr);
    
    // Line 3: Node status with better icons
    char nodeStatus[32];
    int pos = snprintf(nodeStatus, sizeof(nodeStatus), "Nodes:");
    for (uint8_t i = 1; i <= MESH_MAX_NODES && pos < 31; i++) {
        if (i == DEVICE_ID) {
            pos += snprintf(nodeStatus + pos, sizeof(nodeStatus) - pos, "*");  // Self
        } else {
            NodeMessage* node = getNodeMessage(i);
            if (node && node->isOnline) {
                pos += snprintf(nodeStatus + pos, sizeof(nodeStatus) - pos, "%d", i);  // Show node number
            } else if (node && node->hasData) {
                pos += snprintf(nodeStatus + pos, sizeof(nodeStatus) - pos, "x");  // Was online, now offline
            } else {
                pos += snprintf(nodeStatus + pos, sizeof(nodeStatus) - pos, "-");  // Never seen
            }
        }
    }
    display.drawString(0, 30, nodeStatus);
    
    // Line 4: Last heard (if any nodes online)
    char lastHeard[32];
    int lhPos = 0;
    for (uint8_t i = 1; i <= MESH_MAX_NODES && lhPos < 30; i++) {
        if (i == DEVICE_ID) continue;
        NodeMessage* node = getNodeMessage(i);
        if (node && node->hasData && node->isOnline) {
            unsigned long age = node->getAgeSeconds();
            if (lhPos > 0 && lhPos < 31) {
                lastHeard[lhPos++] = ' ';  // Add space separator
            }
            lhPos += snprintf(lastHeard + lhPos, sizeof(lastHeard) - lhPos, "N%d:%lus", i, age);
        }
    }
    if (lhPos > 0) {
        lastHeard[lhPos] = '\0';  // Null terminate
        display.drawString(0, 40, lastHeard);
    }
}

void displaySending() {
    display.drawString(0, 0, ">>> SENDING <<<");

    char packetStr[32];
    snprintf(packetStr, sizeof(packetStr), "Packet #%d", txMessage.seq);
    display.drawString(0, 12, packetStr);

    display.drawString(0, 24, txMessage.payload.substring(0, 21));

    // Draw a simple progress bar
    display.drawString(0, 40, "[==============]");
    display.drawString(0, 52, "Transmitting...");
}

void displayReceived() {
    display.drawString(0, 0, "<<< RECEIVED <<<");

    char fromStr[32];
    snprintf(fromStr, sizeof(fromStr), "From Node %d #%d", rxMessage.originId, rxMessage.seq);
    display.drawString(0, 12, fromStr);

    display.drawString(0, 24, rxMessage.payload.substring(0, 21));

    // Signal quality bar
    char sigBar[32];
    int bars = 0;
    if (rxMessage.rssi > -60) bars = 5;
    else if (rxMessage.rssi > -70) bars = 4;
    else if (rxMessage.rssi > -80) bars = 3;
    else if (rxMessage.rssi > -90) bars = 2;
    else if (rxMessage.rssi > -100) bars = 1;

    int pos = snprintf(sigBar, sizeof(sigBar), "Sig:");
    for (int i = 0; i < 5 && pos < 31; i++) {
        sigBar[pos++] = (i < bars) ? '|' : '.';
    }
    snprintf(sigBar + pos, sizeof(sigBar) - pos, " %.0fdB", rxMessage.rssi);
    display.drawString(0, 40, sigBar);
}

void displayTxFailed() {
    display.drawString(0, 0, "!!! TX FAILED !!!");

    char packetStr[32];
    snprintf(packetStr, sizeof(packetStr), "Packet #%lu", txSeq);
    display.drawString(0, 15, packetStr);

    display.drawString(0, 30, "Check antenna");
    display.drawString(0, 45, "and LoRa module");
}

void updateDisplay() {
    display.clearDisplay();

    switch (currentDisplay) {
        case DISPLAY_WAITING:
            displayWaiting();
            break;
        case DISPLAY_SENDING:
            displaySending();
            break;
        case DISPLAY_RECEIVED_MSG:
            displayReceived();
            break;
        case DISPLAY_TX_FAILED:
            displayTxFailed();
            break;
    }

    display.updateDisplay();
    lastDisplayUpdate = millis();
}

void forceDisplayUpdate() {
    updateDisplay();
}

void setDisplayState(DisplayState state) {
    currentDisplay = state;
    displayStateStart = millis();
    forceDisplayUpdate();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         CONVENIENCE FUNCTIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void updateRxDisplay(const LoRaReceivedPacket& packet) {
    rxMessage.updateFromPacket(packet);
    setDisplayState(DISPLAY_RECEIVED_MSG);
}

void updateTxDisplay(const String& payload, uint16_t seq) {
    txMessage.updateFromTx(payload, seq);
    setDisplayState(DISPLAY_SENDING);
}

void showTxFailed() {
    setDisplayState(DISPLAY_TX_FAILED);
}

void updateRxDisplayFullReport(const LoRaReceivedPacket& packet, const FullReportMsg& report) {
    // Update rxMessage for state tracking
    rxMessage.originId = packet.header.originId;
    rxMessage.seq = packet.header.seq;
    rxMessage.rssi = packet.rssi;
    rxMessage.snr = packet.snr;
    rxMessage.timestamp = millis();
    rxMessage.isNew = true;
    rxMessage.isValid = true;

    char payloadBuf[32];
    snprintf(payloadBuf, sizeof(payloadBuf), "%.1fF %.1f%%",
             report.temperatureF_x10 / 10.0, report.humidity_x10 / 10.0);
    rxMessage.payload = payloadBuf;

    setDisplayState(DISPLAY_RECEIVED_MSG);

    display.clearDisplay();

    // Line 0: Header with node ID
    char headerBuf[32];
    snprintf(headerBuf, sizeof(headerBuf), "<<< FROM NODE %d >>>", packet.header.originId);
    display.drawString(0, 0, headerBuf);

    // Line 1: Temperature and humidity with icons
    char envLine[32];
    snprintf(envLine, sizeof(envLine), "T:%.1fF H:%.1f%%",
             report.temperatureF_x10 / 10.0, report.humidity_x10 / 10.0);
    display.drawString(0, 12, envLine);

    // Line 2: Pressure and altitude
    char pressLine[32];
    snprintf(pressLine, sizeof(pressLine), "P:%dhPa A:%dm",
             report.pressure_hPa, report.altitude_m);
    display.drawString(0, 24, pressLine);
    
    // Line 3: GPS status
    char gpsLine[32];
    if (report.flags & FLAG_GPS_VALID) {
        snprintf(gpsLine, sizeof(gpsLine), "GPS:OK Sats:%d", report.satellites);
    } else {
        snprintf(gpsLine, sizeof(gpsLine), "GPS:No Fix");
    }
    display.drawString(0, 36, gpsLine);

    // Line 4: Signal quality bar
    char sigBar[32];
    int bars = 0;
    if (packet.rssi > -60) bars = 5;
    else if (packet.rssi > -70) bars = 4;
    else if (packet.rssi > -80) bars = 3;
    else if (packet.rssi > -90) bars = 2;
    else if (packet.rssi > -100) bars = 1;

    int pos = snprintf(sigBar, sizeof(sigBar), "Sig:");
    for (int i = 0; i < 5 && pos < 31; i++) {
        sigBar[pos++] = (i < bars) ? '|' : '.';
    }
    snprintf(sigBar + pos, sizeof(sigBar) - pos, " %.0fdB", packet.rssi);
    display.drawString(0, 48, sigBar);

    display.updateDisplay();
}