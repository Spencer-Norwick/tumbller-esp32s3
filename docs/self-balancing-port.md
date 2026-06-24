# Tumbller Self-Balancing Port

## Source References

- Upstream issue: YakRoboticsGarage/tumbller-esp32s3#8, "Port tumbller self balancing code".
- Official tutorial page: https://www.elegoo.com/blogs/arduino-projects/elegoo-tumbller-self-balancing-robot-car-tutorial
- Local source archive: `/Users/spencer/Desktop/Project/yak_rover/ELEGOO TumbllerV1.1 Self-Balancing Car Tutorial 2024.03.01`
- Primary Elegoo code reference: `Tumbller_Code_20240301/Tumbller`
- Important source files:
  - `BalanceCar.h`: 5 ms balance loop, PID mixing, encoder speed loop, fall handling.
  - `KalmanFilter.cpp`: angle estimate from `atan2(ay, az)` and gyro X.
  - `MPU6050.cpp/.h`: MPU6050 register setup and motion reads.

## Milestone 1: Sensor Validation

Goal: make IMU and pitch telemetry observable on the ESP32S3 without enabling automatic balance motor output.

Planned behavior:

- Keep manual `/motor/*` and motor diagnostic endpoints separate from balance telemetry.
- Initialize an MPU6050-style IMU at `0x68`, matching Elegoo's default low-AD0 address.
- Expose raw IMU values at `/imu/raw`.
- Expose pitch estimate, gyro rate, loop timing, calibration state, and disabled motor output at `/balance/status`.
- Stream low-rate `balance_csv` lines over USB serial for sensor validation when WiFi/HTTP observability is unreliable.
- Recalibrate stationary gyro bias through `/balance/calibrate`.
- Protect all I2C users behind one FreeRTOS mutex.

Safety constraints:

- This milestone must not call motor drive methods from balance code.
- Balance motor output stays reported as disabled until IMU orientation, angle zero, and loop timing are validated by hand.
- Tuning constants copied from Elegoo are treated as references, not final values for the ESP32S3 hardware.

## Control Notes From Elegoo V1.1

- The original balance ISR runs every 5 ms.
- Kalman input angle is `atan2(ay, az) * 57.3`.
- Balance gyro is `(gx - 128.1) / 131`.
- Fall range is approximately `-22` to `22` degrees.
- The speed PI controller is intentionally slower than the balance loop; it updates every eighth 5 ms tick.

## IMU Axis Candidate Notes

The accelerometer reports gravity projected onto the sensor's X/Y/Z axes. A tilt angle needs two projected components, so the validation firmware exposes candidates named by the two accelerometer axes passed to `atan2`:

- `accelAngleAyAzDeg`: `atan2(ay, az)`, matching the Elegoo AVR pitch convention.
- `accelAngleAxAzDeg`: `atan2(ax, az)`, an alternate candidate for a different IMU mounting orientation.
- `accelAngleAxAyDeg`: `atan2(ax, ay)`, useful when gravity is mostly shared between X and Y.
- `accelTiltXDeg`: `atan2(ax, sqrt(ay^2 + az^2))`, a pitch-from-X candidate that compares X-axis gravity against the combined non-X gravity magnitude.

The `*SmoothedDeg` fields are low-pass filtered copies for hand validation. The non-smoothed fields remain available for debugging sensor noise and filter lag.

Current hand-test interpretation:

- `accelTiltXDeg` is the leading forward/back pitch candidate.
- `gyroYRateDps` is the forward/back pitch-rate candidate; it moves during forward/back rocking and changes polarity when tilt direction reverses.
- `accelAngleAyAzDeg` behaves like side-to-side roll on this mounting.
- `accelAngleAxAyDeg` wraps near upright and should remain a diagnostic signal, not a control input.

Selected validation path:

- `/balance/status` reports `accelPitchDeg` from `accelTiltXDeg`.
- `/balance/status` reports `gyroRateDps` from bias-corrected `gyroYRateDps`.
- The Kalman pitch estimate uses `accelTiltXDeg + gyroYRateDps` for validation only.
- `balanceMotorOutputEnabled` remains `false`; the selected pitch path is not yet allowed to drive motors.

## Balance Control Preview

The first controller milestone computes a proposed balance output without applying it to the motors.

- Elegoo's vertical-ring reference gains are `Kp=55.0`, `Ki=0.0`, `Kd=0.75`.
- Safer preview defaults are `Kp=5.0`, `Ki=0.0`, `Kd=0.15`, with output limited to `+/-120 pwm`.
- `/balance/config` exposes RAM-only runtime tuning for `kp`, `ki`, `kd`, `setpoint`, `limit`, and `maxAngle`.
- Runtime tuning is intentionally volatile; a board reset returns to compile-time defaults.
- The preview controller computes `P`, `I`, `D`, raw output, clamped output, and proposed left/right PWM.
- With `Ki=0.0`, the integral accumulator is held at zero to avoid hidden windup before integral tuning is deliberately enabled.
- Safety gates require a healthy sensor read, completed gyro calibration, and pitch within `+/-22 deg`.
- If a gate fails, the clamped output is forced to zero and `/balance/status` reports `balanceSafetyReason`.
- `BALANCE_MOTOR_OUTPUT_ENABLED` remains `0`; balance code still does not call motor drive methods.
- Manual `/motor/*` diagnostics remain separate from balance telemetry.

## Milestone Checklist

- [x] Preserve motor diagnostic bring-up work on a dedicated branch.
- [x] Import AVR-vs-ESP32 control-loop comparison notes.
- [x] Document source references and validation workflow.
- [x] Build and expose raw IMU telemetry.
- [x] Build and expose pitch/Kalman telemetry.
- [x] Validate `/i2c/scan`, `/imu/raw`, and `/balance/status` on hardware.
- [x] Record hardware validation results in `docs/balancing-experiments.md`.
- [x] Add preview-only balance PID/output telemetry with motor writes disabled.
- [ ] Only after validation: plan motor-output balance loop as a separate milestone.
