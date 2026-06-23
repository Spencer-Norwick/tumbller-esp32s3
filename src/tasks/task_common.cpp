#include <Arduino.h>
#include <Wire.h>

#include "tasks/task_common.hpp"

QueueHandle_t g_motorQueue = nullptr;
SemaphoreHandle_t g_i2cMutex = nullptr;
String motorState;
SensirionI2cSht3x sensor;
bool sht3xReady = false;

const char *const MOTOR_STATE_STRINGS[5] = {
    "FORWARD",
    "BACK",
    "LEFT",
    "RIGHT",
    "STOP"};

bool i2c_lock(TickType_t timeoutTicks) {
  if (!g_i2cMutex) {
    return true;
  }
  return xSemaphoreTake(g_i2cMutex, timeoutTicks) == pdTRUE;
}

void i2c_unlock() {
  if (g_i2cMutex) {
    xSemaphoreGive(g_i2cMutex);
  }
}

void task_common_init() {
  if (!g_motorQueue) {
    g_motorQueue = xQueueCreate(8, sizeof(MotorCommandMsg));
  }
  if (!g_i2cMutex) {
    g_i2cMutex = xSemaphoreCreateMutex();
  }
  motorState = MOTOR_STATE_STRINGS[4];

  // Initialize SHT3x sensor (I2C)
  Wire.begin();
  if (!i2c_lock(pdMS_TO_TICKS(250))) {
    sht3xReady = false;
    return;
  }
  sensor.begin(Wire, SHT30_I2C_ADDR_44);
  sensor.stopMeasurement();
  delay(1);
  sensor.softReset();
  delay(100);
  sht3xReady = true;
  i2c_unlock();
#ifdef USE_SERIAL
  Serial.println("SHT3x initialized");
#endif
}
