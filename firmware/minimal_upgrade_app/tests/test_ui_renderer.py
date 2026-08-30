from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

try:
    from cffi import FFI
except ImportError:  # pragma: no cover
    FFI = None


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(FFI is None, "cffi is required for native UI tests")
class UiRendererTests(unittest.TestCase):
    WIDTH = 180
    HEIGHT = 128

    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            typedef enum {
                EST_UI_MOTOR_OUTPUT_STOP = 0,
                EST_UI_MOTOR_OUTPUT_FORWARD = 1,
                EST_UI_MOTOR_OUTPUT_REVERSE = 2
            } est_ui_motor_output_state_t;
            typedef struct {
                int connection;
                int kind;
                int state;
                int8_t speed_percent;
                int32_t angle_degrees;
            } est_ui_motor_port_view_t;
            typedef struct {
                int connection;
                const char *model;
                const char *mode;
                char value[20];
            } est_ui_sensor_port_view_t;
            typedef struct {
                est_ui_motor_port_view_t motors[4];
                est_ui_sensor_port_view_t sensors[4];
            } est_ui_ports_view_t;
            typedef struct {
                int page;
                int error_return_page;
                int language;
                uint8_t home_item;
                uint8_t home_first_item;
                uint8_t settings_item;
                uint8_t program_item;
                uint8_t program_count;
                uint8_t delete_choice;
                uint8_t port_item;
                uint8_t remote_group;
                uint8_t motor_output_power_index;
                est_ui_motor_output_state_t motor_output_states[4];
                uint8_t backlight_percent;
                uint8_t volume_percent;
                uint16_t error_code;
                _Bool has_recent_program;
                _Bool dirty;
            } est_ui_state_t;
            typedef struct {
                const char *app_version;
                const char *bootloader_version;
                const char *recent_program_name;
                const char *program_names[8];
                uint8_t program_slots[8];
                uint8_t program_count;
                int programs_state;
                uint16_t programs_error_code;
                est_ui_ports_view_t ports;
                uint8_t remote_group;
                uint8_t remote_codes[2];
                uint8_t remote_fault;
                _Bool remote_output_enabled;
                _Bool transfer_active;
                uint8_t transfer_progress;
                uint8_t battery_percent;
                _Bool battery_valid;
                _Bool battery_low;
                _Bool usb_connected;
            } est_ui_view_t;
            void est_ui_state_init(est_ui_state_t *);
            void est_ui_renderer_render(
                const est_ui_state_t *, const est_ui_view_t *);
            const uint8_t *fake_lcd_framebuffer(void);
            uint32_t fake_lcd_refresh_count(void);
            void fake_lcd_reset(void);
            """
        )
        source_paths = [
            ROOT / "src" / "board_lcd_text.c",
            ROOT / "src" / "est_menu_icons_32x32.c",
            ROOT / "src" / "est_ui_font.c",
            ROOT / "src" / "est_ui_font_data.c",
            ROOT / "src" / "est_ui_renderer.c",
            ROOT / "src" / "est_ui_state.c",
            ROOT / "src" / "est_ui_text.c",
        ]
        source_hash = hashlib.sha256(
            b"".join(path.read_bytes() for path in source_paths)
        ).hexdigest()[:16]
        cls.renderer = cls.ffi.verify(
            f"""
            #include <stdbool.h>
            #include <stddef.h>
            #include <stdint.h>
            #include <string.h>
            #include "board_lcd.h"
            #include "est_ui_renderer.h"

            static uint8_t fake_framebuffer[180U * 16U];
            static uint32_t fake_refreshes;

            static void fake_set_pixel(uint16_t x, uint16_t y, bool on) {{
                uint8_t bit;
                size_t index;
                if (x >= 180U || y >= 128U) return;
                index = (size_t)x * 16U + y / 8U;
                bit = (uint8_t)(1U << (y % 8U));
                if (on) fake_framebuffer[index] |= bit;
                else fake_framebuffer[index] &= (uint8_t)~bit;
            }}
            const uint8_t *fake_lcd_framebuffer(void) {{
                return fake_framebuffer;
            }}
            uint32_t fake_lcd_refresh_count(void) {{ return fake_refreshes; }}
            void fake_lcd_reset(void) {{
                memset(fake_framebuffer, 0, sizeof(fake_framebuffer));
                fake_refreshes = 0U;
            }}
            void board_lcd_clear(void) {{
                memset(fake_framebuffer, 0, sizeof(fake_framebuffer));
            }}
            bool board_lcd_set_pixel(uint16_t x, uint16_t y, bool on) {{
                if (x >= 180U || y >= 128U) return false;
                fake_set_pixel(x, y, on);
                return true;
            }}
            bool board_lcd_draw_line(uint16_t x0, uint16_t y0,
                uint16_t x1, uint16_t y1, bool on) {{
                int32_t x = x0;
                int32_t y = y0;
                int32_t dx = x < x1 ? x1 - x : x - x1;
                int32_t dy = y < y1 ? y - y1 : y1 - y;
                int32_t sx = x < x1 ? 1 : -1;
                int32_t sy = y < y1 ? 1 : -1;
                int32_t error = dx + dy;
                if (x0 >= 180U || x1 >= 180U || y0 >= 128U || y1 >= 128U)
                    return false;
                for (;;) {{
                    int32_t doubled;
                    fake_set_pixel((uint16_t)x, (uint16_t)y, on);
                    if (x == x1 && y == y1) break;
                    doubled = 2 * error;
                    if (doubled >= dy) {{ error += dy; x += sx; }}
                    if (doubled <= dx) {{ error += dx; y += sy; }}
                }}
                return true;
            }}
            bool board_lcd_draw_rectangle(uint16_t x, uint16_t y,
                uint16_t width, uint16_t height, bool filled, bool on) {{
                uint16_t dx;
                uint16_t dy;
                if (width == 0U || height == 0U || x + width > 180U ||
                    y + height > 128U) return false;
                for (dy = 0U; dy < height; dy++) {{
                    for (dx = 0U; dx < width; dx++) {{
                        if (filled || dx == 0U || dy == 0U ||
                            dx == width - 1U || dy == height - 1U)
                            fake_set_pixel((uint16_t)(x + dx),
                                (uint16_t)(y + dy), on);
                    }}
                }}
                return true;
            }}
            bool board_lcd_draw_ui_text(uint16_t x, uint16_t y,
                const char *text, est_ui_text_style_t style) {{
                return est_ui_font_draw(fake_framebuffer, 180U, 128U,
                    x, y, text, style);
            }}
            void board_lcd_refresh(void) {{ fake_refreshes++; }}
            #define EST_UI_RENDERER_TEST_SOURCE_HASH "{source_hash}"
            """,
            include_dirs=[str(ROOT / "include"), str(ROOT / "src")],
            sources=[str(path) for path in source_paths],
            extra_compile_args=["/utf-8"],
        )

    def new_model(self):
        state = self.ffi.new("est_ui_state_t *")
        view = self.ffi.new("est_ui_view_t *")
        self.renderer.est_ui_state_init(state)
        keepalive = [
            self.ffi.new("char[]", b"M1.18A"),
            self.ffi.new("char[]", b"03.00B"),
        ]
        view.app_version = keepalive[0]
        view.bootloader_version = keepalive[1]
        view.battery_valid = True
        view.battery_percent = 75
        view.usb_connected = True
        view.programs_state = 1
        return state, view, keepalive

    def pixel(self, x: int, y: int) -> bool:
        framebuffer = self.renderer.fake_lcd_framebuffer()
        return bool(framebuffer[x * 16 + y // 8] & (1 << (y % 8)))

    def count_pixels(self, x: int, y: int, width: int, height: int) -> int:
        return sum(
            self.pixel(px, py)
            for px in range(x, x + width)
            for py in range(y, y + height)
        )

    def test_home_renders_four_icons_one_compact_selection_and_status(self) -> None:
        state, view, _ = self.new_model()
        self.renderer.fake_lcd_reset()
        self.renderer.est_ui_renderer_render(state, view)
        self.assertEqual(self.renderer.fake_lcd_refresh_count(), 1)
        self.assertGreater(self.count_pixels(0, 0, 180, 128), 1200)
        self.assertGreater(self.count_pixels(2, 23, 44, 55), 1400)
        for zone in (46, 90, 134):
            self.assertGreater(self.count_pixels(zone, 23, 44, 55), 100)
            self.assertLess(self.count_pixels(zone, 23, 44, 55), 1400)
        self.assertGreater(self.count_pixels(0, 113, 180, 15), 20)

    def test_english_home_labels_remain_visible_with_letter_spacing(self) -> None:
        state, view, _ = self.new_model()
        state.language = 1
        self.renderer.fake_lcd_reset()
        self.renderer.est_ui_renderer_render(state, view)
        for x, width in ((44, 49), (96, 32), (131, 49)):
            self.assertGreater(self.count_pixels(x, 59, width, 13), 8)

    def test_program_list_renders_real_slot_names_and_selection(self) -> None:
        state, view, _ = self.new_model()
        names = [b"Program 0", b"Program 3", b"Program 5"]
        keepalive = [self.ffi.new("char[]", name) for name in names]
        state.page = 2
        state.program_count = len(names)
        state.program_item = 1
        view.program_count = len(names)
        for index, name in enumerate(keepalive):
            view.program_names[index] = name
            view.program_slots[index] = (0, 3, 5)[index]
        self.renderer.fake_lcd_reset()
        self.renderer.est_ui_renderer_render(state, view)
        self.assertGreater(self.count_pixels(1, 42, 178, 16), 1800)
        self.assertLess(self.count_pixels(1, 22, 178, 16), 800)

    def test_portuguese_menu_program_list_and_recent_name_render(self) -> None:
        state, view, keepalive = self.new_model()
        name = self.ffi.new("char[]", "Ação rápida".encode())
        keepalive.append(name)
        state.language = 2
        state.has_recent_program = True
        view.recent_program_name = name
        self.renderer.fake_lcd_reset()
        self.renderer.est_ui_renderer_render(state, view)
        self.assertGreater(self.count_pixels(0, 78, 180, 34), 40)

        state.page = 2
        state.program_count = 1
        view.program_count = 1
        view.program_names[0] = name
        view.program_slots[0] = 4
        self.renderer.fake_lcd_reset()
        self.renderer.est_ui_renderer_render(state, view)
        self.assertGreater(self.count_pixels(1, 22, 178, 16), 1800)

    def test_ports_and_settings_fit_inside_framebuffer(self) -> None:
        state, view, keepalive = self.new_model()
        state.page = 5
        view.ports.motors[0].connection = 2
        view.ports.motors[0].kind = 1
        view.ports.motors[0].state = 1
        view.ports.motors[0].speed_percent = 50
        view.ports.motors[0].angle_degrees = 321
        view.ports.sensors[0].connection = 2
        model = self.ffi.new("char[]", b"Gyro")
        keepalive.append(model)
        view.ports.sensors[0].model = model
        view.ports.sensors[0].value = b"-35deg"
        self.renderer.fake_lcd_reset()
        self.renderer.est_ui_renderer_render(state, view)
        self.assertGreater(self.count_pixels(0, 0, 180, 128), 250)
        self.assertGreater(self.count_pixels(0, 19, 180, 52), 120)
        self.assertGreater(self.count_pixels(0, 73, 180, 54), 120)
        state.page = 8
        state.language = 1
        self.renderer.est_ui_renderer_render(state, view)
        self.assertEqual(self.renderer.fake_lcd_refresh_count(), 2)
        self.assertGreater(self.count_pixels(0, 0, 180, 128), 1200)

    def test_scrolled_home_remote_motor_and_transfer_overlay_render(self) -> None:
        state, view, _ = self.new_model()
        state.home_item = 5
        state.home_first_item = 2
        self.renderer.fake_lcd_reset()
        self.renderer.est_ui_renderer_render(state, view)
        self.assertGreater(self.count_pixels(134, 23, 44, 55), 1300)

        state.page = 6
        state.remote_group = 1
        view.remote_fault = 2
        self.renderer.est_ui_renderer_render(state, view)
        self.assertGreater(self.count_pixels(0, 19, 180, 100), 100)

        state.page = 7
        state.motor_output_states[0] = 1
        state.motor_output_power_index = 3
        self.renderer.est_ui_renderer_render(state, view)
        self.assertGreater(self.count_pixels(66, 27, 48, 21), 700)

        view.transfer_active = True
        view.transfer_progress = 50
        self.renderer.est_ui_renderer_render(state, view)
        self.assertGreater(self.count_pixels(20, 39, 140, 52), 300)


if __name__ == "__main__":
    unittest.main()
