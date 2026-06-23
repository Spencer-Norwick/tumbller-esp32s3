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

## 2026-06-23: First Hardware Flash And IMU Identity Check

Goal: Flash the sensor-validation firmware and confirm the board can see the expected I2C devices.

Setup: Arduino Nano ESP32 connected over USB-C at `/dev/cu.usbmodem11301`, WiFi hostname `finland-tumbller-01`, assigned IP `192.168.4.53`.

Change: Uploaded `feature/self-balancing-sensor-validation` firmware and queried `/info`, `/i2c/scan`, `/imu/raw`, and `/balance/status`.

Result: Upload succeeded. `/info` returned the expected hostname/IP. `/i2c/scan` returned `0x44` and `0x68`. The first IMU driver rejected the device because `WHO_AM_I` returned `0x98`, not the common MPU6050-compatible values expected by the driver.

Takeaway: The hardware has an IMU responding at the expected I2C address, but identity validation must be advisory during bring-up. Raw reads are more useful than blocking telemetry on a strict chip ID check.

Next action: Allow raw IMU reads with `whoAmICompatible=false` so orientation and data sanity can be validated before deciding whether the IMU identity needs a driver-specific path.

## 2026-06-23: Advisory IMU Identity And First Raw Reads

Goal: Continue sensor validation even though the IMU identity register does not match the common MPU6050-compatible values.

Setup: Same flashed board at `192.168.4.53`, IMU responding on I2C address `0x68`.

Change: Changed `WHO_AM_I` handling from a hard failure to advisory telemetry. The endpoints now expose `whoAmI` and `whoAmICompatible` while still attempting raw reads.

Result: `/imu/raw` returned `imuReady=true`, `lastReadOk=true`, `whoAmI=0x98`, `whoAmICompatible=false`, with nonzero accelerometer and gyro data. `/balance/status` returned `failedReadCount=0` and `balanceMotorOutputEnabled=false`.

Takeaway: The sensor path is live, but the identity mismatch should stay visible because it may indicate a compatible clone or a different IMU variant. The current Elegoo pitch convention reports about `-178 deg` while the robot is resting in this orientation, so axis/sign validation remains required before motor control.

Next action: Hand-rotate the robot around each axis, record raw value changes and pitch direction, then decide whether pitch should use the Elegoo axis unchanged or an ESP32S3-specific orientation transform.

## 2026-06-23: Stationary Gyro Calibration Smoke Test

Goal: Verify `/balance/calibrate` updates gyro bias and reduces stationary gyro-rate drift.

Setup: Robot kept stationary after raw IMU reads were enabled.

Change: Called `/balance/calibrate`, then queried `/balance/status`.

Result: Calibration completed with `calibrated=true`, `gyroBiasRaw=47.718`, and `gyroRateDps=0.017`. Loop timing reported about `7.000 ms`, not the configured 5 ms target.

Takeaway: Calibration works as a smoke test, but loop timing is slower than intended and should be measured again after any task-priority or delay changes. The angle estimate is still not validated for physical orientation.

Next action: Keep balance motor output disabled; next work should focus on orientation validation and loop timing before any PID output is connected.
