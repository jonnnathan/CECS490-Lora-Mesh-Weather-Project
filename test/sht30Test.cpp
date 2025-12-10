/*
#include <Arduino.h>
#include <Wire.h>

// SHT30 I2C address is 0x44(68)
#define Addr 0x44

void setup()
{
  // Initialise I2C communication as MASTER with custom pins
  // Wire.begin(SDA_pin, SCL_pin) - SDA=20, SCL=19
  Wire.begin(7, 20);
  
  // Initialise serial communication, set baud rate = 9600
  Serial.begin(115200);
  
  Serial.println("SHT30 Temperature & Humidity Sensor");
  Serial.println("Using custom I2C pins: SDA=20, SCL=19");
  Serial.println("Sensor address: 0x44");
  Serial.println("-----------------------------------");
  
  delay(300);
}

void loop()
{
  unsigned int data[6];
  
  // Start I2C Transmission
  Wire.beginTransmission(Addr);
  // Send measurement command
  Wire.write(0x2C);
  Wire.write(0x06);
  // Stop I2C transmission
  Wire.endTransmission();
  delay(500);
  
  // Request 6 bytes of data
  Wire.requestFrom(Addr, 6);
  
  // Read 6 bytes of data
  // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
  if (Wire.available() == 6)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
    data[2] = Wire.read();
    data[3] = Wire.read();
    data[4] = Wire.read();
    data[5] = Wire.read();
    
    // Convert the data
    float cTemp = ((((data[0] * 256.0) + data[1]) * 175) / 65535.0) - 45;
    float fTemp = (cTemp * 1.8) + 32;
    float humidity = ((((data[3] * 256.0) + data[4]) * 100) / 65535.0);
    
    // Output data to serial monitor
    Serial.print("Relative Humidity : ");
    Serial.print(humidity);
    Serial.println(" %RH");
    Serial.print("Temperature in Celsius : ");
    Serial.print(cTemp);
    Serial.println(" C");
    Serial.print("Temperature in Fahrenheit : ");
    Serial.print(fTemp);
    Serial.println(" F");
    Serial.println("-----------------------------------");
  }
  else
  {
    Serial.println("Error: Could not read data from SHT30 sensor");
    Serial.println("Check connections and I2C address (0x44)");
    Serial.println("-----------------------------------");
  }
  
  delay(2000); // Read every 2 seconds
}
*/