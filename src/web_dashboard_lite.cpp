/*
 * web_dashboard_lite.cpp
 * Lightweight Web Dashboard for Access Point Mode
 *
 * Optimized for ESP32 memory constraints:
 * - Minimal HTML (~5KB vs 40KB+)
 * - No external CDN dependencies (works 100% offline)
 * - Simple table-based layout instead of map
 * - Auto-refresh every 3 seconds
 */

#include "web_dashboard.h"
#include "config.h"
#include "node_store.h"
#include "mesh_stats.h"
#include "transmit_queue.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// Web server on port 80
static WebServer serverLite(80);
static DNSServer dnsServerLite;
static bool dashboardLiteRunning = false;
static unsigned long serverLiteStartTime = 0;

// Forward declarations
void handleRootLite();
void handleDataLite();
String generateHTMLLite();
String generateJSONLite();

bool initWebDashboardLite() {
    Serial.println(F("[WIFI-LITE] Starting lightweight AP dashboard..."));

    // Start Access Point with explicit configuration
    WiFi.mode(WIFI_AP);

    // Configure AP with specific settings to prevent disconnection
    WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),  // AP IP
        IPAddress(192, 168, 4, 1),  // Gateway
        IPAddress(255, 255, 255, 0) // Subnet
    );

    bool apStarted = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, 1, 0, 4);
    if (!apStarted) {
        Serial.println(F("[WIFI-LITE] Failed to start AP!"));
        return false;
    }

    delay(500);  // Longer delay for AP to stabilize

    // Start DNS server for captive portal
    dnsServerLite.start(53, "*", WiFi.softAPIP());
    Serial.println(F("[WIFI-LITE] DNS server started"));

    // Setup web server routes
    serverLite.on("/", handleRootLite);
    serverLite.on("/data", handleDataLite);
    serverLite.on("/test", []() {
        serverLite.send(200, "text/plain", "Lite Server OK! Free heap: " + String(ESP.getFreeHeap()));
    });

    // Captive portal detection endpoints
    // Android checks
    serverLite.on("/generate_204", handleRootLite);
    serverLite.on("/gen_204", handleRootLite);
    // iOS/macOS checks
    serverLite.on("/hotspot-detect.html", handleRootLite);
    serverLite.on("/library/test/success.html", handleRootLite);
    // Windows checks
    serverLite.on("/ncsi.txt", handleRootLite);
    serverLite.on("/connecttest.txt", handleRootLite);

    serverLite.onNotFound([]() {
        Serial.print(F("[HTTP-LITE] 404: "));
        Serial.println(serverLite.uri());
        // Redirect all 404s to root for captive portal
        serverLite.sendHeader("Location", "http://192.168.4.1/", true);
        serverLite.send(302, "text/plain", "Redirecting...");
    });

    // Start server
    serverLite.begin();

    Serial.println(F("[WIFI-LITE] ======================================"));
    Serial.println(F("[WIFI-LITE] Lightweight web server started!"));
    Serial.println(F("[WIFI-LITE] ======================================"));
    Serial.print(F("[WIFI-LITE]   Network: "));
    Serial.println(WIFI_AP_SSID);
    Serial.print(F("[WIFI-LITE]   Password: "));
    Serial.println(WIFI_AP_PASSWORD);
    Serial.print(F("[WIFI-LITE]   Dashboard: http://"));
    Serial.println(WiFi.softAPIP());
    Serial.println(F("[WIFI-LITE] ======================================"));
    Serial.print(F("[WIFI-LITE] Free heap: "));
    Serial.println(ESP.getFreeHeap());
    Serial.println(F("[WIFI-LITE] Waiting for connections..."));

    dashboardLiteRunning = true;
    serverLiteStartTime = millis();

    return true;
}

void handleWebDashboardLite() {
    if (dashboardLiteRunning) {
        dnsServerLite.processNextRequest();
        serverLite.handleClient();
    }
}

void handleRootLite() {
    Serial.println(F("[HTTP-LITE] GET / - Client connected!"));
    Serial.print(F("[HTTP-LITE] Free heap: "));
    Serial.println(ESP.getFreeHeap());

    String html = generateHTMLLite();
    Serial.print(F("[HTTP-LITE] Sending "));
    Serial.print(html.length());
    Serial.println(F(" bytes"));

    serverLite.send(200, "text/html", html);

    Serial.println(F("[HTTP-LITE] Page sent successfully"));
}

void handleDataLite() {
    String json = generateJSONLite();
    serverLite.send(200, "application/json", json);
}

String generateJSONLite() {
    String json = "{\"gateway\":{";
    json += "\"uptime\":" + String((millis() - serverLiteStartTime) / 1000) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap());
    json += "},\"nodes\":[";

    bool first = true;
    for (uint8_t i = 1; i <= MESH_MAX_NODES; i++) {
        NodeMessage* node = getNodeMessage(i);
        if (node == nullptr) continue;

        bool online = (i == DEVICE_ID) || (node->hasData && (millis() - node->lastHeardTime < 60000));

        if (!first) json += ",";
        first = false;

        json += "{\"id\":" + String(i) + ",";
        json += "\"online\":" + String(online ? "true" : "false") + ",";
        json += "\"temp\":" + String(node->lastReport.temperatureF_x10 / 10.0, 1) + ",";
        json += "\"hum\":" + String(node->lastReport.humidity_x10 / 10.0, 1) + ",";
        json += "\"lat\":" + String(node->lastReport.latitude_x1e6 / 1000000.0, 6) + ",";
        json += "\"lng\":" + String(node->lastReport.longitude_x1e6 / 1000000.0, 6) + ",";
        json += "\"rssi\":" + String(node->lastRssi, 0);
        json += "}";
    }

    json += "]}";
    return json;
}

String generateHTMLLite() {
    // Ultra-minimal HTML - no external dependencies
    String html = R"(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate">
<meta http-equiv="Pragma" content="no-cache">
<meta http-equiv="Expires" content="0">
<title>LoRa Mesh - Offline Mode</title>
<style>
body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f0f0f0}
h1{color:#333;margin:0 0 10px 0;font-size:24px}
.header{background:#4CAF50;color:white;padding:15px;border-radius:5px;margin-bottom:20px}
.stats{background:white;padding:10px;border-radius:5px;margin-bottom:20px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}
table{width:100%;border-collapse:collapse;background:white;border-radius:5px;overflow:hidden;box-shadow:0 2px 4px rgba(0,0,0,0.1)}
th{background:#4CAF50;color:white;padding:12px;text-align:left}
td{padding:10px;border-bottom:1px solid #ddd}
tr:hover{background:#f5f5f5}
.online{color:#4CAF50;font-weight:bold}
.offline{color:#999}
.info{background:#e3f2fd;padding:10px;border-radius:5px;margin-top:20px;font-size:14px}
</style>
</head>
<body>
<div class="header">
<h1>LoRa Mesh Network</h1>
<div>Gateway Node )" + String(DEVICE_ID) + R"( - Offline Mode</div>
</div>
<div class="stats">
<strong>Uptime:</strong> <span id="uptime">--</span> |
<strong>Free Heap:</strong> <span id="heap">--</span> bytes
</div>
<table>
<thead>
<tr>
<th>Node</th>
<th>Status</th>
<th>Temp (°F)</th>
<th>Humidity (%)</th>
<th>GPS</th>
<th>Signal</th>
</tr>
</thead>
<tbody id="nodes">
<tr><td colspan="6" style="text-align:center">Loading...</td></tr>
</tbody>
</table>
<div class="info">
📡 <strong>Access Point Mode:</strong> Working 100% offline - no internet required!<br>
🗺️ For map features, switch to Station Mode and connect to WiFi with internet.
</div>
<script>
function updateData(){
fetch('/data')
.then(r=>r.json())
.then(d=>{
document.getElementById('uptime').textContent=Math.floor(d.gateway.uptime/60)+'m '+d.gateway.uptime%60+'s';
document.getElementById('heap').textContent=d.gateway.heap;
let html='';
d.nodes.forEach(n=>{
html+='<tr>';
html+='<td><strong>Node '+n.id+'</strong></td>';
html+='<td class="'+(n.online?'online':'offline')+'">'+(n.online?'● ONLINE':'○ Offline')+'</td>';
html+='<td>'+n.temp.toFixed(1)+'</td>';
html+='<td>'+n.hum.toFixed(1)+'</td>';
html+='<td>'+n.lat.toFixed(6)+', '+n.lng.toFixed(6)+'</td>';
html+='<td>'+n.rssi+' dBm</td>';
html+='</tr>';
});
document.getElementById('nodes').innerHTML=html;
})
.catch(e=>console.error('Update failed:',e));
}
updateData();
setInterval(updateData,3000);
</script>
</body>
</html>)";

    return html;
}

bool isWebDashboardLiteRunning() {
    return dashboardLiteRunning;
}

String getGatewayIPLite() {
    if (!WIFI_USE_STATION_MODE) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}
