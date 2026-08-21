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
