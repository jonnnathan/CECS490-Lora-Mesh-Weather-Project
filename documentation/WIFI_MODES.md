# WiFi Configuration Modes

Your ESP32 LoRa Mesh Network supports two WiFi modes:

## Mode 1: Access Point (Default - Works Offline) ✅

**Best for**: Field deployment, outdoor testing, no internet available

### Configuration
```cpp
const bool WIFI_USE_STATION_MODE = false;  // Access Point mode
```

### How it works
- ESP32 creates its own WiFi network: **LoRa_Mesh**
- Password: **mesh1234**
- IP Address: **192.168.4.1**
- **No internet required** - works completely offline

### Connecting
1. Turn on your ESP32 (Device 1 - Gateway)
2. On your phone/laptop, connect to WiFi: **LoRa_Mesh**
3. Password: **mesh1234**
4. Open browser: `http://192.168.4.1`

### Dashboard Features (Offline Mode)
✅ **Node Details** tab - All sensor data in clean tables
✅ **Real-time updates** - Data refreshes automatically
✅ **Signal strength** - RSSI/SNR for each node
✅ **GPS coordinates** - Lat/Lng displayed in tables
✅ **Temperature, humidity, pressure** - All environmental data
❌ **Map View** - Disabled (requires internet for map tiles)
❌ **Heatmap** - Disabled (requires internet)
❌ **ThingSpeak History** - Disabled (requires internet)

---

## Mode 2: Station Mode (Join WiFi - Full Features) 🌐

**Best for**: Indoor testing, demo presentations, when internet is available

### Configuration
```cpp
const bool WIFI_USE_STATION_MODE = true;   // Join existing WiFi
const bool WIFI_USE_ENTERPRISE = false;    // Use simple WiFi (not eduroam)

// Configure your WiFi credentials
const char* WIFI_STA_SSID = "YourWiFiName";     // Your WiFi network
const char* WIFI_STA_PASSWORD = "YourPassword"; // Your WiFi password
```

### How it works
- ESP32 joins your existing WiFi network (home WiFi or phone hotspot)
- Gets IP address from your router (check Serial Monitor for IP)
- **Requires internet** for map tiles and external resources

### Connecting
1. Configure WiFi credentials in [src/config.cpp](src/config.cpp)
2. Upload code to ESP32
3. Check Serial Monitor for IP address (e.g., `192.168.1.100`)
4. On device connected to **same WiFi**, open browser: `http://192.168.1.100`

### Dashboard Features (Online Mode)
✅ **Node Details** tab - All sensor data
✅ **Map View** - Interactive map with node markers at GPS locations
✅ **Heatmap** - Signal strength visualization
✅ **ThingSpeak History** - Historical charts and graphs
✅ **All real-time updates**

---

## Quick Comparison

| Feature | Access Point Mode | Station Mode |
|---------|------------------|--------------|
| **Internet Required** | ❌ No | ✅ Yes |
| **Works Outdoors** | ✅ Yes | ⚠️ If phone hotspot |
| **Setup Complexity** | ✅ Simple | ⚠️ Medium |
| **IP Address** | 192.168.4.1 (fixed) | Assigned by router |
| **Node Data Tables** | ✅ | ✅ |
| **Interactive Map** | ❌ | ✅ |
| **Signal Heatmap** | ❌ | ✅ |
| **ThingSpeak Charts** | ❌ | ✅ |

---

## Switching Between Modes

### To Access Point (Offline)
1. Edit [src/config.cpp](src/config.cpp)
2. Change: `const bool WIFI_USE_STATION_MODE = false;`
3. Upload to ESP32
4. Connect to: **LoRa_Mesh** WiFi
5. Browse to: `http://192.168.4.1`

### To Station Mode (Online with Phone Hotspot)
1. **Enable your phone's hotspot**
2. **Note the exact name** (case-sensitive!)
3. Edit [src/config.cpp](src/config.cpp):
   ```cpp
   const bool WIFI_USE_STATION_MODE = true;
   const char* WIFI_STA_SSID = "YourHotspotName";  // Exact name!
   const char* WIFI_STA_PASSWORD = "your_password";
   ```
4. Upload to ESP32
5. Check Serial Monitor for IP address
6. On phone (or device on same WiFi), browse to that IP

---

## Troubleshooting

### Access Point Mode Not Working
- **Issue**: Can't see LoRa_Mesh WiFi
  **Fix**: Check Serial Monitor - should show "AP Started" and IP 192.168.4.1

- **Issue**: Can connect but dashboard won't load
  **Fix**: Make sure you're browsing to `http://192.168.4.1` (not HTTPS)

### Station Mode Not Connecting
- **Issue**: WiFi connection fails
  **Fix**:
  1. Check SSID spelling (case-sensitive!)
  2. Ensure phone hotspot is 2.4GHz (ESP32 doesn't support 5GHz)
  3. Check password is correct
  4. Look at Serial Monitor for specific error messages

- **Issue**: Map not loading
  **Fix**:
  1. Verify phone has internet (not just hotspot on)
  2. Check browser console for errors
  3. Try refreshing page after ~5 seconds

---

## Recommended Usage

### For Field Testing
Use **Access Point mode** - no internet dependency, works anywhere

### For Demos/Presentations
Use **Station Mode with phone hotspot** - full features with map visualization

### For Development
Use **Station Mode with home WiFi** - fastest, most reliable

---

## GPS Geolocation

Both modes support GPS! Each node with a NEO-6M GPS module will:
- Transmit its real GPS coordinates via LoRa
- Display coordinates in the Node Details tab
- **In online mode**: Show up on the interactive map at actual location
- **In offline mode**: Display lat/lng in the data table

Nodes will appear at their true geographic positions once GPS lock is achieved.
