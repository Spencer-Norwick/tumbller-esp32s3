#include "balance_task.hpp"

#include <Wire.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../config.hpp"
#include "../drivers/KalmanPitch.hpp"
#include "motor_task.hpp"
#include "task_common.hpp"

namespace {
constexpr float AXIS_CANDIDATE_FILTER_ALPHA = 0.10f;
Mpu6050Imu g_imu;
KalmanPitch g_pitchFilter;
BalanceTelemetry g_telemetry;
BalanceControlConfig g_balanceConfig = {
    BALANCE_ANGLE_SETPOINT_DEG,
    BALANCE_PID_KP,
    BALANCE_PID_KI,
    BALANCE_PID_KD,
    BALANCE_PID_OUTPUT_LIMIT,
    BALANCE_CONTROL_MAX_ABS_ANGLE_DEG,
    BALANCE_MOTOR_SIGN,
    BALANCE_SPEED_MIX_ENABLED != 0,
    BALANCE_SPEED_MIX_SCALE,
};
portMUX_TYPE g_telemetryMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE g_configMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE g_armMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool g_calibrationRequested = false;
bool g_balanceMotorArmed = false;
unsigned long g_speedLoopLastEncoderLeft = 0;
unsigned long g_speedLoopLastEncoderRight = 0;
uint8_t g_speedLoopPeriodCount = 0;

void balanceTask(void *pvParameters);
void publishTelemetry(const BalanceTelemetry &telemetry);
void copyTelemetry(BalanceTelemetry &out);
void copyConfig(BalanceControlConfig &out);
bool copyArmState();
void setArmState(bool armed);
void setError(BalanceTelemetry &telemetry, const char *message);
void setBalanceSafetyReason(BalanceTelemetry &telemetry, const char *message);
void copyReason(char *reason, size_t reasonSize, const char *message);
bool readImuLocked(ImuRawSample &sample, const char *&error);
bool runPitchGyroCalibration(float &gyroBiasRaw, BalanceTelemetry &telemetry);
float smoothAngleDeg(float previousDeg, float nextDeg, float alpha);
void computeAxisCandidates(const ImuRawSample &sample, BalanceTelemetry &telemetry);
void computeBalanceControl(BalanceTelemetry &telemetry, float dtSeconds);
void computeSpeedLoopPreview(BalanceTelemetry &telemetry);
void computeSpeedMixedOutput(BalanceTelemetry &telemetry);
void applyBalanceMotorOutput(BalanceTelemetry &telemetry);
void applyConfigToTelemetry(BalanceTelemetry &telemetry, const BalanceControlConfig &config);
float clampFloat(float value, float minValue, float maxValue);
int clampPwm(float value);
void queueMotorStop();
void emitSerialTelemetry(const BalanceTelemetry &telemetry);
}  // namespace

void balance_task_start() {
  xTaskCreatePinnedToCore(balanceTask, "balanceTask", 4096, nullptr, 1, nullptr, 1);
}

bool balance_request_calibration() {
  g_calibrationRequested = true;
  return true;
}

void balance_get_status(BalanceTelemetry &out) {
  copyTelemetry(out);
}

void balance_get_raw(BalanceTelemetry &out) {
  copyTelemetry(out);
}

void balance_get_config(BalanceControlConfig &out) {
  copyConfig(out);
}

void balance_set_config(const BalanceControlConfig &config) {
  taskENTER_CRITICAL(&g_configMux);
  g_balanceConfig = config;
  taskEXIT_CRITICAL(&g_configMux);
}

bool balance_request_motor_arm(char *reason, size_t reasonSize) {
#if BALANCE_MOTOR_OUTPUT_AVAILABLE == 0
  copyReason(reason, reasonSize, "motor output unavailable");
  return false;
#else
  BalanceTelemetry telemetry;
  BalanceControlConfig config;
  copyTelemetry(telemetry);
  copyConfig(config);

  if (!telemetry.lastReadOk) {
    copyReason(reason, reasonSize, "sensor read failed");
    return false;
  }
  if (!telemetry.calibrated) {
    copyReason(reason, reasonSize, "not calibrated");
    return false;
  }
  if (fabsf(telemetry.pitchDeg) > config.maxAbsAngleDeg) {
    copyReason(reason, reasonSize, "angle outside safe window");
    return false;
  }
  if (config.outputLimit > BALANCE_ARM_MAX_OUTPUT_LIMIT) {
    copyReason(reason, reasonSize, "output limit too high for arm");
    return false;
  }

  setArmState(true);
  copyReason(reason, reasonSize, "armed");
  return true;
#endif
}

void balance_request_motor_disarm() {
  setArmState(false);
  queueMotorStop();
}

namespace {
void balanceTask(void *pvParameters) {
  (void)pvParameters;

  BalanceTelemetry telemetry;
  telemetry.imuAddress = IMU_I2C_ADDR;
  BalanceControlConfig config;
  copyConfig(config);
  applyConfigToTelemetry(telemetry, config);
  setBalanceSafetyReason(telemetry, "not calibrated");

  if (i2c_lock(pdMS_TO_TICKS(500))) {
    telemetry.imuReady = g_imu.begin(Wire, IMU_I2C_ADDR);
    telemetry.whoAmI = g_imu.whoAmI();
    telemetry.whoAmICompatible = g_imu.whoAmICompatible();
    setError(telemetry, g_imu.lastError());
    i2c_unlock();
  } else {
    telemetry.imuReady = false;
    setError(telemetry, "i2c lock timeout");
  }
  publishTelemetry(telemetry);

  unsigned long lastMicros = micros();
  for (;;) {
    if (g_calibrationRequested) {
      g_calibrationRequested = false;
      telemetry.calibrationInProgress = true;
      publishTelemetry(telemetry);
      float gyroBiasRaw = telemetry.gyroBiasRaw;
      telemetry.calibrated = runPitchGyroCalibration(gyroBiasRaw, telemetry);
      telemetry.gyroBiasRaw = gyroBiasRaw;
      telemetry.calibrationInProgress = false;
      g_pitchFilter.reset(telemetry.accelPitchDeg);
      telemetry.balanceIntegralError = 0.0f;
      telemetry.speedLoopFilter = 0.0f;
      telemetry.speedLoopIntegral = 0.0f;
      telemetry.speedLoopOutput = 0.0f;
      telemetry.speedLoopSignedCommand = 0.0f;
      telemetry.speedLoopDirectionSign = 0;
      telemetry.speedLoopSampleCount = 0;
      telemetry.speedLoopReady = false;
      setArmState(false);
      queueMotorStop();
      publishTelemetry(telemetry);
    }

    const unsigned long nowMicros = micros();
    float dtSeconds = (nowMicros - lastMicros) / 1000000.0f;
    lastMicros = nowMicros;
    if (dtSeconds <= 0.0f || dtSeconds > 0.1f) {
      dtSeconds = BALANCE_SENSOR_LOOP_MS / 1000.0f;
    }

    ImuRawSample sample;
    const char *readError = nullptr;
    const bool readOk = telemetry.imuReady && readImuLocked(sample, readError);
    telemetry.lastReadOk = readOk;
    telemetry.updatedAtMs = millis();
    telemetry.loopDtMs = dtSeconds * 1000.0f;
    telemetry.loopCount++;

    if (readOk) {
      telemetry.raw = sample;
      computeAxisCandidates(sample, telemetry);
      const float gyroXRateDps = static_cast<float>(sample.gx) / 131.0f;
      const float gyroRateDps = (sample.gy - telemetry.gyroBiasRaw) / 131.0f;
      telemetry.gyroRateDps = gyroRateDps;
      telemetry.gyroXRateDps = gyroXRateDps;
      telemetry.gyroYRateDps = gyroRateDps;
      telemetry.gyroZRateDps = static_cast<float>(sample.gz) / 131.0f;
      g_pitchFilter.update(telemetry.accelPitchDeg, gyroRateDps, dtSeconds);
      telemetry.pitchDeg = g_pitchFilter.angleDeg();
      computeBalanceControl(telemetry, dtSeconds);
      computeSpeedLoopPreview(telemetry);
      computeSpeedMixedOutput(telemetry);
      setError(telemetry, "ok");
    } else {
      telemetry.failedReadCount++;
      computeBalanceControl(telemetry, dtSeconds);
      computeSpeedLoopPreview(telemetry);
      computeSpeedMixedOutput(telemetry);
      setError(telemetry, telemetry.imuReady ? readError : "imu not ready");
    }

    applyBalanceMotorOutput(telemetry);
    publishTelemetry(telemetry);
    emitSerialTelemetry(telemetry);
    vTaskDelay(pdMS_TO_TICKS(BALANCE_SENSOR_LOOP_MS));
  }
}

void publishTelemetry(const BalanceTelemetry &telemetry) {
  taskENTER_CRITICAL(&g_telemetryMux);
  g_telemetry = telemetry;
  taskEXIT_CRITICAL(&g_telemetryMux);
}

void copyTelemetry(BalanceTelemetry &out) {
  taskENTER_CRITICAL(&g_telemetryMux);
  out = g_telemetry;
  taskEXIT_CRITICAL(&g_telemetryMux);
}

void copyConfig(BalanceControlConfig &out) {
  taskENTER_CRITICAL(&g_configMux);
  out = g_balanceConfig;
  taskEXIT_CRITICAL(&g_configMux);
}

bool copyArmState() {
  taskENTER_CRITICAL(&g_armMux);
  const bool armed = g_balanceMotorArmed;
  taskEXIT_CRITICAL(&g_armMux);
  return armed;
}

void setArmState(bool armed) {
  taskENTER_CRITICAL(&g_armMux);
  g_balanceMotorArmed = armed;
  taskEXIT_CRITICAL(&g_armMux);
}

void setError(BalanceTelemetry &telemetry, const char *message) {
  std::strncpy(telemetry.lastError, message, sizeof(telemetry.lastError) - 1);
  telemetry.lastError[sizeof(telemetry.lastError) - 1] = '\0';
}

void setBalanceSafetyReason(BalanceTelemetry &telemetry, const char *message) {
  std::strncpy(telemetry.balanceSafetyReason, message, sizeof(telemetry.balanceSafetyReason) - 1);
  telemetry.balanceSafetyReason[sizeof(telemetry.balanceSafetyReason) - 1] = '\0';
}

void copyReason(char *reason, size_t reasonSize, const char *message) {
  if (!reason || reasonSize == 0) {
    return;
  }
  std::strncpy(reason, message, reasonSize - 1);
  reason[reasonSize - 1] = '\0';
}

bool readImuLocked(ImuRawSample &sample, const char *&error) {
  if (!i2c_lock(pdMS_TO_TICKS(20))) {
    error = "i2c lock timeout";
    return false;
  }
  const bool ok = g_imu.readRaw(Wire, sample);
  error = g_imu.lastError();
  i2c_unlock();
  return ok;
}

bool runPitchGyroCalibration(float &gyroBiasRaw, BalanceTelemetry &telemetry) {
  int64_t pitchGyroSum = 0;
  int samplesRead = 0;

  for (int i = 0; i < BALANCE_GYRO_CALIBRATION_SAMPLES; i++) {
    ImuRawSample sample;
    const char *readError = nullptr;
    if (!readImuLocked(sample, readError)) {
      setError(telemetry, readError);
      return false;
    }
    pitchGyroSum += sample.gy;
    telemetry.raw = sample;
    computeAxisCandidates(sample, telemetry);
    samplesRead++;
    vTaskDelay(pdMS_TO_TICKS(2));
  }

  if (samplesRead == 0) {
    setError(telemetry, "no calibration samples");
    return false;
  }

  gyroBiasRaw = static_cast<float>(pitchGyroSum) / static_cast<float>(samplesRead);
  setError(telemetry, "calibrated");
  return true;
}

void computeAxisCandidates(const ImuRawSample &sample, BalanceTelemetry &telemetry) {
  const float ax = static_cast<float>(sample.ax);
  const float ay = static_cast<float>(sample.ay);
  const float az = static_cast<float>(sample.az);
  telemetry.accelAngleAyAzDeg = atan2f(ay, az) * 57.2957795f;
  telemetry.accelAngleAxAzDeg = atan2f(ax, az) * 57.2957795f;
  telemetry.accelAngleAxAyDeg = atan2f(ax, ay) * 57.2957795f;
  telemetry.accelTiltXDeg = atan2f(ax, sqrtf((ay * ay) + (az * az))) * 57.2957795f;
  if (!telemetry.axisFilterReady) {
    telemetry.accelAngleAyAzSmoothedDeg = telemetry.accelAngleAyAzDeg;
    telemetry.accelAngleAxAzSmoothedDeg = telemetry.accelAngleAxAzDeg;
    telemetry.accelAngleAxAySmoothedDeg = telemetry.accelAngleAxAyDeg;
    telemetry.accelTiltXSmoothedDeg = telemetry.accelTiltXDeg;
    telemetry.axisFilterReady = true;
  } else {
    telemetry.accelAngleAyAzSmoothedDeg =
        smoothAngleDeg(telemetry.accelAngleAyAzSmoothedDeg, telemetry.accelAngleAyAzDeg, AXIS_CANDIDATE_FILTER_ALPHA);
    telemetry.accelAngleAxAzSmoothedDeg =
        smoothAngleDeg(telemetry.accelAngleAxAzSmoothedDeg, telemetry.accelAngleAxAzDeg, AXIS_CANDIDATE_FILTER_ALPHA);
    telemetry.accelAngleAxAySmoothedDeg =
        smoothAngleDeg(telemetry.accelAngleAxAySmoothedDeg, telemetry.accelAngleAxAyDeg, AXIS_CANDIDATE_FILTER_ALPHA);
    telemetry.accelTiltXSmoothedDeg =
        smoothAngleDeg(telemetry.accelTiltXSmoothedDeg, telemetry.accelTiltXDeg, AXIS_CANDIDATE_FILTER_ALPHA);
  }
  telemetry.accelPitchDeg = telemetry.accelTiltXDeg;
}

float smoothAngleDeg(float previousDeg, float nextDeg, float alpha) {
  float delta = nextDeg - previousDeg;
  while (delta > 180.0f) {
    delta -= 360.0f;
  }
  while (delta < -180.0f) {
    delta += 360.0f;
  }

  float smoothed = previousDeg + alpha * delta;
  while (smoothed > 180.0f) {
    smoothed -= 360.0f;
  }
  while (smoothed <= -180.0f) {
    smoothed += 360.0f;
  }
  return smoothed;
}

void computeBalanceControl(BalanceTelemetry &telemetry, float dtSeconds) {
  BalanceControlConfig config;
  copyConfig(config);
  applyConfigToTelemetry(telemetry, config);
  telemetry.balanceAngleErrorDeg = telemetry.pitchDeg - telemetry.balanceSetpointDeg;

  telemetry.balanceControlSafetyOk = false;
  if (!telemetry.balanceControllerEnabled) {
    telemetry.balanceIntegralError = 0.0f;
    setBalanceSafetyReason(telemetry, "controller disabled");
  } else if (!telemetry.lastReadOk) {
    telemetry.balanceIntegralError = 0.0f;
    setBalanceSafetyReason(telemetry, "sensor read failed");
  } else if (!telemetry.calibrated) {
    telemetry.balanceIntegralError = 0.0f;
    setBalanceSafetyReason(telemetry, "not calibrated");
  } else if (fabsf(telemetry.pitchDeg) > telemetry.balanceMaxAbsAngleDeg) {
    telemetry.balanceIntegralError = 0.0f;
    setBalanceSafetyReason(telemetry, "angle outside safe window");
  } else {
    telemetry.balanceControlSafetyOk = true;
    setBalanceSafetyReason(telemetry, "ok");
    if (telemetry.balanceKi != 0.0f) {
      telemetry.balanceIntegralError += telemetry.balanceAngleErrorDeg * dtSeconds;
    } else {
      telemetry.balanceIntegralError = 0.0f;
    }
  }

  telemetry.balancePTerm = telemetry.balanceKp * telemetry.balanceAngleErrorDeg;
  telemetry.balanceITerm = telemetry.balanceKi * telemetry.balanceIntegralError;
  telemetry.balanceDTerm = telemetry.balanceKd * telemetry.gyroRateDps;
  telemetry.balanceOutputRaw = telemetry.balancePTerm + telemetry.balanceITerm + telemetry.balanceDTerm;
  telemetry.balanceOutputClamped =
      clampFloat(telemetry.balanceOutputRaw, -telemetry.balanceOutputLimit, telemetry.balanceOutputLimit);

  if (!telemetry.balanceControlSafetyOk) {
    telemetry.balanceOutputClamped = 0.0f;
  }

  telemetry.balanceMixedOutputRaw = telemetry.balanceOutputClamped;
  telemetry.balanceMixedOutputClamped = telemetry.balanceOutputClamped;
  telemetry.balanceLeftPwm = static_cast<int>(telemetry.balanceMixedOutputClamped);
  telemetry.balanceRightPwm = static_cast<int>(telemetry.balanceMixedOutputClamped);
}

void computeSpeedLoopPreview(BalanceTelemetry &telemetry) {
  EncoderTelemetry encoder;
  motor_get_encoder_status(encoder);
  telemetry.encoderTotalLeft = encoder.totalLeft;
  telemetry.encoderTotalRight = encoder.totalRight;

  if (!encoder.initialized) {
    telemetry.speedLoopReady = false;
    telemetry.speedLoopDeltaLeft = 0;
    telemetry.speedLoopDeltaRight = 0;
    telemetry.speedLoopSignedCommand = 0.0f;
    telemetry.speedLoopDirectionSign = 0;
    telemetry.speedLoopCarSpeed = 0.0f;
    telemetry.speedLoopOutput = 0.0f;
    return;
  }

  const float commandForDirection = telemetry.speedLoopMixEnabled
                                        ? telemetry.balanceOutputClamped -
                                              (telemetry.speedLoopOutput * telemetry.speedLoopMixScale)
                                        : telemetry.balanceOutputClamped;
  telemetry.speedLoopSignedCommand = clampFloat(commandForDirection, -telemetry.balanceOutputLimit,
                                                telemetry.balanceOutputLimit) *
                                     telemetry.balanceMotorSign;
  if (telemetry.speedLoopSignedCommand > 0.0f) {
    telemetry.speedLoopDirectionSign = 1;
  } else if (telemetry.speedLoopSignedCommand < 0.0f) {
    telemetry.speedLoopDirectionSign = -1;
  } else {
    telemetry.speedLoopDirectionSign = 0;
  }

  if (!telemetry.speedLoopReady || encoder.totalLeft < g_speedLoopLastEncoderLeft ||
      encoder.totalRight < g_speedLoopLastEncoderRight) {
    g_speedLoopLastEncoderLeft = encoder.totalLeft;
    g_speedLoopLastEncoderRight = encoder.totalRight;
    g_speedLoopPeriodCount = 0;
    telemetry.speedLoopReady = true;
    telemetry.speedLoopDeltaLeft = 0;
    telemetry.speedLoopDeltaRight = 0;
    telemetry.speedLoopCarSpeed = 0.0f;
    telemetry.speedLoopFilter = 0.0f;
    telemetry.speedLoopIntegral = 0.0f;
    telemetry.speedLoopOutput = 0.0f;
    return;
  }

  g_speedLoopPeriodCount++;
  if (g_speedLoopPeriodCount < BALANCE_SPEED_LOOP_PERIODS) {
    return;
  }
  g_speedLoopPeriodCount = 0;

  const long rawDeltaLeft = static_cast<long>(encoder.totalLeft - g_speedLoopLastEncoderLeft);
  const long rawDeltaRight = static_cast<long>(encoder.totalRight - g_speedLoopLastEncoderRight);
  g_speedLoopLastEncoderLeft = encoder.totalLeft;
  g_speedLoopLastEncoderRight = encoder.totalRight;

  telemetry.speedLoopDeltaLeft = rawDeltaLeft * telemetry.speedLoopDirectionSign;
  telemetry.speedLoopDeltaRight = rawDeltaRight * telemetry.speedLoopDirectionSign;
  telemetry.speedLoopCarSpeed = (telemetry.speedLoopDeltaLeft + telemetry.speedLoopDeltaRight) * 0.5f;
  telemetry.speedLoopFilter = (telemetry.speedLoopFilter * 0.7f) + (telemetry.speedLoopCarSpeed * 0.3f);
  telemetry.speedLoopIntegral += telemetry.speedLoopFilter;
  telemetry.speedLoopIntegral =
      clampFloat(telemetry.speedLoopIntegral, -BALANCE_SPEED_INTEGRAL_LIMIT, BALANCE_SPEED_INTEGRAL_LIMIT);
  telemetry.speedLoopOutput =
      (-BALANCE_SPEED_KP * telemetry.speedLoopFilter) - (BALANCE_SPEED_KI * telemetry.speedLoopIntegral);
  telemetry.speedLoopSampleCount++;
}

void computeSpeedMixedOutput(BalanceTelemetry &telemetry) {
  float mixedOutput = telemetry.balanceOutputClamped;
  if (telemetry.balanceControlSafetyOk && telemetry.speedLoopMixEnabled && telemetry.speedLoopReady) {
    mixedOutput = telemetry.balanceOutputClamped - (telemetry.speedLoopOutput * telemetry.speedLoopMixScale);
  }

  telemetry.balanceMixedOutputRaw = mixedOutput;
  telemetry.balanceMixedOutputClamped = clampFloat(mixedOutput, -telemetry.balanceOutputLimit, telemetry.balanceOutputLimit);
  if (!telemetry.balanceControlSafetyOk) {
    telemetry.balanceMixedOutputClamped = 0.0f;
  }

  telemetry.balanceLeftPwm = static_cast<int>(telemetry.balanceMixedOutputClamped);
  telemetry.balanceRightPwm = static_cast<int>(telemetry.balanceMixedOutputClamped);
}

void applyBalanceMotorOutput(BalanceTelemetry &telemetry) {
  telemetry.balanceDriveCommandSent = false;

#if BALANCE_MOTOR_OUTPUT_AVAILABLE == 0
  telemetry.balanceMotorOutputAvailable = false;
  telemetry.balanceMotorOutputArmed = false;
  telemetry.balanceMotorOutputEnabled = false;
  return;
#else
  const bool armed = copyArmState();
  telemetry.balanceMotorOutputAvailable = true;
  telemetry.balanceMotorOutputArmed = armed;
  telemetry.balanceMotorOutputEnabled = armed;

  if (!armed) {
    return;
  }

  if (!telemetry.balanceControlSafetyOk) {
    setArmState(false);
    telemetry.balanceMotorOutputArmed = false;
    telemetry.balanceMotorOutputEnabled = false;
    telemetry.balanceIntegralError = 0.0f;
    queueMotorStop();
    return;
  }

  const int pwm = clampPwm(telemetry.balanceMixedOutputClamped * telemetry.balanceMotorSign);
  telemetry.balanceLeftPwm = pwm;
  telemetry.balanceRightPwm = pwm;

  if (!g_motorQueue) {
    return;
  }

  MotorCommandMsg msg{};
  msg.cmd = MotorCommand::BalanceDrive;
  msg.timeoutMs = BALANCE_DRIVE_COMMAND_TIMEOUT_MS;
  msg.leftPwm = pwm;
  msg.rightPwm = pwm;
  telemetry.balanceDriveCommandSent = xQueueSend(g_motorQueue, &msg, 0) == pdPASS;
#endif
}

void applyConfigToTelemetry(BalanceTelemetry &telemetry, const BalanceControlConfig &config) {
  telemetry.balanceControllerEnabled = BALANCE_CONTROLLER_COMPUTE_ENABLED != 0;
  telemetry.balanceMotorOutputAvailable = BALANCE_MOTOR_OUTPUT_AVAILABLE != 0;
  telemetry.balanceMotorOutputArmed = copyArmState();
  telemetry.balanceMotorOutputEnabled = telemetry.balanceMotorOutputAvailable && telemetry.balanceMotorOutputArmed;
  telemetry.balanceSetpointDeg = config.setpointDeg;
  telemetry.balanceKp = config.kp;
  telemetry.balanceKi = config.ki;
  telemetry.balanceKd = config.kd;
  telemetry.balanceOutputLimit = config.outputLimit;
  telemetry.balanceMaxAbsAngleDeg = config.maxAbsAngleDeg;
  telemetry.balanceMotorSign = config.motorSign >= 0.0f ? 1.0f : -1.0f;
  telemetry.speedLoopMixEnabled = config.speedMixEnabled;
  telemetry.speedLoopMixScale = clampFloat(config.speedMixScale, 0.0f, 1.0f);
}

float clampFloat(float value, float minValue, float maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

int clampPwm(float value) {
  return static_cast<int>(clampFloat(value, -255.0f, 255.0f));
}

void queueMotorStop() {
  if (!g_motorQueue) {
    return;
  }
  MotorCommandMsg msg{};
  msg.cmd = MotorCommand::Stop;
  msg.timeoutMs = 0;
  xQueueSend(g_motorQueue, &msg, 0);
}

void emitSerialTelemetry(const BalanceTelemetry &telemetry) {
#if defined(USE_SERIAL) && defined(BALANCE_SERIAL_TELEMETRY)
  static bool headerPrinted = false;
  static unsigned long lastSerialMs = 0;
  const unsigned long nowMs = millis();
  if (nowMs - lastSerialMs < BALANCE_SERIAL_TELEMETRY_MS) {
    return;
  }
  lastSerialMs = nowMs;

  if (!headerPrinted) {
    Serial.println(
        "balance_csv,ms,lastReadOk,calibrated,loopDtMs,failedReadCount,accelTiltXSmoothedDeg,accelTiltXDeg,"
        "accelAngleAyAzSmoothedDeg,accelAngleAxAySmoothedDeg,gyroXRateDps,gyroYRateDps,gyroZRateDps,"
        "balanceControlSafetyOk,balanceOutputClamped,balanceMixedOutputClamped,speedLoopSignedCommand,"
        "speedLoopDirectionSign,"
        "balanceSafetyReason,lastError");
    headerPrinted = true;
  }

  Serial.print("balance_csv,");
  Serial.print(telemetry.updatedAtMs);
  Serial.print(",");
  Serial.print(telemetry.lastReadOk ? 1 : 0);
  Serial.print(",");
  Serial.print(telemetry.calibrated ? 1 : 0);
  Serial.print(",");
  Serial.print(telemetry.loopDtMs, 3);
  Serial.print(",");
  Serial.print(telemetry.failedReadCount);
  Serial.print(",");
  Serial.print(telemetry.accelTiltXSmoothedDeg, 3);
  Serial.print(",");
  Serial.print(telemetry.accelTiltXDeg, 3);
  Serial.print(",");
  Serial.print(telemetry.accelAngleAyAzSmoothedDeg, 3);
  Serial.print(",");
  Serial.print(telemetry.accelAngleAxAySmoothedDeg, 3);
  Serial.print(",");
  Serial.print(telemetry.gyroXRateDps, 3);
  Serial.print(",");
  Serial.print(telemetry.gyroYRateDps, 3);
  Serial.print(",");
  Serial.print(telemetry.gyroZRateDps, 3);
  Serial.print(",");
  Serial.print(telemetry.balanceControlSafetyOk ? 1 : 0);
  Serial.print(",");
  Serial.print(telemetry.balanceOutputClamped, 3);
  Serial.print(",");
  Serial.print(telemetry.balanceMixedOutputClamped, 3);
  Serial.print(",");
  Serial.print(telemetry.speedLoopSignedCommand, 3);
  Serial.print(",");
  Serial.print(telemetry.speedLoopDirectionSign);
  Serial.print(",");
  Serial.print(telemetry.balanceSafetyReason);
  Serial.print(",");
  Serial.println(telemetry.lastError);
#endif
}
}  // namespace
