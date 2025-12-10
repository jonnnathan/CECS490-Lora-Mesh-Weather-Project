#include "transmit_queue.h"
#include "mesh_debug.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL INSTANCE                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

TransmitQueue transmitQueue;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         TRANSMIT QUEUE IMPLEMENTATION                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

TransmitQueue::TransmitQueue() : frontIndex(0), count(0) {
    // Initialize all entries to unoccupied
    for (uint8_t i = 0; i < TX_QUEUE_SIZE; i++) {
        messages[i].occupied = false;
        messages[i].length = 0;
    }
}

bool TransmitQueue::enqueue(const uint8_t* data, uint8_t len) {
    // Check if queue is full
    if (count >= TX_QUEUE_SIZE) {
        return false;  // Queue full
    }

    // Validate message length
    if (len == 0 || len > MAX_MESSAGE_SIZE) {
        return false;  // Invalid length
    }

    // Calculate back index (where to insert)
    uint8_t backIndex = (frontIndex + count) % TX_QUEUE_SIZE;

    // Copy message data
    memcpy(messages[backIndex].data, data, len);
    messages[backIndex].length = len;
    messages[backIndex].queuedAtMs = millis();
    messages[backIndex].occupied = true;

    // Increment count
    count++;

    return true;
}

QueuedMessage* TransmitQueue::peek() {
    // Check if queue is empty
    if (count == 0) {
        return nullptr;
    }

    // Return pointer to front message
    return &messages[frontIndex];
}

void TransmitQueue::dequeue() {
    // Check if queue is empty
    if (count == 0) {
        return;
    }

    // Mark front message as unoccupied
    messages[frontIndex].occupied = false;
    messages[frontIndex].length = 0;

    // Advance front index (circular buffer)
    frontIndex = (frontIndex + 1) % TX_QUEUE_SIZE;

    // Decrement count
    count--;

    DEBUG_QUE_F("Dequeued | depth=%d/%d", count, TX_QUEUE_SIZE);
}

uint8_t TransmitQueue::depth() const {
    return count;
}

void TransmitQueue::pruneOld(uint32_t maxAgeMs) {
    if (count == 0) {
        return;
    }

    uint32_t now = millis();
    uint8_t pruned = 0;

    // Check all messages in queue order
    uint8_t checkCount = count;
    for (uint8_t i = 0; i < checkCount; i++) {
        uint8_t idx = (frontIndex + i) % TX_QUEUE_SIZE;

        if (messages[idx].occupied) {
            // Check if message has expired
            if (now - messages[idx].queuedAtMs > maxAgeMs) {
                // This message is too old - if it's at the front, dequeue it
                if (i == 0) {
                    dequeue();
                    pruned++;
                    // Adjust loop since we removed front
                    i--;
                    checkCount--;
                } else {
                    // Message in middle/back of queue - mark invalid but keep position
                    // It will be naturally dequeued when we reach it
                    messages[idx].occupied = false;
                    messages[idx].length = 0;
                    pruned++;
                }
            }
        }
    }

    // Compact queue if we marked any middle entries as invalid
    if (pruned > 0 && count > 0) {
        // Remove any gaps created by marking middle entries invalid
        uint8_t writePos = frontIndex;
        uint8_t readPos = frontIndex;
        uint8_t newCount = 0;

        for (uint8_t i = 0; i < TX_QUEUE_SIZE; i++) {
            readPos = (frontIndex + i) % TX_QUEUE_SIZE;

            if (messages[readPos].occupied) {
                if (readPos != writePos) {
                    // Copy message to fill gap
                    memcpy(&messages[writePos], &messages[readPos], sizeof(QueuedMessage));
                    messages[readPos].occupied = false;
                }
                writePos = (writePos + 1) % TX_QUEUE_SIZE;
                newCount++;
            }
        }

        count = newCount;
        DEBUG_QUE_F("Pruned %d old message(s) | depth=%d/%d", pruned, count, TX_QUEUE_SIZE);
    }
}

void TransmitQueue::clear() {
    for (uint8_t i = 0; i < TX_QUEUE_SIZE; i++) {
        messages[i].occupied = false;
        messages[i].length = 0;
    }
    frontIndex = 0;
    count = 0;
}
