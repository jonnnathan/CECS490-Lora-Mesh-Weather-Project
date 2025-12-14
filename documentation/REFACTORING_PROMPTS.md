# Professional Code Refactoring - Instruction Sets

This document contains a series of detailed, step-by-step instruction sets for refactoring the ESP32 LoRa Mesh project. These instructions are designed to be executed sequentially to modernize the codebase while preserving all original functionality.

**Documentation Standard:** All new header files (`.h`) must use Doxygen-style comments (`/** ... */`) for every class, method, and member variable. Comments must explain the purpose, parameters (`@param`), and return values (`@return`).

---

## Phase 0: Project Restructuring (Housekeeping)

**Goal:** Clean up the cluttered `src/` directory by organizing files into logical modules.

### Instruction Set: Organize Source Files
**Prompt / Task:**
> "Reorganize the `src/` directory into logical subfolders.
> 1.  Create the following directories inside `src/`:
>     *   `core`
>     *   `radio`
>     *   `routing`
>     *   `timing`
>     *   `sensors` (merge with existing `managers` if applicable)
>     *   `ui`
>     *   `utils`
> 2.  Move the files as follows:
>     *   **Core:** `main.cpp`, `config.cpp`, `memory_monitor.cpp`, `MeshContext.cpp`
>     *   **Radio:** `lora_comm.cpp`, `transmit_queue.cpp`, `packet_handler.cpp`, `SX1262Radio.cpp`
>     *   **Routing:** `gradient_routing.cpp`, `neighbor_table.cpp`, `node_store.cpp`, `duplicate_cache.cpp`
>     *   **Timing:** `tdma_scheduler.cpp`, `network_time.cpp`, `neo6m.cpp`
>     *   **Sensors:** `sht30.cpp`, `bmp180.cpp`, `SensorManager.cpp` (if exists)
>     *   **UI:** `display_manager.cpp`, `OLED.cpp`, `web_dashboard.cpp`, `web_dashboard_lite.cpp`, `serial_output.cpp`, `serial_json.cpp`
>     *   **Utils:** `mesh_stats.cpp`, `mesh_commands.cpp`, `thingspeak.cpp`
> 3.  **Constraint:** Do NOT change `#include` paths yet (PlatformIO handles subdirs automatically)."

---

## Phase 1: Encapsulation & Global State Management

**Goal:** Eliminate "spaghetti code" caused by `main.cpp` accessing global variables directly. Move logic into dedicated Manager classes.

### Instruction Set 1: Create `SensorManager`
**Prompt / Task:**
> "Refactor the sensor logic currently in `main.cpp` into a new class called `SensorManager`.
> 1.  Create `src/managers/SensorManager.h` and `.cpp`.
> 2.  **Documentation:** Add Doxygen comments to `SensorManager.h`, fully explaining `begin()`, `update()`, and all getter methods.
> 3.  Move the global `SHT30`, `BMP180` objects, and variables like `sensor_tempF`, `sensor_humidity` into this class as private members.
> 4.  Expose public methods: `begin()`, `update()`, and getters like `getTemperatureF()`.
> 5.  Move the `readSensors()` function logic from `main.cpp` into `SensorManager::update()`.
> 6.  Update `main.cpp` to purely instantiate `SensorManager` and call `sensors.update()` in the loop."

### Instruction Set 2: Create `MeshContext` (Dependency Injection)
**Prompt / Task:**
> "Create a central `MeshContext` struct or class that holds references to all major system components (Radio, Scheduler, Routing, Display).
> 1.  Define a `MeshContext` struct in `src/MeshContext.h`.
> 2.  **Documentation:** Document each member pointer in `MeshContext` explaining the subsystem it represents and its lifecycle.
> 3.  Instead of using `extern` globals across files (like `tdmaScheduler` being accessed everywhere), pass a pointer to `MeshContext` to classes that need access to siblings.
> 4.  Refactor `Main` to initialize this context and pass it down.
> 5.  **Constraint:** Ensure no circular dependencies created."

---

---

## Phase 2: Secrets Management

**Goal:** Remove sensitive hardcoded strings from the codebase.

### Instruction Set 3: Extract Secrets
**Prompt / Task:**
> "Extract all sensitive hardcoded strings into a separate header that is git-ignored.
> 1.  Create `include/secrets.h`.
> 2.  **Documentation:** Add a warning comment block at the top of `secrets.h` enabling the user to understand this file should not be committed.
> 3.  Move `WIFI_AP_PASSWORD`, `THINGSPEAK_API_KEYS`, and `WIFI_STA_PASSWORD` into this file.
> 4.  Add `#include "secrets.h"` to `config.cpp`.
> 5.  Add `include/secrets.h` to `.gitignore`.
> 6.  Create a `include/secrets_example.h` with dummy values for other developers."

---

## Phase 3: Hardware Abstraction Layer (HAL)

**Goal:** Decouple the "Business Logic" (Mesh algorithms, TDMA) from the specific "Hardware Drivers" (SX1262, SSD1306).

### Instruction Set 5: Abstract the Radio Interface
**Prompt / Task:**
> "Create an interface `IMeshRadio` to decouple the mesh logic from the SX1262 library.
> 1.  Define an abstract base class `IMeshRadio` with pure virtual methods: `init()`, `sendPacket()`, `receivePacket()`, `setFrequency()`.
> 2.  **Documentation:** Add detailed Doxygen comments to `IMeshRadio.h` for each virtual method, defining the expected contract, parameter units (e.g., Hz, dBm), and return codes.
> 3.  Rename `LoRaComm` to `SX1262Radio` and make it implement `IMeshRadio`.
> 4.  Update `PacketHandler` and `TransmitQueue` to take a reference to `IMeshRadio` instead of calling `LoRaComm::xxx` directly.
> 5.  **Benefit:** This allows easily swapping to an SX1276 or simulated radio for testing."

---

## Phase 4: Professional Error Handling

**Goal:** Standardize how the system reports errors, warnings, and debug info.

### Instruction Set 6: Centralized System Logger
**Prompt / Task:**
> "Replace random `Serial.print` calls with a structured Logging system.
> 1.  Create `src/system/Logger.h`.
> 2.  **Documentation:** Document the boolean logic for log levels and the syntax of the `LOG` macro in the header file.
> 3.  Define log levels: `LOG_ERROR`, `LOG_WARN`, `LOG_INFO`, `LOG_DEBUG`.
> 4.  Create a macro `LOG(level, format, ...)` that prepends timestamp and level (e.g., `[12:00:01][INFO]`).
> 5.  (Optional) Add support for multiple 'Sinks': Serial, SD Card, or UDP (Web Dashboard).
> 6.  Refactor `main.cpp` and `packet_handler.cpp` to use `LOG_INFO(...)` instead of `Serial.println(...)`."

---

## Execution Strategy
**Recommended Order:**
1.  **Phase 4 (Logger):** Easiest to implement and immediately cleans up output.
2.  **Phase 1 (Encapsulation):** Cleans up `main.cpp` and organizes code.
3.  **Phase 2 (Config):** Adds significant user value (runtime changes).
4.  **Phase 3 (HAL):** Most complex, save for when hardware changes are actually needed.
