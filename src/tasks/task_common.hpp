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
