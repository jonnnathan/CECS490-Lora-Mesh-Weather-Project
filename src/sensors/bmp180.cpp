#include "bmp180.h"
#include <Wire.h>
#include <math.h>

BMP180::BMP180() : _wire(nullptr), _oss(BMP180_OSS_ULTRAHIGHRES) {}

bool BMP180::begin(TwoWire* wire, uint8_t oss) {
    _wire = wire;
    _oss  = oss;

    if (!isConnected()) {
        return false;
    }
    return readCalibrationData();
}


bool BMP180::isConnected() {
    if (_wire == nullptr) return false;
    _wire->beginTransmission(BMP180_ADDR);
    return (_wire->endTransmission() == 0);
}

bool BMP180::readCalibrationData() {
    // Read all calibration coefficients as per datasheet
    ac1 = (int16_t)readRegister16(BMP180_REG_CAL_AC1);
    ac2 = (int16_t)readRegister16(BMP180_REG_CAL_AC2);
    ac3 = (int16_t)readRegister16(BMP180_REG_CAL_AC3);
    ac4 =              readRegister16(BMP180_REG_CAL_AC4);
    ac5 =              readRegister16(BMP180_REG_CAL_AC5);
    ac6 =              readRegister16(BMP180_REG_CAL_AC6);
    b1  = (int16_t)readRegister16(BMP180_REG_CAL_B1);
    b2  = (int16_t)readRegister16(BMP180_REG_CAL_B2);
    mb  = (int16_t)readRegister16(BMP180_REG_CAL_MB);
    mc  = (int16_t)readRegister16(BMP180_REG_CAL_MC);
    md  = (int16_t)readRegister16(BMP180_REG_CAL_MD);

    // Basic sanity checks (protect against all-zeros or 0xFFFF on unsigned fields)
    if (ac1 == 0 || ac1 == -1) return false;
    if (ac4 == 0 || ac4 == 0xFFFF) return false;
    if (ac5 == 0 || ac5 == 0xFFFF) return false;

    return true;
}

void BMP180::writeRegister(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(BMP180_ADDR);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

uint8_t BMP180::readRegister8(uint8_t reg) {
    _wire->beginTransmission(BMP180_ADDR);
    _wire->write(reg);
    _wire->endTransmission();
    _wire->requestFrom(BMP180_ADDR, (uint8_t)1);
    if (_wire->available() < 1) return 0;
    return _wire->read();
}

uint16_t BMP180::readRegister16(uint8_t reg) {
    _wire->beginTransmission(BMP180_ADDR);
    _wire->write(reg);
    _wire->endTransmission();
    _wire->requestFrom(BMP180_ADDR, (uint8_t)2);
    if (_wire->available() < 2) return 0;
    uint16_t hi = _wire->read();
    uint16_t lo = _wire->read();
    return (hi << 8) | lo;
}

int32_t BMP180::readRawTemperature() {
    // Start temperature conversion
    writeRegister(BMP180_REG_CONTROL, BMP180_CMD_TEMP);
    // Wait 4.5 ms minimum
    delay(5);
    // Read uncompensated temperature
    return (int32_t)readRegister16(BMP180_REG_RESULT);
}

int32_t BMP180::readRawPressure() {
    uint8_t cmd;
    uint8_t delay_time;

    switch (_oss) {
        case BMP180_OSS_ULTRALOWPOWER: cmd = BMP180_CMD_PRESS0; delay_time = 5;  break;
        case BMP180_OSS_STANDARD:      cmd = BMP180_CMD_PRESS1; delay_time = 8;  break;
        case BMP180_OSS_HIGHRES:       cmd = BMP180_CMD_PRESS2; delay_time = 14; break;
        case BMP180_OSS_ULTRAHIGHRES:
        default:                       cmd = BMP180_CMD_PRESS3; delay_time = 26; break;
    }

    // Start pressure conversion
    writeRegister(BMP180_REG_CONTROL, cmd);
    delay(delay_time);

    // Read 3 bytes (MSB, LSB, XLSB)
    _wire->beginTransmission(BMP180_ADDR);
    _wire->write(BMP180_REG_RESULT);
    _wire->endTransmission();
    _wire->requestFrom(BMP180_ADDR, (uint8_t)3);
    if (_wire->available() < 3) return 0;

    uint32_t raw = 0;
    raw  = (uint32_t)_wire->read(); // MSB
    raw <<= 8;
    raw |= (uint32_t)_wire->read(); // LSB
    raw <<= 8;
    raw |= (uint32_t)_wire->read(); // XLSB

    raw >>= (8 - _oss);
    return (int32_t)raw;
}

int32_t BMP180::computeB5(int32_t UT) {
    // B5 = X1 + X2, where:
    // X1 = (UT - AC6) * AC5 / 2^15
    // X2 = MC * 2^11 / (X1 + MD)
    int32_t X1 = ((UT - (int32_t)ac6) * (int32_t)ac5) >> 15;
    int32_t X2 = ((int32_t)mc << 11) / (X1 + (int32_t)md);
    return X1 + X2;
}

float BMP180::readTemperature() {
    int32_t UT = readRawTemperature();
    int32_t B5 = computeB5(UT);
    // T in 0.1 °C: T = (B5 + 8) / 2^4
    int32_t T = (B5 + 8) >> 4;
    return T / 10.0f;
}

float BMP180::readPressure() {
    int32_t UT = readRawTemperature();
    int32_t UP = readRawPressure();
    // True pressure calculation (datasheet)
    int32_t B5 = computeB5(UT);
    int32_t B6 = B5 - 4000;
    int32_t X1 = ((int32_t)b2 * ((B6 * B6) >> 12)) >> 11;
    int32_t X2 = ((int32_t)ac2 * B6) >> 11;
    int32_t X3 = X1 + X2;
    // Correct rounding per datasheet:
    // B3 = (((AC1*4 + X3) << OSS) + 2) / 4
    int32_t B3 = (((((int32_t)ac1) * 4 + X3) << _oss) + 2) >> 2;
    X1 = ((int32_t)ac3 * B6) >> 13;
    X2 = ((int32_t)b1  * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    uint32_t B4 = ((uint32_t)ac4 * (uint32_t)(X3 + 32768)) >> 15;
    uint32_t B7 = ((uint32_t)UP - (uint32_t)B3) * (uint32_t)(50000UL >> _oss);
    int32_t p;
    if (B7 < 0x80000000UL) {
        // p = (B7 * 2) / B4
        p = (int32_t)((B7 << 1) / B4);
    } else {
        // p = (B7 / B4) * 2
        p = (int32_t)((B7 / B4) << 1);
    }
    X1 = (p >> 8);
    X1 = (X1 * X1);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;
    p = p + ((X1 + X2 + (int32_t)3791) >> 4);
    return (float)p; // Pascals
}

float BMP180::readAltitude(float seaLevelPressure /* Pa */) {
    // Protect against bad input
    if (seaLevelPressure <= 0.0f) seaLevelPressure = 101325.0f;
    const float pressure = readPressure(); // Pa
    // Standard barometric formula: 0.190294957 ≈ 1/5.255
    return 44330.0f * (1.0f - powf(pressure / seaLevelPressure, 0.190294957f));
}

int32_t BMP180::getRawTemperature() {
    return readRawTemperature();
}

int32_t BMP180::getRawPressure() {
    return readRawPressure();
}



