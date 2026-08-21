"""Build an EST 3.0 Bootloader-compatible APP=-prefixed firmware package."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

APP_FLASH_START = 0x08010000
APP_FLASH_END = 0x08080000
SRAM_START = 0x20000000
SRAM_END = 0x20030000
CCM_START = 0x10000000
CCM_END = 0x10010000
PACKAGE_HEADER = b"APP="
DEFAULT_PACKAGE_SIZE = 256 * 1024
BOOTLOADER_MIN_STORED_LENGTH = 200 * 1024
BOOTLOADER_MAX_STORED_LENGTH = APP_FLASH_END - APP_FLASH_START


def validate_app_image(image: bytes) -> tuple[int, int]:
    if len(image) < 8:
        raise ValueError("APP image is too short to contain a vector table")
    if len(image) > BOOTLOADER_MAX_STORED_LENGTH:
        raise ValueError("APP image exceeds the APP Flash region")

    initial_msp, reset_handler = struct.unpack_from("<II", image)
    msp_valid = SRAM_START <= initial_msp <= SRAM_END
    msp_valid |= CCM_START <= initial_msp <= CCM_END
    reset_address = reset_handler & ~1
    reset_valid = bool(reset_handler & 1)
    reset_valid &= APP_FLASH_START <= reset_address < APP_FLASH_END
    if not msp_valid:
        raise ValueError(f"invalid initial MSP: 0x{initial_msp:08X}")
    if not reset_valid:
        raise ValueError(f"invalid Reset_Handler: 0x{reset_handler:08X}")
    return initial_msp, reset_handler


def build_packages(image: bytes, package_size: int) -> tuple[bytes, bytes]:
    validate_app_image(image)
    package = PACKAGE_HEADER + image
    unpadded = package
    if package_size:
        if package_size & 1:
            raise ValueError("final package size must be even")
        if len(package) > package_size:
            raise ValueError("APP image does not fit the requested package size")
        if package_size >= BOOTLOADER_MAX_STORED_LENGTH:
            raise ValueError("package does not satisfy the Bootloader upper limit")
        package = package.ljust(package_size, b"\xFF")
    if len(package) & 1:
        package += b"\xFF"
    return unpadded, package


def write_outputs(
    input_path: Path,
    output_dir: Path,
    stem: str,
    version: str,
    package_size: int,
) -> dict[str, object]:
    image = input_path.read_bytes()
    initial_msp, reset_handler = validate_app_image(image)
    unpadded, padded = build_packages(image, package_size)
    output_dir.mkdir(parents=True, exist_ok=True)

    app_package_path = output_dir / f"{stem}.app.bin"
    upgrade_path = output_dir / f"{stem}.upgrade.bin"
    manifest_path = output_dir / f"{stem}.manifest.json"
    app_package_path.write_bytes(unpadded)
    upgrade_path.write_bytes(padded)

    manifest: dict[str, object] = {
        "version": version,
        "input": input_path.name,
        "raw_app_size": len(image),
        "unpadded_package_size": len(unpadded),
        "upgrade_package_size": len(padded),
        "bootloader_stored_length": len(padded) - 1,
        "initial_msp": f"0x{initial_msp:08X}",
        "reset_handler": f"0x{reset_handler:08X}",
        "header_ascii": PACKAGE_HEADER.decode("ascii"),
        "sha256": hashlib.sha256(padded).hexdigest(),
    }
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=True, indent=2) + "\n", encoding="ascii"
    )
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--stem", default="est_minimal_upgrade_app")
    parser.add_argument("--version", default="unknown")
    parser.add_argument("--package-size", type=int, default=DEFAULT_PACKAGE_SIZE)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    manifest = write_outputs(
        args.input, args.output_dir, args.stem, args.version, args.package_size
    )
    print(json.dumps(manifest, ensure_ascii=True))


if __name__ == "__main__":
    main()
