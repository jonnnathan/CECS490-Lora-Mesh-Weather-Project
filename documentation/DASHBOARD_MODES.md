# Dashboard Modes

Your ESP32 LoRa Mesh Network now has **two separate web dashboards** optimized for different scenarios:

## Lite Dashboard (Access Point Mode) 📱

**Used when:** `WIFI_USE_STATION_MODE = false`

### Features
- ✅ **Ultra-lightweight** (~5KB HTML vs 40KB+)
- ✅ **Works 100% offline** - no internet needed
- ✅ **No external dependencies** - all code embedded
- ✅ **Fast loading** - optimized for ESP32 memory
- ✅ **Captive portal** - automatic phone detection
- ✅ **Simple table view** - clean, responsive design
- ✅ **Auto-refresh** - updates every 3 seconds
- ✅ **All sensor data** - temperature, humidity, GPS, signal strength

### What's Included
- Node status (online/offline)
- Temperature (°F)
- Humidity (%)
- GPS coordinates
- RSSI signal strength
- Gateway uptime
- Free heap memory

### What's NOT Included
- Interactive map (requires internet for tiles)
- Signal heatmap (requires internet for Leaflet.js)
- ThingSpeak history charts (requires internet)

### Access
1. Connect to WiFi: **LoRa_Mesh**
2. Password: **mesh1234**
3. Navigate to: **http://192.168.4.1**

---

## Full Dashboard (Station Mode) 🌐

**Used when:** `WIFI_USE_STATION_MODE = true`

### Features
- ✅ **All Lite features** (node data, sensor readings)
- ✅ **Interactive map** with node markers at GPS locations
- ✅ **Signal strength heatmap** (RSSI, SNR, coverage)
- ✅ **ThingSpeak history** - charts and graphs
- ✅ **Connection lines** showing mesh topology
- ✅ **Advanced visualizations**

### Requirements
- ⚠️ **Internet connection required** (for map tiles and external resources)
- ⚠️ **More memory usage** - may not work well in AP mode
- ⚠️ **WiFi network** - home WiFi or phone hotspot with internet

### Access
1. Configure WiFi in [src/config.cpp](src/config.cpp)
2. Upload code to ESP32
3. Check Serial Monitor for IP address
4. Navigate to: **http://[IP_ADDRESS]**

---

## Switching Between Modes

### To Use Lite Dashboard (Offline)
Edit [src/config.cpp](src/config.cpp):
```cpp
const bool WIFI_USE_STATION_MODE = false;  // Use Access Point
```

### To Use Full Dashboard (Online)
Edit [src/config.cpp](src/config.cpp):
```cpp
const bool WIFI_USE_STATION_MODE = true;   // Join existing WiFi
const char* WIFI_STA_SSID = "YourWiFi";
const char* WIFI_STA_PASSWORD = "YourPassword";
```

---

## Technical Details

### Why Two Dashboards?

The ESP32 has limited RAM (~320KB total, ~180KB available for heap). The full dashboard with maps requires:
- **Leaflet.js** library (~140KB)
- **Map tiles** from OpenStreetMap
- **Google Fonts**
- **Complex HTML/CSS** (~40KB+)

This is too large to reliably serve in Access Point mode, causing:
- Out of memory crashes
- Failed HTTP responses
- Client disconnections

The **Lite Dashboard** solves this by:
- Embedding all code in a single HTML file (~5KB)
- Using simple CSS (no external frameworks)
- Table-based layout (no map libraries)
- Minimal JavaScript (just fetch API)

### File Structure

- **web_dashboard.cpp** - Full dashboard (Station mode)
- **web_dashboard_lite.cpp** - Lite dashboard (AP mode)
- **main.cpp** - Auto-selects based on `WIFI_USE_STATION_MODE`

---

## Troubleshooting

### Lite Dashboard Not Loading
1. Verify you're connected to "LoRa_Mesh" WiFi
2. Navigate to **http://192.168.4.1** (not HTTPS!)
3. Check Serial Monitor for "[HTTP-LITE] GET /" messages
4. Try the test endpoint: http://192.168.4.1/test

### Full Dashboard Not Loading
1. Verify ESP32 connected to WiFi (check Serial Monitor)
2. Verify your device is on the **same WiFi network**
3. Navigate to the correct IP address from Serial Monitor
4. Verify internet connection is working (for map tiles)

### Dashboard Loads But No Data
1. Verify you're the gateway node (`IS_GATEWAY = true`)
2. Check that `DEVICE_ID == GATEWAY_NODE_ID` in config.cpp
3. Verify other nodes are transmitting (check Serial Monitor)
4. Wait 30-60 seconds for initial data collection

---

## Performance Comparison

| Feature | Lite Dashboard | Full Dashboard |
|---------|----------------|----------------|
| **HTML Size** | ~5 KB | ~40 KB |
| **Memory Usage** | Low (~50KB heap) | High (~150KB heap) |
| **Load Time** | < 1 second | 3-5 seconds |
| **Internet Required** | ❌ No | ✅ Yes |
| **External Dependencies** | 0 | 5+ (maps, fonts, etc) |
| **ESP32 Stability** | ✅ Excellent | ⚠️ May crash in AP mode |
| **Map Features** | ❌ | ✅ |
| **Sensor Data** | ✅ | ✅ |

---

## Recommendations

### For Field Testing
Use **Lite Dashboard** (Access Point mode)
- Works anywhere, no internet needed
- Stable and reliable
- Fast loading

### For Demos/Presentations
Use **Full Dashboard** (Station mode with WiFi)
- Shows off map capabilities
- Visual impact with heatmaps
- Historical data from ThingSpeak

### For Development
Use **Full Dashboard** (Station mode with home WiFi)
- Most features available
- Reliable internet connection
- Easier debugging
