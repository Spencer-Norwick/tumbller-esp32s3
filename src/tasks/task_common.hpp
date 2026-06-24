#pragma once

#include "../config.hpp"

#include <Arduino.h>
#include <SensirionI2cSht3x.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Motor command types shared between tasks
enum class MotorCommand : uint8_t {
  Forward,
  Back,
  Left,
  Right,
  LeftHigh,
  LeftLow,
  RightHigh,
  RightLow,
  BalanceDrive,
  Stop
};

struct MotorCommandMsg {
  MotorCommand cmd;
  unsigned long timeoutMs; // how long to run before auto-stop; 0 means no timer
  int leftPwm = 0;
  int rightPwm = 0;
};

struct EncoderTelemetry {
  bool initialized = false;
  unsigned long updatedAtMs = 0;
  unsigned long windowMs = 0;
  unsigned long totalLeft = 0;
  unsigned long totalRight = 0;
  long deltaLeft = 0;
  long deltaRight = 0;
  float leftRatePps = 0.0f;
  float rightRatePps = 0.0f;
  float combinedSpeedPulses = 0.0f;
  float speedFilter = 0.0f;
  uint32_t sampleCount = 0;
};

// Shared resources
extern QueueHandle_t g_motorQueue;
extern SemaphoreHandle_t g_i2cMutex;
extern String motorState;
extern const char *const MOTOR_STATE_STRINGS[5];
extern SensirionI2cSht3x sensor;
extern bool sht3xReady;

// Motor behavior constants
constexpr int MOTOR_SPEED = 90;
constexpr unsigned long MOTOR_FORWARD_BACK_TIME = 200; // ms
constexpr unsigned long MOTOR_TURN_TIME = 180; // ms
constexpr unsigned long MOTOR_DIAGNOSTIC_TIME = 220; // ms
constexpr unsigned long MOTOR_HOLD_REFRESH_TIME = 900; // ms

// Initialize shared queue and state
void task_common_init();
bool i2c_lock(TickType_t timeoutTicks);
void i2c_unlock();
