#ifndef TRANSMIT_QUEUE_H
#define TRANSMIT_QUEUE_H

#include <Arduino.h>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TRANSMIT QUEUE CONFIGURATION                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#define TX_QUEUE_SIZE 8
#define MAX_MESSAGE_SIZE 64

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         QUEUED MESSAGE STRUCTURE                          ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

struct QueuedMessage {
    uint8_t  data[MAX_MESSAGE_SIZE];  // Message payload
    uint8_t  length;                   // Actual data length
    uint32_t queuedAtMs;               // Timestamp when queued
    bool     occupied;                 // Slot in use
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TRANSMIT QUEUE CLASS                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

class TransmitQueue {
private:
    QueuedMessage messages[TX_QUEUE_SIZE];
    uint8_t frontIndex;  // Read position (dequeue from here)
    uint8_t count;       // Number of messages in queue

public:
    TransmitQueue();

    // Queue operations
    bool enqueue(const uint8_t* data, uint8_t len);  // Add message to back, returns false if full
    QueuedMessage* peek();                            // Get front message without removing
    void dequeue();                                   // Remove front message
    uint8_t depth() const;                            // Count queued messages
    void pruneOld(uint32_t maxAgeMs);                 // Remove stale messages
    void clear();                                     // Clear all messages
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL INSTANCE                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

extern TransmitQueue transmitQueue;

#endif // TRANSMIT_QUEUE_H
