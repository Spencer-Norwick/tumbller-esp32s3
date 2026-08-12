#!/usr/bin/env python3
"""Measure wheels-up motor deadband and command-to-encoder response latency."""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import asdict, dataclass
from typing import Any


DEFAULT_ROVER_URL = os.environ.get("YAKROVER_URL", "http://192.168.4.53").rstrip("/")
DEFAULT_PWM_LEVELS = (20, 30, 40, 50, 60, 75, 90)


@dataclass
class Trial:
    side: str
    direction: str
    pwm: int
    apply_lag_ms: int | None
    first_pulse_ms: int | None
    driven_pulses: int
    settled_pulses: int


def get_json(base_url: str, path: str, timeout: float = 2.0) -> dict[str, Any]:
    request = urllib.request.Request(base_url + path, headers={"Connection": "close"})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return json.load(response)
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"{path}: HTTP {exc.code}: {detail}") from exc
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError(f"{path}: rover request failed: {exc}") from exc


def get_ack(base_url: str, path: str, timeout: float = 2.0) -> None:
    request = urllib.request.Request(base_url + path, headers={"Connection": "close"})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            response.read()
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"{path}: HTTP {exc.code}: {detail}") from exc
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError(f"{path}: rover request failed: {exc}") from exc


def device_elapsed_ms(start_ms: int, end_ms: int) -> int:
    return (end_ms - start_ms) & 0xFFFFFFFF


def wait_for_stop(base_url: str, timeout_s: float = 1.5) -> dict[str, Any]:
    deadline = time.monotonic() + timeout_s
    latest: dict[str, Any] = {}
    while time.monotonic() < deadline:
        latest = get_json(base_url, "/motor/measurement")
        if not latest["running"] and latest["leftPwm"] == 0 and latest["rightPwm"] == 0:
            return latest
        time.sleep(0.01)
    raise RuntimeError(f"motor did not report stopped state: {latest}")


def run_trial(base_url: str, side: str, signed_pwm: int, duration_ms: int, settle_ms: int) -> Trial:
    get_ack(base_url, "/motor/stop")
    wait_for_stop(base_url)
    time.sleep(settle_ms / 1000.0)
    get_json(base_url, "/encoder/reset")
    baseline = get_json(base_url, "/motor/measurement")

    left_pwm = signed_pwm if side == "left" else 0
    right_pwm = signed_pwm if side == "right" else 0
    query = urllib.parse.urlencode({"left": left_pwm, "right": right_pwm, "ms": duration_ms})
    accepted = get_json(base_url, f"/motor/test/signed?{query}")
    if not accepted.get("queued"):
        raise RuntimeError(f"motor command was not queued: {accepted}")

    expected = (left_pwm, right_pwm)
    applied_at_ms: int | None = None
    apply_lag_ms: int | None = None
    first_pulse_ms: int | None = None
    driven_pulses = 0
    deadline = time.monotonic() + duration_ms / 1000.0 + 1.0

    while time.monotonic() < deadline:
        measured = get_json(base_url, "/motor/measurement")
        measured_pwm = (int(measured["leftPwm"]), int(measured["rightPwm"]))
        if (
            applied_at_ms is None
            and int(measured["sequence"]) > int(baseline["sequence"])
            and measured_pwm == expected
        ):
            applied_at_ms = int(measured["appliedAtMs"])
            apply_lag_ms = device_elapsed_ms(int(accepted["queuedAtMs"]), applied_at_ms)

        pulses = int(measured["encoderTotalLeft"] if side == "left" else measured["encoderTotalRight"])
        if applied_at_ms is not None and first_pulse_ms is None and pulses > 0:
            first_pulse_ms = device_elapsed_ms(applied_at_ms, int(measured["servedAtMs"]))

        if applied_at_ms is not None and measured_pwm == (0, 0) and not measured["running"]:
            driven_pulses = pulses
            break
        time.sleep(0.01)
    else:
        raise RuntimeError(f"timed command did not finish for {side} at {signed_pwm} PWM")

    time.sleep(settle_ms / 1000.0)
    settled = get_json(base_url, "/motor/measurement")
    settled_pulses = int(settled["encoderTotalLeft"] if side == "left" else settled["encoderTotalRight"])
    return Trial(
        side=side,
        direction="forward" if signed_pwm > 0 else "reverse",
        pwm=abs(signed_pwm),
        apply_lag_ms=apply_lag_ms,
        first_pulse_ms=first_pulse_ms,
        driven_pulses=driven_pulses,
        settled_pulses=settled_pulses,
    )


def print_trial(trial: Trial) -> None:
    apply_text = "--" if trial.apply_lag_ms is None else str(trial.apply_lag_ms)
    pulse_text = "--" if trial.first_pulse_ms is None else str(trial.first_pulse_ms)
    print(
        f"{trial.side:5} {trial.direction:7} {trial.pwm:3} PWM | "
        f"apply {apply_text:>3} ms | first pulse {pulse_text:>3} ms | "
        f"pulses {trial.driven_pulses:4} drive / {trial.settled_pulses:4} settled",
        flush=True,
    )


def reliable_threshold(cases: list[Trial], minimum_pulses: int) -> Trial | None:
    ordered = sorted(cases, key=lambda trial: trial.pwm)
    for index, trial in enumerate(ordered):
        if all(candidate.driven_pulses >= minimum_pulses for candidate in ordered[index:]):
            return trial
    return None


def print_summary(trials: list[Trial], minimum_pulses: int) -> None:
    print("\nReliable startup thresholds", flush=True)
    for side in ("left", "right"):
        for direction in ("forward", "reverse"):
            cases = [trial for trial in trials if trial.side == side and trial.direction == direction]
            threshold = reliable_threshold(cases, minimum_pulses)
            if threshold is None:
                print(f"  {side:5} {direction:7}: not reached")
            else:
                print(
                    f"  {side:5} {direction:7}: {threshold.pwm} PWM "
                    f"({threshold.first_pulse_ms} ms to first observed pulse, "
                    f"{threshold.driven_pulses} driven pulses)"
                )


def parse_levels(value: str) -> tuple[int, ...]:
    try:
        levels = tuple(sorted({int(item.strip()) for item in value.split(",") if item.strip()}))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("PWM levels must be comma-separated integers") from exc
    if not levels or levels[0] < 1 or levels[-1] > 90:
        raise argparse.ArgumentTypeError("PWM levels must be between 1 and 90")
    return levels


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rover-url", default=DEFAULT_ROVER_URL)
    parser.add_argument("--levels", type=parse_levels, default=DEFAULT_PWM_LEVELS)
    parser.add_argument("--duration-ms", type=int, default=450, choices=range(100, 601))
    parser.add_argument("--settle-ms", type=int, default=150, choices=range(50, 1001))
    parser.add_argument("--minimum-pulses", type=int, default=2)
    parser.add_argument("--json", dest="json_path")
    parser.add_argument(
        "--wheels-up",
        action="store_true",
        help="required acknowledgement that both wheels are clear of the surface",
    )
    args = parser.parse_args()

    if not args.wheels_up:
        parser.error("--wheels-up is required; this procedure drives one wheel at a time")

    base_url = args.rover_url.rstrip("/")
    get_json(base_url, "/balance/disarm")
    get_ack(base_url, "/motor/stop")
    wait_for_stop(base_url)

    print(
        f"Characterizing {base_url}: PWM {','.join(map(str, args.levels))}; "
        f"{args.duration_ms} ms pulses; one wheel at a time",
        flush=True,
    )
    trials: list[Trial] = []
    try:
        for side in ("left", "right"):
            for direction_sign in (1, -1):
                for pwm in args.levels:
                    trial = run_trial(base_url, side, direction_sign * pwm, args.duration_ms, args.settle_ms)
                    trials.append(trial)
                    print_trial(trial)
    finally:
        try:
            get_ack(base_url, "/motor/stop")
        except RuntimeError as exc:
            print(f"WARNING: final stop request failed: {exc}", file=sys.stderr)

    print_summary(trials, args.minimum_pulses)
    if args.json_path:
        report = {
            "roverUrl": base_url,
            "durationMs": args.duration_ms,
            "settleMs": args.settle_ms,
            "minimumPulses": args.minimum_pulses,
            "trials": [asdict(trial) for trial in trials],
        }
        with open(args.json_path, "w", encoding="utf-8") as output:
            json.dump(report, output, indent=2)
            output.write("\n")
        print(f"\nJSON report: {args.json_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
