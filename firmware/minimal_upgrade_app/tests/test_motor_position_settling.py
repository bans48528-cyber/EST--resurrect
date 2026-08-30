from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BOARD_PATH = ROOT / "src" / "board_motor.c"


def section(source: str, start: str, end: str) -> str:
    return source.split(start, 1)[1].split(end, 1)[0]


def define(source: str, name: str) -> int:
    match = re.search(rf"^#define {name} (\d+)(?:U)?$", source, re.MULTILINE)
    if match is None:
        raise AssertionError(f"missing integer define {name}")
    return int(match.group(1))


class MotorPositionSettlingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.board = BOARD_PATH.read_text(encoding="utf-8")

    def test_single_motor_has_tracking_settling_and_stable_completion(self) -> None:
        update = section(
            self.board,
            "static void update_position_control_port",
            "static void update_position_control",
        )
        settling = section(
            self.board,
            "static void update_position_settling",
            "static void update_position_control_port",
        )

        self.assertIn("MOTOR_POSITION_PHASE_TRACKING", self.board)
        self.assertIn("MOTOR_POSITION_PHASE_SETTLING", self.board)
        self.assertIn("control->phase = MOTOR_POSITION_PHASE_SETTLING;", update)
        self.assertIn("control->requested_speed)) * 2;", update)
        self.assertIn("position_error * kp_x100 - velocity * kd_x100", settling)
        self.assertIn("MOTOR_POSITION_SETTLING_STABLE_SAMPLES", settling)
        self.assertIn("measured_speed_percent", settling)
        self.assertIn("position_tolerance", settling)
        self.assertNotIn("integral", settling.lower())
        self.assertEqual(
            define(self.board, "MOTOR_POSITION_SETTLING_STABLE_SAMPLES"), 8
        )

    def test_single_motor_does_not_complete_on_first_target_crossing(self) -> None:
        update = section(
            self.board,
            "static void update_position_control_port",
            "static void update_position_control",
        )
        first_completion = update.index("finish_position_control")
        pair_guard = update.index("if (pair_owned &&")
        settling_call = update.index("update_position_settling")

        self.assertGreater(first_completion, pair_guard)
        self.assertGreater(settling_call, first_completion)
        self.assertIn(
            "finish_position_control(port, now_ms,\n\t\t\tBOARD_MOTOR_POSITION_COMPLETE);",
            section(
                self.board,
                "static void update_position_settling",
                "static void update_position_control_port",
            ),
        )

    def test_tracking_reference_decelerates_continuously(self) -> None:
        tracking = section(
            self.board,
            "static int8_t motor_position_tracking_speed",
            "static void reset_motor_measurements",
        )
        self.assertIn("requested_magnitude * remaining", tracking)
        self.assertIn("motor_position_slowdown_degrees", tracking)
        self.assertNotIn("target_speed = control->requested_speed < 0", tracking)

        requested = 80
        slowdown = 140
        deceleration_window = slowdown * 2
        targets = [
            max(
                1,
                (requested * remaining + deceleration_window - 1)
                // deceleration_window,
            )
            for remaining in (280, 210, 140, 70, 1)
        ]
        self.assertEqual(targets, [80, 60, 40, 20, 1])
        self.assertEqual(targets, sorted(targets, reverse=True))

    def test_settling_limits_output_reverse_and_pwm_slew(self) -> None:
        settling = section(
            self.board,
            "static void update_position_settling",
            "static void update_position_control_port",
        )
        for token in (
            "MOTOR_POSITION_SETTLING_LARGE_MAX_PWM_X100",
            "MOTOR_POSITION_SETTLING_MEDIUM_MAX_PWM_X100",
            "MOTOR_POSITION_SETTLING_LARGE_REVERSE_MAX_PWM_X100",
            "MOTOR_POSITION_SETTLING_MEDIUM_REVERSE_MAX_PWM_X100",
            "MOTOR_POSITION_SETTLING_LARGE_PWM_SLEW_X100",
            "MOTOR_POSITION_SETTLING_MEDIUM_PWM_SLEW_X100",
            "position_settling_slew_pwm",
        ):
            self.assertIn(token, settling)

        slew = section(
            self.board,
            "static int32_t position_settling_slew_pwm",
            "static void apply_hold_pwm",
        )
        self.assertIn("MOTOR_POSITION_SETTLING_MEDIUM_RELEASE_SLEW_X100", slew)
        self.assertIn("adjusted = 0;", slew)
        self.assertEqual(
            define(self.board, "MOTOR_POSITION_SETTLING_MEDIUM_MAX_PWM_X100"),
            3000,
        )
        self.assertEqual(
            define(
                self.board,
                "MOTOR_POSITION_SETTLING_MEDIUM_REVERSE_MAX_PWM_X100",
            ),
            2000,
        )
        self.assertEqual(
            define(
                self.board,
                "MOTOR_POSITION_SETTLING_MEDIUM_POSITION_TOLERANCE_COUNTS",
            ),
            5,
        )

    def test_position_to_hold_handoff_preserves_target_and_output_state(self) -> None:
        handoff = section(
            self.board,
            "static bool handoff_position_to_hold",
            "static void motor_apply_completion_stop_mode",
        )
        finish = section(
            self.board,
            "static void finish_position_control",
            "static void update_position_settling",
        )

        self.assertIn("hold->target_count = position->target_count;", handoff)
        self.assertIn("hold->pwm_x100 = position->pwm_x100;", handoff)
        self.assertIn("hold->last_count = position->last_count;", handoff)
        self.assertNotIn("motor_output_off", handoff)
        self.assertIn("handoff_position_to_hold", finish)
        self.assertIn("if (!handed_off_to_hold)", finish)

    def test_medium_recovery_requires_large_error_and_true_stop(self) -> None:
        hold = section(
            self.board,
            "static void update_hold_control",
            "static void configure_pwm_channel",
        )
        settling = section(
            self.board,
            "static void update_position_settling",
            "static void update_position_control_port",
        )
        threshold = define(
            self.board, "MOTOR_HOLD_MEDIUM_RECOVERY_THRESHOLD_COUNTS"
        )

        self.assertGreater(
            threshold, define(self.board, "MOTOR_HOLD_MEDIUM_EXIT_DEADBAND_COUNTS")
        )
        self.assertIn(
            "absolute_error >= MOTOR_HOLD_MEDIUM_RECOVERY_THRESHOLD_COUNTS", hold
        )
        self.assertIn("velocity == 0", hold)
        self.assertIn("absolute_error >= recovery_threshold", settling)
        self.assertIn("MOTOR_POSITION_SETTLING_RECOVERY_VELOCITY_COUNTS", settling)
        self.assertIn("measured_speed_percent[(uint8_t)port] == 0", settling)
        self.assertIn("recovery_drive_samples", settling)
        self.assertIn("recovery_cooldown_samples", settling)
        self.assertIn(
            "MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_RAMP_X100", settling
        )
        self.assertEqual(
            define(
                self.board,
                "MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_DRIVE_SAMPLES",
            ),
            3,
        )
        self.assertEqual(
            define(
                self.board,
                "MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_COOLDOWN_SAMPLES",
            ),
            4,
        )
        self.assertGreater(
            define(
                self.board,
                "MOTOR_POSITION_SETTLING_MEDIUM_RECOVERY_THRESHOLD_COUNTS",
            ),
            define(
                self.board,
                "MOTOR_POSITION_SETTLING_MEDIUM_POSITION_TOLERANCE_COUNTS",
            ),
        )

    def test_pair_position_keeps_m113_tracking_branch(self) -> None:
        update = section(
            self.board,
            "static void update_position_control_port",
            "static void update_position_control",
        )
        pair_branch = update.split("if (pair_owned) {", 1)[1].split(
            "settling_entry =", 1
        )[0]

        self.assertIn("motor_position_slowdown_degrees", pair_branch)
        self.assertIn("MOTOR_POSITION_MIN_SPEED_PERCENT", pair_branch)
        self.assertIn("pair_adjust_position_speed", pair_branch)
        self.assertNotIn("update_position_settling", pair_branch)


if __name__ == "__main__":
    unittest.main()
