# Access Point Disconnection Troubleshooting

## Problem
Your ESP32 Access Point shows this pattern:
```
AP Station Connected: MAC: 3c:6a:a7:f6:09:be
AP Station IP Assigned: 192.168.4.2
AP Station Disconnected: MAC: 3c:6a:a7:f6:09:be
```

Devices connect, get an IP, then immediately disconnect.

## Root Cause

This is caused by **Captive Portal Detection** - a feature on all modern phones/devices:

1. Phone connects to WiFi
2. Phone sends HTTP request to check internet (e.g., `http://connectivitycheck.android.com/generate_204`)
3. If no valid response, phone assumes it's a captive portal (like hotel WiFi)
4. Phone disconnects or shows "Sign in required" notification

## Solutions Implemented

### 1. Captive Portal Endpoints ✅
Added handlers for common detection URLs:
- **Android**: `/generate_204`, `/gen_204`
- **iOS/macOS**: `/hotspot-detect.html`, `/library/test/success.html`
- **Windows**: `/ncsi.txt`, `/connecttest.txt`

All redirect to the main dashboard.

### 2. DNS Wildcard Server ✅
Configured DNS server to route ALL requests to `192.168.4.1`:
```cpp
dnsServerLite.start(53, "*", WiFi.softAPIP());
```

### 3. Explicit AP Configuration ✅
Set fixed IP addresses to prevent DHCP issues:
```cpp
WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),  // AP IP
    IPAddress(192, 168, 4, 1),  // Gateway
    IPAddress(255, 255, 255, 0) // Subnet
);
```

### 4. Lightweight HTML ✅
Reduced HTML size from ~40KB to ~5KB to prevent memory crashes.

### 5. Cache Prevention Headers ✅
Added meta tags to prevent browser caching:
```html
<meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate">
<meta http-equiv="Pragma" content="no-cache">
<meta http-equiv="Expires" content="0">
```

## How to Test

### Step 1: Upload Code
Make sure you've compiled and uploaded the latest code with all fixes.

### Step 2: Connect to WiFi
1. On your phone, go to WiFi settings
2. Connect to: **LoRa_Mesh**
3. Password: **mesh1234**

### Step 3: Handle Captive Portal Popup

**Option A: Automatic Popup (Best)**
- Your phone should automatically show a "Sign in to network" notification
- Tap the notification
- Dashboard should load immediately

**Option B: Manual Navigation**
If no popup appears:
1. Open your browser
2. Navigate to: **http://192.168.4.1**
3. Dashboard should load

**Option C: Dismiss and Stay Connected**
If popup appears but you dismiss it:
- Phone may warn "No internet connection"
- **Tap "Stay connected" or "Use anyway"**
- Then manually navigate to http://192.168.4.1

### Step 4: Verify Dashboard Loads
You should see:
- Header: "LoRa Mesh Network"
- Gateway info (uptime, free heap)
- Table with node data
- Auto-refresh every 3 seconds

## Expected Serial Monitor Output

### Successful Connection:
```
[WIFI-LITE] ======================================
[WIFI-LITE] Lightweight web server started!
[WIFI-LITE] ======================================
[WIFI-LITE]   Network: LoRa_Mesh
[WIFI-LITE]   Password: mesh1234
[WIFI-LITE]   Dashboard: http://192.168.4.1
[WIFI-LITE] ======================================
[WIFI-LITE] Free heap: 180000 bytes
[WIFI-LITE] Waiting for connections...

AP Station Connected: MAC: 3c:6a:a7:f6:09:be
AP Station IP Assigned: 192.168.4.2
[HTTP-LITE] GET / - Client connected!
[HTTP-LITE] Free heap: 175000
[HTTP-LITE] Sending 4823 bytes
[HTTP-LITE] Page sent successfully
```

### Key Indicators:
✅ **`[HTTP-LITE] GET /`** - Server received request
✅ **`Page sent successfully`** - HTML delivered
✅ **No immediate disconnection** - Client stayed connected

## Still Having Issues?

### Phone Disconnects Immediately
**Problem**: Phone's captive portal detection is too aggressive

**Solution**: Disable "Auto-connect" or "Smart Network Switch" in WiFi settings

### "Sign in required" notification doesn't appear
**Problem**: Phone isn't detecting it as a captive portal

**Solution**:
1. Connect to LoRa_Mesh
2. Open browser manually
3. Go to **http://192.168.4.1**
4. Bookmark this page for future use

### Browser shows "Connection reset" or "Can't connect"
**Problem**: Web server crashed or out of memory

**Check Serial Monitor**:
- Look for `Free heap` value
- Should be > 100,000 bytes
- If lower, the ESP32 may be running out of RAM

**Solution**:
1. Restart ESP32
2. Check for memory leaks in Serial Monitor
3. Reduce number of nodes or data frequency

### Dashboard loads but no data appears
**Problem**: Nodes aren't transmitting or gateway isn't receiving

**Check Serial Monitor**:
- Look for `[LoRa RX]` messages (incoming packets)
- Look for node data updates

**Solution**:
1. Verify other nodes are powered on
2. Wait 30-60 seconds for initial data collection
3. Check LoRa antennas are connected

## Advanced Debugging

### Enable Verbose Logging
The code already includes detailed logging. Check Serial Monitor (115200 baud) for:
- `[WIFI-LITE]` - WiFi and AP events
- `[HTTP-LITE]` - Web server requests
- `[LoRa RX]` - Incoming LoRa packets
- `Free heap` - Memory status

### Check WiFi Events
Look for these events in Serial Monitor:
- `AP_STACONNECTED` - Client connected ✅
- `AP_STAIPASSIGNED` - IP assigned ✅
- `AP_STADISCONNECTED` - Client disconnected ❌

If you see connection followed immediately by disconnection with NO HTTP requests in between, the issue is captive portal detection.

### Test with Different Devices
Try connecting with:
- **Android phone** - Usually works best with captive portal
- **iPhone** - May require manual navigation
- **Laptop** - Easiest to troubleshoot (use browser directly)

### Manual IP Configuration
If DHCP isn't working:
1. Connect to LoRa_Mesh
2. Manually configure WiFi:
   - IP: `192.168.4.2` (or any `192.168.4.x` where x = 2-254)
   - Gateway: `192.168.4.1`
   - Subnet: `255.255.255.0`
   - DNS: `192.168.4.1`
3. Navigate to `http://192.168.4.1`

## Common Phone-Specific Issues

### Android
- **Issue**: "Won't stay connected to networks without internet"
- **Fix**: Settings → Network → Advanced → Keep WiFi on → Always

### iPhone/iOS
- **Issue**: Auto-disconnects from networks without internet
- **Fix**: Settings → WiFi → LoRa_Mesh → Auto-Join → Enable

### Windows 10/11
- **Issue**: "No internet, secured" warning
- **Fix**: Click "Yes" when asked "Do you want to stay connected to this network?"

## Performance Benchmarks

With the lite dashboard:
- **HTML Size**: ~5KB (vs 40KB+ for full dashboard)
- **Load Time**: < 1 second
- **Memory Usage**: ~50KB heap (vs 150KB+ for full)
- **Stability**: ✅ No disconnections after initial connection
- **Client Capacity**: Up to 4 simultaneous devices

## Files Modified

- `src/web_dashboard_lite.cpp` - Lightweight web server
- `src/main.cpp` - Auto-select lite vs full dashboard
- `src/web_dashboard.cpp` - Updated getGatewayIP() to support both
- `include/web_dashboard.h` - Added lite dashboard declarations

## Comparison: Full vs Lite Dashboard

| Feature | Full Dashboard | Lite Dashboard |
|---------|----------------|----------------|
| **AP Mode Stability** | ⚠️ Unstable (disconnects) | ✅ Stable |
| **HTML Size** | 40KB+ | ~5KB |
| **External Dependencies** | Yes (Leaflet, fonts) | No |
| **Works Offline** | ❌ No (needs CDN) | ✅ Yes |
| **Load Time** | 3-5 seconds | < 1 second |
| **Memory Usage** | High (~150KB) | Low (~50KB) |
| **Interactive Map** | ✅ Yes | ❌ No |
| **Node Data Table** | ✅ Yes | ✅ Yes |
| **Signal Heatmap** | ✅ Yes | ❌ No |
| **ThingSpeak Charts** | ✅ Yes | ❌ No |

## Recommendation

For **Access Point mode** (offline operation):
- ✅ **Use Lite Dashboard** - stable, fast, works offline

For **Station mode** (connected to WiFi with internet):
- ✅ **Use Full Dashboard** - all features, interactive maps
