import unittest
from pathlib import Path

from cffi import FFI


ROOT = Path(__file__).resolve().parents[1]
BOARD_PATH = ROOT / "src" / "board_motor.c"


def section(source: str, start: str, end: str) -> str:
    return source.split(start, 1)[1].split(end, 1)[0]


class MotorPairRecoveryTests(unittest.TestCase):
    def test_source_freezes_incremental_pwm_before_confirmed_stall(self) -> None:
        source = BOARD_PATH.read_text(encoding="utf-8")
        stall = section(
            source,
            "static bool pair_speed_update_stall_state",
            "static void pair_speed_set_recovery_limit",
        )
        active_limit = section(
            source,
            "static int32_t pair_active_recovery_pwm_limit",
            "static void apply_closed_loop_speed",
        )

        self.assertIn("pair_stall_samples == 0U", stall)
        self.assertIn("absolute_i32(control->pwm_x100)", stall)
        self.assertIn("pair_stall_samples != 0U", active_limit)
        self.assertIn("pair_stalled", active_limit)
        self.assertIn("pair_recovery_samples != 0U", active_limit)

    def test_source_uses_tick_progress_and_damped_sync_correction(self) -> None:
        source = BOARD_PATH.read_text(encoding="utf-8")
        speed_recovery = section(
            source,
            "static int16_t pair_speed_sample_tick_progress",
            "static int8_t pair_adjust_continuous_speed",
        )
        pd = section(
            source,
            "static int8_t pair_sync_pd_correction",
            "static void pair_sync_limit_recovery_error",
        )
        tick = section(
            source,
            "void board_motor_tick",
            "bool board_motor_test_start",
        )

        self.assertIn("tacho_count[(uint8_t)port]", speed_recovery)
        self.assertIn("pair_tick_progress", speed_recovery)
        self.assertIn("board_motor_pair_sync_correction", pd)
        self.assertLess(
            tick.index("update_pair_speed_correction(now_ms)"),
            tick.index("update_speed_control(now_ms)"),
        )

    def test_source_rebases_large_error_and_only_slows_leader(self) -> None:
        source = BOARD_PATH.read_text(encoding="utf-8")
        recovery = section(
            source,
            "static void pair_sync_limit_recovery_error",
            "static void pair_sync_release_offset",
        )
        adjustment = section(
            source,
            "static int8_t pair_adjust_continuous_speed",
            "static int32_t pair_active_recovery_pwm_limit",
        )

        self.assertIn("MOTOR_PAIR_SYNC_RECOVERY_ERROR_COUNTS", recovery)
        self.assertIn("synchronization_error - limited_error", recovery)
        self.assertIn("board_motor_pair_adjust_speed(target_speed, reduction)", adjustment)
        self.assertNotIn("equivalent_speed", adjustment)

    def test_pair_state_is_cleared_on_stop_and_retarget(self) -> None:
        source = BOARD_PATH.read_text(encoding="utf-8")
        finish = section(
            source,
            "static void finish_pair_speed",
            "static void update_pair_speed_state",
        )
        start = section(
            source,
            "static bool start_pair_speed",
            "bool board_motor_start_pair_speed",
        )

        for state in (
            "pair_stall_samples = 0U",
            "pair_recovery_samples = 0U",
            "pair_recovery_pwm_limit_x100 = 0",
            "pair_stalled = false",
            "synchronization_offset = 0",
            "previous_control_error = 0",
        ):
            self.assertIn(state, finish)
            self.assertIn(state, start)


class PairRegulatorNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef("""
            struct board_motor_pair_regulator {
                int32_t last_count;
                uint32_t last_ms;
                int32_t speed_x100;
                int32_t previous_error_x100;
            };
            void board_motor_pair_regulator_reset(struct board_motor_pair_regulator *,
                int32_t, uint32_t);
            int32_t board_motor_pair_regulator_step(struct board_motor_pair_regulator *,
                int32_t, uint32_t, uint32_t, int8_t, int32_t, int32_t);
            int8_t board_motor_pair_sync_correction(int32_t, int32_t, int8_t);
            int8_t board_motor_pair_adjust_speed(int8_t, int32_t);
        """)
        cls.native = cls.ffi.verify(
            (ROOT / "src" / "board_motor_pair_regulator.c").read_text(encoding="utf-8"),
            include_dirs=[str(ROOT / "include")],
        )

    def regulator(self, now: int = 0):
        control = self.ffi.new("struct board_motor_pair_regulator *")
        self.native.board_motor_pair_regulator_reset(control, 0, now)
        return control

    def test_native_sync_has_no_peer_speed_relay(self) -> None:
        self.assertEqual(self.native.board_motor_pair_adjust_speed(50, 0), 50)
        self.assertEqual(self.native.board_motor_pair_adjust_speed(50, 1), 49)
        self.assertEqual(self.native.board_motor_pair_adjust_speed(50, 50), 0)
        self.assertEqual(self.native.board_motor_pair_adjust_speed(-50, 1), -49)
        self.assertEqual(self.native.board_motor_pair_adjust_speed(-50, 100), 0)
        self.assertEqual(self.native.board_motor_pair_adjust_speed(0, 100), 0)

    def test_native_correction_releases_before_reversing(self) -> None:
        value = 10
        outputs = []
        for _ in range(5):
            value = self.native.board_motor_pair_sync_correction(-50, -50, value)
            outputs.append(value)
        self.assertEqual(outputs, [6, 2, 0, -2, -4])

    def test_native_release_unwinds_without_waiting_for_stalled_flag(self) -> None:
        for counts_per_speed in (12800, 8100):
            for direction in (1, -1):
                with self.subTest(profile=counts_per_speed, direction=direction):
                    control = self.regulator()
                    pwm = 0
                    for now in range(10, 1010, 10):
                        pwm = self.native.board_motor_pair_regulator_step(
                            control, 0, now, counts_per_speed, direction * 50, pwm, 7500)
                    self.assertEqual(pwm, direction * 7500)
                    count = round(direction * 50 * 128834 / counts_per_speed * 0.01)
                    pwm = self.native.board_motor_pair_regulator_step(
                        control, count, 1010, counts_per_speed, direction * 50, pwm, 7500)
                    self.assertLess(abs(pwm), 7000)

    def test_longer_stall_does_not_store_hidden_pwm(self) -> None:
        recoveries = []
        for stall_ms in (1000, 10000):
            control = self.regulator()
            pwm = 0
            for now in range(10, stall_ms + 10, 10):
                pwm = self.native.board_motor_pair_regulator_step(
                    control, 0, now, 12800, 50, pwm, 10000)
            outputs = []
            for sample in range(1, 21):
                pwm = self.native.board_motor_pair_regulator_step(
                    control, sample * 6, stall_ms + sample * 10, 12800, 50, pwm, 10000)
                outputs.append(pwm)
            recoveries.append(outputs)
        self.assertEqual(recoveries[0], recoveries[1])
        self.assertLess(recoveries[0][5], 6000)

    def test_live_20_zero_75_and_low_speed(self) -> None:
        control = self.regulator()
        now = 0
        for speed in (20, 0, 75, 1, 5, 9, -1, -5, -9):
            now += 10
            pwm = self.native.board_motor_pair_regulator_step(
                control, 0, now, 12800, speed, 0, 10000)
            if speed == 0:
                self.assertEqual(pwm, 0)
            else:
                self.assertGreaterEqual(pwm * speed, 0)
            self.assertLessEqual(abs(pwm), 10000)

    def test_tick_wrap_and_reset_clear_controller_state(self) -> None:
        control = self.regulator(0xFFFFFFFA)
        pwm = self.native.board_motor_pair_regulator_step(
            control, -5, 4, 12800, -50, 0, 10000)
        self.assertLess(control.speed_x100, 0)
        self.assertLess(pwm, 0)
        self.native.board_motor_pair_regulator_reset(control, 123, 800)
        self.assertEqual((control.speed_x100, control.previous_error_x100), (0, 0))
        self.assertEqual((control.last_count, control.last_ms), (123, 800))

    def test_running_pair_regulator_accepts_zero_and_resumes_without_reset(self) -> None:
        control = self.regulator()
        pwm = 0
        speed = 0.0
        count = 0.0
        observed = {}
        for now in range(10, 6010, 10):
            target = 20 if now <= 2000 else (0 if now <= 4000 else 75)
            pwm = self.native.board_motor_pair_regulator_step(
                control, round(count), now, 12800, target, pwm, 10000)
            speed += (pwm / 100 - speed) * 0.1
            count += speed * 128834 / 12800 * 0.01
            if now in (2000, 4000, 6000):
                observed[now] = speed
        self.assertAlmostEqual(observed[2000], 20, delta=2)
        self.assertAlmostEqual(observed[4000], 0, delta=1)
        self.assertAlmostEqual(observed[6000], 75, delta=3)

    def simulate_pair(self, base=50, stalled_side=0, direction=1):
        controls = [self.regulator(), self.regulator()]
        counts = [0.0, 0.0]
        speed = [0.0, 0.0]
        pwm = [0, 0]
        correction = 0
        previous_error = 0
        samples = []
        for now in range(10, 10010, 10):
            error = direction * (round(counts[0]) - round(counts[1]))
            correction = self.native.board_motor_pair_sync_correction(
                error, previous_error, correction)
            previous_error = error
            for side in range(2):
                reduction = max(0, correction if side == 0 else -correction)
                target = self.native.board_motor_pair_adjust_speed(direction * base, reduction)
                pwm[side] = self.native.board_motor_pair_regulator_step(
                    controls[side], round(counts[side]), now, 12800, target, pwm[side], 10000)
                # Unequal first-order plants with encoder quantization; not a hardware model.
                equilibrium = pwm[side] / 100 * (1.0 if side == 0 else 0.94)
                if 2000 <= now < 4000 and side == stalled_side:
                    speed[side] = 0
                else:
                    speed[side] += (equilibrium - speed[side]) * 0.01 / 0.10
                counts[side] += speed[side] * 128834 / 12800 * 0.01
            samples.append((now, error, *speed, *pwm))
        return samples

    def test_native_pair_stall_release_converges_for_both_sides_and_directions(self) -> None:
        for base in (20, 50, 80):
            for stalled_side in (0, 1):
                for direction in (1, -1):
                    with self.subTest(base=base, side=stalled_side, direction=direction):
                        samples = self.simulate_pair(base, stalled_side, direction)
                        late = [sample for sample in samples if sample[0] >= 8000]
                        self.assertLessEqual(max(abs(sample[1]) for sample in late), 6)
                        self.assertLess(max(abs(sample[2] - sample[3]) for sample in late), 4)
                        self.assertAlmostEqual(late[-1][2], direction * base, delta=3)
                        self.assertAlmostEqual(late[-1][3], direction * base, delta=3)
                        self.assertLess(max(abs(sample[2]) for sample in samples), base + 15)
                        self.assertLess(max(abs(sample[3]) for sample in samples), base + 15)


if __name__ == "__main__":
    unittest.main()
