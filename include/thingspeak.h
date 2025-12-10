#ifndef THINGSPEAK_H
#define THINGSPEAK_H

#include <Arduino.h>
#include "lora_comm.h"

// Initialize ThingSpeak (call in setup)
void initThingSpeak();

// Send a FULL_REPORT to ThingSpeak
// Returns true if successful, false otherwise
bool sendToThingSpeak(uint8_t nodeId, const FullReportMsg& report, float rssi);

// Get statistics
unsigned long getThingSpeakSuccessCount();
unsigned long getThingSpeakFailCount();

#endif // THINGSPEAK_H