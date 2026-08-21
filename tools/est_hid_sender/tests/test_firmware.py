from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT.parent))

from est_hid_sender.errors import ManifestMismatchError, ManifestNotFoundError  # noqa: E402
from est_hid_sender.firmware import compare_versions, load_firmware_package  # noqa: E402


def write_package_with_manifest(directory: Path, version: str = "M0.20A") -> Path:
    package_path = directory / "est_test.upgrade.bin"
    data = b"APP=" + bytes(1020)
    package_path.write_bytes(data)
    manifest = {
        "version": version,
        "header_ascii": "APP=",
        "upgrade_package_size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }
    (directory / "est_test.manifest.json").write_text(
        json.dumps(manifest), encoding="utf-8"
    )
    return package_path


class ManifestTests(unittest.TestCase):
    def test_auto_discovers_and_verifies_matching_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            package = load_firmware_package(
                write_package_with_manifest(Path(temp)), require_manifest=True
            )
            self.assertEqual(package.target_version, "M0.20A")
            self.assertEqual(package.manifest.header_ascii, "APP=")

    def test_rejects_manifest_sha_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            package_path = write_package_with_manifest(directory)
            manifest_path = directory / "est_test.manifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["sha256"] = "0" * 64
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(ManifestMismatchError, "sha256"):
                load_firmware_package(package_path, require_manifest=True)

    def test_strict_verification_requires_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "orphan.upgrade.bin"
            path.write_bytes(b"APP=")
            with self.assertRaises(ManifestNotFoundError):
                load_firmware_package(path, require_manifest=True)


class VersionTests(unittest.TestCase):
    def test_compares_est_versions(self) -> None:
        self.assertEqual(compare_versions("M0.19A", "M0.20A"), 1)
        self.assertEqual(compare_versions("M0.20A", "M0.20A"), 0)
        self.assertEqual(compare_versions("M0.20A", "M0.19A"), -1)

    def test_does_not_compare_different_version_families(self) -> None:
        self.assertIsNone(compare_versions("03.00B", "M0.20A"))


if __name__ == "__main__":
    unittest.main()
