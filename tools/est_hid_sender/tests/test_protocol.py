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
    build_flash_id_frame,
    build_flash_scan_frame,
    build_flash_test_frame,
    build_flash_status_frame,
    build_flash_mode_probe_frame,
    build_heartbeat_frame,
    build_key_status_frame,
    build_motor_control_frame,
    build_motor_dual_test_frame,
    build_motor_stop_test_frame,
    build_motor_tacho_test_frame,
    build_motor_test_frame,
    build_update_frame,
    checksum,
    parse_heartbeat_response,
    parse_flash_id_response,
    parse_flash_scan_response,
    parse_flash_test_response,
    parse_flash_status_response,
    parse_flash_mode_probe_response,
    parse_key_status_response,
    parse_motor_control_response,
    parse_motor_dual_test_response,
    parse_motor_stop_test_response,
    parse_motor_tacho_test_response,
    parse_motor_test_response,
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
