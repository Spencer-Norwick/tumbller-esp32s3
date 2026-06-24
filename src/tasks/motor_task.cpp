#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../drivers/Motor.hpp"
#include "task_common.hpp"
#include "motor_task.hpp"

static void motorTask(void *pvParameters);
static void publishEncoderTelemetry();
static void publishEncoderTelemetryLocked(const EncoderTelemetry &telemetry);
static Motor motor;
static EncoderTelemetry g_encoderTelemetry;
static portMUX_TYPE g_encoderTelemetryMux = portMUX_INITIALIZER_UNLOCKED;
static unsigned long g_encoderLastLeft = 0;
static unsigned long g_encoderLastRight = 0;
static float g_encoderSpeedFilterOld = 0.0f;

void motor_task_start() {
  motor.Pin_init();
  motor.Encoder_init();
  motor.Stop(0);
  motor_reset_encoder_counts();
  xTaskCreatePinnedToCore(motorTask, "motorTask", 4096, nullptr, 2, nullptr, 1);
}

void motor_get_encoder_status(EncoderTelemetry &out) {
  taskENTER_CRITICAL(&g_encoderTelemetryMux);
  out = g_encoderTelemetry;
  taskEXIT_CRITICAL(&g_encoderTelemetryMux);
}

void motor_reset_encoder_counts() {
  Motor::ResetEncoderCounts();
  g_encoderLastLeft = 0;
  g_encoderLastRight = 0;
  g_encoderSpeedFilterOld = 0.0f;
  EncoderTelemetry telemetry;
  telemetry.initialized = true;
  telemetry.updatedAtMs = millis();
  telemetry.windowMs = ENCODER_SPEED_WINDOW_MS;
  publishEncoderTelemetryLocked(telemetry);
}

static void motorTask(void *pvParameters) {
  (void)pvParameters;
  bool motorRunning = false;
  unsigned long motorStartTime = 0;
  unsigned long motorTimeout = 0;
  unsigned long lastEncoderSampleMs = millis();

  for (;;) {
    MotorCommandMsg msg;
    if (g_motorQueue && xQueueReceive(g_motorQueue, &msg, pdMS_TO_TICKS(5)) == pdPASS) {
      switch (msg.cmd) {
        case MotorCommand::Forward:
          motor.Forward(MOTOR_SPEED);
          motorState = MOTOR_STATE_STRINGS[0];
          motorRunning = true;
          motorTimeout = msg.timeoutMs;
          motorStartTime = millis();
          break;
        case MotorCommand::Back:
          motor.Back(MOTOR_SPEED);
          motorState = MOTOR_STATE_STRINGS[1];
          motorRunning = true;
          motorTimeout = msg.timeoutMs;
          motorStartTime = millis();
          break;
        case MotorCommand::Left:
          motor.Left(MOTOR_SPEED);
          motorState = MOTOR_STATE_STRINGS[2];
          motorRunning = true;
          motorTimeout = msg.timeoutMs;
          motorStartTime = millis();
          break;
        case MotorCommand::Right:
          motor.Right(MOTOR_SPEED);
          motorState = MOTOR_STATE_STRINGS[3];
          motorRunning = true;
          motorTimeout = msg.timeoutMs;
          motorStartTime = millis();
          break;
        case MotorCommand::LeftHigh:
          motor.LeftOnlyHigh(MOTOR_SPEED);
          motorState = "LEFT_HIGH";
          motorRunning = true;
          motorTimeout = msg.timeoutMs;
          motorStartTime = millis();
          break;
        case MotorCommand::LeftLow:
          motor.LeftOnlyLow(MOTOR_SPEED);
          motorState = "LEFT_LOW";
          motorRunning = true;
          motorTimeout = msg.timeoutMs;
          motorStartTime = millis();
          break;
        case MotorCommand::RightHigh:
          motor.RightOnlyHigh(MOTOR_SPEED);
          motorState = "RIGHT_HIGH";
          motorRunning = true;
          motorTimeout = msg.timeoutMs;
          motorStartTime = millis();
          break;
        case MotorCommand::RightLow:
          motor.RightOnlyLow(MOTOR_SPEED);
          motorState = "RIGHT_LOW";
          motorRunning = true;
          motorTimeout = msg.timeoutMs;
          motorStartTime = millis();
          break;
        case MotorCommand::BalanceDrive:
          motor.DriveSigned(msg.leftPwm, msg.rightPwm);
          motorState = "BALANCE_DRIVE";
          motorRunning = true;
          motorTimeout = msg.timeoutMs;
          motorStartTime = millis();
          break;
        case MotorCommand::Stop:
        default:
          motor.Stop(0);
          motorState = MOTOR_STATE_STRINGS[4];
          motorRunning = false;
          motorTimeout = 0;
          break;
      }
    }

    if (motorRunning && motorTimeout > 0 && (millis() - motorStartTime >= motorTimeout)) {
      motor.Stop(0);
      motorRunning = false;
      motorState = MOTOR_STATE_STRINGS[4];
#ifdef USE_SERIAL
      Serial.println("Motor stopped after time limit");
#endif
    }

    const unsigned long nowMs = millis();
    if (nowMs - lastEncoderSampleMs >= ENCODER_SPEED_WINDOW_MS) {
      lastEncoderSampleMs = nowMs;
      publishEncoderTelemetry();
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

static void publishEncoderTelemetry() {
  unsigned long left = 0;
  unsigned long right = 0;
  Motor::SnapshotEncoderCounts(left, right);

  EncoderTelemetry telemetry;
  motor_get_encoder_status(telemetry);
  const long deltaLeft = static_cast<long>(left - g_encoderLastLeft);
  const long deltaRight = static_cast<long>(right - g_encoderLastRight);
  g_encoderLastLeft = left;
  g_encoderLastRight = right;

  telemetry.initialized = true;
  telemetry.updatedAtMs = millis();
  telemetry.windowMs = ENCODER_SPEED_WINDOW_MS;
  telemetry.totalLeft = left;
  telemetry.totalRight = right;
  telemetry.deltaLeft = deltaLeft;
  telemetry.deltaRight = deltaRight;
  telemetry.leftRatePps = static_cast<float>(deltaLeft) * (1000.0f / ENCODER_SPEED_WINDOW_MS);
  telemetry.rightRatePps = static_cast<float>(deltaRight) * (1000.0f / ENCODER_SPEED_WINDOW_MS);
  telemetry.combinedSpeedPulses = (static_cast<float>(deltaLeft) + static_cast<float>(deltaRight)) * 0.5f;
  telemetry.speedFilter = g_encoderSpeedFilterOld * 0.7f + telemetry.combinedSpeedPulses * 0.3f;
  g_encoderSpeedFilterOld = telemetry.speedFilter;
  telemetry.sampleCount++;

  publishEncoderTelemetryLocked(telemetry);
}

static void publishEncoderTelemetryLocked(const EncoderTelemetry &telemetry) {
  taskENTER_CRITICAL(&g_encoderTelemetryMux);
  g_encoderTelemetry = telemetry;
  taskEXIT_CRITICAL(&g_encoderTelemetryMux);
}
