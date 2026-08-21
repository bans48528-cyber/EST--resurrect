"""Verify the linked EST APP and its EST 3.0 APP=-prefixed upgrade package."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path

APP_FLASH_START = 0x08010000
APP_FLASH_END = 0x08080000
SRAM_START = 0x20000000
SRAM_END = 0x20030000
CCM_START = 0x10000000
CCM_END = 0x10010000
PACKAGE_HEADER = b"APP="
BOOTLOADER_MIN_STORED_LENGTH = 200 * 1024
BOOTLOADER_COPY_STEP_BYTES = 2048
BOOTLOADER_COPY_BYTES = 4096
USB_DEVICE_DESCRIPTOR = bytes.fromhex(
    "12 01 00 02 00 00 00 40 83 04 50 57 00 02 01 02 03 01"
)
USB_IN_ENDPOINT_DESCRIPTOR = bytes.fromhex("07 05 82 03 00 04 01")
USB_OUT_ENDPOINT_DESCRIPTOR = bytes.fromhex("07 05 01 03 00 04 01")
USB_PRODUCT_STRING = b"EST USB HS Mode\x00"
USB_SERIAL_NUMBER = b"00000000011B\x00"
USB_DEVICE_QUALIFIER_DESCRIPTOR = bytes.fromhex(
    "0A 06 00 02 00 00 00 40 01 00"
)

HID_REPORT_DESCRIPTOR = bytes(
    (
        0x05,
        0x8C,
        0x09,
        0x01,
        0xA1,
        0x01,
        0x09,
        0x03,
        0x15,
        0x00,
        0x26,
        0x00,
        0xFF,
        0x75,
        0x08,
        0x96,
        0x00,
        0x04,
        0x81,
        0x02,
        0x09,
        0x04,
        0x15,
        0x00,
        0x26,
        0x00,
        0xFF,
        0x75,
        0x08,
        0x96,
        0x00,
        0x04,
        0x91,
        0x02,
        0xC0,
    )
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def simulate_current_bootloader_copy(upgrade: bytes) -> bytes:
    """Reproduce the EST 3.0 Bootloader's update-to-APP copy."""
    stored_length = len(upgrade) - 1
    block_count = stored_length // BOOTLOADER_COPY_STEP_BYTES

    required_source_length = len(PACKAGE_HEADER) + (
        block_count * BOOTLOADER_COPY_STEP_BYTES
    ) + BOOTLOADER_COPY_BYTES
    source = upgrade.ljust(required_source_length, b"\xFF")
    destination = bytearray([0xFF]) * (APP_FLASH_END - APP_FLASH_START)
    written = bytearray(len(destination))
    for index in range(block_count + 1):
        source_offset = len(PACKAGE_HEADER) + index * BOOTLOADER_COPY_STEP_BYTES
        destination_offset = index * BOOTLOADER_COPY_STEP_BYTES
        destination_end = destination_offset + BOOTLOADER_COPY_BYTES
        require(
            destination_end <= len(destination),
            "legacy Bootloader copy would overrun the APP region",
        )
        block = source[source_offset : source_offset + BOOTLOADER_COPY_BYTES]
        require(
            len(block) == BOOTLOADER_COPY_BYTES,
            "legacy Bootloader simulation source is too short",
        )
        for byte_index, value in enumerate(block, destination_offset):
            require(
                not written[byte_index] or destination[byte_index] == value,
                "legacy Bootloader overlap would program conflicting data",
            )
            destination[byte_index] = value
            written[byte_index] = 1
    return bytes(destination)


def section_vma(objdump: str, elf_path: Path, section: str) -> int:
    result = subprocess.run(
        (objdump, "-h", str(elf_path)),
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    pattern = re.compile(
        rf"^\s*\d+\s+{re.escape(section)}\s+[0-9A-Fa-f]+\s+"
        r"([0-9A-Fa-f]+)\s+",
        re.MULTILINE,
    )
    match = pattern.search(result.stdout)
    if match is None:
        raise ValueError(f"ELF section {section!r} was not found")
    return int(match.group(1), 16)


def elf_has_symbol(objdump: str, elf_path: Path, symbol: str) -> bool:
    result = subprocess.run(
        (objdump, "-t", str(elf_path)),
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return re.search(rf"\b{re.escape(symbol)}$", result.stdout, re.MULTILINE) is not None


def verify_version_pair(
    first_image: bytes,
    first_version: str,
    second_image: bytes,
    second_version: str,
) -> None:
    first_bytes = first_version.encode("ascii")
    second_bytes = second_version.encode("ascii")
    require(len(first_bytes) == 6, "first version must contain six ASCII bytes")
    require(len(second_bytes) == 6, "second version must contain six ASCII bytes")
    require(len(first_image) == len(second_image), "versioned APP sizes differ")
    require(first_image.count(first_bytes) == 1, "first version is not unique")
    require(second_image.count(second_bytes) == 1, "second version is not unique")

    offset = first_image.index(first_bytes)
    require(
        second_image[offset : offset + len(second_bytes)] == second_bytes,
        "version strings are stored at different offsets",
    )
    expected = bytearray(first_image)
    expected[offset : offset + len(first_bytes)] = second_bytes
    require(
        bytes(expected) == second_image,
        "versioned APPs differ outside the six-byte version string",
    )


def verify_artifacts(
    objdump: str,
    elf_path: Path,
    bin_path: Path,
    app_package_path: Path,
    upgrade_path: Path,
    manifest_path: Path,
    version: str,
) -> dict[str, object]:
    image = bin_path.read_bytes()
    app_package = app_package_path.read_bytes()
    upgrade = upgrade_path.read_bytes()
    manifest = json.loads(manifest_path.read_text(encoding="ascii"))

    require(section_vma(objdump, elf_path, ".text") == APP_FLASH_START,
            ".text is not linked at the APP start address")
    require(
        elf_has_symbol(objdump, elf_path, "est_otghs_ulpi_usb_driver"),
        "EST OTG HS ULPI driver is not linked into the APP",
    )
    require(len(image) >= 8, "APP image is too short")
    initial_msp, reset_handler = struct.unpack_from("<II", image)
    reset_address = reset_handler & ~1
    require(
        SRAM_START <= initial_msp <= SRAM_END
        or CCM_START <= initial_msp <= CCM_END,
        f"invalid initial MSP: 0x{initial_msp:08X}",
    )
    require(reset_handler & 1 == 1, "Reset_Handler is not a Thumb address")
    require(
        APP_FLASH_START <= reset_address < APP_FLASH_END,
        f"Reset_Handler is outside APP Flash: 0x{reset_handler:08X}",
    )
    reset_offset = reset_address - APP_FLASH_START
    require(reset_offset + 2 <= len(image), "Reset_Handler is outside the binary")
    require(
        image[reset_offset : reset_offset + 2] == b"\x72\xB6",
        "Reset_Handler no longer starts with 'cpsid i'",
    )

    version_bytes = version.encode("ascii")
    require(len(version_bytes) == 6, "version must contain six ASCII bytes")
    require(image.count(version_bytes) == 1, "version string is not unique in APP")
    require(
        image.count(HID_REPORT_DESCRIPTOR) == 1,
        "legacy HID report descriptor is missing or duplicated",
    )
    require(
        image.count(USB_DEVICE_DESCRIPTOR) == 1,
        "legacy USB device descriptor is missing or duplicated",
    )
    require(
        image.count(USB_IN_ENDPOINT_DESCRIPTOR) == 2,
        "EST 3.0 HID IN endpoint descriptors are missing or duplicated",
    )
    require(
        image.count(USB_OUT_ENDPOINT_DESCRIPTOR) == 2,
        "EST 3.0 HID OUT endpoint descriptors are missing or duplicated",
    )
    require(
        image.count(USB_PRODUCT_STRING) == 1,
        "EST 3.0 USB HS product string is missing or duplicated",
    )
    require(
        image.count(USB_SERIAL_NUMBER) == 1,
        "EST 3.0 USB HS serial number is missing or duplicated",
    )
    require(
        image.count(USB_DEVICE_QUALIFIER_DESCRIPTOR) == 1,
        "USB high-speed device qualifier is missing or duplicated",
    )

    require(app_package == PACKAGE_HEADER + image,
            "app package is not APP= followed by the raw APP")
    require(upgrade.startswith(app_package),
            "upgrade package is not APP= followed by the raw APP")
    require(set(upgrade[len(app_package):]) <= {0xFF},
            "upgrade package padding is not 0xFF-filled")
    stored_length = len(upgrade) - 1
    require(len(upgrade) < APP_FLASH_END - APP_FLASH_START,
            "upgrade length does not pass the old Bootloader maximum")

    copied_app = simulate_current_bootloader_copy(upgrade)
    require(
        copied_app[: len(image)] == image,
        "current Bootloader simulation does not reproduce the raw APP",
    )

    digest = hashlib.sha256(upgrade).hexdigest()
    expected_manifest = {
        "version": version,
        "raw_app_size": len(image),
        "unpadded_package_size": len(app_package),
        "upgrade_package_size": len(upgrade),
        "bootloader_stored_length": stored_length,
        "initial_msp": f"0x{initial_msp:08X}",
        "reset_handler": f"0x{reset_handler:08X}",
        "header_ascii": "APP=",
        "sha256": digest,
    }
    for key, expected in expected_manifest.items():
        require(manifest.get(key) == expected,
                f"manifest field {key!r} does not match the artifact")
    return expected_manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--bin", type=Path)
    parser.add_argument("--app-package", type=Path)
    parser.add_argument("--upgrade-package", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--version")
    parser.add_argument("--compare-bin", nargs=2, type=Path, metavar=("FIRST", "SECOND"))
    parser.add_argument(
        "--compare-version", nargs=2, metavar=("FIRST_VERSION", "SECOND_VERSION")
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.compare_bin is not None or args.compare_version is not None:
        if args.compare_bin is None or args.compare_version is None:
            raise SystemExit("--compare-bin and --compare-version must be used together")
        verify_version_pair(
            args.compare_bin[0].read_bytes(),
            args.compare_version[0],
            args.compare_bin[1].read_bytes(),
            args.compare_version[1],
        )
        print("version pair verified")
        return

    required = (
        args.elf,
        args.bin,
        args.app_package,
        args.upgrade_package,
        args.manifest,
        args.version,
    )
    if any(value is None for value in required):
        raise SystemExit("artifact verification requires all artifact paths and --version")
    verified = verify_artifacts(
        args.objdump,
        args.elf,
        args.bin,
        args.app_package,
        args.upgrade_package,
        args.manifest,
        args.version,
    )
    print(json.dumps(verified, ensure_ascii=True))


if __name__ == "__main__":
    main()
