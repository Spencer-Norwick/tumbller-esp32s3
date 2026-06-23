# Balancing Experiments

Use this log for each meaningful experiment, including failed attempts. Keep entries concrete enough that another contributor can reproduce the setup and understand the result.

## Template

Date:

Goal:

Setup:

Change:

Result:

Takeaway:

Next action:

## 2026-06-23: Branch And Safety Baseline

Goal: Start the self-balancing port with a public, reviewable branch and preserve the existing motor bring-up work.

Setup: Local fork `Spencer-Norwick/tumbller-esp32s3`, upstream `YakRoboticsGarage/tumbller-esp32s3`, starting from `spencer-dev`.

Change: Created `feature/self-balancing-sensor-validation` and committed the existing motor diagnostic controls before adding balance telemetry.

Result: Branch has a clean first commit for motor diagnostics, keeping safety and polarity work distinct from IMU work.

Takeaway: The balancing port should remain incremental: motor diagnostics, sensor validation, controller design, then tuning.

Next action: Add IMU raw telemetry and pitch validation endpoints with balance motor output disabled.

## 2026-06-23: Elegoo Source Inspection

Goal: Confirm the exact Elegoo source files used as references for the ESP32S3 port.

Setup: Local official archive at `/Users/spencer/Desktop/Project/yak_rover/ELEGOO TumbllerV1.1 Self-Balancing Car Tutorial 2024.03.01`.

Change: Inspected `Tumbller_Code_20240301/Tumbller/BalanceCar.h`, `KalmanFilter.cpp/.h`, and `MPU6050.cpp/.h`.

Result: Confirmed the original loop uses an MPU6050 at `0x68`, a 5 ms balance interval, `atan2(ay, az)` pitch input, gyro X scaled by `131`, and Kalman constants `Q_angle=0.001`, `Q_gyro=0.005`, `R_angle=0.5`, `C_0=1`.

Takeaway: The first firmware milestone should validate axes and angle sign before any motor balancing output is connected.

Next action: Implement ESP32S3 IMU reads and pitch telemetry without copying the full AVR-oriented vendor library.

## 2026-06-23: Sensor Telemetry Firmware Build

Goal: Add sensor-validation firmware without connecting balance output to the motors.

Setup: PlatformIO environment `arduino_nano_esp32`, Arduino Nano ESP32 target, existing WiFi/server and motor-task architecture.

Change: Added a minimal MPU6050 register driver, a Kalman pitch estimator based on Elegoo's `KalmanFilter.cpp`, a `balanceTask` that samples at 5 ms, a shared I2C mutex, and HTTP endpoints `/imu/raw`, `/balance/status`, and `/balance/calibrate`.

Result: `platformio run` completed successfully. Reported memory use: RAM 14.4%, flash 24.7%.

Takeaway: The first balance milestone now supports observable raw IMU and pitch telemetry while keeping `balanceMotorOutputEnabled=false`.

Next action: Flash the board, run `/i2c/scan`, verify the IMU appears at `0x68`, then hand-rotate the robot and record angle sign/range observations.
