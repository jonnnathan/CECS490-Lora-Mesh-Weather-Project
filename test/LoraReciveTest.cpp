/*
#include "lora_comm.h"
#include "OLED.h"

// OLED object
OLED display;

// Timing control
unsigned long lastMessageTime = 0;
bool messageOnScreen = false;

// Counter for received messages
unsigned long messageCount = 0;

// Statistics tracking
unsigned long totalMessages = 0;
unsigned long validMessages = 0;
unsigned long corruptedMessages = 0;
float bestRSSI = -999.0;
float worstRSSI = 0.0;
float bestSNR = -999.0;
float worstSNR = 999.0;
float avgRSSI = 0.0;
float avgSNR = 0.0;

// Corruption detection for repeated sentences
String lastSentence = "";
unsigned long lastTxCount = 0;
unsigned long corruptionDetected = 0;
unsigned long duplicatesReceived = 0;

// Message quality tracking
unsigned long lastStatsDisplay = 0;
const unsigned long STATS_INTERVAL = 30000; // Show stats every 30 seconds

// --- Forward declarations ---
void printSystemInfo();
void printReceptionStats();
void printSeparator();
void updateStatistics(float rssi, float snr);
void displayMessageQuality(float rssi, float snr);
String getSignalQualityDescription(float rssi, float snr);
bool parseMessage(const String& message, String& sentence, unsigned long& txCount);
void checkForCorruption(const String& sentence, unsigned long txCount);

void setup() {
  Serial.begin(115200);
  delay(300);
  
  // Enhanced startup messages
  printSeparator();
  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                     LoRa Receiver v1.0                      ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  Serial.println();
  
  Serial.println("[STARTUP] Initializing receiver components...");
  Serial.print("[STARTUP] System time: ");
  Serial.print(millis());
  Serial.println(" ms");
  Serial.print("[STARTUP] Loop delay configured: 50 ms");
  Serial.println();
  Serial.print("[STARTUP] Statistics interval: ");
  Serial.print(STATS_INTERVAL / 1000);
  Serial.println(" seconds");
  Serial.println("[STARTUP] Corruption detection: ENABLED");
  Serial.println();

  // Initialize OLED with detailed feedback
  Serial.print("[OLED] Initializing display... ");
  if (display.init()) {
    Serial.println("✓ SUCCESS");
    Serial.println("[OLED] Display cleared and ready message shown");
    display.clearDisplay();
    display.drawString(0, 0, "LoRa Receiver Ready");
    display.updateDisplay();
  } else {
    Serial.println("✗ FAILED");
    Serial.println("[ERROR] OLED initialization failed - continuing without display");
  }

  // Initialize LoRa with detailed feedback
  Serial.print("[LORA] Initializing radio module... ");
  if (initLoRa()) {
    Serial.println("✓ SUCCESS");
    Serial.print("[LORA] Setting receive mode... ");
    setLoRaReceiveMode(); // enter continuous RX mode
    Serial.println("✓ ACTIVE");
    Serial.println("[LORA] Radio is now listening for incoming messages");
    printSystemInfo();
  } else {
    Serial.println("✗ FAILED");
    Serial.println("[ERROR] LoRa initialization failed - system will not function");
  }

  // Initialize statistics
  bestRSSI = -999.0;
  worstRSSI = 0.0;
  bestSNR = -999.0;
  worstSNR = 999.0;
  lastStatsDisplay = millis();
  
  printSeparator();
  Serial.println("[MAIN LOOP] Starting message reception loop...");
  Serial.println("[RECEIVER] Listening for incoming transmissions...");
  Serial.println("[CORRUPTION-TEST] Ready to detect message corruption patterns");
  Serial.println();
}

void loop() {
  unsigned long loopStart = millis();
  
  // Try to receive a new message
  Serial.print("[RX-POLL] Checking for messages... ");
  String msg = receiveMessage();

  if (msg.length() > 0) {
    Serial.println("✓ MESSAGE RECEIVED");
    
    // Got new message
    float rssi = getLastRSSI();
    float snr  = getLastSNR();
    totalMessages++;
    
    // Parse the message to extract sentence and count
    String sentence = "";
    unsigned long txCount = 0;
    bool parseSuccess = parseMessage(msg, sentence, txCount);
    
    // Validate message
    bool isValid = parseSuccess && sentence.length() > 0;
    if (isValid) {
      validMessages++;
      messageCount++;   // increment counter for valid messages only
      
      // Check for corruption by comparing with expected pattern
      checkForCorruption(sentence, txCount);
    } else {
      corruptedMessages++;
      Serial.println("[WARNING] Message appears corrupted or invalid");
    }

    // Update statistics
    updateStatistics(rssi, snr);
    
    // Display detailed reception information
    Serial.println();
    Serial.println("┌─────────────────────────────────────────────────────────────┐");
    Serial.print("│ MESSAGE RECEIVED #");
    Serial.print(messageCount);
    
    // Pad to align the right side
    int padding = 42 - String(messageCount).length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");
    
    Serial.print("│ Timestamp: ");
    Serial.print(millis() / 1000UL);
    Serial.print("s");
    
    // Calculate padding for timestamp
    String timestamp = String(millis() / 1000UL) + "s";
    padding = 48 - timestamp.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");
    
    Serial.print("│ TX Group: ");
    unsigned long txGroup = (txCount - 1) / 10 + 1;
    unsigned long attemptInGroup = (txCount - 1) % 10 + 1;
    Serial.print(txGroup);
    Serial.print(" (Attempt ");
    Serial.print(attemptInGroup);
    Serial.print("/10)");
    
    // Calculate padding for TX group
    String txGroupStr = String(txGroup) + " (Attempt " + String(attemptInGroup) + "/10)";
    padding = 44 - txGroupStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");
    
    // Show full sentence without truncation
    Serial.print("│ Sentence: \"");
    Serial.print(sentence);
    Serial.print("\"");
    
    // Calculate padding for sentence (minimum box width)
    int sentenceLineLength = 12 + sentence.length() + 1; // "│ Sentence: \"" + sentence + "\""
    int minBoxWidth = 65; // Minimum box width
    if (sentenceLineLength < minBoxWidth) {
      padding = minBoxWidth - sentenceLineLength;
      for(int i = 0; i < padding; i++) Serial.print(" ");
    }
    Serial.println("│");
    
    // Show full raw message without truncation
    Serial.print("│ Raw Message: \"");
    Serial.print(msg);
    Serial.print("\"");
    
    // Calculate padding for raw message
    int rawLineLength = 15 + msg.length() + 1; // "│ Raw Message: \"" + msg + "\""
    if (rawLineLength < minBoxWidth) {
      padding = minBoxWidth - rawLineLength;
      for(int i = 0; i < padding; i++) Serial.print(" ");
    }
    Serial.println("│");
    
    Serial.print("│ Length: ");
    Serial.print(msg.length());
    Serial.print(" characters");
    
    // Calculate padding for length
    String lengthStr = String(msg.length()) + " characters";
    padding = 43 - lengthStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");
    
    Serial.println("├─────────────────────────────────────────────────────────────┤");
    
    // Signal quality information
    Serial.print("│ RSSI: ");
    Serial.print(rssi, 1);
    Serial.print(" dBm");
    
    String rssiStr = String(rssi, 1) + " dBm";
    padding = 50 - rssiStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");
    
    Serial.print("│ SNR: ");
    Serial.print(snr, 1);
    Serial.print(" dB");
    
    String snrStr = String(snr, 1) + " dB";
    padding = 52 - snrStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");
    
    // Signal quality assessment
    String quality = getSignalQualityDescription(rssi, snr);
    Serial.print("│ Quality: ");
    Serial.print(quality);
    
    padding = 49 - quality.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("│");
    
    Serial.println("└─────────────────────────────────────────────────────────────┘");
    
    displayMessageQuality(rssi, snr);
    Serial.println();

    // Show on OLED for 1 second
    Serial.println("[UI] Updating display with received message");
    if (parseSuccess) {
      Serial.print("[SENTENCE-PARSED] \"");
      Serial.print(sentence);
      Serial.print("\" from transmission #");
      Serial.println(txCount);
      
      display.clearDisplay();
      display.drawString(0, 0, "RX #" + String(messageCount));
      display.drawString(0, 10, "TX #" + String(txCount) + " (" + String(attemptInGroup) + "/10)");
      
      // Show sentence on OLED (truncate if needed)
      String oledSentence = sentence;
      if (oledSentence.length() > 21) { // OLED character limit per line
        oledSentence = oledSentence.substring(0, 18) + "...";
      }
      display.drawString(0, 20, oledSentence);
      display.drawString(0, 30, "RSSI:" + String(rssi, 0) + " SNR:" + String(snr, 0));
    } else {
      Serial.println("[ERROR] Failed to parse sentence from message");
      display.clearDisplay();
      display.drawString(0, 0, "RX #" + String(messageCount));
      display.drawString(0, 10, "Parse Error");
      display.drawString(0, 20, msg.substring(0, 20));
      display.drawString(0, 30, "RSSI:" + String(rssi, 0));
    }
    display.updateDisplay();

    lastMessageTime = millis();
    messageOnScreen = true;
    
  } else {
    Serial.println("No message");
  }

  // After 1 second, switch to "Waiting..."
  if (messageOnScreen && (millis() - lastMessageTime > 1000)) {
    Serial.println("[UI] Updating display: 'Waiting...'");
    display.clearDisplay();
    display.drawString(0, 0, "Waiting...");
    display.updateDisplay();
    messageOnScreen = false;
  }
  
  // Show statistics periodically
  if (millis() - lastStatsDisplay > STATS_INTERVAL) {
    printReceptionStats();
    lastStatsDisplay = millis();
  }

  // Show loop timing if it's taking too long
  unsigned long loopDuration = millis() - loopStart;
  if (loopDuration > 100) { // Warn if loop takes more than 100ms
    Serial.print("[PERFORMANCE-WARNING] Loop took ");
    Serial.print(loopDuration);
    Serial.println(" ms (expected <100ms)");
  }

  delay(50); // small delay
}

void updateStatistics(float rssi, float snr) {
  // Update RSSI statistics
  if (rssi > bestRSSI || bestRSSI == -999.0) bestRSSI = rssi;
  if (rssi < worstRSSI || worstRSSI == 0.0) worstRSSI = rssi;
  
  // Update SNR statistics
  if (snr > bestSNR || bestSNR == -999.0) bestSNR = snr;
  if (snr < worstSNR || worstSNR == 999.0) worstSNR = snr;
  
  // Calculate running averages
  if (validMessages == 1) {
    avgRSSI = rssi;
    avgSNR = snr;
  } else {
    avgRSSI = (avgRSSI * (validMessages - 1) + rssi) / validMessages;
    avgSNR = (avgSNR * (validMessages - 1) + snr) / validMessages;
  }
}

String getSignalQualityDescription(float rssi, float snr) {
  if (rssi > -70 && snr > 10) return "Excellent";
  else if (rssi > -85 && snr > 5) return "Good";
  else if (rssi > -100 && snr > 0) return "Fair";
  else if (rssi > -110 && snr > -5) return "Poor";
  else return "Very Poor";
}

void displayMessageQuality(float rssi, float snr) {
  Serial.print("[SIGNAL-ANALYSIS] ");
  
  if (rssi > -70) Serial.print("Strong signal");
  else if (rssi > -85) Serial.print("Good signal");
  else if (rssi > -100) Serial.print("Moderate signal");
  else if (rssi > -110) Serial.print("Weak signal");
  else Serial.print("Very weak signal");
  
  Serial.print(" | ");
  
  if (snr > 10) Serial.println("Excellent noise ratio");
  else if (snr > 5) Serial.println("Good noise ratio");
  else if (snr > 0) Serial.println("Fair noise ratio");
  else if (snr > -5) Serial.println("Poor noise ratio");
  else Serial.println("Very poor noise ratio");
}

void printSystemInfo() {
  Serial.println();
  Serial.println("┌─────────────────────────────────────────────────────────────┐");
  Serial.println("│                        SYSTEM INFO                         │");
  Serial.println("├─────────────────────────────────────────────────────────────┤");
  Serial.print("│ Free RAM: ");
  #ifdef ESP32
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes                              │");
  #else
  Serial.println("N/A (platform not supported)                  │");
  #endif
  Serial.print("│ Loop Delay: 50 ms                                          │");
  Serial.println();
  Serial.print("│ Stats Interval: ");
  Serial.print(STATS_INTERVAL / 1000);
  Serial.println(" seconds                              │");
  Serial.println("│ Corruption Detection: ENABLED                              │");
  Serial.print("│ Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds                                      │");
  Serial.println("└─────────────────────────────────────────────────────────────┘");
  Serial.println();
}

void printReceptionStats() {
  Serial.println();
  Serial.println("╔═════════════════════════════════════════════════════════════╗");
  Serial.println("║                    RECEPTION STATISTICS                    ║");
  Serial.println("╠═════════════════════════════════════════════════════════════╣");
  
  Serial.print("║ Total Messages: ");
  Serial.print(totalMessages);
  
  int padding = 42 - String(totalMessages).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");
  
  Serial.print("║ Valid Messages: ");
  Serial.print(validMessages);
  
  padding = 42 - String(validMessages).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");
  
  Serial.print("║ Duplicates Received: ");
  Serial.print(duplicatesReceived);
  
  padding = 38 - String(duplicatesReceived).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");
  
  Serial.print("║ Corruption Detected: ");
  Serial.print(corruptionDetected);
  
  padding = 38 - String(corruptionDetected).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");
  
  if (totalMessages > 0) {
    float successRate = (float)validMessages / totalMessages * 100.0;
    Serial.print("║ Success Rate: ");
    Serial.print(successRate, 1);
    Serial.print("%");
    
    String rateStr = String(successRate, 1) + "%";
    padding = 44 - rateStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("║");
    
    if (validMessages > 0) {
      float corruptionRate = (float)corruptionDetected / validMessages * 100.0;
      Serial.print("║ Corruption Rate: ");
      Serial.print(corruptionRate, 1);
      Serial.print("%");
      
      String corrRateStr = String(corruptionRate, 1) + "%";
      padding = 42 - corrRateStr.length();
      for(int i = 0; i < padding; i++) Serial.print(" ");
      Serial.println("║");
    }
  }
  
  Serial.println("╠═════════════════════════════════════════════════════════════╣");
  
  if (validMessages > 0) {
    Serial.print("║ RSSI - Best: ");
    Serial.print(bestRSSI, 1);
    Serial.print(" dBm, Worst: ");
    Serial.print(worstRSSI, 1);
    Serial.print(" dBm");
    
    String rssiStr = String(bestRSSI, 1) + " dBm, Worst: " + String(worstRSSI, 1) + " dBm";
    padding = 32 - rssiStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("║");
    
    Serial.print("║ SNR - Best: ");
    Serial.print(bestSNR, 1);
    Serial.print(" dB, Worst: ");
    Serial.print(worstSNR, 1);
    Serial.print(" dB");
    
    String snrStr = String(bestSNR, 1) + " dB, Worst: " + String(worstSNR, 1) + " dB";
    padding = 35 - snrStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("║");
    
    Serial.print("║ Average RSSI: ");
    Serial.print(avgRSSI, 1);
    Serial.print(" dBm");
    
    String avgRSSIStr = String(avgRSSI, 1) + " dBm";
    padding = 42 - avgRSSIStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("║");
    
    Serial.print("║ Average SNR: ");
    Serial.print(avgSNR, 1);
    Serial.print(" dB");
    
    String avgSNRStr = String(avgSNR, 1) + " dB";
    padding = 44 - avgSNRStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("║");
  }
  
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

bool parseMessage(const String& message, String& sentence, unsigned long& txCount) {
  // Expected format: "Sentence text [#123]"
  
  // Find the last occurrence of " [#"
  int bracketStart = message.lastIndexOf(" [#");
  if (bracketStart == -1) {
    Serial.println("[PARSE-ERROR] No transmission count found in message");
    return false;
  }
  
  // Find the closing bracket
  int bracketEnd = message.indexOf(']', bracketStart);
  if (bracketEnd == -1) {
    Serial.println("[PARSE-ERROR] Malformed transmission count bracket");
    return false;
  }
  
  // Extract the sentence (everything before " [#")
  sentence = message.substring(0, bracketStart);
  sentence.trim(); // Remove any trailing whitespace
  
  // Extract the transmission count
  String countStr = message.substring(bracketStart + 3, bracketEnd); // +3 to skip " [#"
  txCount = countStr.toInt();
  
  // Validate the parsing
  if (sentence.length() == 0) {
    Serial.println("[PARSE-ERROR] Empty sentence extracted");
    return false;
  }
  
  if (txCount == 0 && countStr != "0") {
    Serial.println("[PARSE-ERROR] Invalid transmission count");
    return false;
  }
  
  Serial.print("[PARSE-SUCCESS] Extracted sentence: \"");
  Serial.print(sentence);
  Serial.print("\" with TX count: ");
  Serial.println(txCount);
  
  return true;
}

void checkForCorruption(const String& sentence, unsigned long txCount) {
  // Expected sentences (same as transmitter)
  const char* expectedSentences[] = {
    "Hello from LoRa transmitter!",
    "The quick brown fox jumps over the lazy dog.",
    "LoRa communication is working perfectly.",
    "Testing long range radio transmission.",
    "Arduino and ESP32 make great IoT devices.",
    "Wireless communication spans great distances.",
    "Radio waves travel at the speed of light.",
    "This message traveled through the air.",
    "LoRa stands for Long Range radio.",
    "Digital communication revolutionized the world."
  };
  
  const int numSentences = sizeof(expectedSentences) / sizeof(expectedSentences[0]);
  
  // Calculate which sentence should be sent based on TX count
  int expectedSentenceGroup = (txCount - 1) / 10;
  int expectedSentenceIndex = expectedSentenceGroup % numSentences;
  String expectedSentence = String(expectedSentences[expectedSentenceIndex]);
  
  // Check if received sentence matches expected
  if (sentence != expectedSentence) {
    corruptionDetected++;
    Serial.println();
    Serial.println("⚠️  CORRUPTION DETECTED! ⚠️");
    Serial.print("[CORRUPTION] Expected: \"");
    Serial.print(expectedSentence);
    Serial.println("\"");
    Serial.print("[CORRUPTION] Received:  \"");
    Serial.print(sentence);
    Serial.println("\"");
    Serial.print("[CORRUPTION] TX Count: ");
    Serial.print(txCount);
    Serial.print(" (Group ");
    Serial.print(expectedSentenceGroup + 1);
    Serial.print(", Attempt ");
    Serial.print((txCount - 1) % 10 + 1);
    Serial.println("/10)");
    
    // Character-by-character comparison for detailed analysis
    Serial.println("[CORRUPTION] Character analysis:");
    int minLen = min(sentence.length(), expectedSentence.length());
    for (int i = 0; i < minLen; i++) {
      if (sentence.charAt(i) != expectedSentence.charAt(i)) {
        Serial.print("  Position ");
        Serial.print(i);
        Serial.print(": Expected '");
        Serial.print(expectedSentence.charAt(i));
        Serial.print("', Got '");
        Serial.print(sentence.charAt(i));
        Serial.println("'");
      }
    }
    if (sentence.length() != expectedSentence.length()) {
      Serial.print("  Length mismatch: Expected ");
      Serial.print(expectedSentence.length());
      Serial.print(", Got ");
      Serial.println(sentence.length());
    }
    Serial.println();
  } else {
    Serial.print("[INTEGRITY-CHECK] ✓ Message integrity verified for TX #");
    Serial.println(txCount);
  }
  
  // Check for duplicate transmission numbers
  if (txCount == lastTxCount && txCount > 0) {
    duplicatesReceived++;
    Serial.print("[DUPLICATE] Warning: Received TX #");
    Serial.print(txCount);
    Serial.println(" again!");
  }
  
  // Store for next comparison
  lastSentence = sentence;
  lastTxCount = txCount;
}
  */