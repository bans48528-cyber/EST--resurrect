from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "src"


def section(source: str, start: str, end: str) -> str:
    return source.split(start, 1)[1].split(end, 1)[0]


class HoldPositionControlTests(unittest.TestCase):
    def test_hold_is_independent_background_position_control(self) -> None:
        header = (ROOT / "include" / "board_motor.h").read_text(encoding="utf-8")
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")

        self.assertIn("BOARD_MOTOR_HOLD_HOLDING", header)
        self.assertIn("BOARD_MOTOR_STOP_HOLD_POSITION", header)
        self.assertIn("struct motor_hold_control", board)
        self.assertIn("hold_controls[BOARD_MOTOR_PORT_COUNT]", board)
        self.assertIn("update_hold_control(now_ms);", board)
        self.assertNotIn("timeout", section(board, "struct motor_hold_control", "struct motor_pair_position_control"))

    def test_explicit_timed_and_angle_hold_choose_the_required_targets(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")

        stop_position = section(
            board, "bool board_motor_stop_position", "bool board_motor_position_snapshot_for_port"
        )
        timed_finish = section(
            board, "static void update_timed_speed_state", "static void record_pair_speed_error"
        )
        position_finish = section(
            board, "static void finish_position_control", "static void update_position_control_port"
        )

        self.assertIn("tacho_count[(uint8_t)port]", stop_position)
        self.assertIn("tacho_count[port_index]", timed_finish)
        self.assertIn("control->target_count", position_finish)
        self.assertIn("BOARD_MOTOR_STOP_HOLD_POSITION", position_finish)

    def test_pd_control_has_type_profiles_hysteresis_limits_and_no_integral(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        hold = section(board, "static void update_hold_control", "static void configure_pwm_channel")

        for token in (
            "MOTOR_HOLD_LARGE_KP_X100",
            "MOTOR_HOLD_LARGE_KD_X100",
            "MOTOR_HOLD_MEDIUM_KP_X100",
            "MOTOR_HOLD_MEDIUM_KD_X100",
            "MOTOR_HOLD_LARGE_MAX_PWM_X100",
            "MOTOR_HOLD_MEDIUM_MAX_PWM_X100",
            "MOTOR_HOLD_MEDIUM_MIN_RECOVERY_PWM_X100",
            "enter_deadband",
            "exit_deadband",
            "hold_slew_pwm",
        ):
            self.assertIn(token, hold)
        self.assertIn("error * kp_x100 - velocity * kd_x100", hold)
        self.assertNotIn("integral", hold.lower())

    def test_new_commands_cancel_hold_and_passive_stops_release_it(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")

        set_power = section(board, "bool board_motor_set_power", "bool board_motor_coast")
        start_position = section(
            board, "bool board_motor_start_position", "bool board_motor_stop_position"
        )
        start_speed = section(
            board, "bool board_motor_start_speed", "bool board_motor_start_speed_for_time"
        )
        coast = section(board, "bool board_motor_coast", "bool board_motor_brake")
        brake = section(board, "bool board_motor_brake", "bool board_motor_reset_tacho")

        self.assertIn("cancel_hold_control(port, false);", set_power)
        self.assertIn("cancel_hold_control(port, false);", start_position)
        self.assertIn("cancel_hold_control(port, false);", start_speed)
        self.assertIn("cancel_hold_control(port, true);", coast)
        self.assertIn("cancel_hold_control(port, false);", brake)

    def test_disconnect_and_all_safety_cleanup_paths_release_hold(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        vm = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        system = (SOURCE_DIR / "est_system.c").read_text(encoding="utf-8")
        hold = section(board, "static void update_hold_control", "static void configure_pwm_channel")
        stop_all = section(board, "void board_motor_stop(void)", "void board_motor_tick")

        self.assertIn("MOTOR_HOLD_DISCONNECT_SAMPLES", hold)
        self.assertIn("BOARD_MOTOR_TYPE_NONE", hold)
        self.assertIn("cancel_hold_control(port, true);", hold)
        self.assertIn("hold_controls[port_index].state = BOARD_MOTOR_HOLD_IDLE", stop_all)
        self.assertIn("est_motor_stop_all(EST_STOP_COAST)", vm)
        self.assertIn("return est_system_emergency_stop();", system)

    def test_single_motor_service_and_python_layers_accept_hold(self) -> None:
        service = (SOURCE_DIR / "est_motor.c").read_text(encoding="utf-8")
        module = (ROOT / "micropython_port" / "modest.c").read_text(encoding="utf-8")
        runtime = (ROOT / "micropython_port" / "modules" / "est_runtime.py").read_text(
            encoding="utf-8"
        )

        self.assertIn("BOARD_MOTOR_STOP_HOLD_POSITION", service)
        self.assertIn("EST_MOTOR_HOLDING", service)
        motor_api = section(module, "static mp_obj_t modest_motor_stop", "static mp_obj_t modest_motor_run_power")
        angle_api = section(module, "static mp_obj_t modest_motor_run_angle", "static mp_obj_t modest_motor_reset_angle")
        self.assertIn("EST_STOP_HOLD", motor_api)
        self.assertIn("EST_STOP_HOLD", angle_api)
        self.assertIn("_HOLD = 2", runtime)
        self.assertIn('if action == "hold":\n        return _HOLD', runtime)

    def test_pair_completion_holds_each_motor_at_the_required_target(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")

        pair_position = section(
            board,
            "bool board_motor_start_pair_position",
            "struct board_motor_pair_position_snapshot",
        )
        pair_speed_finish = section(
            board, "static void finish_pair_speed", "static void update_pair_speed_state"
        )

        self.assertIn("enum board_motor_stop_mode stop_mode", pair_position)
        self.assertIn("left_speed_percent, left_degrees, stop_mode", pair_position)
        self.assertIn("right_speed_percent, right_degrees, stop_mode", pair_position)
        self.assertIn("motor_apply_completion_stop_mode", pair_position)
        self.assertIn("tacho_count[(uint8_t)pair_position_control.left_port]", pair_position)
        self.assertIn("tacho_count[(uint8_t)pair_position_control.right_port]", pair_position)
        self.assertIn("motor_apply_completion_stop_mode", pair_speed_finish)
        self.assertIn("tacho_count[(uint8_t)pair_speed_control.left_port]", pair_speed_finish)
        self.assertIn("tacho_count[(uint8_t)pair_speed_control.right_port]", pair_speed_finish)

    def test_pair_takeover_releases_old_holds_before_busy_checks(self) -> None:
        board = (SOURCE_DIR / "board_motor.c").read_text(encoding="utf-8")
        pair_position = section(
            board,
            "bool board_motor_start_pair_position",
            "bool board_motor_stop_pair_position",
        )
        pair_speed = section(
            board, "static bool start_pair_speed", "bool board_motor_start_pair_speed"
        )

        for command in (pair_position, pair_speed):
            left_cancel = command.index("cancel_hold_control(left_port, false);")
            right_cancel = command.index("cancel_hold_control(right_port, false);")
            busy_check = command.index(
                "port_control_active(left_port) || port_control_active(right_port)"
            )
            self.assertLess(left_cancel, busy_check)
            self.assertLess(right_cancel, busy_check)

        self.assertIn(
            "pair_speed_control.state = BOARD_MOTOR_PAIR_SPEED_IDLE;",
            pair_position,
        )
        self.assertIn(
            "pair_position_control.state = BOARD_MOTOR_PAIR_POSITION_IDLE;",
            pair_speed,
        )

    def test_pair_drive_and_python_layers_accept_hold(self) -> None:
        service = (SOURCE_DIR / "est_drive.c").read_text(encoding="utf-8")
        module = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )

        self.assertIn("return BOARD_MOTOR_STOP_HOLD_POSITION;", service)
        self.assertNotIn("stop_mode == EST_STOP_HOLD", section(
            service,
            "est_result_t est_motor_pair_run_speeds_for_time",
            "est_result_t est_motor_pair_stop",
        ))
        self.assertNotIn("EST_ERR_NOT_SUPPORTED", section(
            service,
            "est_result_t est_motor_pair_run_angles",
            "est_result_t est_motor_pair_run_speeds",
        ))
        pair_api = section(
            module,
            "static mp_obj_t modest_pair_stop",
            "static MP_DEFINE_CONST_DICT(\n\tmodest_motor_pair_locals",
        )
        drive_api = section(
            module,
            "static mp_obj_t modest_drive_straight_angle",
            "static MP_DEFINE_CONST_DICT(\n\tmodest_drive_base_locals",
        )
        self.assertIn("EST_STOP_HOLD", pair_api)
        self.assertIn("MP_QSTR_STOP_HOLD", pair_api)
        self.assertIn("EST_STOP_HOLD", drive_api)
        self.assertIn("MP_QSTR_STOP_HOLD", drive_api)

    def test_protocol_advertises_complete_hold_support(self) -> None:
        config = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(encoding="utf-8")
        tool_constants = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "constants.py"
        ).read_text(encoding="utf-8")
        tool_cli = (
            ROOT.parents[1] / "tools" / "est_hid_sender" / "cli.py"
        ).read_text(encoding="utf-8")

        self.assertIn("DEVICE_PROTOCOL_MINOR           22U", config)
        self.assertIn("DEVICE_CAPABILITY_HOLD_POSITION_CONTROL (1UL << 21U)", config)
        self.assertIn("DEVICE_CAPABILITY_HOLD_POSITION_CONTROL", protocol)
        self.assertIn("DEVICE_PROTOCOL_MINOR = 22", tool_constants)
        self.assertIn("DEVICE_CAPABILITY_HOLD_POSITION_CONTROL = 1 << 21", tool_constants)
        self.assertIn('"hold-position-control"', tool_cli)


if __name__ == "__main__":
    unittest.main()
