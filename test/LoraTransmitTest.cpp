/*
#include "lora_comm.h"
#include "OLED.h"

OLED display;

// precise schedule
const unsigned long TX_INTERVAL = 3000; // 3s (keep >= 1000ms so RX UI isn't spammed)
unsigned long nextTx = 0;
unsigned long txSeq  = 0;

// UI timing for Waiting... after a successful send
static unsigned long lastSendAt = 0;
static bool          sentFlag   = false;

// Statistics tracking
static unsigned long totalTransmissions = 0;
static unsigned long successfulTransmissions = 0;
static unsigned long failedTransmissions = 0;

// --- fwd decls ---
bool sendHelloWorld();
inline void scheduleNextTick(unsigned long& nextTick,
                             unsigned long interval,
                             unsigned long now);
void printSystemInfo();
void printTransmissionStats();
void printSeparator();

void setup() {
  Serial.begin(115200);
  delay(200);
  
  // Enhanced startup messages
  printSeparator();
  Serial.println("╔══════════════════════════════════════════════════════════════╗");
  Serial.println("║                    LoRa Transmitter v1.0                    ║");
  Serial.println("╚══════════════════════════════════════════════════════════════╝");
  Serial.println();
  
  Serial.println("[STARTUP] Initializing system components...");
  Serial.print("[STARTUP] System time: ");
  Serial.print(millis());
  Serial.println(" ms");
  Serial.print("[STARTUP] TX interval configured: ");
  Serial.print(TX_INTERVAL);
  Serial.println(" ms");
  Serial.println("[STARTUP] Corruption test mode: Same sentence x10 attempts");
  Serial.println();

  // OLED initialization with detailed feedback
  Serial.print("[OLED] Initializing display... ");
  if (display.init()) {
    Serial.println("✓ SUCCESS");
    Serial.println("[OLED] Display cleared and ready message shown");
    display.clearDisplay();
    display.drawString(0, 0, "LoRa TX Ready");
    display.updateDisplay();
  } else {
    Serial.println("✗ FAILED");
    Serial.println("[ERROR] OLED initialization failed - continuing without display");
  }

  // LoRa initialization with detailed feedback
  Serial.print("[LORA] Initializing radio module... ");
  if (initLoRa()) {
    Serial.println("✓ SUCCESS");
    Serial.println("[LORA] Radio module ready for transmission");
    printSystemInfo();
  } else {
    Serial.println("✗ FAILED");
    Serial.println("[ERROR] LoRa initialization failed - system may not function properly");
  }

  // Schedule first transmission
  nextTx = millis();
  Serial.print("[SCHEDULER] First transmission scheduled at: ");
  Serial.print(nextTx);
  Serial.println(" ms");
  
  printSeparator();
  Serial.println("[MAIN LOOP] Starting transmission loop...");
  Serial.println();
}

void loop() {
  unsigned long now = millis();

  // Fire exactly on schedule
  if ((long)(now - nextTx) >= 0) {
    unsigned long scheduledTime = nextTx;
    unsigned long actualTime = now;
    long timingError = (long)(actualTime - scheduledTime);
    
    Serial.print("[TX-SCHEDULE] Transmission #");
    Serial.print(txSeq + 1);
    Serial.print(" triggered");
    if (timingError > 5) {  // Only show timing error if significant (>5ms)
      Serial.print(" (");
      Serial.print(timingError);
      Serial.print("ms late)");
    }
    Serial.println();
    
    bool success = sendHelloWorld();
    totalTransmissions++;

    if (success) {
      successfulTransmissions++;
      // Record actual send time and kick off UI cycle
      lastSendAt = now;
      sentFlag   = true;

      Serial.println("[UI] Updating display: 'Sending...'");
      display.clearDisplay();
      display.drawString(0, 0, "Sending...");
      display.updateDisplay();
    } else {
      failedTransmissions++;
      Serial.println("[UI] Updating display: 'TX failed'");
      // Optional: brief feedback for failures (doesn't start Waiting... cycle)
      display.clearDisplay();
      display.drawString(0, 0, "TX failed");
      display.updateDisplay();
    }

    // Jump to the first future tick (no drift, no burst)
    unsigned long oldNextTx = nextTx;
    scheduleNextTick(nextTx, TX_INTERVAL, now);
    
    Serial.print("[SCHEDULER] Next transmission scheduled for: ");
    Serial.print(nextTx);
    Serial.print(" ms (in ");
    Serial.print((nextTx - now));
    Serial.println(" ms)");
    
    // Show stats every 10 transmissions
    if (totalTransmissions % 10 == 0) {
      printTransmissionStats();
    }
    
    Serial.println();
  }

  // After 1s following a successful send, show "Waiting..." for the remaining ~2s
  if (sentFlag) {
    unsigned long elapsed = millis() - lastSendAt; // rollover-safe unsigned diff
    
    if (elapsed >= 1000 && elapsed < 3000) {
      static bool waitingDisplayed = false;
      if (!waitingDisplayed) {
        Serial.println("[UI] Updating display: 'Waiting...'");
        display.clearDisplay();
        display.drawString(0, 0, "Waiting...");
        display.updateDisplay();
        waitingDisplayed = true;
      }
    } else if (elapsed >= 3000) {
      Serial.println("[UI-CYCLE] Transmission cycle complete");
      sentFlag = false; // end of the cycle; next loop will handle UI again
    }
    
    // Reset the waiting flag when we exit the waiting period
    static unsigned long lastElapsed = 0;
    if (lastElapsed >= 1000 && elapsed < 1000) {
      // We've wrapped around, reset the flag
    }
    lastElapsed = elapsed;
  }
}

bool sendHelloWorld() {
  Serial.print("[LORA-CHECK] Verifying radio readiness... ");
  if (!isLoRaReady()) {
    Serial.println("✗ NOT READY");
    Serial.println("[ERROR] LoRa module not in ready state");
    return false;
  }
  Serial.println("✓ READY");

  txSeq++;

  // Array of sentences to transmit
  const char* sentences[] = {
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
  
  const int numSentences = sizeof(sentences) / sizeof(sentences[0]);
  
  // Send the same sentence for 10 attempts, then move to next sentence
  int sentenceGroup = (txSeq - 1) / 10; // Which group of 10 transmissions
  int attemptInGroup = (txSeq - 1) % 10 + 1; // Which attempt within the group (1-10)
  int sentenceIndex = sentenceGroup % numSentences; // Cycle through sentences every 10 attempts
  
  // Create payload with sentence and count
  char payload[100]; // Increased size to accommodate longer sentences
  snprintf(payload, sizeof(payload), "%s [#%lu]", sentences[sentenceIndex], txSeq);

  Serial.println("┌─────────────────────────────────────────────────────────────┐");
  Serial.print("│ TRANSMISSION #");
  Serial.print(txSeq);
  
  // Pad to align the right side
  int padding = 43 - String(txSeq).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");
  
  Serial.print("│ Sentence Group: #");
  Serial.print(sentenceIndex + 1);
  Serial.print(" (Attempt ");
  Serial.print(attemptInGroup);
  Serial.print("/10)");
  
  // Calculate padding for sentence group info
  String groupInfo = "#" + String(sentenceIndex + 1) + " (Attempt " + String(attemptInGroup) + "/10)";
  padding = 44 - groupInfo.length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");
  
  Serial.print("│ Message: \"");
  // Truncate for display if needed
  String displayPayload = String(payload);
  if (displayPayload.length() > 45) {
    displayPayload = displayPayload.substring(0, 42) + "...";
  }
  Serial.print(displayPayload);
  Serial.print("\"");
  
  // Calculate padding for payload
  padding = 47 - displayPayload.length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");
  
  Serial.print("│ Message size: ");
  Serial.print(strlen(payload));
  Serial.print(" bytes");
  
  // Calculate padding for size
  String sizeStr = String(strlen(payload)) + " bytes";
  padding = 44 - sizeStr.length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("│");
  
  Serial.println("└─────────────────────────────────────────────────────────────┘");

  Serial.print("[LORA-TX] Transmitting... ");
  
  // If your lora_comm has a const char* overload, prefer passing payload directly.
  bool ok = sendMessage(String(payload));
  
  if (ok) {
    Serial.println("✓ SUCCESS");
    Serial.print("[LORA-TX] Message transmitted successfully at ");
    Serial.print(millis());
    Serial.println(" ms");
  } else {
    Serial.println("✗ FAILED");
    Serial.println("[ERROR] Transmission failed - check radio module and antenna");
  }
  
  return ok;
}

// Schedules nextTick to the first future slot aligned to 'interval'.
// - No drift: stays aligned to the original cadence
// - No burst: skips any missed slots instead of firing back-to-back
inline void scheduleNextTick(unsigned long& nextTick,
                             unsigned long interval,
                             unsigned long now) {
  if ((long)(now - nextTick) < 0) return; // already in the future
  unsigned long diff   = (unsigned long)((long)(now - nextTick)); // safe when now>=nextTick
  unsigned long missed = diff / interval + 1;                     // how many slots behind
  
  if (missed > 1) {
    Serial.print("[SCHEDULER-WARNING] Skipped ");
    Serial.print(missed - 1);
    Serial.print(" transmission slot(s) due to timing delay");
    Serial.println();
  }
  
  nextTx += missed * interval;                                  // jump to first future tick
}

void printSystemInfo() {
  Serial.println();
  Serial.println("┌─────────────────────────────────────────────────────────────┐");
  Serial.println("│                        SYSTEM INFO                         │");
  Serial.println("├─────────────────────────────────────────────────────────────┤");
  Serial.print("│ Free RAM: ");
  // Note: This requires ESP32/Arduino - adjust for your platform
  #ifdef ESP32
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes                              │");
  #else
  Serial.println("N/A (platform not supported)                  │");
  #endif
  Serial.print("│ TX Interval: ");
  Serial.print(TX_INTERVAL);
  Serial.println(" ms                                    │");
  Serial.println("│ Mode: Corruption Test (10x same sentence)                  │");
  Serial.print("│ Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds                                      │");
  Serial.println("└─────────────────────────────────────────────────────────────┘");
  Serial.println();
}

void printTransmissionStats() {
  Serial.println();
  Serial.println("╔═════════════════════════════════════════════════════════════╗");
  Serial.println("║                    TRANSMISSION STATISTICS                 ║");
  Serial.println("╠═════════════════════════════════════════════════════════════╣");
  
  Serial.print("║ Total Transmissions: ");
  Serial.print(totalTransmissions);
  
  int padding = 38 - String(totalTransmissions).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");
  
  Serial.print("║ Successful: ");
  Serial.print(successfulTransmissions);
  
  padding = 46 - String(successfulTransmissions).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");
  
  Serial.print("║ Failed: ");
  Serial.print(failedTransmissions);
  
  padding = 50 - String(failedTransmissions).length();
  for(int i = 0; i < padding; i++) Serial.print(" ");
  Serial.println("║");
  
  if (totalTransmissions > 0) {
    float successRate = (float)successfulTransmissions / totalTransmissions * 100.0;
    Serial.print("║ Success Rate: ");
    Serial.print(successRate, 1);
    Serial.print("%");
    
    String rateStr = String(successRate, 1) + "%";
    padding = 44 - rateStr.length();
    for(int i = 0; i < padding; i++) Serial.print(" ");
    Serial.println("║");
  }
  
  // Show current sentence group info
  if (txSeq > 0) {
    int currentGroup = ((txSeq - 1) / 10) % 10 + 1;
    int currentAttempt = (txSeq - 1) % 10 + 1;
    Serial.print("║ Current: Group ");
    Serial.print(currentGroup);
    Serial.print(", Attempt ");
    Serial.print(currentAttempt);
    Serial.print("/10");
    
    String currentInfo = "Group " + String(currentGroup) + ", Attempt " + String(currentAttempt) + "/10";
    padding = 41 - currentInfo.length();
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

*/