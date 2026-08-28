from __future__ import annotations

import importlib.util
import sys
import types
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNTIME_PATH = ROOT / "micropython_port" / "modules" / "est_runtime.py"


def make_fake_est() -> types.ModuleType:
    module = types.ModuleType("est")
    module.now_ms = 0
    module.events = []

    def millis():
        module.now_ms += 100
        return module.now_ms

    class Motor:
        STATE_IDLE = 0
        STATE_POSITION = 3
        STATE_TIMED = 4

        def __init__(self, port):
            self.port = port
            self.states = []

        def run_speed(self, speed):
            module.events.append(("motor.run_speed", self.port, speed))

        def run_time(self, duration_ms, **kwargs):
            module.events.append(
                ("motor.run_time", self.port, duration_ms, kwargs)
            )
            self.states = [self.STATE_TIMED, self.STATE_IDLE]

        def run_angle(self, degrees, **kwargs):
            module.events.append(("motor.run_angle", self.port, degrees, kwargs))
            self.states = [self.STATE_POSITION, self.STATE_IDLE]

        def state(self):
            if self.states:
                return self.states.pop(0)
            return self.STATE_IDLE

        def stop(self, stop_mode=0):
            module.events.append(("motor.stop", self.port, stop_mode))

    class DriveBase:
        def __init__(self, left_port, right_port):
            self.left_port = left_port
            self.right_port = right_port
            module.events.append(("drive.new", left_port, right_port))

        def straight_angle(self, degrees, **kwargs):
            module.events.append(("drive.straight_angle", degrees, kwargs))

        def straight_time(self, duration_ms, **kwargs):
            module.events.append(("drive.straight_time", duration_ms, kwargs))

        def steer_angle(self, steering, degrees, **kwargs):
            module.events.append(
                ("drive.steer_angle", steering, degrees, kwargs)
            )

        def steer_time(self, steering, duration_ms, **kwargs):
            module.events.append(
                ("drive.steer_time", steering, duration_ms, kwargs)
            )

        def steer(self, steering, **kwargs):
            module.events.append(("drive.steer", steering, kwargs))

        def stop(self, stop_mode=0):
            module.events.append(("drive.stop", stop_mode))

    class TouchSensor:
        def __init__(self, port):
            self.port = port

        def pressed(self):
            return True

    class ColorSensor:
        def __init__(self, port):
            self.port = port

        def reflection(self):
            return 42

        def ambient(self):
            return 7

        def color(self):
            return 5

    class GyroSensor:
        def __init__(self, port):
            self.port = port
            self.zeroed = False

        def angle(self):
            return 90

        def speed(self):
            return -3

        def reset_angle(self):
            self.zeroed = True

    class UltrasonicSensor:
        def __init__(self, port):
            self.port = port

        def distance_mm(self):
            return 1234

        def inches_tenths(self):
            return 48

        def presence(self):
            return True

    class InfraredSensor:
        def __init__(self, port):
            self.port = port

        def proximity(self):
            return 31

        def beacon(self):
            return (-4, 22)

        def remote(self):
            return 9

    module.millis = millis
    module.Motor = Motor
    module.DriveBase = DriveBase
    module.TouchSensor = TouchSensor
    module.ColorSensor = ColorSensor
    module.GyroSensor = GyroSensor
    module.UltrasonicSensor = UltrasonicSensor
    module.InfraredSensor = InfraredSensor
    return module


def load_runtime():
    fake_est = make_fake_est()
    sys.modules["est"] = fake_est
    spec = importlib.util.spec_from_file_location("est_runtime_test", RUNTIME_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load est_runtime.py")
    runtime = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runtime)
    return runtime, fake_est


class RuntimeLifecycleTests(unittest.TestCase):
    def test_on_start_run_and_wait_helpers(self) -> None:
        runtime, _ = load_runtime()
        calls = []

        @runtime.on_start
        def first():
            calls.append("first")

        @runtime.on_start
        def second():
            calls.append("second")

        runtime.run()
        self.assertEqual(calls, ["first", "second"])

        attempts = []
        runtime.wait_until(lambda: attempts.append(1) or len(attempts) == 3)
        self.assertEqual(len(attempts), 3)
        runtime.sleep(0.5)

    def test_time_comparison_and_repeat_helpers(self) -> None:
        runtime, fake_est = load_runtime()
        self.assertEqual(runtime.API_VERSION, 1)
        self.assertEqual(runtime.seconds_to_ms(0.5), 500)
        self.assertTrue(runtime.compare(2, "less", 3))
        self.assertTrue(runtime.compare(3, "greater", 2))
        self.assertTrue(runtime.compare(3, "equal", 3))
        self.assertTrue(runtime.boolean(1))
        self.assertEqual(runtime.repeat_count(-2), 0)
        self.assertEqual(runtime.repeat_count(4), 4)
        runtime.reset_timer()
        fake_est.now_ms += 2100
        self.assertAlmostEqual(runtime.timer_seconds(), 2.2)
        with self.assertRaises(NotImplementedError):
            runtime.compare(1, "changed", 2)


class RuntimeHardwareTests(unittest.TestCase):
    def test_motor_wrappers(self) -> None:
        runtime, fake_est = load_runtime()
        runtime.motor_set_speed("A", 60)
        runtime.motor_run_for("A", "clockwise", 2, "rotations")
        runtime.motor_start("A", "counterclockwise")
        runtime.motor_stop("A")

        self.assertIn(
            ("motor.run_angle", "A", 720, {"speed": 60, "stop": 0}),
            fake_est.events,
        )
        self.assertIn(("motor.run_speed", "A", -60), fake_est.events)
        self.assertIn(("motor.stop", "A", 0), fake_est.events)

        runtime.motor_run_for("B", None, 2, "seconds", speed=-70)
        self.assertIn(
            ("motor.run_time", "B", 2000, {"speed": -70, "stop": 0}),
            fake_est.events,
        )
        with self.assertRaisesRegex(NotImplementedError, "hold stop action"):
            runtime.motor_set_stop_action("A", "hold")

    def test_drive_wrappers(self) -> None:
        runtime, fake_est = load_runtime()
        runtime.drive_set_pair("A", "C")
        runtime.drive_set_speed(80)
        runtime.drive_move_for("backward", 2, "rotations")
        runtime.drive_steer_for(25, -2, "seconds", speed=70)
        runtime.drive_start_steer(-30, speed=-40)
        runtime.drive_stop()

        self.assertIn(("drive.new", "A", "C"), fake_est.events)
        self.assertIn(
            (
                "drive.straight_angle",
                -720,
                {"speed": 80, "stop": 0, "wait": True},
            ),
            fake_est.events,
        )
        self.assertIn(
            (
                "drive.steer_time",
                25,
                2000,
                {"speed": -70, "stop": 0, "wait": True},
            ),
            fake_est.events,
        )
        self.assertIn(("drive.steer", -30, {"speed": -40}), fake_est.events)
        self.assertIn(("drive.stop", 0), fake_est.events)

    def test_sensor_wrappers_and_cache(self) -> None:
        runtime, _ = load_runtime()
        self.assertIs(runtime.color("3"), runtime.color("3"))
        self.assertEqual(runtime.color("3").reflection(), 42)
        self.assertTrue(runtime.touch("1").pressed())
        self.assertEqual(runtime.gyro("2").angle(), 90)
        self.assertEqual(runtime.ultrasonic("4").distance("centimeters"), 123.4)
        self.assertEqual(runtime.ultrasonic("4").distance("inches"), 4.8)
        self.assertEqual(runtime.infrared("4").proximity(), 31)
        self.assertEqual(runtime.infrared("4").beacon(), (-4, 22))
        with self.assertRaises(NotImplementedError):
            runtime.infrared("4").beacon_heading(1)


class RuntimeContractTests(unittest.TestCase):
    def test_generator_runtime_names_are_explicit(self) -> None:
        runtime, _ = load_runtime()
        expected = {
            "boolean", "broadcast", "color", "color_calibrate",
            "color_reset_calibration", "compare", "display_image_for",
            "drive_dual_speed_for", "drive_move_for", "drive_set_pair",
            "drive_set_speed", "drive_set_stop_action",
            "drive_start_dual_speed", "drive_start_steer",
            "drive_steer_for", "drive_stop", "gyro", "infrared",
            "ir_beacon_compare", "motor", "motor_run_for",
            "motor_set_speed", "motor_set_stop_action", "motor_start",
            "motor_stop", "on_brick_button", "on_broadcast", "on_color",
            "on_condition", "on_gyro_angle", "on_ir_beacon_button",
            "on_ir_proximity", "on_start", "on_timer_gt", "on_touch",
            "on_ultrasonic", "repeat_count", "reset_timer", "run",
            "seconds_to_ms", "sleep", "stop", "stop_other_stacks",
            "timer_seconds", "touch", "ultrasonic", "wait_brick_button",
            "wait_color", "wait_gyro", "wait_ir_beacon_button",
            "wait_ir_proximity", "wait_touch", "wait_ultrasonic",
            "wait_until", "yield_once",
        }
        self.assertEqual([name for name in expected if not hasattr(runtime, name)], [])

    def test_unsupported_interfaces_raise(self) -> None:
        runtime, _ = load_runtime()
        unsupported = (
            "broadcast", "color_calibrate", "color_reset_calibration",
            "display_image_for", "drive_dual_speed_for",
            "drive_start_dual_speed", "ir_beacon_compare",
            "on_brick_button", "on_broadcast", "on_color", "on_condition",
            "on_gyro_angle", "on_ir_beacon_button", "on_ir_proximity",
            "on_timer_gt", "on_touch", "on_ultrasonic", "stop",
            "stop_other_stacks", "wait_brick_button", "wait_color",
            "wait_gyro", "wait_ir_beacon_button", "wait_ir_proximity",
            "wait_touch", "wait_ultrasonic",
        )
        for name in unsupported:
            with self.subTest(name=name):
                with self.assertRaisesRegex(NotImplementedError, name):
                    getattr(runtime, name)()


if __name__ == "__main__":
    unittest.main()
