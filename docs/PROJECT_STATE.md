# Project State

Last updated: 2026-08-12
Repository: `tumbller-esp32s3`
Branch: `feature/self-balancing-sensor-validation`
Validated source baseline: `cfb2f12 fix(balance): damp wheel speed in port mixer` plus the pending latest-command mailbox fix
Last hardware-validated firmware commit: pending latest-command mailbox fix built and uploaded on 2026-08-12

## Current Objective

Measure the motor plant before further controller tuning. Characterize per-side/per-direction PWM deadband and command-to-motion latency wheels-up, then use that evidence to choose the next inner angle-loop change.

## Verified State

- The mounted forward/back pitch path uses X-axis tilt with gyro Y and has been calibrated and dynamically validated on the robot.
- Balance output is runtime-armed, starts disarmed after reset, and is protected by sensor, calibration, angle, PWM-limit, and timeout gates.
- Positive motor direction has been aligned with physical forward motion.
- Both single-channel encoders count physical wheel motion. Direction is inferred from commanded PWM rather than measured quadrature.
- Wheels-up tests validated signed speed-loop behavior, low-scale speed mixing, state reset/anti-windup behavior, and a 1.5-second timed arm with zero sampled saturation.
- A five-second, 90-PWM wheels-up authority test produced clear, controlled correction in both directions with both encoders active and automatic shutdown.
- Corrected speed-loop mixing reduces same-direction drive instead of reinforcing it, verified in both directions during a five-second wheels-up run.
- Motor commands now use a one-element latest-value mailbox, eliminating FIFO command backlog and full-queue stop rejection. A ten-second wheels-up reversal test confirmed live command changes and automatic disarm, but the operator still observed substantial physical response delay.
- The local dashboard provides runtime tuning, timed-arm profiles, telemetry traces, and immediate disarm/stop controls.

Detailed evidence is in `docs/balancing-experiments.md` and the operating procedure is in `docs/operator-guide.md`.

## Recent Progress

- Replaced the motor command FIFO with a latest-value mailbox so stale corrections cannot accumulate (pending commit).
- Corrected the port-specific speed-mix feedback sign and added a host regression test (`cfb2f12`).
- Extended bounded tether trials to ten seconds (`ac79027`).
- Raised the arm ceiling to 90 PWM after controlled wheels-up validation and updated the tether profile to a human-observable five-second envelope (`3ae46da`).
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
| Five-second wheels-up authority test at `3ae46da` | passed | 698 samples, correct controlled bidirectional response, encoder totals left 656/right 835, zero sensor failures, timed disarm, and angle gate verified on 2026-08-12. |
| Corrected speed-mix wheels-up test at `cfb2f12` | passed | 694 samples; corrected feedback reduced same-direction drive in both directions with zero mixed-output saturation and zero sensor failures. |
| Latest-command mailbox wheels-up reversal test | passed with unresolved observation | Build/upload passed; ten-second test exercised both directions and angle-limit disarm, but visible response still felt delayed to the operator. |
| Dashboard source comparison | passed | Workspace and repository copies had identical SHA-256 on 2026-08-12. |

## Active Problems

- Stable wheels-down self-balancing has not yet been demonstrated.
- Motor deadband and command-to-first-motion latency have not been measured; perceived lag remains unexplained after eliminating queue backlog.
- Encoder direction is inferred from output sign, so it cannot independently detect wheel reversal or external wheel motion.
- The firmware has no substantive PlatformIO unit tests yet; only build and hardware validation currently cover control behavior.

## Decisions and Constraints

- Tune the angle loop before enabling meaningful speed-loop contribution.
- Keep angle-loop integral at zero during initial tether tuning.
- Keep runtime tuning volatile so reset returns to conservative defaults.
- Use a ten-second timed arm as the normal human-observable envelope for tethered trials; retain automatic timeout, angle gating, and explicit stop.
- Treat simulator/build evidence and physical hardware evidence as distinct validation levels.

## Next Action

1. Add and run an automated wheels-up motor characterization across PWM for the left/right forward/reverse cases.
2. Record command-application delay, first encoder response, pulse count, and practical startup threshold.
3. Use the measurements to revise the inner angle-loop actuation strategy before another wheels-down tether trial.
