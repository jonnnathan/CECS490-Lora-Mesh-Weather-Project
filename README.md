# GPS-Timed Transmission System

## Overview

This system implements **Time Division Multiple Access (TDMA)** using GPS time synchronization for LoRa communication between multiple ESP32 MCUs. Each device gets assigned specific time slots during which it can transmit, preventing collisions and ensuring orderly communication.

## How It Works

### Time Slot Allocation

- **Device 1 (DEV1)**: Transmits on **EVEN minutes** (0, 2, 4, 6, ..., 58)
- **Device 2 (DEV2)**: Transmits on **ODD minutes** (1, 3, 5, 7, ..., 59)

### Transmission Schedule

Within each assigned minute, a device transmits **4 times** at the following seconds:
- **0 seconds** (HH:MM:00)
- **15 seconds** (HH:MM:15)
- **30 seconds** (HH:MM:30)
- **45 seconds** (HH:MM:45)

After completing all 4 transmissions, the device goes into **receive-only mode** for the remainder of the minute.

### Example Timeline

```
Time        Device 1 (EVEN min)    Device 2 (ODD min)
------------------------------------------------------
14:00:00    TX (1/4)               RX
14:00:15    TX (2/4)               RX
14:00:30    TX (3/4)               RX
14:00:45    TX (4/4)               RX
14:00:46-59 RX                     RX

14:01:00    RX                     TX (1/4)
14:01:15    RX                     TX (2/4)
14:01:30    RX                     TX (3/4)
14:01:45    RX                     TX (4/4)
14:01:46-59 RX                     RX

14:02:00    TX (1/4)               RX
...and so on
```

## Message Format

Messages now include GPS timestamps for precise timing verification:

```
<MESSAGE CONTENT> [DEVICE_ID:#TX_COUNT@YYYY-MM-DD HH:MM:SS]
```

### Example Messages

```
Hello from device transmitter! [DEV1:#1@2025-10-27 14:00:00]
Two-way LoRa communication active. [DEV2:#1@2025-10-27 14:01:15]
```

### Message Components

- **MESSAGE CONTENT**: The actual data being transmitted
- **DEVICE_ID**: Device identifier (DEV1, DEV2, etc.)
- **TX_COUNT**: Sequential transmission counter
- **GPS TIMESTAMP**: Exact GPS time when message was sent (YYYY-MM-DD HH:MM:SS)

## Configuration

### Device Setup

To configure a device, edit the following constants in `LoraBidirectional.cpp`:

```cpp
const uint8_t DEVICE_ID = 1;        // Set to 1 for Device 1, 2 for Device 2
const char* DEVICE_NAME = "DEV1";   // Set to "DEV1" or "DEV2"
```

### Customizing Transmission Times

To change when transmissions occur within a minute, modify the TDMA scheduler initialization in `setup()`:

```cpp
// Default: transmit at 0, 15, 30, 45 seconds
tdmaScheduler.setTransmissionSeconds(0, 15, 30, 45);

// Example alternative: transmit at 5, 20, 35, 50 seconds
tdmaScheduler.setTransmissionSeconds(5, 20, 35, 50);
```

### Custom Time Slot Allocation

For more than 2 devices, you can assign custom minutes:

```cpp
// Device 3 transmits at minutes: 0, 3, 6, 9, 12, ...
uint8_t device3Minutes[] = {0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57};
tdmaScheduler.setCustomMinutes(device3Minutes, 20);
```

## Hardware Requirements

### GPS Module

- **Model**: NEO-6M GPS Module
- **Connection**: Serial2 on ESP32
  - **TX Pin**: GPIO 46 (GPS TX → ESP32 RX)
  - **VCC**: 5V or 3.3V (depending on module)
  - **GND**: Ground

### LoRa Module

- **Model**: SX1262 (Heltec WiFi Kit 32 V3 built-in)
- **Frequency**: 915 MHz (adjust for your region)

### Display

- **Model**: OLED display (Heltec built-in)
- Shows GPS time, TX/RX mode, and transmission status

## Operation Modes

The system operates in the following modes:

### 1. WAIT_GPS
- **Description**: Waiting for GPS fix
- **Display**: Shows "GPS?" indicator
- **Behavior**: No transmission, listening only

### 2. RX_MODE
- **Description**: Receive-only mode (not in assigned time slot)
- **Display**: Shows "RX Mode"
- **Behavior**: Continuously listens for incoming messages

### 3. TX_MODE
- **Description**: Transmission mode (in assigned time slot)
- **Display**: Shows "TX@XXs" (next transmission second)
- **Behavior**: Transmits at scheduled seconds, listens in between

### 4. TX_DONE
- **Description**: Completed all 4 transmissions for this minute
- **Display**: Shows "TX Complete"
- **Behavior**: Waiting for next assigned minute

## Display Information

The OLED display shows:

### Normal Operation (DISPLAY_WAITING)
```
Line 1: HH:MM:SS MODE     (GPS time and current mode)
Line 2: TX:### RX:###     (Transmission and reception counts)
Line 3: TX@15s or RX Mode (Next action or current mode)
Line 4: Rate:XX%          (Transmission success rate)
```

### During Transmission (DISPLAY_SENDING)
```
Line 1: Sending...
Line 2: TX #XXX
Line 3: HH:MM:SS          (GPS time of transmission)
Line 4: Size:XXXb         (Message size in bytes)
```

### Message Received (DISPLAY_RECEIVED_MSG)
```
Line 1: Received!
Line 2: From: DEV#
Line 3: RX #XXX
Line 4: TX #XXX
```

## GPS Time Synchronization

### Why GPS Time?

GPS provides:
- **Global time reference**: All devices use the same time source
- **High accuracy**: GPS time is accurate to within microseconds
- **No drift**: Unlike system clocks (millis()), GPS time doesn't drift
- **Automatic sync**: No manual time setting required

### Time Considerations

- **UTC Offset**: Set in `neo6m.cpp` (default: -7 for Mountain Time)
- **Fix Time**: GPS may take 30-60 seconds to acquire initial fix
- **Indoor Operation**: GPS requires clear sky view; may not work indoors

## Troubleshooting

### No GPS Fix

**Symptoms**: Display shows "GPS?" or "Wait GPS"

**Solutions**:
- Move device outdoors or near a window
- Wait 30-60 seconds for initial GPS acquisition
- Check GPS module connections (TX pin to GPIO 46)
- Verify GPS module power (LED should blink when searching)

### No Transmissions

**Symptoms**: Device never transmits

**Solutions**:
- Check if GPS has a valid fix (time displayed on OLED)
- Verify DEVICE_ID matches expected time slots
- Ensure current GPS minute matches device's assigned minutes
- Check LoRa module initialization (Serial monitor)

### Messages Not Received

**Symptoms**: TX succeeds but other device doesn't receive

**Solutions**:
- Verify both devices have GPS fix and synchronized time
- Check LoRa frequency and settings match on both devices
- Ensure devices are within LoRa range (~1-5 km line of sight)
- Check RSSI/SNR values in Serial monitor for signal quality

### Time Slot Collisions

**Symptoms**: Both devices transmit at the same time

**Solutions**:
- Verify DEVICE_ID is set correctly (1 vs 2)
- Check that devices have GPS time synchronized (within 1 second)
- Ensure one device is configured for even minutes, the other for odd

## Serial Monitor Output

The Serial Monitor (115200 baud) provides detailed logging:

### Startup Messages
```
[STARTUP] Initializing two-way communication system...
[STARTUP] Device ID: 1 (DEV1)
[OLED] Initializing display... ✓ SUCCESS
[LORA] Initializing radio module... ✓ SUCCESS
[GPS] Initializing NEO-6M GPS module... ✓ SUCCESS
[TDMA] Initializing GPS-timed scheduler... ✓ SUCCESS
[TDMA] Device 1: Transmits on EVEN minutes (0, 2, 4, ..., 58)
```

### Transmission Events
```
[TDMA-TX] Time slot active - Transmission 1/4 at 14:00:00
┌─────────────────────────────────────────────────────────────┐
│ TRANSMITTING FROM DEV1 - MESSAGE #1                         │
│ GPS Time: 2025-10-27 14:00:00                               │
│ Content: "Hello from device transmitter!"                   │
│ Size: 78 bytes                                              │
└─────────────────────────────────────────────────────────────┘
[TX-SUCCESS] Message transmitted successfully
TX complete (1/4)
```

### Reception Events
```
[RX-ACTIVITY] ✓ MESSAGE RECEIVED
[RX-GPS] Message timestamp: 2025-10-27 14:01:15
┌─────────────────────────────────────────────────────────────┐
│ MESSAGE FROM DEV2 - RX #1                                   │
│ Remote TX #1 | Timestamp: 3675s                             │
│ Content: "Hello from device transmitter!"                   │
│ Raw: "Hello from device transmitter! [DEV2:#1@2025-10-27..."│
├─────────────────────────────────────────────────────────────┤
│ RSSI: -45.2 dBm | SNR: 12.5 dB | Quality: Excellent         │
└─────────────────────────────────────────────────────────────┘
```

## Performance Statistics

Every 10 transmissions, the system displays statistics:

```
╔═════════════════════════════════════════════════════════════╗
║                  TWO-WAY COMMUNICATION STATS               ║
╠═════════════════════════════════════════════════════════════╣
║ TX Attempts: 40 | Successful: 40                           ║
║ TX Success Rate: 100.0%                                    ║
║ RX Total: 38 | Valid: 38 | Corrupted: 0                    ║
║ RX Success Rate: 100.0%                                    ║
║ Duplicates: 0                                              ║
║ Runtime: 0h 20m 15s                                        ║
╚═════════════════════════════════════════════════════════════╝
```

## Advantages of GPS-Timed TDMA

1. **Collision Avoidance**: Devices never transmit at the same time
2. **Predictable Timing**: You know exactly when each device will transmit
3. **Power Efficiency**: Devices can sleep during non-assigned time slots
4. **Scalability**: Easy to add more devices with custom time slot assignments
5. **Synchronized Logs**: GPS timestamps allow correlation of events across devices
6. **No Master Required**: All devices self-coordinate using GPS time

## Future Enhancements

Possible improvements to the system:

- **Adaptive transmission**: Skip transmissions when no new data
- **Power saving**: Deep sleep during long RX-only periods
- **ACK/NACK protocol**: Confirm message reception
- **Message queuing**: Store and forward multiple messages
- **Dynamic time slots**: Adjust allocation based on traffic
- **Multi-hop routing**: Relay messages through intermediate devices

## Files Modified/Created

### New Files
- `include/tdma_scheduler.h` - TDMA scheduler header
- `src/tdma_scheduler.cpp` - TDMA scheduler implementation
- `GPS_TIMED_TRANSMISSION.md` - This documentation

### Modified Files
- `src/LoraBidirectional.cpp` - Main application with GPS-TDMA integration

### Existing Files (Used)
- `src/neo6m.cpp` - GPS module interface
- `include/neo6m.h` - GPS function declarations
- `src/lora_comm.cpp` - LoRa communication module
- `src/OLED.cpp` - Display driver
