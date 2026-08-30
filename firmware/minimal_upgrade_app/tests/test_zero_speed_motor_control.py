from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "src"


def section(source: str, start: str, end: str) -> str:
    return source.split(start, 1)[1].split(end, 1)[0]


class ZeroSpeedMotorControlTests(unittest.TestCase):
    def test_zero_speed_is_accepted_by_every_firmware_layer(self) -> None:
        runtime = (ROOT / "micropython_port" / "modules" / "est_runtime.py").read_text(
            encoding="utf-8"
        )
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        service = (SOURCE_DIR / "est_motor.c").read_text(encoding="utf-8")
        drive = (SOURCE_DIR / "est_drive.c").read_text(encoding="utf-8")
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")

        speed_helper = section(runtime, "def _speed_magnitude", "def _stop_mode")
        self.assertIn("if value > 100:", speed_helper)
        self.assertNotIn("value < 1", speed_helper)

        motor_run_speed = section(
            module, "static mp_obj_t modest_motor_run_speed", "static mp_obj_t modest_motor_run_time"
        )
        self.assertNotIn("speed == 0", motor_run_speed)
        self.assertIn("speed must be -100..100", motor_run_speed)

        service_run_speed = section(
            service, "est_result_t est_motor_run_speed", "est_result_t est_motor_run_time"
        )
        self.assertNotIn("magnitude < 1", service_run_speed)

        pair_service = section(
            drive,
            "est_result_t est_motor_pair_run_speeds",
            "est_result_t est_motor_pair_run_speeds_for_time",
        )
        self.assertNotIn("left_speed_percent == 0", pair_service)
        self.assertNotIn("right_speed_percent == 0", pair_service)

        board_start = section(
            board, "bool board_motor_start_speed", "bool board_motor_start_speed_for_time"
        )
        closed_loop = section(
            board, "static void apply_closed_loop_speed", "static void update_speed_control"
        )
        self.assertNotIn("speed_percent == 0", board_start)
        self.assertIn("magnitude != 0U && magnitude < MOTOR_SPEED_MIN_PERCENT", board_start)
        self.assertIn("control->requested_speed = speed_percent;", board_start)
        self.assertIn("if (target_speed == 0)", closed_loop)
        self.assertIn("measured_speed_percent[(uint8_t)port] == 0", closed_loop)
        self.assertIn("speed_error * 8", closed_loop)

    def test_zero_position_waits_for_stop_without_timeout_or_hold(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        start = section(
            board, "bool board_motor_start_position", "bool board_motor_stop_position"
        )
        update = section(
            board, "static void update_position_control_port", "static void update_position_control"
        )

        self.assertIn("speed_percent > 100U", start)
        self.assertIn("control->requested_speed = degrees < 0", start)
        self.assertNotIn("timeout_ms", start)
        zero_branch = update.split("if (control->requested_speed == 0)", 1)[1].split(
            "pair_owned = pair_position_owns_port(port);", 1
        )[0]
        self.assertIn("apply_closed_loop_speed(port, 0", zero_branch)
        self.assertIn("return;", zero_branch)
        self.assertNotIn("finish_position_control", zero_branch)
        self.assertNotIn("motor_output_high_push_pull_stop", zero_branch)
        self.assertNotIn("update_position_stall", zero_branch)

    def test_pair_zero_side_bypasses_sync_and_supports_live_retarget(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        pair_start = section(
            board, "static bool start_pair_speed", "bool board_motor_start_pair_speed"
        )
        correction = section(
            board, "static void update_pair_speed_correction", "static int8_t pair_adjust_continuous_speed"
        )
        adjustment = section(
            board, "static int8_t pair_adjust_continuous_speed", "static void apply_closed_loop_speed"
        )

        self.assertNotIn("left_speed_percent == 0", pair_start)
        self.assertNotIn("right_speed_percent == 0", pair_start)
        self.assertIn(
            "pair_speed_control.state == BOARD_MOTOR_PAIR_SPEED_RUNNING", pair_start
        )
        self.assertIn(
            "speed_controls[(uint8_t)left_port].requested_speed =", pair_start
        )
        self.assertIn(
            "speed_controls[(uint8_t)right_port].requested_speed =", pair_start
        )
        self.assertIn("pair_speed_target_magnitude", correction)
        self.assertIn("pair_speed_target_magnitude", adjustment)
        self.assertIn("leader_port = BOARD_MOTOR_PORT_COUNT", correction)

    def test_timed_zero_speed_uses_elapsed_time_and_all_program_exits_clean(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        vm = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        system = (SOURCE_DIR / "est_system.c").read_text(encoding="utf-8")
        timed = section(
            board, "static void update_timed_speed_state", "static void record_pair_speed_error"
        )
        stop = section(
            vm, "est_result_t est_micropython_program_stop", "est_result_t est_micropython_program_clear"
        )

        self.assertIn("control->elapsed_ms = now_ms - control->started_ms;", timed)
        self.assertIn("control->elapsed_ms < control->duration_ms", timed)
        self.assertNotIn("requested_speed == 0", timed)
        self.assertIn("(void)est_motor_stop_all(EST_STOP_COAST);", stop)
        self.assertIn("(void)est_motor_stop_all(EST_STOP_COAST);", vm)
        self.assertIn("program_cleanup_after_failure();", vm)
        self.assertIn("return est_system_emergency_stop();", system)

    def test_capability_is_advertised_without_changing_open_loop_power(self) -> None:
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        run_power = section(
            module, "static mp_obj_t modest_motor_run_power", "static mp_obj_t modest_motor_run_speed"
        )

        self.assertIn("DEVICE_CAPABILITY_ZERO_SPEED_MOTOR_CONTROL (1UL << 20U)", config)
        self.assertIn("DEVICE_CAPABILITY_ZERO_SPEED_MOTOR_CONTROL", protocol)
        self.assertIn("est_motor_set_power", run_power)
        self.assertIn("power must be -100..100", run_power)


if __name__ == "__main__":
    unittest.main()
