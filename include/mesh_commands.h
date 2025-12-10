#ifndef MESH_COMMANDS_H
#define MESH_COMMANDS_H

#include <Arduino.h>

// ╔═══════════════════════════════════════════════════════════════════════════╗
// ║                         MESH COMMAND HANDLER                              ║
// ╚═══════════════════════════════════════════════════════════════════════════╝

/**
 * Serial Command Handler for Mesh Network Testing
 *
 * Available Commands:
 *   mesh status  - Print neighbor table and queue status
 *   mesh stats   - Print detailed mesh statistics
 *   mesh reset   - Clear all caches and reset statistics
 *   mesh test    - Send test message with configurable TTL
 *   mesh help    - Show command help
 *
 * Usage:
 *   Call processMeshCommands() regularly in main loop
 *   Commands are processed when user enters text via Serial Monitor
 */

/**
 * Process incoming serial commands
 * Call this in your main loop to check for commands
 */
void processMeshCommands();

/**
 * Print available mesh commands
 */
void printMeshCommandHelp();

/**
 * Print current neighbor table
 */
void printNeighborTable();

/**
 * Print queue status
 */
void printQueueStatus();

/**
 * Print duplicate cache status
 */
void printDuplicateCacheStatus();

/**
 * Send a test message with specific parameters
 *
 * @param destId - Destination node ID (use 0xFF for broadcast)
 * @param ttl - Time to live (number of hops allowed)
 * @param testData - Optional test data string
 */
void sendTestMessage(uint8_t destId, uint8_t ttl, const char* testData);

/**
 * Reset all mesh subsystems
 * Clears duplicate cache, neighbor table, transmit queue, and resets stats
 */
void resetMeshSubsystems();

#endif // MESH_COMMANDS_H
