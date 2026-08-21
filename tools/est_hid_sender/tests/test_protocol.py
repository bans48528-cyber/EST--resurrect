from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent))

from est_hid_sender.constants import (  # noqa: E402
    FRAME_END,
    LEGACY_REPORT_SIZE,
    MAX_PAYLOAD,
)
from est_hid_sender.firmware import detect_header, load_firmware_package  # noqa: E402
from est_hid_sender.protocol import (  # noqa: E402
    build_frame,
    build_heartbeat_frame,
    build_update_frame,
    checksum,
    parse_heartbeat_response,
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


class FirmwareTests(unittest.TestCase):
    def test_accepts_existing_est_and_app_upgrade_headers(self) -> None:
        self.assertEqual(detect_header(b"APP=" + b"\xFF"), b"APP=")
        self.assertEqual(detect_header(b"EST" + b"\xFF"), b"EST")

    def test_rejects_unknown_firmware_header(self) -> None:
        with self.assertRaisesRegex(ValueError, "not EST/APP="):
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
    def __init__(self, output_len: int = 1025) -> None:
        self.input_len = 1025
        self.output_len = output_len
        self.path = "fake"
        self.payloads: list[bytes] = []
        self.reports: list[bytes] = []
        self.acks: list[bytes] = []

    def write_payload(self, payload: bytes) -> None:
        self.payloads.append(payload)
        total = int.from_bytes(payload[5:7], "little")
        index = int.from_bytes(payload[7:9], "little")
        self.acks.append(build_ack(total, index))

    def write_report(self, report: bytes) -> None:
        self.reports.append(report)
        if report[0:3] == b"\x68\x11\x01":
            self.acks.append(build_heartbeat())
            return
        if report[0:3] == b"\x68\x11\x05":
            total = int.from_bytes(report[5:7], "little")
            index = int.from_bytes(report[7:9], "little")
            self.acks.append(build_ack(total, index))

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


if __name__ == "__main__":
    unittest.main()
