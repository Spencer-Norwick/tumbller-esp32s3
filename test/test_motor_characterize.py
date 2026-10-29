import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from motor_characterize import Trial, device_elapsed_ms, parse_levels, reliable_threshold  # noqa: E402


def trial(pwm: int, pulses: int) -> Trial:
    return Trial("left", "forward", pwm, 1, 30 if pulses else None, pulses, pulses)


class MotorCharacterizeTest(unittest.TestCase):
    def test_device_elapsed_handles_millis_wrap(self) -> None:
        self.assertEqual(device_elapsed_ms(0xFFFFFFFE, 3), 5)

    def test_levels_are_sorted_and_deduplicated(self) -> None:
        self.assertEqual(parse_levels("40,20,40,30"), (20, 30, 40))

    def test_high_authority_level_is_accepted(self) -> None:
        self.assertEqual(parse_levels("160"), (160,))

    def test_threshold_requires_every_higher_level_to_move(self) -> None:
        cases = [trial(20, 8), trial(30, 0), trial(40, 23), trial(50, 39)]
        self.assertEqual(reliable_threshold(cases, minimum_pulses=2).pwm, 40)

    def test_threshold_is_missing_when_top_level_stalls(self) -> None:
        cases = [trial(40, 20), trial(50, 0)]
        self.assertIsNone(reliable_threshold(cases, minimum_pulses=2))


if __name__ == "__main__":
    unittest.main()
