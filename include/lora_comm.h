#ifndef LORA_COMM_H
#define LORA_COMM_H

#include <RadioLib.h>
#include <Arduino.h>

// LoRa communication functions
bool initLoRa();
bool sendSensorData(float tempF, float pressureHPa, float altitudeM, String gpsData);
bool sendMessage(String message);
void setLoRaReceiveMode();
String receiveMessage();
bool isLoRaReady();

// LoRa status functions
float getLastRSSI();
float getLastSNR();

#endif
