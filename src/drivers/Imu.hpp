#pragma once

#include <Arduino.h>
#include <Wire.h>

struct ImuRawSample {
  int16_t ax = 0;
  int16_t ay = 0;
  int16_t az = 0;
  int16_t gx = 0;
  int16_t gy = 0;
  int16_t gz = 0;
};

class Mpu6050Imu {
 public:
  bool begin(TwoWire &wire, uint8_t address);
  bool readRaw(TwoWire &wire, ImuRawSample &sample);

  uint8_t address() const { return _address; }
  uint8_t whoAmI() const { return _whoAmI; }
  const char *lastError() const { return _lastError; }

 private:
  bool writeRegister(TwoWire &wire, uint8_t reg, uint8_t value);
  bool readRegister(TwoWire &wire, uint8_t reg, uint8_t &value);
  bool readRegisters(TwoWire &wire, uint8_t reg, uint8_t *buffer, size_t length);
  void setError(const char *message);

  uint8_t _address = 0x68;
  uint8_t _whoAmI = 0;
  char _lastError[40] = "not initialized";
};
