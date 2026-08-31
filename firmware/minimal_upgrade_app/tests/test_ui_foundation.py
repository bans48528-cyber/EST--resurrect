from __future__ import annotations

import hashlib
import importlib.util
import re
import unittest
from pathlib import Path

try:
    from cffi import FFI
except ImportError:  # pragma: no cover
    FFI = None


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(FFI is None, "cffi is required for native UI tests")
class UiFontTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            typedef enum {
                EST_UI_TEXT_NORMAL = 0,
                EST_UI_TEXT_INVERSE = 1,
                EST_UI_TEXT_COMPACT = 2,
                EST_UI_TEXT_COMPACT_INVERSE = 3
            } est_ui_text_style_t;
            _Bool est_ui_font_draw(
                uint8_t *, uint16_t, uint16_t, uint16_t, uint16_t,
                const char *, est_ui_text_style_t);
            uint16_t est_ui_font_measure(
                const char *, est_ui_text_style_t);
            """
        )
        sources = (
            ROOT / "src" / "est_ui_font.c",
            ROOT / "src" / "est_ui_font_data.c",
            ROOT / "src" / "board_lcd_text.c",
        )
        source_hash = hashlib.sha256(
            b"".join(path.read_bytes() for path in sources)
        ).hexdigest()[:16]
        cls.renderer = cls.ffi.verify(
            f'#define EST_UI_TEST_SOURCE_HASH "{source_hash}"\n'
            '#include "est_ui_font.h"',
            include_dirs=[str(ROOT / "include"), str(ROOT / "src")],
            sources=[str(path) for path in sources],
        )

    @staticmethod
    def pixel(buffer, height: int, x: int, y: int) -> bool:
        pages = (height + 7) // 8
        return bool(buffer[x * pages + y // 8] & (1 << (y % 8)))

    def test_chinese_and_compact_english_measure_and_render(self) -> None:
        self.assertEqual(
            self.renderer.est_ui_font_measure("中文".encode(), 0), 32
        )
        self.assertEqual(
            self.renderer.est_ui_font_measure(b"Settings", 2), 48
        )
        framebuffer = self.ffi.new("uint8_t[]", 64 * 2)
        self.assertTrue(
            self.renderer.est_ui_font_draw(
                framebuffer, 64, 16, 0, 0, "中文".encode(), 0
            )
        )
        self.assertGreater(sum(framebuffer), 0)

    def test_english_styles_leave_visible_letter_spacing(self) -> None:
        compact = self.ffi.new("uint8_t[]", 24 * 2)
        normal = self.ffi.new("uint8_t[]", 28 * 2)
        self.assertTrue(
            self.renderer.est_ui_font_draw(compact, 24, 16, 0, 0, b"TEST", 2)
        )
        self.assertTrue(
            self.renderer.est_ui_font_draw(normal, 28, 16, 0, 0, b"TEST", 0)
        )
        for x in (5, 11, 17, 23):
            self.assertFalse(any(self.pixel(compact, 16, x, y) for y in range(16)))
        for x in (5, 6, 12, 13, 19, 20, 26, 27):
            self.assertFalse(any(self.pixel(normal, 16, x, y) for y in range(16)))

    def test_portuguese_accents_use_narrow_cells_and_render_marks(self) -> None:
        precomposed = "ÁÀÂÃÇÉÊÍÓÔÕÚ áàâãçéêíóôõú Português".encode()
        decomposed = "ac\u0327a\u0303o".encode()
        self.assertEqual(
            self.renderer.est_ui_font_measure(precomposed, 0),
            len("ÁÀÂÃÇÉÊÍÓÔÕÚ áàâãçéêíóôõú Português") * 7,
        )
        self.assertEqual(self.renderer.est_ui_font_measure(decomposed, 0), 4 * 7)
        framebuffer = self.ffi.new("uint8_t[]", 14 * 2)
        self.assertTrue(
            self.renderer.est_ui_font_draw(
                framebuffer, 14, 16, 0, 0, "ãç".encode(), 0
            )
        )
        self.assertTrue(any(self.pixel(framebuffer, 16, x, 0) for x in range(7)))
        self.assertTrue(any(self.pixel(framebuffer, 16, x, 13) for x in range(7, 14)))

    def test_portuguese_common_symbols_are_built_in(self) -> None:
        symbols = "ªº°«»€".encode()
        self.assertEqual(self.renderer.est_ui_font_measure(symbols, 2), 6 * 6)
        framebuffer = self.ffi.new("uint8_t[]", 36 * 2)
        self.assertTrue(
            self.renderer.est_ui_font_draw(
                framebuffer, 36, 16, 0, 0, symbols, 2
            )
        )
        self.assertGreater(sum(framebuffer), 0)

    def test_inverse_background_is_local(self) -> None:
        width = 48
        height = 24
        framebuffer = self.ffi.new("uint8_t[]", width * 3)
        self.assertTrue(
            self.renderer.est_ui_font_draw(
                framebuffer, width, height, 4, 3, "中".encode(), 1
            )
        )
        self.assertFalse(self.pixel(framebuffer, height, 0, 0))
        self.assertFalse(self.pixel(framebuffer, height, 20, 3))
        self.assertTrue(self.pixel(framebuffer, height, 4, 3))
        self.assertTrue(self.pixel(framebuffer, height, 19, 18))
        self.assertTrue(
            any(
                not self.pixel(framebuffer, height, x, y)
                for x in range(4, 20)
                for y in range(3, 19)
            )
        )

    def test_unknown_codepoint_and_clipping_are_safe(self) -> None:
        width = 8
        height = 8
        storage = self.ffi.new("uint8_t[]", width + 2)
        storage[0] = 0xA5
        storage[width + 1] = 0x5A
        framebuffer = storage + 1
        self.assertTrue(
            self.renderer.est_ui_font_draw(
                framebuffer, width, height, 7, 7, "龘".encode(), 0
            )
        )
        self.assertEqual(storage[0], 0xA5)
        self.assertEqual(storage[width + 1], 0x5A)

    def test_generated_subset_covers_every_chinese_catalog_character(self) -> None:
        spec = importlib.util.spec_from_file_location(
            "generate_ui_font", ROOT / "tools" / "generate_ui_font.py"
        )
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        catalog = (ROOT / "src" / "est_ui_text.c").read_text(encoding="utf-8")
        used = set(re.findall(r"[\u3400-\u9fff]", catalog))
        self.assertEqual(used - set(module.unique_glyphs()), set())
        generated = (ROOT / "src" / "est_ui_font_data.c").read_text(
            encoding="ascii"
        )
        generated_codepoints = {
            int(value, 16)
            for value in re.findall(r"\{0x([0-9A-F]{4})U, \{", generated)
        }
        self.assertEqual(
            {ord(character) for character in used} - generated_codepoints,
            set(),
        )


@unittest.skipIf(FFI is None, "cffi is required for native UI tests")
class KeyEventTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            void fake_buttons_set(
                uint8_t pressed_mask, uint8_t pressed_events,
                uint8_t released_events, uint8_t long_events);
            void est_key_events_init(void);
            void est_key_events_tick(void);
            void est_key_events_reset(void);
            uint8_t est_key_events_take_short(void);
            uint8_t est_key_events_take_long(void);
            """
        )
        source = (ROOT / "src" / "est_key_events.c").read_bytes()
        source_hash = hashlib.sha256(source).hexdigest()[:16]
        cls.events = cls.ffi.verify(
            f"""
            #include <stdint.h>
            #define EST_BUTTON_COUNT 6U
            static uint8_t fake_mask;
            static uint8_t fake_pressed;
            static uint8_t fake_released;
            static uint8_t fake_long;
            void fake_buttons_set(uint8_t mask, uint8_t pressed,
                uint8_t released, uint8_t long_events) {{
                fake_mask = mask;
                fake_pressed = pressed;
                fake_released = released;
                fake_long = long_events;
            }}
            uint8_t est_buttons_pressed_mask(void) {{ return fake_mask; }}
            uint8_t est_buttons_take_pressed_events(void) {{
                uint8_t value = fake_pressed; fake_pressed = 0U; return value;
            }}
            uint8_t est_buttons_take_released_events(void) {{
                uint8_t value = fake_released; fake_released = 0U; return value;
            }}
            uint8_t est_buttons_take_long_press_events(void) {{
                uint8_t value = fake_long; fake_long = 0U; return value;
            }}
            #define EST_BUTTONS_H
            #define EST_UI_KEY_TEST_SOURCE_HASH "{source_hash}"
            #include "est_key_events.c"
            """,
            include_dirs=[str(ROOT / "include"), str(ROOT / "src")],
        )

    def setUp(self) -> None:
        self.events.fake_buttons_set(0, 0, 0, 0)
        self.events.est_key_events_init()

    def test_release_emits_one_short_press(self) -> None:
        bit = 1 << 4
        self.events.fake_buttons_set(bit, bit, 0, 0)
        self.events.est_key_events_tick()
        self.events.fake_buttons_set(0, 0, bit, 0)
        self.events.est_key_events_tick()
        self.assertEqual(self.events.est_key_events_take_short(), bit)
        self.assertEqual(self.events.est_key_events_take_short(), 0)

    def test_long_press_suppresses_release_short_press(self) -> None:
        bit = 1
        self.events.fake_buttons_set(bit, bit, 0, 0)
        self.events.est_key_events_tick()
        self.events.fake_buttons_set(bit, 0, 0, bit)
        self.events.est_key_events_tick()
        self.assertEqual(self.events.est_key_events_take_long(), bit)
        self.events.fake_buttons_set(0, 0, bit, 0)
        self.events.est_key_events_tick()
        self.assertEqual(self.events.est_key_events_take_short(), 0)

    def test_reset_suppresses_a_key_already_held(self) -> None:
        bit = 1
        self.events.fake_buttons_set(bit, bit, 0, 0)
        self.events.est_key_events_reset()
        self.events.fake_buttons_set(0, 0, bit, 0)
        self.events.est_key_events_tick()
        self.assertEqual(self.events.est_key_events_take_short(), 0)


@unittest.skipIf(FFI is None, "cffi is required for native UI tests")
class UiStateTests(unittest.TestCase):
    HOME = 0
    POWER_CONFIRM = 1
    PROGRAMS = 2
    DELETE_CONFIRM = 3
    PORTS = 5
    REMOTE = 6
    MOTOR_OUTPUT = 7
    SETTINGS = 8
    DEVICE_INFO = 9

    BACK = 0
    LEFT = 1
    UP = 2
    DOWN = 3
    RIGHT = 4
    CONFIRM = 5

    ACTION_NONE = 0
    ACTION_RUN_RECENT = 1
    ACTION_REFRESH_PROGRAMS = 2
    ACTION_RUN_SELECTED = 3
    ACTION_DELETE_SELECTED = 4
    ACTION_POWER_OFF = 6
    ACTION_EMERGENCY_STOP = 7
    ACTION_APPLY_BACKLIGHT = 8
    ACTION_SAVE_SETTINGS = 10
    ACTION_CYCLE_SENSOR_MODE = 12
    ACTION_ENTER_REMOTE = 13
    ACTION_SWITCH_REMOTE_MOTOR_GROUP = 14
    ACTION_EXIT_REMOTE = 15
    ACTION_ENTER_MOTOR_OUTPUT = 16
    ACTION_UPDATE_MOTOR_OUTPUT = 17
    ACTION_EXIT_MOTOR_OUTPUT = 18

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
                uint8_t remote_motor_group;
                uint8_t motor_output_power_index;
                est_ui_motor_output_state_t motor_output_states[4];
                uint8_t backlight_percent;
                uint8_t volume_percent;
                uint16_t error_code;
                _Bool has_recent_program;
                _Bool dirty;
            } est_ui_state_t;
            void est_ui_state_init(est_ui_state_t *);
            void est_ui_state_set_recent(est_ui_state_t *, _Bool);
            void est_ui_state_set_program_count(est_ui_state_t *, uint8_t);
            int est_ui_state_handle_short(est_ui_state_t *, int);
            int est_ui_state_handle_long(est_ui_state_t *, int);
            _Bool est_ui_state_take_dirty(est_ui_state_t *);
            """
        )
        source = ROOT / "src" / "est_ui_state.c"
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()[:16]
        cls.state_api = cls.ffi.verify(
            f'#define EST_UI_STATE_TEST_SOURCE_HASH "{source_hash}"\n'
            '#include "est_ui_state.h"',
            include_dirs=[str(ROOT / "include"), str(ROOT / "src")],
            sources=[str(source)],
        )

    def new_state(self):
        state = self.ffi.new("est_ui_state_t *")
        self.state_api.est_ui_state_init(state)
        return state

    def test_home_wraps_and_power_dialog_ignores_left_right(self) -> None:
        state = self.new_state()
        self.assertEqual(state.page, self.HOME)
        self.state_api.est_ui_state_handle_short(state, self.LEFT)
        self.assertEqual(state.home_item, 5)
        self.assertEqual(state.home_first_item, 2)
        self.state_api.est_ui_state_handle_short(state, self.BACK)
        self.assertEqual(state.page, self.POWER_CONFIRM)
        self.state_api.est_ui_state_handle_short(state, self.RIGHT)
        self.assertEqual(state.page, self.POWER_CONFIRM)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_POWER_OFF,
        )

    def test_recent_and_program_delete_flow(self) -> None:
        state = self.new_state()
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_NONE,
        )
        self.state_api.est_ui_state_set_recent(state, True)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_RUN_RECENT,
        )
        self.state_api.est_ui_state_handle_short(state, self.RIGHT)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_REFRESH_PROGRAMS,
        )
        self.assertEqual(state.page, self.PROGRAMS)
        self.state_api.est_ui_state_set_program_count(state, 3)
        self.state_api.est_ui_state_handle_short(state, self.RIGHT)
        self.assertEqual(state.page, self.DELETE_CONFIRM)
        self.assertEqual(state.delete_choice, 0)
        self.state_api.est_ui_state_handle_short(state, self.RIGHT)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_DELETE_SELECTED,
        )

    def test_settings_bounds_language_and_device_info(self) -> None:
        state = self.new_state()
        self.assertEqual(state.language, 1)
        state.page = self.SETTINGS
        state.backlight_percent = 100
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.RIGHT),
            self.ACTION_NONE,
        )
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.LEFT),
            self.ACTION_APPLY_BACKLIGHT,
        )
        self.assertEqual(state.backlight_percent, 90)
        self.state_api.est_ui_state_handle_short(state, self.DOWN)
        self.state_api.est_ui_state_handle_short(state, self.DOWN)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.RIGHT),
            self.ACTION_SAVE_SETTINGS,
        )
        self.assertEqual(state.language, 2)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.LEFT),
            self.ACTION_SAVE_SETTINGS,
        )
        self.assertEqual(state.language, 1)
        self.state_api.est_ui_state_handle_short(state, self.LEFT)
        self.assertEqual(state.language, 0)
        self.state_api.est_ui_state_handle_short(state, self.LEFT)
        self.assertEqual(state.language, 2)
        self.state_api.est_ui_state_handle_short(state, self.DOWN)
        self.state_api.est_ui_state_handle_short(state, self.CONFIRM)
        self.assertEqual(state.page, self.DEVICE_INFO)

    def test_long_back_is_single_global_emergency_action(self) -> None:
        state = self.new_state()
        state.page = self.PORTS
        self.assertEqual(
            self.state_api.est_ui_state_handle_long(state, self.BACK),
            self.ACTION_EMERGENCY_STOP,
        )
        self.assertEqual(state.page, self.HOME)
        self.assertEqual(
            self.state_api.est_ui_state_handle_long(state, self.CONFIRM),
            self.ACTION_NONE,
        )

    def test_six_item_home_remote_and_motor_output_flows(self) -> None:
        state = self.new_state()
        for _ in range(3):
            self.state_api.est_ui_state_handle_short(state, self.RIGHT)
        self.assertEqual(state.home_item, 3)
        self.assertEqual(state.home_first_item, 0)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_ENTER_REMOTE,
        )
        self.assertEqual(state.page, self.REMOTE)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_SWITCH_REMOTE_MOTOR_GROUP,
        )
        self.assertEqual(state.remote_motor_group, 1)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.BACK),
            self.ACTION_EXIT_REMOTE,
        )

        state = self.new_state()
        for _ in range(4):
            self.state_api.est_ui_state_handle_short(state, self.RIGHT)
        self.assertEqual(state.home_first_item, 1)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_ENTER_MOTOR_OUTPUT,
        )
        self.assertEqual(state.page, self.MOTOR_OUTPUT)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.UP),
            self.ACTION_UPDATE_MOTOR_OUTPUT,
        )
        self.assertEqual(state.motor_output_states[0], 1)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_UPDATE_MOTOR_OUTPUT,
        )
        self.assertEqual(state.motor_output_power_index, 1)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.BACK),
            self.ACTION_EXIT_MOTOR_OUTPUT,
        )

    def test_unified_eight_port_selector_cycles_sensor_mode(self) -> None:
        state = self.new_state()
        state.page = self.PORTS
        for _ in range(4):
            self.state_api.est_ui_state_handle_short(state, self.RIGHT)
        self.assertEqual(state.port_item, 4)
        self.assertEqual(
            self.state_api.est_ui_state_handle_short(state, self.CONFIRM),
            self.ACTION_CYCLE_SENSOR_MODE,
        )


@unittest.skipIf(FFI is None, "cffi is required for native UI tests")
class UiProgramsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            typedef struct {
                uint8_t slot_id;
                char name[32];
            } est_ui_program_entry_t;
            void fake_store_reset(void);
            void fake_store_set_saved(uint8_t, const char *);
            void fake_store_set_error(uint8_t, uint8_t);
            void fake_store_change(uint8_t, uint8_t);
            void est_ui_programs_init(uint32_t);
            void est_ui_programs_request_scan(uint32_t);
            _Bool est_ui_programs_tick(uint32_t);
            int est_ui_programs_state(void);
            uint8_t est_ui_programs_count(void);
            const est_ui_program_entry_t *est_ui_programs_entry(uint8_t);
            const est_ui_program_entry_t *est_ui_programs_recent(void);
            uint16_t est_ui_programs_error_code(void);
            """
        )
        source = ROOT / "src" / "est_ui_programs.c"
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()[:16]
        cls.programs = cls.ffi.verify(
            f"""
            #include <string.h>
            #include "est_ui_programs.h"
            static est_program_store_status_t fake_slots[8];
            static uint32_t fake_sequence;
            static uint8_t fake_changed_slot;
            static est_program_store_record_type_t fake_changed_type;

            void fake_store_reset(void) {{
                uint8_t index;
                memset(fake_slots, 0, sizeof(fake_slots));
                for (index = 0U; index < 8U; index++) {{
                    fake_slots[index].state = EST_PROGRAM_STORE_READY;
                    fake_slots[index].program_slot_id = index;
                }}
                fake_sequence = 0U;
                fake_changed_slot = 0U;
                fake_changed_type = EST_PROGRAM_STORE_RECORD_NONE;
            }}
            void fake_store_set_saved(uint8_t slot, const char *name) {{
                fake_slots[slot].state = EST_PROGRAM_STORE_SAVED;
                fake_slots[slot].record_type = EST_PROGRAM_STORE_RECORD_PROGRAM;
                strncpy(fake_slots[slot].name, name, 31U);
                fake_slots[slot].name[31] = '\\0';
            }}
            void fake_store_set_error(uint8_t slot, uint8_t error) {{
                fake_slots[slot].state = EST_PROGRAM_STORE_OCCUPIED;
                fake_slots[slot].last_error = (est_program_store_error_t)error;
            }}
            void fake_store_change(uint8_t slot, uint8_t type) {{
                fake_sequence++;
                fake_changed_slot = slot;
                fake_changed_type = (est_program_store_record_type_t)type;
            }}
            bool est_program_store_get_slot_status(uint8_t slot,
                est_program_store_status_t *status) {{
                *status = fake_slots[slot];
                return true;
            }}
            bool est_program_store_last_change(uint32_t *sequence,
                uint8_t *slot, est_program_store_record_type_t *type) {{
                *sequence = fake_sequence;
                *slot = fake_changed_slot;
                *type = fake_changed_type;
                return true;
            }}
            #define EST_UI_PROGRAMS_TEST_SOURCE_HASH "{source_hash}"
            """,
            include_dirs=[str(ROOT / "include"), str(ROOT / "src")],
            sources=[str(source)],
        )

    def setUp(self) -> None:
        self.programs.fake_store_reset()

    def complete_scan(self) -> None:
        for index in range(8):
            self.programs.est_ui_programs_tick(index * 20)

    def test_scan_is_incremental_and_preserves_slot_ids(self) -> None:
        self.programs.fake_store_set_saved(0, b"Alpha")
        self.programs.fake_store_set_saved(5, b"Five")
        self.programs.est_ui_programs_init(0)
        self.assertEqual(self.programs.est_ui_programs_state(), 0)
        self.programs.est_ui_programs_tick(0)
        self.assertEqual(self.programs.est_ui_programs_state(), 0)
        self.complete_scan()
        self.assertEqual(self.programs.est_ui_programs_state(), 1)
        self.assertEqual(self.programs.est_ui_programs_count(), 2)
        first = self.programs.est_ui_programs_entry(0)
        second = self.programs.est_ui_programs_entry(1)
        self.assertEqual(first.slot_id, 0)
        self.assertEqual(self.ffi.string(first.name), b"Alpha")
        self.assertEqual(second.slot_id, 5)

    def test_store_change_rescans_and_marks_saved_slot_recent(self) -> None:
        self.programs.est_ui_programs_init(0)
        self.complete_scan()
        self.programs.fake_store_set_saved(3, b"Downloaded")
        self.programs.fake_store_change(3, 1)
        self.assertTrue(self.programs.est_ui_programs_tick(200))
        self.assertEqual(self.programs.est_ui_programs_state(), 0)
        for index in range(8):
            self.programs.est_ui_programs_tick(200 + index * 20)
        recent = self.programs.est_ui_programs_recent()
        self.assertNotEqual(recent, self.ffi.NULL)
        self.assertEqual(recent.slot_id, 3)

    def test_portuguese_utf8_name_is_preserved_for_list_and_recent(self) -> None:
        name = "Ação rápida".encode()
        self.programs.est_ui_programs_init(0)
        self.complete_scan()
        self.programs.fake_store_set_saved(4, name)
        self.programs.fake_store_change(4, 1)
        self.programs.est_ui_programs_tick(200)
        for index in range(8):
            self.programs.est_ui_programs_tick(220 + index * 20)
        entry = self.programs.est_ui_programs_entry(0)
        recent = self.programs.est_ui_programs_recent()
        self.assertEqual(self.ffi.string(entry.name), name)
        self.assertEqual(self.ffi.string(recent.name), name)

    def test_scan_error_clears_stale_entries_and_exposes_stable_code(self) -> None:
        self.programs.fake_store_set_saved(0, b"Old")
        self.programs.fake_store_set_error(2, 2)
        self.programs.est_ui_programs_init(0)
        self.programs.est_ui_programs_tick(0)
        self.programs.est_ui_programs_tick(20)
        self.programs.est_ui_programs_tick(40)
        self.assertEqual(self.programs.est_ui_programs_state(), 2)
        self.assertEqual(self.programs.est_ui_programs_count(), 0)
        self.assertEqual(self.programs.est_ui_programs_error_code(), 1002)


if __name__ == "__main__":
    unittest.main()
