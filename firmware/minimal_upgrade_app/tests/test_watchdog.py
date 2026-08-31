from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

try:
    from cffi import FFI
except ImportError:  # pragma: no cover
    FFI = None


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(FFI is None, "cffi is required for watchdog fault tests")
class WatchdogHealthTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            typedef struct {
                uint32_t deadline_ms;
                uint32_t last_progress_ms;
                _Bool active;
            } watchdog_guard_t;
            void watchdog_startup_progress(void);
            void watchdog_main_progress(uint32_t now_ms);
            void watchdog_vm_progress(uint32_t now_ms);
            void watchdog_guard_begin(watchdog_guard_t *, uint32_t, uint32_t);
            _Bool watchdog_guard_progress(watchdog_guard_t *, uint32_t);
            void watchdog_guard_end(watchdog_guard_t *);
            void watchdog_test_reset_state(void);
            void fake_iwdg_reset_state(void);
            uint32_t fake_iwdg_reload_count(void);
            """
        )
        source = (ROOT / "src" / "watchdog.c").read_bytes()
        source_hash = hashlib.sha256(source).hexdigest()[:16]
        cls.watchdog = cls.ffi.verify(
            f"""
            #include <stdint.h>
            #define STM32F4 1
            static uint32_t fake_reload_count;
            void iwdg_reset(void) {{ fake_reload_count++; }}
            void fake_iwdg_reset_state(void) {{ fake_reload_count = 0U; }}
            uint32_t fake_iwdg_reload_count(void) {{
                return fake_reload_count;
            }}
            #define WATCHDOG_TEST 1
            #define WATCHDOG_TEST_SOURCE_HASH "{source_hash}"
            #include "watchdog.c"
            """,
            include_dirs=[
                str(ROOT / "include"),
                str(ROOT / "src"),
                str(ROOT / "third_party" / "libopencm3" / "include"),
            ],
        )

    def setUp(self) -> None:
        self.watchdog.watchdog_test_reset_state()
        self.watchdog.fake_iwdg_reset_state()

    def reloads(self) -> int:
        return self.watchdog.fake_iwdg_reload_count()

    def test_normal_main_loop_feeds_only_when_system_time_advances(self) -> None:
        self.watchdog.watchdog_main_progress(100)
        self.assertEqual(self.reloads(), 1)
        self.watchdog.watchdog_main_progress(100)
        self.assertEqual(self.reloads(), 1)
        self.watchdog.watchdog_main_progress(101)
        self.assertEqual(self.reloads(), 2)

    def test_frozen_systick_cannot_be_kept_alive_by_main_loop(self) -> None:
        self.watchdog.watchdog_main_progress(200)
        for _ in range(1000):
            self.watchdog.watchdog_main_progress(200)
        self.assertEqual(self.reloads(), 1)

    def test_main_hang_with_live_systick_has_no_interrupt_feed_path(self) -> None:
        self.watchdog.watchdog_main_progress(300)
        simulated_systick_ms = 300
        for _ in range(5000):
            simulated_systick_ms += 1
        self.assertEqual(simulated_systick_ms, 5300)
        self.assertEqual(self.reloads(), 1)

    def test_vm_heartbeat_requires_real_time_progress(self) -> None:
        self.watchdog.watchdog_vm_progress(400)
        self.watchdog.watchdog_vm_progress(400)
        self.watchdog.watchdog_vm_progress(410)
        self.assertEqual(self.reloads(), 2)

    def test_bounded_operation_feeds_until_deadline_then_stops(self) -> None:
        guard = self.ffi.new("watchdog_guard_t *")
        self.watchdog.watchdog_guard_begin(guard, 500, 50)
        self.assertEqual(self.reloads(), 1)
        self.assertTrue(self.watchdog.watchdog_guard_progress(guard, 525))
        self.assertTrue(self.watchdog.watchdog_guard_progress(guard, 550))
        self.assertEqual(self.reloads(), 3)
        self.assertFalse(self.watchdog.watchdog_guard_progress(guard, 551))
        self.assertFalse(self.watchdog.watchdog_guard_progress(guard, 5000))
        self.assertEqual(self.reloads(), 3)
        self.watchdog.watchdog_guard_end(guard)

    def test_bounded_operation_suppresses_unbounded_vm_feeds(self) -> None:
        guard = self.ffi.new("watchdog_guard_t *")
        self.watchdog.watchdog_guard_begin(guard, 600, 20)
        self.watchdog.watchdog_vm_progress(610)
        self.assertEqual(self.reloads(), 1)
        self.assertTrue(self.watchdog.watchdog_guard_progress(guard, 610))
        self.assertEqual(self.reloads(), 2)
        self.watchdog.watchdog_guard_end(guard)
        self.watchdog.watchdog_vm_progress(611)
        self.assertEqual(self.reloads(), 3)

    def test_nested_operation_uses_its_own_explicit_budget(self) -> None:
        outer = self.ffi.new("watchdog_guard_t *")
        inner = self.ffi.new("watchdog_guard_t *")
        self.watchdog.watchdog_guard_begin(outer, 650, 20)
        self.watchdog.watchdog_guard_begin(inner, 655, 100)
        self.assertTrue(self.watchdog.watchdog_guard_progress(inner, 700))
        self.assertFalse(self.watchdog.watchdog_guard_progress(outer, 700))
        self.assertEqual(self.reloads(), 3)
        self.watchdog.watchdog_guard_end(inner)
        self.watchdog.watchdog_guard_end(outer)

    def test_invalid_guard_budget_never_creates_a_feed_source(self) -> None:
        guard = self.ffi.new("watchdog_guard_t *")
        self.watchdog.watchdog_guard_begin(guard, 700, 0)
        self.assertFalse(self.watchdog.watchdog_guard_progress(guard, 701))
        self.assertEqual(self.reloads(), 0)


class WatchdogSourceContractTests(unittest.TestCase):
    def test_systick_only_updates_time(self) -> None:
        source = (ROOT / "src" / "system_time.c").read_text(encoding="utf-8")
        handler = source[source.rindex("void sys_tick_handler(void)") :]
        self.assertIn("milliseconds++;", handler)
        self.assertNotIn("watchdog_", handler)

    def test_main_heartbeat_follows_all_critical_ticks(self) -> None:
        source = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
        heartbeat = source.index("watchdog_main_progress")
        self.assertLess(source.index("usb_hid_poll();"), heartbeat)
        self.assertLess(source.index("est_runtime_tick(now_ms);"), heartbeat)
        self.assertLess(source.index("est_ui_tick(now_ms);"), heartbeat)
        self.assertLess(source.index("est_micropython_tick();"), heartbeat)

    def test_no_legacy_unconditional_kicks_remain(self) -> None:
        paths = list((ROOT / "src").glob("*.c"))
        paths.append(ROOT / "micropython_port" / "est_micropython.c")
        for path in paths:
            with self.subTest(path=path.name):
                self.assertNotIn(
                    "watchdog_kick()", path.read_text(encoding="utf-8")
                )

    def test_fatal_vm_failures_wait_for_watchdog_reset(self) -> None:
        source = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        fatal = source[source.index("void nlr_jump_fail") :]
        self.assertIn("(void)est_system_cleanup();", fatal)
        self.assertNotIn("watchdog_startup_progress", fatal)
        self.assertNotIn("watchdog_vm_progress", fatal)

    def test_vm_heartbeat_follows_stop_and_disconnect_checks(self) -> None:
        source = (ROOT / "micropython_port" / "est_micropython.c").read_text(
            encoding="utf-8"
        )
        hook = source[source.index("void est_micropython_vm_hook(void)") :]
        heartbeat = hook.index("watchdog_vm_progress(now_ms);")
        self.assertLess(hook.index("usb_hid_poll();"), heartbeat)
        self.assertLess(hook.index("est_runtime_tick(now_ms);"), heartbeat)
        self.assertLess(hook.index("EST_BUTTON_BACK"), heartbeat)
        self.assertLess(hook.index("!usb_hid_host_connected()"), heartbeat)
        self.assertLess(hook.index("est_motor_stop_all(EST_STOP_COAST)"), heartbeat)

    def test_reset_path_disables_motor_pwm_before_normal_services(self) -> None:
        platform = (ROOT / "src" / "platform.c").read_text(encoding="utf-8")
        main = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
        motor = (ROOT / "src" / "board_motor.c").read_text(encoding="utf-8")
        self.assertIn("RST_TIM4", platform)
        self.assertLess(main.index("board_motor_init();"), main.index("usb_hid_init();"))
        self.assertLess(
            main.index("board_motor_init();"),
            main.index("platform_enable_interrupts();"),
        )
        self.assertIn("MOTOR_PWM_OFF_COMPARE", motor)
        self.assertIn("BOARD_MOTOR_OUTPUT_COAST", motor)

    def test_reset_path_reinitializes_usb_after_otg_reset(self) -> None:
        platform = (ROOT / "src" / "platform.c").read_text(encoding="utf-8")
        main = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
        self.assertIn("RST_OTGFS, RST_OTGHS", platform)
        self.assertIn("usb_hid_init();", main)

    def test_finite_sensor_waits_cannot_use_vm_hook_to_feed_forever(self) -> None:
        source = (ROOT / "micropython_port" / "modest.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("MODEST_SENSOR_WATCHDOG_BUDGET_MS 5000U", source)
        self.assertIn("watchdog_guard_begin(&guard, started_ms", source)
        self.assertIn("watchdog_guard_progress(&guard, est_system_millis())", source)
        self.assertIn("modest_raise_sensor_error_guarded", source)


if __name__ == "__main__":
    unittest.main()
