/*
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                 ESP32 LoRa GPS-Timed TDMA Mesh Network                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║  A peer-to-peer mesh network using GPS-synchronized TDMA time slots       ║
 * ║                                                                           ║
 * ║  Modules:                                                                 ║
 * ║    - config.h         : All configuration constants                       ║
 * ║    - node_store       : Per-node message storage & tracking               ║
 * ║    - packet_handler   : Duplicate detection & RX processing               ║
 * ║    - display_manager  : OLED display handling                             ║
 * ║    - serial_output    : Fancy serial terminal output                      ║
 * ║    - lora_comm        : LoRa radio communication                          ║
 * ║    - tdma_scheduler   : GPS-timed transmission scheduling                 ║
 * ║    - neo6m            : GPS module interface                              ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */

#include <Arduino.h>
#include <Wire.h>

// Project modules
#include "config.h"
#include "node_store.h"
#include "packet_handler.h"
#include "display_manager.h"
#include "serial_output.h"
#include "serial_json.h"
#include "web_dashboard.h"
#include "thingspeak.h"
#include "neighbor_table.h"
#include "duplicate_cache.h"
#include "transmit_queue.h"
#include "mesh_stats.h"
#include "mesh_debug.h"
#include "mesh_commands.h"
#include "memory_monitor.h"
// Hardware interfaces
#include "lora_comm.h"
#include "tdma_scheduler.h"
#include "neo6m.h"
#include "gradient_routing.h"
#include "network_time.h"


// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL OBJECTS                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

TDMAScheduler tdmaScheduler;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STATISTICS                                        ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

unsigned long txSeq = 0;
unsigned long totalTxAttempts = 0;
unsigned long successfulTx = 0;

// These are accessed by serial_output.cpp
unsigned long validRxMessages = 0;
unsigned long duplicateRxMessages = 0;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TIMING VARIABLES                                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static unsigned long lastRxCheck = 0;
static unsigned long lastDisplayUpdate = 0;
static unsigned long lastGPSStatusPrint = 0;
static unsigned long lastNodeCheck = 0;
static unsigned long lastStatsPrint = 0;
static unsigned long lastNeighborPrune = 0;
static unsigned long lastBeaconSent = 0;

// Note: BEACON_INTERVAL_MS is now defined in config.h/config.cpp

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         SLOT TRACKING                                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static uint8_t primaryTxThisSlot = 0;
static bool wasInSlot = false;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         BUILD FULL REPORT                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void buildFullReport(FullReportMsg& report) {
    // Clear the struct
    memset(&report, 0, sizeof(report));
    
    // Environmental sensors (placeholder values for now - add real sensors later)
    report.temperatureF_x10 = 725;      // 72.5°F placeholder
    report.humidity_x10 = 450;          // 45.0% placeholder
    report.pressure_hPa = 1013;         // 1013 hPa placeholder
    report.altitude_m = 100;            // 100m placeholder
    
    // GPS data
    if (g_location_valid) {
        report.latitude_x1e6 = (int32_t)(g_latitude * 1000000.0);
        report.longitude_x1e6 = (int32_t)(g_longitude * 1000000.0);
        report.gps_altitude_m = (int16_t)getGPSAltitude();
        report.satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
        report.hdop_x10 = gps.hdop.isValid() ? (uint8_t)(gps.hdop.hdop() * 10) : 255;
        report.flags |= FLAG_GPS_VALID;
    } else {
        report.latitude_x1e6 = 0;
        report.longitude_x1e6 = 0;
        report.gps_altitude_m = 0;
        report.satellites = 0;
        report.hdop_x10 = 255;
    }
    
    // System status
    report.uptime_sec = millis() / 1000;
    report.txCount = (uint16_t)txSeq;
    report.rxCount = (uint16_t)getRxCount();
    report.battery_pct = 100;  // Placeholder - add real battery reading later
    report.neighborCount = neighborTable.getActiveCount();
    
    // Flags
    report.flags |= FLAG_SENSORS_OK;

    // Set time source flags based on TDMA scheduler's current time source
    TDMAStatus tdmaStatus = tdmaScheduler.getStatus();
    switch (tdmaStatus.timeSource) {
        case TIME_SOURCE_GPS:
            report.flags |= FLAG_TIME_SRC_GPS;
            break;
        case TIME_SOURCE_NETWORK:
            report.flags |= FLAG_TIME_SRC_NET;
            break;
        default:
            report.flags |= FLAG_TIME_SRC_NONE;
            break;
    }
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TRANSMISSION                                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool transmit() {
    if (!isLoRaReady()) {
        return false;
    }

    txSeq++;
    
    // Build the FULL_REPORT
    FullReportMsg report;
    buildFullReport(report);
    
    NodeMessage* selfNode = getNodeMessage(DEVICE_ID);
    if (selfNode != nullptr) {
        selfNode->lastReport = report;
        selfNode->hasData = true;
        selfNode->lastHeardTime = millis();
        selfNode->messageCount++;  // <-- ADD THIS
    }

    // Encode to binary
    uint8_t buffer[64];
    uint8_t length = encodeFullReport(buffer, report);
    
    // Print TX info
    Serial.println();
    printHeader("TRANSMITTING FULL_REPORT");
    printRow("Sequence", String(txSeq));
    printRow("Temp", String(report.temperatureF_x10 / 10.0, 1) + " F");
    printRow("Humidity", String(report.humidity_x10 / 10.0, 1) + " %");
    printRow("Pressure", String(report.pressure_hPa) + " hPa");
    printRow("GPS Valid", (report.flags & FLAG_GPS_VALID) ? "Yes" : "No");
    if (report.flags & FLAG_GPS_VALID) {
        printRow("Satellites", String(report.satellites));
    }
    printRow("Uptime", String(report.uptime_sec) + " sec");
    printRow("Payload Size", String(length) + " bytes");
    printFooter();
    
    // Send the binary message
    bool success = sendBinaryMessage(buffer, length);

    // Print result
    printTxResult(success);

    // Update display and stats
    if (success) {
        incrementPacketsSent();
        DEBUG_TX_F("Transmitted own report | seq=%lu size=%d", txSeq, length);
        // Create a summary string for the display
        String summary = "T:" + String(report.temperatureF_x10 / 10.0, 1) + "F";
        updateTxDisplay(summary, txSeq);

        // Output JSON for desktop dashboard (our own node data)
        // This ensures the gateway's data appears on the dashboard
        outputNodeDataJson(DEVICE_ID, report, 0, 0);  // RSSI/SNR = 0 for self
    } else {
        DEBUG_TX("Transmission FAILED");
        showTxFailed();
    }

    return success;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TRANSMIT QUEUED FORWARDS                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void transmitQueuedForwards(uint8_t slotEndSecond) {
    // Calculate how much time remains in our slot
    uint8_t currentSecond = g_second;

    // If we're past our slot end, don't transmit
    if (currentSecond >= slotEndSecond) {
        DEBUG_TIME_F("No time for forwards | current=%d end=%d", currentSecond, slotEndSecond);
        return;
    }

    // Reserve 1 second buffer before slot end to avoid overrun
    uint8_t safeEndSecond = (slotEndSecond > 0) ? (slotEndSecond - 1) : 0;

    DEBUG_TIME_F("Forward window | current=%d safe_end=%d queue=%d",
                 currentSecond, safeEndSecond, transmitQueue.depth());

    // Transmit queued forwards while time and messages remain
    uint8_t forwardsSent = 0;
    const uint8_t MAX_FORWARDS_PER_SLOT = 5;  // Safety limit

    while (transmitQueue.depth() > 0 && forwardsSent < MAX_FORWARDS_PER_SLOT) {
        // Check if we still have time
        currentSecond = g_second;
        if (currentSecond >= safeEndSecond) {
            Serial.println(F("⏱️ Slot time ending - stopping forwards"));
            break;
        }

        // Get front message from queue
        QueuedMessage* msg = transmitQueue.peek();
        if (msg == nullptr || !msg->occupied) {
            // Queue issue - clear and break
            transmitQueue.dequeue();
            continue;
        }

        // Transmit the forwarded packet
        Serial.print(F("🔄 Forwarding queued packet ("));
        Serial.print(forwardsSent + 1);
        Serial.print(F("/"));
        Serial.print(transmitQueue.depth());
        Serial.print(F(") size="));
        Serial.print(msg->length);
        Serial.println(F(" bytes"));

        bool success = sendBinaryMessage(msg->data, msg->length);

        if (success) {
            incrementPacketsForwarded();
            DEBUG_TX_F("Forward transmitted | size=%d queue_after=%d",
                      msg->length, transmitQueue.depth() - 1);
            Serial.println(F("✅ Forward transmitted"));
        } else {
            DEBUG_TX_F("Forward transmission FAILED | size=%d", msg->length);
            Serial.println(F("❌ Forward failed"));
        }

        // Remove from queue regardless of success/failure
        transmitQueue.dequeue();
        forwardsSent++;

        // Small delay between forwards to avoid radio collisions
        delay(50);
    }

    if (forwardsSent > 0) {
        Serial.print(F("📊 Forwarded "));
        Serial.print(forwardsSent);
        Serial.print(F(" packet(s) this slot. Queue remaining: "));
        Serial.println(transmitQueue.depth());
    }
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GATEWAY BEACON BROADCASTING                        ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Send a gradient routing beacon from the gateway
 * This establishes routing paths for all nodes in the mesh
 */
void sendGatewayBeacon() {
    if (!IS_GATEWAY) return;  // Only gateway sends beacons
    if (!isLoRaReady()) return;

    // Build beacon message
    BeaconMsg beacon;
    beacon.distanceToGateway = 0;  // Gateway is distance 0
    beacon.gatewayId = DEVICE_ID;
    beacon.sequenceNumber = (uint16_t)(millis() / 1000);  // Use uptime as sequence

    // ─────────────────────────────────────────────────────────────────────────
    // Include GPS time for network time synchronization
    // Use local time (g_hour/g_minute/g_second) which has UTC offset applied
    // This ensures all nodes use consistent local time for TDMA scheduling
    // ─────────────────────────────────────────────────────────────────────────
    if (g_datetime_valid) {
        beacon.gpsHour = g_hour;      // Local time (UTC offset already applied)
        beacon.gpsMinute = g_minute;
        beacon.gpsSecond = g_second;
        beacon.gpsValid = 1;
    } else {
        beacon.gpsHour = 0;
        beacon.gpsMinute = 0;
        beacon.gpsSecond = 0;
        beacon.gpsValid = 0;
    }

    // Encode to buffer (now 16 bytes with time sync)
    uint8_t buffer[20];
    uint8_t length = encodeBeacon(buffer, beacon);

    // Send beacon
    bool success = sendBinaryMessage(buffer, length);

    if (success) {
        Serial.println(F(""));
        Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
        Serial.println(F("║           GATEWAY BEACON TRANSMITTED                      ║"));
        Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));
        Serial.print(F("  Distance: 0 (gateway)"));
        Serial.print(F("  |  Seq: "));
        Serial.print(beacon.sequenceNumber);
        Serial.print(F("  |  Size: "));
        Serial.print(length);
        Serial.println(F(" bytes"));
        if (beacon.gpsValid) {
            Serial.print(F("  Time: "));
            Serial.print(beacon.gpsHour);
            Serial.print(F(":"));
            if (beacon.gpsMinute < 10) Serial.print(F("0"));
            Serial.print(beacon.gpsMinute);
            Serial.print(F(":"));
            if (beacon.gpsSecond < 10) Serial.print(F("0"));
            Serial.print(beacon.gpsSecond);
            Serial.println(F(" (GPS)"));
        }
        Serial.println(F("─────────────────────────────────────────────────────────────"));
    } else {
        Serial.println(F("⚠️ Gateway beacon transmission FAILED"));
    }
}

/**
 * Send any pending beacon rebroadcast (for non-gateway nodes)
 */
void sendPendingBeacon() {
    if (IS_GATEWAY) return;  // Gateway doesn't rebroadcast
    if (!isLoRaReady()) return;

    BeaconMsg beacon;
    if (getPendingBeacon(beacon)) {
        // Encode to buffer
        uint8_t buffer[16];
        uint8_t length = encodeBeacon(buffer, beacon);

        // Send beacon
        bool success = sendBinaryMessage(buffer, length);

        if (success) {
            Serial.println(F(""));
            Serial.println(F("╔═══════════════════════════════════════════════════════════╗"));
            Serial.println(F("║           BEACON REBROADCAST                              ║"));
            Serial.println(F("╚═══════════════════════════════════════════════════════════╝"));
            Serial.print(F("  Distance: "));
            Serial.print(beacon.distanceToGateway);
            Serial.print(F(" hops  |  TTL: "));
            Serial.print(beacon.meshHeader.ttl);
            Serial.print(F("  |  Size: "));
            Serial.print(length);
            Serial.println(F(" bytes"));
            // Show relayed time info
            if (beacon.gpsValid) {
                Serial.print(F("  Relaying Time: "));
                Serial.print(beacon.gpsHour);
                Serial.print(F(":"));
                if (beacon.gpsMinute < 10) Serial.print(F("0"));
                Serial.print(beacon.gpsMinute);
                Serial.print(F(":"));
                if (beacon.gpsSecond < 10) Serial.print(F("0"));
                Serial.print(beacon.gpsSecond);
                Serial.println(F(" (multi-hop relay)"));
            }
            Serial.println(F("─────────────────────────────────────────────────────────────"));
        } else {
            Serial.println(F("⚠️ Beacon rebroadcast FAILED"));
        }
    }
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         SETUP                                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void setup() {
    Serial.begin(115200);
    delay(200);

    // Print startup banner
    printStartupBanner();
    printHeader("SYSTEM INITIALIZATION");

    // Initialize node store
    initNodeStore();
    printRow("Node Store", "OK (" + String(MESH_MAX_NODES) + " slots)");

    // Initialize packet handler
    initPacketHandler();
    printRow("Packet Handler", "OK");

    // Initialize mesh statistics
    initMeshStats();
    printRow("Mesh Statistics", "OK");

    // Initialize memory monitor
    initMemoryMonitor();
    printRow("Memory Monitor", "OK");

    // Initialize gradient routing
    initGradientRouting();
    printRow("Gradient Routing", IS_GATEWAY ? "OK (Gateway)" : "OK (Node)");

    // Initialize network time sync (for nodes without GPS)
    initNetworkTime();
    printRow("Network Time Sync", "OK (fallback enabled)");

    // Initialize display
    if (initDisplay()) {
        printRow("OLED Display", "OK");
    } else {
        printRow("OLED Display", "FAILED");
    }

    // Initialize GPS
    Serial2.begin(9600, SERIAL_8N1, 46, -1);
    initGPS();
    printRow("GPS Module", "OK - Waiting for fix");

    // Initialize LoRa
    if (initLoRa()) {
        printRow("LoRa Radio", "OK");
        setLoRaReceiveMode();
    } else {
        printRow("LoRa Radio", "FAILED");
    }

    // Initialize TDMA Scheduler
    tdmaScheduler.init(DEVICE_ID);
    printRow("TDMA Scheduler", "OK");
    if (IS_GATEWAY) {
        initThingSpeak();
        printRow("ThingSpeak", THINGSPEAK_ENABLED ? "Enabled" : "Disabled");
    }
    printRow("  Slot Start", String(tdmaScheduler.getSlotStart()) + "s");
    printRow("  Slot End", String(tdmaScheduler.getSlotEnd()) + "s");

    printDivider();
    printRow("Device ID", String(DEVICE_ID));
    printRow("Device Name", String(DEVICE_NAME));
    printRow("Node Timeout", String(NODE_TIMEOUT_MS / 1000) + "s");
    printRow("Message Type", "FULL_REPORT (32 bytes)");
    
        // Initialize Web Dashboard (gateway only)
    if (IS_GATEWAY) {
        printDivider();
        // Use lightweight dashboard for AP mode, full dashboard for Station mode
        if (!WIFI_USE_STATION_MODE) {
            if (initWebDashboardLite()) {
                printRow("WiFi Mode", "Access Point (Lite)");
                printRow("Dashboard IP", getGatewayIP());
            } else {
                printRow("WiFi AP", "FAILED");
            }
        } else {
            if (initWebDashboard()) {
                printRow("WiFi Mode", "Station (Full)");
                printRow("Dashboard IP", getGatewayIP());
            } else {
                printRow("WiFi", "FAILED");
            }
        }
    }
    
    printFooter();



    // Initialize timing
    unsigned long now = millis();
    lastRxCheck = now;
    lastDisplayUpdate = now;
    lastGPSStatusPrint = now;
    lastNodeCheck = now;
    lastStatsPrint = now;
    lastNeighborPrune = now;
    lastBeaconSent = now;

    // Ready message
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║  >> SYSTEM READY - Listening for transmissions...             ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
    Serial.println();

    // Memory check
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.println(F("║                      MEMORY STATUS                            ║"));
    Serial.print(F("║  Free Heap:  "));
    Serial.print(ESP.getFreeHeap() / 1024);
    Serial.println(F(" KB                                          ║"));
    Serial.print(F("║  Total Heap: "));
    Serial.print(ESP.getHeapSize() / 1024);
    Serial.println(F(" KB                                          ║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MAIN LOOP                                         ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void loop() {
    unsigned long now = millis();
    // ─────────────────────────────────────────────────────────────────────────
    // GPS Processing (High Priority)
    // ─────────────────────────────────────────────────────────────────────────
    while (Serial2.available() > 0) {
        processGPSData();
    }

    // Update TDMA scheduler with current time (GPS or network fallback)
    // Require at least 1 satellite for GPS time to be valid for TDMA
    // This prevents using stale cached GPS time when satellites are lost
    bool gpsValidForTDMA = g_datetime_valid && gps.satellites.isValid() && gps.satellites.value() >= 1;
    TimeSource timeSource = tdmaScheduler.updateWithFallback(g_hour, g_minute, g_second, gpsValidForTDMA);

    // ─────────────────────────────────────────────────────────────────────────
    // Slot Transition Detection
    // ─────────────────────────────────────────────────────────────────────────
    bool inSlot = tdmaScheduler.isMyTimeSlot();

    if (inSlot && !wasInSlot) {
        primaryTxThisSlot = 0;
        printSlotEntry();
    } else if (!inSlot && wasInSlot) {
        printSlotExit(primaryTxThisSlot);
    }
    wasInSlot = inSlot;

    // ─────────────────────────────────────────────────────────────────────────
    // Periodic Tasks
    // ─────────────────────────────────────────────────────────────────────────
    
    // GPS Status
    if (now - lastGPSStatusPrint >= GPS_STATUS_INTERVAL_MS) {
        printGPSStatusLine();
        lastGPSStatusPrint = now;
    }

    // Node timeout checks
    if (now - lastNodeCheck >= NODE_CHECK_INTERVAL_MS) {
        checkNodeTimeouts();
        lastNodeCheck = now;
    }

    // Periodic stats
    if (now - lastStatsPrint >= STATS_PRINT_INTERVAL_MS) {
        validRxMessages = getValidRxCount();
        duplicateRxMessages = getDuplicateCount();
        printNetworkStatus();
        printSystemStats();
        printMeshStats();
        printRoutingTable();   // Show gradient routing status
        printRoutingStats();   // Show routing statistics

        // Output JSON for desktop dashboard (serial bridge)
        outputGatewayStatusJson();
        outputMeshStatsJson();

        lastStatsPrint = now;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Gradient Routing - Beacon Broadcasting (only if enabled in config)
    // ─────────────────────────────────────────────────────────────────────────

    if (USE_GRADIENT_ROUTING) {
        // Gateway: Send periodic beacons
        if (IS_GATEWAY && (now - lastBeaconSent >= BEACON_INTERVAL_MS)) {
            sendGatewayBeacon();
            lastBeaconSent = now;
        }

        // Non-gateway nodes: Send pending beacon rebroadcasts
        if (!IS_GATEWAY && hasPendingBeacon()) {
            sendPendingBeacon();
        }
    }

    // Neighbor table and duplicate cache pruning
    if (now - lastNeighborPrune >= NEIGHBOR_PRUNE_INTERVAL_MS) {
        // Prune expired neighbors
        uint8_t prunedNeighbors = neighborTable.pruneExpired(NEIGHBOR_TIMEOUT_MS);
        if (prunedNeighbors > 0) {
            Serial.print(F("🗑 Pruned "));
            Serial.print(prunedNeighbors);
            Serial.print(F(" expired neighbor(s). Active: "));
            Serial.println(neighborTable.getActiveCount());
        }

        // Prune expired duplicate cache entries
        uint8_t prunedDuplicates = duplicateCache.prune();
        if (prunedDuplicates > 0) {
            Serial.print(F("🧹 Cleaned "));
            Serial.print(prunedDuplicates);
            Serial.print(F(" old duplicate entries. Cached: "));
            Serial.println(duplicateCache.getCount());
        }

        // Prune old queued forwards (older than 1 minute = 60000ms)
        uint8_t queueDepthBefore = transmitQueue.depth();
        transmitQueue.pruneOld(60000);
        uint8_t queueDepthAfter = transmitQueue.depth();

        if (queueDepthBefore > queueDepthAfter) {
            Serial.print(F("🗑️ Pruned "));
            Serial.print(queueDepthBefore - queueDepthAfter);
            Serial.print(F(" stale forward(s). Queue: "));
            Serial.println(queueDepthAfter);
        }

        // Check memory health
        updateMemoryStats();

        lastNeighborPrune = now;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Receive Processing
    // ─────────────────────────────────────────────────────────────────────────
    if (now - lastRxCheck >= RX_CHECK_INTERVAL_MS) {
        checkForIncomingMessages();
        lastRxCheck = now;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Serial Command Processing
    // ─────────────────────────────────────────────────────────────────────────
    processMeshCommands();

    // ─────────────────────────────────────────────────────────────────────────
    // Transmission
    // ─────────────────────────────────────────────────────────────────────────
    if (tdmaScheduler.shouldTransmitNow()) {
        if (primaryTxThisSlot < 1) {
            totalTxAttempts++;

            if (transmit()) {
                successfulTx++;
                primaryTxThisSlot++;

                // Small delay after primary transmission
                delay(100);

                // Now transmit any queued forwards during remaining slot time
                transmitQueuedForwards(tdmaScheduler.getSlotEnd());
            }
        }
        tdmaScheduler.markTransmissionComplete();
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Display Management
    // ─────────────────────────────────────────────────────────────────────────
    
    // Check for display state timeout
    if (currentDisplay != DISPLAY_WAITING) {
        if (now - displayStateStart >= DISPLAY_TIME_MS) {
            setDisplayState(DISPLAY_WAITING);
        }
    }

    // Regular display refresh
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS) {
        updateDisplay();
        lastDisplayUpdate = now;
    }

    // Handle web dashboard (use lite version for AP mode)
    if (!WIFI_USE_STATION_MODE) {
        handleWebDashboardLite();
    } else {
        handleWebDashboard();
    }

    // Small delay to prevent tight loop
    delay(5);
}