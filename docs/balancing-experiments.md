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

## 2026-06-24: Side-to-Side Roll Candidate Shape

Goal: Confirm which candidate represents side-to-side roll when lifting one wheel at a time.

Setup: Balance Lab dashboard after adding the X-tilt candidate. Robot was tilted side-to-side by lifting one wheel, then the other.

Change: Observed the trace shape for `atan2(AY, AZ)`, X tilt, and `atan2(AX, AY)` during wheel-lift roll motion.

Result: `atan2(AY, AZ)` moved smoothly and symmetrically across the resting axis during side-to-side roll. X tilt did not show the same clean roll-centered shape. `atan2(AX, AY)` continued to show wrap-like behavior.

Takeaway: `atan2(AY, AZ)` is the roll-axis candidate on this mounting. This reinforces that the original Elegoo `atan2(ay, az)` pitch convention does not map directly to forward/back pitch on the ESP32S3 Tumbller hardware.

Next action: Identify the gyro axis that best matches X-tilt forward/back pitch motion, then switch the validation Kalman path from the Elegoo AY/AZ + gyro-X convention to the mounted X-tilt + matching-gyro convention.

## 2026-06-24: Forward/Back Gyro Axis Match

Goal: Identify which gyro axis matches the X-tilt forward/back pitch candidate.

Setup: Balance Lab dashboard with separate gyro-rate trace for gyro X, Y, and Z. Robot was manually tilted forward and backward.

Change: Cleared the trace, then compared gyro X/Y/Z movement while rocking forward/back.

Result: Gyro Y was the only gyro trace that clearly moved during forward/back tilt. Its polarity changed when the tilt direction reversed. Gyro X and gyro Z did not show comparable movement for this test.

Takeaway: The mounted forward/back pitch path should use X tilt for angle and gyro Y for rate. This differs from the original Elegoo AVR convention, which used `atan2(ay, az)` and gyro X.

Next action: Switch the validation Kalman input to `accelTiltXDeg + gyroYRateDps`, calibrating stationary gyro bias from raw gyro Y, while keeping balance motor output disabled.

## 2026-06-24: Validation Kalman Axis Switch

Goal: Apply the mounted pitch-axis finding to the validation-only Kalman path.

Setup: Firmware update after hand tests identified X tilt as the forward/back angle candidate and gyro Y as the matching rate candidate.

Change: Changed `accelPitchDeg` to use `accelTiltXDeg`, changed `gyroRateDps` to use bias-corrected raw gyro Y, changed stationary gyro calibration to average raw gyro Y, and added source labels to `/balance/status`.

Result: Code and documentation updated locally. Build/upload validation is still pending because PlatformIO could not access its normal home-directory cache in the current restricted shell environment.

Takeaway: The selected validation path now matches the observed ESP32S3 Tumbller IMU mounting, but it must still be built, uploaded, calibrated, and hand-validated before any motor-control milestone.

Next action: Run `platformio run`, upload to the Nano ESP32, call `/balance/calibrate` while the robot is still, and validate that `pitchDeg` now follows forward/back rocking with `balanceMotorOutputEnabled=false`.

## 2026-06-24: Mounted Pitch Path Build, Upload, and Stationary Calibration

Goal: Build and upload the `accelTiltXDeg + gyroYRateDps` validation path, then verify stationary calibration.

Setup: Arduino Nano ESP32 connected over USB at `/dev/cu.usbmodem11201`; Balance Lab dashboard pointed at `http://192.168.4.53`.

Change: Built with PlatformIO, uploaded the firmware, restarted the local dashboard, and triggered `/balance/calibrate` while the robot was held still.

Result: Build and upload succeeded. `/balance/status` reported `accelPitchSource="accelTiltXDeg"`, `gyroRateSource="gyroYRateDps"`, `calibrated=true`, `balanceMotorOutputEnabled=false`, `failedReadCount=0`, and `lastError="ok"`. Stationary gyro-Y bias calibrated to `227.746` raw counts. After calibration, `gyroRateDps` hovered near zero and `pitchDeg` stayed near `-2.35 deg` during the sampled stationary window.

Takeaway: The selected mounted pitch path is now deployed and stationary gyro-Y calibration behaves correctly. No motor output is enabled.

Next action: Repeat the trace test with deliberate forward/back rocking and confirm `pitchDeg` follows X tilt dynamically while side-to-side roll is rejected.

## 2026-06-24: Mounted Pitch Dynamic Forward/Back Validation

Goal: Confirm the deployed `accelTiltXDeg + gyroYRateDps` path responds to deliberate forward/back rocking and rejects side-to-side roll.

Setup: Balance Lab dashboard after upload and stationary gyro-Y calibration. The trace was cleared, the robot was slowly rocked forward/back, then returned to upright stationary.

Change: Compared dashboard movement ranges for angle candidates and gyro axes after the forward/back rocking trace.

Result: X tilt moved `64.53 deg` during the trace, while `atan2(AY, AZ)` moved only `0.24 deg`. `atan2(AX, AY)` moved `178.75 deg`, consistent with the known near-upright wrap behavior. Gyro Y showed the dominant rate movement at `20.95 dps`; gyro X moved `0.30 dps` and gyro Z moved `1.52 dps`. After returning upright, `/balance/status` reported `pitchDeg=-0.622 deg`, `accelPitchSource="accelTiltXDeg"`, `gyroRateSource="gyroYRateDps"`, `gyroRateDps=-0.036`, `balanceMotorOutputEnabled=false`, `failedReadCount=0`, and `lastError="ok"`.

Takeaway: The deployed validation path correctly selects the forward/back mounted pitch axis. X tilt is the pitch angle candidate, gyro Y is the pitch-rate candidate, AY/AZ is rejected as roll for this mounting, and AX/AY remains a diagnostic wrap signal.

Next action: Add pre-motor balance-control scaffolding: explicit enable gate, PID/output telemetry, and safety constraints, with computed motor output still disabled by default.

## 2026-06-24: Preview-Only Balance Controller Scaffolding

Goal: Add enough controller structure to inspect proposed balance output before allowing any automatic motor writes.

Setup: Firmware after mounted pitch dynamic validation. Motor output is intentionally disabled by configuration.

Change: Added preview controller telemetry using `pitchDeg` and `gyroYRateDps`, starting from the Elegoo vertical-ring reference gains `Kp=55.0`, `Ki=0.0`, and `Kd=0.75`. Added safety gates for sensor health, completed gyro calibration, and a `+/-22 deg` pitch window. Added `/balance/status` fields for controller enabled state, safety state, safety reason, gains, P/I/D terms, raw output, clamped output, and proposed left/right PWM. Held the integral accumulator at zero while `Ki=0.0`.

Result: Build and upload succeeded. Before calibration, `/balance/status` reported `balanceControlSafetyOk=false`, `balanceSafetyReason="not calibrated"`, and `balanceOutputClamped=0.000` despite nonzero raw output. After calibration, stationary telemetry reported `balanceControlSafetyOk=true`, `balanceMotorOutputEnabled=false`, `balanceIntegralError=0.000`, and proposed output around `39 pwm` near `0.7 deg` pitch. During a forward/back hand-rock trace, X tilt moved `92.78 deg`, gyro Y moved `38.22 dps`, and proposed output reached the clamp in both directions: `-255.0 / 255.0 pwm`.

Takeaway: The preview scaffolding works and remains non-actuating. The Elegoo reference gains are high enough to saturate the preview output during large hand-rock tests, so the next tuning step should use smaller preview gains or constrained near-upright tests before any motor-apply milestone.

Next action: Add runtime tuning or a reduced-gain preview profile, then validate proposed output polarity and magnitude with small near-upright pitch motions before enabling any balance motor writes.

## 2026-06-24: Runtime Preview Gain Tuning

Goal: Avoid rebuild/upload cycles for preview-controller tuning while keeping balance motor output disabled.

Setup: Preview-only controller after large hand-rock tests saturated the Elegoo reference output.

Change: Changed compile-time preview defaults to `Kp=5.0`, `Ki=0.0`, `Kd=0.15`, and output limit `120 pwm`. Added RAM-only `/balance/config` tuning for `kp`, `ki`, `kd`, `setpoint`, `limit`, and `maxAngle`. Added dashboard controls to read and apply preview gains without firmware rebuilds.

Result: Build and upload succeeded. `/balance/config` default readback returned `Kp=5.0`, `Ki=0.0`, `Kd=0.15`, output limit `120 pwm`, and `maxAbsAngle=22 deg`. A runtime update to `Kp=6.5`, `Ki=0.0`, `Kd=0.2`, output limit `90 pwm`, setpoint `0 deg`, and `maxAbsAngle=20 deg` returned `updated=true`. After calibration, `/balance/status` reflected the updated gains, `balanceControlSafetyOk=true`, `balanceMotorOutputEnabled=false`, `balanceIntegralError=0.000`, and proposed output around `5 pwm` near `0.8 deg` pitch.

Takeaway: The next tuning loop should happen through the dashboard, not repeated firmware edits. Settings reset on board restart, which keeps experimental gain changes nonpersistent.

Next action: Use the dashboard runtime controls for small near-upright hand tilts and choose preview gains that keep proposed output within range without saturation.

## 2026-06-24: First-Motion Arm Path

Goal: Allow a controlled first balance-motor movement without letting the controller drive motors automatically after reset.

Setup: Robot connected over USB and WiFi after runtime preview tuning. Dashboard running at `http://127.0.0.1:8787`.

Change: Added signed balance-drive motor commands owned by the motor task, runtime `/balance/arm` and `/balance/disarm` endpoints, a RAM-only `motorSign` config field, and dashboard Arm/Disarm controls. Space and Escape in the dashboard disarm balance and send `/motor/stop`.

Result: Build and upload succeeded. Default state after reset was disarmed. A conservative first-motion config was applied with `Kp=5.0`, `Ki=0.0`, `Kd=0.15`, output limit `30 pwm`, max angle `12 deg`, and `motorSign=+1`. With the robot not calibrated and not near upright, `/balance/arm` returned `409 Conflict` with reason `not calibrated`; `/balance/disarm` returned cleanly.

Takeaway: The first-motion path now has an explicit software arm gate, dashboard kill controls, and expected refusal behavior when prerequisites are not met.

Next action: Put the robot wheels-down near upright, recalibrate gyro while stationary, verify `balanceControlSafetyOk=true`, then arm briefly with one hand ready to catch and use Space/Escape to disarm.
