#ifndef NEIGHBOR_TABLE_H
#define NEIGHBOR_TABLE_H

#include <Arduino.h>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NEIGHBOR TABLE CONFIGURATION                      ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

#define MAX_NEIGHBORS 10                    // Maximum number of neighbors to track
#define NEIGHBOR_TIMEOUT_MS 180000          // 3 minutes - neighbor considered stale after this

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NEIGHBOR STRUCTURE                                ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Neighbor - Represents a neighboring node in the mesh network
 *
 * Tracks signal strength statistics and activity for routing decisions.
 * Used to determine which neighbors are good candidates for forwarding packets.
 */
struct Neighbor {
    uint8_t  nodeId;            // Node ID of the neighbor
    int16_t  rssi;              // Last received signal strength (dBm)
    int16_t  rssiMin;           // Minimum RSSI observed (weakest signal)
    int16_t  rssiMax;           // Maximum RSSI observed (strongest signal)
    uint32_t lastHeardMs;       // Timestamp of last packet received (millis)
    uint8_t  packetsReceived;   // Number of packets received from this neighbor
    bool     isActive;          // True if entry is in use

    // Constructor
    Neighbor() :
        nodeId(0),
        rssi(-120),
        rssiMin(-120),
        rssiMax(-120),
        lastHeardMs(0),
        packetsReceived(0),
        isActive(false)
    {}
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         NEIGHBOR TABLE CLASS                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * NeighborTable - Manages a list of neighboring nodes
 *
 * Maintains information about nearby nodes based on received packets.
 * Used for routing decisions and network topology awareness.
 *
 * Usage:
 *   NeighborTable neighbors;
 *   neighbors.update(nodeId, rssi);      // Add or update neighbor
 *   Neighbor* n = neighbors.get(nodeId); // Look up neighbor
 *   neighbors.pruneExpired(180000);      // Remove stale neighbors
 */
class NeighborTable {
private:
    Neighbor neighbors[MAX_NEIGHBORS];  // Fixed-size array of neighbors
    uint8_t count;                      // Current number of active neighbors

public:
    // Constructor
    NeighborTable();

    /**
     * Update or add a neighbor entry
     *
     * If the neighbor exists, updates RSSI stats and timestamp.
     * If not found and space available, creates new entry.
     *
     * @param nodeId - ID of the neighboring node
     * @param rssi - Signal strength of received packet (dBm)
     */
    void update(uint8_t nodeId, int16_t rssi);

    /**
     * Get neighbor by node ID
     *
     * @param nodeId - ID to search for
     * @return Pointer to Neighbor if found, nullptr otherwise
     */
    Neighbor* get(uint8_t nodeId);

    /**
     * Remove expired neighbors
     *
     * Marks neighbors as inactive if not heard from within timeout period.
     *
     * @param timeoutMs - Time in milliseconds before considering neighbor stale
     * @return Number of neighbors pruned
     */
    uint8_t pruneExpired(uint32_t timeoutMs);

    /**
     * Get count of active neighbors
     *
     * @return Number of currently active neighbor entries
     */
    uint8_t getActiveCount() const;

    /**
     * Get all active neighbors
     *
     * Useful for iterating through the neighbor list for routing decisions.
     *
     * @param outNeighbors - Array to fill with pointers to active neighbors
     * @param maxCount - Maximum number of entries to return
     * @return Number of neighbors returned
     */
    uint8_t getActiveNeighbors(Neighbor** outNeighbors, uint8_t maxCount);

    /**
     * Clear all neighbor entries
     *
     * Resets the entire table to empty state.
     */
    void clear();

    /**
     * Get average RSSI for a neighbor
     *
     * Returns the average of min and max observed RSSI.
     * Provides a more stable metric than last RSSI alone.
     *
     * @param nodeId - Node to query
     * @return Average RSSI, or -120 if not found
     */
    int16_t getAverageRSSI(uint8_t nodeId);
};

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         GLOBAL INSTANCE                                   ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

// Global neighbor table instance
extern NeighborTable neighborTable;

#endif // NEIGHBOR_TABLE_H
