import importlib.util
import builtins
from pathlib import Path
import sys
from types import ModuleType
import unittest
import zlib
from unittest.mock import Mock, patch

from cffi import FFI

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("audio_generator", ROOT / "tools/generate_audio_resources.py")
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)
START_PREFILL_BYTES = 2048
END_FILL_BYTES = 2048


class AudioPlaybackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ffi = FFI()
        ffi.cdef("""
            void board_audio_init(void);
            bool board_audio_play(const char *, uint32_t);
            bool board_audio_tone(uint8_t, int32_t, uint32_t);
            bool board_audio_feedback_tone(uint32_t);
            void board_audio_tick(uint32_t);
            void board_audio_stop(void);
            bool board_audio_ready(void);
            int board_audio_state(void);
            uint32_t board_audio_generation(void);
            uint32_t board_audio_stream_crc32(void);
            uint32_t board_audio_dreq_waits(void);
            bool board_audio_read_register(uint8_t, uint16_t *);
            bool board_audio_set_volume_percent(uint8_t);
            uint8_t board_audio_volume_percent(void);
            extern bool mock_dreq, mock_reset, mock_flash_valid, mock_bus_ok;
            extern uint32_t mock_sent, mock_reads, mock_max_chunk, mock_address;
            extern uint32_t mock_volume_writes;
            extern uint16_t mock_volume, mock_mode, mock_mode_write;
            extern uint16_t mock_hdat0, mock_hdat1;
        """)
        stub = r"""
            #include <stdbool.h>
            #include <stdint.h>
            #include <stddef.h>
            #include <string.h>
            #include "audio_resources.h"
            bool mock_dreq, mock_reset, mock_flash_valid, mock_bus_ok;
            uint32_t mock_sent, mock_reads, mock_max_chunk, mock_address;
            uint32_t mock_volume_writes;
            uint16_t mock_volume, mock_mode, mock_mode_write;
            uint16_t mock_hdat0, mock_hdat1;
            uint32_t system_time_millis(void) { return 0; }
            void board_audio_bus_init(void) {}
            void board_audio_bus_reset(bool asserted) { mock_reset = asserted; }
            bool board_audio_bus_ready(void) { return mock_dreq; }
            bool board_audio_bus_read(uint8_t reg, uint16_t *value) {
                *value = reg == 0 ? mock_mode :
                    (reg == 8 ? mock_hdat0 :
                    (reg == 9 ? mock_hdat1 : 0x0030));
                return mock_bus_ok;
            }
            bool board_audio_bus_write(uint8_t reg, uint16_t value) {
                if (reg == 11) {
                    mock_volume = value;
                    mock_volume_writes++;
                }
                if (reg == 0) mock_mode_write = value;
                return mock_bus_ok;
            }
            bool board_audio_bus_send(const uint8_t *data, size_t length) {
                (void)data;
                if (length > mock_max_chunk) mock_max_chunk = (uint32_t)length;
                mock_sent += (uint32_t)length;
                return mock_bus_ok;
            }
            bool board_flash_read_4byte(uint32_t address, uint8_t *buffer, size_t length) {
                mock_reads++;
                mock_address = address;
                memset(buffer, 0, length);
                if (length == 3) memcpy(buffer, "ID3", 3);
                return mock_flash_valid;
            }
            bool est_audio_resource_find(const char *name,
                    struct audio_resource *resource, char *name_buffer,
                    size_t name_buffer_size) {
                (void)name_buffer_size;
                if (strcmp(name, "External/Cheering") != 0) {
                    return false;
                }
                strcpy(name_buffer, name);
                resource->name = name_buffer;
                resource->data = NULL;
                resource->length = 27616;
                resource->duration_ms = 3000;
                resource->flash_address = 0x01B40080;
                return true;
            }
        """
        cls.native = ffi.verify(stub + GENERATOR.generate() +
                                (ROOT / "src/board_audio.c").read_text(encoding="utf-8"),
                                include_dirs=[str(ROOT / "include")])

    def setUp(self):
        n = self.native
        n.mock_dreq = n.mock_bus_ok = n.mock_flash_valid = True
        n.mock_sent = n.mock_reads = n.mock_max_chunk = 0
        n.mock_volume_writes = 0
        n.mock_mode_write = 0
        n.mock_mode = 0x0800
        n.mock_hdat0 = n.mock_hdat1 = 0
        n.board_audio_init()

    def tick(self, start=0, end=2200):
        for now in range(start, end):
            self.native.board_audio_tick(now)

    @staticmethod
    def legacy_piano_stream_length(length):
        limit = (length // 3) * 2
        return ((limit + 31) // 32) * 32

    def test_index_has_37_flash_resources_without_embedded_audio(self):
        generated = GENERATOR.generate()
        self.assertEqual(generated.count('"Piano/'), 37)
        self.assertIn('"System/FastClick"', generated)
        self.assertEqual(len(GENERATOR.feedback_tone_wav()), 2476)
        self.assertIn('0x01F7F000U', generated)
        self.assertNotIn('hello_mp3', generated)
        self.assertEqual(sum(GENERATOR.PIANO_LENGTHS), 206906)

    def test_idle_init_does_not_play(self):
        self.tick()
        self.assertEqual(self.native.mock_sent, START_PREFILL_BYTES)
        self.assertEqual(self.native.board_audio_state(), 0)

    def test_flash_stream_drains_and_completes(self):
        n = self.native
        self.assertTrue(n.board_audio_play(b"Piano/C4", 0))
        self.assertEqual(n.mock_address, 0x01F40000)
        self.tick(0, 1000)
        self.assertEqual(n.board_audio_state(), 1)
        self.tick(1000, 2200)
        self.assertEqual(n.board_audio_state(), 0)
        self.assertEqual(n.mock_sent,
                         START_PREFILL_BYTES +
                         self.legacy_piano_stream_length(6366) + END_FILL_BYTES)
        self.assertLessEqual(n.mock_max_chunk, 32)
        self.assertGreater(n.mock_reads, 100)
        self.assertFalse(n.mock_reset)
        self.assertEqual(n.board_audio_stream_crc32(),
                         zlib.crc32(bytes(self.legacy_piano_stream_length(6366))))

    def test_piano_stream_uses_legacy_two_thirds_window(self):
        n = self.native
        self.assertTrue(n.board_audio_play(b"Piano/C7", 0))
        self.tick(0, 1200)
        self.assertEqual(n.board_audio_state(), 0)
        self.assertEqual(n.mock_sent,
                         START_PREFILL_BYTES +
                         self.legacy_piano_stream_length(2918) + END_FILL_BYTES)

    def test_second_piano_after_natural_finish_skips_prefill(self):
        n = self.native
        self.assertTrue(n.board_audio_play(b"Piano/C4", 0))
        self.tick(0, 2200)
        self.assertEqual(n.board_audio_state(), 0)
        n.mock_sent = 0
        self.assertTrue(n.board_audio_play(b"Piano/E4", 2201))
        self.tick(2201, 4400)
        self.assertEqual(n.board_audio_state(), 0)
        self.assertEqual(n.mock_sent,
                         self.legacy_piano_stream_length(6366) + END_FILL_BYTES)

    def test_external_flash_resource_reuses_ready_decoder_after_idle_piano(self):
        n = self.native
        self.assertTrue(n.board_audio_play(b"Piano/C4", 0))
        self.tick(0, 2200)
        self.assertEqual(n.board_audio_state(), 0)
        n.mock_reset = False
        n.mock_sent = 0
        self.assertTrue(n.board_audio_play(b"External/Cheering", 2201))
        self.assertFalse(n.mock_reset)
        self.tick(2201, 2450)
        self.assertGreater(n.mock_sent, 0)

    def test_external_flash_resource_finishes_with_decoder_ready(self):
        n = self.native
        self.assertTrue(n.board_audio_play(b"External/Cheering", 0))
        generation = n.board_audio_generation()
        self.tick(0, 3900)
        self.assertEqual(n.board_audio_state(), 0)
        self.assertFalse(n.mock_reset)
        self.assertGreater(n.board_audio_generation(), generation)
        self.assertTrue(n.board_audio_ready())

    def test_boot_mode_is_retried_until_decoder_settles(self):
        n = self.native
        n.mock_mode = 0
        self.assertTrue(n.board_audio_play(b"Piano/C4", 0))
        self.tick(0, 40)
        self.assertEqual(n.board_audio_state(), 1)
        self.assertEqual(n.mock_sent, 0)
        n.mock_mode = 0x0800
        self.tick(40, 120)
        self.assertGreaterEqual(n.mock_sent, START_PREFILL_BYTES)

    def test_idle_piano_reuses_ready_decoder_without_reset(self):
        n = self.native
        self.tick(0, 40)
        self.assertEqual(n.board_audio_state(), 0)
        self.assertTrue(n.board_audio_ready())
        self.assertFalse(n.mock_reset)
        self.assertTrue(n.board_audio_play(b"Piano/C4", 41))
        self.assertFalse(n.mock_reset)
        self.tick(41, 80)
        self.assertEqual(n.board_audio_state(), 1)

    def test_feedback_tone_starts_immediately_without_reset(self):
        n = self.native
        self.tick(0, 40)
        sent = n.mock_sent
        generation = n.board_audio_generation()
        n.mock_reset = False
        self.assertTrue(n.board_audio_feedback_tone(41))
        self.assertEqual(n.board_audio_state(), 1)
        self.assertFalse(n.mock_reset)
        self.assertGreater(n.board_audio_generation(), generation)
        n.mock_hdat0 = 0x7761
        n.mock_hdat1 = 0x7665
        self.tick(42, 300)
        self.assertEqual(
            n.mock_sent,
            sent + len(GENERATOR.feedback_tone_wav()) + END_FILL_BYTES,
        )
        self.assertEqual(n.board_audio_state(), 0)
        self.assertTrue(n.board_audio_ready())
        self.assertFalse(n.mock_reset)

    def test_feedback_tone_mutes_startup_transient_then_fades_in(self):
        n = self.native
        self.tick(0, 40)
        self.assertTrue(n.board_audio_set_volume_percent(100))
        self.assertTrue(n.board_audio_feedback_tone(41))
        n.board_audio_tick(41)
        self.assertEqual(n.mock_volume, 0xFEFE)
        n.board_audio_tick(42)
        self.assertEqual(n.mock_volume, 0xFEFE)
        n.mock_hdat0 = 0x7761
        n.mock_hdat1 = 0x7665
        n.board_audio_tick(43)
        n.board_audio_tick(47)
        self.assertGreater(n.mock_volume, 0x0000)
        self.tick(48, 54)
        self.assertEqual(n.mock_volume, 0x0000)

    def test_feedback_waits_for_wav_decoder_before_sending_tone_samples(self):
        n = self.native
        self.tick(0, 40)
        sent = n.mock_sent
        self.assertTrue(n.board_audio_feedback_tone(41))
        self.tick(41, 60)
        self.assertEqual(n.mock_sent, sent + 64)
        self.assertEqual(n.mock_volume, 0xFEFE)
        n.mock_hdat0 = 0x7761
        n.mock_hdat1 = 0x7665
        self.tick(60, 72)
        self.assertGreater(n.mock_sent, sent + 64)

    def test_user_volume_expands_audible_hardware_range(self):
        n = self.native
        self.tick(0, 40)
        self.assertTrue(n.board_audio_set_volume_percent(10))
        self.assertEqual(n.mock_volume, 0x1212)
        self.assertTrue(n.board_audio_set_volume_percent(50))
        self.assertEqual(n.mock_volume, 0x0A0A)
        self.assertTrue(n.board_audio_set_volume_percent(80))
        volume_80 = n.mock_volume
        self.assertTrue(n.board_audio_set_volume_percent(90))
        volume_90 = n.mock_volume
        self.assertLess(volume_90, volume_80)
        self.assertEqual(volume_80, 0x0404)
        self.assertEqual(volume_90, 0x0202)
        self.assertTrue(n.board_audio_set_volume_percent(100))
        self.assertEqual(n.mock_volume, 0x0000)
        self.assertTrue(n.board_audio_set_volume_percent(0))
        self.assertEqual(n.mock_volume, 0xFEFE)

    def test_feedback_tone_does_not_interrupt_active_audio(self):
        n = self.native
        self.tick(0, 40)
        self.assertTrue(n.board_audio_play(b"Piano/C4", 41))
        generation = n.board_audio_generation()
        self.assertFalse(n.board_audio_feedback_tone(42))
        self.assertEqual(n.board_audio_generation(), generation)
        self.assertEqual(n.board_audio_state(), 1)

    def test_idle_volume_change_is_applied_before_playback(self):
        n = self.native
        self.tick(0, 40)
        writes = n.mock_volume_writes
        self.assertTrue(n.board_audio_set_volume_percent(85))
        self.assertGreater(n.mock_volume_writes, writes)
        self.assertEqual(n.mock_volume, 0x0303)
        writes = n.mock_volume_writes
        self.assertTrue(n.board_audio_play(b"Piano/C4", 41))
        self.tick(41, 80)
        self.assertEqual(n.mock_volume_writes, writes)
        self.assertEqual(n.mock_volume, 0x0303)

    def test_boot_prefill_is_quiet_then_restores_user_volume(self):
        n = self.native
        self.assertTrue(n.board_audio_set_volume_percent(85))
        self.tick(0, 20)
        self.assertEqual(n.mock_volume, 0x6464)
        self.assertGreater(n.mock_sent, 0)
        self.tick(20, 35)
        self.assertEqual(n.mock_volume, 0x0303)
        self.assertEqual(n.mock_sent, START_PREFILL_BYTES)
        self.assertTrue(n.board_audio_play(b"Piano/C4", 36))
        self.tick(36, 45)
        self.assertGreater(n.mock_sent, START_PREFILL_BYTES)

    def test_diagnostic_reads_are_gated_and_checksum_resets(self):
        n = self.native
        ffi = FFI()
        value = ffi.new("uint16_t *")
        self.assertFalse(n.board_audio_read_register(3, value))
        n.board_audio_play(b"Piano/C4", 0)
        self.tick(0, 30)
        self.assertTrue(n.board_audio_read_register(1, value))
        self.assertEqual(value[0], 0x30)
        n.mock_dreq = False
        self.assertFalse(n.board_audio_read_register(1, value))
        n.board_audio_tick(30)
        self.assertEqual(n.board_audio_dreq_waits(), 1)
        n.board_audio_stop()
        self.assertFalse(n.board_audio_read_register(1, value))
        n.mock_dreq = True
        n.board_audio_play(b"Piano/C4", 31)
        self.assertEqual(n.board_audio_stream_crc32(), 0)
        self.assertEqual(n.board_audio_dreq_waits(), 0)

    def test_all_resource_addresses_and_missing_file(self):
        n = self.native
        notes = ('C', 'Cs', 'D', 'Ds', 'E', 'F', 'Fs', 'G', 'Gs', 'A', 'As', 'B')
        for index in range(37):
            name = f"Piano/{notes[index % 12]}{4 + index // 12}".encode()
            self.assertTrue(n.board_audio_play(name, 0))
            self.assertEqual(n.mock_address, 0x01F40000 + index * 0x1C00)
        self.assertFalse(n.board_audio_play(b"communication_hello", 0))
        n.mock_flash_valid = False
        self.assertFalse(n.board_audio_play(b"Piano/C4", 0))

    def test_stop_and_new_note_takeover(self):
        n = self.native
        n.board_audio_play(b"Piano/C4", 0)
        self.tick(0, 100)
        generation = n.board_audio_generation()
        n.board_audio_play(b"Piano/G4", 100)
        self.assertGreater(n.board_audio_generation(), generation)
        self.assertTrue(n.mock_reset)
        self.tick(100, 200)
        self.assertEqual(n.mock_mode_write, 0)
        n.board_audio_stop()
        sent = n.mock_sent
        self.tick(200, 500)
        self.assertTrue(n.mock_reset)
        self.assertEqual(n.board_audio_state(), 0)
        self.assertEqual(n.mock_sent, sent)

    def test_dreq_timeout_before_and_during_play(self):
        n = self.native
        n.board_audio_play(b"Piano/C4", 0)
        n.mock_dreq = False
        self.tick(0, 600)
        self.assertEqual(n.board_audio_state(), 2)
        self.assertTrue(n.mock_reset)
        self.setUp()
        n.board_audio_play(b"Piano/C4", 0)
        self.tick(0, 20)
        n.mock_dreq = False
        self.tick(20, 1300)
        self.assertEqual(n.board_audio_state(), 2)
        self.assertTrue(n.mock_reset)

    def test_volume_and_mute(self):
        n = self.native
        n.board_audio_set_volume_percent(80)
        n.board_audio_play(b"Piano/C4", 0)
        self.tick(0, 40)
        self.assertEqual(n.mock_volume, 0x0404)
        n.board_audio_set_volume_percent(0)
        n.board_audio_tick(150)
        self.assertEqual(n.mock_volume, 0xFEFE)
        self.assertFalse(n.board_audio_set_volume_percent(101))
        self.assertEqual(n.board_audio_volume_percent(), 0)

    def test_user_stop_hard_resets_decoder(self):
        n = self.native
        n.board_audio_set_volume_percent(85)
        self.assertTrue(n.board_audio_play(b"Piano/C4", 0))
        self.tick(0, 150)
        self.assertEqual(n.board_audio_state(), 1)
        self.assertEqual(n.mock_volume, 0x0303)
        n.board_audio_stop()
        self.assertTrue(n.mock_reset)
        self.assertEqual(n.board_audio_state(), 0)
        self.assertTrue(n.board_audio_play(b"Piano/G4", 230))
        self.assertTrue(n.mock_reset)
        self.tick(230, 370)
        self.assertEqual(n.mock_volume, 0x0303)

    def test_new_note_restarts_decoder_from_baseline(self):
        n = self.native
        n.board_audio_set_volume_percent(80)
        self.assertTrue(n.board_audio_play(b"Piano/C4", 0))
        self.tick(0, 150)
        self.assertTrue(n.board_audio_play(b"Piano/E4", 151))
        self.assertTrue(n.mock_reset)
        n.board_audio_tick(151)
        self.assertEqual(n.mock_mode_write, 0)
        self.tick(152, 300)
        self.assertEqual(n.mock_volume, 0x0404)

    def test_timed_and_continuous_tone_stop(self):
        n = self.native
        self.assertTrue(n.board_audio_tone(69, 100, 0))
        self.tick(0, 200)
        self.assertEqual(n.board_audio_state(), 0)
        self.assertTrue(n.board_audio_tone(69, -1, 200))
        self.tick(200, 1500)
        self.assertEqual(n.board_audio_state(), 1)
        n.board_audio_stop()
        self.assertTrue(n.mock_reset)
        self.assertFalse(n.board_audio_tone(128, 0, 1500))

    def test_active_tone_changes_note_by_restarting_decoder(self):
        n = self.native
        self.assertTrue(n.board_audio_tone(60, -1, 0))
        self.tick(0, 180)
        generation = n.board_audio_generation()
        self.assertEqual(n.board_audio_state(), 1)
        self.assertEqual(n.mock_mode_write, 0)
        self.assertTrue(n.board_audio_tone(67, -1, 180))
        self.assertGreater(n.board_audio_generation(), generation)
        self.assertTrue(n.mock_reset)
        self.assertEqual(n.mock_mode_write, 0)
        self.tick(180, 220)
        self.assertEqual(n.board_audio_state(), 1)

    def test_timed_tone_duration_restarts_on_note_change(self):
        n = self.native
        self.assertTrue(n.board_audio_tone(60, 100, 0))
        self.tick(0, 80)
        self.assertTrue(n.board_audio_tone(67, 100, 80))
        self.assertEqual(n.mock_mode_write, 0)
        self.assertTrue(n.mock_reset)
        self.tick(80, 160)
        self.assertEqual(n.board_audio_state(), 1)
        self.tick(160, 220)
        self.assertEqual(n.board_audio_state(), 0)

    def test_bad_mode_and_spi_failure(self):
        n = self.native
        n.mock_mode = 0xFFFF
        n.board_audio_play(b"Piano/C4", 0)
        self.tick(0, 20)
        self.assertEqual(n.board_audio_state(), 1)
        self.tick(20, 600)
        self.assertEqual(n.board_audio_state(), 2)
        self.assertTrue(n.mock_reset)
        self.setUp()
        n.board_audio_play(b"Piano/C4", 0)
        self.tick(0, 20)
        n.mock_bus_ok = False
        n.board_audio_tick(20)
        self.assertEqual(n.board_audio_state(), 2)
        self.assertTrue(n.mock_reset)


class MelodyScriptTests(unittest.TestCase):
    def test_melody_runs_without_enumerate_and_always_stops(self):
        source = (ROOT.parents[1] / "tools/est_hid_sender/examples/m123a_internal_piano_melody.py").read_text(encoding="utf-8")
        est = ModuleType("est")
        est.display = Mock()
        est.audio = Mock()
        est.audio._diagnostics.return_value = (1, 0x30, 1000, True, 85, 0x0800, 6, 15, 0)
        est._program_result = Mock()
        runtime = ModuleType("est_runtime")
        runtime.sleep = Mock()
        limited_builtins = dict(vars(builtins))
        limited_builtins.pop("enumerate")
        with patch.dict(sys.modules, {"est": est, "est_runtime": runtime}):
            exec(compile(source, "melody.py", "exec"), {"__builtins__": limited_builtins})
        self.assertEqual(est.audio.play.call_count, 14)
        self.assertEqual(est.audio.stop.call_count, 15)
        est._program_result.assert_called_with(12301)

    def test_real_source_lines_are_enabled(self):
        config = (ROOT / "micropython_port/mpconfigport.h").read_text(encoding="utf-8")
        self.assertIn("#define MICROPY_ENABLE_SOURCE_LINE (1)", config)


if __name__ == "__main__":
    unittest.main()
