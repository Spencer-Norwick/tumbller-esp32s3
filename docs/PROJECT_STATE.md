# Project State

Last updated: 2026-08-12
Repository: `tumbller-esp32s3`
Branch: `feature/self-balancing-sensor-validation`
Validated source baseline: `fc781ca tools(balance): add tether tuning dashboard`
Last hardware-validated firmware commit: `8b97f2f feat(balance): add bounded arm run telemetry`

## Current Objective

Progress from validated wheels-up balance control to repeatable, bounded wheels-down overhead-tether trials. A successful increment keeps direction correct, remains inside the safety window, avoids repeated output saturation, auto-disarms on schedule, and produces enough telemetry to explain the result.

## Verified State

- The mounted forward/back pitch path uses X-axis tilt with gyro Y and has been calibrated and dynamically validated on the robot.
- Balance output is runtime-armed, starts disarmed after reset, and is protected by sensor, calibration, angle, PWM-limit, and timeout gates.
- Positive motor direction has been aligned with physical forward motion.
- Both single-channel encoders count physical wheel motion. Direction is inferred from commanded PWM rather than measured quadrature.
- Wheels-up tests validated signed speed-loop behavior, low-scale speed mixing, state reset/anti-windup behavior, and a 1.5-second timed arm with zero sampled saturation.
- The local dashboard provides runtime tuning, timed-arm profiles, telemetry traces, and immediate disarm/stop controls.

Detailed evidence is in `docs/balancing-experiments.md` and the operating procedure is in `docs/operator-guide.md`.

## Recent Progress

- Added the tether tuning dashboard and documented the overhead-tether workflow (`fc781ca`).
- Added bounded arm timeouts and run-level sample/saturation telemetry (`8b97f2f`).
- Reset speed-loop state on disarm, unsafe state, encoder reset, near-zero command, and direction change (`6ecea2f`).
- Enabled guarded runtime speed-loop mixing after successful wheels-up sign validation (`371c7cf`).

## Validation

| Check | Result | Evidence |
| --- | --- | --- |
| `platformio run` for `8b97f2f` | passed | Build, upload, and 1.5-second hardware run recorded on 2026-06-30. |
| Wheels-up timed arm | passed | 209 samples, automatic disarm, safety reason `ok`, saturation ratio `0.000`. |
| `./scripts/check.sh` at `fc781ca` | passed | PlatformIO build passed on 2026-08-12; RAM 14.7%, flash 25.8%. Dashboard Python compilation and `git diff --check` also passed. |
| Dashboard source comparison | passed | Workspace and repository copies had identical SHA-256 on 2026-08-12. |

## Active Problems

- Stable wheels-down self-balancing has not yet been demonstrated.
- The next physical step needs several repeatable small-tilt timed trials before advancing to a slack overhead tether.
- Encoder direction is inferred from output sign, so it cannot independently detect wheel reversal or external wheel motion.
- The firmware has no substantive PlatformIO unit tests yet; only build and hardware validation currently cover control behavior.

## Decisions and Constraints

- Tune the angle loop before enabling meaningful speed-loop contribution.
- Keep angle-loop integral at zero during initial tether tuning.
- Keep runtime tuning volatile so reset returns to conservative defaults.
- Use timed arms as the normal envelope for physical balance trials.
- Treat simulator/build evidence and physical hardware evidence as distinct validation levels.

## Next Action

1. Perform several controlled 1,000–1,500 ms wheels-up trials using small tilts and the dashboard's angle-only tether profile. Record saturation and direction evidence before the first wheels-down tether trial.
