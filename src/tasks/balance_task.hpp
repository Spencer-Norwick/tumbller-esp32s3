#pragma once

#include <Arduino.h>

#include "../drivers/Imu.hpp"

struct BalanceTelemetry {
  bool imuReady = false;
  bool lastReadOk = false;
  bool calibrated = false;
  bool calibrationInProgress = false;
  bool balanceMotorOutputEnabled = false;
  uint8_t imuAddress = 0;
  uint8_t whoAmI = 0;
  bool whoAmICompatible = false;
  uint32_t loopCount = 0;
  uint32_t failedReadCount = 0;
  unsigned long updatedAtMs = 0;
  float accelPitchDeg = 0.0f;
  float accelAngleAyAzDeg = 0.0f;
  float accelAngleAxAzDeg = 0.0f;
  float accelAngleAxAyDeg = 0.0f;
  bool axisFilterReady = false;
  float accelAngleAyAzSmoothedDeg = 0.0f;
  float accelAngleAxAzSmoothedDeg = 0.0f;
  float accelAngleAxAySmoothedDeg = 0.0f;
  float pitchDeg = 0.0f;
  float gyroRateDps = 0.0f;
  float gyroXRateDps = 0.0f;
  float gyroYRateDps = 0.0f;
  float gyroZRateDps = 0.0f;
  float gyroBiasRaw = 0.0f;
  float loopDtMs = 0.0f;
  ImuRawSample raw;
  char lastError[48] = "not started";
};

void balance_task_start();
bool balance_request_calibration();
void balance_get_status(BalanceTelemetry &out);
void balance_get_raw(BalanceTelemetry &out);
