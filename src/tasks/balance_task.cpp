#include "balance_task.hpp"

#include <Wire.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../config.hpp"
#include "../drivers/KalmanPitch.hpp"
#include "task_common.hpp"

namespace {
Mpu6050Imu g_imu;
KalmanPitch g_pitchFilter;
BalanceTelemetry g_telemetry;
portMUX_TYPE g_telemetryMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool g_calibrationRequested = false;

void balanceTask(void *pvParameters);
void publishTelemetry(const BalanceTelemetry &telemetry);
void copyTelemetry(BalanceTelemetry &out);
void setError(BalanceTelemetry &telemetry, const char *message);
bool readImuLocked(ImuRawSample &sample, const char *&error);
bool runGyroCalibration(float &gyroBiasRaw, BalanceTelemetry &telemetry);
float computeElegooPitchDeg(const ImuRawSample &sample);
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

namespace {
void balanceTask(void *pvParameters) {
  (void)pvParameters;

  BalanceTelemetry telemetry;
  telemetry.imuAddress = IMU_I2C_ADDR;
  telemetry.balanceMotorOutputEnabled = false;

  if (i2c_lock(pdMS_TO_TICKS(500))) {
    telemetry.imuReady = g_imu.begin(Wire, IMU_I2C_ADDR);
    telemetry.whoAmI = g_imu.whoAmI();
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
      telemetry.calibrated = runGyroCalibration(gyroBiasRaw, telemetry);
      telemetry.gyroBiasRaw = gyroBiasRaw;
      telemetry.calibrationInProgress = false;
      g_pitchFilter.reset(telemetry.accelPitchDeg);
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
      telemetry.accelPitchDeg = computeElegooPitchDeg(sample);
      const float gyroRateDps = (sample.gx - telemetry.gyroBiasRaw) / 131.0f;
      telemetry.gyroRateDps = gyroRateDps;
      g_pitchFilter.update(telemetry.accelPitchDeg, gyroRateDps, dtSeconds);
      telemetry.pitchDeg = g_pitchFilter.angleDeg();
      setError(telemetry, "ok");
    } else {
      telemetry.failedReadCount++;
      setError(telemetry, telemetry.imuReady ? readError : "imu not ready");
    }

    publishTelemetry(telemetry);
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

void setError(BalanceTelemetry &telemetry, const char *message) {
  std::strncpy(telemetry.lastError, message, sizeof(telemetry.lastError) - 1);
  telemetry.lastError[sizeof(telemetry.lastError) - 1] = '\0';
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

bool runGyroCalibration(float &gyroBiasRaw, BalanceTelemetry &telemetry) {
  int64_t gyroXSum = 0;
  int samplesRead = 0;

  for (int i = 0; i < BALANCE_GYRO_CALIBRATION_SAMPLES; i++) {
    ImuRawSample sample;
    const char *readError = nullptr;
    if (!readImuLocked(sample, readError)) {
      setError(telemetry, readError);
      return false;
    }
    gyroXSum += sample.gx;
    telemetry.raw = sample;
    telemetry.accelPitchDeg = computeElegooPitchDeg(sample);
    samplesRead++;
    vTaskDelay(pdMS_TO_TICKS(2));
  }

  if (samplesRead == 0) {
    setError(telemetry, "no calibration samples");
    return false;
  }

  gyroBiasRaw = static_cast<float>(gyroXSum) / static_cast<float>(samplesRead);
  setError(telemetry, "calibrated");
  return true;
}

float computeElegooPitchDeg(const ImuRawSample &sample) {
  return atan2f(static_cast<float>(sample.ay), static_cast<float>(sample.az)) * 57.2957795f;
}
}  // namespace
