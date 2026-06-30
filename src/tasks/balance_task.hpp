#pragma once

#include <Arduino.h>

#include "../drivers/Imu.hpp"

struct BalanceControlConfig {
  BalanceControlConfig() = default;
  BalanceControlConfig(float setpoint, float kpValue, float kiValue, float kdValue, float limit, float maxAngle, float sign,
                       bool speedMix, float speedScale)
      : setpointDeg(setpoint),
        kp(kpValue),
        ki(kiValue),
        kd(kdValue),
        outputLimit(limit),
        maxAbsAngleDeg(maxAngle),
        motorSign(sign),
        speedMixEnabled(speedMix),
        speedMixScale(speedScale) {}

  float setpointDeg = 0.0f;
  float kp = 0.0f;
  float ki = 0.0f;
  float kd = 0.0f;
  float outputLimit = 0.0f;
  float maxAbsAngleDeg = 0.0f;
  float motorSign = 1.0f;
  bool speedMixEnabled = false;
  float speedMixScale = 0.0f;
};

struct BalanceTelemetry {
  bool imuReady = false;
  bool lastReadOk = false;
  bool calibrated = false;
  bool calibrationInProgress = false;
  bool balanceControllerEnabled = false;
  bool balanceControlSafetyOk = false;
  bool balanceMotorOutputEnabled = false;
  bool balanceMotorOutputAvailable = false;
  bool balanceMotorOutputArmed = false;
  bool balanceDriveCommandSent = false;
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
  float accelTiltXDeg = 0.0f;
  bool axisFilterReady = false;
  float accelAngleAyAzSmoothedDeg = 0.0f;
  float accelAngleAxAzSmoothedDeg = 0.0f;
  float accelAngleAxAySmoothedDeg = 0.0f;
  float accelTiltXSmoothedDeg = 0.0f;
  float pitchDeg = 0.0f;
  float gyroRateDps = 0.0f;
  float gyroXRateDps = 0.0f;
  float gyroYRateDps = 0.0f;
  float gyroZRateDps = 0.0f;
  float gyroBiasRaw = 0.0f;
  float balanceSetpointDeg = 0.0f;
  float balanceKp = 0.0f;
  float balanceKi = 0.0f;
  float balanceKd = 0.0f;
  float balanceOutputLimit = 0.0f;
  float balanceMaxAbsAngleDeg = 0.0f;
  float balanceMotorSign = 1.0f;
  float balanceAngleErrorDeg = 0.0f;
  float balanceIntegralError = 0.0f;
  float balancePTerm = 0.0f;
  float balanceITerm = 0.0f;
  float balanceDTerm = 0.0f;
  float balanceOutputRaw = 0.0f;
  float balanceOutputClamped = 0.0f;
  float balanceMixedOutputRaw = 0.0f;
  float balanceMixedOutputClamped = 0.0f;
  int balanceLeftPwm = 0;
  int balanceRightPwm = 0;
  bool speedLoopReady = false;
  unsigned long encoderTotalLeft = 0;
  unsigned long encoderTotalRight = 0;
  long speedLoopDeltaLeft = 0;
  long speedLoopDeltaRight = 0;
  float speedLoopSignedCommand = 0.0f;
  int speedLoopDirectionSign = 0;
  bool speedLoopMixEnabled = false;
  float speedLoopMixScale = 0.0f;
  float speedLoopCarSpeed = 0.0f;
  float speedLoopFilter = 0.0f;
  float speedLoopIntegral = 0.0f;
  float speedLoopOutput = 0.0f;
  uint32_t speedLoopSampleCount = 0;
  float loopDtMs = 0.0f;
  ImuRawSample raw;
  char lastError[48] = "not started";
  char balanceSafetyReason[48] = "not evaluated";
};

void balance_task_start();
bool balance_request_calibration();
void balance_get_status(BalanceTelemetry &out);
void balance_get_raw(BalanceTelemetry &out);
void balance_get_config(BalanceControlConfig &out);
void balance_set_config(const BalanceControlConfig &config);
bool balance_request_motor_arm(char *reason, size_t reasonSize);
void balance_request_motor_disarm();
