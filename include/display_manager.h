#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include "OLED.h"
#include "lora_comm.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DISPLAY STATES                                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

enum DisplayState {
    DISPLAY_WAITING,
    DISPLAY_SENDING,
    DISPLAY_RECEIVED_MSG,
    DISPLAY_TX_FAILED
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DISPLAY MESSAGE STRUCT                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

struct DisplayMessage {
    String payload;
    String meta;
    uint8_t originId;
    uint16_t seq;
    float rssi;
    float snr;
    unsigned long timestamp;
    bool isNew;
    bool isValid;
    
    DisplayMessage();
    void clear();
    void updateFromPacket(const LoRaReceivedPacket& packet);
    void updateFromTx(const String& msg, uint16_t seqNum);
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL DISPLAY ACCESS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

extern OLED display;
extern DisplayMessage rxMessage;
extern DisplayMessage txMessage;
extern DisplayState currentDisplay;
extern unsigned long displayStateStart;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         FUNCTION DECLARATIONS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

bool initDisplay();
void updateDisplay();
void forceDisplayUpdate();
void setDisplayState(DisplayState state);

// Convenience functions for updating display from events
void updateRxDisplay(const LoRaReceivedPacket& packet);
void updateTxDisplay(const String& payload, uint16_t seq);
void showTxFailed();
void updateRxDisplayFullReport(const LoRaReceivedPacket& packet, const FullReportMsg& report);
#endif // DISPLAY_MANAGER_H