from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

try:
    from cffi import FFI
except ImportError:  # pragma: no cover
    FFI = None


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(FFI is None, "cffi is required for settings store tests")
class UiSettingsStoreTests(unittest.TestCase):
    BANK0 = 0x01FCE000
    BANK1 = 0x01FCF000

    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            typedef struct {
                uint8_t backlight_percent;
                uint8_t volume_percent;
                int language;
                uint8_t recent_program_slot;
            } est_ui_settings_data_t;
            void fake_flash_reset(void);
            void fake_flash_set_supported(_Bool);
            void fake_flash_set_byte(uint32_t, uint8_t);
            uint32_t fake_flash_program_count(void);
            uint32_t fake_flash_erase_count(void);
            int est_ui_settings_load(est_ui_settings_data_t *);
            int est_ui_settings_save(const est_ui_settings_data_t *);
            """
        )
        source = ROOT / "src" / "est_ui_settings.c"
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()[:16]
        cls.store = cls.ffi.verify(
            f"""
            #include <stdbool.h>
            #include <stddef.h>
            #include <stdint.h>
            #include <string.h>
            #include "board_flash.h"
            #include "est_ui_settings.h"

            static uint8_t fake_flash[EST_UI_SETTINGS_SECTOR_SIZE * 2U];
            static bool fake_supported;
            static uint32_t program_count;
            static uint32_t erase_count;

            static bool fake_range(uint32_t address, size_t length,
                size_t *offset) {{
                if (address < EST_UI_SETTINGS_BANK0_ADDRESS ||
                    length > sizeof(fake_flash) ||
                    address - EST_UI_SETTINGS_BANK0_ADDRESS >
                        sizeof(fake_flash) - length) {{
                    return false;
                }}
                *offset = address - EST_UI_SETTINGS_BANK0_ADDRESS;
                return true;
            }}

            void fake_flash_reset(void) {{
                memset(fake_flash, 0xFF, sizeof(fake_flash));
                fake_supported = true;
                program_count = 0U;
                erase_count = 0U;
            }}
            void fake_flash_set_supported(bool supported) {{
                fake_supported = supported;
            }}
            void fake_flash_set_byte(uint32_t address, uint8_t value) {{
                size_t offset;
                if (fake_range(address, 1U, &offset)) fake_flash[offset] = value;
            }}
            uint32_t fake_flash_program_count(void) {{ return program_count; }}
            uint32_t fake_flash_erase_count(void) {{ return erase_count; }}

            struct board_flash_identity board_flash_read_identity(void) {{
                struct board_flash_identity identity = {{0xEFU, 0x40U, 0x19U}};
                if (!fake_supported) identity.capacity = 0U;
                return identity;
            }}
            bool board_flash_read_4byte(uint32_t address, uint8_t *buffer,
                size_t length) {{
                size_t offset;
                if (buffer == NULL || !fake_range(address, length, &offset))
                    return false;
                memcpy(buffer, &fake_flash[offset], length);
                return true;
            }}
            bool board_flash_sector_is_erased_4byte(uint32_t address) {{
                size_t offset;
                size_t index;
                if ((address & (EST_UI_SETTINGS_SECTOR_SIZE - 1U)) != 0U ||
                    !fake_range(address, EST_UI_SETTINGS_SECTOR_SIZE, &offset))
                    return false;
                for (index = 0U; index < EST_UI_SETTINGS_SECTOR_SIZE; index++) {{
                    if (fake_flash[offset + index] != 0xFFU) return false;
                }}
                return true;
            }}
            bool board_flash_program_4byte(uint32_t address,
                const uint8_t *data, size_t length) {{
                size_t offset;
                size_t index;
                if (data == NULL || !fake_range(address, length, &offset))
                    return false;
                for (index = 0U; index < length; index++)
                    fake_flash[offset + index] &= data[index];
                program_count++;
                return true;
            }}
            bool board_flash_erase_sector_4byte(uint32_t address) {{
                size_t offset;
                if ((address & (EST_UI_SETTINGS_SECTOR_SIZE - 1U)) != 0U ||
                    !fake_range(address, EST_UI_SETTINGS_SECTOR_SIZE, &offset))
                    return false;
                memset(&fake_flash[offset], 0xFF, EST_UI_SETTINGS_SECTOR_SIZE);
                erase_count++;
                return true;
            }}
            #define EST_UI_SETTINGS_TEST_SOURCE_HASH "{source_hash}"
            """,
            include_dirs=[str(ROOT / "include"), str(ROOT / "src")],
            sources=[str(source)],
        )

    def setUp(self) -> None:
        self.store.fake_flash_reset()

    def settings(self, backlight=100, volume=80, language=1, recent=0xFF):
        value = self.ffi.new("est_ui_settings_data_t *")
        value.backlight_percent = backlight
        value.volume_percent = volume
        value.language = language
        value.recent_program_slot = recent
        return value

    def test_empty_store_uses_caller_defaults(self) -> None:
        loaded = self.settings(70, 40, 0, 3)
        self.assertEqual(self.store.est_ui_settings_load(loaded), -9)
        self.assertEqual(loaded.backlight_percent, 70)
        self.assertEqual(loaded.language, 0)

    def test_save_load_and_same_value_do_not_write_again(self) -> None:
        expected = self.settings(90, 60, 0, 5)
        loaded = self.settings()
        self.assertEqual(self.store.est_ui_settings_save(expected), 0)
        self.assertEqual(self.store.fake_flash_program_count(), 2)
        self.assertEqual(self.store.est_ui_settings_load(loaded), 0)
        self.assertEqual(loaded.backlight_percent, 90)
        self.assertEqual(loaded.volume_percent, 60)
        self.assertEqual(loaded.language, 0)
        self.assertEqual(loaded.recent_program_slot, 5)
        self.assertEqual(self.store.est_ui_settings_save(expected), 0)
        self.assertEqual(self.store.fake_flash_program_count(), 2)
        self.assertEqual(self.store.fake_flash_erase_count(), 0)

    def test_portuguese_language_persists_without_format_migration(self) -> None:
        expected = self.settings(60, 30, 2, 4)
        loaded = self.settings()
        self.assertEqual(self.store.est_ui_settings_save(expected), 0)
        self.assertEqual(self.store.est_ui_settings_load(loaded), 0)
        self.assertEqual(loaded.backlight_percent, 60)
        self.assertEqual(loaded.volume_percent, 30)
        self.assertEqual(loaded.language, 2)
        self.assertEqual(loaded.recent_program_slot, 4)

    def test_corrupt_latest_bank_falls_back_then_recovers(self) -> None:
        first = self.settings(100, 80, 1)
        second = self.settings(60, 30, 0)
        third = self.settings(70, 40, 1)
        loaded = self.settings()
        self.assertEqual(self.store.est_ui_settings_save(first), 0)
        self.assertEqual(self.store.est_ui_settings_save(second), 0)
        self.store.fake_flash_set_byte(self.BANK1 + 20, 0xFF)
        self.assertEqual(self.store.est_ui_settings_load(loaded), 0)
        self.assertEqual(loaded.backlight_percent, 100)
        self.assertEqual(loaded.language, 1)
        self.assertEqual(self.store.est_ui_settings_save(third), 0)
        self.assertEqual(self.store.fake_flash_erase_count(), 1)
        self.assertEqual(self.store.est_ui_settings_load(loaded), 0)
        self.assertEqual(loaded.backlight_percent, 70)

    def test_third_distinct_save_erases_only_older_bank(self) -> None:
        for value in (100, 90, 80):
            self.assertEqual(
                self.store.est_ui_settings_save(self.settings(value, 80, 1)), 0
            )
        self.assertEqual(self.store.fake_flash_erase_count(), 1)
        self.assertEqual(self.store.fake_flash_program_count(), 6)

    def test_foreign_data_is_never_erased(self) -> None:
        self.store.fake_flash_set_byte(self.BANK1 + 100, 0x00)
        self.assertEqual(self.store.est_ui_settings_save(self.settings()), -9)
        self.assertEqual(self.store.fake_flash_erase_count(), 0)
        self.assertEqual(self.store.fake_flash_program_count(), 0)

    def test_invalid_values_and_unsupported_flash_are_rejected(self) -> None:
        self.assertEqual(
            self.store.est_ui_settings_save(self.settings(5, 80, 1)), -1
        )
        self.assertEqual(
            self.store.est_ui_settings_save(self.settings(100, 101, 1)), -1
        )
        self.assertEqual(
            self.store.est_ui_settings_save(self.settings(100, 80, 3)), -1
        )
        self.store.fake_flash_set_supported(False)
        self.assertEqual(self.store.est_ui_settings_save(self.settings()), -5)

    def test_menu_loads_settings_and_debounces_persistent_writes(self) -> None:
        state = (ROOT / "src" / "est_ui_state.c").read_text(encoding="utf-8")
        ui = (ROOT / "src" / "est_ui.c").read_text(encoding="utf-8")
        self.assertIn("state->language = EST_UI_LANGUAGE_ENGLISH;", state)
        self.assertIn("est_ui_settings_load(&settings)", ui)
        self.assertIn("EST_UI_SETTINGS_SAVE_DELAY_MS 750U", ui)
        self.assertIn("schedule_settings_save();", ui)
        self.assertIn("est_ui_settings_save(&settings)", ui)
        self.assertIn("est_backlight_set_percent(ui_state.backlight_percent)", ui)
        self.assertIn("board_audio_set_volume_percent(ui_state.volume_percent)", ui)


if __name__ == "__main__":
    unittest.main()
