from __future__ import annotations

import hashlib
import math
from dataclasses import dataclass
from pathlib import Path

from .constants import MAX_PAYLOAD, SUPPORTED_FIRMWARE_HEADERS


@dataclass(frozen=True)
class FirmwarePackage:
    path: Path
    data: bytes
    header: bytes
    sha256: str

    @property
    def size(self) -> int:
        return len(self.data)

    @property
    def total_frames(self) -> int:
        return math.ceil(len(self.data) / MAX_PAYLOAD)


def detect_header(data: bytes) -> bytes:
    for header in SUPPORTED_FIRMWARE_HEADERS:
        if data.startswith(header):
            return header
    raise ValueError("firmware package is not EST/APP= upgrade format")


def load_firmware_package(path: Path) -> FirmwarePackage:
    data = path.read_bytes()
    header = detect_header(data)
    return FirmwarePackage(
        path=path,
        data=data,
        header=header,
        sha256=hashlib.sha256(data).hexdigest(),
    )
