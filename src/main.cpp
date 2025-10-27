/*
 * ESP32 LoRa GPS-Timed TDMA Communication System
 * with Environmental Sensors and Hybrid Altimeter
 *
 * Required Components:
 * - GPS (NEO-6M) - Required for time synchronization
 * - LoRa (SX1262) - Required for communication
 *
 * Optional Components (will use dummy data if not detected):
 * - BMP180: Barometric pressure and temperature
 * - SHT30: Temperature and humidity
 *
 * Features:
 * - GPS-synchronized TDMA for collision-free LoRa communication
 * - Automatic dummy data when sensors not detected
 * - GPS-calibrated hybrid altimeter (when BMP180 available)
 * - Real-time sensor data transmission
 */

#include "lora_comm.h"      // LoRa communication
#include "tdma_scheduler.h" // TDMA scheduler
#include "neo6m.h"          // GPS functionality
#include "sht30.h"          // Temperature and humidity sensor
#include "OLED.h"           // Display
#include "bmp180.h"         // Barometric sensor
#include <Wire.h>
#include <math.h>

// ==================== DEVICE CONFIGURATION ====================
// SET THIS DIFFERENTLY ON EACH DEVICE
const uint8_t DEVICE_ID = 1;        // Device 1 or Device 2
const char* DEVICE_NAME = "DEV1";   // "DEV1" or "DEV2"

// ==================== TIMING CONFIGURATION ====================
const unsigned long RX_CHECK_INTERVAL = 100;    // Check for messages every 100ms
const unsigned long DISPLAY_TIME = 3000;        // Show status for 3 seconds
const unsigned long SENSOR_INTERVAL = 1000;     // Read sensors every 1 second
const unsigned long RECALIBRATION_INTERVAL = 300000; // GPS recalibration: 5 minutes

// ==================== DISPLAY STATES ====================
enum DisplayState {
  DISPLAY_WAITING,      // Normal operation
  DISPLAY_SENDING,      // During transmission
  DISPLAY_RECEIVED_MSG, // Message received
  DISPLAY_TX_FAILED,    // Transmission failed
  DISPLAY_SENSORS       // Show detailed sensor data
};

// ==================== GLOBAL OBJECTS ====================
OLED display;
TDMAScheduler tdmaScheduler;
BMP180 bmp180;
SHT30 sht30;

// Second I2C bus for sensors (Heltec V3)
TwoWire I2C_second = TwoWire(1);
#define SDA2_PIN 7
#define SCL2_PIN 20

// ==================== SENSOR VARIABLES ====================
// BMP180
bool  bmp180_ready = false;
float temperatureF = 0.0f;
float pressurePa = 0.0f;
float altitudeStd = 0.0f;
float altitudeCal = 0.0f;
bool  calibrated = false;
static const float SEA_LEVEL_DEFAULT_PA = 101325.0f;
float seaLevelPa = SEA_LEVEL_DEFAULT_PA;

// SHT30
bool sht30_ready = false;
float humidity = 0.0f;
float temperatureC_SHT = 0.0f;
float temperatureF_SHT = 0.0f;

// Hybrid Altimeter
bool autoCalibrated = false;
unsigned long lastGPSCalibration = 0;

// ==================== LORA VARIABLES ====================
unsigned long txSeq = 0;
unsigned long rxCount = 0;
unsigned long lastRxCheck = 0;
unsigned long lastSensorRead = 0;

// Statistics
unsigned long totalTxAttempts = 0;
unsigned long successfulTx = 0;
unsigned long totalRxMessages = 0;
unsigned long validRxMessages = 0;
unsigned long corruptedRxMessages = 0;
unsigned long duplicateRxMessages = 0;

// Message tracking
String lastReceivedMessage = "";
unsigned long lastRxTxCount = 0;
String lastSentMessage = "";

// Display state
DisplayState currentDisplay = DISPLAY_WAITING;
unsigned long displayStateStart = 0;
unsigned long lastDisplaySwitch = 0;
const unsigned long AUTO_DISPLAY_SWITCH = 5000; // Auto-switch display every 5s

// GPS time monitoring
unsigned long lastGPSStatusPrint = 0;
const unsigned long GPS_STATUS_INTERVAL = 2000; // Print GPS status every 2 seconds

// ==================== FUNCTION PROTOTYPES ====================
// Sensor functions
void initBMP180();
void initSHT30();
void calibrateSeaLevel(float knownAltM);
void readBMP180Data();
void readSHT30Data();
void updateHybridAltimeter();
float getHybridAltitude();
String getAltitudeSource();
static float seaLevelPressureFrom(float pressurePa, float knownAltitudeM);

// LoRa functions
bool transmitMessage();
void checkForIncomingMessages();
bool parseReceivedMessage(const String& message, String& sentence, unsigned long& txCount, String& fromDevice);
void handleReceivedMessage(const String& message, float rssi, float snr);
String buildSensorPayload();

// Display functions
void updateDisplay();
void displaySensorData();
void displayLoRaStatus();

// Utility functions
void printSystemInfo();
void printTransceiverStats();
void printSeparator();
void printGPSStatus();

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(200);

  printSeparator();
  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.print("║     ESP32 LoRa GPS-TDMA + Sensor System - ");
  Serial.print(DEVICE_NAME);
  Serial.println("           ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  Serial.println();

  Serial.println("[STARTUP] Initializing integrated system...");
  Serial.print("[STARTUP] Device ID: ");
  Serial.print(DEVICE_ID);
  Serial.print(" (");
  Serial.print(DEVICE_NAME);
  Serial.println(")");
  Serial.println("[INFO] Required: GPS + LoRa | Optional: BMP180, SHT30");
  Serial.println("[INFO] System will use dummy data if sensors not detected");
  Serial.println();

  // Initialize OLED
  Serial.print("[OLED] Initializing display... ");
  if (display.init()) {
    Serial.println("✓ SUCCESS");
    display.clearDisplay();
    display.drawString(0, 0, "LoRa " + String(DEVICE_NAME));
    display.drawString(0, 10, "Initializing...");
    display.updateDisplay();
  } else {
    Serial.println("✗ FAILED");
  }

  // Initialize I2C for sensors
  Serial.print("[I2C] Initializing sensor bus... ");
  I2C_second.begin(SDA2_PIN, SCL2_PIN, 100000);
  Serial.println("✓ SUCCESS");

  // Initialize BMP180 (optional)
  initBMP180();
  if (!bmp180_ready) {
    Serial.println("[WARNING] BMP180 not detected - will use dummy temperature/pressure/altitude");
  }

  // Initialize SHT30 (optional)
  initSHT30();
  if (!sht30_ready) {
    Serial.println("[WARNING] SHT30 not detected - will use dummy humidity data");
  }

  // Initialize GPS on Serial2 (RX pin 46)
  // GPS TX connects to ESP32 RX (GPIO 46)
  // GPS RX not used (we don't send commands to GPS)
  Serial.print("[GPS] Initializing NEO-6M GPS module... ");
  Serial2.begin(9600, SERIAL_8N1, 46, -1); // RX=46 (GPS TX->ESP32 RX), TX=-1 (not used)
  initGPS();
  Serial.println("✓ SUCCESS");
  Serial.println("[GPS] Waiting for GPS fix...");

  // Initialize LoRa
  Serial.print("[LORA] Initializing radio module... ");
  if (initLoRa()) {
    Serial.println("✓ SUCCESS");
    Serial.println("[LORA] Radio ready for GPS-timed communication");
  } else {
    Serial.println("✗ FAILED");
    Serial.println("[ERROR] LoRa initialization failed");
  }

  // Initialize TDMA Scheduler
  Serial.print("[TDMA] Initializing GPS-timed scheduler... ");
  tdmaScheduler.init(DEVICE_ID, 4); // 4 transmissions per time slot
  tdmaScheduler.setTransmissionSeconds(0, 15, 30, 45); // Transmit at 0, 15, 30, 45 seconds
  Serial.println("✓ SUCCESS");

  if (DEVICE_ID == 1) {
    Serial.println("[TDMA] Device 1: Transmits on EVEN minutes (0, 2, 4, ..., 58)");
  } else if (DEVICE_ID == 2) {
    Serial.println("[TDMA] Device 2: Transmits on ODD minutes (1, 3, 5, ..., 59)");
  }

  // Set initial timing
  lastRxCheck = millis();
  lastSensorRead = millis();
  lastDisplaySwitch = millis();
  lastGPSStatusPrint = millis();

  // Print component status summary
  Serial.println();
  Serial.println("[SUMMARY] Active Components:");
  Serial.print("  - GPS: INITIALIZED | LoRa: ");
  Serial.println(isLoRaReady() ? "READY" : "FAILED");
  Serial.print("  - BMP180: ");
  Serial.print(bmp180_ready ? "ACTIVE" : "DUMMY DATA");
  Serial.print(" | SHT30: ");
  Serial.println(sht30_ready ? "ACTIVE" : "DUMMY DATA");
  Serial.println("[READY] System operational - GPS + LoRa are sufficient for transmission");

  printSystemInfo();
  printSeparator();
  Serial.println("[MAIN LOOP] Starting GPS-timed communication...");
  Serial.println("[MODE] GPS-synchronized TDMA + Real-time data transmission");
  Serial.println();
}

// ==================== MAIN LOOP ====================
void loop() {
  unsigned long now = millis();

  // Process GPS data continuously (processGPSData handles checking Serial2.available internally)
  processGPSData();

  // Update TDMA scheduler with current GPS time
  tdmaScheduler.update(g_hour, g_minute, g_second, g_datetime_valid);

  // Read sensor data periodically
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    readBMP180Data();
    readSHT30Data();
    updateHybridAltimeter();
    lastSensorRead = now;
  }

  // Print GPS status periodically
  if (now - lastGPSStatusPrint >= GPS_STATUS_INTERVAL) {
    printGPSStatus();
    lastGPSStatusPrint = now;
  }

  // Check for incoming messages frequently (always listen)
  if (now - lastRxCheck >= RX_CHECK_INTERVAL) {
    checkForIncomingMessages();
    lastRxCheck = now;
  }

  // GPS-synchronized transmission: Only transmit during assigned time slots
  if (tdmaScheduler.shouldTransmitNow()) {
    TDMAStatus status = tdmaScheduler.getStatus();

    Serial.print("[TDMA-TX] Time slot active - Transmission ");
    Serial.print(status.currentTransmissionIndex + 1);
    Serial.print("/4 at ");
    Serial.print(g_hour);
    Serial.print(":");
    if (g_minute < 10) Serial.print("0");
    Serial.print(g_minute);
    Serial.print(":");
    if (g_second < 10) Serial.print("0");
    Serial.println(g_second);

    bool success = transmitMessage();
    totalTxAttempts++;

    if (success) {
      successfulTx++;
      currentDisplay = DISPLAY_SENDING;
      Serial.println("[TX-SUCCESS] Message transmitted successfully");
      tdmaScheduler.markTransmissionComplete();
    } else {
      currentDisplay = DISPLAY_TX_FAILED;
      Serial.println("[TX-FAILED] Transmission failed");
    }

    displayStateStart = now;
    updateDisplay();

    // Show stats every 10 transmissions
    if (totalTxAttempts % 10 == 0) {
      printTransceiverStats();
    }

    Serial.println();
  }

  // Auto-switch display between sensor and LoRa status
  if (currentDisplay == DISPLAY_WAITING && now - lastDisplaySwitch >= AUTO_DISPLAY_SWITCH) {
    currentDisplay = DISPLAY_SENSORS;
    lastDisplaySwitch = now;
  } else if (currentDisplay == DISPLAY_SENSORS && now - lastDisplaySwitch >= AUTO_DISPLAY_SWITCH) {
    currentDisplay = DISPLAY_WAITING;
    lastDisplaySwitch = now;
  }

  // Update display based on state and timing
  if (currentDisplay != DISPLAY_WAITING && currentDisplay != DISPLAY_SENSORS) {
    if (now - displayStateStart >= DISPLAY_TIME) {
      currentDisplay = DISPLAY_WAITING;
      updateDisplay();
    }
  }

  updateDisplay();
  delay(10); // Small delay to prevent overwhelming the loop
}

// ==================== SENSOR FUNCTIONS ====================

void initBMP180() {
  Serial.print("[BMP180] Initializing... ");
  if (bmp180.begin(&I2C_second, BMP180_OSS_ULTRAHIGHRES)) {
    bmp180_ready = true;
    Serial.println("✓ SUCCESS");
  } else {
    bmp180_ready = false;
    Serial.println("✗ FAILED");
  }
}

void initSHT30() {
  Serial.print("[SHT30] Initializing... ");
  if (sht30.begin(&I2C_second)) {
    sht30_ready = true;
    Serial.println("✓ SUCCESS");
  } else {
    sht30_ready = false;
    Serial.println("✗ FAILED");
  }
}

void readSHT30Data() {
  if (!sht30_ready) return;

  if (sht30.read()) {
    temperatureC_SHT = sht30.getTemperature();
    temperatureF_SHT = (temperatureC_SHT * 9.0f / 5.0f) + 32.0f;
    humidity = sht30.getHumidity();
  }
}

void calibrateSeaLevel(float knownAltM) {
  const int N = 8;
  float sumPa = 0.0f;
  for (int i = 0; i < N; ++i) {
    float p = bmp180.readPressure();
    if (p > 10000.0f && p < 120000.0f) {
      sumPa += p;
    } else {
      --i;
    }
    delay(60);
  }
  float pNow = sumPa / N;
  seaLevelPa = seaLevelPressureFrom(pNow, knownAltM);
  calibrated = true;
  Serial.println("[BMP180] Calibrated to altitude: " + String(knownAltM) + "m, SLP: " + String(seaLevelPa/100.0f) + "hPa");
}

static float seaLevelPressureFrom(float pressurePa, float knownAltitudeM) {
  return pressurePa / powf(1.0f - (knownAltitudeM / 44330.0f), 5.255f);
}

void readBMP180Data() {
  if (!bmp180_ready) return;

  float tempC = bmp180.readTemperature();
  temperatureF = (tempC * 9.0f / 5.0f) + 32.0f;
  pressurePa = bmp180.readPressure();
  altitudeStd = bmp180.readAltitude(SEA_LEVEL_DEFAULT_PA);

  if (calibrated) {
    altitudeCal = bmp180.readAltitude(seaLevelPa);
  } else {
    altitudeCal = altitudeStd;
  }
}

void updateHybridAltimeter() {
  if (isLocationValid() && isAltitudeValid() && bmp180_ready &&
      (millis() - lastGPSCalibration > RECALIBRATION_INTERVAL)) {

    float gpsAlt = getGPSAltitude();

    if (gpsAlt > -500.0f && gpsAlt < 9000.0f) {
      calibrateSeaLevel(gpsAlt);
      autoCalibrated = true;
      lastGPSCalibration = millis();
      Serial.println("[AUTO-CAL] BMP180 calibrated with GPS altitude: " + String(gpsAlt, 1) + "m");
    }
  }
}

float getHybridAltitude() {
  if (autoCalibrated && bmp180_ready) {
    return bmp180.readAltitude(seaLevelPa);
  } else if (isAltitudeValid()) {
    return getGPSAltitude();
  }
  return -999.0f;
}

String getAltitudeSource() {
  if (autoCalibrated && bmp180_ready) return "BAR";
  else if (isAltitudeValid()) return "GPS";
  return "---";
}

// ==================== LORA FUNCTIONS ====================

bool transmitMessage() {
  if (!isLoRaReady()) {
    Serial.println("[TX-ERROR] LoRa module not ready");
    return false;
  }

  txSeq++;
  String payload = buildSensorPayload();

  lastSentMessage = payload;

  Serial.println("┌─────────────────────────────────────────────────────────────┐");
  Serial.print("│ TRANSMITTING FROM ");
  Serial.print(DEVICE_NAME);
  Serial.print(" - MESSAGE #");
  Serial.print(txSeq);

  int padding = 29 - String(txSeq).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");

  String gpsTimestamp = tdmaScheduler.getGPSTimestampString();
  Serial.print("│ GPS Time: ");
  Serial.print(gpsTimestamp);
  padding = 51 - gpsTimestamp.length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");

  Serial.print("│ Payload: ");
  Serial.print(payload.substring(0, 50));
  if (payload.length() > 50) Serial.print("...");
  padding = 51 - min((int)payload.length(), 50);
  if (payload.length() > 50) padding -= 3;
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");

  Serial.print("│ Size: ");
  Serial.print(payload.length());
  Serial.print(" bytes");
  padding = 52 - String(payload.length()).length() - 6;
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");

  Serial.println("└─────────────────────────────────────────────────────────────┘");

  return sendMessage(payload);
}

String buildSensorPayload() {
  // Get GPS timestamp
  String gpsTimestamp = tdmaScheduler.getGPSTimestampString();

  // Build sensor data string with real or dummy data
  String sensorData = "";

  // Temperature - use real if available, otherwise dummy
  if (bmp180_ready) {
    sensorData += "T:" + String(temperatureF, 1) + "F";
  } else {
    sensorData += "T:--.-F";  // Dummy temperature
  }

  // Humidity - use real if available, otherwise dummy
  if (sht30_ready) {
    sensorData += ",H:" + String(humidity, 1) + "%";
  } else {
    sensorData += ",H:--.-%";  // Dummy humidity
  }

  // Pressure - use real if available, otherwise dummy
  if (bmp180_ready) {
    sensorData += ",P:" + String(pressurePa/100.0f, 1) + "hPa";
  } else {
    sensorData += ",P:---.-hPa";  // Dummy pressure
  }

  // Altitude - use hybrid if available, otherwise dummy
  float altitude = getHybridAltitude();
  if (altitude != -999.0f) {
    sensorData += ",A:" + String(altitude, 1) + "m(" + getAltitudeSource() + ")";
  } else {
    sensorData += ",A:----.-m(---)";  // Dummy altitude
  }

  // GPS coordinates - use real if available, otherwise dummy
  if (isLocationValid()) {
    sensorData += ",GPS:" + String(getLatitude(), 4) + "," + String(getLongitude(), 4);
  } else {
    sensorData += ",GPS:---.----,---.----";  // Dummy GPS coordinates
  }

  // Create complete message: "SENSOR_DATA [DEV1:#123@TIMESTAMP]"
  String message = sensorData + " [" + String(DEVICE_NAME) + ":#" + String(txSeq) + "@" + gpsTimestamp + "]";

  return message;
}

void checkForIncomingMessages() {
  String msg = receiveMessage();

  if (msg.length() > 0) {
    float rssi = getLastRSSI();
    float snr = getLastSNR();

    Serial.println("[RX-ACTIVITY] ✓ MESSAGE RECEIVED");
    handleReceivedMessage(msg, rssi, snr);

    currentDisplay = DISPLAY_RECEIVED_MSG;
    displayStateStart = millis();
    updateDisplay();
  }
}

void handleReceivedMessage(const String& message, float rssi, float snr) {
  totalRxMessages++;

  String sentence = "";
  unsigned long txCount = 0;
  String fromDevice = "";
  bool parseSuccess = parseReceivedMessage(message, sentence, txCount, fromDevice);

  if (parseSuccess) {
    validRxMessages++;
    rxCount++;

    if (message == lastReceivedMessage) {
      duplicateRxMessages++;
      Serial.println("[RX-WARNING] Duplicate message detected");
    }

    lastReceivedMessage = message;
    lastRxTxCount = txCount;

    Serial.println();
    Serial.println("┌─────────────────────────────────────────────────────────────┐");
    Serial.print("│ MESSAGE FROM ");
    Serial.print(fromDevice);
    Serial.print(" - RX #");
    Serial.print(rxCount);

    int padding = 39 - fromDevice.length() - String(rxCount).length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");

    Serial.print("│ Remote TX #");
    Serial.print(txCount);
    padding = 48 - String(txCount).length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");

    Serial.print("│ Data: ");
    Serial.print(sentence.substring(0, 54));
    if (sentence.length() > 54) Serial.print("...");
    padding = 54 - min((int)sentence.length(), 54);
    if (sentence.length() > 54) padding -= 3;
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");

    Serial.println("├─────────────────────────────────────────────────────────────┤");

    Serial.print("│ RSSI: ");
    Serial.print(rssi, 1);
    Serial.print(" dBm | SNR: ");
    Serial.print(snr, 1);
    Serial.print(" dB | Quality: ");

    String quality;
    if (rssi > -70 && snr > 10) quality = "Excellent";
    else if (rssi > -85 && snr > 5) quality = "Good";
    else if (rssi > -100 && snr > 0) quality = "Fair";
    else quality = "Poor";

    Serial.print(quality);

    String signalInfo = String(rssi, 1) + " dBm | SNR: " + String(snr, 1) + " dB | Quality: " + quality;
    padding = 56 - signalInfo.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");

    Serial.println("└─────────────────────────────────────────────────────────────┘");
    Serial.println();

  } else {
    corruptedRxMessages++;
    Serial.println("[RX-ERROR] Failed to parse received message");
    Serial.print("[RX-ERROR] Raw content: \"");
    Serial.print(message);
    Serial.println("\"");
  }
}

bool parseReceivedMessage(const String& message, String& sentence, unsigned long& txCount, String& fromDevice) {
  int bracketStart = message.lastIndexOf(" [");
  if (bracketStart == -1) return false;

  int bracketEnd = message.indexOf(']', bracketStart);
  if (bracketEnd == -1) return false;

  sentence = message.substring(0, bracketStart);
  sentence.trim();

  String bracketContent = message.substring(bracketStart + 2, bracketEnd);

  int colonPos = bracketContent.indexOf(":#");
  if (colonPos == -1) return false;

  fromDevice = bracketContent.substring(0, colonPos);

  String afterColon = bracketContent.substring(colonPos + 2);
  int atPos = afterColon.indexOf('@');

  String countStr;
  String gpsTimestamp = "";

  if (atPos != -1) {
    countStr = afterColon.substring(0, atPos);
    gpsTimestamp = afterColon.substring(atPos + 1);
  } else {
    countStr = afterColon;
  }

  txCount = countStr.toInt();

  if (sentence.length() == 0 || fromDevice.length() == 0 || (txCount == 0 && countStr != "0")) {
    return false;
  }

  if (fromDevice == DEVICE_NAME) {
    Serial.println("[RX-FILTER] Ignoring our own transmission");
    return false;
  }

  if (gpsTimestamp.length() > 0) {
    Serial.print("[RX-GPS] Message timestamp: ");
    Serial.println(gpsTimestamp);
  }

  return true;
}

// ==================== DISPLAY FUNCTIONS ====================

void updateDisplay() {
  display.clearDisplay();

  switch (currentDisplay) {
    case DISPLAY_WAITING:
      displayLoRaStatus();
      break;

    case DISPLAY_SENSORS:
      displaySensorData();
      break;

    case DISPLAY_SENDING: {
      display.drawString(0, 0, "Sending...");
      display.drawString(0, 10, "TX #" + String(txSeq));
      if (g_datetime_valid) {
        char timeStr[12];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", g_hour, g_minute, g_second);
        display.drawString(0, 20, String(timeStr));
      }
      display.drawString(0, 30, String(lastSentMessage.length()) + " bytes");
      break;
    }

    case DISPLAY_RECEIVED_MSG:
      display.drawString(0, 0, "Received!");
      display.drawString(0, 10, "From: " + String(lastReceivedMessage.indexOf("DEV1") > 0 ? "DEV1" : "DEV2"));
      display.drawString(0, 20, "RX #" + String(rxCount));
      display.drawString(0, 30, "TX #" + String(lastRxTxCount));
      break;

    case DISPLAY_TX_FAILED:
      display.drawString(0, 0, "TX Failed!");
      display.drawString(0, 10, "Check radio");
      display.drawString(0, 20, "TX #" + String(txSeq));
      display.drawString(0, 30, "Retrying...");
      break;
  }

  display.updateDisplay();
}

void displayLoRaStatus() {
  // Show GPS time and mode
  if (g_datetime_valid) {
    char timeStr[20];
    String mode = tdmaScheduler.getDeviceMode();
    if (mode.length() > 8) mode = mode.substring(0, 8);
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d %s", g_hour, g_minute, g_second, mode.c_str());
    display.drawString(0, 0, String(timeStr));
  } else {
    display.drawString(0, 0, String(DEVICE_NAME) + " GPS?");
  }

  // Show TX/RX counts
  display.drawString(0, 10, "TX:" + String(txSeq) + " RX:" + String(rxCount));

  // Show TDMA mode
  String mode = tdmaScheduler.getDeviceMode();
  if (mode == "TX_MODE") {
    TDMAStatus status = tdmaScheduler.getStatus();
    int nextSec = status.nextTransmissionSecond;
    display.drawString(0, 20, "TX@" + String(nextSec) + "s");
  } else if (mode == "RX_MODE") {
    display.drawString(0, 20, "RX Mode");
  } else if (mode == "TX_DONE") {
    display.drawString(0, 20, "TX Complete");
  } else {
    display.drawString(0, 20, "Wait GPS");
  }

  // Show success rate
  if (totalTxAttempts > 0) {
    float txSuccessRate = (float)successfulTx / totalTxAttempts * 100.0;
    display.drawString(0, 30, "Rate:" + String(txSuccessRate, 0) + "%");
  }
}

void displaySensorData() {
  // Temperature
  if (bmp180_ready) {
    display.drawString(0, 0, "T:" + String(temperatureF, 1) + "F");
  } else {
    display.drawString(0, 0, "Temp: ---");
  }

  // Humidity
  if (sht30_ready) {
    display.drawString(0, 10, "H:" + String(humidity, 1) + "%");
  } else {
    display.drawString(0, 10, "Hum: ---");
  }

  // Pressure
  if (bmp180_ready) {
    display.drawString(0, 20, "P:" + String(pressurePa/100.0f, 1) + "hPa");
  } else {
    display.drawString(0, 20, "Press: ---");
  }

  // Altitude
  float altitude = getHybridAltitude();
  String altSource = getAltitudeSource();
  if (altitude != -999.0f) {
    display.drawString(0, 30, "A:" + String(altitude, 0) + "m " + altSource);
  } else {
    display.drawString(0, 30, "Alt: ---");
  }
}

// ==================== UTILITY FUNCTIONS ====================

void printSystemInfo() {
  Serial.println();
  Serial.println("┌─────────────────────────────────────────────────────────────┐");
  Serial.println("│              GPS-TIMED SENSOR SYSTEM INFO                  │");
  Serial.println("├─────────────────────────────────────────────────────────────┤");
  Serial.print("│ Device: ");
  Serial.print(DEVICE_NAME);
  Serial.print(" (ID: ");
  Serial.print(DEVICE_ID);
  Serial.print(")");

  String deviceInfo = String(DEVICE_NAME) + " (ID: " + String(DEVICE_ID) + ")";
  int padding = 48 - deviceInfo.length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");

  Serial.print("│ RX Check: ");
  Serial.print(RX_CHECK_INTERVAL);
  Serial.println(" ms                                     │");
  Serial.println("│ Mode: GPS-Synchronized TDMA with Sensors               │");
  Serial.println("│ TX Schedule: 4 transmissions per assigned minute       │");
  Serial.println("│ TX Seconds: 0, 15, 30, 45                              │");

  if (DEVICE_ID == 1) {
    Serial.println("│ Time Slots: EVEN minutes (0, 2, 4, ..., 58)            │");
  } else if (DEVICE_ID == 2) {
    Serial.println("│ Time Slots: ODD minutes (1, 3, 5, ..., 59)             │");
  }

  Serial.print("│ Sensors: BMP180=");
  Serial.print(bmp180_ready ? "OK" : "FAIL");
  Serial.print(", SHT30=");
  Serial.print(sht30_ready ? "OK" : "FAIL");
  Serial.print(", GPS=INIT");
  padding = 25;
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");

  Serial.println("└─────────────────────────────────────────────────────────────┘");
  Serial.println();
}

void printTransceiverStats() {
  Serial.println();
  Serial.println("╔═════════════════════════════════════════════════════════════╗");
  Serial.println("║              GPS-TIMED COMMUNICATION STATS                 ║");
  Serial.println("╠═════════════════════════════════════════════════════════════╣");

  // TX Stats
  Serial.print("║ TX Attempts: ");
  Serial.print(totalTxAttempts);
  Serial.print(" | Successful: ");
  Serial.print(successfulTx);

  String txStats = String(totalTxAttempts) + " | Successful: " + String(successfulTx);
  int padding = 38 - txStats.length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");

  if (totalTxAttempts > 0) {
    float txRate = (float)successfulTx / totalTxAttempts * 100.0;
    Serial.print("║ TX Success Rate: ");
    Serial.print(txRate, 1);
    Serial.print("%");

    String txRateStr = String(txRate, 1) + "%";
    padding = 40 - txRateStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("║");
  }

  // RX Stats
  Serial.print("║ RX Total: ");
  Serial.print(totalRxMessages);
  Serial.print(" | Valid: ");
  Serial.print(validRxMessages);
  Serial.print(" | Corrupted: ");
  Serial.print(corruptedRxMessages);

  String rxStats = String(totalRxMessages) + " | Valid: " + String(validRxMessages) + " | Corrupted: " + String(corruptedRxMessages);
  padding = 59 - rxStats.length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");

  if (totalRxMessages > 0) {
    float rxRate = (float)validRxMessages / totalRxMessages * 100.0;
    Serial.print("║ RX Success Rate: ");
    Serial.print(rxRate, 1);
    Serial.print("%");

    String rxRateStr = String(rxRate, 1) + "%";
    padding = 40 - rxRateStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("║");
  }

  Serial.print("║ Duplicates: ");
  Serial.print(duplicateRxMessages);

  padding = 46 - String(duplicateRxMessages).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");

  // Sensor status
  Serial.print("║ BMP180: ");
  Serial.print(bmp180_ready ? "OK" : "FAIL");
  Serial.print(" | SHT30: ");
  Serial.print(sht30_ready ? "OK" : "FAIL");
  Serial.print(" | GPS: ");
  Serial.print(g_datetime_valid ? "SYNCED" : "SEARCHING");

  String sensorStatus = (bmp180_ready ? "OK" : "FAIL") + String(" | SHT30: ") + (sht30_ready ? "OK" : "FAIL") + " | GPS: " + (g_datetime_valid ? "SYNCED" : "SEARCHING");
  padding = 51 - sensorStatus.length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");

  Serial.print("║ Runtime: ");
  unsigned long uptimeSeconds = millis() / 1000;
  unsigned long hours = uptimeSeconds / 3600;
  unsigned long minutes = (uptimeSeconds % 3600) / 60;
  unsigned long seconds = uptimeSeconds % 60;

  Serial.print(hours);
  Serial.print("h ");
  Serial.print(minutes);
  Serial.print("m ");
  Serial.print(seconds);
  Serial.print("s");

  String uptimeStr = String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
  padding = 41 - uptimeStr.length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");

  Serial.println("╚═════════════════════════════════════════════════════════════╝");
  Serial.println();
}

void printSeparator() {
  Serial.println("===============================================================");
}

void printGPSStatus() {
  // Always show GPS status line with raw values
  Serial.print("[GPS-TIME] ");

  // Show time values (mark as invalid if not synced)
  if (gps.time.isValid() && gps.date.isValid()) {
    char timeStr[30];
    snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d%s",
             g_year, g_month, g_day, g_hour, g_minute, g_second,
             g_datetime_valid ? "" : "?");
    Serial.print(timeStr);
  } else if (gps.time.isValid()) {
    // Only time available, no date
    char timeStr[15];
    snprintf(timeStr, sizeof(timeStr), "??-??-?? %02d:%02d:%02d?",
             gps.time.hour(), gps.time.minute(), gps.time.second());
    Serial.print(timeStr);
  } else {
    Serial.print("????-??-?? ??:??:??");
  }

  // Show satellite count
  Serial.print(" | Sats:");
  if (gps.satellites.isValid()) {
    Serial.print(gps.satellites.value());
  } else {
    Serial.print("?");
  }

  // Show HDOP (horizontal dilution of precision) - lower is better
  if (gps.hdop.isValid()) {
    Serial.print(" | HDOP:");
    Serial.print(gps.hdop.hdop(), 1);
  }

  // Show location (mark as invalid if not locked)
  Serial.print(" | Pos:");
  if (gps.location.isValid()) {
    Serial.print(gps.location.lat(), 4);
    Serial.print(",");
    Serial.print(gps.location.lng(), 4);
    if (!g_location_valid) {
      Serial.print("?");
    }
  } else {
    Serial.print("?.????,?.????");
  }

  // Show TDMA mode only when time is synced
  if (g_datetime_valid) {
    String mode = tdmaScheduler.getDeviceMode();
    Serial.print(" | ");
    Serial.print(mode);

    // Show next action based on mode
    if (mode == "TX_MODE") {
      TDMAStatus status = tdmaScheduler.getStatus();
      Serial.print("@");
      Serial.print(status.nextTransmissionSecond);
      Serial.print("s");
    } else if (mode == "RX_MODE") {
      Serial.print("(Listen)");
    }
  } else {
    Serial.print(" | ACQUIRING_FIX");
  }

  Serial.println();
}
