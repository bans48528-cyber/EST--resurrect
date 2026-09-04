from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

from cffi import FFI


ROOT = Path(__file__).resolve().parents[1]


class SystemFeedbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            void fake_audio_reset(void);
            unsigned int fake_audio_play_count(void);
            unsigned int fake_audio_feedback_count(void);
            unsigned int fake_audio_stop_count(void);
            _Bool fake_audio_name_is(const char *);
            void fake_audio_accept(_Bool);
            void est_feedback_init(void);
            void est_feedback_button(uint32_t);
            void est_feedback_usb_connected(uint32_t);
            void est_feedback_transfer_complete(uint32_t);
            void est_feedback_program_start(_Bool, uint32_t);
            """
        )
        source = ROOT / "src" / "est_feedback.c"
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()[:16]
        stub = r"""
            #include <stdbool.h>
            #include <stdint.h>
            #include <string.h>
            static char fake_name[48];
            static unsigned int fake_count;
            static unsigned int fake_feedback_count;
            static unsigned int fake_stops;
            static bool fake_accept = true;
            void fake_audio_reset(void) {
                fake_name[0] = '\0';
                fake_count = 0U;
                fake_feedback_count = 0U;
                fake_stops = 0U;
                fake_accept = true;
            }
            unsigned int fake_audio_play_count(void) { return fake_count; }
            unsigned int fake_audio_feedback_count(void) {
                return fake_feedback_count;
            }
            unsigned int fake_audio_stop_count(void) { return fake_stops; }
            bool fake_audio_name_is(const char *name) {
                return strcmp(fake_name, name) == 0;
            }
            void fake_audio_accept(bool accept) { fake_accept = accept; }
            bool board_audio_play(const char *name, uint32_t now_ms) {
                (void)now_ms;
                fake_count++;
                strcpy(fake_name, name);
                return fake_accept;
            }
            bool board_audio_feedback_tone(uint32_t now_ms) {
                (void)now_ms;
                fake_feedback_count++;
                return fake_accept;
            }
            void board_audio_stop(void) { fake_stops++; }
        """
        cls.native = cls.ffi.verify(
            f'#define EST_FEEDBACK_TEST_SOURCE_HASH "{source_hash}"\n'
            + stub
            + source.read_text(encoding="utf-8"),
            include_dirs=[str(ROOT / "include")],
        )

    def setUp(self) -> None:
        self.native.fake_audio_reset()
        self.native.est_feedback_init()

    def assert_last(self, name: str, count: int = 1) -> None:
        self.assertEqual(self.native.fake_audio_play_count(), count)
        self.assertTrue(self.native.fake_audio_name_is(name.encode()))

    def test_button_and_usb_use_system_resources(self) -> None:
        self.native.est_feedback_button(10)
        self.assertEqual(self.native.fake_audio_feedback_count(), 1)
        self.assertEqual(self.native.fake_audio_play_count(), 0)
        self.native.est_feedback_usb_connected(20)
        self.assert_last("System/Connect")

    def test_online_start_keeps_recent_download_sound(self) -> None:
        self.native.est_feedback_transfer_complete(100)
        self.assert_last("System/Download")
        self.native.est_feedback_program_start(True, 200)
        self.assert_last("System/Download")
        self.assertEqual(self.native.fake_audio_stop_count(), 0)

    def test_saved_or_later_start_uses_download_resource(self) -> None:
        self.native.est_feedback_transfer_complete(100)
        self.native.est_feedback_program_start(False, 200)
        self.assert_last("System/Download", 2)
        self.assertEqual(self.native.fake_audio_stop_count(), 0)
        self.native.est_feedback_init()
        self.native.fake_audio_reset()
        self.native.est_feedback_transfer_complete(100)
        self.native.est_feedback_program_start(True, 1301)
        self.assert_last("System/Download", 2)

    def test_missing_download_resource_does_not_suppress_start(self) -> None:
        self.native.fake_audio_accept(False)
        self.native.est_feedback_transfer_complete(100)
        self.native.fake_audio_accept(True)
        self.native.est_feedback_program_start(True, 101)
        self.assert_last("System/Download", 2)


if __name__ == "__main__":
    unittest.main()
