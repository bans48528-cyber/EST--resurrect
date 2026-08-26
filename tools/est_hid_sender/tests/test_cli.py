from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent))

from est_hid_sender import cli  # noqa: E402
from test_firmware import write_package_with_manifest  # noqa: E402
from test_protocol import FakeTransport  # noqa: E402


class CliTests(unittest.TestCase):
    def test_info_and_verify_do_not_open_device(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            package_path = write_package_with_manifest(Path(temp))
            output = io.StringIO()
            with mock.patch.object(cli.HidTransport, "open") as open_device:
                with contextlib.redirect_stdout(output):
                    self.assertEqual(cli.main(["info", "--file", str(package_path)]), 0)
                    self.assertEqual(cli.main(["verify", "--file", str(package_path)]), 0)
            open_device.assert_not_called()
            self.assertIn("manifest_status=verified", output.getvalue())

    def test_keys_prints_pressed_button_names(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(key_mask=0x21)
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["keys"]), 0)
        self.assertIn("current_version=M0.19A", output.getvalue())
        self.assertIn("pressed=KEY0,KEY5 mask=0x21", output.getvalue())

    def test_device_status_prints_complete_machine_snapshot(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["device-status"]), 0)
        text = output.getvalue()
        self.assertIn("firmware=M0.52A", text)
        self.assertIn("protocol=1.0", text)
        self.assertIn("firmware-update,motor-control,motor-tacho", text)
        self.assertIn("battery_level=4/4", text)
        self.assertIn("battery_percent=100", text)
        self.assertIn("motor_A=state:coast power:0 tacho:12", text)
        self.assertIn("motor_C=state:drive power:-30 tacho:-456", text)
        self.assertIn("input_1=state:streaming model:EST/EV3-color", text)
        self.assertIn("input_3=state:streaming model:EST-temperature", text)

    def test_flash_id_identifies_w25q64(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(jedec_id=bytes.fromhex("EF4017"))
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["flash-id"]), 0)
        self.assertIn("current_version=M0.19A", output.getvalue())
        self.assertIn("jedec_id=EF4017", output.getvalue())
        self.assertIn("model=W25Q64", output.getvalue())
        self.assertIn("capacity_bytes=8388608", output.getvalue())
        self.assertIn("model_known=yes", output.getvalue())

    def test_flash_id_identifies_w25q256jv(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(jedec_id=bytes.fromhex("EF4019"))
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["flash-id"]), 0)
        self.assertIn("jedec_id=EF4019", output.getvalue())
        self.assertIn("manufacturer=Winbond", output.getvalue())
        self.assertIn("model=W25Q256JV", output.getvalue())
        self.assertIn("capacity_bytes=33554432", output.getvalue())
        self.assertIn("model_known=yes", output.getvalue())

    def test_flash_scan_is_read_only_and_prints_empty_sector(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(jedec_id=bytes.fromhex("EF4019"))
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["flash-scan"]), 0)
        self.assertIn("test_address=0x01FFF000", output.getvalue())
        self.assertIn("device_supported=yes", output.getvalue())
        self.assertIn("test_sector_empty=yes", output.getvalue())

    def test_flash_test_runs_only_after_empty_scan(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(jedec_id=bytes.fromhex("EF4019"))
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["flash-test"]), 0)
        self.assertIn("test_status=success", output.getvalue())
        self.assertIn("restored_empty=yes", output.getvalue())

    def test_flash_test_refuses_nonempty_sector(self) -> None:
        errors = io.StringIO()
        transport = FakeTransport(
            jedec_id=bytes.fromhex("EF4019"), flash_sector_empty=False
        )
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stderr(errors):
                self.assertEqual(cli.main(["flash-test"]), 2)
        self.assertIn("测试扇区不是空白", errors.getvalue())

    def test_flash_test_reports_4byte_mode_failure(self) -> None:
        errors = io.StringIO()
        output = io.StringIO()
        transport = FakeTransport(
            jedec_id=bytes.fromhex("EF4019"), flash_test_status=10
        )
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                self.assertEqual(cli.main(["flash-test"]), 2)
        self.assertIn("test_status=enter-4byte-mode-failed", output.getvalue())

    def test_flash_status_decodes_protection_bits(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(flash_status=bytes((0x3C, 0x40, 0x01)))
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["flash-status"]), 0)
        self.assertIn("status1=0x3C", output.getvalue())
        self.assertIn("block_protect=7", output.getvalue())
        self.assertIn("complement_protect=1", output.getvalue())
        self.assertIn("address_mode_4byte=1", output.getvalue())

    def test_flash_mode_probe_reports_restored_state(self) -> None:
        output = io.StringIO()
        with mock.patch.object(cli.HidTransport, "open", return_value=FakeTransport()):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["flash-mode-probe"]), 0)
        self.assertIn("write_enable_works=yes", output.getvalue())
        self.assertIn("four_byte_mode_entered=yes", output.getvalue())
        self.assertIn("three_byte_mode_restored=yes", output.getvalue())

    def test_motor_test_reports_both_directions_and_confirmed_stop(self) -> None:
        output = io.StringIO()
        with mock.patch.object(cli.HidTransport, "open", return_value=FakeTransport()), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["motor-test"]), 0)
        text = output.getvalue()
        self.assertIn("motor_port=A", text)
        self.assertIn("motor_state=forward", text)
        self.assertIn("motor_state=reverse", text)
        self.assertIn("motor_test=complete", text)
        self.assertIn("motor_stopped=yes", text)

    def test_motor_tacho_test_reports_motion_and_opposite_directions(self) -> None:
        output = io.StringIO()
        with mock.patch.object(cli.HidTransport, "open", return_value=FakeTransport()), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["motor-tacho-test"]), 0)
        text = output.getvalue()
        self.assertIn("tacho_pins=PE5,PE6", text)
        self.assertIn("motor_power=30%", text)
        self.assertIn("tacho_forward=-24", text)
        self.assertIn("tacho_reverse=24", text)
        self.assertIn("motion_detected=yes", text)
        self.assertIn("directions_opposite=yes", text)
        self.assertIn("motor_tacho_test=complete", text)
        self.assertIn("motor_stopped=yes", text)

    def test_motor_types_prints_large_medium_and_id_voltage(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["motor-types"]), 0)
        text = output.getvalue()
        self.assertIn("motor_A=type:large id_mv:349 adc_raw:286", text)
        self.assertIn("motor_B=type:medium id_mv:2001 adc_raw:1640", text)
        self.assertIn("motor_C=type:none", text)

    def test_motor_identify_refreshes_one_port_without_motion(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["motor-identify", "--port", "B"]), 0)
        text = output.getvalue()
        self.assertIn("motor_port=B", text)
        self.assertIn("identification_motion=none", text)
        self.assertIn("motor_type=medium", text)
        self.assertIn("motor_identify=complete", text)
        refresh = next(
            report for report in transport.reports
            if report[0:3] == b"\x68\x11\x1a" and report[3:5] == b"\x02\x00"
        )
        self.assertEqual(refresh[5:7], bytes((1, 1)))

    def test_motor_tacho_test_accepts_full_power(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    cli.main(["motor-tacho-test", "--power", "100"]), 0
                )
        self.assertIn("motor_power=100%", output.getvalue())
        motor_start = next(
            report for report in transport.reports if report[0:3] == b"\x68\x11\x14"
        )
        self.assertEqual(motor_start[5:7], bytes((1, 100)))

    def test_motor_tacho_test_selects_port_b(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    cli.main(["motor-tacho-test", "--port", "B"]), 0
                )
        self.assertIn("motor_port=B", output.getvalue())
        self.assertIn("tacho_pins=PE13,PE14", output.getvalue())
        motor_start = next(
            report for report in transport.reports if report[0:3] == b"\x68\x11\x14"
        )
        self.assertEqual(motor_start[5:8], bytes((1, 30, 1)))

    def test_motor_tacho_test_selects_port_c(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    cli.main(["motor-tacho-test", "--port", "C"]), 0
                )
        self.assertIn("motor_port=C", output.getvalue())
        self.assertIn("tacho_pins=PC7,PC6", output.getvalue())
        motor_start = next(
            report for report in transport.reports if report[0:3] == b"\x68\x11\x14"
        )
        self.assertEqual(motor_start[5:8], bytes((1, 30, 2)))

    def test_motor_tacho_test_selects_port_d(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    cli.main(["motor-tacho-test", "--port", "D"]), 0
                )
        self.assertIn("motor_port=D", output.getvalue())
        self.assertIn("tacho_pins=PC9,PC8", output.getvalue())
        motor_start = next(
            report for report in transport.reports if report[0:3] == b"\x68\x11\x14"
        )
        self.assertEqual(motor_start[5:8], bytes((1, 30, 3)))

    def test_motor_stop_compare_identifies_brake_and_coast_states(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["motor-stop-compare"]), 0)
        text = output.getvalue()
        self.assertIn("motor_power=60%", text)
        self.assertIn("state_A_after_stop=48", text)
        self.assertIn("state_B_after_stop=8", text)
        self.assertIn("brake_state=B", text)
        self.assertIn("coast_state=A", text)
        self.assertIn("comparison=clear", text)
        self.assertIn("motor_stopped=yes", text)

    def test_motor_dual_test_reports_both_ports_and_balance(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["motor-dual-test"]), 0)
        text = output.getvalue()
        self.assertIn("motor_ports=A,B", text)
        self.assertIn("a_forward=160", text)
        self.assertIn("b_forward=158", text)
        self.assertIn("a_reverse=-165", text)
        self.assertIn("b_reverse=-162", text)
        self.assertIn("both_motors_detected=yes", text)
        self.assertIn("both_directions_ok=yes", text)
        self.assertIn("forward_balance_percent=99", text)
        self.assertIn("reverse_balance_percent=98", text)
        self.assertIn("motor_dual_test=complete", text)
        self.assertIn("motors_stopped=yes", text)

    def test_motor_control_runs_signed_power_and_finishes_safe(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    cli.main(
                        [
                            "motor-control",
                            "--port", "D",
                            "--power", "-40",
                            "--duration", "0.5",
                            "--stop", "brake",
                        ]
                    ),
                    0,
                )
        text = output.getvalue()
        self.assertIn("motor_port=D", text)
        self.assertIn("commanded_power=-40%", text)
        self.assertIn("reported_power=-40%", text)
        self.assertIn("tacho_count=-120", text)
        self.assertIn("stop_state=brake", text)
        self.assertIn("safe_final_state=coast", text)
        self.assertEqual(transport.motor_control_state[3], 0)

    def test_motor_position_converts_rotations_and_finishes_safe(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    cli.main(
                        [
                            "motor-position", "--port", "A", "--speed", "30",
                            "--rotations", "1",
                        ]
                    ),
                    0,
                )
        text = output.getvalue()
        self.assertIn("motor_type=large", text)
        self.assertIn("requested_degrees=360", text)
        self.assertIn("effective_speed=30%", text)
        self.assertIn("actual_degrees=360", text)
        self.assertIn("actual_rotations=1", text)
        self.assertIn("position_error=0", text)
        self.assertIn("motor_position=complete", text)
        self.assertIn("safe_final_state=coast", text)
        motor_start = next(
            report for report in transport.reports if report[0:3] == b"\x68\x11\x1b"
            and report[5] == 1
        )
        self.assertEqual(motor_start[6:8], bytes((0, 30)))
        self.assertEqual(
            int.from_bytes(motor_start[8:12], "little", signed=True), 360
        )

    def test_motor_speed_maintains_signed_target_and_finishes_safe(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    cli.main(
                        [
                            "motor-speed", "--port", "B", "--speed", "-30",
                            "--duration", "0.5", "--stop", "brake",
                        ]
                    ),
                    0,
                )
        text = output.getvalue()
        self.assertIn("motor_type=large", text)
        self.assertIn("requested_speed=-30%", text)
        self.assertIn("measured_speed_average=-30.0%", text)
        self.assertIn("stop_state=brake", text)
        self.assertIn("safe_final_state=coast", text)
        self.assertEqual(transport.motor_speed_state, 0)

    def test_motor_pair_position_reports_sync_and_finishes_safe(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    cli.main(
                        [
                            "motor-pair-position",
                            "--left-port", "C",
                            "--right-port", "D",
                            "--left-degrees", "360",
                            "--right-degrees", "-360",
                            "--speed", "20",
                        ]
                    ),
                    0,
                )
        text = output.getvalue()
        self.assertIn("motor_ports=C,D", text)
        self.assertIn("target_degrees=360,-360", text)
        self.assertIn("max_sync_error=8", text)
        self.assertIn("motor_pair_position=complete", text)
        self.assertIn("safe_final_state=coast", text)
        self.assertEqual(transport.motor_pair_state, 0)

    def test_motor_pair_control_runs_two_ports_independently(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport), \
             mock.patch.object(cli.time, "sleep"):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["motor-pair-control"]), 0)
        text = output.getvalue()
        self.assertIn("motor_ports=A,D", text)
        self.assertIn("A_commanded_power=30%", text)
        self.assertIn("D_commanded_power=-30%", text)
        self.assertIn("A_tacho_count=120", text)
        self.assertIn("D_tacho_count=-120", text)
        self.assertIn("motor_pair_control=complete", text)
        self.assertIn("A_safe_final_state=coast", text)
        self.assertIn("D_safe_final_state=coast", text)
        self.assertEqual(transport.motor_control_state, [0, 0, 0, 0])

    def test_sensor_read_reports_reflected_value_and_uart_health(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["sensor-read"]), 0)
        text = output.getvalue()
        self.assertIn("current_version=M0.19A", text)
        self.assertIn("input_port=1", text)
        self.assertIn("sensor_state=streaming", text)
        self.assertIn("sensor_type=0x1D", text)
        self.assertIn("sensor_model=EST/EV3-color", text)
        self.assertIn("sensor_mode=reflect", text)
        self.assertIn("sensor_value=42", text)
        self.assertIn("checksum_errors=2", text)

    def test_sensor_read_can_select_color_mode(self) -> None:
        output = io.StringIO()
        transport = FakeTransport()
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["sensor-read", "--mode", "color"]), 0)
        text = output.getvalue()
        self.assertIn("sensor_mode=color", text)
        self.assertIn("sensor_value=5", text)
        self.assertIn("color_name=red", text)

    def test_sensor_read_formats_ultrasonic_distance(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(sensor_type=0x1E, sensor_value=123)
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["sensor-read", "--mode", "cm"]), 0)
        text = output.getvalue()
        self.assertIn("sensor_type=0x1E", text)
        self.assertIn("sensor_model=EST/EV3-ultrasonic", text)
        self.assertIn("sensor_mode=cm", text)
        self.assertIn("distance_cm=12.3", text)

    def test_sensor_read_formats_temperature(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(sensor_type=0x06, sensor_value=235)
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(
                    cli.main(["sensor-read", "--mode", "celsius"]), 0
                )
        text = output.getvalue()
        self.assertIn("sensor_type=0x06", text)
        self.assertIn("sensor_model=EST-temperature", text)
        self.assertIn("sensor_mode=celsius", text)
        self.assertIn("temperature_c=23.5", text)

    def test_sensor_read_formats_gyro_angle_and_signed_rate(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(sensor_type=0x20, sensor_value=123)
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["sensor-read", "--mode", "angle"]), 0)
        text = output.getvalue()
        self.assertIn("sensor_type=0x20", text)
        self.assertIn("sensor_model=EST/EV3-gyro", text)
        self.assertIn("sensor_mode=angle", text)
        self.assertIn("sensor_value=123", text)
        self.assertIn("gyro_angle=123", text)

    def test_sensor_read_formats_gyro_signed_rate(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(sensor_type=0x20, sensor_value=0xFFD6)
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["sensor-read", "--mode", "rate"]), 0)
        text = output.getvalue()
        self.assertIn("sensor_mode=rate", text)
        self.assertIn("sensor_value=-42", text)
        self.assertIn("gyro_rate=-42", text)

    def test_sensor_read_formats_nxt_sound_level(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(sensor_type=0x03, sensor_value=64)
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["sensor-read", "--mode", "db"]), 0)
        text = output.getvalue()
        self.assertIn("sensor_type=0x03", text)
        self.assertIn("sensor_model=NXT-sound", text)
        self.assertIn("sensor_mode=db", text)
        self.assertIn("sound_level_db=64", text)

    def test_sensor_read_formats_infrared_beacon(self) -> None:
        output = io.StringIO()
        transport = FakeTransport(sensor_type=0x21)
        with mock.patch.object(cli.HidTransport, "open", return_value=transport):
            with contextlib.redirect_stdout(output):
                self.assertEqual(cli.main(["sensor-read", "--mode", "beacon"]), 0)
        text = output.getvalue()
        self.assertIn("sensor_type=0x21", text)
        self.assertIn("sensor_model=EST/EV3-infrared", text)
        self.assertIn("sensor_mode=beacon", text)
        self.assertIn("beacon_heading=-9", text)
        self.assertIn("beacon_distance=65", text)

    def test_flash_shows_versions_and_writes_success_log(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            package_path = write_package_with_manifest(directory)
            transport = FakeTransport()
            output = io.StringIO()
            with mock.patch.object(cli.HidTransport, "open", return_value=transport):
                with contextlib.redirect_stdout(output):
                    result = cli.main(
                        [
                            "flash",
                            "--file",
                            str(package_path),
                            "--log-dir",
                            str(directory / "logs"),
                        ]
                    )
            self.assertEqual(result, 0)
            self.assertIn("current_version=M0.19A", output.getvalue())
            self.assertIn("target_version=M0.20A", output.getvalue())
            logs = list((directory / "logs").glob("upgrade_*.log"))
            self.assertEqual(len(logs), 1)
            self.assertIn("result=success", logs[0].read_text(encoding="utf-8"))

    def test_flash_blocks_same_version_without_force(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            package_path = write_package_with_manifest(directory, version="M0.19A")
            transport = FakeTransport()
            output = io.StringIO()
            errors = io.StringIO()
            with mock.patch.object(cli.HidTransport, "open", return_value=transport):
                with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                    result = cli.main(
                        [
                            "flash",
                            "--file",
                            str(package_path),
                            "--log-dir",
                            str(directory / "logs"),
                        ]
                    )
            self.assertEqual(result, 2)
            self.assertIn("error[version-safety]", errors.getvalue())
            self.assertEqual(transport.payloads, [])
            logs = list((directory / "logs").glob("upgrade_*.log"))
            self.assertEqual(len(logs), 1)
            self.assertIn("result=failed", logs[0].read_text(encoding="utf-8"))

    def test_flash_blocks_incomparable_versions_without_force(self) -> None:
        with self.assertRaisesRegex(cli.VersionSafetyError, "无法自动比较"):
            cli.enforce_version_safety("03.00B", "M0.20A", force=False)


if __name__ == "__main__":
    unittest.main()
