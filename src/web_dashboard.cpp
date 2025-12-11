/*
 * web_dashboard.cpp
 * WiFi Web Dashboard with GPS Map, Connection Lines, and ThingSpeak History
 * 
 * Features:
 * - Station mode (joins existing WiFi) or AP mode (creates own network)
 * - Real-time node data via JSON API
 * - Interactive map with Leaflet.js
 * - Connection lines showing signal strength between nodes
 * - Historical data graphs from ThingSpeak
 * - Auto-updating every 2 seconds
 */

#include "web_dashboard.h"
#include "config.h"
#include "node_store.h"
#include "mesh_stats.h"
#include "transmit_queue.h"
#include "packet_handler.h"
#include "network_time.h"  // For manual time setting
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_wpa2.h>  // For WPA2-Enterprise (eduroam) support

// Web server on port 80
static WebServer server(80);
static DNSServer dnsServer;
static bool dashboardRunning = false;
static unsigned long serverStartTime = 0;

// Forward declarations
void handleRoot();
void handleData();
void handleSetTime();
void handleNotFound();
String generateHTML();
String generateJSON();

bool initWebDashboard() {
    if (!IS_GATEWAY) {
        Serial.println(F("[WIFI] Not a gateway node, skipping WiFi setup"));
        return false;
    }

    if (WIFI_USE_STATION_MODE) {
        // Station Mode - Connect to existing WiFi
        WiFi.mode(WIFI_STA);

        // Scan for available networks to verify target network is visible
        Serial.println(F("[WIFI] Scanning for available networks..."));
        int n = WiFi.scanNetworks();
        Serial.print(F("[WIFI] Found "));
        Serial.print(n);
        Serial.println(F(" networks:"));

        bool targetFound = false;
        const char* targetSSID = WIFI_USE_ENTERPRISE ? WIFI_ENTERPRISE_SSID : WIFI_STA_SSID;

        for (int i = 0; i < n; i++) {
            Serial.print(F("  "));
            Serial.print(i + 1);
            Serial.print(F(": "));
            Serial.print(WiFi.SSID(i));
            Serial.print(F(" ("));
            Serial.print(WiFi.RSSI(i));
            Serial.print(F(" dBm) "));
            Serial.print(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? F("Open") : F("Encrypted"));

            if (WiFi.SSID(i) == targetSSID) {
                Serial.print(F(" <- TARGET NETWORK FOUND!"));
                targetFound = true;
            }
            Serial.println();
        }

        if (!targetFound) {
            Serial.print(F("[WIFI] WARNING: Target network '"));
            Serial.print(targetSSID);
            Serial.println(F("' not found in scan results!"));
            Serial.println(F("[WIFI] Check: 1) Network is broadcasting 2) Device is in range 3) SSID spelling is correct"));
        }

        Serial.println();

        if (WIFI_USE_ENTERPRISE) {
            // WPA2-Enterprise (eduroam, corporate networks)
            Serial.println(F("[WIFI] Connecting to WPA2-Enterprise network..."));
            Serial.print(F("[WIFI] SSID: "));
            Serial.println(WIFI_ENTERPRISE_SSID);
            Serial.print(F("[WIFI] Identity: "));
            Serial.println(WIFI_ENTERPRISE_IDENTITY);

            // Disconnect first to ensure clean state
            WiFi.disconnect(true);
            delay(100);

            Serial.println(F("[WIFI] Configuring WPA2-Enterprise authentication..."));
            Serial.println(F("[WIFI] Method: PEAP/MSCHAPv2 with anonymous identity"));
            Serial.print(F("[WIFI] Anonymous ID: "));
            Serial.println(WIFI_ENTERPRISE_ANONYMOUS_IDENTITY);

            // Configure WPA2-Enterprise with anonymous identity
            // Anonymous identity is used for outer/unencrypted RADIUS communication
            esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)WIFI_ENTERPRISE_ANONYMOUS_IDENTITY, strlen(WIFI_ENTERPRISE_ANONYMOUS_IDENTITY));
            esp_wifi_sta_wpa2_ent_set_username((uint8_t *)WIFI_ENTERPRISE_USERNAME, strlen(WIFI_ENTERPRISE_USERNAME));
            esp_wifi_sta_wpa2_ent_set_password((uint8_t *)WIFI_ENTERPRISE_PASSWORD, strlen(WIFI_ENTERPRISE_PASSWORD));

            // Disable certificate validation (common for eduroam when no CA cert is configured)
            esp_wifi_sta_wpa2_ent_clear_ca_cert();

            esp_wifi_sta_wpa2_ent_enable();

            Serial.println(F("[WIFI] WPA2-Enterprise configured, attempting connection..."));

            // Begin connection - use the strongest eduroam AP signal
            WiFi.begin(WIFI_ENTERPRISE_SSID);
        } else {
            // WPA2-Personal (standard home/simple WiFi)
            Serial.println(F("[WIFI] Connecting to WPA2-Personal network..."));
            Serial.print(F("[WIFI] SSID: "));
            Serial.println(WIFI_STA_SSID);

            WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);
        }

        // Wait for connection (max 20 seconds)
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 40) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println(F("[WIFI] Connected to WiFi!"));
            Serial.print(F("[WIFI] IP Address: "));
            Serial.println(WiFi.localIP());
        } else {
            Serial.println(F("[WIFI] Failed to connect! Falling back to AP mode..."));
            // Fall back to AP mode
            WiFi.mode(WIFI_AP);
            WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
            delay(100);
            Serial.print(F("[WIFI] AP IP Address: "));
            Serial.println(WiFi.softAPIP());
        }
    } else {
        // Access Point Mode - Create own network
        Serial.println(F("[WIFI] Starting Access Point..."));

        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);

        delay(100);

        // Start DNS server for captive portal
        dnsServer.start(53, "*", WiFi.softAPIP());
        Serial.println(F("[WIFI] DNS server started for captive portal"));

        Serial.print(F("[WIFI] AP Started! SSID: "));
        Serial.println(WIFI_AP_SSID);
        Serial.print(F("[WIFI] IP Address: "));
        Serial.println(WiFi.softAPIP());
    }
    
    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/settime", handleSetTime);
    server.on("/test", []() {
        server.send(200, "text/html", "<html><body><h1>Server Working!</h1><p>Free heap: " + String(ESP.getFreeHeap()) + " bytes</p></body></html>");
    });
    server.onNotFound(handleNotFound);
    
    // Start server
    server.begin();
    Serial.println(F("[WIFI] ======================================"));
    Serial.println(F("[WIFI] Web server started successfully!"));
    Serial.println(F("[WIFI] ======================================"));
    Serial.println(F("[WIFI] Connect to this WiFi network:"));
    if (WIFI_USE_STATION_MODE) {
        Serial.print(F("[WIFI]   Network: "));
        Serial.println(WIFI_USE_ENTERPRISE ? WIFI_ENTERPRISE_SSID : WIFI_STA_SSID);
        Serial.print(F("[WIFI]   Dashboard URL: http://"));
        Serial.println(WiFi.localIP());
    } else {
        Serial.print(F("[WIFI]   Network: "));
        Serial.println(WIFI_AP_SSID);
        Serial.print(F("[WIFI]   Password: "));
        Serial.println(WIFI_AP_PASSWORD);
        Serial.print(F("[WIFI]   Dashboard URL: http://"));
        Serial.println(WiFi.softAPIP());
    }
    Serial.println(F("[WIFI] ======================================"));
    Serial.print(F("[WIFI] Free heap: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
    Serial.println(F("[WIFI] Waiting for HTTP requests..."));

    dashboardRunning = true;
    serverStartTime = millis();

    return true;
}

void handleWebDashboard() {
    if (dashboardRunning) {
        if (!WIFI_USE_STATION_MODE) {
            dnsServer.processNextRequest();
        }
        server.handleClient();
    }
}

bool isWebDashboardRunning() {
    return dashboardRunning;
}

String getGatewayIP() {
    // Check both full and lite dashboards
    if (dashboardRunning || isWebDashboardLiteRunning()) {
        if (WIFI_USE_STATION_MODE && WiFi.status() == WL_CONNECTED) {
            return WiFi.localIP().toString();
        }
        return WiFi.softAPIP().toString();
    }
    return "N/A";
}

void handleRoot() {
    Serial.println(F("[HTTP] GET / - Sending dashboard HTML"));
    Serial.print(F("[HTTP] Free heap before: "));
    Serial.println(ESP.getFreeHeap());

    // Use chunked transfer for large HTML
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    String html = generateHTML();
    Serial.print(F("[HTTP] HTML size: "));
    Serial.print(html.length());
    Serial.println(F(" bytes"));

    // Send in chunks to avoid memory issues
    const size_t chunkSize = 1024;
    size_t sent = 0;
    while (sent < html.length()) {
        size_t remaining = html.length() - sent;
        size_t toSend = (remaining < chunkSize) ? remaining : chunkSize;
        server.sendContent(html.substring(sent, sent + toSend));
        sent += toSend;
        yield(); // Allow ESP32 to handle WiFi
    }

    Serial.print(F("[HTTP] Free heap after: "));
    Serial.println(ESP.getFreeHeap());
    Serial.println(F("[HTTP] Dashboard sent successfully"));
}

void handleData() {
    Serial.println(F("[HTTP] GET /data - Sending JSON"));
    String json = generateJSON();
    Serial.print(F("[HTTP] JSON size: "));
    Serial.print(json.length());
    Serial.println(F(" bytes"));
    server.send(200, "application/json", json);
}

void handleSetTime() {
    Serial.println(F("[HTTP] /settime - Manual time set request"));

    // Check for required parameters: hour, minute, second
    if (!server.hasArg("hour") || !server.hasArg("minute") || !server.hasArg("second")) {
        server.send(400, "application/json", "{\"error\":\"Missing parameters. Required: hour, minute, second\"}");
        return;
    }

    // Parse parameters
    int hour = server.arg("hour").toInt();
    int minute = server.arg("minute").toInt();
    int second = server.arg("second").toInt();

    // Validate ranges
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        server.send(400, "application/json", "{\"error\":\"Invalid time. Hour: 0-23, Minute: 0-59, Second: 0-59\"}");
        return;
    }

    // Set the manual time
    setManualTime((uint8_t)hour, (uint8_t)minute, (uint8_t)second);

    // Build response JSON
    String response = "{\"success\":true,\"time\":\"";
    if (hour < 10) response += "0";
    response += String(hour) + ":";
    if (minute < 10) response += "0";
    response += String(minute) + ":";
    if (second < 10) response += "0";
    response += String(second) + "\",\"message\":\"Time set successfully. TDMA scheduling enabled.\"}";

    server.send(200, "application/json", response);
}

void handleNotFound() {
    Serial.print(F("[HTTP] 404 Not Found: "));
    Serial.println(server.uri());
    server.send(404, "text/plain", "Not Found");
}

String generateJSON() {
    String json;
    // Pre-allocate capacity to prevent multiple reallocations
    // Estimate: ~200 bytes gateway + ~400 bytes per node × 5 nodes = ~2200 bytes
    json.reserve(2500);

    json = "{";

    // Gateway info with mesh statistics
    unsigned long uptime = (millis() - serverStartTime) / 1000;
    MeshStats stats = getMeshStats();

    json += "\"gateway\":{";
    json += "\"nodeId\":" + String(DEVICE_ID) + ",";
    json += "\"uptime\":" + String(uptime) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"wifiClients\":" + String(WiFi.softAPgetStationNum()) + ",";

    // Mesh statistics
    json += "\"meshStats\":{";
    json += "\"packetsReceived\":" + String(stats.packetsReceived) + ",";
    json += "\"packetsSent\":" + String(stats.packetsSent) + ",";
    json += "\"packetsForwarded\":" + String(stats.packetsForwarded) + ",";
    json += "\"duplicatesDropped\":" + String(stats.duplicatesDropped) + ",";
    json += "\"ttlExpired\":" + String(stats.ttlExpired) + ",";
    json += "\"queueOverflows\":" + String(stats.queueOverflows) + ",";
    json += "\"queueDepth\":" + String(transmitQueue.depth());
    json += "}";

    json += "},";
    
    // All nodes
    json += "\"nodes\":{";
    bool first = true;
    
    for (uint8_t i = 1; i <= MESH_MAX_NODES; i++) {
        NodeMessage* node = getNodeMessage(i);
        if (node == nullptr) continue;
        
        if (!first) json += ",";
        first = false;
        
        // Check if online (has data and heard within last 60 seconds)
    
        bool online;
        if (i == DEVICE_ID) {
            // Gateway is always online (it's running this code!)
            online = true;
        } else {
            online = node->hasData && (millis() - node->lastHeardTime < 60000);
        }

        json += "\"" + String(i) + "\":{";
        json += "\"online\":" + String(online ? "true" : "false") + ",";
        json += "\"lastHeard\":" + String(node->lastHeardTime) + ",";
        json += "\"messageCount\":" + String(node->messageCount) + ",";
        json += "\"rssi\":" + String(node->lastRssi, 0) + ",";
        json += "\"snr\":" + String(node->lastSnr, 1) + ",";
        json += "\"packetsLost\":" + String(node->packetsLost) + ",";

        // Mesh header info
        json += "\"meshSourceId\":" + String(node->lastReport.meshHeader.sourceId) + ",";
        json += "\"meshSenderId\":" + String(node->lastReport.meshHeader.senderId) + ",";
        json += "\"meshTtl\":" + String(node->lastReport.meshHeader.ttl) + ",";
        json += "\"meshMsgId\":" + String(node->lastReport.meshHeader.messageId) + ",";

        // Calculate hop distance from TTL (how many hops packet traveled)
        // Gateway (i==1) is always 0 hops, others calculated from TTL decrease
        uint8_t hopDistance = (i == GATEWAY_NODE_ID) ? 0 : (MESH_DEFAULT_TTL - node->lastReport.meshHeader.ttl);
        json += "\"hopDistance\":" + String(hopDistance) + ",";

        // Sensor data from last report
        json += "\"temp\":" + String(node->lastReport.temperatureF_x10 / 10.0, 1) + ",";
        json += "\"humidity\":" + String(node->lastReport.humidity_x10 / 10.0, 1) + ",";
        json += "\"pressure\":" + String(node->lastReport.pressure_hPa) + ",";
        json += "\"altitude\":" + String(node->lastReport.altitude_m) + ",";

        // GPS data
        json += "\"lat\":" + String(node->lastReport.latitude_x1e6 / 1000000.0, 6) + ",";
        json += "\"lng\":" + String(node->lastReport.longitude_x1e6 / 1000000.0, 6) + ",";
        json += "\"satellites\":" + String(node->lastReport.satellites) + ",";
        json += "\"gpsAlt\":" + String(node->lastReport.gps_altitude_m) + ",";

        // Neighbor information
        json += "\"neighborCount\":" + String(node->lastReport.neighborCount);

        json += "}";
    }
    
    json += "}}";
    return json;
}

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         HTML DASHBOARD (PROGMEM)                          ║
// ║  Stored in flash memory to save ~30KB of RAM                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LoRa Mesh Dashboard</title>
    <link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
    <script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/leaflet.heat@0.2.0/dist/leaflet-heat.min.js"></script>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }

        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Sans:wght@300;400;500;600;700&display=swap');

        :root {
            --primary: #2563eb;
            --primary-hover: #1d4ed8;
            --success: #10b981;
            --danger: #ef4444;
            --warning: #f59e0b;
            --bg-page: #f1f5f9;
            --bg-card: #ffffff;
            --text-primary: #0f172a;
            --text-secondary: #64748b;
            --border: #e2e8f0;
            --shadow-sm: 0 1px 2px rgba(0, 0, 0, 0.05);
            --shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
            --shadow-lg: 0 4px 12px rgba(0, 0, 0, 0.08);
        }

        body {
            font-family: 'IBM Plex Sans', -apple-system, system-ui, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
            background: var(--bg-page);
            color: var(--text-primary);
            min-height: 100vh;
            line-height: 1.6;
        }

        .header {
            background: var(--bg-card);
            border-bottom: 1px solid var(--border);
            padding: 20px 0;
            box-shadow: var(--shadow-sm);
            position: sticky;
            top: 0;
            z-index: 100;
        }

        .header-content {
            max-width: 100%;
            margin: 0 auto;
            padding: 0 24px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 32px;
        }

        @media (min-width: 1600px) {
            .header-content {
                max-width: 1600px;
            }
        }

        .header h1 {
            color: var(--text-primary);
            font-size: 1.5rem;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 12px;
            margin: 0;
        }

        .logo {
            width: 6px;
            height: 28px;
            background: var(--primary);
            border-radius: 3px;
        }

        .stats {
            display: flex;
            gap: 32px;
            flex-wrap: wrap;
        }

        .stat {
            display: flex;
            flex-direction: column;
            gap: 2px;
        }

        .stat-label {
            color: var(--text-secondary);
            font-size: 0.75rem;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            font-weight: 500;
        }

        .stat-value {
            color: var(--text-primary);
            font-weight: 600;
            font-size: 1.125rem;
        }
        .tabs {
            background: var(--bg-card);
            border-bottom: 1px solid var(--border);
            display: flex;
            padding: 0 24px;
        }

        @media (min-width: 1600px) {
            .tabs {
                max-width: 1600px;
                margin: 0 auto;
            }
        }

        .tab {
            padding: 16px 24px;
            cursor: pointer;
            border-bottom: 2px solid transparent;
            transition: all 0.2s ease;
            font-weight: 500;
            color: var(--text-secondary);
            font-size: 0.875rem;
        }

        .tab:hover {
            color: var(--text-primary);
            background: rgba(37, 99, 235, 0.04);
        }

        .tab.active {
            border-bottom-color: var(--primary);
            color: var(--primary);
        }
        .content {
            display: none;
        }
        .content.active {
            display: block;
        }
        #mapContent, #heatmapContent {
            position: relative;
            height: calc(100vh - 160px);
        }
        #map, #heatmap {
            width: 100%;
            height: 100%;
        }
        #nodesContent, #historyContent {
            min-height: calc(100vh - 160px);
        }
        .node-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(480px, 1fr));
            gap: 24px;
            padding: 24px;
            width: 100%;
        }

        @media (min-width: 1600px) {
            .node-grid {
                max-width: 1600px;
                margin: 0 auto;
            }
        }

        .node-card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 24px;
            transition: all 0.2s ease;
            box-shadow: var(--shadow-sm);
        }

        .node-card:hover {
            box-shadow: var(--shadow-lg);
            border-color: var(--primary);
        }

        .node-card.offline {
            opacity: 0.6;
            background: #fafafa;
        }
        .node-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 12px;
        }
        .node-id {
            font-size: 1.25em;
            font-weight: 700;
            color: var(--text-primary);
            letter-spacing: -0.5px;
        }

        .node-status {
            padding: 4px 12px;
            border-radius: 12px;
            font-size: 0.75rem;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.5px;
            display: inline-flex;
            align-items: center;
            gap: 6px;
        }

        .node-status::before {
            content: '';
            width: 6px;
            height: 6px;
            border-radius: 50%;
        }

        .node-status.online {
            background: #d1fae5;
            color: #065f46;
        }

        .node-status.online::before {
            background: var(--success);
        }

        .node-status.offline {
            background: #fee2e2;
            color: #991b1b;
        }

        .node-status.offline::before {
            background: var(--danger);
        }
        .node-data {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
        }
        .data-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 16px;
            border-radius: 8px;
            background: var(--bg-page);
            transition: all 0.2s;
            border: 1px solid transparent;
        }

        .data-item:hover {
            border-color: var(--primary);
            background: #f8fafc;
        }

        .data-label {
            color: var(--text-secondary);
            font-size: 0.875rem;
            font-weight: 500;
            line-height: 1.2;
        }

        .data-value {
            color: var(--text-primary);
            font-weight: 600;
            font-size: 1rem;
            line-height: 1.2;
            text-align: right;
        }
        .signal-bar {
            display: flex;
            gap: 3px;
            align-items: flex-end;
            height: 20px;
        }

        .signal-bar div {
            width: 4px;
            background: var(--border);
            border-radius: 2px;
            transition: all 0.2s ease;
        }

        .signal-bar div.active {
            background: var(--primary);
        }

        .legend {
            position: absolute;
            bottom: 24px;
            left: 24px;
            background: var(--bg-card);
            padding: 16px;
            border-radius: 8px;
            z-index: 1000;
            border: 1px solid var(--border);
            font-size: 0.875rem;
            box-shadow: var(--shadow-lg);
        }

        .legend-title {
            font-weight: 700;
            margin-bottom: 12px;
            color: var(--text-primary);
            font-size: 1rem;
            letter-spacing: -0.3px;
        }
        .legend-section {
            margin-bottom: 8px;
            font-weight: 600;
            color: var(--text-primary);
            font-size: 0.8125rem;
        }
        .legend-item {
            display: flex;
            align-items: center;
            margin-bottom: 6px;
            color: var(--text-secondary);
            font-size: 0.8125rem;
        }
        .legend-marker {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
            border: 2px solid var(--bg-card);
        }
        .legend-line {
            width: 28px;
            margin-right: 8px;
            height: 2px;
        }
        /* History tab styles */
        .history-container {
            padding: 24px;
            width: 100%;
        }

        @media (min-width: 1600px) {
            .history-container {
                max-width: 1600px;
                margin: 0 auto;
            }
        }
        .history-header {
            display: flex;
            align-items: center;
            gap: 20px;
            margin-bottom: 24px;
        }
        .history-header h2 {
            color: var(--text-primary);
            margin: 0;
            font-size: 1.5rem;
            font-weight: 700;
        }
        .node-selector {
            padding: 10px 16px;
            font-size: 0.875rem;
            background: var(--bg-card);
            color: var(--text-primary);
            border: 1px solid var(--border);
            border-radius: 6px;
            cursor: pointer;
            font-weight: 500;
            transition: all 0.2s;
        }
        .node-selector:hover {
            border-color: var(--primary);
        }
        .node-selector:focus {
            outline: none;
            border-color: var(--primary);
            box-shadow: 0 0 0 3px rgba(37, 99, 235, 0.1);
        }
        .chart-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(450px, 1fr));
            gap: 20px;
        }
        .chart-card {
            background: var(--bg-card);
            border: 1px solid var(--border);
            border-radius: 8px;
            padding: 20px;
            transition: all 0.2s;
            box-shadow: var(--shadow-sm);
        }
        .chart-card:hover {
            box-shadow: var(--shadow-lg);
            border-color: var(--primary);
        }
        .chart-card h3 {
            color: var(--text-primary);
            margin-bottom: 16px;
            font-size: 1rem;
            font-weight: 600;
        }
        .chart-frame {
            width: 100%;
            height: 260px;
            border: none;
            border-radius: 6px;
            background: var(--bg-page);
        }
        .thingspeak-link {
            display: inline-block;
            margin-top: 16px;
            padding: 8px 16px;
            background: var(--primary);
            border: none;
            border-radius: 6px;
            color: white;
            text-decoration: none;
            font-size: 0.875rem;
            font-weight: 500;
            transition: all 0.2s;
        }
        .thingspeak-link:hover {
            background: var(--primary-hover);
            box-shadow: var(--shadow);
        }

        /* Heatmap Styles - Bottom Toolbar */
        .heatmap-toolbar {
            position: absolute;
            bottom: 0;
            left: 0;
            right: 0;
            background: var(--bg-card);
            border-top: 1px solid var(--border);
            padding: 12px 20px;
            display: flex;
            align-items: center;
            gap: 24px;
            z-index: 1000;
            flex-wrap: wrap;
        }
        .toolbar-group {
            display: flex;
            align-items: center;
            gap: 8px;
        }
        .toolbar-label {
            color: var(--text-secondary);
            font-size: 0.8rem;
            font-weight: 500;
            white-space: nowrap;
        }
        .toolbar-select {
            padding: 6px 10px;
            border: 1px solid var(--border);
            border-radius: 4px;
            background: var(--bg-card);
            color: var(--text-primary);
            font-size: 0.8rem;
            cursor: pointer;
        }
        .toolbar-select:hover {
            border-color: var(--primary);
        }
        .toolbar-checkbox {
            display: flex;
            align-items: center;
            gap: 6px;
            cursor: pointer;
        }
        .toolbar-checkbox input {
            accent-color: var(--primary);
        }
        .toolbar-range {
            width: 150px;
            accent-color: var(--primary);
        }
        .toolbar-value {
            color: var(--text-primary);
            font-weight: 600;
            font-size: 0.8rem;
            min-width: 45px;
        }
        .toolbar-divider {
            width: 1px;
            height: 24px;
            background: var(--border);
        }
        .toolbar-legend {
            display: flex;
            align-items: center;
            gap: 6px;
            margin-left: auto;
            padding-left: 16px;
            border-left: 1px solid var(--border);
        }

        /* Responsive Design */
        @media (max-width: 1024px) {
            .node-grid {
                grid-template-columns: repeat(auto-fill, minmax(240px, 1fr));
                padding: 15px;
            }

            .header {
                padding: 15px 20px;
            }

            .stats {
                flex-wrap: wrap;
                gap: 15px;
            }
        }

        @media (max-width: 768px) {
            .header {
                flex-direction: column;
                gap: 15px;
                align-items: flex-start;
            }

            .header h1 {
                font-size: 1.5em;
            }

            .stats {
                width: 100%;
                justify-content: space-between;
            }

            .stat {
                font-size: 0.85em;
            }

            .tabs {
                overflow-x: auto;
                -webkit-overflow-scrolling: touch;
            }

            .tab {
                padding: 12px 20px;
                white-space: nowrap;
            }

            .legend {
                left: 10px;
                bottom: 10px;
                padding: 15px;
                font-size: 0.8em;
            }
        }

        @media (max-width: 600px) {
            .chart-grid {
                grid-template-columns: 1fr;
            }

            .history-header {
                flex-direction: column;
                align-items: flex-start;
            }

            .node-grid {
                grid-template-columns: 1fr;
                padding: 10px;
            }

            .node-data {
                grid-template-columns: 1fr;
            }

            .stats {
                font-size: 0.8em;
                gap: 10px;
            }
        }
    </style>
</head>
<body>
    <div class="header">
        <div class="header-content">
            <h1>LoRa Mesh Network</h1>
            <div class="stats">
                <div class="stat">
                    <div class="stat-label">Uptime</div>
                    <div class="stat-value" id="uptime">--</div>
                </div>
                <div class="stat">
                    <div class="stat-label">Nodes Online</div>
                    <div class="stat-value" id="nodesOnline">--</div>
                </div>
                <div class="stat">
                    <div class="stat-label">Set Time (UTC)</div>
                    <div style="display:flex;gap:4px;align-items:center;">
                        <input type="number" id="setHour" min="0" max="23" placeholder="HH" style="width:45px;padding:4px;border:1px solid var(--border);border-radius:4px;font-size:0.875rem;">
                        <span>:</span>
                        <input type="number" id="setMinute" min="0" max="59" placeholder="MM" style="width:45px;padding:4px;border:1px solid var(--border);border-radius:4px;font-size:0.875rem;">
                        <span>:</span>
                        <input type="number" id="setSecond" min="0" max="59" placeholder="SS" style="width:45px;padding:4px;border:1px solid var(--border);border-radius:4px;font-size:0.875rem;">
                        <button onclick="setManualTime()" style="padding:4px 12px;background:var(--primary);color:white;border:none;border-radius:4px;cursor:pointer;font-size:0.75rem;">Set</button>
                        <button onclick="setCurrentTime()" style="padding:4px 8px;background:var(--success);color:white;border:none;border-radius:4px;cursor:pointer;font-size:0.75rem;" title="Use current UTC time">Now</button>
                    </div>
                    <div id="timeStatus" style="font-size:0.7rem;color:var(--text-secondary);margin-top:2px;"></div>
                </div>
            </div>
        </div>
    </div>
    
    <div class="tabs">
        <div class="tab active" onclick="showTab('map')">Map View</div>
        <div class="tab" onclick="showTab('nodes')">Node Details</div>
        <div class="tab" onclick="showTab('heatmap')">Signal Heatmap</div>
        <div class="tab" onclick="showTab('history')">History</div>
    </div>
    
    <div id="mapContent" class="content active">
        <div id="map"></div>
        <div class="legend">
            <div class="legend-title">Legend</div>
            <div class="legend-section">Nodes:</div>
            <div class="legend-item">
                <div class="legend-marker" style="background:#10b981;"></div>
                <span>Gateway</span>
            </div>
            <div class="legend-item">
                <div class="legend-marker" style="background:#2563eb;"></div>
                <span>Online</span>
            </div>
            <div class="legend-item">
                <div class="legend-marker" style="background:#ef4444;"></div>
                <span>Offline</span>
            </div>
            <div class="legend-section" style="margin-top:12px;">Signal Strength:</div>
            <div class="legend-item">
                <div class="legend-line" style="background:#10b981;"></div>
                <span>Excellent (> -60)</span>
            </div>
            <div class="legend-item">
                <div class="legend-line" style="background:#2563eb;"></div>
                <span>Good (-60 to -80)</span>
            </div>
            <div class="legend-item">
                <div class="legend-line" style="background:#f59e0b;"></div>
                <span>Fair (-80 to -100)</span>
            </div>
            <div class="legend-item">
                <div class="legend-line" style="background:#ef4444;"></div>
                <span>Poor (< -100)</span>
            </div>
        </div>
    </div>
    
    <div id="nodesContent" class="content">
        <div class="node-grid" id="nodeGrid"></div>
    </div>

    <div id="heatmapContent" class="content">
        <div id="heatmapContainer" style="position: relative; height: calc(100% - 50px);">
            <div id="heatmap"></div>
        </div>
        <div class="heatmap-toolbar">
            <div class="toolbar-group">
                <span class="toolbar-label">Est. Range:</span>
                <input type="range" id="heatmapRadius" class="toolbar-range" min="100" max="1000" value="500" step="50" onchange="updateRangeCircles(this.value)">
                <span id="radiusValue" class="toolbar-value">500m</span>
            </div>
            <div class="toolbar-divider"></div>
            <label class="toolbar-checkbox">
                <input type="checkbox" id="showNodes" checked onchange="toggleNodes()">
                <span class="toolbar-label">Node Labels</span>
            </label>
            <div class="toolbar-divider"></div>
            <label class="toolbar-checkbox">
                <input type="checkbox" id="showRangeCircles" checked onchange="toggleRangeCircles()">
                <span class="toolbar-label">Range Circles</span>
            </label>
            <div class="toolbar-legend">
                <span class="toolbar-label" style="color: rgba(37, 99, 235, 0.6);">&#9679;</span>
                <span class="toolbar-label">= Estimated LoRa Range</span>
            </div>
        </div>
    </div>

    <div id="historyContent" class="content">
        <div class="history-container">
            <div class="history-header">
                <h2>📈 Historical Data</h2>
                <select class="node-selector" id="historyNodeSelect" onchange="updateHistoryCharts()">
                    <option value="2">Node 2</option>
                    <option value="3">Node 3</option>
                    <option value="4">Node 4</option>
                    <option value="5">Node 5</option>
                </select>
                <a class="thingspeak-link" id="thingspeakLink" href="#" target="_blank">
                    🔗 View Full Channel on ThingSpeak
                </a>
            </div>
            <div class="chart-grid" id="chartGrid">
                <!-- Charts will be loaded dynamically -->
            </div>
        </div>
    </div>
    
    <script>
        // Detect online/offline mode
        let isOnline = false;
        let map = null;
        let markers = {};
        let connectionLines = {};
        let mapInitialized = false;
        let nodesData = {};  // Store latest node data for heatmap

        // Check internet connectivity by trying to load a tiny image from a reliable CDN
        function checkOnlineStatus() {
            return new Promise((resolve) => {
                const timeout = setTimeout(() => resolve(false), 3000);
                const img = new Image();
                img.onload = () => { clearTimeout(timeout); resolve(true); };
                img.onerror = () => { clearTimeout(timeout); resolve(false); };
                img.src = 'https://www.google.com/favicon.ico?' + Date.now();
            });
        }

        // Initialize on page load
        (async function() {
            isOnline = await checkOnlineStatus();
            console.log('Dashboard mode:', isOnline ? 'ONLINE (with map)' : 'OFFLINE (table mode)');

            if (!isOnline) {
                // Hide map-dependent tabs and show warning
                document.querySelector('.tab[onclick*="heatmap"]').style.display = 'none';
                document.querySelector('.tab[onclick*="history"]').style.display = 'none';
                const mapContent = document.getElementById('mapContent');
                mapContent.innerHTML = '<div style="padding: 20px; text-align: center; color: #666;">' +
                    '<h2>📡 Offline Mode</h2>' +
                    '<p>Running without internet connection. Map features disabled.</p>' +
                    '<p>Node data is shown in the "Node Details" tab.</p></div>';
            } else {
                // Initialize map now that we know we're online
                initMap();
            }

            // Start data updates
            updateData();
            setInterval(updateData, 2000);
        })();

        // ThingSpeak channel configuration
        const thingspeakChannels = {
            2: { id: 3194362, readKey: 'DZ7L3266JBJ0TITC' },
            3: { id: 3194371, readKey: 'HZFT8OH0W6CI6BXJ' },
            4: { id: 3194372, readKey: '3LOL0G23XL9SYF6F' },
            5: { id: 3194374, readKey: 'UEF28CAKQ0OUGYX8' }
        };
        
        // Chart definitions
        const chartConfigs = [
            { field: 1, title: 'Temperature', color: 'ff6b6b', unit: '°F' },
            { field: 2, title: 'Humidity', color: '00d4ff', unit: '%' },
            { field: 3, title: 'Pressure', color: '44ff44', unit: 'hPa' },
            { field: 5, title: 'Signal Strength (RSSI)', color: 'ffaa00', unit: 'dBm' },
            { field: 6, title: 'GPS Satellites', color: 'aa44ff', unit: '' },
            { field: 8, title: 'Battery', color: 'ff44aa', unit: '%' }
        ];
        
        // Initialize map (only if online)
        function initMap() {
            if (!isOnline || typeof L === 'undefined') {
                console.log('Skipping map initialization - offline mode');
                return;
            }
            map = L.map('map').setView([0, 0], 2);
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                attribution: '© OpenStreetMap contributors',
                maxZoom: 19
            }).addTo(map);
        }

        // Heatmap variables
        let heatmapMap = null;
        let rangeCircles = {};
        let heatmapMarkers = {};
        let rangeRadius = 500;
        let showCircles = true;

        // Tab switching
        function showTab(tabName) {
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.content').forEach(c => c.classList.remove('active'));

            if (tabName === 'map') {
                document.querySelector('.tabs .tab:nth-child(1)').classList.add('active');
                document.getElementById('mapContent').classList.add('active');
                setTimeout(() => map && map.invalidateSize(), 100);
            } else if (tabName === 'nodes') {
                document.querySelector('.tabs .tab:nth-child(2)').classList.add('active');
                document.getElementById('nodesContent').classList.add('active');
            } else if (tabName === 'heatmap') {
                document.querySelector('.tabs .tab:nth-child(3)').classList.add('active');
                document.getElementById('heatmapContent').classList.add('active');
                if (!heatmapMap) {
                    initHeatmap();
                } else {
                    setTimeout(() => heatmapMap.invalidateSize(), 100);
                }
                updateHeatmap();
            } else if (tabName === 'history') {
                document.querySelector('.tabs .tab:nth-child(4)').classList.add('active');
                document.getElementById('historyContent').classList.add('active');
                updateHistoryCharts();
            }
        }

        // Initialize heatmap
        function initHeatmap() {
            if (!isOnline || typeof L === 'undefined') {
                console.log('Skipping heatmap initialization - offline mode');
                return;
            }
            heatmapMap = L.map('heatmap').setView([0, 0], 2);
            L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
                attribution: '© OpenStreetMap contributors',
                maxZoom: 19
            }).addTo(heatmapMap);
        }

        // Update coverage map with range circles
        function updateHeatmap() {
            if (!heatmapMap) return;

            // Clear existing range circles
            for (let nodeId in rangeCircles) {
                heatmapMap.removeLayer(rangeCircles[nodeId]);
            }
            rangeCircles = {};

            let bounds = [];

            // Create range circles for each node
            for (const [nodeId, node] of Object.entries(nodesData)) {
                const lat = node.lat;
                const lng = node.lng;

                if (lat !== undefined && lng !== undefined && lat !== 0 && lng !== 0) {
                    bounds.push([lat, lng]);

                    // Only add circles if enabled
                    if (showCircles) {
                        // Color based on node type: gateway=green, online=blue, offline=red
                        const isGateway = nodeId == 1;
                        const fillColor = isGateway ? '#10b981' : (node.online ? '#2563eb' : '#ef4444');
                        const strokeColor = isGateway ? '#059669' : (node.online ? '#1d4ed8' : '#dc2626');

                        // Create range circle
                        const circle = L.circle([lat, lng], {
                            radius: rangeRadius,
                            fillColor: fillColor,
                            fillOpacity: 0.15,
                            color: strokeColor,
                            weight: 2,
                            opacity: 0.6,
                            dashArray: '5, 5'
                        }).addTo(heatmapMap);

                        rangeCircles[nodeId] = circle;
                    }
                }
            }

            // Fit map to show all nodes with padding for circles
            if (bounds.length > 0) {
                heatmapMap.fitBounds(bounds, { padding: [100, 100] });
            }

            // Update node markers
            updateHeatmapMarkers();
        }

        // Update markers on heatmap
        function updateHeatmapMarkers() {
            if (!heatmapMap) return;

            const showNodesChecked = document.getElementById('showNodes').checked;

            // Clear existing markers
            for (let nodeId in heatmapMarkers) {
                heatmapMap.removeLayer(heatmapMarkers[nodeId]);
            }
            heatmapMarkers = {};

            // Add markers if enabled
            if (showNodesChecked) {
                for (const [nodeId, node] of Object.entries(nodesData)) {
                    const lat = node.lat;
                    const lng = node.lng;

                    if (lat !== undefined && lng !== undefined && lat !== 0 && lng !== 0) {
                        const color = nodeId == 1 ? '#10b981' : (node.online ? '#2563eb' : '#ef4444');
                        const marker = L.circleMarker([lat, lng], {
                            radius: 8,
                            fillColor: color,
                            color: '#ffffff',
                            weight: 2,
                            opacity: 1,
                            fillOpacity: 0.9
                        }).addTo(heatmapMap);

                        marker.bindPopup(`<b>Node ${nodeId}</b><br>
                            RSSI: ${node.rssi || 'N/A'} dBm<br>
                            SNR: ${node.snr || 'N/A'} dB`);

                        heatmapMarkers[nodeId] = marker;
                    }
                }
            }
        }

        // Coverage map control functions
        function updateRangeCircles(value) {
            rangeRadius = parseInt(value);
            document.getElementById('radiusValue').textContent = value + 'm';
            updateHeatmap();
        }

        function toggleNodes() {
            updateHeatmapMarkers();
        }

        function toggleRangeCircles() {
            showCircles = document.getElementById('showRangeCircles').checked;
            updateHeatmap();
        }

        // Update history charts based on selected node
        function updateHistoryCharts() {
            const nodeId = document.getElementById('historyNodeSelect').value;
            const channel = thingspeakChannels[nodeId];
            
            if (!channel) {
                document.getElementById('chartGrid').innerHTML = '<p style="color:#ff4444;">No channel configured for this node.</p>';
                return;
            }
            
            // Update ThingSpeak link
            document.getElementById('thingspeakLink').href = 'https://thingspeak.com/channels/' + channel.id;
            
            // Build chart HTML
            let chartsHTML = '';
            chartConfigs.forEach(config => {
                const chartUrl = 'https://thingspeak.com/channels/' + channel.id + 
                    '/charts/' + config.field + 
                    '?bgcolor=%231a1a2e&color=%23' + config.color + 
                    '&dynamic=true&results=60&type=line&title=' + encodeURIComponent(config.title);
                
                chartsHTML += `
                    <div class="chart-card">
                        <h3>${config.title} ${config.unit ? '(' + config.unit + ')' : ''}</h3>
                        <iframe class="chart-frame" src="${chartUrl}"></iframe>
                    </div>
                `;
            });
            
            document.getElementById('chartGrid').innerHTML = chartsHTML;
        }
        
        // Generate signal strength bars
        function getSignalBars(rssi) {
            let bars = 1;
            if (rssi > -60) bars = 5;
            else if (rssi > -70) bars = 4;
            else if (rssi > -80) bars = 3;
            else if (rssi > -90) bars = 2;
            
            let html = '<div class="signal-bar">';
            for (let i = 1; i <= 5; i++) {
                const height = i * 3;
                const active = i <= bars ? 'active' : '';
                html += `<div class="${active}" style="height:${height}px;"></div>`;
            }
            html += '</div>';
            return html;
        }
        
        // Format uptime
        function formatUptime(seconds) {
            const h = Math.floor(seconds / 3600);
            const m = Math.floor((seconds % 3600) / 60);
            const s = seconds % 60;
            return `${h}h ${m}m ${s}s`;
        }
        
        // Update map with nodes and connection lines
        function updateMap(nodes, gateway) {
            if (!isOnline || !map) return;
            
            let gatewayCoords = null;
            
            // First pass: find gateway coordinates and update all markers
            for (const [id, node] of Object.entries(nodes)) {
                if (node.lat === 0 && node.lng === 0) continue;
                
                const coords = [node.lat, node.lng];
                const isGateway = (parseInt(id) === gateway.nodeId);
                
                if (isGateway) {
                    gatewayCoords = coords;
                }
                
                // Determine marker color
                let color = '#00d4ff'; // blue for online
                if (isGateway) {
                    color = '#44ff44'; // green for gateway
                } else if (!node.online) {
                    color = '#ff4444'; // red for offline
                }
                
                // Create marker icon
                const icon = L.divIcon({
                    className: 'custom-marker',
                    html: '<div style="width:20px;height:20px;background:' + color + ';border-radius:50%;border:3px solid white;box-shadow:0 2px 10px rgba(0,0,0,0.4);"></div>',
                    iconSize: [20, 20],
                    iconAnchor: [10, 10]
                });
                
                // Popup content
                const hopText = node.hopDistance === 0 ? 'Gateway' : node.hopDistance + ' hop' + (node.hopDistance > 1 ? 's' : '');
                const popup = '<div style="font-family:Arial;min-width:160px;">' +
                    '<b style="color:#00d4ff;">Node ' + id + '</b>' + (isGateway ? ' (Gateway)' : '') + '<br>' +
                    '<hr style="border:none;border-top:1px solid #00d4ff33;margin:5px 0;">' +
                    '<small style="color:#888;">Mesh Info:</small><br>' +
                    '<b>Distance: ' + hopText + '</b><br>' +
                    'Msg ID: #' + node.meshMsgId + '<br>' +
                    'TTL: ' + node.meshTtl + ' hops<br>' +
                    'Last Sender: ' + (node.meshSenderId === 0 ? 'Self' : 'Node ' + node.meshSenderId) + '<br>' +
                    '<hr style="border:none;border-top:1px solid #00d4ff33;margin:5px 0;">' +
                    '<small style="color:#888;">Sensor Data:</small><br>' +
                    'Temp: ' + node.temp + '°F<br>' +
                    'Humidity: ' + node.humidity + '%<br>' +
                    'Pressure: ' + node.pressure + ' hPa<br>' +
                    'Satellites: ' + node.satellites + '<br>' +
                    (isGateway ? '' : 'RSSI: ' + node.rssi + ' dBm<br>') +
                    '</div>';
                
                // Update or create marker
                if (markers[id]) {
                    markers[id].setLatLng(coords);
                    markers[id].setIcon(icon);
                    markers[id].setPopupContent(popup);
                } else {
                    markers[id] = L.marker(coords, {icon: icon}).addTo(map).bindPopup(popup);
                }
                
                // Center map on first valid coordinate
                if (!mapInitialized) {
                    map.setView(coords, 17);
                    mapInitialized = true;
                }
            }
            
            // Second pass: draw connection lines from gateway to each node
            if (gatewayCoords) {
                for (const [id, node] of Object.entries(nodes)) {
                    if (node.lat === 0 && node.lng === 0) continue;
                    if (parseInt(id) === gateway.nodeId) continue; // skip gateway itself
                    
                    const nodeCoords = [node.lat, node.lng];
                    const lineId = 'gw-' + id;
                    
                    // Determine line style based on RSSI
                    let lineColor, lineWeight, dashArray;
                    const rssi = node.rssi || -100;
                    
                    if (rssi > -60) {
                        lineColor = '#44ff44'; // green - excellent
                        lineWeight = 4;
                        dashArray = null;
                    } else if (rssi > -80) {
                        lineColor = '#00d4ff'; // blue - good
                        lineWeight = 3;
                        dashArray = null;
                    } else if (rssi > -100) {
                        lineColor = '#ffaa00'; // orange - fair
                        lineWeight = 2;
                        dashArray = '10, 5';
                    } else {
                        lineColor = '#ff4444'; // red - poor
                        lineWeight = 2;
                        dashArray = '5, 10';
                    }
                    
                    // Line style
                    const lineStyle = {
                        color: lineColor,
                        weight: lineWeight,
                        opacity: node.online ? 0.8 : 0.3,
                        dashArray: dashArray
                    };
                    
                    // Update or create line
                    if (connectionLines[lineId]) {
                        connectionLines[lineId].setLatLngs([gatewayCoords, nodeCoords]);
                        connectionLines[lineId].setStyle(lineStyle);
                    } else {
                        connectionLines[lineId] = L.polyline([gatewayCoords, nodeCoords], lineStyle).addTo(map);
                        
                        // Add tooltip showing signal strength
                        connectionLines[lineId].bindTooltip('RSSI: ' + rssi + ' dBm', {
                            permanent: false,
                            direction: 'center'
                        });
                    }
                }
                
                // Remove lines for nodes that no longer exist
                for (const lineId of Object.keys(connectionLines)) {
                    const nodeId = lineId.replace('gw-', '');
                    if (!nodes[nodeId]) {
                        map.removeLayer(connectionLines[lineId]);
                        delete connectionLines[lineId];
                    }
                }
            }
        }
        
        // Update node cards
        function updateNodeCards(nodes) {
            const grid = document.getElementById('nodeGrid');
            let html = '';
            
            for (const [id, node] of Object.entries(nodes)) {
                const online = node.online;
                const lossRate = node.messageCount > 0 
                    ? ((node.packetsLost / (node.messageCount + node.packetsLost)) * 100).toFixed(1)
                    : '0.0';
                
                html += `
                    <div class="node-card ${online ? '' : 'offline'}">
                        <div class="node-header">
                            <div class="node-id">Node ${id}</div>
                            <div class="node-status ${online ? 'online' : 'offline'}">${online ? 'ONLINE' : 'OFFLINE'}</div>
                        </div>
                        <div class="node-data">
                            <div class="data-item">
                                <span class="data-label">Temperature</span>
                                <span class="data-value">${node.temp}°F</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Humidity</span>
                                <span class="data-value">${node.humidity}%</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Pressure</span>
                                <span class="data-value">${node.pressure} hPa</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Altitude</span>
                                <span class="data-value">${node.altitude} m</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Satellites</span>
                                <span class="data-value">${node.satellites}</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Messages</span>
                                <span class="data-value">${node.messageCount}</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Mesh Msg ID</span>
                                <span class="data-value">#${node.meshMsgId}</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Hop Distance</span>
                                <span class="data-value">${node.hopDistance === 0 ? 'Gateway' : node.hopDistance + ' hop' + (node.hopDistance > 1 ? 's' : '')}</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">TTL</span>
                                <span class="data-value">${node.meshTtl} hops</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Last Sender</span>
                                <span class="data-value">${node.meshSenderId === 0 ? 'Self' : 'Node ' + node.meshSenderId}</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Neighbors</span>
                                <span class="data-value">${node.neighborCount || 0} nodes</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Signal</span>
                                <span class="data-value">${getSignalBars(node.rssi)}</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">RSSI</span>
                                <span class="data-value">${node.rssi} dBm</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">SNR</span>
                                <span class="data-value">${node.snr} dB</span>
                            </div>
                            <div class="data-item">
                                <span class="data-label">Loss Rate</span>
                                <span class="data-value">${lossRate}%</span>
                            </div>
                        </div>
                    </div>
                `;
            }
            
            grid.innerHTML = html;
        }
        
        // Fetch and update data
        async function updateData() {
            try {
                const response = await fetch('/data');
                const data = await response.json();

                // Store nodes data globally for heatmap
                nodesData = data.nodes;

                // Update header stats (only uptime and nodes online remain)
                document.getElementById('uptime').textContent = formatUptime(data.gateway.uptime);

                // Count online nodes
                let online = 0;
                for (const node of Object.values(data.nodes)) {
                    if (node.online) online++;
                }
                document.getElementById('nodesOnline').textContent = online + '/' + Object.keys(data.nodes).length;

                // Update map and cards
                updateMap(data.nodes, data.gateway);
                updateNodeCards(data.nodes);

            } catch (e) {
                console.error('Update failed:', e);
            }
        }
        
        // Note: Initialization is done in the async IIFE above after online check completes

        // Manual time setting functions
        function setManualTime() {
            const hour = document.getElementById('setHour').value;
            const minute = document.getElementById('setMinute').value;
            const second = document.getElementById('setSecond').value;

            if (hour === '' || minute === '' || second === '') {
                document.getElementById('timeStatus').textContent = 'Please enter hour, minute, and second';
                document.getElementById('timeStatus').style.color = 'var(--danger)';
                return;
            }

            fetch('/settime?hour=' + hour + '&minute=' + minute + '&second=' + second)
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        document.getElementById('timeStatus').textContent = 'Time set: ' + data.time + ' UTC';
                        document.getElementById('timeStatus').style.color = 'var(--success)';
                    } else {
                        document.getElementById('timeStatus').textContent = data.error || 'Failed to set time';
                        document.getElementById('timeStatus').style.color = 'var(--danger)';
                    }
                })
                .catch(err => {
                    document.getElementById('timeStatus').textContent = 'Error: ' + err.message;
                    document.getElementById('timeStatus').style.color = 'var(--danger)';
                });
        }

        function setCurrentTime() {
            const now = new Date();
            document.getElementById('setHour').value = now.getUTCHours();
            document.getElementById('setMinute').value = now.getUTCMinutes();
            document.getElementById('setSecond').value = now.getUTCSeconds();
            setManualTime();
        }
    </script>
</body>
</html>
)rawliteral";

String generateHTML() {
    // Return HTML from PROGMEM (flash memory) instead of RAM
    // FPSTR() macro reads from flash and converts to String
    return FPSTR(DASHBOARD_HTML);
}