#!/usr/bin/env python3
"""Apply the verified W25Q256 4-byte-address fix to an EST 3.0 Project tree."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


FLASH_RELATIVE_PATH = Path("src/SYSTEM/w25qxx.c")
DISK_RELATIVE_PATH = Path("src/FATFS/src/diskio.c")
MARKER = "W25X_Enter4ByteAddrMode"


@dataclass(frozen=True)
class TextFile:
    text: str
    encoding: str
    newline: str


def read_source(path: Path) -> TextFile:
    data = path.read_bytes()
    try:
        decoded = data.decode("utf-8")
        encoding = "utf-8"
    except UnicodeDecodeError:
        decoded = data.decode("gb18030")
        encoding = "gb18030"
    newline = "\r\n" if "\r\n" in decoded else "\n"
    return TextFile(decoded.replace("\r\n", "\n"), encoding, newline)


def function_span(source: str, signature: str) -> tuple[int, int]:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return start, index + 1
    raise ValueError(f"函数没有结束：{signature}")


def replace_function(source: str, signature: str, transform) -> str:
    start, end = function_span(source, signature)
    return source[:start] + transform(source[start:end]) + source[end:]


def replace_address_lines(block: str, address: str) -> str:
    pattern = re.compile(
        rf"^[ \t]*SPI3_ReadWriteByte\(\(u8\)\(\({address}\)>>16\)\);[^\n]*\n"
        rf"^[ \t]*SPI3_ReadWriteByte\(\(u8\)\(\({address}\)>>8\)\);[^\n]*\n"
        rf"^[ \t]*SPI3_ReadWriteByte\(\(u8\){address}\);[^\n]*$",
        re.MULTILINE,
    )
    replacement = f"\tW25QXX_SendAddress({address});"
    changed, count = pattern.subn(replacement, block)
    if count != 1:
        raise ValueError(f"未找到唯一的三字节地址代码：{address}")
    return changed


def patch_flash_source(source: str) -> str:
    if MARKER in source:
        validate_flash_source(source)
        return source

    source = source.replace(
        "u8 * W25QXX_BUF;",
        "u8 * W25QXX_BUF;\n\n"
        "#define W25X_Enter4ByteAddrMode 0xB7\n"
        "#define W25X_Exit4ByteAddrMode  0xE9\n\n"
        "static u8 W25QXX_4BYTE_ACTIVE=0;",
        1,
    )

    _, cs_end = function_span(source, "void W25QXX_CS_1(void)")
    helpers = """

static void W25QXX_Set4ByteAddrMode(u8 enable)
{
\tW25QXX_CS_0();
\tSPI3_ReadWriteByte(enable ? W25X_Enter4ByteAddrMode : W25X_Exit4ByteAddrMode);
\tW25QXX_CS_1();
\tW25QXX_4BYTE_ACTIVE=enable;
}

static void W25QXX_Enter4ByteAddrMode(void)
{
\tif(W25QXX_TYPE==W25Q256)W25QXX_Set4ByteAddrMode(1);
}

static void W25QXX_Exit4ByteAddrMode(void)
{
\tif(W25QXX_TYPE==W25Q256)W25QXX_Set4ByteAddrMode(0);
}

static void W25QXX_SendAddress(u32 address)
{
\tif(W25QXX_TYPE==W25Q256)SPI3_ReadWriteByte((u8)(address>>24));
\tSPI3_ReadWriteByte((u8)(address>>16));
\tSPI3_ReadWriteByte((u8)(address>>8));
\tSPI3_ReadWriteByte((u8)address);
}
"""
    source = source[:cs_end] + helpers + source[cs_end:]

    def patch_init(block: str) -> str:
        return block.replace(
            "\tW25QXX_TYPE=W25QXX_ReadID();",
            "\tW25QXX_Set4ByteAddrMode(0);\t//恢复引导程序使用的3字节模式\n"
            "\tW25QXX_TYPE=W25QXX_ReadID();",
            1,
        )

    def patch_status(block: str) -> str:
        return block.replace(
            "\treturn byte;",
            "\tif(((byte&0x01)==0)&&W25QXX_4BYTE_ACTIVE)"
            "W25QXX_Exit4ByteAddrMode();\n\treturn byte;",
            1,
        )

    def patch_read(block: str) -> str:
        block = block.replace(
            "\tW25QXX_CS_0();",
            "\tW25QXX_Enter4ByteAddrMode();\n\tW25QXX_CS_0();",
            1,
        )
        block = replace_address_lines(block, "ReadAddr")
        return block[:-1] + "\tW25QXX_Exit4ByteAddrMode();\n}"

    def patch_write_page(block: str) -> str:
        block = block.replace(
            "\tW25QXX_Write_Enable();",
            "\tW25QXX_Enter4ByteAddrMode();\n\tW25QXX_Write_Enable();",
            1,
        )
        return replace_address_lines(block, "WriteAddr")

    def patch_erase(block: str) -> str:
        lines = block.splitlines()
        write_index = next(
            index for index, line in enumerate(lines) if "W25QXX_Write_Enable();" in line
        )
        wait_index = next(
            index
            for index in range(write_index + 1, len(lines))
            if "W25QXX_Wait_Busy();" in lines[index]
        )
        write_line = lines.pop(write_index)
        wait_index -= 1
        lines.insert(wait_index + 1, "\tW25QXX_Enter4ByteAddrMode();")
        lines.insert(wait_index + 2, write_line)
        return replace_address_lines("\n".join(lines), "Dst_Addr")

    def patch_power_down(block: str) -> str:
        opening = block.index("{") + 1
        return block[:opening] + "\n\tW25QXX_Exit4ByteAddrMode();" + block[opening:]

    source = replace_function(source, "void W25QXX_Init(void)", patch_init)
    source = replace_function(source, "u8 W25QXX_ReadSR(void)", patch_status)
    source = replace_function(source, "void W25QXX_Read(", patch_read)
    source = replace_function(source, "void W25QXX_Write_Page(", patch_write_page)
    source = replace_function(source, "void W25QXX_Erase_Sector(", patch_erase)
    source = replace_function(source, "void W25QXX_PowerDown(void)", patch_power_down)
    validate_flash_source(source)
    return source


def validate_flash_source(source: str) -> None:
    required = (
        "W25X_Enter4ByteAddrMode 0xB7",
        "W25X_Exit4ByteAddrMode  0xE9",
        "address>>24",
        "W25QXX_SendAddress(ReadAddr)",
        "W25QXX_SendAddress(WriteAddr)",
        "W25QXX_SendAddress(Dst_Addr)",
        "W25QXX_Set4ByteAddrMode(0)",
        "((byte&0x01)==0)&&W25QXX_4BYTE_ACTIVE",
    )
    missing = [item for item in required if item not in source]
    if missing:
        raise ValueError(f"Flash 修正不完整：{missing}")


def patch_disk_source(source: str) -> str:
    if "FLASH_FATFS_SECTOR_COUNT" not in source:
        lines = source.splitlines()
        sector_size_index = next(
            index for index, line in enumerate(lines) if "#define FLASH_SECTOR_SIZE" in line
        )
        lines[sector_size_index + 1:sector_size_index + 1] = [
            "",
            "#define FLASH_FATFS_SIZE_MB 28",
            "#define FLASH_FATFS_SECTOR_COUNT "
            "(FLASH_FATFS_SIZE_MB*1024*1024/FLASH_SECTOR_SIZE)",
        ]
        source = "\n".join(lines)
    source = source.replace("2048*28", "FLASH_FATFS_SECTOR_COUNT")
    validate_disk_source(source)
    return source


def validate_disk_source(source: str) -> None:
    if "FLASH_FATFS_SIZE_MB 28" not in source or "2048*28" in source:
        raise ValueError("FATFS 容量没有修正为 28 MiB / 7168 扇区")


def write_source(path: Path, original: TextFile, text: str) -> None:
    encoded = text.replace("\n", original.newline).encode(original.encoding)
    temporary = path.with_suffix(path.suffix + ".w25q256.tmp")
    temporary.write_bytes(encoded)
    temporary.replace(path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="修正 EST 3.0 官方主控的 W25Q256 高地址访问")
    parser.add_argument("--project-root", type=Path, required=True, help="EST 3.0 Project 目录")
    parser.add_argument("--apply", action="store_true", help="实际写入；省略时只检查")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    flash_path = args.project_root / FLASH_RELATIVE_PATH
    disk_path = args.project_root / DISK_RELATIVE_PATH
    flash = read_source(flash_path)
    disk = read_source(disk_path)
    patched_flash = patch_flash_source(flash.text)
    patched_disk = patch_disk_source(disk.text)
    changed = patched_flash != flash.text or patched_disk != disk.text

    if args.apply and changed:
        write_source(flash_path, flash, patched_flash)
        write_source(disk_path, disk, patched_disk)
    print(f"flash={flash_path}")
    print(f"disk={disk_path}")
    print(f"status={'applied' if args.apply and changed else 'already-applied' if not changed else 'needs-apply'}")
    print("fatfs_sector_count=7168")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
