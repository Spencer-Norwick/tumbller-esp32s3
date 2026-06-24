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

## 2026-06-23: Hands-On Axis Observation

Goal: Determine whether the current Elegoo pitch convention measures the robot's forward/back balance axis on this hardware.

Setup: Balance Lab dashboard open at `http://127.0.0.1:8787`, robot manually tilted by hand.

Change: Observed pitch response while tilting the robot forward/back and side-to-side.

Result: The current pitch estimate is close to `0 deg` when upright on wheels and close to `-180 deg` when wheels-up, but it changes much more during side-to-side wheel-lift motion than during forward/back tilt.

Takeaway: The current Elegoo `atan2(ay, az)` angle appears to be measuring the side-to-side roll axis for this ESP32/IMU mounting, not the forward/back balance axis. We need explicit axis-candidate telemetry before choosing the Kalman input for motor balancing.

Next action: Expose alternate accelerometer angle candidates and gyro axis rates in `/balance/status`, then repeat the hands-on tilt test to identify the forward/back axis.

## 2026-06-23: Telemetry Polling Load Check

Goal: Verify that repeated telemetry reads can support hands-on IMU orientation testing without disrupting the ESP32 HTTP server.

Setup: Balance Lab dashboard pointed at the robot over WiFi, with `/balance/status` and `/imu/raw` requested repeatedly through the local dashboard proxy.

Change: Added axis-candidate fields to `/balance/status`, then refreshed the dashboard while it was polling telemetry.

Result: `/balance/status` initially returned the new axis fields successfully, but repeated polling produced intermittent proxy `502` responses and eventually direct requests to the board timed out.

Takeaway: The current single-client HTTP server path needs to fail fast on stale connections and yield while waiting for request headers. The dashboard should avoid unnecessary concurrent polling while we are validating control-loop sensors.

Next action: Shorten the firmware header-read timeout, add a maximum header size, yield inside the client-read loop, and keep live dashboard polling focused on `/balance/status`.

## 2026-06-24: Axis Candidate Readability

Goal: Make hands-on IMU orientation characterization easier to interpret.

Setup: Robot manually tilted while watching the Balance Lab dashboard axis-candidate panel.

Change: Reviewed the axis-candidate telemetry names and live readout behavior after a hand test showed noisy values and unclear `AxAz`/`AxAy` labels.

Result: The raw candidate values were too jumpy for easy hand interpretation, and the two-axis field names were not self-explanatory without knowing that each candidate is an `atan2(axis1, axis2)` angle.

Takeaway: Sensor-validation telemetry needs both machine-useful raw values and human-readable, smoothed readouts for bring-up work.

Next action: Add low-pass filtered `*SmoothedDeg` fields to `/balance/status`, keep raw candidate fields, and update dashboard labels to show the `atan2(...)` convention directly.

## 2026-06-24: Axis Trace Selection

Goal: Make the dashboard trace useful for identifying the forward/back balance axis.

Setup: Balance Lab dashboard polling `/balance/status`; robot manually rocked forward/back and side-to-side.

Change: Compared dashboard trace response against the numeric axis-candidate readouts.

Result: The original trace mostly moved during side-to-side wheel-lift motion because it plotted the legacy `pitchDeg`/`gyroRateDps` path, which still follows the Elegoo `atan2(ay, az)` and gyro-X convention.

Takeaway: Until the correct mounted pitch axis is selected, the trace should show all candidate angle movement rather than only the current Kalman pitch path.

Next action: Use a candidate-delta trace and recent peak-to-peak movement readouts to compare forward/back rocking against side-to-side rocking.

## 2026-06-24: Forward/Back Candidate Movement

Goal: Identify which accelerometer angle candidates respond to forward/back rocking.

Setup: Balance Lab dashboard using the candidate-delta trace and peak-to-peak movement readouts; robot manually rocked forward/back.

Change: Cleared the trace, then rocked the robot forward/back while comparing `atan2(AY, AZ)`, `atan2(AX, AZ)`, and `atan2(AX, AY)`.

Result: `atan2(AX, AZ)` and `atan2(AX, AY)` both moved during forward/back rocking. `atan2(AX, AY)` appeared to move roughly three times as much as `atan2(AX, AZ)`.

Takeaway: The forward/back balance axis likely involves the accelerometer X component, but larger amplitude alone is not enough to select `atan2(AX, AY)`. If AY is near zero around the upright pose, `atan2(AX, AY)` can be overly sensitive and less stable than `atan2(AX, AZ)`.

Next action: Run the same cleared-trace test for side-to-side wheel-lift motion and compare peak-to-peak movement. Prefer the candidate that moves strongly during forward/back rocking and weakly during side-to-side rocking.

## 2026-06-24: AX/AY Wrap During Forward/Back Rocking

Goal: Determine whether the high-amplitude `atan2(AX, AY)` response is a usable pitch candidate.

Setup: Balance Lab candidate-delta trace after a few forward/back rocks. Screenshot captured with `AY/AZ movement = 1.47 deg`, `AX/AZ movement = 126.48 deg`, and `AX/AY movement = 179.77 deg`.

Change: Compared trace shape and symmetry, not only peak-to-peak movement.

Result: The `atan2(AX, AY)` trace jumped sharply and was not symmetric around the resting position. It approached a near-180 degree movement range, while `atan2(AX, AZ)` moved more continuously.

Takeaway: `atan2(AX, AY)` is likely crossing an unstable region around the upright pose and should be treated as a debug signal, not the primary pitch estimate. A pitch-from-X candidate should compare AX against the combined non-X gravity magnitude instead of only AY.

Next action: Add `accelTiltXDeg = atan2(ax, sqrt(ay^2 + az^2))` and use it in the dashboard trace for the next forward/back and side-to-side tests.

## 2026-06-24: Tilt-Dependent WiFi Telemetry Dropout

Goal: Determine whether telemetry loss during backward tilt is caused by sensor math, sensor reads, WiFi/HTTP, or physical connection issues.

Setup: Balance Lab dashboard over WiFi/HTTP while manually tilting forward and backward.

Change: Slowly tilted forward, then attempted to tilt backward.

Result: Forward tilt produced visible X-tilt trace movement. During backward tilt the dashboard telemetry went offline; tilting forward again brought telemetry back, but the dashboard was laggy.

Takeaway: A dashboard offline state means the HTTP telemetry path failed, not necessarily that IMU reads failed. The position-dependent behavior may be WiFi orientation, USB/power strain, loose wiring, or firmware HTTP overload, so we need an observability channel that bypasses WiFi.

Next action: Add low-rate USB serial `balance_csv` telemetry and repeat the tilt test with a serial monitor open. If serial keeps streaming while WiFi drops, debug WiFi/HTTP/tooling. If serial stops or resets, inspect power, USB cable strain, and wiring.

## 2026-06-24: X-Tilt Candidate Shape

Goal: Compare `accelTiltXDeg` against the wrapping `atan2(AX, AY)` signal during slow forward/back rocking.

Setup: Balance Lab dashboard after adding the X-tilt candidate. Robot was slowly rocked forward/back near upright.

Change: Observed the trace shape for `atan2(AY, AZ)`, X tilt, and `atan2(AX, AY)`.

Result: X tilt moved smoothly through the forward/back rocking motion. `atan2(AX, AY)` jumped across the plot axis as the robot approached and crossed the upright balance point.

Takeaway: X tilt is the leading accelerometer pitch candidate for forward/back balance on this mounting. `atan2(AX, AY)` should remain visible as a diagnostic signal only, because it wraps near upright.

Next action: Run the same cleared-trace test for side-to-side wheel-lift motion. If X tilt remains comparatively quiet during side-to-side motion, use X tilt plus the matching gyro axis as the next Kalman pitch input.
