from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent))

from est_hid_sender.constants import (  # noqa: E402
    FRAME_END,
    LEGACY_REPORT_SIZE,
    MAX_PAYLOAD,
)
from est_hid_sender.firmware import detect_header, load_firmware_package  # noqa: E402
from est_hid_sender.errors import (  # noqa: E402
    AckRejectedError,
    AckTimeoutError,
    FirmwareValidationError,
    HeartbeatTimeoutError,
)
from est_hid_sender.protocol import (  # noqa: E402
    build_frame,
    build_drive_steer_frame,
    build_drive_steer_for_frame,
    build_drive_straight_frame,
    build_drive_run_frame,
    build_device_status_frame,
    build_flash_id_frame,
    build_flash_scan_frame,
    build_flash_test_frame,
    build_flash_status_frame,
    build_flash_mode_probe_frame,
    build_heartbeat_frame,
    build_input_sensor_frame,
    build_key_status_frame,
    build_motor_control_frame,
    build_motor_dual_test_frame,
    build_motor_position_frame,
    build_motor_pair_position_frame,
    build_motor_pair_speed_frame,
    build_motor_speed_frame,
    build_motor_stop_test_frame,
    build_motor_tacho_test_frame,
    build_motor_test_frame,
    build_motor_type_frame,
    build_update_frame,
    checksum,
    parse_heartbeat_response,
    parse_drive_steer_response,
    parse_drive_steer_for_response,
    parse_drive_straight_response,
    parse_drive_run_response,
    parse_device_status_response,
    parse_input_sensor_response,
    parse_flash_id_response,
    parse_flash_scan_response,
    parse_flash_test_response,
    parse_flash_status_response,
    parse_flash_mode_probe_response,
    parse_key_status_response,
    parse_motor_control_response,
    parse_motor_dual_test_response,
    parse_motor_position_response,
    parse_motor_pair_position_response,
    parse_motor_pair_speed_response,
    parse_motor_speed_response,
    parse_motor_stop_test_response,
    parse_motor_tacho_test_response,
    parse_motor_test_response,
    parse_motor_type_response,
    parse_update_ack,
    split_reports,
)
from est_hid_sender.updater import FirmwareUpdater, PacketProgress  # noqa: E402


def build_ack(total: int, index: int, flag: int = 1) -> bytes:
    report = bytearray(LEGACY_REPORT_SIZE)
    report[:10] = bytes(
        (
            0x68,
            0x21,
            0x05,
            0x05,
            0x00,
            total & 0xFF,
            total >> 8,
            index & 0xFF,
            index >> 8,
            flag,
        )
    )
    report[10] = checksum(report[:10])
    report[11] = 0x16
    return bytes(report)


def build_heartbeat(version: bytes = b"M0.19A") -> bytes:
    payload = version
    frame = bytearray((0x68, 0x21, 0x01))
    frame += len(payload).to_bytes(2, "little")
    frame += payload
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_key_status(mask: int = 0) -> bytes:
    frame = bytearray((0x68, 0x21, 0x0D, 0x01, 0x00, mask & 0x3F))
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_flash_id(jedec_id: bytes = bytes.fromhex("EF4017")) -> bytes:
    frame = bytearray((0x68, 0x21, 0x0E, 0x03, 0x00))
    frame += jedec_id
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_flash_diagnostic(command: int, first: int, second: int,
                           address: int = 0x01FFF000) -> bytes:
    frame = bytearray((0x68, 0x21, command, 0x06, 0x00, first, second))
    frame += address.to_bytes(4, "little")
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_test_result(result: int = 1, state: int = 1) -> bytes:
    frame = bytearray((0x68, 0x21, 0x13, 0x02, 0x00, result, state))
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_tacho_test_result(
    result: int = 1,
    state: int = 4,
    total: int = 0,
    forward: int = -24,
    reverse: int = 24,
) -> bytes:
    frame = bytearray((0x68, 0x21, 0x14, 0x0E, 0x00, result, state))
    frame += total.to_bytes(4, "little", signed=True)
    frame += forward.to_bytes(4, "little", signed=True)
    frame += reverse.to_bytes(4, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_stop_test_result(
    result: int = 1,
    state: int = 3,
    mode: int = 0,
    total: int = 132,
    powered: int = 100,
    stopped: int = 32,
) -> bytes:
    frame = bytearray((0x68, 0x21, 0x15, 0x0F, 0x00, result, state, mode))
    frame += total.to_bytes(4, "little", signed=True)
    frame += powered.to_bytes(4, "little", signed=True)
    frame += stopped.to_bytes(4, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_dual_test_result(
    result: int = 1,
    state: int = 5,
    a_forward: int = 160,
    b_forward: int = 158,
    a_reverse: int = -165,
    b_reverse: int = -162,
) -> bytes:
    frame = bytearray((0x68, 0x21, 0x16, 0x12, 0x00, result, state))
    frame += a_forward.to_bytes(4, "little", signed=True)
    frame += b_forward.to_bytes(4, "little", signed=True)
    frame += a_reverse.to_bytes(4, "little", signed=True)
    frame += b_reverse.to_bytes(4, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_control_result(
    result: int = 1,
    port: int = 3,
    output_state: int = 1,
    power: int = -40,
    tacho: int = -123,
) -> bytes:
    frame = bytearray(
        (0x68, 0x21, 0x17, 0x08, 0x00, result, port, output_state, power & 0xFF)
    )
    frame += tacho.to_bytes(4, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_position_result(
    result: int = 1,
    port: int = 0,
    state: int = 2,
    motor_type: int = 4,
    requested_speed: int = 30,
    measured_speed: int = 0,
    start_count: int = 12,
    target_count: int = 372,
    current_count: int = 370,
) -> bytes:
    frame = bytearray(
        (
            0x68, 0x21, 0x1B, 0x16, 0x00, result, port, state, motor_type,
            requested_speed & 0xFF, measured_speed & 0xFF,
        )
    )
    frame += start_count.to_bytes(4, "little", signed=True)
    frame += target_count.to_bytes(4, "little", signed=True)
    frame += current_count.to_bytes(4, "little", signed=True)
    frame += (target_count - current_count).to_bytes(4, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_speed_result(
    result: int = 1,
    port: int = 1,
    state: int = 1,
    output_state: int = 1,
    motor_type: int = 4,
    requested_speed: int = 30,
    measured_speed: int = 29,
    power: int = 34,
    tacho: int = 456,
) -> bytes:
    frame = bytearray(
        (
            0x68, 0x21, 0x1C, 0x0C, 0x00, result, port, state,
            output_state, motor_type, requested_speed & 0xFF,
            measured_speed & 0xFF, power & 0xFF,
        )
    )
    frame += tacho.to_bytes(4, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_pair_position_result(
    result: int = 1,
    state: int = 2,
    left_port: int = 2,
    right_port: int = 3,
    left_target: int = 360,
    right_target: int = -360,
    left_actual: int = 364,
    right_actual: int = -367,
    sync_error: int = -3,
    max_sync_error: int = 11,
    error: int = 0,
) -> bytes:
    frame = bytearray(
        (0x68, 0x21, 0x1D, 0x1D, 0x00, result, state, left_port, right_port)
    )
    for value in (
        left_target,
        right_target,
        left_actual,
        right_actual,
        sync_error,
        max_sync_error,
    ):
        frame += value.to_bytes(4, "little", signed=True)
    frame += error.to_bytes(1, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_pair_speed_result(
    command: int = 0x1E,
    result: int = 1,
    state: int = 1,
    left_port: int = 0,
    right_port: int = 2,
    left_requested: int = 20,
    right_requested: int = 20,
    left_measured: int = 19,
    right_measured: int = 18,
    left_power: int = 23,
    right_power: int = 24,
    left_actual: int = 180,
    right_actual: int = 178,
    sync_error: int = 2,
    max_sync_error: int = 6,
    error: int = 0,
) -> bytes:
    frame = bytearray(
        (
            0x68, 0x21, command, 0x1B, 0x00, result, state,
            left_port, right_port,
            left_requested & 0xFF, right_requested & 0xFF,
            left_measured & 0xFF, right_measured & 0xFF,
            left_power & 0xFF, right_power & 0xFF,
        )
    )
    for value in (left_actual, right_actual, sync_error, max_sync_error):
        frame += value.to_bytes(4, "little", signed=True)
    frame += error.to_bytes(1, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_drive_straight_result(
    result: int = 1,
    state: int = 2,
    left_port: int = 0,
    right_port: int = 2,
    target_distance: int = 500,
    actual_distance: int = 501,
    left_target: int = 1023,
    right_target: int = 1023,
    left_actual: int = 1030,
    right_actual: int = 1029,
    sync_error: int = 1,
    max_sync_error: int = 6,
    error: int = 0,
) -> bytes:
    frame = bytearray(
        (0x68, 0x21, 0x1F, 0x25, 0x00, result, state, left_port, right_port)
    )
    for value in (
        target_distance,
        actual_distance,
        left_target,
        right_target,
        left_actual,
        right_actual,
        sync_error,
        max_sync_error,
    ):
        frame += value.to_bytes(4, "little", signed=True)
    frame += error.to_bytes(1, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_drive_run_result(
    result: int = 1,
    state: int = 2,
    mode: int = 0,
    left_port: int = 0,
    right_port: int = 2,
    requested_speed: int = 40,
    target_value: int = 720,
    actual_value: int = 721,
    left_actual: int = 722,
    right_actual: int = 720,
    sync_error: int = 2,
    max_sync_error: int = 6,
    error: int = 0,
) -> bytes:
    frame = bytearray(
        (
            0x68, 0x21, 0x20, 0x1F, 0x00, result, state, mode,
            left_port, right_port, requested_speed & 0xFF,
        )
    )
    for value in (
        target_value,
        actual_value,
        left_actual,
        right_actual,
        sync_error,
        max_sync_error,
    ):
        frame += value.to_bytes(4, "little", signed=True)
    frame += error.to_bytes(1, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_drive_steer_for_result(
    result: int = 1,
    state: int = 2,
    mode: int = 0,
    left_port: int = 0,
    right_port: int = 2,
    steering: int = 50,
    requested_speed: int = 80,
    left_requested: int = 80,
    right_requested: int = 40,
    target_value: int = 720,
    actual_value: int = 720,
    left_target: int = 720,
    right_target: int = 360,
    left_actual: int = 722,
    right_actual: int = 361,
    sync_error: int = 2,
    max_sync_error: int = 6,
    error: int = 0,
) -> bytes:
    frame = bytearray(
        (
            0x68, 0x21, 0x22, 0x2A, 0x00, result, state, mode,
            left_port, right_port, steering & 0xFF, requested_speed & 0xFF,
            left_requested & 0xFF, right_requested & 0xFF,
        )
    )
    for value in (
        target_value,
        actual_value,
        left_target,
        right_target,
        left_actual,
        right_actual,
        sync_error,
        max_sync_error,
    ):
        frame += value.to_bytes(4, "little", signed=True)
    frame += error.to_bytes(1, "little", signed=True)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_motor_type_result(include_digital: bool = True) -> bytes:
    payload_size = 0x39 if include_digital else 0x35
    frame = bytearray((0x68, 0x21, 0x1A, payload_size, 0x00, 0x01))
    for motor_type, adc_raw, millivolts, low_raw, low_mv, pullup_raw, pullup_mv, high in (
        (4, 286, 349, 280, 341, 310, 378, 0),
        (5, 1640, 2001, 1600, 1953, 1680, 2050, 1),
        (0, 614, 749, 610, 744, 900, 1098, 1),
        (0xFF, 4095, 4998, 4090, 4992, 4080, 4980, 0),
    ):
        frame.append(motor_type)
        frame += adc_raw.to_bytes(2, "little")
        frame += millivolts.to_bytes(2, "little")
        frame += low_raw.to_bytes(2, "little")
        frame += low_mv.to_bytes(2, "little")
        frame += pullup_raw.to_bytes(2, "little")
        frame += pullup_mv.to_bytes(2, "little")
        if include_digital:
            frame.append(high)
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_input_sensor_result(
    result: int = 1,
    port: int = 0,
    state: int = 2,
    sensor_type: int = 0x1D,
    mode: int = 0,
    value_valid: bool = True,
    value: int = 42,
    adc0_raw: int = 123,
    adc1_raw: int = 456,
    digital_mask: int = 0x05,
    rx_count: int = 3210,
    checksum_errors: int = 2,
) -> bytes:
    frame = bytearray(
        (
            0x68, 0x21, 0x18, 0x13, 0x00, result, port, state,
            sensor_type, mode, 1 if value_valid else 0,
        )
    )
    frame += value.to_bytes(2, "little")
    frame += adc0_raw.to_bytes(2, "little")
    frame += adc1_raw.to_bytes(2, "little")
    frame.append(digital_mask)
    frame += rx_count.to_bytes(4, "little")
    frame += checksum_errors.to_bytes(2, "little")
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame).ljust(LEGACY_REPORT_SIZE, b"\x00")


def build_device_status_result() -> bytes:
    payload = bytearray((1, 0))
    payload += b"M0.52A"
    payload += bytes((4, 4, 0x12, 4))
    payload += (2800).to_bytes(2, "little")
    payload += (1708).to_bytes(2, "little")
    payload += (0x3F).to_bytes(4, "little")
    payload += (123456).to_bytes(4, "little")
    for state, power, tacho in (
        (0, 0, 12),
        (1, 40, 345),
        (1, -30, -456),
        (2, 0, 789),
    ):
        payload += bytes((state, power & 0xFF))
        payload += tacho.to_bytes(4, "little", signed=True)
    for state, sensor_type, mode, valid, value in (
        (2, 0x1D, 2, 1, 5),
        (2, 0x10, 0, 1, 1),
        (2, 0x06, 0, 1, 235),
        (0, 0x00, 0, 0, 0),
    ):
        payload += bytes((state, sensor_type, mode, valid))
        payload += value.to_bytes(2, "little")
    frame = bytearray((0x68, 0x21, 0x19))
    frame += len(payload).to_bytes(2, "little")
    frame += payload
    frame.append(checksum(frame))
    frame.append(0x16)
    return bytes(frame)


class ProtocolTests(unittest.TestCase):
    def test_heartbeat_frame_preserves_legacy_frame_format(self) -> None:
        frame = build_heartbeat_frame()
        self.assertEqual(frame, bytes((0x68, 0x11, 0x01, 0x00, 0x00, 0x7A, 0x16)))
        self.assertEqual(build_frame(0x01), frame)

    def test_update_frame_preserves_total_index_payload_layout(self) -> None:
        payload = b"APP=" + bytes(MAX_PAYLOAD - 4)
        frame = build_update_frame(260, 0, payload)
        self.assertEqual(frame[:9], b"\x68\x11\x05\xf6\x03\x04\x01\x00\x00")
        self.assertEqual(len(frame), 1021)
        self.assertEqual(frame[-1], FRAME_END)
        self.assertEqual(frame[-2], checksum(frame[:-2]))

    def test_split_reports_uses_legacy_64_byte_fragments(self) -> None:
        reports = split_reports(bytes(range(130)))
        self.assertEqual([len(report) for report in reports], [64, 64, 64])
        self.assertEqual(reports[-1][2:], b"\x00" * 62)

    def test_parse_heartbeat_and_update_ack(self) -> None:
        self.assertEqual(parse_heartbeat_response(build_heartbeat()), "M0.19A")
        ack = parse_update_ack(build_ack(260, 3))
        self.assertIsNotNone(ack)
        self.assertEqual(ack.total_frames, 260)
        self.assertEqual(ack.frame_index, 3)
        self.assertEqual(ack.flag, 1)

    def test_key_status_query_and_response(self) -> None:
        self.assertEqual(
            build_key_status_frame(),
            bytes((0x68, 0x11, 0x0D, 0x00, 0x00, 0x86, 0x16)),
        )
        self.assertEqual(parse_key_status_response(build_key_status(0x25)), 0x25)

    def test_flash_id_query_and_response(self) -> None:
        self.assertEqual(
            build_flash_id_frame(),
            bytes((0x68, 0x11, 0x0E, 0x00, 0x00, 0x87, 0x16)),
        )
        self.assertEqual(parse_flash_id_response(build_flash_id()), bytes.fromhex("EF4017"))

    def test_flash_scan_query_and_response(self) -> None:
        self.assertEqual(build_flash_scan_frame(), build_frame(0x0F))
        result = parse_flash_scan_response(build_flash_diagnostic(0x0F, 1, 1))
        self.assertIsNotNone(result)
        self.assertTrue(result.supported)
        self.assertTrue(result.erased)
        self.assertEqual(result.address, 0x01FFF000)

    def test_flash_test_query_and_response(self) -> None:
        self.assertEqual(build_flash_test_frame(), build_frame(0x10))
        result = parse_flash_test_response(build_flash_diagnostic(0x10, 1, 1))
        self.assertIsNotNone(result)
        self.assertEqual(result.status, 1)
        self.assertTrue(result.restored)

    def test_flash_status_query_and_response(self) -> None:
        self.assertEqual(build_flash_status_frame(), build_frame(0x11))
        response = build_flash_id(bytes((0x3C, 0x40, 0x01)))
        response = bytes((response[0], response[1], 0x11)) + response[3:]
        response = bytearray(response)
        response[8] = checksum(response[:8])
        status = parse_flash_status_response(bytes(response))
        self.assertIsNotNone(status)
        self.assertEqual((status.status1, status.status2, status.status3), (0x3C, 0x40, 0x01))

    def test_flash_mode_probe_query_and_response(self) -> None:
        self.assertEqual(build_flash_mode_probe_frame(), build_frame(0x12))
        frame = bytearray((0x68, 0x21, 0x12, 0x06, 0x00, 0x00, 0x02, 0x00, 0x60, 0x61, 0x60))
        frame.append(checksum(frame))
        frame.append(0x16)
        probe = parse_flash_mode_probe_response(bytes(frame))
        self.assertIsNotNone(probe)
        self.assertEqual(probe.status1_write_enabled, 0x02)
        self.assertEqual(probe.status3_four_byte, 0x61)

    def test_motor_test_uses_additive_command_without_changing_frame_format(self) -> None:
        self.assertEqual(
            build_motor_test_frame(1),
            bytes((0x68, 0x11, 0x13, 0x01, 0x00, 0x01, 0x8E, 0x16)),
        )
        result = parse_motor_test_response(build_motor_test_result(1, 3))
        self.assertIsNotNone(result)
        self.assertEqual((result.result, result.state), (1, 3))

    def test_motor_tacho_test_parses_signed_direction_counts(self) -> None:
        self.assertEqual(
            build_motor_tacho_test_frame(1),
            bytes((0x68, 0x11, 0x14, 0x01, 0x00, 0x01, 0x8F, 0x16)),
        )
        result = parse_motor_tacho_test_response(
            build_motor_tacho_test_result(total=2, forward=-23, reverse=25)
        )
        self.assertIsNotNone(result)
        self.assertEqual(result.total_count, 2)
        self.assertEqual(result.forward_count, -23)
        self.assertEqual(result.reverse_count, 25)
        self.assertEqual(
            build_motor_tacho_test_frame(1, 100),
            bytes((0x68, 0x11, 0x14, 0x02, 0x00, 0x01, 0x64, 0xF4, 0x16)),
        )

    def test_motor_stop_test_preserves_frame_and_parses_counts(self) -> None:
        self.assertEqual(
            build_motor_stop_test_frame(1, 0, 60),
            bytes((0x68, 0x11, 0x15, 0x03, 0x00, 0x01, 0x00, 0x3C, 0xCE, 0x16)),
        )
        result = parse_motor_stop_test_response(
            build_motor_stop_test_result(
                mode=1, total=-140, powered=-100, stopped=-40
            )
        )
        self.assertIsNotNone(result)
        self.assertEqual(result.mode, 1)
        self.assertEqual(result.total_count, -140)
        self.assertEqual(result.powered_count, -100)
        self.assertEqual(result.stopped_count, -40)
        self.assertEqual(
            build_motor_tacho_test_frame(1, 30, 1),
            bytes((0x68, 0x11, 0x14, 0x03, 0x00, 0x01, 0x1E, 0x01, 0xB0, 0x16)),
        )
        self.assertEqual(
            build_motor_tacho_test_frame(1, 30, 2),
            bytes((0x68, 0x11, 0x14, 0x03, 0x00, 0x01, 0x1E, 0x02, 0xB1, 0x16)),
        )
        self.assertEqual(
            build_motor_tacho_test_frame(1, 30, 3),
            bytes((0x68, 0x11, 0x14, 0x03, 0x00, 0x01, 0x1E, 0x03, 0xB2, 0x16)),
        )
        self.assertEqual(
            build_motor_stop_test_frame(1, 0, 60, 1),
            bytes((0x68, 0x11, 0x15, 0x04, 0x00, 0x01, 0x00, 0x3C, 0x01, 0xD0, 0x16)),
        )

    def test_motor_dual_test_builds_frame_and_parses_both_ports(self) -> None:
        self.assertEqual(
            build_motor_dual_test_frame(1, 30),
            bytes((0x68, 0x11, 0x16, 0x02, 0x00, 0x01, 0x1E, 0xB0, 0x16)),
        )
        result = parse_motor_dual_test_response(build_motor_dual_test_result())
        self.assertIsNotNone(result)
        self.assertEqual(result.a_forward_count, 160)
        self.assertEqual(result.b_forward_count, 158)
        self.assertEqual(result.a_reverse_count, -165)
        self.assertEqual(result.b_reverse_count, -162)

    def test_motor_control_adds_signed_power_without_changing_frame_format(self) -> None:
        self.assertEqual(
            build_motor_control_frame(1, 3, -40),
            bytes((0x68, 0x11, 0x17, 0x03, 0x00, 0x01, 0x03, 0xD8, 0x6F, 0x16)),
        )
        self.assertEqual(
            build_motor_control_frame(3, 2),
            bytes((0x68, 0x11, 0x17, 0x02, 0x00, 0x03, 0x02, 0x97, 0x16)),
        )
        result = parse_motor_control_response(build_motor_control_result())
        self.assertIsNotNone(result)
        self.assertEqual(result.port, 3)
        self.assertEqual(result.output_state, 1)
        self.assertEqual(result.power_percent, -40)
        self.assertEqual(result.tacho_count, -123)
        with self.assertRaisesRegex(ValueError, "-100 and 100"):
            build_motor_control_frame(1, 0, 101)

    def test_motor_position_builds_signed_degrees_and_parses_runtime_state(self) -> None:
        self.assertEqual(
            build_motor_position_frame(1, 1, 30, -360),
            build_frame(0x1B, bytes((1, 1, 30)) + (-360).to_bytes(4, "little", signed=True)),
        )
        result = parse_motor_position_response(build_motor_position_result())
        self.assertIsNotNone(result)
        self.assertEqual(result.motor_type, 4)
        self.assertEqual(result.requested_speed_percent, 30)
        self.assertEqual(result.start_count, 12)
        self.assertEqual(result.target_count, 372)
        self.assertEqual(result.current_count, 370)
        self.assertEqual(result.error_count, 2)
        with self.assertRaisesRegex(ValueError, "between 10 and 100"):
            build_motor_position_frame(1, 0, 9, 360)

    def test_motor_speed_builds_signed_target_and_parses_closed_loop_state(self) -> None:
        self.assertEqual(
            build_motor_speed_frame(1, 1, -30),
            build_frame(0x1C, bytes((1, 1, 0xE2))),
        )
        result = parse_motor_speed_response(
            build_motor_speed_result(requested_speed=-30, measured_speed=-29,
                                     power=-34, tacho=-456)
        )
        self.assertIsNotNone(result)
        self.assertEqual(result.motor_type, 4)
        self.assertEqual(result.requested_speed_percent, -30)
        self.assertEqual(result.measured_speed_percent, -29)
        self.assertEqual(result.power_percent, -34)
        self.assertEqual(result.tacho_count, -456)
        with self.assertRaisesRegex(ValueError, "magnitude at least 10"):
            build_motor_speed_frame(1, 0, 9)

    def test_motor_pair_position_supports_proportional_targets_and_parses_error(self) -> None:
        self.assertEqual(
            build_motor_pair_position_frame(1, 2, 3, 20, 720, -360),
            build_frame(
                0x1D,
                bytes((1, 2, 3, 20))
                + (720).to_bytes(4, "little", signed=True)
                + (-360).to_bytes(4, "little", signed=True),
            ),
        )
        result = parse_motor_pair_position_response(
            build_motor_pair_position_result()
        )
        self.assertIsNotNone(result)
        self.assertEqual((result.left_port, result.right_port), (2, 3))
        self.assertEqual(result.left_actual_degrees, 364)
        self.assertEqual(result.right_actual_degrees, -367)
        self.assertEqual(result.synchronization_error_degrees, -3)
        self.assertEqual(result.maximum_synchronization_error_degrees, 11)
        with self.assertRaisesRegex(ValueError, "excluding 0"):
            build_motor_pair_position_frame(1, 2, 3, 20, 360, 0)

    def test_motor_pair_speed_accepts_independent_speeds_and_parses_runtime(self) -> None:
        self.assertEqual(
            build_motor_pair_speed_frame(1, 0, 2, 40, -20),
            build_frame(0x1E, bytes((1, 0, 2, 40, 0xEC))),
        )
        result = parse_motor_pair_speed_response(
            build_motor_pair_speed_result(
                left_requested=20,
                right_requested=-20,
                left_measured=19,
                right_measured=-18,
                left_actual=180,
                right_actual=-178,
            )
        )
        self.assertIsNotNone(result)
        self.assertEqual((result.left_port, result.right_port), (0, 2))
        self.assertEqual((result.left_measured_speed_percent,
                          result.right_measured_speed_percent), (19, -18))
        self.assertEqual((result.left_actual_degrees,
                          result.right_actual_degrees), (180, -178))
        self.assertEqual(result.maximum_synchronization_error_degrees, 6)
        with self.assertRaisesRegex(ValueError, "magnitude between 10 and 100"):
            build_motor_pair_speed_frame(1, 0, 2, 5, 30)

    def test_drive_steer_uses_ev3_classroom_speed_mix(self) -> None:
        self.assertEqual(
            build_drive_steer_frame(1, 0, 2, 50, 80),
            build_frame(0x21, bytes((1, 0, 2, 50, 80))),
        )
        result = parse_drive_steer_response(
            build_motor_pair_speed_result(
                command=0x21,
                left_requested=80,
                right_requested=40,
                left_measured=79,
                right_measured=39,
            )
        )
        self.assertIsNotNone(result)
        self.assertEqual(
            (result.left_requested_speed_percent,
             result.right_requested_speed_percent),
            (80, 40),
        )
        self.assertEqual(
            build_drive_steer_frame(1, 0, 2, 100, 80),
            build_frame(0x21, bytes((1, 0, 2, 100, 80))),
        )
        with self.assertRaisesRegex(ValueError, "between -100 and 100"):
            build_drive_steer_frame(1, 0, 2, 101, 80)

    def test_drive_steer_for_sends_finite_target_and_parses_progress(self) -> None:
        self.assertEqual(
            build_drive_steer_for_frame(1, 0, 2, 0, 50, 80, 720, 0),
            build_frame(
                0x22,
                bytes((1, 0, 2, 0, 50, 80))
                + (720).to_bytes(4, "little", signed=True)
                + bytes((0,)),
            ),
        )
        result = parse_drive_steer_for_response(
            build_drive_steer_for_result(
                steering=-100,
                requested_speed=-80,
                left_requested=80,
                right_requested=-80,
                left_target=720,
                right_target=-720,
                left_actual=721,
                right_actual=-719,
            )
        )
        self.assertIsNotNone(result)
        self.assertEqual(result.steering, -100)
        self.assertEqual(result.requested_speed_percent, -80)
        self.assertEqual(
            (result.left_requested_speed_percent,
             result.right_requested_speed_percent),
            (80, -80),
        )
        self.assertEqual(
            (result.left_target_degrees, result.right_target_degrees),
            (720, -720),
        )
        with self.assertRaisesRegex(ValueError, "greater than zero"):
            build_drive_steer_for_frame(1, 0, 2, 0, 50, 80, -720, 0)
        with self.assertRaisesRegex(ValueError, "coast stop only"):
            build_drive_steer_for_frame(1, 0, 2, 0, 50, 80, 720, 1)

    def test_drive_straight_sends_geometry_and_parses_distance(self) -> None:
        self.assertEqual(
            build_drive_straight_frame(1, 0, 2, 56, 120, -500, 40, 0),
            build_frame(
                0x1F,
                bytes((1, 0, 2))
                + (56).to_bytes(2, "little")
                + (120).to_bytes(2, "little")
                + (-500).to_bytes(4, "little", signed=True)
                + bytes((40, 0)),
            ),
        )
        result = parse_drive_straight_response(
            build_drive_straight_result(
                target_distance=-500,
                actual_distance=-501,
                left_target=-1023,
                right_target=-1023,
                left_actual=-1025,
                right_actual=-1024,
            )
        )
        self.assertIsNotNone(result)
        self.assertEqual((result.left_port, result.right_port), (0, 2))
        self.assertEqual(result.target_distance_mm, -500)
        self.assertEqual(result.actual_distance_mm, -501)
        self.assertEqual((result.left_target_degrees,
                          result.right_target_degrees), (-1023, -1023))
        self.assertEqual(result.maximum_synchronization_error_degrees, 6)
        with self.assertRaisesRegex(ValueError, "coast stop only"):
            build_drive_straight_frame(1, 0, 2, 56, 120, 500, 40, 1)

    def test_drive_run_sends_degrees_or_time_and_parses_progress(self) -> None:
        self.assertEqual(
            build_drive_run_frame(1, 0, 2, 0, -720, 40, 0),
            build_frame(
                0x20,
                bytes((1, 0, 2, 0))
                + (-720).to_bytes(4, "little", signed=True)
                + bytes((40, 0)),
            ),
        )
        self.assertEqual(
            build_drive_run_frame(1, 0, 2, 1, -3000, 60, 1)[5:15],
            bytes((1, 0, 2, 1))
            + (-3000).to_bytes(4, "little", signed=True)
            + bytes((60, 1)),
        )
        result = parse_drive_run_response(
            build_drive_run_result(
                mode=1,
                requested_speed=-60,
                target_value=-3000,
                actual_value=-3000,
                left_actual=-540,
                right_actual=-538,
            )
        )
        self.assertIsNotNone(result)
        self.assertEqual(result.mode, 1)
        self.assertEqual(result.requested_speed_percent, -60)
        self.assertEqual(result.target_value, -3000)
        self.assertEqual(result.actual_value, -3000)
        self.assertEqual((result.left_actual_degrees,
                          result.right_actual_degrees), (-540, -538))
        with self.assertRaisesRegex(ValueError, "coast stop only"):
            build_drive_run_frame(1, 0, 2, 0, 720, 40, 1)

    def test_input_sensor_command_builds_modes_and_parses_diagnostics(self) -> None:
        self.assertEqual(
            build_input_sensor_frame(0, 0),
            bytes((0x68, 0x11, 0x18, 0x02, 0x00, 0x00, 0x00, 0x93, 0x16)),
        )
        self.assertEqual(
            build_input_sensor_frame(1, 0, 2),
            bytes((0x68, 0x11, 0x18, 0x03, 0x00, 0x01, 0x00, 0x02, 0x97, 0x16)),
        )
        result = parse_input_sensor_response(build_input_sensor_result())
        self.assertIsNotNone(result)
        self.assertEqual(result.sensor_type, 0x1D)
        self.assertEqual(result.mode, 0)
        self.assertTrue(result.value_valid)
        self.assertEqual(result.value, 42)
        self.assertEqual((result.adc0_raw, result.adc1_raw), (123, 456))
        self.assertEqual(result.rx_count, 3210)
        self.assertEqual(result.checksum_errors, 2)
        with self.assertRaisesRegex(ValueError, "mode must be 0, 1, or 2"):
            build_input_sensor_frame(1, 0, 3)

    def test_device_status_returns_one_complete_machine_snapshot(self) -> None:
        self.assertEqual(
            build_device_status_frame(),
            bytes((0x68, 0x11, 0x19, 0x00, 0x00, 0x92, 0x16)),
        )
        status = parse_device_status_response(build_device_status_result())
        self.assertIsNotNone(status)
        self.assertEqual(status.firmware_version, "M0.52A")
        self.assertEqual((status.protocol_major, status.protocol_minor), (1, 0))
        self.assertEqual(status.battery_level, 4)
        self.assertEqual(status.battery_adc_raw, 2800)
        self.assertEqual(status.battery_sample_mv, 1708)
        self.assertEqual(status.capabilities, 0x3F)
        self.assertEqual(status.uptime_ms, 123456)
        self.assertEqual(status.motors[1].power_percent, 40)
        self.assertEqual(status.motors[2].tacho_count, -456)
        self.assertEqual(status.sensors[0].sensor_type, 0x1D)
        self.assertEqual(status.sensors[2].value, 235)

    def test_motor_type_query_parses_all_four_output_ports(self) -> None:
        self.assertEqual(build_motor_type_frame(), build_frame(0x1A))
        self.assertEqual(build_motor_type_frame(1, 1), build_frame(0x1A, bytes((1, 1))))
        result = parse_motor_type_response(build_motor_type_result())
        self.assertIsNotNone(result)
        self.assertEqual(result.result, 1)
        self.assertEqual(result.motors[0].motor_type, 4)
        self.assertEqual(result.motors[1].motor_type, 5)
        self.assertEqual(result.motors[1].millivolts, 2001)
        self.assertEqual(result.motors[2].adc_raw, 614)
        self.assertEqual(result.motors[0].pin6_low_millivolts, 341)
        self.assertEqual(result.motors[1].pin5_pullup_millivolts, 2050)
        self.assertTrue(result.motors[1].pin5_pullup_high)

        legacy_result = parse_motor_type_response(build_motor_type_result(False))
        self.assertIsNotNone(legacy_result)
        self.assertEqual(legacy_result.motors[1].pin5_pullup_millivolts, 2050)
        self.assertFalse(legacy_result.motors[1].pin5_pullup_high)


class FirmwareTests(unittest.TestCase):
    def test_accepts_existing_est_and_app_upgrade_headers(self) -> None:
        self.assertEqual(detect_header(b"APP=" + b"\xFF"), b"APP=")
        self.assertEqual(detect_header(b"EST" + b"\xFF"), b"EST")

    def test_rejects_unknown_firmware_header(self) -> None:
        with self.assertRaisesRegex(FirmwareValidationError, "升级包头无效"):
            detect_header(b"BAD")

    def test_load_firmware_package_reports_size_total_and_sha(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "firmware.upgrade.bin"
            path.write_bytes(b"APP=" + b"\xFF" * 1010)
            package = load_firmware_package(path)
            self.assertEqual(package.header, b"APP=")
            self.assertEqual(package.size, 1014)
            self.assertEqual(package.total_frames, 2)
            self.assertEqual(len(package.sha256), 64)


class FakeTransport:
    def __init__(
        self,
        output_len: int = 1025,
        ack_flag: int = 1,
        key_mask: int = 0,
        jedec_id: bytes = bytes.fromhex("EF4017"),
        flash_sector_empty: bool = True,
        flash_test_status: int = 1,
        flash_status: bytes = bytes((0x00, 0x00, 0x00)),
        flash_mode_probe: bytes = bytes((0x00, 0x02, 0x00, 0x60, 0x61, 0x60)),
        sensor_type: int = 0x1D,
        sensor_value: int = 42,
    ) -> None:
        self.input_len = 1025
        self.output_len = output_len
        self.path = "fake"
        self.payloads: list[bytes] = []
        self.reports: list[bytes] = []
        self.acks: list[bytes] = []
        self.ack_flag = ack_flag
        self.key_mask = key_mask
        self.jedec_id = jedec_id
        self.flash_sector_empty = flash_sector_empty
        self.flash_test_status = flash_test_status
        self.flash_status = flash_status
        self.flash_mode_probe = flash_mode_probe
        self.motor_status_index = 0
        self.motor_tacho_status_index = 0
        self.motor_stop_status_index = 0
        self.motor_stop_mode = 0
        self.motor_dual_status_index = 0
        self.motor_control_state = [0, 0, 0, 0]
        self.motor_control_power = [0, 0, 0, 0]
        self.motor_control_tacho = [0, 0, 0, 0]
        self.motor_position_status_index = 0
        self.motor_position_port = 0
        self.motor_position_speed = 30
        self.motor_position_target = 360
        self.motor_speed_state = 0
        self.motor_speed_port = 0
        self.motor_speed_target = 0
        self.motor_speed_tacho = 0
        self.motor_pair_state = 0
        self.motor_pair_status_index = 0
        self.motor_pair_left_port = 2
        self.motor_pair_right_port = 3
        self.motor_pair_left_target = 360
        self.motor_pair_right_target = 360
        self.motor_pair_speed_state = 0
        self.motor_pair_speed_left_port = 0
        self.motor_pair_speed_right_port = 2
        self.motor_pair_speed_left_target = 20
        self.motor_pair_speed_right_target = 20
        self.motor_pair_speed_left_actual = 0
        self.motor_pair_speed_right_actual = 0
        self.motor_pair_speed_max_error = 0
        self.drive_straight_state = 0
        self.drive_straight_status_index = 0
        self.drive_straight_left_port = 0
        self.drive_straight_right_port = 2
        self.drive_straight_target_distance = 500
        self.drive_straight_target_degrees = 1023
        self.drive_run_state = 0
        self.drive_run_status_index = 0
        self.drive_run_mode = 0
        self.drive_run_left_port = 0
        self.drive_run_right_port = 2
        self.drive_run_target = 720
        self.drive_run_speed = 40
        self.drive_steer_for_state = 0
        self.drive_steer_for_status_index = 0
        self.drive_steer_for_mode = 0
        self.drive_steer_for_left_port = 0
        self.drive_steer_for_right_port = 2
        self.drive_steer_for_steering = 50
        self.drive_steer_for_speed = 80
        self.drive_steer_for_target = 720
        self.drive_steer_for_left_speed = 80
        self.drive_steer_for_right_speed = 40
        self.drive_steer_for_left_target = 720
        self.drive_steer_for_right_target = 360
        self.sensor_state = 2
        self.sensor_type = sensor_type
        self.sensor_mode = 0
        self.sensor_value = sensor_value

    def __enter__(self) -> "FakeTransport":
        return self

    def __exit__(self, exc_type: object, exc: object, traceback: object) -> None:
        pass

    def write_payload(self, payload: bytes) -> None:
        self.payloads.append(payload)
        total = int.from_bytes(payload[5:7], "little")
        index = int.from_bytes(payload[7:9], "little")
        self.acks.append(build_ack(total, index, self.ack_flag))

    def write_report(self, report: bytes) -> None:
        self.reports.append(report)
        if report[0:3] == b"\x68\x11\x01":
            self.acks.append(build_heartbeat())
            return
        if report[0:3] == b"\x68\x11\x0d":
            self.acks.append(build_key_status(self.key_mask))
            return
        if report[0:3] == b"\x68\x11\x0e":
            self.acks.append(build_flash_id(self.jedec_id))
            return
        if report[0:3] == b"\x68\x11\x0f":
            supported = 1 if self.jedec_id == bytes.fromhex("EF4019") else 0
            self.acks.append(
                build_flash_diagnostic(
                    0x0F, supported, 1 if self.flash_sector_empty else 0
                )
            )
            return
        if report[0:3] == b"\x68\x11\x10":
            self.acks.append(
                build_flash_diagnostic(
                    0x10, self.flash_test_status, 1 if self.flash_test_status == 1 else 0
                )
            )
            return
        if report[0:3] == b"\x68\x11\x11":
            response = bytearray(build_flash_id(self.flash_status))
            response[2] = 0x11
            response[8] = checksum(response[:8])
            self.acks.append(bytes(response))
            return
        if report[0:3] == b"\x68\x11\x12":
            response = bytearray((0x68, 0x21, 0x12, 0x06, 0x00))
            response += self.flash_mode_probe
            response.append(checksum(response))
            response.append(0x16)
            self.acks.append(bytes(response).ljust(LEGACY_REPORT_SIZE, b"\x00"))
            return
        if report[0:3] == b"\x68\x11\x13":
            action = report[5]
            if action == 1:
                self.motor_status_index = 0
                state = 1
            elif action == 2:
                state = 0
            else:
                states = (2, 3, 4)
                state = states[min(self.motor_status_index, len(states) - 1)]
                self.motor_status_index += 1
            self.acks.append(build_motor_test_result(1, state))
            return
        if report[0:3] == b"\x68\x11\x14":
            action = report[5]
            if action == 1:
                self.motor_tacho_status_index = 0
                response = build_motor_tacho_test_result(
                    state=1, total=-1, forward=-1, reverse=0
                )
            elif action == 2:
                response = build_motor_tacho_test_result(state=0)
            else:
                responses = (
                    build_motor_tacho_test_result(
                        state=2, total=-24, forward=-24, reverse=0
                    ),
                    build_motor_tacho_test_result(
                        state=3, total=-10, forward=-24, reverse=14
                    ),
                    build_motor_tacho_test_result(
                        state=4, total=0, forward=-24, reverse=24
                    ),
                )
                response = responses[
                    min(self.motor_tacho_status_index, len(responses) - 1)
                ]
                self.motor_tacho_status_index += 1
            self.acks.append(response)
            return
        if report[0:3] == b"\x68\x11\x15":
            action = report[5]
            if action == 1:
                self.motor_stop_status_index = 0
                self.motor_stop_mode = report[6]
                response = build_motor_stop_test_result(
                    state=1, mode=self.motor_stop_mode, total=1,
                    powered=1, stopped=0
                )
            elif action == 2:
                response = build_motor_stop_test_result(
                    state=0, mode=self.motor_stop_mode, total=0,
                    powered=0, stopped=0
                )
            else:
                stopped = 48 if self.motor_stop_mode == 0 else 8
                responses = (
                    build_motor_stop_test_result(
                        state=2, mode=self.motor_stop_mode, total=100,
                        powered=90, stopped=10
                    ),
                    build_motor_stop_test_result(
                        state=3, mode=self.motor_stop_mode,
                        total=100 + stopped, powered=100, stopped=stopped
                    ),
                )
                response = responses[
                    min(self.motor_stop_status_index, len(responses) - 1)
                ]
                self.motor_stop_status_index += 1
            self.acks.append(response)
            return
        if report[0:3] == b"\x68\x11\x16":
            action = report[5]
            if action == 1:
                self.motor_dual_status_index = 0
                response = build_motor_dual_test_result(
                    state=1, a_forward=1, b_forward=1,
                    a_reverse=0, b_reverse=0
                )
            elif action == 2:
                response = build_motor_dual_test_result(
                    state=0, a_forward=0, b_forward=0,
                    a_reverse=0, b_reverse=0
                )
            else:
                states = (2, 3, 4, 5)
                state = states[
                    min(self.motor_dual_status_index, len(states) - 1)
                ]
                self.motor_dual_status_index += 1
                response = build_motor_dual_test_result(state=state)
            self.acks.append(response)
            return
        if report[0:3] == b"\x68\x11\x17":
            action = report[5]
            port = report[6]
            result = 1
            if port >= 4:
                result = 0
            elif action == 0:
                if self.motor_control_state[port] == 1:
                    direction = 1 if self.motor_control_power[port] > 0 else -1
                    self.motor_control_tacho[port] += direction * 120
            elif action == 1:
                power = int.from_bytes(report[7:8], "little", signed=True)
                self.motor_control_power[port] = power
                self.motor_control_state[port] = 1 if power else 0
            elif action == 2:
                if self.motor_control_state[port] == 1:
                    direction = 1 if self.motor_control_power[port] > 0 else -1
                    self.motor_control_tacho[port] += direction * 120
                self.motor_control_state[port] = 0
                self.motor_control_power[port] = 0
            elif action == 3:
                if self.motor_control_state[port] == 1:
                    direction = 1 if self.motor_control_power[port] > 0 else -1
                    self.motor_control_tacho[port] += direction * 120
                self.motor_control_state[port] = 2
                self.motor_control_power[port] = 0
            elif action == 4:
                self.motor_control_tacho[port] = 0
            else:
                result = 0
            self.acks.append(
                build_motor_control_result(
                    result=result,
                    port=port,
                    output_state=self.motor_control_state[port] if port < 4 else 0,
                    power=self.motor_control_power[port] if port < 4 else 0,
                    tacho=self.motor_control_tacho[port] if port < 4 else 0,
                )
            )
            return
        if report[0:3] == b"\x68\x11\x1b":
            action = report[5]
            port = report[6]
            if action == 1:
                self.motor_position_status_index = 0
                self.motor_position_port = port
                self.motor_position_speed = report[7]
                self.motor_position_target = int.from_bytes(
                    report[8:12], "little", signed=True
                )
                response = build_motor_position_result(
                    port=port, state=1, requested_speed=self.motor_position_speed,
                    start_count=0, target_count=self.motor_position_target,
                    current_count=0,
                )
            elif action == 2:
                response = build_motor_position_result(
                    port=port, state=0, requested_speed=0,
                    start_count=0, target_count=0, current_count=0,
                )
            else:
                current_values = (
                    self.motor_position_target * 2 // 3,
                    self.motor_position_target,
                )
                current = current_values[
                    min(self.motor_position_status_index, len(current_values) - 1)
                ]
                state = 2 if current == self.motor_position_target else 1
                self.motor_position_status_index += 1
                response = build_motor_position_result(
                    port=port, state=state,
                    requested_speed=self.motor_position_speed,
                    measured_speed=0 if state == 2 else self.motor_position_speed,
                    start_count=0, target_count=self.motor_position_target,
                    current_count=current,
                )
            self.acks.append(response)
            return
        if report[0:3] == b"\x68\x11\x1c":
            action = report[5]
            port = report[6]
            if action == 1:
                self.motor_speed_state = 1
                self.motor_speed_port = port
                self.motor_speed_target = int.from_bytes(
                    report[7:8], "little", signed=True
                )
            elif action == 0 and self.motor_speed_state == 1:
                direction = 1 if self.motor_speed_target > 0 else -1
                self.motor_speed_tacho += direction * 120
            elif action in (2, 3):
                self.motor_speed_state = 0
            output_state = 1 if self.motor_speed_state else (2 if action == 3 else 0)
            requested = self.motor_speed_target if self.motor_speed_state else 0
            measured = self.motor_speed_target if self.motor_speed_state else 0
            self.acks.append(
                build_motor_speed_result(
                    port=port,
                    state=self.motor_speed_state,
                    output_state=output_state,
                    requested_speed=requested,
                    measured_speed=measured,
                    power=requested,
                    tacho=self.motor_speed_tacho,
                )
            )
            return
        if report[0:3] == b"\x68\x11\x1d":
            action = report[5]
            if action == 1:
                self.motor_pair_state = 1
                self.motor_pair_status_index = 0
                self.motor_pair_left_port = report[6]
                self.motor_pair_right_port = report[7]
                self.motor_pair_left_target = int.from_bytes(
                    report[9:13], "little", signed=True
                )
                self.motor_pair_right_target = int.from_bytes(
                    report[13:17], "little", signed=True
                )
                left_actual = 0
                right_actual = 0
                sync_error = 0
                max_sync_error = 0
            elif action == 0 and self.motor_pair_state == 1:
                fractions = ((1, 2), (1, 1))
                numerator, denominator = fractions[
                    min(self.motor_pair_status_index, len(fractions) - 1)
                ]
                left_actual = self.motor_pair_left_target * numerator // denominator
                right_actual = self.motor_pair_right_target * numerator // denominator
                sync_error = 8 if denominator == 2 else 0
                max_sync_error = 8
                self.motor_pair_status_index += 1
                if denominator == 1:
                    self.motor_pair_state = 2
            elif action == 2:
                self.motor_pair_state = 0
                left_actual = self.motor_pair_left_target
                right_actual = self.motor_pair_right_target
                sync_error = 0
                max_sync_error = 8
            else:
                left_actual = 0
                right_actual = 0
                sync_error = 0
                max_sync_error = 0
            self.acks.append(
                build_motor_pair_position_result(
                    state=self.motor_pair_state,
                    left_port=self.motor_pair_left_port,
                    right_port=self.motor_pair_right_port,
                    left_target=self.motor_pair_left_target,
                    right_target=self.motor_pair_right_target,
                    left_actual=left_actual,
                    right_actual=right_actual,
                    sync_error=sync_error,
                    max_sync_error=max_sync_error,
                )
            )
            return
        if report[0:3] in (b"\x68\x11\x1e", b"\x68\x11\x21"):
            command = report[2]
            action = report[5]
            if action == 1:
                self.motor_pair_speed_state = 1
                self.motor_pair_speed_left_port = report[6]
                self.motor_pair_speed_right_port = report[7]
                if command == 0x21:
                    steering = int.from_bytes(report[8:9], "little", signed=True)
                    movement_speed = int.from_bytes(
                        report[9:10], "little", signed=True
                    )
                    if abs(steering) == 100:
                        left_raw, right_raw = steering, -steering
                    else:
                        left_raw = max(-100, min(100, 100 + steering))
                        right_raw = max(-100, min(100, 100 - steering))

                    def mix(raw: int) -> int:
                        product = raw * movement_speed
                        if product >= 0:
                            return (product + 50) // 100
                        return -((-product + 50) // 100)

                    self.motor_pair_speed_left_target = mix(left_raw)
                    self.motor_pair_speed_right_target = mix(right_raw)
                else:
                    self.motor_pair_speed_left_target = int.from_bytes(
                        report[8:9], "little", signed=True
                    )
                    self.motor_pair_speed_right_target = int.from_bytes(
                        report[9:10], "little", signed=True
                    )
                self.motor_pair_speed_left_actual = 0
                self.motor_pair_speed_right_actual = 0
                self.motor_pair_speed_max_error = 0
            elif action == 0 and self.motor_pair_speed_state == 1:
                left_direction = 1 if self.motor_pair_speed_left_target > 0 else -1
                right_direction = 1 if self.motor_pair_speed_right_target > 0 else -1
                self.motor_pair_speed_left_actual += left_direction * (
                    abs(self.motor_pair_speed_left_target) * 2
                )
                self.motor_pair_speed_right_actual += right_direction * max(
                    1, abs(self.motor_pair_speed_right_target) * 2 - 2
                )
                self.motor_pair_speed_max_error = 6
            elif action in (2, 3):
                self.motor_pair_speed_state = 0
            running = self.motor_pair_speed_state == 1
            self.acks.append(
                build_motor_pair_speed_result(
                    command=command,
                    state=self.motor_pair_speed_state,
                    left_port=self.motor_pair_speed_left_port,
                    right_port=self.motor_pair_speed_right_port,
                    left_requested=(self.motor_pair_speed_left_target if running else 0),
                    right_requested=(self.motor_pair_speed_right_target if running else 0),
                    left_measured=(self.motor_pair_speed_left_target if running else 0),
                    right_measured=(self.motor_pair_speed_right_target if running else 0),
                    left_power=(23 if running else 0),
                    right_power=(24 if running else 0),
                    left_actual=self.motor_pair_speed_left_actual,
                    right_actual=self.motor_pair_speed_right_actual,
                    sync_error=(2 if running else 0),
                    max_sync_error=self.motor_pair_speed_max_error,
                )
            )
            return
        if report[0:3] == b"\x68\x11\x1f":
            action = report[5]
            if action == 1:
                self.drive_straight_state = 1
                self.drive_straight_status_index = 0
                self.drive_straight_left_port = report[6]
                self.drive_straight_right_port = report[7]
                wheel_diameter = int.from_bytes(report[8:10], "little")
                self.drive_straight_target_distance = int.from_bytes(
                    report[12:16], "little", signed=True
                )
                numerator = self.drive_straight_target_distance * 360 * 113
                denominator = wheel_diameter * 355
                self.drive_straight_target_degrees = (
                    (numerator + denominator // 2) // denominator
                    if numerator >= 0
                    else -((-numerator + denominator // 2) // denominator)
                )
                fraction = 0
            elif action == 0 and self.drive_straight_state == 1:
                fractions = (2, 1)
                fraction = fractions[
                    min(self.drive_straight_status_index, len(fractions) - 1)
                ]
                self.drive_straight_status_index += 1
                if fraction == 1:
                    self.drive_straight_state = 2
            elif action == 2:
                self.drive_straight_state = 0
                fraction = 1
            else:
                fraction = 0
            actual_degrees = (
                self.drive_straight_target_degrees // fraction if fraction else 0
            )
            actual_distance = (
                self.drive_straight_target_distance // fraction if fraction else 0
            )
            self.acks.append(
                build_drive_straight_result(
                    state=self.drive_straight_state,
                    left_port=self.drive_straight_left_port,
                    right_port=self.drive_straight_right_port,
                    target_distance=self.drive_straight_target_distance,
                    actual_distance=actual_distance,
                    left_target=self.drive_straight_target_degrees,
                    right_target=self.drive_straight_target_degrees,
                    left_actual=actual_degrees,
                    right_actual=actual_degrees,
                    sync_error=0,
                    max_sync_error=5,
                )
            )
            return
        if report[0:3] == b"\x68\x11\x20":
            action = report[5]
            if action == 1:
                self.drive_run_state = 1
                self.drive_run_status_index = 0
                self.drive_run_left_port = report[6]
                self.drive_run_right_port = report[7]
                self.drive_run_mode = report[8]
                self.drive_run_target = int.from_bytes(
                    report[9:13], "little", signed=True
                )
                self.drive_run_speed = report[13]
                fraction = 0
            elif action == 0 and self.drive_run_state == 1:
                fractions = (2, 1)
                fraction = fractions[
                    min(self.drive_run_status_index, len(fractions) - 1)
                ]
                self.drive_run_status_index += 1
                if fraction == 1:
                    self.drive_run_state = 2
            elif action == 2:
                self.drive_run_state = 0
                fraction = 1
            else:
                fraction = 0
            actual_value = self.drive_run_target // fraction if fraction else 0
            if self.drive_run_mode == 0:
                actual_degrees = actual_value
            else:
                direction = -1 if self.drive_run_target < 0 else 1
                actual_degrees = direction * (360 // fraction) if fraction else 0
            requested_speed = (
                (-self.drive_run_speed if self.drive_run_target < 0 else self.drive_run_speed)
                if self.drive_run_mode == 1
                else self.drive_run_speed
            )
            self.acks.append(
                build_drive_run_result(
                    state=self.drive_run_state,
                    mode=self.drive_run_mode,
                    left_port=self.drive_run_left_port,
                    right_port=self.drive_run_right_port,
                    requested_speed=requested_speed,
                    target_value=self.drive_run_target,
                    actual_value=actual_value,
                    left_actual=actual_degrees,
                    right_actual=actual_degrees,
                    sync_error=0,
                    max_sync_error=5,
                )
            )
            return
        if report[0:3] == b"\x68\x11\x22":
            action = report[5]
            if action == 1:
                self.drive_steer_for_state = 1
                self.drive_steer_for_status_index = 0
                self.drive_steer_for_left_port = report[6]
                self.drive_steer_for_right_port = report[7]
                self.drive_steer_for_mode = report[8]
                self.drive_steer_for_steering = int.from_bytes(
                    report[9:10], "little", signed=True
                )
                self.drive_steer_for_speed = int.from_bytes(
                    report[10:11], "little", signed=True
                )
                self.drive_steer_for_target = int.from_bytes(
                    report[11:15], "little", signed=True
                )
                steering = self.drive_steer_for_steering
                if abs(steering) == 100:
                    left_raw, right_raw = steering, -steering
                else:
                    left_raw = max(-100, min(100, 100 + steering))
                    right_raw = max(-100, min(100, 100 - steering))

                def mix(raw: int) -> int:
                    product = raw * self.drive_steer_for_speed
                    if product >= 0:
                        return (product + 50) // 100
                    return -((-product + 50) // 100)

                self.drive_steer_for_left_speed = mix(left_raw)
                self.drive_steer_for_right_speed = mix(right_raw)
                maximum_speed = max(
                    abs(self.drive_steer_for_left_speed),
                    abs(self.drive_steer_for_right_speed),
                )

                def scale(speed: int) -> int:
                    product = self.drive_steer_for_target * speed
                    if product >= 0:
                        value = (product + maximum_speed // 2) // maximum_speed
                    else:
                        value = -((-product + maximum_speed // 2) // maximum_speed)
                    return value or (-1 if speed < 0 else 1)

                if self.drive_steer_for_mode == 0:
                    self.drive_steer_for_left_target = scale(
                        self.drive_steer_for_left_speed
                    )
                    self.drive_steer_for_right_target = scale(
                        self.drive_steer_for_right_speed
                    )
                else:
                    self.drive_steer_for_left_target = 0
                    self.drive_steer_for_right_target = 0
                fraction = 0
            elif action == 0 and self.drive_steer_for_state == 1:
                fractions = (2, 1)
                fraction = fractions[
                    min(self.drive_steer_for_status_index, len(fractions) - 1)
                ]
                self.drive_steer_for_status_index += 1
                if fraction == 1:
                    self.drive_steer_for_state = 2
            elif action == 2:
                self.drive_steer_for_state = 0
                fraction = 1
            else:
                fraction = 0
            actual_value = (
                self.drive_steer_for_target // fraction if fraction else 0
            )
            if self.drive_steer_for_mode == 0:
                left_actual = (
                    self.drive_steer_for_left_target // fraction if fraction else 0
                )
                right_actual = (
                    self.drive_steer_for_right_target // fraction if fraction else 0
                )
            else:
                left_actual = (
                    self.drive_steer_for_left_speed * 6 // fraction
                    if fraction else 0
                )
                right_actual = (
                    self.drive_steer_for_right_speed * 6 // fraction
                    if fraction else 0
                )
            self.acks.append(
                build_drive_steer_for_result(
                    state=self.drive_steer_for_state,
                    mode=self.drive_steer_for_mode,
                    left_port=self.drive_steer_for_left_port,
                    right_port=self.drive_steer_for_right_port,
                    steering=self.drive_steer_for_steering,
                    requested_speed=self.drive_steer_for_speed,
                    left_requested=self.drive_steer_for_left_speed,
                    right_requested=self.drive_steer_for_right_speed,
                    target_value=self.drive_steer_for_target,
                    actual_value=actual_value,
                    left_target=self.drive_steer_for_left_target,
                    right_target=self.drive_steer_for_right_target,
                    left_actual=left_actual,
                    right_actual=right_actual,
                    sync_error=0,
                    max_sync_error=5,
                )
            )
            return
        if report[0:3] == b"\x68\x11\x18":
            action = report[5]
            port = report[6]
            result = 1
            if port != 0:
                result = 0
            elif action == 1:
                self.sensor_mode = report[7]
                if self.sensor_type == 0x1E:
                    values = (123, 48, 1)
                elif self.sensor_type == 0x06:
                    values = (235, 743, 235)
                elif self.sensor_type == 0x20:
                    values = (123, 0xFFD6, 123)
                elif self.sensor_type == 0x03:
                    values = (self.sensor_value,) * 3
                elif self.sensor_type == 0x21:
                    values = (72, (65 << 8) | 0xF6, 5)
                else:
                    values = (42, 17, 5)
                self.sensor_value = values[self.sensor_mode]
            elif action == 2:
                self.sensor_state = 1
            elif action != 0:
                result = 0
            self.acks.append(
                build_input_sensor_result(
                    result=result,
                    port=port,
                    state=self.sensor_state,
                    sensor_type=self.sensor_type,
                    mode=self.sensor_mode,
                    value=self.sensor_value,
                    value_valid=self.sensor_state == 2,
                )
            )
            return
        if report[0:3] == b"\x68\x11\x19":
            self.acks.append(build_device_status_result())
            return
        if report[0:3] == b"\x68\x11\x1A":
            self.acks.append(build_motor_type_result())
            return
        if report[0:3] == b"\x68\x11\x05":
            total = int.from_bytes(report[5:7], "little")
            index = int.from_bytes(report[7:9], "little")
            self.acks.append(build_ack(total, index, self.ack_flag))

    def read_report(self, timeout_ms: int = 250) -> bytes | None:
        if self.acks:
            return self.acks.pop(0)
        return None


class UpdaterTests(unittest.TestCase):
    def test_ping_uses_transport_and_parses_version(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)
        self.assertEqual(updater.ping(), "M0.19A")
        self.assertEqual(len(transport.reports), 1)

    def test_reads_debounced_key_mask_from_device(self) -> None:
        updater = FirmwareUpdater(FakeTransport(key_mask=0x12))
        self.assertEqual(updater.read_key_mask(), 0x12)

    def test_reads_complete_device_status_in_one_request(self) -> None:
        transport = FakeTransport()
        status = FirmwareUpdater(transport).read_device_status()
        self.assertEqual(status.firmware_version, "M0.52A")
        self.assertEqual(status.battery_level, 4)
        self.assertEqual(len(status.motors), 4)
        self.assertEqual(len(status.sensors), 4)

    def test_reads_large_and_medium_motor_identification(self) -> None:
        result = FirmwareUpdater(FakeTransport()).read_motor_types()
        self.assertEqual(result.motors[0].motor_type, 4)
        self.assertEqual(result.motors[1].motor_type, 5)

    def test_refreshes_motor_identification_without_a_motor_drive_command(self) -> None:
        transport = FakeTransport()
        result = FirmwareUpdater(transport).refresh_motor_type(1)
        self.assertEqual(result.motors[1].motor_type, 5)
        self.assertEqual(transport.reports[-1][0:8], b"\x68\x11\x1a\x02\x00\x01\x01\x97")

    def test_starts_reads_and_stops_closed_loop_motor_speed(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)
        started = updater.start_motor_speed(1, -30)
        self.assertEqual(started.requested_speed_percent, -30)
        running = updater.read_motor_speed_status(1)
        self.assertEqual(running.measured_speed_percent, -30)
        stopped = updater.stop_motor_speed(1, "brake")
        self.assertEqual(stopped.output_state, 2)

    def test_reads_and_selects_input_sensor_mode(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)
        status = updater.read_input_sensor(0)
        self.assertEqual(status.sensor_type, 0x1D)
        self.assertEqual(status.value, 42)
        color = updater.set_input_sensor_mode(0, 2)
        self.assertEqual(color.mode, 2)
        self.assertEqual(color.value, 5)
        self.assertEqual(transport.reports[-1][5:8], bytes((1, 0, 2)))

    def test_reads_external_flash_jedec_id(self) -> None:
        updater = FirmwareUpdater(FakeTransport(jedec_id=bytes.fromhex("EF4017")))
        self.assertEqual(updater.read_flash_id(), bytes.fromhex("EF4017"))

    def test_scans_and_tests_external_flash_high_address(self) -> None:
        updater = FirmwareUpdater(FakeTransport(jedec_id=bytes.fromhex("EF4019")))
        scan = updater.scan_flash_test_sector()
        self.assertTrue(scan.supported)
        self.assertTrue(scan.erased)
        result = updater.test_flash_4byte_addressing()
        self.assertEqual(result.status, 1)
        self.assertTrue(result.restored)

    def test_reads_external_flash_protection_status(self) -> None:
        updater = FirmwareUpdater(FakeTransport(flash_status=bytes((0x3C, 0x40, 0x01))))
        status = updater.read_flash_status()
        self.assertEqual((status.status1, status.status2, status.status3), (0x3C, 0x40, 0x01))

    def test_probes_write_enable_and_address_mode_without_data_write(self) -> None:
        probe = FirmwareUpdater(FakeTransport()).probe_flash_modes()
        self.assertEqual(probe.status1_write_enabled & 0x02, 0x02)
        self.assertEqual(probe.status3_four_byte & 0x01, 0x01)
        self.assertEqual(probe.status3_restored & 0x01, 0x00)

    def test_starts_reads_and_stops_motor_test(self) -> None:
        updater = FirmwareUpdater(FakeTransport())
        self.assertEqual(updater.start_motor_test().state, 1)
        self.assertEqual(updater.read_motor_test_status().state, 2)
        self.assertEqual(updater.read_motor_test_status().state, 3)
        self.assertEqual(updater.read_motor_test_status().state, 4)
        self.assertEqual(updater.stop_motor_test().state, 0)

    def test_reads_signed_tacho_counts_for_both_motor_directions(self) -> None:
        updater = FirmwareUpdater(FakeTransport())
        self.assertEqual(updater.start_motor_tacho_test().state, 1)
        self.assertEqual(updater.read_motor_tacho_test_status().state, 2)
        self.assertEqual(updater.read_motor_tacho_test_status().state, 3)
        complete = updater.read_motor_tacho_test_status()
        self.assertEqual(complete.state, 4)
        self.assertEqual(complete.forward_count, -24)
        self.assertEqual(complete.reverse_count, 24)
        self.assertEqual(updater.stop_motor_tacho_test().state, 0)

    def test_starts_tacho_test_with_requested_power(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)
        self.assertEqual(updater.start_motor_tacho_test(100).state, 1)
        self.assertEqual(transport.reports[-1][5:7], bytes((1, 100)))

    def test_selects_motor_port_b_for_tacho_test(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)
        self.assertEqual(updater.start_motor_tacho_test(30, 1).state, 1)
        self.assertEqual(transport.reports[-1][5:8], bytes((1, 30, 1)))

    def test_compares_both_motor_stop_states(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)
        self.assertEqual(updater.start_motor_stop_test(0, 60).state, 1)
        self.assertEqual(updater.read_motor_stop_test_status().state, 2)
        complete = updater.read_motor_stop_test_status()
        self.assertEqual(complete.state, 3)
        self.assertEqual(complete.stopped_count, 48)
        self.assertEqual(updater.stop_motor_stop_test().state, 0)

    def test_runs_dual_motor_test_and_reads_both_tachos(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)
        self.assertEqual(updater.start_motor_dual_test(30).state, 1)
        self.assertEqual(updater.read_motor_dual_test_status().state, 2)
        self.assertEqual(updater.read_motor_dual_test_status().state, 3)
        self.assertEqual(updater.read_motor_dual_test_status().state, 4)
        complete = updater.read_motor_dual_test_status()
        self.assertEqual(complete.state, 5)
        self.assertEqual(complete.a_forward_count, 160)
        self.assertEqual(complete.b_reverse_count, -162)
        self.assertEqual(updater.stop_motor_dual_test().state, 0)

    def test_starts_reads_and_stops_synchronized_motor_pair(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)
        started = updater.start_motor_pair_position(2, 720, 3, -360, 20)
        self.assertEqual(started.state, 1)
        self.assertEqual((started.left_port, started.right_port), (2, 3))

        running = updater.read_motor_pair_position_status()
        self.assertEqual(running.state, 1)
        self.assertEqual(running.maximum_synchronization_error_degrees, 8)

        complete = updater.read_motor_pair_position_status()
        self.assertEqual(complete.state, 2)
        self.assertEqual(complete.left_actual_degrees, 720)
        self.assertEqual(complete.right_actual_degrees, -360)
        self.assertEqual(updater.stop_motor_pair_position().state, 0)

    def test_starts_reads_and_explicitly_stops_continuous_motor_pair(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)

        started = updater.start_motor_pair_speed(0, 20, 2, -20)
        self.assertEqual(started.state, 1)
        self.assertEqual((started.left_port, started.right_port), (0, 2))
        running = updater.read_motor_pair_speed_status()
        self.assertEqual(running.state, 1)
        self.assertEqual((running.left_actual_degrees,
                          running.right_actual_degrees), (40, -38))
        stopped = updater.stop_motor_pair_speed("brake")
        self.assertEqual(stopped.state, 0)
        self.assertEqual(transport.motor_pair_speed_state, 0)

    def test_starts_reads_and_stops_ev3_continuous_steering(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)

        started = updater.start_drive_steer(0, 2, 50, 80)
        self.assertEqual(started.state, 1)
        self.assertEqual(
            (started.left_requested_speed_percent,
             started.right_requested_speed_percent),
            (80, 40),
        )
        running = updater.read_drive_steer_status()
        self.assertEqual(running.state, 1)
        self.assertEqual(
            (running.left_actual_degrees, running.right_actual_degrees),
            (160, 78),
        )
        stopped = updater.stop_drive_steer("coast")
        self.assertEqual(stopped.state, 0)
        self.assertEqual(transport.motor_pair_speed_state, 0)

    def test_starts_reads_and_stops_drive_straight(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)

        started = updater.start_drive_straight(0, 2, 56, 120, 500, 40)
        self.assertEqual(started.state, 1)
        self.assertEqual(started.target_distance_mm, 500)
        running = updater.read_drive_straight_status()
        self.assertEqual(running.state, 1)
        self.assertEqual(running.actual_distance_mm, 250)
        complete = updater.read_drive_straight_status()
        self.assertEqual(complete.state, 2)
        self.assertEqual(complete.actual_distance_mm, 500)
        self.assertEqual(updater.stop_drive_straight().state, 0)

    def test_runs_drive_by_degrees_and_firmware_timed_seconds(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)

        started = updater.start_drive_run(0, 2, 0, 720, 40)
        self.assertEqual(started.state, 1)
        self.assertEqual(updater.read_drive_run_status().actual_value, 360)
        self.assertEqual(updater.read_drive_run_status().state, 2)
        self.assertEqual(updater.stop_drive_run().state, 0)

        timed = updater.start_drive_run(0, 2, 1, -3000, 60, "brake")
        self.assertEqual(timed.state, 1)
        timed_complete = updater.read_drive_run_status()
        self.assertEqual(timed_complete.actual_value, -1500)
        timed_complete = updater.read_drive_run_status()
        self.assertEqual(timed_complete.state, 2)
        self.assertEqual(timed_complete.requested_speed_percent, -60)
        self.assertEqual(updater.stop_drive_run().state, 0)

    def test_runs_finite_steering_by_degrees_and_time(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)

        started = updater.start_drive_steer_for(0, 2, 0, 50, 80, 720)
        self.assertEqual(started.state, 1)
        self.assertEqual(
            (started.left_requested_speed_percent,
             started.right_requested_speed_percent),
            (80, 40),
        )
        self.assertEqual(
            (started.left_target_degrees, started.right_target_degrees),
            (720, 360),
        )
        self.assertEqual(updater.read_drive_steer_for_status().actual_value, 360)
        self.assertEqual(updater.read_drive_steer_for_status().state, 2)
        self.assertEqual(updater.stop_drive_steer_for().state, 0)

        timed = updater.start_drive_steer_for(
            0, 2, 1, -50, -60, 3000, "brake"
        )
        self.assertEqual(timed.state, 1)
        self.assertEqual(updater.read_drive_steer_for_status().actual_value, 1500)
        self.assertEqual(updater.read_drive_steer_for_status().state, 2)
        self.assertEqual(updater.stop_drive_steer_for().state, 0)

    def test_flash_sends_high_speed_payloads_and_waits_for_matching_ack(self) -> None:
        transport = FakeTransport(output_len=1025)
        updater = FirmwareUpdater(transport)
        progress: list[PacketProgress] = []
        updater.flash(b"APP=" + bytes(1011), progress=progress.append)
        self.assertEqual(len(transport.payloads), 2)
        self.assertEqual(
            [(item.sent, item.total, item.phase) for item in progress],
            [(1, 2, "sending"), (1, 2, "acked"), (2, 2, "sending"), (2, 2, "acked")],
        )

    def test_flash_falls_back_to_legacy_64_byte_reports(self) -> None:
        transport = FakeTransport(output_len=64)
        updater = FirmwareUpdater(transport)
        updater.flash(b"APP=" + bytes(MAX_PAYLOAD - 4))
        self.assertGreater(len(transport.reports), 1)
        self.assertTrue(all(len(report) == LEGACY_REPORT_SIZE for report in transport.reports))

    def test_ping_timeout_has_specific_error(self) -> None:
        transport = FakeTransport()
        transport.write_report = mock.Mock()
        updater = FirmwareUpdater(transport)
        with mock.patch(
            "est_hid_sender.updater.time.monotonic", side_effect=(0.0, 10.0)
        ):
            with self.assertRaises(HeartbeatTimeoutError):
                updater.ping()

    def test_ack_timeout_has_specific_error(self) -> None:
        transport = FakeTransport()
        updater = FirmwareUpdater(transport)
        with mock.patch.object(transport, "read_report", return_value=None), mock.patch(
            "est_hid_sender.updater.time.monotonic", side_effect=(0.0, 20.0)
        ):
            with self.assertRaises(AckTimeoutError):
                updater.flash(b"APP=")

    def test_ack_flag_failure_has_specific_error(self) -> None:
        updater = FirmwareUpdater(FakeTransport(ack_flag=7))
        with self.assertRaisesRegex(AckRejectedError, "ACK flag=7"):
            updater.flash(b"APP=")


if __name__ == "__main__":
    unittest.main()
