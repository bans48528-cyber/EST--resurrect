from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "src"
TOOLS_ROOT = ROOT.parents[1] / "tools" / "est_hid_sender"


def section(source: str, start: str, end: str) -> str:
    return source.split(start, 1)[1].split(end, 1)[0]


class MotorStallDetectionTests(unittest.TestCase):
    def test_position_stall_is_state_not_completion_or_timeout(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        update = section(
            board,
            "static void update_position_control_port",
            "static void update_position_control",
        )
        detector = section(
            board,
            "static void update_position_stall",
            "static void update_position_settling",
        )

        self.assertNotIn("timeout_ms", update)
        self.assertNotIn("BOARD_MOTOR_POSITION_TIMEOUT", update)
        self.assertIn("control->stalled = true;", detector)
        self.assertNotIn("finish_position_control", detector)
        self.assertNotIn("motor_output_off", detector)

    def test_detector_requires_unfinished_position_low_speed_and_duration(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        detector = section(
            board,
            "static void update_position_stall",
            "static void update_position_settling",
        )

        self.assertIn("absolute_i32(position_error) <= position_tolerance", detector)
        self.assertIn("speed_measurement_valid", detector)
        self.assertIn("MOTOR_POSITION_STALL_TRACKING_PWM_X100", detector)
        self.assertIn("now_ms - control->stall_candidate_ms", detector)
        self.assertIn("MOTOR_POSITION_STALL_LARGE_MS 1000U", board)
        self.assertIn("MOTOR_POSITION_STALL_MEDIUM_MS 700U", board)
        self.assertIn("control->requested_speed == 0", detector)

    def test_status_is_only_true_while_position_control_remains_active(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        query = section(
            board,
            "bool board_motor_position_stalled",
            "struct board_motor_position_snapshot board_motor_position_snapshot",
        )
        stop = section(board, "void board_motor_stop(void)", "bool board_motor_test_start")

        self.assertIn("position_control_active(port)", query)
        self.assertIn("position_controls[(uint8_t)port].stalled", query)
        self.assertIn("position_controls[port_index].stalled = false;", stop)

    def test_service_micropython_and_frozen_runtime_expose_query(self) -> None:
        service = (SOURCE_DIR / "est_motor.c").read_text(encoding="utf-8")
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        runtime = (
            ROOT / "micropython_port" / "modules" / "est_runtime.py"
        ).read_text(encoding="utf-8")

        self.assertIn("est_result_t est_motor_stalled", service)
        self.assertIn("board_motor_position_stalled", service)
        self.assertIn("MP_QSTR_stalled", module)
        self.assertIn("est_motor_stalled(self->port, &stalled)", module)
        self.assertIn("def motor_stalled(port):", runtime)
        self.assertIn("motor(port).stalled()", runtime)
        self.assertIn("port = _motor_port(port)", runtime)

    def test_protocol_advertises_bit25(self) -> None:
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        constants = (TOOLS_ROOT / "constants.py").read_text(encoding="utf-8")
        cli = (TOOLS_ROOT / "cli.py").read_text(encoding="utf-8")

        self.assertIn("DEVICE_PROTOCOL_MINOR           28U", config)
        self.assertIn(
            "DEVICE_CAPABILITY_MOTOR_STALL_DETECTION (1UL << 25U)", config
        )
        self.assertIn("DEVICE_CAPABILITY_MOTOR_STALL_DETECTION", protocol)
        self.assertIn("DEVICE_PROTOCOL_MINOR = 28", constants)
        self.assertIn("DEVICE_CAPABILITY_MOTOR_STALL_DETECTION = 1 << 25", constants)
        self.assertIn('"motor-stall-detection"', cli)


if __name__ == "__main__":
    unittest.main()
