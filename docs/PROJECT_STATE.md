# Project State

Last updated: 2026-08-12
Repository: `tumbller-esp32s3`
Branch: `feature/self-balancing-sensor-validation`
Validated source baseline: `86af765 feat(balance): validate 160 pwm authority`
Last hardware-validated firmware commit: `86af765`, exercised wheels-up and wheels-down on 2026-08-12

## Current Objective

Add wheel-speed damping to stop the angle-only controller from racing between tether boundaries.

## Verified State

- The mounted forward/back pitch path uses X-axis tilt with gyro Y and has been calibrated and dynamically validated on the robot.
- Balance output is runtime-armed, starts disarmed after reset, and is protected by sensor, calibration, angle, PWM-limit, and timeout gates.
- Positive motor direction has been aligned with physical forward motion.
- Both single-channel encoders count physical wheel motion. Direction is inferred from commanded PWM rather than measured quadrature.
- Wheels-up tests validated signed speed-loop behavior, low-scale speed mixing, state reset/anti-windup behavior, and a 1.5-second timed arm with zero sampled saturation.
- A five-second, 90-PWM wheels-up authority test produced clear, controlled correction in both directions with both encoders active and automatic shutdown.
- Corrected speed-loop mixing reduces same-direction drive instead of reinforcing it, verified in both directions during a five-second wheels-up run.
- Motor commands now use a one-element latest-value mailbox, eliminating FIFO command backlog and full-queue stop rejection. A ten-second wheels-up reversal test confirmed live command changes and automatic disarm, but the operator still observed substantial physical response delay.
- Automated wheels-up characterization measured `0-6 ms` command-application lag and a reliable startup threshold of approximately `40 pwm` in all four side/direction cases. The prior `Kp=10` profile stayed below reliable motor effort until about `4 deg` of proportional error.
- An initial `Kp=35`, `Kd=0.5` wheels-up run commanded `+45 pwm` at `+1.13 deg`, reversed in both directions, completed 1390 samples, and timed out safely. A subsequent onboard-traced run provided the physical validation below.
- The onboard 20 ms trace captured 465 armed samples, 13 requested sign reversals, zero requested/applied PWM mismatches, at most 1 ms observed applied-command age, balanced encoder activity, corrected 79.8% saturation evidence during deliberate large rocking, and automatic stop. The operator reported the response was “a lot more responsive.”
- The first traced wheels-down responsive-profile run produced a growing `-5.7/+14.1/-27.9/+32.0/-44.4 deg` oscillation, saturated for 91.5% of 293 armed samples, and auto-disarmed at 6.25 seconds. Applied PWM followed requests within 1 ms after startup, proving the remaining slow catch is the `90 pwm` floor-load authority limit rather than software lag or reverted configuration.
- The `160 pwm` ceiling passed all four bounded wheel/direction cases with `0-4 ms` application lag and `29-41 ms` first motion. A five-second wheels-up controller run captured two reversals, zero requested/applied mismatches, balanced `2075/2039` encoder gains, and timed stop.
- The first `160 pwm` floor run at `Kp=35`, `Kd=0.5` auto-disarmed at the angle gate after approximately `2.87 s`; it saturated `349/400` samples (`87.3%`) and the operator observed abrupt tire slip and jerky motion. The trace confirms an overdriven, underdamped response rather than command delay.
- Reducing `Kp` to `20` lowered saturation to `66.4%` but still hit the angle gate after approximately `2.18 s`. The trace and operator observation show that the angle-only loop keeps racing while wheel speed accumulates, then reverses too late. Encoder gains were strongly asymmetric at about `1811/1202` despite identical commands.
- The local dashboard provides runtime tuning, timed-arm profiles, telemetry traces, and immediate disarm/stop controls.

Detailed evidence is in `docs/balancing-experiments.md` and the operating procedure is in `docs/operator-guide.md`.

## Recent Progress

- Added an onboard 20 ms balance trace and physically validated the measured responsive angle profile (`d82b4fb`).
- Corrected angle-loop saturation telemetry and installed the measured responsive dashboard profile (`3ac079b`).
- Added bounded signed-motor characterization and applied-output telemetry, then completed all 28 wheels-up cases (`59cf51b`).
- Replaced the motor command FIFO with a latest-value mailbox so stale corrections cannot accumulate (`45cef66`).
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
| Automated motor characterization | passed | 28 bounded wheels-up cases; application lag `0-6 ms`; all side/direction cases reliably active at `40 pwm`; final motor state stopped; invalid requests rejected. |
| High-rate responsive-profile run | passed wheels-up | 465 armed trace samples over 9991 ms; 13 reversals; zero requested/applied PWM mismatches; operator reported much more responsive behavior; timed stop captured. |

## Active Problems

- Stable wheels-down self-balancing has not yet been demonstrated.
- Angle-only control remains underdamped across the tested authority/gain range; it needs active wheel-speed damping rather than another proportional-gain adjustment.
- Floor-run left/right encoder activity is strongly asymmetric despite balanced wheels-up characterization, consistent with tire slip or unequal traction.
- Encoder direction is inferred from output sign, so it cannot independently detect wheel reversal or external wheel motion.
- Hardware-independent mixer and characterization logic have host regression tests; there are still no on-device PlatformIO unit tests.

## Decisions and Constraints

- Tune the angle loop before enabling meaningful speed-loop contribution.
- Keep angle-loop integral at zero during initial tether tuning.
- Keep runtime tuning volatile so reset returns to conservative defaults.
- Use a ten-second timed arm as the normal human-observable envelope for tethered trials; retain automatic timeout, angle gating, and explicit stop.
- Treat simulator/build evidence and physical hardware evidence as distinct validation levels.

## Next Action

1. Return the robot upright under the slack overhead tether and recalibrate stationary.
2. Keep `Kp=20`, `Ki=0`, `Kd=0.5`, limit `160 pwm`, and max angle `45 deg`; enable corrected speed mixing at `speedScale=0.20`.
3. Arm for `10000 ms`, then compare tether-boundary racing, reversal timing, peak growth, saturation, and left/right encoder activity against the angle-only trace.
