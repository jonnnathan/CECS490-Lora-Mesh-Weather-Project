#ifndef DUPLICATE_CACHE_H
#define DUPLICATE_CACHE_H

#include <Arduino.h>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DUPLICATE CACHE CONFIGURATION                     ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#define SEEN_CACHE_SIZE 32                  // Number of recent messages to track
#define DUPLICATE_WINDOW_MS 120000          // 2 minutes - messages older than this are forgotten

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         SEEN MESSAGE STRUCTURE                            ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * SeenMessage - Represents a recently received message for duplicate detection
 *
 * Tracks the unique identifier (sourceId + messageId) and timestamp of messages
 * to prevent processing the same message multiple times due to mesh forwarding.
 */
struct SeenMessage {
    uint8_t  sourceId;      // Original sender of the message
    uint8_t  messageId;     // Message sequence number (wraps at 255)
    uint32_t timestampMs;   // When this message was first seen (millis)
    bool     valid;         // True if this cache slot is in use

    // Constructor
    SeenMessage() :
        sourceId(0),
        messageId(0),
        timestampMs(0),
        valid(false)
    {}
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         DUPLICATE CACHE CLASS                             ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * DuplicateCache - Tracks recently seen messages to detect duplicates
 *
 * Maintains a ring buffer of recent messages. When a message arrives, we check
 * if we've seen the same (sourceId, messageId) pair recently. This prevents
 * processing duplicates that occur when:
 * - Multiple nodes forward the same broadcast message
 * - A packet is received via multiple paths in the mesh
 * - Retransmissions or corrupted packets cause re-sends
 *
 * Usage:
 *   DuplicateCache cache;
 *   if (cache.isDuplicate(sourceId, msgId)) {
 *       // Ignore duplicate
 *   } else {
 *       cache.markSeen(sourceId, msgId);
 *       // Process message
 *   }
 */
class DuplicateCache {
private:
    SeenMessage messages[SEEN_CACHE_SIZE];  // Ring buffer of seen messages
    uint8_t writeIndex;                     // Next slot to write (ring buffer pointer)

public:
    // Constructor
    DuplicateCache();

    /**
     * Check if message is a duplicate
     *
     * Searches the cache for a matching (sourceId, messageId) pair that was
     * seen within the duplicate window. Also prunes expired entries as a
     * side effect to keep the cache fresh.
     *
     * @param sourceId - Original sender node ID
     * @param messageId - Message sequence number (0-255, wraps)
     * @return true if this message was recently seen, false if new
     */
    bool isDuplicate(uint8_t sourceId, uint8_t messageId);

    /**
     * Mark message as seen
     *
     * Records this message in the cache using a ring buffer. If the cache is
     * full, the oldest entry is automatically overwritten.
     *
     * @param sourceId - Original sender node ID
     * @param messageId - Message sequence number
     */
    void markSeen(uint8_t sourceId, uint8_t messageId);

    /**
     * Remove expired entries
     *
     * Scans the cache and marks entries as invalid if they're older than
     * DUPLICATE_WINDOW_MS. This keeps the cache lean and prevents false
     * positives after sequence number wraparound.
     *
     * @return Number of entries pruned
     */
    uint8_t prune();

    /**
     * Clear all entries
     *
     * Resets the entire cache to empty state. Useful for testing or when
     * re-initializing the network.
     */
    void clear();

    /**
     * Get count of valid entries
     *
     * @return Number of currently cached messages
     */
    uint8_t getCount() const;
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL INSTANCE                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Global duplicate cache instance
extern DuplicateCache duplicateCache;

#endif // DUPLICATE_CACHE_H
