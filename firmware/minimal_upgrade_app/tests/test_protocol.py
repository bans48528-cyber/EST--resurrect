from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from package_firmware import (  # noqa: E402
    APP_FLASH_START,
    DEFAULT_PACKAGE_SIZE,
    build_packages,
    validate_app_image,
    write_outputs,
)
from protocol_reference import (  # noqa: E402
    FRAME_END,
    MAX_PAYLOAD,
    REPORT_SIZE,
    build_update_frame,
    checksum,
    parse_update_ack,
    split_hid_reports,
)
from verify_build import simulate_current_bootloader_copy, verify_version_pair  # noqa: E402


class PackageTests(unittest.TestCase):
    @staticmethod
    def valid_image() -> bytes:
        return (0x20030000).to_bytes(4, "little") + (
            APP_FLASH_START + 0x101
        ).to_bytes(4, "little") + b"minimal-app"

    def test_package_has_header_padding_and_legacy_length(self) -> None:
        unpadded, padded = build_packages(self.valid_image(), DEFAULT_PACKAGE_SIZE)
        self.assertTrue(unpadded.startswith(b"APP="))
        self.assertEqual(len(padded), 256 * 1024)
        self.assertEqual(padded[4 : 4 + len(self.valid_image())], self.valid_image())
        self.assertEqual(set(padded[len(unpadded):]), {0xFF})

    def test_stack_top_at_end_of_sram_is_valid(self) -> None:
        msp, reset = validate_app_image(self.valid_image())
        self.assertEqual(msp, 0x20030000)
        self.assertEqual(reset & 1, 1)

    def test_manifest_matches_output(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            image_path = directory / "app.bin"
            image_path.write_bytes(self.valid_image())
            manifest = write_outputs(
                image_path, directory, "test", "M0.01A", DEFAULT_PACKAGE_SIZE
            )
            self.assertEqual(manifest["header_ascii"], "APP=")
            self.assertEqual(
                (directory / "test.upgrade.bin").stat().st_size,
                len(build_packages(self.valid_image(), DEFAULT_PACKAGE_SIZE)[1]),
            )

    def test_rejects_non_thumb_reset_handler(self) -> None:
        image = (0x20001000).to_bytes(4, "little") + APP_FLASH_START.to_bytes(
            4, "little"
        )
        with self.assertRaisesRegex(ValueError, "Reset_Handler"):
            validate_app_image(image)


class ProtocolTests(unittest.TestCase):
    def test_max_frame_fits_one_hs_report(self) -> None:
        payload = b"APP=" + bytes(MAX_PAYLOAD - 4)
        frame = build_update_frame(260, 0, payload)
        self.assertEqual(len(frame), 1021)
        reports = split_hid_reports(frame)
        self.assertEqual(len(reports), 1)
        self.assertTrue(all(len(report) == REPORT_SIZE for report in reports))
        rebuilt = b"".join(reports)[: len(frame)]
        self.assertEqual(rebuilt, frame)
        self.assertEqual(frame[-1], FRAME_END)
        self.assertEqual(checksum(frame[:-2]), frame[-2])

    def test_parse_zero_padded_ack(self) -> None:
        report = bytearray(REPORT_SIZE)
        report[:10] = bytes((0x68, 0x21, 0x05, 0x05, 0x00, 0x04, 0x01, 0x03, 0x00, 0x01))
        report[10] = checksum(report[:10])
        report[11] = 0x16
        ack = parse_update_ack(bytes(report))
        self.assertEqual(ack.total_frames, 260)
        self.assertEqual(ack.frame_index, 3)
        self.assertEqual(ack.flag, 1)


class BuildVerificationTests(unittest.TestCase):
    def test_legacy_bootloader_overlap_reproduces_raw_app(self) -> None:
        image = PackageTests.valid_image()
        _, upgrade = build_packages(image, DEFAULT_PACKAGE_SIZE)
        copied_app = simulate_current_bootloader_copy(upgrade)
        self.assertEqual(copied_app[: len(image)], image)

    def test_version_pair_differs_only_in_version_text(self) -> None:
        first = b"prefix-M0.01A-suffix"
        second = b"prefix-M0.02A-suffix"
        verify_version_pair(first, "M0.01A", second, "M0.02A")

    def test_version_pair_rejects_an_unrelated_change(self) -> None:
        first = b"prefix-M0.01A-suffix"
        second = b"Prefix-M0.02A-suffix"
        with self.assertRaisesRegex(ValueError, "outside"):
            verify_version_pair(first, "M0.01A", second, "M0.02A")


if __name__ == "__main__":
    unittest.main()
