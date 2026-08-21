from __future__ import annotations

import hashlib
import json
import math
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .constants import MAX_PAYLOAD, SUPPORTED_FIRMWARE_HEADERS
from .errors import (
    FirmwareValidationError,
    ManifestMismatchError,
    ManifestNotFoundError,
)


@dataclass(frozen=True)
class FirmwareManifest:
    path: Path
    version: str
    header_ascii: str
    upgrade_package_size: int
    sha256: str


@dataclass(frozen=True)
class FirmwarePackage:
    path: Path
    data: bytes
    header: bytes
    sha256: str
    manifest: FirmwareManifest | None = None

    @property
    def size(self) -> int:
        return len(self.data)

    @property
    def total_frames(self) -> int:
        return math.ceil(len(self.data) / MAX_PAYLOAD)

    @property
    def target_version(self) -> str | None:
        return self.manifest.version if self.manifest is not None else None


def detect_header(data: bytes) -> bytes:
    for header in SUPPORTED_FIRMWARE_HEADERS:
        if data.startswith(header):
            return header
    raise FirmwareValidationError("升级包头无效：必须以 EST 或 APP= 开头")


def discover_manifest_path(package_path: Path) -> Path | None:
    name = package_path.name
    if name.endswith(".upgrade.bin"):
        candidate = package_path.with_name(
            name[: -len(".upgrade.bin")] + ".manifest.json"
        )
        if candidate.is_file():
            return candidate
    candidate = package_path.with_suffix(".manifest.json")
    return candidate if candidate.is_file() else None


def _require_manifest_field(data: dict[str, Any], name: str, expected_type: type) -> Any:
    value = data.get(name)
    if not isinstance(value, expected_type):
        raise FirmwareValidationError(
            f"manifest 字段 {name!r} 缺失或类型错误，应为 {expected_type.__name__}"
        )
    return value


def load_manifest(path: Path) -> FirmwareManifest:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ManifestNotFoundError(f"未找到 manifest：{path}") from exc
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise FirmwareValidationError(f"无法读取 manifest：{path}（{exc}）") from exc
    if not isinstance(raw, dict):
        raise FirmwareValidationError("manifest 顶层必须是 JSON 对象")
    return FirmwareManifest(
        path=path,
        version=_require_manifest_field(raw, "version", str),
        header_ascii=_require_manifest_field(raw, "header_ascii", str),
        upgrade_package_size=_require_manifest_field(raw, "upgrade_package_size", int),
        sha256=_require_manifest_field(raw, "sha256", str).lower(),
    )


def verify_manifest(package: FirmwarePackage, manifest: FirmwareManifest) -> None:
    mismatches: list[str] = []
    header_ascii = package.header.decode("ascii")
    if manifest.header_ascii != header_ascii:
        mismatches.append(f"header={header_ascii}，manifest={manifest.header_ascii}")
    if manifest.upgrade_package_size != package.size:
        mismatches.append(
            f"size={package.size}，manifest={manifest.upgrade_package_size}"
        )
    if not re.fullmatch(r"[0-9a-f]{64}", manifest.sha256):
        mismatches.append("manifest SHA256 格式无效")
    elif manifest.sha256 != package.sha256:
        mismatches.append(f"sha256={package.sha256}，manifest={manifest.sha256}")
    if not manifest.version.strip():
        mismatches.append("manifest version 为空")
    if mismatches:
        raise ManifestMismatchError("manifest 与升级包不一致：" + "; ".join(mismatches))


def load_firmware_package(
    path: Path,
    manifest_path: Path | None = None,
    *,
    require_manifest: bool = False,
) -> FirmwarePackage:
    try:
        data = path.read_bytes()
    except FileNotFoundError as exc:
        raise FirmwareValidationError(f"未找到升级包：{path}") from exc
    except OSError as exc:
        raise FirmwareValidationError(f"无法读取升级包：{path}（{exc}）") from exc
    if not data:
        raise FirmwareValidationError("升级包为空")
    header = detect_header(data)
    package = FirmwarePackage(
        path=path,
        data=data,
        header=header,
        sha256=hashlib.sha256(data).hexdigest(),
    )
    resolved_manifest = manifest_path or discover_manifest_path(path)
    if resolved_manifest is None:
        if require_manifest:
            raise ManifestNotFoundError(
                "未找到配套 manifest；请用 --manifest 指定，或确认它与升级包同目录同名"
            )
        return package
    manifest = load_manifest(resolved_manifest)
    verify_manifest(package, manifest)
    return FirmwarePackage(
        path=package.path,
        data=package.data,
        header=package.header,
        sha256=package.sha256,
        manifest=manifest,
    )


_VERSION_PATTERN = re.compile(r"^([A-Za-z]*)(\d+)\.(\d+)([A-Za-z]*)$")


def compare_versions(current: str, target: str) -> int | None:
    """Compare known EST versions, returning None for incomparable version families."""
    current_match = _VERSION_PATTERN.fullmatch(current)
    target_match = _VERSION_PATTERN.fullmatch(target)
    if current_match is None or target_match is None:
        return None
    current_family = (current_match.group(1).upper(), current_match.group(4).upper())
    target_family = (target_match.group(1).upper(), target_match.group(4).upper())
    if current_family != target_family:
        return None
    current_number = (int(current_match.group(2)), int(current_match.group(3)))
    target_number = (int(target_match.group(2)), int(target_match.group(3)))
    return (target_number > current_number) - (target_number < current_number)
