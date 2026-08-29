#!/usr/bin/env python3
"""Convert the built-in expression BMPs to firmware C resources."""

from __future__ import annotations

import argparse
import json
import struct
from dataclasses import dataclass
from pathlib import Path


LCD_WIDTH = 180
LCD_HEIGHT = 128
EXPECTED_COUNTS = {"Expressions": 14, "Eyes": 28}


@dataclass(frozen=True)
class MonoImage:
    width: int
    height: int
    pixels: tuple[tuple[bool, ...], ...]


@dataclass(frozen=True)
class ImageResource:
    name: str
    width: int
    height: int
    compressed: bytes
    packed: bytes


def _require_range(data: bytes, offset: int, size: int, field: str) -> None:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError(f"BMP {field} is truncated")


def read_monochrome_bmp(path: Path) -> MonoImage:
    data = path.read_bytes()
    if len(data) < 62 or data[:2] != b"BM":
        raise ValueError(f"{path}: not a BMP file")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    if dib_size < 40:
        raise ValueError(f"{path}: unsupported BMP header")
    _require_range(data, 14, dib_size, "header")
    width, signed_height = struct.unpack_from("<ii", data, 18)
    planes, bits_per_pixel = struct.unpack_from("<HH", data, 26)
    compression = struct.unpack_from("<I", data, 30)[0]
    if width <= 0 or signed_height == 0:
        raise ValueError(f"{path}: invalid BMP dimensions")
    if planes != 1 or bits_per_pixel != 1 or compression != 0:
        raise ValueError(f"{path}: expected an uncompressed 1-bit BMP")

    height = abs(signed_height)
    palette_offset = 14 + dib_size
    _require_range(data, palette_offset, 8, "palette")
    palette = []
    for index in range(2):
        blue, green, red, _ = struct.unpack_from(
            "<BBBB", data, palette_offset + index * 4
        )
        palette.append(red + green + blue)
    if palette[0] == palette[1]:
        raise ValueError(f"{path}: monochrome palette has no black/white contrast")
    black_index = 0 if palette[0] < palette[1] else 1

    row_stride = ((width + 31) // 32) * 4
    _require_range(data, pixel_offset, row_stride * height, "pixels")
    top_down = signed_height < 0
    rows = []
    for y in range(height):
        source_y = y if top_down else height - 1 - y
        row_offset = pixel_offset + source_y * row_stride
        row = []
        for x in range(width):
            palette_index = (
                data[row_offset + x // 8] >> (7 - (x % 8))
            ) & 1
            row.append(palette_index == black_index)
        rows.append(tuple(row))
    return MonoImage(width, height, tuple(rows))


def pack_for_lcd(
    image: MonoImage, width: int = LCD_WIDTH, height: int = LCD_HEIGHT
) -> bytes:
    if image.width > width or image.height > height:
        raise ValueError("source image does not fit the LCD")
    offset_x = (width - image.width) // 2
    offset_y = (height - image.height) // 2
    stride = (width + 7) // 8
    packed = bytearray(stride * height)
    for source_y, row in enumerate(image.pixels):
        target_y = offset_y + source_y
        for source_x, black in enumerate(row):
            if black:
                target_x = offset_x + source_x
                packed[target_y * stride + target_x // 8] |= (
                    0x80 >> (target_x % 8)
                )
    return bytes(packed)


def encode_row_delta(packed: bytes, width: int, height: int) -> bytes:
    stride = (width + 7) // 8
    if len(packed) != stride * height:
        raise ValueError("packed image size does not match dimensions")
    previous = bytearray(stride)
    delta = bytearray()
    for y in range(height):
        row = packed[y * stride : (y + 1) * stride]
        delta.extend(value ^ previous[x] for x, value in enumerate(row))
        previous[:] = row
    return bytes(delta)


def decode_row_delta(delta: bytes, width: int, height: int) -> bytes:
    stride = (width + 7) // 8
    if len(delta) != stride * height:
        raise ValueError("delta image size does not match dimensions")
    previous = bytearray(stride)
    packed = bytearray()
    for y in range(height):
        row_delta = delta[y * stride : (y + 1) * stride]
        row = bytes(value ^ previous[x] for x, value in enumerate(row_delta))
        packed.extend(row)
        previous[:] = row
    return bytes(packed)


def packbits_encode(data: bytes) -> bytes:
    encoded = bytearray()
    index = 0
    while index < len(data):
        run_length = 1
        while (
            index + run_length < len(data)
            and data[index + run_length] == data[index]
            and run_length < 128
        ):
            run_length += 1
        if run_length >= 3:
            encoded.extend((0x80 | (run_length - 1), data[index]))
            index += run_length
            continue

        literal_start = index
        index += run_length
        while index < len(data) and index - literal_start < 128:
            next_run = 1
            while (
                index + next_run < len(data)
                and data[index + next_run] == data[index]
                and next_run < 128
            ):
                next_run += 1
            if next_run >= 3:
                break
            remaining = 128 - (index - literal_start)
            index += min(next_run, remaining)
        encoded.append(index - literal_start - 1)
        encoded.extend(data[literal_start:index])
    return bytes(encoded)


def packbits_decode(data: bytes, expected_size: int) -> bytes:
    decoded = bytearray()
    index = 0
    while index < len(data):
        control = data[index]
        index += 1
        count = (control & 0x7F) + 1
        if control & 0x80:
            if index >= len(data):
                raise ValueError("truncated PackBits run")
            decoded.extend((data[index],) * count)
            index += 1
        else:
            if index + count > len(data):
                raise ValueError("truncated PackBits literal")
            decoded.extend(data[index : index + count])
            index += count
        if len(decoded) > expected_size:
            raise ValueError("PackBits output exceeds expected size")
    if len(decoded) != expected_size:
        raise ValueError("PackBits output has the wrong size")
    return bytes(decoded)


def load_resources(assets_dir: Path) -> list[ImageResource]:
    paths = sorted(
        assets_dir.glob("*/*.bmp"),
        key=lambda path: path.relative_to(assets_dir).as_posix().lower(),
    )
    counts = {category: 0 for category in EXPECTED_COUNTS}
    resources = []
    for path in paths:
        category = path.parent.name
        if category not in EXPECTED_COUNTS:
            raise ValueError(f"unsupported display image category: {category}")
        counts[category] += 1
        name = path.relative_to(assets_dir).with_suffix("").as_posix()
        if not name.isascii():
            raise ValueError(f"resource name must be ASCII: {name}")
        image = read_monochrome_bmp(path)
        packed = pack_for_lcd(image)
        delta = encode_row_delta(packed, LCD_WIDTH, LCD_HEIGHT)
        compressed = packbits_encode(delta)
        resources.append(
            ImageResource(name, LCD_WIDTH, LCD_HEIGHT, compressed, packed)
        )
    if counts != EXPECTED_COUNTS:
        raise ValueError(
            f"expected display image counts {EXPECTED_COUNTS}, found {counts}"
        )
    return resources


def _format_bytes(data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 12):
        chunk = data[offset : offset + 12]
        lines.append("\t" + ", ".join(f"0x{value:02X}U" for value in chunk) + ",")
    return "\n".join(lines)


def generate_c(resources: list[ImageResource]) -> str:
    data = b"".join(resource.compressed for resource in resources)
    table_lines = []
    offset = 0
    for resource in resources:
        table_lines.append(
            "\t{%s, %dU, %dU, %dU, %dU},"
            % (
                json.dumps(resource.name),
                resource.width,
                resource.height,
                offset,
                len(resource.compressed),
            )
        )
        offset += len(resource.compressed)
    return "\n".join(
        (
            "/* Generated by tools/generate_display_images.py. */",
            "#include <stddef.h>",
            "#include <stdint.h>",
            "",
            '#include "board_lcd_image.h"',
            "",
            "const uint8_t board_lcd_image_data[] = {",
            _format_bytes(data),
            "};",
            "",
            "const size_t board_lcd_image_data_size =",
            "\tsizeof(board_lcd_image_data);",
            "",
            "const board_lcd_image_resource_t board_lcd_image_resources[] = {",
            *table_lines,
            "};",
            "",
            "const size_t board_lcd_image_resource_count =",
            "\tsizeof(board_lcd_image_resources) /",
            "\tsizeof(board_lcd_image_resources[0]);",
            "",
        )
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()

    resources = load_resources(arguments.assets_dir)
    output = generate_c(resources)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(output, encoding="ascii", newline="\n")
    compressed_size = sum(len(resource.compressed) for resource in resources)
    print(
        f"display_images={len(resources)} compressed_bytes={compressed_size} "
        f"output={arguments.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
