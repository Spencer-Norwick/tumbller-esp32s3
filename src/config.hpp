#pragma once

// Global project configuration: serial logging toggle and pin assignments.
#ifndef USE_SERIAL
// #define USE_SERIAL
#endif

#ifdef USE_SERIAL
  #define SERIAL_BEGIN(baud) Serial.begin(baud)
  #define SERIAL_PRINTLN(msg) Serial.println(msg)
#else
  #define SERIAL_BEGIN(baud) ((void)0)
  #define SERIAL_PRINTLN(msg) ((void)0)
#endif

// SAFE_BRINGUP keeps movement commands disabled while validating a new board.
// /motor/stop still works. Remove -DSAFE_BRINGUP from platformio.ini after
// logic rails, I2C devices, motor polarity, and emergency stop are proven.

// Motor driver pins (Arduino pin names map via board variant)
#define OE   A6
#define AIN1 7
#define PWMA_LEFT 5
#define BIN1 A0
#define PWMB_RIGHT 6
#define STBY_PIN 8

// Encoder pins
#define ENCODER_LEFT_A_PIN 2
#define ENCODER_RIGHT_A_PIN 4

// IMU / balance sensor validation
#define IMU_I2C_ADDR 0x68
#define BALANCE_SENSOR_LOOP_MS 5
#define BALANCE_GYRO_CALIBRATION_SAMPLES 500
#define BALANCE_SERIAL_TELEMETRY 1
#define BALANCE_SERIAL_TELEMETRY_MS 100

// Balance controller scaffolding. This computes proposed output telemetry only.
// Motor writes remain disabled unless a later milestone deliberately adds and
// validates an apply path.
#define BALANCE_CONTROLLER_COMPUTE_ENABLED 1
#define BALANCE_MOTOR_OUTPUT_ENABLED 0
#define BALANCE_ANGLE_SETPOINT_DEG 0.0f
#define BALANCE_PID_KP 55.0f
#define BALANCE_PID_KI 0.0f
#define BALANCE_PID_KD 0.75f
#define BALANCE_PID_OUTPUT_LIMIT 255.0f
#define BALANCE_CONTROL_MAX_ABS_ANGLE_DEG 22.0f

// WiFi / mDNS hostname
#define WIFI_HOSTNAME "finland-tumbller-01"
