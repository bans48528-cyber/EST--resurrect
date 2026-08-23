import importlib.util
import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
PATCH_TOOL = (
    REPO_ROOT
    / "firmware"
    / "official_est3_app"
    / "tools"
    / "apply_w25q256_fix.py"
)
SPEC = importlib.util.spec_from_file_location("apply_w25q256_fix", PATCH_TOOL)
assert SPEC is not None and SPEC.loader is not None
PATCH_MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = PATCH_MODULE
SPEC.loader.exec_module(PATCH_MODULE)


FLASH_FIXTURE = """\
u16 W25QXX_TYPE=0x0000;
u8 * W25QXX_BUF;

void W25QXX_CS_0(void) { }
void W25QXX_CS_1(void)
{
}

void W25QXX_Init(void)
{
\tW25QXX_CS_1();
\tW25QXX_TYPE=W25QXX_ReadID();
}

u8 W25QXX_ReadSR(void)
{
\tu8 byte=0;
\tW25QXX_CS_1();
\treturn byte;
}

void W25QXX_Write_SR(u8 sr) { }

void W25QXX_Read(u8* pBuffer,u32 ReadAddr,u16 NumByteToRead)
{
\tW25QXX_CS_0();
\tSPI3_ReadWriteByte(W25X_ReadData);
\tSPI3_ReadWriteByte((u8)((ReadAddr)>>16));
\tSPI3_ReadWriteByte((u8)((ReadAddr)>>8));
\tSPI3_ReadWriteByte((u8)ReadAddr);
\tW25QXX_CS_1();
}

void W25QXX_Write_Page(u8* pBuffer,u32 WriteAddr,u16 NumByteToWrite)
{
\tW25QXX_Write_Enable();
\tSPI3_ReadWriteByte((u8)((WriteAddr)>>16));
\tSPI3_ReadWriteByte((u8)((WriteAddr)>>8));
\tSPI3_ReadWriteByte((u8)WriteAddr);
\tW25QXX_Wait_Busy();
}

void W25QXX_Erase_Sector(u32 Dst_Addr)
{
\tW25QXX_Write_Enable();
\tW25QXX_Wait_Busy();
\tSPI3_ReadWriteByte((u8)((Dst_Addr)>>16));
\tSPI3_ReadWriteByte((u8)((Dst_Addr)>>8));
\tSPI3_ReadWriteByte((u8)Dst_Addr);
\tW25QXX_Wait_Busy();
}

void W25QXX_PowerDown(void)
{
\tW25QXX_CS_0();
}
"""

DISK_FIXTURE = """\
#define EX_FLASH 1
#define FLASH_SECTOR_SIZE 4096
u16 FLASH_SECTOR_COUNT = 2048*28;
void disk_initialize(void)
{
    FLASH_SECTOR_COUNT=2048*28;
}
"""


def function_text(source: str, signature: str) -> str:
    start, end = PATCH_MODULE.function_span(source, signature)
    return source[start:end]


class LegacyW25Q256PatchTests(unittest.TestCase):
    def test_patch_uses_verified_four_byte_mode_sequence(self) -> None:
        source = PATCH_MODULE.patch_flash_source(FLASH_FIXTURE)
        read = function_text(source, "void W25QXX_Read(")
        write = function_text(source, "void W25QXX_Write_Page(")
        erase = function_text(source, "void W25QXX_Erase_Sector(")

        self.assertIn("W25X_Enter4ByteAddrMode 0xB7", source)
        self.assertIn("W25X_Exit4ByteAddrMode  0xE9", source)
        self.assertIn("address>>24", source)
        self.assertLess(read.index("W25QXX_Enter4ByteAddrMode"), read.index("W25X_ReadData"))
        self.assertLess(read.index("W25QXX_SendAddress"), read.index("W25QXX_Exit4ByteAddrMode"))
        self.assertLess(write.index("W25QXX_Enter4ByteAddrMode"), write.index("W25QXX_Write_Enable"))
        self.assertIn("W25QXX_SendAddress(WriteAddr)", write)
        self.assertLess(erase.index("W25QXX_Wait_Busy"), erase.index("W25QXX_Enter4ByteAddrMode"))
        self.assertLess(erase.index("W25QXX_Enter4ByteAddrMode"), erase.index("W25QXX_Write_Enable"))
        self.assertIn("W25QXX_SendAddress(Dst_Addr)", erase)

    def test_patch_restores_bootloader_mode_and_is_repeatable(self) -> None:
        source = PATCH_MODULE.patch_flash_source(FLASH_FIXTURE)
        status = function_text(source, "u8 W25QXX_ReadSR(")
        power_down = function_text(source, "void W25QXX_PowerDown(")
        init = function_text(source, "void W25QXX_Init(")

        self.assertIn("((byte&0x01)==0)&&W25QXX_4BYTE_ACTIVE", status)
        self.assertIn("W25QXX_Exit4ByteAddrMode", status)
        self.assertIn("W25QXX_Exit4ByteAddrMode", power_down)
        self.assertLess(init.index("W25QXX_Set4ByteAddrMode(0)"), init.index("W25QXX_ReadID"))
        self.assertEqual(PATCH_MODULE.patch_flash_source(source), source)

    def test_patch_corrects_28mb_capacity_and_is_repeatable(self) -> None:
        source = PATCH_MODULE.patch_disk_source(DISK_FIXTURE)

        self.assertEqual(28 * 1024 * 1024 // 4096, 7168)
        self.assertIn("FLASH_FATFS_SIZE_MB 28", source)
        self.assertIn("FLASH_FATFS_SECTOR_COUNT", source)
        self.assertNotIn("2048*28", source)
        self.assertEqual(PATCH_MODULE.patch_disk_source(source), source)


if __name__ == "__main__":
    unittest.main()
