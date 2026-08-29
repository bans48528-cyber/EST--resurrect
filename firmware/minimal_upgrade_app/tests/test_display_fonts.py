from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

try:
    from cffi import FFI
except ImportError:  # pragma: no cover - the firmware build can run without cffi
    FFI = None


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(FFI is None, "cffi is required for native framebuffer tests")
class DisplayFontFramebufferTests(unittest.TestCase):
    REGULAR_BLACK = 0
    BOLD_BLACK = 1
    LARGE_BLACK = 2
    REGULAR_WHITE = 3
    BOLD_WHITE = 4
    LARGE_WHITE = 5
    GLYPH_A = (0x7E, 0x11, 0x11, 0x11, 0x7E)

    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            typedef enum {
                BOARD_LCD_TEXT_REGULAR_BLACK = 0,
                BOARD_LCD_TEXT_BOLD_BLACK = 1,
                BOARD_LCD_TEXT_LARGE_BLACK = 2,
                BOARD_LCD_TEXT_REGULAR_WHITE = 3,
                BOARD_LCD_TEXT_BOLD_WHITE = 4,
                BOARD_LCD_TEXT_LARGE_WHITE = 5
            } board_lcd_text_style_t;
            _Bool board_lcd_text_draw_legacy(
                uint8_t *, uint16_t, uint16_t, uint16_t, uint16_t,
                const char *, uint8_t);
            _Bool board_lcd_text_draw_style(
                uint8_t *, uint16_t, uint16_t, uint16_t, uint16_t,
                const char *, board_lcd_text_style_t);
            """
        )
        renderer_source = (ROOT / "src" / "board_lcd_text.c").read_bytes()
        source_hash = hashlib.sha256(renderer_source).hexdigest()[:16]
        cls.renderer = cls.ffi.verify(
            f'#define BOARD_LCD_TEXT_TEST_SOURCE_HASH "{source_hash}"\n'
            '#include "board_lcd_text.c"',
            include_dirs=[str(ROOT / "include"), str(ROOT / "src")],
        )

    @staticmethod
    def pixel(buffer, height: int, x: int, y: int) -> bool:
        pages = (height + 7) // 8
        return bool(buffer[x * pages + y // 8] & (1 << (y % 8)))

    @classmethod
    def glyph_pixel(cls, x: int, y: int) -> bool:
        return 0 <= x < 5 and 0 <= y < 7 and bool(cls.GLYPH_A[x] & (1 << y))

    @classmethod
    def expected_foreground(
        cls, style: int, x: int, y: int
    ) -> bool:
        if style in (cls.LARGE_BLACK, cls.LARGE_WHITE):
            return cls.glyph_pixel(x // 2, y // 2)
        if style in (cls.BOLD_BLACK, cls.BOLD_WHITE):
            return cls.glyph_pixel(x, y) or cls.glyph_pixel(x - 1, y)
        return cls.glyph_pixel(x, y)

    def test_six_styles_render_expected_framebuffer_and_local_background(self) -> None:
        width = 20
        height = 24
        pages = (height + 7) // 8
        text_x = 3
        text_y = 3
        styles = (
            (self.REGULAR_BLACK, 5, 7, 0, True),
            (self.BOLD_BLACK, 6, 7, 0, True),
            (self.LARGE_BLACK, 10, 14, 0, True),
            (self.REGULAR_WHITE, 5, 7, 1, False),
            (self.BOLD_WHITE, 6, 7, 1, False),
            (self.LARGE_WHITE, 10, 14, 2, False),
        )

        for style, text_width, text_height, padding, foreground_on in styles:
            with self.subTest(style=style):
                initial = 0xFF if foreground_on else 0x00
                framebuffer = self.ffi.new(
                    "uint8_t[]", [initial] * (width * pages)
                )
                self.assertTrue(
                    self.renderer.board_lcd_text_draw_style(
                        framebuffer, width, height, text_x, text_y, b"A", style
                    )
                )
                background_x = text_x - padding
                background_y = text_y - padding
                background_width = text_width + 2 * padding
                background_height = text_height + 2 * padding
                for y in range(background_y, background_y + background_height):
                    for x in range(background_x, background_x + background_width):
                        inside_text = (
                            text_x <= x < text_x + text_width
                            and text_y <= y < text_y + text_height
                        )
                        if inside_text:
                            glyph_on = self.expected_foreground(
                                style, x - text_x, y - text_y
                            )
                            expected = glyph_on if foreground_on else not glyph_on
                        else:
                            expected = not foreground_on
                        self.assertEqual(
                            self.pixel(framebuffer, height, x, y),
                            expected,
                        )
                self.assertEqual(
                    self.pixel(framebuffer, height, 0, 0), bool(initial)
                )
                self.assertEqual(
                    self.pixel(
                        framebuffer,
                        height,
                        background_x + background_width,
                        text_y,
                    ),
                    bool(initial),
                )

    def test_styled_text_clips_at_right_and_bottom_without_overwrite(self) -> None:
        width = 8
        height = 8
        framebuffer_size = width
        storage = self.ffi.new("uint8_t[]", framebuffer_size + 2)
        storage[0] = 0xA5
        storage[framebuffer_size + 1] = 0x5A
        framebuffer = storage + 1

        self.assertTrue(
            self.renderer.board_lcd_text_draw_style(
                framebuffer,
                width,
                height,
                width - 1,
                height - 1,
                b"A",
                self.LARGE_WHITE,
            )
        )
        self.assertTrue(self.pixel(framebuffer, height, width - 1, height - 1))
        self.assertTrue(self.pixel(framebuffer, height, width - 2, height - 2))
        self.assertEqual(storage[0], 0xA5)
        self.assertEqual(storage[framebuffer_size + 1], 0x5A)

    def test_legacy_scale_two_keeps_original_transparent_rendering(self) -> None:
        width = 12
        height = 16
        framebuffer = self.ffi.new("uint8_t[]", width * (height // 8))

        self.assertTrue(
            self.renderer.board_lcd_text_draw_legacy(
                framebuffer, width, height, 0, 0, b"A", 2
            )
        )
        for y in range(14):
            for x in range(10):
                self.assertEqual(
                    self.pixel(framebuffer, height, x, y),
                    self.glyph_pixel(x // 2, y // 2),
                )
        self.assertFalse(self.pixel(framebuffer, height, 10, 0))

    def test_renderer_rejects_invalid_parameters(self) -> None:
        framebuffer = self.ffi.new("uint8_t[]", 16)
        self.assertFalse(
            self.renderer.board_lcd_text_draw_legacy(
                framebuffer, 8, 8, 0, 0, b"A", 0
            )
        )
        self.assertFalse(
            self.renderer.board_lcd_text_draw_legacy(
                framebuffer, 8, 8, 0, 0, b"A", 5
            )
        )
        self.assertFalse(
            self.renderer.board_lcd_text_draw_style(
                framebuffer, 8, 8, 0, 0, b"A", 99
            )
        )
        self.assertFalse(
            self.renderer.board_lcd_text_draw_style(
                framebuffer, 8, 8, 0, 0, self.ffi.NULL, self.REGULAR_BLACK
            )
        )


if __name__ == "__main__":
    unittest.main()
