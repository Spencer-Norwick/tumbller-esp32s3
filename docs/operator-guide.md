# Tumbller ESP32S3 Operator Guide

This guide is for local firmware bring-up and balance-control validation.

## Repository

- Firmware repo: this repository.
- Working branch: `feature/self-balancing-sensor-validation`
- Local dashboard/proxy: `tools/rover_control.py` in this repo; `/Users/spencer/Desktop/Projects/yak_rover/rover_control.py` is the current working copy used during bring-up.
- Current robot URL: `http://192.168.4.53`
- Current USB upload port: `/dev/cu.usbmodem11201`

## Build And Upload

From the firmware repo:

```sh
platformio run
platformio run --target upload --upload-port /dev/cu.usbmodem11201
```

Use `platformio run` before each firmware milestone. Use upload only when the build passes.

## Dashboard

From this repo:

```sh
python3 tools/rover_control.py --rover-url http://192.168.4.53 serve --host 127.0.0.1 --port 8787
```

The older workspace copy also works and should be kept in sync while bring-up is active:

```sh
python3 /Users/spencer/Desktop/Projects/yak_rover/rover_control.py --rover-url http://192.168.4.53 serve --host 127.0.0.1 --port 8787
```

Open:

```text
http://127.0.0.1:8787
```

The dashboard reads the robot over WiFi and serves a local browser UI. USB is used for flashing and optional serial telemetry, not for the HTTP dashboard.

## Direct Endpoint Checks

```sh
curl http://192.168.4.53/info
curl http://192.168.4.53/i2c/scan
curl http://192.168.4.53/imu/raw
curl http://192.168.4.53/balance/status
curl http://192.168.4.53/encoder/status
```

Calibration requires the robot to be stationary:

```sh
curl http://192.168.4.53/balance/calibrate
```

For bounded motor tests, add `ms` to the arm request, up to the firmware maximum runtime:

```sh
curl 'http://192.168.4.53/balance/arm?ms=1500'
```

Stop and disarm:

```sh
curl http://192.168.4.53/balance/disarm
curl http://192.168.4.53/motor/stop
```

## What Runs Where

- `src/tasks/balance_task.cpp`: IMU reads, pitch estimate, preview balance PID, speed-loop preview, balance arm/disarm behavior.
- `src/tasks/motor_task.cpp`: motor command queue, signed motor drive, encoder sampling.
- `src/tasks/server_task.cpp`: HTTP endpoints served by the ESP32.
- `src/config.hpp`: compile-time pins, loop timing, default gains, and safety limits.
- `tools/rover_control.py`: local dashboard and HTTP proxy for hands-on testing.

## Safe Test Order

1. Build with `platformio run`.
2. Upload over USB.
3. Confirm `/i2c/scan` shows `0x44` and `0x68`.
4. Confirm `/imu/raw` has live accel/gyro values.
5. Keep robot still and call `/balance/calibrate`.
6. Confirm `/balance/status` has `lastReadOk=true`, `calibrated=true`, and `failedReadCount=0`.
7. For motor tests, put the robot wheels-up and keep fingers clear.
8. Use Space or Escape in the dashboard to disarm and stop.

## Wheels-Up Speed Sign Check

1. Put the robot wheels-up, keep it stationary, and reset encoders.
2. Calibrate, then use `/balance/config?limit=45` before arming.
3. Arm balance output and gently pitch the chassis forward/back so the wheels visibly respond.
4. Watch `/balance/status`: `speedLoopDirectionSign` should follow `speedLoopSignedCommand`, and `speedLoopDeltaLeft` / `speedLoopDeltaRight` should flip sign when the balance command flips sign.
5. Disarm immediately after the sign check.

## Wheels-Up Speed Mix Check

1. Complete the speed sign check first.
2. Keep the robot wheels-up and near upright, then set `/balance/config?limit=45&speedMix=1&speedScale=0.10`.
3. Arm with a timeout, for example `/balance/arm?ms=1500`, and use small tilts only; watch `balanceMixedOutputClamped`, `speedLoopOutput`, `speedLoopMixScale`, `speedLoopResetReason`, and `balanceMixedOutputSaturationRatio`.
4. Disarm immediately if pitch approaches the safe angle window or the mixed output saturates repeatedly.

## Overhead Tether Balance Trials

Start with the dashboard, not raw curl, so the same screen controls the arm timeout, gains, stop command, and readout.

1. Rig the tether so it catches the body before a fall, but leaves the wheels touching the floor freely. Keep the tether slack at upright.
2. Open the dashboard, press `DISARM / STOP`, then `Calibrate Gyro` while the robot is stationary and upright.
3. Press `Angle-only Tether`. This applies `Kp=5.0`, `Ki=0.0`, `Kd=0.15`, `limit=45`, `maxAngle=18`, `speedMix=off`, and `Arm ms=1500`.
4. Press `Arm Timed` and watch the robot, not just the numbers. Classify the run as: correct-direction catch, wrong-direction fall, underpowered fall, oscillation, or saturation.
5. After each run, check `Run saturation`, `Mixed output`, `Safety reason`, `Arm timer`, and `Trace safety samples`.
6. Tune angle loop first. If it falls the correct direction but weakly, increase `Kp` or `limit` slightly. If it snaps/oscillates, lower `Kp` or raise `Kd` slightly. Keep `Ki=0` for these trials.
7. Use `Low Speed Mix` only after angle-only behavior pushes the right way. Keep `speedScale=0.05` until saturation is rare and the physical behavior is understandable.

Stop criteria for a trial: wrong motor direction, repeated saturation, safety blocked, violent oscillation, tether carrying most of the weight, or uncertainty about what happened.

## Editing Firmware

For a new telemetry variable:

1. Add the field to the relevant telemetry struct.
2. Update the producer task that fills the field.
3. Add the field to the HTTP JSON response in `server_task.cpp`.
4. Add dashboard display only after the endpoint returns valid JSON.
5. Build, upload, and record the result in `docs/balancing-experiments.md`.

For a control change:

1. Add telemetry first.
2. Prove sign and magnitude with motors disabled or wheels-up.
3. Add safety gates before any motor write.
4. Keep runtime defaults conservative.
5. Document the experiment and next action before tuning further.
