#include "mesh_commands.h"
#include "config.h"
#include "neighbor_table.h"
#include "duplicate_cache.h"
#include "transmit_queue.h"
#include "mesh_stats.h"
#include "mesh_protocol.h"
#include "lora_comm.h"
#include "memory_monitor.h"

// External declarations for functions we need
extern uint8_t encodeFullReport(uint8_t* buffer, const FullReportMsg& report);
extern bool sendBinaryMessage(const uint8_t* data, uint8_t length);
extern void incrementPacketsSent();

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         COMMAND BUFFER                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static char commandBuffer[128];
static uint8_t commandBufferIndex = 0;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         HELPER FUNCTIONS                                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static void printBoxedHeader(const char* title) {
    Serial.println();
    Serial.println(F("╔═══════════════════════════════════════════════════════════════╗"));
    Serial.print(F("║  "));
    Serial.print(title);

    // Calculate padding
    int titleLen = strlen(title);
    int padding = 59 - titleLen;
    for (int i = 0; i < padding; i++) {
        Serial.print(' ');
    }
    Serial.println(F("║"));
    Serial.println(F("╚═══════════════════════════════════════════════════════════════╝"));
}

static void printSeparator() {
    Serial.println(F("─────────────────────────────────────────────────────────────────"));
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         COMMAND HELP                                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printMeshCommandHelp() {
    printBoxedHeader("MESH NETWORK COMMANDS");
    Serial.println();
    Serial.println(F("📋 Available Commands:"));
    Serial.println();

    Serial.println(F("  mesh status"));
    Serial.println(F("    └─ Show neighbor table, queue depth, and cache status"));
    Serial.println();

    Serial.println(F("  mesh stats"));
    Serial.println(F("    └─ Display detailed mesh network statistics"));
    Serial.println();

    Serial.println(F("  mesh reset"));
    Serial.println(F("    └─ Clear all caches and reset statistics"));
    Serial.println();

    Serial.println(F("  mesh test [destId] [ttl] [message]"));
    Serial.println(F("    └─ Send test message"));
    Serial.println(F("       Examples:"));
    Serial.println(F("         mesh test 3 2 Hello      - Send to Node 3, TTL=2"));
    Serial.println(F("         mesh test 255 3 Broadcast - Broadcast with TTL=3"));
    Serial.println();

    Serial.println(F("  mesh memory"));
    Serial.println(F("    └─ Display memory usage report"));
    Serial.println();

    Serial.println(F("  mesh help"));
    Serial.println(F("    └─ Show this help message"));
    Serial.println();

    printSeparator();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NEIGHBOR TABLE DISPLAY                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printNeighborTable() {
    printBoxedHeader("NEIGHBOR TABLE");

    uint8_t activeCount = neighborTable.getActiveCount();

    Serial.print(F("Active Neighbors: "));
    Serial.print(activeCount);
    Serial.print(F(" / "));
    Serial.println(MAX_NEIGHBORS);
    Serial.println();

    if (activeCount == 0) {
        Serial.println(F("  No active neighbors found."));
        Serial.println();
        return;
    }

    // Get all active neighbors
    Neighbor* neighbors[MAX_NEIGHBORS];
    uint8_t count = neighborTable.getActiveNeighbors(neighbors, MAX_NEIGHBORS);

    // Print table header
    Serial.println(F("┌──────┬─────────┬─────────┬─────────┬──────────┬──────────┐"));
    Serial.println(F("│ Node │  RSSI   │   Min   │   Max   │ Packets  │ Last Heard│"));
    Serial.println(F("├──────┼─────────┼─────────┼─────────┼──────────┼──────────┤"));

    // Print each neighbor
    for (uint8_t i = 0; i < count; i++) {
        Neighbor* n = neighbors[i];

        Serial.print(F("│  "));
        if (n->nodeId < 10) Serial.print(' ');
        Serial.print(n->nodeId);
        Serial.print(F("  │ "));

        // RSSI
        if (n->rssi > -100) {
            Serial.print(n->rssi);
            Serial.print(F(" dBm"));
        } else {
            Serial.print(F("  N/A  "));
        }
        Serial.print(F(" │ "));

        // Min RSSI
        if (n->rssiMin > -120) {
            if (n->rssiMin > -100) Serial.print(' ');
            Serial.print(n->rssiMin);
            Serial.print(F(" dBm"));
        } else {
            Serial.print(F("  N/A  "));
        }
        Serial.print(F(" │ "));

        // Max RSSI
        if (n->rssiMax > -120) {
            if (n->rssiMax > -100) Serial.print(' ');
            Serial.print(n->rssiMax);
            Serial.print(F(" dBm"));
        } else {
            Serial.print(F("  N/A  "));
        }
        Serial.print(F(" │ "));

        // Packets
        if (n->packetsReceived < 10) Serial.print(F("   "));
        else if (n->packetsReceived < 100) Serial.print(F("  "));
        else if (n->packetsReceived < 1000) Serial.print(' ');
        Serial.print(n->packetsReceived);
        Serial.print(F("    │ "));

        // Last heard (seconds ago)
        uint32_t secondsAgo = (millis() - n->lastHeardMs) / 1000;
        if (secondsAgo < 10) Serial.print(F("   "));
        else if (secondsAgo < 100) Serial.print(F("  "));
        else if (secondsAgo < 1000) Serial.print(' ');
        Serial.print(secondsAgo);
        Serial.println(F("s ago │"));
    }

    Serial.println(F("└──────┴─────────┴─────────┴─────────┴──────────┴──────────┘"));
    Serial.println();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         QUEUE STATUS DISPLAY                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printQueueStatus() {
    printBoxedHeader("TRANSMIT QUEUE STATUS");

    uint8_t depth = transmitQueue.depth();

    Serial.print(F("Queue Depth: "));
    Serial.print(depth);
    Serial.print(F(" / "));
    Serial.print(TX_QUEUE_SIZE);
    Serial.print(F(" ("));
    Serial.print((depth * 100) / TX_QUEUE_SIZE);
    Serial.println(F("% full)"));
    Serial.println();

    // Visual queue representation
    Serial.print(F("Queue: ["));
    for (uint8_t i = 0; i < TX_QUEUE_SIZE; i++) {
        if (i < depth) {
            Serial.print(F("█"));
        } else {
            Serial.print(F("░"));
        }
    }
    Serial.println(F("]"));
    Serial.println();

    if (depth > 0) {
        Serial.print(F("⚠️  "));
        Serial.print(depth);
        Serial.println(F(" packet(s) queued for forwarding"));
    } else {
        Serial.println(F("✅ Queue is empty"));
    }
    Serial.println();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DUPLICATE CACHE STATUS                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printDuplicateCacheStatus() {
    printBoxedHeader("DUPLICATE DETECTION CACHE");

    // Note: We don't have direct access to cache internals, so show high-level info
    Serial.println(F("Cache Configuration:"));
    Serial.print(F("  Max Entries: "));
    Serial.println(SEEN_CACHE_SIZE);
    Serial.print(F("  Timeout: "));
    Serial.print(DUPLICATE_WINDOW_MS / 1000);
    Serial.println(F(" seconds"));
    Serial.println();

    Serial.println(F("Cache automatically prunes expired entries every 60 seconds."));
    Serial.println();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TEST MESSAGE SENDER                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void sendTestMessage(uint8_t destId, uint8_t ttl, const char* testData) {
    printBoxedHeader("SENDING TEST MESSAGE");

    Serial.print(F("Destination: "));
    if (destId == ADDR_BROADCAST) {
        Serial.println(F("BROADCAST (0xFF)"));
    } else if (destId == ADDR_GATEWAY) {
        Serial.println(F("GATEWAY (0x00)"));
    } else {
        Serial.print(F("Node "));
        Serial.println(destId);
    }

    Serial.print(F("TTL: "));
    Serial.println(ttl);

    Serial.print(F("Test Data: \""));
    Serial.print(testData);
    Serial.println(F("\""));
    Serial.println();

    // Build a test FULL_REPORT message
    FullReportMsg testReport;
    memset(&testReport, 0, sizeof(FullReportMsg));

    // Fill in mesh header
    testReport.meshHeader.version = MESH_PROTOCOL_VERSION;
    testReport.meshHeader.messageType = MSG_FULL_REPORT;
    testReport.meshHeader.sourceId = DEVICE_ID;
    testReport.meshHeader.destId = destId;
    testReport.meshHeader.senderId = DEVICE_ID;

    // Generate unique message ID (use millis for test)
    static uint8_t testMsgId = 200; // Start at 200 to distinguish from normal messages
    testReport.meshHeader.messageId = testMsgId++;

    testReport.meshHeader.ttl = ttl;
    testReport.meshHeader.flags = 0;

    // Fill in test data (use temperature field to encode test message length)
    testReport.temperatureF_x10 = strlen(testData) * 10;
    testReport.humidity_x10 = 990; // 99.0% indicates test message
    testReport.pressure_hPa = 1013; // Standard pressure
    testReport.satellites = 0;
    testReport.flags = 0; // No GPS
    testReport.uptime_sec = millis() / 1000;
    testReport.latitude_x1e6 = 0;
    testReport.longitude_x1e6 = 0;

    // Encode to binary
    uint8_t buffer[64];
    uint8_t length = encodeFullReport(buffer, testReport);

    Serial.print(F("Encoded "));
    Serial.print(length);
    Serial.println(F(" bytes"));

    // Send the message
    bool success = sendBinaryMessage(buffer, length);

    if (success) {
        Serial.println(F("✅ Test message transmitted successfully"));
        incrementPacketsSent();
    } else {
        Serial.println(F("❌ Test message transmission FAILED"));
    }

    Serial.println();
    printSeparator();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         RESET MESH SUBSYSTEMS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void resetMeshSubsystems() {
    printBoxedHeader("RESETTING MESH SUBSYSTEMS");

    Serial.println(F("Clearing duplicate cache..."));
    duplicateCache.clear();
    Serial.println(F("✅ Duplicate cache cleared"));
    Serial.println();

    Serial.println(F("Clearing neighbor table..."));
    neighborTable.clear();
    Serial.println(F("✅ Neighbor table cleared"));
    Serial.println();

    Serial.println(F("Clearing transmit queue..."));
    transmitQueue.clear();
    Serial.println(F("✅ Transmit queue cleared"));
    Serial.println();

    Serial.println(F("Resetting mesh statistics..."));
    resetMeshStats();
    Serial.println(F("✅ Statistics reset"));
    Serial.println();

    Serial.println(F("🔄 All mesh subsystems have been reset!"));
    Serial.println();
    printSeparator();
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         COMMAND PROCESSOR                                 ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void processMeshCommands() {
    // Check if serial data is available
    while (Serial.available() > 0) {
        char c = Serial.read();

        // Echo character (for user feedback)
        Serial.write(c);

        // Handle newline (command complete)
        if (c == '\n' || c == '\r') {
            if (commandBufferIndex > 0) {
                // Null-terminate the command
                commandBuffer[commandBufferIndex] = '\0';

                Serial.println(); // New line after command

                // Process the command
                String cmd = String(commandBuffer);
                cmd.trim();
                cmd.toLowerCase();

                // Parse command
                if (cmd.startsWith("mesh ")) {
                    String subCmd = cmd.substring(5);
                    subCmd.trim();

                    // ─────────────────────────────────────────────────────────
                    // mesh help
                    // ─────────────────────────────────────────────────────────
                    if (subCmd == "help") {
                        printMeshCommandHelp();
                    }

                    // ─────────────────────────────────────────────────────────
                    // mesh status
                    // ─────────────────────────────────────────────────────────
                    else if (subCmd == "status") {
                        printNeighborTable();
                        printQueueStatus();
                        printDuplicateCacheStatus();
                    }

                    // ─────────────────────────────────────────────────────────
                    // mesh stats
                    // ─────────────────────────────────────────────────────────
                    else if (subCmd == "stats") {
                        printMeshStats();
                    }

                    // ─────────────────────────────────────────────────────────
                    // mesh reset
                    // ─────────────────────────────────────────────────────────
                    else if (subCmd == "reset") {
                        resetMeshSubsystems();
                    }

                    // ─────────────────────────────────────────────────────────
                    // mesh memory
                    // ─────────────────────────────────────────────────────────
                    else if (subCmd == "memory") {
                        printMemoryReport();
                    }

                    // ─────────────────────────────────────────────────────────
                    // mesh test [destId] [ttl] [message]
                    // ─────────────────────────────────────────────────────────
                    else if (subCmd.startsWith("test")) {
                        // Parse test command arguments
                        int firstSpace = subCmd.indexOf(' ', 5);
                        int secondSpace = subCmd.indexOf(' ', firstSpace + 1);
                        int thirdSpace = subCmd.indexOf(' ', secondSpace + 1);

                        if (firstSpace == -1) {
                            // No arguments - use defaults
                            Serial.println(F("Using defaults: dest=255 (broadcast), ttl=3, message=\"TEST\""));
                            sendTestMessage(ADDR_BROADCAST, 3, "TEST");
                        } else if (secondSpace == -1) {
                            // Only destId provided
                            uint8_t destId = subCmd.substring(firstSpace + 1).toInt();
                            Serial.print(F("Using defaults: ttl=3, message=\"TEST\" for dest="));
                            Serial.println(destId);
                            sendTestMessage(destId, 3, "TEST");
                        } else if (thirdSpace == -1) {
                            // destId and TTL provided
                            uint8_t destId = subCmd.substring(firstSpace + 1, secondSpace).toInt();
                            uint8_t ttl = subCmd.substring(secondSpace + 1).toInt();
                            Serial.print(F("Using default message=\"TEST\" for dest="));
                            Serial.print(destId);
                            Serial.print(F(" ttl="));
                            Serial.println(ttl);
                            sendTestMessage(destId, ttl, "TEST");
                        } else {
                            // All arguments provided
                            uint8_t destId = subCmd.substring(firstSpace + 1, secondSpace).toInt();
                            uint8_t ttl = subCmd.substring(secondSpace + 1, thirdSpace).toInt();
                            String message = subCmd.substring(thirdSpace + 1);
                            message.trim();
                            sendTestMessage(destId, ttl, message.c_str());
                        }
                    }

                    // ─────────────────────────────────────────────────────────
                    // Unknown mesh command
                    // ─────────────────────────────────────────────────────────
                    else {
                        Serial.println(F("❌ Unknown mesh command"));
                        Serial.println(F("Type 'mesh help' for available commands"));
                        Serial.println();
                    }
                } else if (cmd == "help") {
                    printMeshCommandHelp();
                }

                // Clear the command buffer
                commandBufferIndex = 0;
            }
        }
        // Handle backspace
        else if (c == '\b' || c == 127) {
            if (commandBufferIndex > 0) {
                commandBufferIndex--;
                Serial.write('\b');
                Serial.write(' ');
                Serial.write('\b');
            }
        }
        // Add character to buffer (if space available)
        else if (commandBufferIndex < sizeof(commandBuffer) - 1) {
            commandBuffer[commandBufferIndex++] = c;
        }
    }
}
