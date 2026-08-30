from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

try:
    from cffi import FFI
except ImportError:  # pragma: no cover
    FFI = None


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(FFI is None, "cffi is required for battery service tests")
class BatteryPercentTests(unittest.TestCase):
    CURVE = (
        (1500, 0),
        (1635, 5),
        (1805, 10),
        (1845, 15),
        (1855, 20),
        (1865, 25),
        (1875, 30),
        (1885, 35),
        (1895, 40),
        (1900, 45),
        (1910, 50),
        (1925, 55),
        (1935, 60),
        (1955, 65),
        (1975, 70),
        (1990, 75),
        (2010, 80),
        (2040, 85),
        (2055, 90),
        (2075, 95),
        (2100, 100),
    )

    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            typedef struct {
                _Bool valid;
                _Bool low;
                uint8_t level;
                uint8_t percent;
                uint16_t adc_raw;
                uint16_t sample_mv;
            } est_battery_status_t;
            void fake_battery_set(_Bool, uint8_t, uint16_t, uint16_t);
            int est_battery_get_status(est_battery_status_t *);
            """
        )
        source = ROOT / "src" / "est_battery.c"
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()[:16]
        cls.battery = cls.ffi.verify(
            f"""
            #include <stdbool.h>
            #include <stdint.h>
            #include "board_battery.h"
            #include "est_battery.h"

            #define _Static_assert(condition, message)
            #include "est_battery.c"

            static struct board_battery_snapshot fake_snapshot;

            void fake_battery_set(bool valid, uint8_t level, uint16_t adc_raw,
                uint16_t sample_mv) {{
                fake_snapshot.valid = valid;
                fake_snapshot.level = level;
                fake_snapshot.adc_raw = adc_raw;
                fake_snapshot.sample_mv = sample_mv;
            }}
            void board_battery_init(uint32_t now_ms) {{ (void)now_ms; }}
            void board_battery_tick(uint32_t now_ms) {{ (void)now_ms; }}
            struct board_battery_snapshot board_battery_snapshot(void) {{
                return fake_snapshot;
            }}
            #define EST_BATTERY_TEST_SOURCE_HASH "{source_hash}"
            """,
            include_dirs=[str(ROOT / "include"), str(ROOT / "src")],
        )

    def status(self, sample_mv: int, *, level: int = 4, valid: bool = True):
        self.battery.fake_battery_set(valid, level, 1234, sample_mv)
        status = self.ffi.new("est_battery_status_t *")
        result = self.battery.est_battery_get_status(status)
        return result, status

    def test_curve_points_map_to_continuous_li_ion_percentages(self) -> None:
        for sample_mv, expected in self.CURVE:
            with self.subTest(sample_mv=sample_mv):
                result, status = self.status(sample_mv)
                self.assertEqual(result, 0)
                self.assertEqual(status.percent, expected)

    def test_interpolation_is_monotonic_and_clamped(self) -> None:
        percentages = [self.status(mv)[1].percent for mv in range(1400, 2201)]
        self.assertEqual(percentages[0], 0)
        self.assertEqual(percentages[-1], 100)
        self.assertTrue(all(a <= b for a, b in zip(percentages, percentages[1:])))
        self.assertGreater(len(set(percentages)), 90)

    def test_percent_uses_voltage_instead_of_legacy_level(self) -> None:
        _, empty = self.status(1500, level=4)
        _, full = self.status(2100, level=0)
        self.assertEqual(empty.percent, 0)
        self.assertEqual(full.percent, 100)

    def test_legacy_low_battery_safety_status_is_unchanged(self) -> None:
        _, low = self.status(1900, level=1)
        _, normal = self.status(1900, level=2)
        self.assertTrue(low.low)
        self.assertFalse(normal.low)

    def test_invalid_sample_reports_state_error_and_zero_percent(self) -> None:
        result, status = self.status(0, valid=False)
        self.assertEqual(result, -9)
        self.assertFalse(status.valid)
        self.assertEqual(status.percent, 0)
