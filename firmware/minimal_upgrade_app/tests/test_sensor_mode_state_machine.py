from __future__ import annotations

import unittest
from pathlib import Path

try:
    from cffi import FFI
except ImportError:  # pragma: no cover
    FFI = None


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(FFI is None, "cffi is required for sensor mode tests")
class SensorModeTrackerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            typedef struct {
                uint8_t requested_mode;
                uint8_t active_mode;
                _Bool pending;
                _Bool command_sent;
                uint32_t data_generation;
                uint32_t last_data_ms;
                uint32_t mode_command_count;
            } board_sensor_mode_tracker_t;
            void board_sensor_mode_init(board_sensor_mode_tracker_t *, uint8_t);
            void board_sensor_mode_reset_stream(
                board_sensor_mode_tracker_t *, uint8_t);
            _Bool board_sensor_mode_request(
                board_sensor_mode_tracker_t *, uint8_t);
            _Bool board_sensor_mode_command_needed(
                const board_sensor_mode_tracker_t *);
            void board_sensor_mode_mark_command_sent(
                board_sensor_mode_tracker_t *);
            _Bool board_sensor_mode_accept_data(
                board_sensor_mode_tracker_t *, uint8_t, uint32_t);
            """
        )
        cls.mode = cls.ffi.verify(
            """
            #include "board_sensor_mode.h"
            typedef struct board_sensor_mode_tracker
                board_sensor_mode_tracker_t;
            """,
            include_dirs=[str(ROOT / "include")],
            sources=[str(ROOT / "src" / "board_sensor_mode.c")],
        )

    def tracker(self):
        tracker = self.ffi.new("board_sensor_mode_tracker_t *")
        self.mode.board_sensor_mode_init(tracker, 0)
        return tracker

    def test_pending_request_is_sent_once_and_requires_new_target_frame(self) -> None:
        tracker = self.tracker()
        self.assertTrue(tracker.pending)
        self.assertFalse(self.mode.board_sensor_mode_command_needed(tracker))

        self.assertTrue(self.mode.board_sensor_mode_request(tracker, 1))
        self.assertTrue(self.mode.board_sensor_mode_command_needed(tracker))
        self.mode.board_sensor_mode_mark_command_sent(tracker)
        self.mode.board_sensor_mode_mark_command_sent(tracker)
        self.assertEqual(tracker.mode_command_count, 1)
        self.assertFalse(self.mode.board_sensor_mode_request(tracker, 1))
        self.assertEqual(tracker.mode_command_count, 1)

        self.assertFalse(self.mode.board_sensor_mode_accept_data(tracker, 0, 10))
        self.assertTrue(tracker.pending)
        self.assertEqual(tracker.data_generation, 1)
        self.assertTrue(self.mode.board_sensor_mode_accept_data(tracker, 1, 20))
        self.assertFalse(tracker.pending)
        self.assertEqual(tracker.active_mode, 1)
        self.assertEqual(tracker.data_generation, 2)
        self.assertEqual(tracker.last_data_ms, 20)

    def test_color_ambient_reflection_switches_reject_stale_frames(self) -> None:
        tracker = self.tracker()
        self.assertTrue(self.mode.board_sensor_mode_accept_data(tracker, 0, 10))

        self.assertTrue(self.mode.board_sensor_mode_request(tracker, 1))
        self.mode.board_sensor_mode_mark_command_sent(tracker)
        self.assertFalse(self.mode.board_sensor_mode_accept_data(tracker, 0, 20))
        self.assertTrue(self.mode.board_sensor_mode_accept_data(tracker, 1, 30))

        self.assertTrue(self.mode.board_sensor_mode_request(tracker, 0))
        self.assertTrue(self.mode.board_sensor_mode_command_needed(tracker))
        self.mode.board_sensor_mode_mark_command_sent(tracker)
        self.assertFalse(self.mode.board_sensor_mode_accept_data(tracker, 1, 40))
        self.assertTrue(self.mode.board_sensor_mode_accept_data(tracker, 0, 50))
        self.assertEqual(tracker.mode_command_count, 2)
        self.assertEqual(tracker.data_generation, 5)

    def test_same_ready_mode_does_not_clear_data_or_send_command(self) -> None:
        tracker = self.tracker()
        self.assertTrue(self.mode.board_sensor_mode_accept_data(tracker, 0, 25))
        generation = tracker.data_generation
        self.assertFalse(self.mode.board_sensor_mode_request(tracker, 0))
        self.assertFalse(tracker.pending)
        self.assertEqual(tracker.data_generation, generation)
        self.assertEqual(tracker.last_data_ms, 25)
        self.assertEqual(tracker.mode_command_count, 0)

    def test_twenty_hz_reads_for_one_minute_do_not_resend_mode(self) -> None:
        tracker = self.tracker()
        for sample in range(1200):
            self.assertTrue(
                self.mode.board_sensor_mode_accept_data(tracker, 0, sample * 50)
            )
            self.assertFalse(self.mode.board_sensor_mode_request(tracker, 0))
        self.assertEqual(tracker.data_generation, 1200)
        self.assertEqual(tracker.mode_command_count, 0)
        self.assertFalse(tracker.pending)


@unittest.skipIf(FFI is None, "cffi is required for sensor wait tests")
class SensorWaitContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if FFI is None:
            return
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            int fake_wait_decide(int, int, int, int, _Bool, _Bool, int,
                uint32_t, uint32_t, _Bool);
            int fake_timeout_error(int, int);
            """
        )
        cls.wait = cls.ffi.verify(
            """
            #include <string.h>
            #include "est_sensor_wait.h"

            int fake_wait_decide(int type, int state, int requested,
                int active, bool pending, bool valid, int error,
                uint32_t generation, uint32_t baseline, bool require_new) {
                est_sensor_status_t status;
                memset(&status, 0, sizeof(status));
                status.type = (est_sensor_type_t)type;
                status.state = (est_sensor_state_t)state;
                status.active_mode = (est_sensor_mode_t)active;
                status.mode_pending = pending;
                status.value_valid = valid;
                status.error = (est_result_t)error;
                status.data_generation = generation;
                return est_sensor_wait_decide(&status,
                    EST_SENSOR_TYPE_COLOR,
                    (est_sensor_mode_t)requested, baseline, require_new);
            }

            int fake_timeout_error(int type, int expected) {
                est_sensor_status_t status;
                memset(&status, 0, sizeof(status));
                status.type = (est_sensor_type_t)type;
                return est_sensor_wait_timeout_error(&status,
                    (est_sensor_type_t)expected);
            }
            """,
            include_dirs=[str(ROOT / "include")],
            sources=[str(ROOT / "src" / "est_sensor_wait.c")],
        )

    def test_wait_policy_handles_ready_pending_stale_and_failures(self) -> None:
        color = 0x1D
        ultrasonic = 0x1E
        none = 0
        syncing = 1
        streaming = 2
        stale = 3

        self.assertEqual(
            self.wait.fake_wait_decide(
                color, streaming, 0, 0, False, True, 0, 4, 3, True
            ),
            1,
        )
        self.assertEqual(
            self.wait.fake_wait_decide(
                color, streaming, 0, 0, True, False, 0, 4, 3, True
            ),
            0,
        )
        self.assertEqual(
            self.wait.fake_wait_decide(
                color, stale, 0, 0, False, False, -7, 4, 3, True
            ),
            0,
        )
        self.assertEqual(
            self.wait.fake_wait_decide(
                none, syncing, 0, 0, False, False, -6, 0, 0, False
            ),
            0,
        )
        self.assertEqual(
            self.wait.fake_wait_decide(
                none, 0, 0, 0, False, False, -3, 0, 0, False
            ),
            2,
        )
        self.assertEqual(
            self.wait.fake_wait_decide(
                ultrasonic, streaming, 0, 0, False, True, 0, 4, 3, True
            ),
            3,
        )
        self.assertEqual(self.wait.fake_timeout_error(none, color), -3)
        self.assertEqual(self.wait.fake_timeout_error(ultrasonic, color), -4)
        self.assertEqual(self.wait.fake_timeout_error(color, color), -7)

    def test_waiter_uses_generation_without_periodic_mode_retry(self) -> None:
        modest = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        policy = (ROOT / "src" / "est_sensor_wait.c").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("MODEST_SENSOR_MODE_RETRY_MS", modest)
        self.assertIn("if (!mode_requested)", modest)
        self.assertIn("modest_sensor_wait_for_type", modest)
        self.assertGreaterEqual(modest.count("est_micropython_vm_hook();"), 2)
        self.assertIn("status->data_generation != request_generation", policy)
        self.assertIn("status->active_mode == requested_mode", policy)
        self.assertIn("!status->mode_pending", policy)
        self.assertIn("status->state == EST_SENSOR_STALE", policy)
        self.assertIn("est_micropython_vm_hook();", modest)
        self.assertIn('MODEST_SENSOR_DIAGNOSTICS', modest)
        self.assertIn("MODEST_SENSOR_TRACE_CAPACITY = 8U", modest)
        self.assertIn("modest_sensor_trace_records", modest)
        self.assertNotIn("printf(\"sensor port=", modest)
        for reason in (
            '"mode-requested"',
            '"complete"',
            '"disconnected"',
            '"type-mismatch"',
            '"recovery-timeout"',
        ):
            self.assertIn(reason, modest)
        self.assertIn('"recovery-timeout"', modest)

    def test_board_driver_has_stale_grace_and_mode_diagnostics(self) -> None:
        sensor = (ROOT / "src" / "board_sensor.c").read_text(encoding="utf-8")
        header = (ROOT / "include" / "board_sensor.h").read_text(
            encoding="utf-8"
        )
        stale_start = sensor.index(
            "runtime->snapshot.state = BOARD_SENSOR_STALE;"
        )
        stale_branch_end = sensor.index("} else if", stale_start)
        self.assertNotIn("start_sync", sensor[stale_start:stale_branch_end])
        self.assertIn("SENSOR_STALE_RECOVERY_MS", sensor)
        self.assertIn("runtime->snapshot.state != BOARD_SENSOR_STALE", sensor)
        for field in (
            "requested_mode",
            "active_mode",
            "data_generation",
            "last_data_ms",
            "mode_command_count",
            "mode_pending",
        ):
            self.assertIn(field, header)


if __name__ == "__main__":
    unittest.main()
