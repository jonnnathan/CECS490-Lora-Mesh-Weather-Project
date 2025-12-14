/**
 * @file MeshContext.h
 * @brief Central dependency injection container for the mesh network system.
 *
 * The MeshContext provides a structured way to pass references to major
 * subsystems throughout the application, eliminating the need for scattered
 * `extern` declarations and global variable access.
 *
 * @section Purpose
 * Instead of having each module access globals directly via `extern`, modules
 * receive a pointer to MeshContext which contains references to all siblings.
 * This approach:
 * - Makes dependencies explicit and documented
 * - Enables easier unit testing with mock implementations
 * - Prevents circular dependency issues
 * - Centralizes system initialization order
 *
 * @section Lifecycle
 * 1. MeshContext is created in main.cpp during setup()
 * 2. All subsystem pointers are initialized after their respective begin() calls
 * 3. The context pointer is passed to modules that need cross-system access
 * 4. Context remains valid for the entire application lifetime
 *
 * @section Usage
 * @code
 * // In main.cpp setup()
 * MeshContext ctx;
 * ctx.radio = &loraComm;
 * ctx.scheduler = &tdmaScheduler;
 * ctx.routing = &gradientRouting;
 * ctx.sensors = &sensorManager;
 *
 * // Pass to modules that need it
 * packetHandler.setContext(&ctx);
 *
 * // In PacketHandler
 * void PacketHandler::processPacket() {
 *     if (m_ctx->scheduler->isMySlot()) {
 *         m_ctx->radio->send(packet);
 *     }
 * }
 * @endcode
 *
 * @author ESP32 LoRa Mesh Team
 * @version 1.0.0
 * @date 2025
 */

#ifndef MESH_CONTEXT_H
#define MESH_CONTEXT_H

#include <Arduino.h>  // For uint8_t and other standard types

// Forward declarations to avoid circular includes
class TDMAScheduler;
class SensorManager;
class TransmitQueue;
class NeighborTable;
class DuplicateCache;
class IRadio;  // Radio interface for hardware abstraction

// Forward declarations for modules not yet refactored to classes
// These will be updated as refactoring progresses
struct DisplayInterface;     // Will become IDisplay
struct GradientRoutingState; // Will become GradientRouter

/**
 * @brief Central container holding references to all major mesh subsystems.
 *
 * MeshContext implements the Service Locator pattern, providing a single
 * point of access to system-wide services. Unlike true dependency injection,
 * this approach is simpler for embedded systems while still making
 * dependencies explicit.
 *
 * @note All pointers are non-owning. The actual objects are owned by main.cpp
 *       or other modules. MeshContext only holds references.
 *
 * @warning Do not delete objects through MeshContext pointers.
 */
struct MeshContext {
    // =========================================================================
    // Core Subsystems
    // =========================================================================

    /**
     * @brief TDMA time slot scheduler.
     *
     * Manages GPS-synchronized transmission slots. Used by:
     * - Transmission logic to check if current slot is ours
     * - Display module to show current slot status
     * - Statistics module to track TDMA compliance
     *
     * @note Initialized in setup() after GPS module init.
     * @see TDMAScheduler
     */
    TDMAScheduler* scheduler;

    /**
     * @brief Environmental sensor manager.
     *
     * Provides access to temperature, humidity, pressure, and altitude
     * readings from attached sensors (SHT30, BMP180).
     *
     * @note Initialized in setup() after I2C bus init.
     * @see SensorManager
     */
    SensorManager* sensors;

    // =========================================================================
    // Network Infrastructure
    // =========================================================================

    /**
     * @brief Packet transmission queue.
     *
     * FIFO queue holding packets scheduled for forwarding during
     * our TDMA slot. Used by:
     * - Packet handler to enqueue received packets for forwarding
     * - Main loop to dequeue and transmit during our slot
     *
     * @note Thread-safe for ISR enqueue operations.
     * @see TransmitQueue
     */
    TransmitQueue* txQueue;

    /**
     * @brief Direct neighbor tracking table.
     *
     * Maintains list of nodes heard directly (1-hop neighbors)
     * with RSSI statistics. Used by:
     * - Routing decisions
     * - Network topology visualization
     * - Link quality monitoring
     *
     * @see NeighborTable
     */
    NeighborTable* neighbors;

    /**
     * @brief Duplicate packet detection cache.
     *
     * Tracks recently seen (sourceId, messageId) pairs to prevent
     * processing the same packet multiple times in the mesh.
     *
     * @see DuplicateCache
     */
    DuplicateCache* dupCache;

    /**
     * @brief Radio interface for LoRa communication.
     *
     * Points to either LoRaHW (ESP32 hardware) or SimulatedRadio (native).
     * Used for all packet transmission and reception.
     *
     * @note Initialized in setup() based on build target.
     * @see IRadio, LoRaHW, SimulatedRadio
     */
    IRadio* radio;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief This node's unique identifier.
     *
     * 8-bit device ID used in mesh protocol headers.
     * Value 1 is reserved for the gateway node.
     */
    uint8_t deviceId;

    /**
     * @brief Whether this node is the mesh gateway.
     *
     * Gateway nodes have special responsibilities:
     * - Originate routing beacons
     * - Upload data to ThingSpeak
     * - Don't forward broadcast packets (loop prevention)
     */
    bool isGateway;

    // =========================================================================
    // Constructor
    // =========================================================================

    /**
     * @brief Construct a new MeshContext with null pointers.
     *
     * All pointers must be assigned before use. The constructor
     * initializes everything to safe defaults.
     */
    MeshContext()
        : scheduler(nullptr)
        , sensors(nullptr)
        , txQueue(nullptr)
        , neighbors(nullptr)
        , dupCache(nullptr)
        , radio(nullptr)
        , deviceId(0)
        , isGateway(false)
    {
    }

    // =========================================================================
    // Validation
    // =========================================================================

    /**
     * @brief Check if all required subsystems are initialized.
     *
     * @return true if all critical pointers are non-null
     * @return false if any required subsystem is missing
     *
     * @note Call this after setup() to verify initialization.
     */
    bool isValid() const {
        return scheduler != nullptr
            && sensors != nullptr
            && txQueue != nullptr
            && neighbors != nullptr
            && dupCache != nullptr
            && deviceId != 0;
    }

    /**
     * @brief Check if core subsystems are ready (minimal set).
     *
     * Used during early initialization when not all systems
     * are available yet.
     *
     * @return true if scheduler is initialized
     */
    bool hasScheduler() const {
        return scheduler != nullptr;
    }

    /**
     * @brief Check if sensor subsystem is available.
     *
     * @return true if sensor manager is initialized
     */
    bool hasSensors() const {
        return sensors != nullptr;
    }

    /**
     * @brief Check if radio interface is available.
     *
     * @return true if radio interface is initialized
     */
    bool hasRadio() const {
        return radio != nullptr;
    }
};

/**
 * @brief Global mesh context instance.
 *
 * Single global instance created in main.cpp and populated during setup().
 * Modules can include this header and access `g_meshContext` directly,
 * or receive a pointer to it for better testability.
 *
 * @note Prefer passing MeshContext* to classes over using this global.
 */
extern MeshContext g_meshContext;

#endif // MESH_CONTEXT_H
