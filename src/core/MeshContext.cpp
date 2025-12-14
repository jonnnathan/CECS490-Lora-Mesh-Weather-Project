/**
 * @file MeshContext.cpp
 * @brief Global MeshContext instance definition.
 *
 * @see MeshContext.h for detailed documentation.
 */

#include "MeshContext.h"

/**
 * @brief Global mesh context instance.
 *
 * Initialized with null pointers. Must be populated during setup()
 * in main.cpp before any modules attempt to use it.
 */
MeshContext g_meshContext;
