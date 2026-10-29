# Repository Engineering Rules

Apply the `reliable-software-delivery` skill when it is available. These repository rules remain mandatory when the skill is unavailable.

## Start Every Modifying Task

1. Read `docs/PROJECT_STATE.md` and the documentation relevant to the requested subsystem.
2. Inspect `git status --short --branch`, recent commits, and existing diffs.
3. Preserve unrelated work and identify the smallest independently verifiable increment.
4. Define the acceptance check before editing.
5. Run `./scripts/check.sh` as a baseline when the environment permits. Record any pre-existing failure.

## Change Discipline

- Keep each change self-contained and reversible, with one reason to exist.
- Separate refactoring from behavior changes unless they cannot be validated independently.
- Make the minimum coherent change; do not silently expand scope.
- Keep each completed commit buildable and understandable.
- Commit validated increments with outcome-oriented messages. Do not include unrelated files.
- Never discard user changes, rewrite history, force push, merge, or publish without explicit authorization.
- Never commit caches, local device configuration, credentials, private notes, or generated artifacts that are not required outputs.

## Validation

- Run focused checks while iterating and `./scripts/check.sh` before committing.
- Add PlatformIO unit tests for extracted hardware-independent logic when feasible.
- Treat a successful build as necessary but not equivalent to hardware validation.
- Report each check as passed, failed, skipped, or blocked. Never imply an unrun check passed.
- Inspect `git diff --check`, the staged diff, and final status before committing.

## Hardware Safety

- Build successfully before uploading firmware.
- Keep balance output disarmed by default and retain existing angle, sensor-health, PWM-limit, and timeout gates.
- Require an explicitly prepared physical test before motor actuation.
- Validate direction and low-output behavior wheels-up before wheels-down testing.
- Use bounded arm durations for balance trials and keep a physical stop/catch path ready.
- Stop immediately on wrong direction, repeated saturation, unsafe pitch, violent oscillation, power instability, or uncertain behavior.
- Record physical setup, firmware commit, configuration, observations, safety response, and next action in `docs/balancing-experiments.md`.

## Documentation and State

- Keep `README.md` focused on stable setup, architecture, and normal usage.
- Keep `docs/PROJECT_STATE.md` concise and current; update it after validated milestones, new blockers, or changed next actions.
- Update `docs/operator-guide.md` with repeatable operating procedures.
- Remove or correct stale checklist items when later evidence supersedes them.
- Keep private learning, interview, and agent-process notes outside the public repository.

## Definition of Done

Work is complete only when the requested behavior and acceptance criteria are met, relevant automated checks pass, required hardware evidence is recorded, documentation matches behavior, project state is current, and the change is committed without unrelated files.
