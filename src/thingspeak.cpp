#include "thingspeak.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Statistics
static unsigned long successCount = 0;
static unsigned long failCount = 0;

// Rate limiting - ThingSpeak free tier requires 15 seconds between updates
static unsigned long lastSendTime = 0;
const unsigned long THINGSPEAK_MIN_INTERVAL_MS = 15000;  // 15 seconds

void initThingSpeak() {
    successCount = 0;
    failCount = 0;
    lastSendTime = 0;
    
    Serial.println(F("[THINGSPEAK] Initialized"));
    Serial.print(F("[THINGSPEAK] Minimum interval: "));
    Serial.print(THINGSPEAK_MIN_INTERVAL_MS / 1000);
    Serial.println(F(" seconds"));
}

bool sendToThingSpeak(uint8_t nodeId, const FullReportMsg& report, float rssi) {
    // Check if ThingSpeak is enabled
    if (!THINGSPEAK_ENABLED) {
        return false;
    }
    
    // Check if we're connected to WiFi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[THINGSPEAK] Not connected to WiFi, skipping"));
        return false;
    }
    
    // Validate node ID (must be 2-5, gateway doesn't send its own data)
    if (nodeId < 2 || nodeId > 5) {
        Serial.print(F("[THINGSPEAK] Invalid node ID for cloud upload: "));
        Serial.println(nodeId);
        return false;
    }
    
    // Get the API key for this node
    // Array index = nodeId - 1 (Node 2 = index 1, Node 3 = index 2, etc.)
    const char* apiKey = THINGSPEAK_API_KEYS[nodeId - 1];
    
    if (apiKey == nullptr || strlen(apiKey) == 0) {
        Serial.print(F("[THINGSPEAK] No API key configured for Node "));
        Serial.println(nodeId);
        return false;
    }
    
    // Build the URL with all fields
    String url = "http://api.thingspeak.com/update?api_key=";
    url += apiKey;
    url += "&field1=" + String(report.temperatureF_x10 / 10.0, 1);
    url += "&field2=" + String(report.humidity_x10 / 10.0, 1);
    url += "&field3=" + String(report.pressure_hPa);
    url += "&field4=" + String(nodeId);
    url += "&field5=" + String((int)rssi);
    url += "&field6=" + String(report.satellites);
    url += "&field7=" + String(report.altitude_m);
    url += "&field8=" + String(report.battery_pct);
    
    Serial.print(F("[THINGSPEAK] Sending Node "));
    Serial.print(nodeId);
    Serial.print(F(" data | Temp: "));
    Serial.print(report.temperatureF_x10 / 10.0, 1);
    Serial.print(F("F | Humidity: "));
    Serial.print(report.humidity_x10 / 10.0, 1);
    Serial.println(F("%"));
    
    // Make the HTTP request
    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000);
    
    int httpCode = http.GET();
    String response = http.getString();
    http.end();
    
    // Check response
    if (httpCode == 200 && response.toInt() > 0) {
        successCount++;
        Serial.print(F("[THINGSPEAK] Success! Node "));
        Serial.print(nodeId);
        Serial.print(F(" Entry ID: "));
        Serial.println(response);
        return true;
    } else {
        failCount++;
        Serial.print(F("[THINGSPEAK] Failed for Node "));
        Serial.print(nodeId);
        Serial.print(F(". HTTP code: "));
        Serial.print(httpCode);
        Serial.print(F(" Response: "));
        Serial.println(response);
        return false;
    }
}

unsigned long getThingSpeakSuccessCount() {
    return successCount;
}

unsigned long getThingSpeakFailCount() {
    return failCount;
}