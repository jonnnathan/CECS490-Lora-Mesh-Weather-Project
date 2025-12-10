#ifndef SERIAL_OUTPUT_H
#define SERIAL_OUTPUT_H

#include <Arduino.h>
#include "lora_comm.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         BOX DRAWING FUNCTIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printHeader(const char* title);
void printFooter();
void printDivider();
void printRow(const char* label, const String& value);
void printRow(const char* label, int value);
void printRow(const char* label, float value, int decimals = 1);
void printBoxLine(const char* text);
void printRxFullReport(const LoRaReceivedPacket& packet, const FullReportMsg& report, uint16_t gap, unsigned long msgCount, unsigned long lost, float lossPercent);
// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         STATUS DISPLAY FUNCTIONS                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printStartupBanner();
void printNetworkStatus();
void printSystemStats();
void printGPSStatusLine();

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         EVENT NOTIFICATIONS                               ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

void printNodeOfflineAlert(uint8_t nodeId, unsigned long lastSeenSeconds);
void printSlotEntry();
void printSlotExit(uint8_t packetsSent);
void printTxResult(bool success);
void printRxPacket(const LoRaReceivedPacket& packet, uint16_t gap, unsigned long msgCount, unsigned long lost, float lossPercent);
void printTxPacket(const char* deviceName, uint16_t seq, const String& payload);

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         HELPER FUNCTIONS                                  ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

String getSignalBars(float rssi);
String formatUptime(unsigned long uptimeSeconds);

#endif // SERIAL_OUTPUT_H