#include "duplicate_cache.h"

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL INSTANCE                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

DuplicateCache duplicateCache;

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DUPLICATE CACHE IMPLEMENTATION                    ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

DuplicateCache::DuplicateCache() : writeIndex(0) {
    // Initialize all entries to invalid
    for (uint8_t i = 0; i < SEEN_CACHE_SIZE; i++) {
        messages[i].valid = false;
    }
}

bool DuplicateCache::isDuplicate(uint8_t sourceId, uint8_t messageId) {
    unsigned long now = millis();

    // Search through cache for matching message
    for (uint8_t i = 0; i < SEEN_CACHE_SIZE; i++) {
        if (messages[i].valid) {
            // Check if entry is expired
            if (now - messages[i].timestampMs > DUPLICATE_WINDOW_MS) {
                // Mark as invalid - this entry has expired
                messages[i].valid = false;
                continue;
            }

            // Check for match
            if (messages[i].sourceId == sourceId && messages[i].messageId == messageId) {
                // Found a duplicate!
                return true;
            }
        }
    }

    // Not found - this is a new message
    return false;
}

void DuplicateCache::markSeen(uint8_t sourceId, uint8_t messageId) {
    unsigned long now = millis();

    // Use ring buffer - write to current index and advance
    messages[writeIndex].sourceId = sourceId;
    messages[writeIndex].messageId = messageId;
    messages[writeIndex].timestampMs = now;
    messages[writeIndex].valid = true;

    // Advance write pointer (ring buffer wraparound)
    writeIndex = (writeIndex + 1) % SEEN_CACHE_SIZE;
}

uint8_t DuplicateCache::prune() {
    unsigned long now = millis();
    uint8_t pruned = 0;

    for (uint8_t i = 0; i < SEEN_CACHE_SIZE; i++) {
        if (messages[i].valid) {
            // Check if entry has expired
            if (now - messages[i].timestampMs > DUPLICATE_WINDOW_MS) {
                messages[i].valid = false;
                pruned++;
            }
        }
    }

    return pruned;
}

void DuplicateCache::clear() {
    for (uint8_t i = 0; i < SEEN_CACHE_SIZE; i++) {
        messages[i].valid = false;
    }
    writeIndex = 0;
}

uint8_t DuplicateCache::getCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < SEEN_CACHE_SIZE; i++) {
        if (messages[i].valid) {
            count++;
        }
    }
    return count;
}
