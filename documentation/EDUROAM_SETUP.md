# eduroam / WPA2-Enterprise WiFi Setup Guide

## Overview

Your ESP32 LoRa mesh network now supports **WPA2-Enterprise** authentication, which is used by:
- **eduroam** (educational networks)
- **Corporate networks** with username/password authentication
- **Enterprise WiFi** networks requiring 802.1X authentication

## How to Configure

### Step 1: Edit `src/config.cpp`

Open [src/config.cpp](src/config.cpp) and configure the following settings:

```cpp
// Enable enterprise WiFi
const bool WIFI_USE_ENTERPRISE = true;  // Change to true for eduroam

// Enterprise WiFi credentials
const char* WIFI_ENTERPRISE_SSID = "eduroam";                        // Network name
const char* WIFI_ENTERPRISE_ANONYMOUS_IDENTITY = "anonymous@school.edu";  // Anonymous outer identity
const char* WIFI_ENTERPRISE_IDENTITY = "your.email@school.edu";      // Your school email (inner identity)
const char* WIFI_ENTERPRISE_USERNAME = "your.email@school.edu";      // Same as identity
const char* WIFI_ENTERPRISE_PASSWORD = "your_password";              // Your network password
```

### Step 2: Enter Your Credentials

Replace the placeholder values with your actual credentials:

#### For eduroam:
- **SSID**: Usually `"eduroam"` (same everywhere)
- **Anonymous Identity**: `"anonymous@school.edu"` (outer/public identity for RADIUS)
- **Identity**: Your school email (e.g., `"student@csulb.edu"`) - inner/secure identity
- **Username**: Same as identity (e.g., `"student@csulb.edu"`)
- **Password**: Your eduroam password (might be different from your regular school password)

#### Example for CSULB eduroam:
```cpp
const bool WIFI_USE_ENTERPRISE = true;
const char* WIFI_ENTERPRISE_SSID = "eduroam";
const char* WIFI_ENTERPRISE_ANONYMOUS_IDENTITY = "anonymous@csulb.edu";
const char* WIFI_ENTERPRISE_IDENTITY = "john.doe@csulb.edu";
const char* WIFI_ENTERPRISE_USERNAME = "john.doe@csulb.edu";
const char* WIFI_ENTERPRISE_PASSWORD = "MySecurePassword123";
```

⚠️ **IMPORTANT - CSULB IoT Device Restrictions**:
According to CSULB IT documentation, game consoles, smart TVs, and other IoT/streaming devices may not be able to connect to eduroam. If your ESP32 cannot connect despite correct credentials, consider using:
- **BeachGuests** network (if available and device registration is supported)
- **beachnet+** network (may require Campus ID instead of full email)
- Your home/personal WiFi network for testing

### Step 3: Keep Station Mode Enabled

Make sure this is set to `true`:
```cpp
const bool WIFI_USE_STATION_MODE = true;  // Must be true
```

### Step 4: Upload to ESP32

Compile and upload the code:
```bash
platformio run --target upload
```

## How It Works

When `WIFI_USE_ENTERPRISE` is enabled:

1. ESP32 sets up WPA2-Enterprise authentication
2. Sends your **identity** and **username** to the authentication server
3. Authenticates using **PEAP/MSCHAPv2** (most common method for eduroam)
4. Connects to the network once authenticated

## Troubleshooting

### Connection Fails
- **Check credentials**: Verify your username/password are correct
- **Check SSID**: Make sure it's exactly "eduroam" (case-sensitive)
- **Check identity format**: Some schools require `username@school.edu`, others just `username`
- **Serial Monitor**: Watch the output to see what's happening

### Serial Monitor Output
You should see:
```
[WIFI] Connecting to WPA2-Enterprise network...
[WIFI] SSID: eduroam
[WIFI] Identity: your.email@school.edu
..................
[WIFI] Connected to WiFi!
[WIFI] IP Address: 192.168.x.x
```

If it fails:
```
[WIFI] Failed to connect! Falling back to AP mode...
[WIFI] AP IP Address: 192.168.4.1
```

### Common Issues

1. **Wrong username format**
   - Try with domain: `user@school.edu`
   - Try without domain: `user`
   - Try with backslash: `DOMAIN\user`

2. **Password incorrect**
   - eduroam password might be different from regular login
   - Contact your school IT department

3. **Certificate errors**
   - Most schools don't require certificates for basic eduroam
   - If your school requires CA certificates, you'll need to add certificate validation code

4. **Network not available**
   - Make sure you're in range of eduroam access points
   - Try scanning networks first to verify eduroam is visible

## Switching Back to Regular WiFi

To go back to regular home/personal WiFi:

```cpp
const bool WIFI_USE_ENTERPRISE = false;  // Disable enterprise

// Standard WiFi credentials (these will be used instead)
const char* WIFI_STA_SSID = "YourHomeNetwork";
const char* WIFI_STA_PASSWORD = "YourHomePassword";
```

## Advanced Configuration

### Multiple Authentication Methods

eduroam typically supports these methods:
- **PEAP/MSCHAPv2** (most common, used by this code)
- **TTLS/PAP**
- **TLS** (requires client certificates)

This implementation uses **PEAP/MSCHAPv2**, which works with most eduroam networks.

### Adding Certificate Validation (Advanced)

If your school requires certificate validation:

```cpp
// Add this after setting username/password
const char* ca_pem = "-----BEGIN CERTIFICATE-----\n"
                     "YOUR CA CERTIFICATE HERE\n"
                     "-----END CERTIFICATE-----\n";

esp_wifi_sta_wpa2_ent_set_ca_cert((uint8_t *)ca_pem, strlen(ca_pem));
```

Contact your IT department for the CA certificate.

## Testing

1. **Upload code** with enterprise settings
2. **Open Serial Monitor** (115200 baud)
3. **Watch connection process**
4. **Access dashboard** at the IP address shown in Serial Monitor
5. **Verify internet connectivity** (if dashboard loads external resources)

## Security Notes

⚠️ **IMPORTANT**:
- Your credentials are stored in **plain text** in `config.cpp`
- Don't share your code with credentials in it
- Consider using environment variables for production deployments
- The ESP32 doesn't validate server certificates by default (vulnerable to MITM)

## Support

If you can't connect:
1. Check Serial Monitor output
2. Verify credentials with your IT department
3. Try connecting a laptop/phone to eduroam with same credentials
4. Contact your school's IT support for eduroam setup

## References

- [ESP32 WPA2 Enterprise Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wpa2-enterprise-method)
- [eduroam Official Site](https://www.eduroam.org/)
- Your school's IT department for specific eduroam configuration
