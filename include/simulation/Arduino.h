/**
 * @file Arduino.h
 * @brief Redirect to Arduino_stub.h for native simulation builds.
 *
 * This file is placed in include/simulation/ and gets picked up first
 * due to the -I include/simulation flag in platformio.ini for native builds.
 * It redirects all #include <Arduino.h> to our stub implementation.
 */

#ifndef ARDUINO_REDIRECT_H
#define ARDUINO_REDIRECT_H

#include "Arduino_stub.h"

#endif // ARDUINO_REDIRECT_H
