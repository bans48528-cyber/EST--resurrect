from __future__ import annotations

import importlib.util
import struct
import sys
import tempfile
import unittest
from pathlib import Path

try:
    import cffi
except ImportError:  # pragma: no cover
    cffi = None


ROOT = Path(__file__).resolve().parents[1]
GENERATOR_PATH = ROOT / "tools" / "generate_display_images.py"
ASSETS_DIR = ROOT / "assets" / "display_images"

SPEC = importlib.util.spec_from_file_location("display_image_generator", GENERATOR_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("cannot load display image generator")
GENERATOR = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = GENERATOR
SPEC.loader.exec_module(GENERATOR)


EXPECTED_NAMES = [
    "Expressions/Big smile",
    "Expressions/Heart large",
    "Expressions/Heart small",
    "Expressions/Mouth 1 open",
    "Expressions/Mouth 1 shut",
    "Expressions/Mouth 2 open",
    "Expressions/Mouth 2 shut",
    "Expressions/Sad",
    "Expressions/Sick",
    "Expressions/Smile",
    "Expressions/Swearing",
    "Expressions/Talking",
    "Expressions/Wink",
    "Expressions/ZZZ",
    "Eyes/Angry",
    "Eyes/Awake",
    "Eyes/Black eye",
    "Eyes/Bottom left",
    "Eyes/Bottom right",
    "Eyes/Crazy 1",
    "Eyes/Crazy 2",
    "Eyes/Disappointed",
    "Eyes/Dizzy",
    "Eyes/Down",
    "Eyes/Evil",
    "Eyes/Hurt",
    "Eyes/Knocked out",
    "Eyes/Love",
    "Eyes/Middle left",
    "Eyes/Middle right",
    "Eyes/Neutral",
    "Eyes/Nuclear",
    "Eyes/Pinch left",
    "Eyes/Pinch middle",
    "Eyes/Pinch right",
    "Eyes/Tear",
    "Eyes/Tired left",
    "Eyes/Tired middle",
    "Eyes/Tired right",
    "Eyes/Toxic",
    "Eyes/Up",
    "Eyes/Winking",
]


def make_bmp(
    rows: list[list[bool]], *, top_down: bool = False, black_index: int = 1
) -> bytes:
    height = len(rows)
    width = len(rows[0])
    stride = ((width + 31) // 32) * 4
    pixel_rows = []
    source_rows = rows if top_down else list(reversed(rows))
    for row in source_rows:
        packed = bytearray(stride)
        for x, black in enumerate(row):
            palette_index = black_index if black else 1 - black_index
            if palette_index:
                packed[x // 8] |= 0x80 >> (x % 8)
        pixel_rows.append(bytes(packed))
    pixels = b"".join(pixel_rows)
    palette = (
        b"\x00\x00\x00\x00\xff\xff\xff\x00"
        if black_index == 0
        else b"\xff\xff\xff\x00\x00\x00\x00\x00"
    )
    pixel_offset = 14 + 40 + len(palette)
    signed_height = -height if top_down else height
    file_size = pixel_offset + len(pixels)
    file_header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, pixel_offset)
    dib_header = struct.pack(
        "<IiiHHIIiiII",
        40,
        width,
        signed_height,
        1,
        1,
        0,
        len(pixels),
        0,
        0,
        2,
        2,
    )
    return file_header + dib_header + palette + pixels


class DisplayImageConversionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.resources = GENERATOR.load_resources(ASSETS_DIR)

    def test_stable_names_and_all_42_assets(self) -> None:
        self.assertEqual(
            [resource.name for resource in self.resources], EXPECTED_NAMES
        )
        self.assertEqual(len(self.resources), 42)
        self.assertEqual(
            sum(len(resource.compressed) for resource in self.resources), 38346
        )

    def test_all_assets_round_trip_through_delta_and_packbits(self) -> None:
        expected_size = ((GENERATOR.LCD_WIDTH + 7) // 8) * GENERATOR.LCD_HEIGHT
        for resource in self.resources:
            with self.subTest(resource=resource.name):
                delta = GENERATOR.packbits_decode(
                    resource.compressed, expected_size
                )
                packed = GENERATOR.decode_row_delta(
                    delta, resource.width, resource.height
                )
                self.assertEqual(packed, resource.packed)

    def test_orientation_palette_polarity_and_centering(self) -> None:
        rows = [
            [True, False, False, False],
            [False, False, False, True],
        ]
        with tempfile.TemporaryDirectory() as directory:
            directory_path = Path(directory)
            variants = []
            for top_down in (False, True):
                for black_index in (0, 1):
                    path = directory_path / f"{top_down}-{black_index}.bmp"
                    path.write_bytes(
                        make_bmp(
                            rows, top_down=top_down, black_index=black_index
                        )
                    )
                    variants.append(GENERATOR.read_monochrome_bmp(path))

        for image in variants:
            self.assertEqual(image.pixels, tuple(tuple(row) for row in rows))
            packed = GENERATOR.pack_for_lcd(image)
            stride = (GENERATOR.LCD_WIDTH + 7) // 8
            offset_x = (GENERATOR.LCD_WIDTH - image.width) // 2
            offset_y = (GENERATOR.LCD_HEIGHT - image.height) // 2

            def black(x: int, y: int) -> bool:
                value = packed[y * stride + x // 8]
                return bool(value & (0x80 >> (x % 8)))

            self.assertTrue(black(offset_x, offset_y))
            self.assertFalse(black(offset_x + 1, offset_y))
            self.assertTrue(black(offset_x + 3, offset_y + 1))
            self.assertFalse(black(0, 0))

    def test_packbits_rejects_truncated_and_wrong_sized_data(self) -> None:
        with self.assertRaisesRegex(ValueError, "truncated"):
            GENERATOR.packbits_decode(b"\x80", 1)
        with self.assertRaisesRegex(ValueError, "wrong size"):
            GENERATOR.packbits_decode(b"\x80\x00", 2)


@unittest.skipIf(cffi is None, "cffi is required for native framebuffer tests")
class DisplayImageNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.resources = GENERATOR.load_resources(ASSETS_DIR)
        cls.temporary_directory = tempfile.TemporaryDirectory()
        generated_path = (
            Path(cls.temporary_directory.name) / "display_image_resources.c"
        )
        generated_path.write_text(
            GENERATOR.generate_c(cls.resources), encoding="ascii", newline="\n"
        )
        cls.ffi = cffi.FFI()
        cls.ffi.cdef(
            """
            typedef struct {
                const char *name;
                uint16_t width;
                uint16_t height;
                uint32_t data_offset;
                uint32_t data_size;
            } board_lcd_image_resource_t;
            const board_lcd_image_resource_t *board_lcd_image_find(
                const char *name);
            _Bool board_lcd_image_decode(uint8_t *, uint16_t, uint16_t,
                uint16_t, uint16_t, uint16_t, uint16_t,
                const uint8_t *, size_t);
            _Bool board_lcd_image_draw(uint8_t *, uint16_t, uint16_t,
                uint16_t, uint16_t, const board_lcd_image_resource_t *);
            """
        )
        cls.renderer = cls.ffi.verify(
            '#include "board_lcd_image.c"\n'
            '#include "display_image_resources.c"\n',
            include_dirs=[
                str(ROOT / "include"),
                str(ROOT / "src"),
                cls.temporary_directory.name,
            ],
        )

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temporary_directory.cleanup()

    @staticmethod
    def framebuffer_pixel(framebuffer, x: int, y: int) -> bool:
        return bool(framebuffer[x * (GENERATOR.LCD_HEIGHT // 8) + y // 8] & (1 << (y % 8)))

    def test_native_decoder_matches_all_42_source_images(self) -> None:
        framebuffer_size = GENERATOR.LCD_WIDTH * (GENERATOR.LCD_HEIGHT // 8)
        source_stride = (GENERATOR.LCD_WIDTH + 7) // 8
        for resource in self.resources:
            with self.subTest(resource=resource.name):
                framebuffer = self.ffi.new("uint8_t[]", framebuffer_size)
                entry = self.renderer.board_lcd_image_find(
                    resource.name.encode("ascii")
                )
                self.assertNotEqual(entry, self.ffi.NULL)
                self.assertTrue(
                    self.renderer.board_lcd_image_draw(
                        framebuffer,
                        GENERATOR.LCD_WIDTH,
                        GENERATOR.LCD_HEIGHT,
                        0,
                        0,
                        entry,
                    )
                )
                for y in range(GENERATOR.LCD_HEIGHT):
                    for x in range(GENERATOR.LCD_WIDTH):
                        expected = bool(
                            resource.packed[y * source_stride + x // 8]
                            & (0x80 >> (x % 8))
                        )
                        self.assertEqual(
                            self.framebuffer_pixel(framebuffer, x, y), expected
                        )

    def test_unknown_name_and_boundaries_are_rejected_without_overrun(self) -> None:
        self.assertEqual(
            self.renderer.board_lcd_image_find(b"Eyes/Does not exist"),
            self.ffi.NULL,
        )
        entry = self.renderer.board_lcd_image_find(b"Eyes/Neutral")
        framebuffer_size = GENERATOR.LCD_WIDTH * (GENERATOR.LCD_HEIGHT // 8)
        storage = self.ffi.new("uint8_t[]", framebuffer_size + 2)
        storage[0] = 0xA5
        storage[framebuffer_size + 1] = 0x5A
        framebuffer = storage + 1
        self.assertTrue(
            self.renderer.board_lcd_image_draw(
                framebuffer, 180, 128, 0, 0, entry
            )
        )
        self.assertEqual(storage[0], 0xA5)
        self.assertEqual(storage[framebuffer_size + 1], 0x5A)
        self.assertFalse(
            self.renderer.board_lcd_image_draw(
                framebuffer, 180, 128, 1, 0, entry
            )
        )
        self.assertFalse(
            self.renderer.board_lcd_image_draw(
                framebuffer, 179, 128, 0, 0, entry
            )
        )
        self.assertEqual(storage[0], 0xA5)
        self.assertEqual(storage[framebuffer_size + 1], 0x5A)

    def test_native_decoder_rejects_truncated_stream(self) -> None:
        framebuffer = self.ffi.new("uint8_t[]", 180 * 16)
        truncated = self.ffi.new("uint8_t[]", [0x80])
        self.assertFalse(
            self.renderer.board_lcd_image_decode(
                framebuffer, 180, 128, 0, 0, 180, 128, truncated, 1
            )
        )


class DisplayImageApiTests(unittest.TestCase):
    def test_image_api_is_explicit_refresh_and_unknown_is_value_error(self) -> None:
        modest = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        display = (ROOT / "src" / "est_display.c").read_text(encoding="utf-8")
        self.assertIn("MP_QSTR_image", modest)
        self.assertIn('MP_ERROR_TEXT("unknown display image")', modest)
        self.assertIn("est_display_image(const char *name)", display)
        image_function = display.split("est_display_image(const char *name)", 1)[1]
        image_function = image_function.split("void est_display_refresh", 1)[0]
        self.assertNotIn("refresh", image_function)


if __name__ == "__main__":
    unittest.main()
