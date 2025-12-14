#include "sht30.h"

SHT30::SHT30() {
  _temperature = 0.0;
  _humidity = 0.0;
}

bool SHT30::begin(TwoWire *wire) {
  _wire = wire;
  
  // Check if sensor responds
  _wire->beginTransmission(0x44);
  return (_wire->endTransmission() == 0);
}

bool SHT30::read() {
  // Send measurement command
  _wire->beginTransmission(0x44);
  _wire->write(0x2C);
  _wire->write(0x06);
  if (_wire->endTransmission() != 0) return false;
  
  delay(20); // Wait for measurement
  
  // Read 6 bytes
  if (_wire->requestFrom(0x44, 6) != 6) return false;
  
  uint8_t data[6];
  for (int i = 0; i < 6; i++) {
    data[i] = _wire->read();
  }
  
  // Convert temperature
  uint16_t temp_raw = (data[0] << 8) | data[1];
  _temperature = -45.0 + 175.0 * (temp_raw / 65535.0);
  
  // Convert humidity
  uint16_t hum_raw = (data[3] << 8) | data[4];
  _humidity = 100.0 * (hum_raw / 65535.0);
  
  return true;
}

float SHT30::getTemperature() {
  return _temperature;
}

float SHT30::getHumidity() {
  return _humidity;
}