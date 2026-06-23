#include "Imu.hpp"

#include <cstring>

namespace {
constexpr uint8_t MPU6050_RA_SMPLRT_DIV = 0x19;
constexpr uint8_t MPU6050_RA_CONFIG = 0x1A;
constexpr uint8_t MPU6050_RA_GYRO_CONFIG = 0x1B;
constexpr uint8_t MPU6050_RA_ACCEL_CONFIG = 0x1C;
constexpr uint8_t MPU6050_RA_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t MPU6050_RA_PWR_MGMT_1 = 0x6B;
constexpr uint8_t MPU6050_RA_WHO_AM_I = 0x75;
constexpr uint8_t MPU6050_WHO_AM_I_EXPECTED = 0x68;
constexpr uint8_t MPU6050_WHO_AM_I_COMPATIBLE = 0x70;
}  // namespace

bool Mpu6050Imu::begin(TwoWire &wire, uint8_t address) {
  _address = address;
  _whoAmI = 0;

  if (!writeRegister(wire, MPU6050_RA_PWR_MGMT_1, 0x01)) {
    return false;
  }
  delay(100);
  if (!writeRegister(wire, MPU6050_RA_SMPLRT_DIV, 0x04)) {
    return false;
  }
  if (!writeRegister(wire, MPU6050_RA_CONFIG, 0x03)) {
    return false;
  }
  if (!writeRegister(wire, MPU6050_RA_GYRO_CONFIG, 0x00)) {
    return false;
  }
  if (!writeRegister(wire, MPU6050_RA_ACCEL_CONFIG, 0x00)) {
    return false;
  }
  if (!readRegister(wire, MPU6050_RA_WHO_AM_I, _whoAmI)) {
    return false;
  }

  if (_whoAmI != MPU6050_WHO_AM_I_EXPECTED && _whoAmI != MPU6050_WHO_AM_I_COMPATIBLE) {
    setError("unexpected WHO_AM_I");
    return false;
  }

  setError("ok");
  return true;
}

bool Mpu6050Imu::readRaw(TwoWire &wire, ImuRawSample &sample) {
  uint8_t buffer[14] = {};
  if (!readRegisters(wire, MPU6050_RA_ACCEL_XOUT_H, buffer, sizeof(buffer))) {
    return false;
  }

  sample.ax = static_cast<int16_t>((buffer[0] << 8) | buffer[1]);
  sample.ay = static_cast<int16_t>((buffer[2] << 8) | buffer[3]);
  sample.az = static_cast<int16_t>((buffer[4] << 8) | buffer[5]);
  sample.gx = static_cast<int16_t>((buffer[8] << 8) | buffer[9]);
  sample.gy = static_cast<int16_t>((buffer[10] << 8) | buffer[11]);
  sample.gz = static_cast<int16_t>((buffer[12] << 8) | buffer[13]);
  setError("ok");
  return true;
}

bool Mpu6050Imu::writeRegister(TwoWire &wire, uint8_t reg, uint8_t value) {
  wire.beginTransmission(_address);
  wire.write(reg);
  wire.write(value);
  if (wire.endTransmission() != 0) {
    setError("i2c write failed");
    return false;
  }
  return true;
}

bool Mpu6050Imu::readRegister(TwoWire &wire, uint8_t reg, uint8_t &value) {
  return readRegisters(wire, reg, &value, 1);
}

bool Mpu6050Imu::readRegisters(TwoWire &wire, uint8_t reg, uint8_t *buffer, size_t length) {
  wire.beginTransmission(_address);
  wire.write(reg);
  if (wire.endTransmission(false) != 0) {
    setError("i2c register select failed");
    return false;
  }

  const size_t received = wire.requestFrom(static_cast<int>(_address), static_cast<int>(length));
  if (received != length) {
    setError("i2c read length mismatch");
    return false;
  }

  for (size_t i = 0; i < length; i++) {
    buffer[i] = wire.read();
  }
  return true;
}

void Mpu6050Imu::setError(const char *message) {
  std::strncpy(_lastError, message, sizeof(_lastError) - 1);
  _lastError[sizeof(_lastError) - 1] = '\0';
}
