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
    module.millis_step = 100
    module.button_mask = 0
    module.touch_pressed = True
    module.color_value = 5
    module.gyro_angle = 90
    module.ultrasonic_distance_mm = 1234
    module.infrared_proximity = 31
    module.infrared_remote = 9
    module.events = []

    class GlobalProgramStop(BaseException):
        pass

    def millis():
        module.now_ms += module.millis_step
        return module.now_ms

    class Buttons:
        NONE = 0
        BACK = 1 << 0
        LEFT = 1 << 1
        UP = 1 << 2
        DOWN = 1 << 3
        RIGHT = 1 << 4
        CONFIRM = 1 << 5
        CENTER = CONFIRM

        @staticmethod
        def value():
            return module.button_mask

    class Motor:
        STATE_IDLE = 0
        STATE_POSITION = 3
        STATE_TIMED = 4
        STATE_FAULT = 6

        def __init__(self, port):
            self.port = port
            self.states = []
            self.timed_until_ms = None
            self.stalled_value = False

        def run_speed(self, speed):
            self.states = []
            self.timed_until_ms = None
            module.events.append(("motor.run_speed", self.port, speed))

        def run_power(self, power):
            self.states = []
            self.timed_until_ms = None
            module.events.append(("motor.run_power", self.port, power))

        def run_time(self, duration_ms, **kwargs):
            module.events.append(
                ("motor.run_time", self.port, duration_ms, kwargs)
            )
            self.timed_until_ms = module.now_ms + duration_ms

        def run_angle(self, degrees, **kwargs):
            module.events.append(("motor.run_angle", self.port, degrees, kwargs))
            self.states = [self.STATE_POSITION, self.STATE_IDLE]

        def state(self):
            if self.timed_until_ms is not None:
                if module.now_ms < self.timed_until_ms:
                    return self.STATE_TIMED
                self.timed_until_ms = None
                return self.STATE_IDLE
            if self.states:
                return self.states.pop(0)
            return self.STATE_IDLE

        def stalled(self):
            return self.stalled_value

        def stop(self, stop_mode=0):
            self.states = []
            self.timed_until_ms = None
            module.events.append(("motor.stop", self.port, stop_mode))

    class DriveBase:
        STATE_IDLE = 0
        STATE_RUNNING = 1
        STATE_COMPLETE = 2
        STATE_FAULT = 3

        def __init__(self, left_port, right_port):
            self.left_port = left_port
            self.right_port = right_port
            self.states = []
            module.events.append(("drive.new", left_port, right_port))

        def straight_angle(self, degrees, **kwargs):
            module.events.append(("drive.straight_angle", degrees, kwargs))
            if not kwargs.get("wait", True):
                self.states = [self.STATE_RUNNING, self.STATE_COMPLETE]

        def straight_time(self, duration_ms, **kwargs):
            module.events.append(("drive.straight_time", duration_ms, kwargs))
            if not kwargs.get("wait", True):
                self.states = [self.STATE_RUNNING, self.STATE_COMPLETE]

        def steer_angle(self, steering, degrees, **kwargs):
            module.events.append(
                ("drive.steer_angle", steering, degrees, kwargs)
            )
            if not kwargs.get("wait", True):
                self.states = [self.STATE_RUNNING, self.STATE_COMPLETE]

        def steer_time(self, steering, duration_ms, **kwargs):
            module.events.append(
                ("drive.steer_time", steering, duration_ms, kwargs)
            )
            if not kwargs.get("wait", True):
                self.states = [self.STATE_RUNNING, self.STATE_COMPLETE]

        def steer(self, steering, **kwargs):
            module.events.append(("drive.steer", steering, kwargs))

        def stop(self, stop_mode=0):
            self.states = []
            module.events.append(("drive.stop", stop_mode))

        def state(self):
            if self.states:
                return self.states.pop(0)
            return self.STATE_IDLE

    class MotorPair:
        STATE_IDLE = 0
        STATE_RUNNING = 1
        STATE_COMPLETE = 2
        STATE_FAULT = 3

        def __init__(self, left_port, right_port):
            self.left_port = left_port
            self.right_port = right_port
            self.states = []
            module.events.append(("pair.new", left_port, right_port))

        def run_speed(self, left_speed, right_speed):
            self.states = [self.STATE_RUNNING]
            module.events.append(("pair.run_speed", left_speed, right_speed))

        def run_time(self, duration_ms, **kwargs):
            module.events.append(("pair.run_time", duration_ms, kwargs))
            if not kwargs.get("wait", True):
                self.states = [self.STATE_RUNNING, self.STATE_COMPLETE]

        def run_angle(self, left_degrees, right_degrees, **kwargs):
            module.events.append(
                ("pair.run_angle", left_degrees, right_degrees, kwargs)
            )
            if not kwargs.get("wait", True):
                self.states = [self.STATE_RUNNING, self.STATE_COMPLETE]

        def state(self):
            if self.states:
                return self.states.pop(0)
            return self.STATE_IDLE

    class TouchSensor:
        def __init__(self, port):
            self.port = port

        def pressed(self):
            return module.touch_pressed

    class SoundSensor:
        def __init__(self, port):
            self.port = port

        def db(self):
            return 35

    class ColorSensor:
        def __init__(self, port):
            self.port = port

        def reflection(self):
            return 42

        def ambient(self):
            return 7

        def color(self):
            return module.color_value

    class GyroSensor:
        def __init__(self, port):
            self.port = port
            self.zeroed = False

        def angle(self):
            return module.gyro_angle

        def speed(self):
            return -3

        def reset_angle(self):
            self.zeroed = True

    class TemperatureSensor:
        def __init__(self, port):
            self.port = port

        def celsius_tenths(self):
            return 215

        def fahrenheit_tenths(self):
            return 707

    class UltrasonicSensor:
        def __init__(self, port):
            self.port = port

        def distance_mm(self):
            return module.ultrasonic_distance_mm

        def inches_tenths(self):
            return 48

        def presence(self):
            return True

    class InfraredSensor:
        def __init__(self, port):
            self.port = port

        def proximity(self):
            return module.infrared_proximity

        def beacon(self):
            return (-4, 22)

        def remote(self):
            return module.infrared_remote

    class Display:
        @staticmethod
        def image(name):
            module.events.append(("display.image", name))

        @staticmethod
        def text(x, y, value, **kwargs):
            module.events.append(("display.text", x, y, value, kwargs))

        @staticmethod
        def text_line(line, value):
            module.events.append(("display.text_line", line, value))

        @staticmethod
        def refresh():
            module.events.append(("display.refresh",))

    module.millis = millis
    module.Motor = Motor
    module.MotorPair = MotorPair
    module.DriveBase = DriveBase
    module.TouchSensor = TouchSensor
    module.SoundSensor = SoundSensor
    module.ColorSensor = ColorSensor
    module.GyroSensor = GyroSensor
    module.TemperatureSensor = TemperatureSensor
    module.UltrasonicSensor = UltrasonicSensor
    module.InfraredSensor = InfraredSensor
    module.display = Display()
    module.buttons = Buttons()

    def stop_user_program():
        module.events.append(("program.stop",))
        raise GlobalProgramStop

    module.GlobalProgramStop = GlobalProgramStop
    module._stop_user_program = stop_user_program
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

    def test_async_start_tasks_are_cooperatively_interleaved(self) -> None:
        runtime, _ = load_runtime()
        calls = []

        @runtime.on_start
        async def first():
            calls.append("first-start")
            await runtime.sleep(0.3)
            calls.append("first-end")

        @runtime.on_start
        async def second():
            calls.append("second-start")
            await runtime.yield_once()
            calls.append("second-end")

        runtime.run()

        self.assertEqual(calls[0:2], ["first-start", "second-start"])
        self.assertIn("first-end", calls)
        self.assertIn("second-end", calls)

    def test_wait_until_yields_to_another_start_task(self) -> None:
        runtime, _ = load_runtime()
        state = {"ready": False}
        calls = []

        @runtime.on_start
        async def waiter():
            calls.append("waiting")
            await runtime.wait_until(lambda: state["ready"])
            calls.append("ready")

        @runtime.on_start
        async def setter():
            await runtime.yield_once()
            state["ready"] = True
            calls.append("set")

        runtime.run()
        self.assertEqual(calls, ["waiting", "set", "ready"])

    def test_at_most_eight_start_tasks_are_accepted(self) -> None:
        runtime, _ = load_runtime()

        for _ in range(9):
            runtime.on_start(lambda: None)

        with self.assertRaisesRegex(RuntimeError, "at most 8"):
            runtime.run()

    def test_stop_this_stack_keeps_peer_running(self) -> None:
        runtime, _ = load_runtime()
        calls = []

        @runtime.on_start
        async def stopped():
            calls.append("stopped-start")
            runtime.stop("this_stack")
            calls.append("unreachable")

        @runtime.on_start
        async def peer():
            await runtime.yield_once()
            calls.append("peer-finished")

        runtime.run()
        self.assertEqual(calls, ["stopped-start", "peer-finished"])

    def test_stop_other_stacks_cancels_peers(self) -> None:
        runtime, _ = load_runtime()
        calls = []

        @runtime.on_start
        async def peer():
            calls.append("peer-start")
            await runtime.yield_once()
            calls.append("peer-unreachable")

        @runtime.on_start
        async def controller():
            runtime.stop_other_stacks()
            calls.append("controller-only")

        runtime.run()
        self.assertEqual(calls, ["peer-start", "controller-only"])

    def test_stop_all_uses_uncatchable_native_stop(self) -> None:
        runtime, fake_est = load_runtime()

        @runtime.on_start
        async def task():
            runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertIn(("program.stop",), fake_est.events)

    def test_old_task_cleanup_does_not_stop_new_motor_owner(self) -> None:
        runtime, fake_est = load_runtime()

        @runtime.on_start
        async def old_owner():
            runtime.motor_set_speed("A", 20)
            runtime.motor_start("A", "clockwise")
            await runtime.yield_once()
            runtime.stop("this_stack")

        @runtime.on_start
        async def new_owner():
            runtime.motor_set_speed("A", 75)
            runtime.motor_start("A", "clockwise")
            await runtime.yield_once()
            runtime.motor_set_speed("A", 80)
            runtime.motor_start("A", "clockwise")

        runtime.run()
        events = fake_est.events
        speed_20 = events.index(("motor.run_speed", "A", 20))
        speed_75 = events.index(("motor.run_speed", "A", 75))
        speed_80 = events.index(("motor.run_speed", "A", 80))
        self.assertNotIn(("motor.stop", "A", 0), events[speed_20 + 1:speed_75])
        self.assertNotIn(("motor.stop", "A", 0), events[speed_75 + 1:speed_80])

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
        runtime.motor_set_stop_action("A", "hold")
        runtime.motor_stop("A")
        self.assertIn(("motor.stop", "A", 2), fake_est.events)

    def test_motor_stalled_reports_native_position_control_state(self) -> None:
        runtime, _ = load_runtime()
        motor = runtime.motor("A")

        self.assertFalse(runtime.motor_stalled("A"))
        motor.stalled_value = True
        self.assertTrue(runtime.motor_stalled("A"))

    def test_motor_start_speed_forwards_20_zero_75_without_interrupting(self) -> None:
        runtime, fake_est = load_runtime()

        runtime.motor_start_speed("A", 20)
        runtime.motor_start_speed("A", 0)
        runtime.motor_start_speed("A", 75)

        self.assertEqual(
            [
                ("motor.run_speed", "A", 20),
                ("motor.run_speed", "A", 0),
                ("motor.run_speed", "A", 75),
            ],
            [event for event in fake_est.events if event[0] == "motor.run_speed"],
        )
    def test_motor_and_drive_speed_accept_zero_and_low_values(self) -> None:
        runtime, fake_est = load_runtime()

        for speed in (0, 1, 5, 9):
            runtime.motor_set_speed("A", speed)
            runtime.motor_start("A", "clockwise")
            runtime.drive_set_speed(speed)

        self.assertEqual(
            [0, 1, 5, 9],
            [event[2] for event in fake_est.events if event[0] == "motor.run_speed"],
        )
        runtime.motor_set_speed("A", 101)
        runtime.motor_start("A", "clockwise")
        runtime.drive_set_speed(-101)
        self.assertEqual(("motor.run_speed", "A", 100), fake_est.events[-1])

    def test_motor_start_speed_and_power_normalize_clamp_and_switch(self) -> None:
        runtime, fake_est = load_runtime()

        runtime.motor_start_speed("a", 101.9)
        runtime.motor_start_speed("A", -101)
        runtime.motor_start_power("a", 101)
        runtime.motor_start_power("A", -101.9)

        self.assertEqual(
            [
                ("motor.run_speed", "A", 100),
                ("motor.run_speed", "A", -100),
                ("motor.stop", "A", 0),
                ("motor.run_power", "A", 100),
                ("motor.run_power", "A", -100),
            ],
            fake_est.events,
        )
        with self.assertRaisesRegex(ValueError, "motor port"):
            runtime.motor_start_speed("E", 20)

    def test_new_motor_owner_survives_old_task_cleanup(self) -> None:
        runtime, fake_est = load_runtime()

        @runtime.on_start
        async def old_owner():
            runtime.motor_start_speed("A", 20)
            await runtime.yield_once()
            await runtime.yield_once()

        @runtime.on_start
        async def new_owner():
            await runtime.yield_once()
            runtime.motor_start_power("A", 75)
            await runtime.yield_once()

        runtime.run()

        power_index = fake_est.events.index(("motor.run_power", "A", 75))
        self.assertEqual(
            [("motor.stop", "A", 0)],
            [
                event for event in fake_est.events[power_index + 1:]
                if event[0] == "motor.stop"
            ],
        )

    def test_all_generated_speed_entries_clamp_dynamic_values(self) -> None:
        runtime, fake_est = load_runtime()

        runtime.motor_run_for("A", None, 1, "seconds", speed=-101)
        runtime.drive_set_pair("B", "C")
        runtime.drive_set_speed(101)
        runtime.drive_move_for("forward", 1, "rotations")
        runtime.drive_steer_for(25, 1, "seconds", speed=101)
        runtime.drive_start_steer(0, speed=-101)

        self.assertIn(
            ("motor.run_time", "A", 1000, {"speed": -100, "stop": 0}),
            fake_est.events,
        )
        self.assertIn(
            (
                "drive.straight_angle",
                360,
                {"speed": 100, "stop": 0, "wait": True},
            ),
            fake_est.events,
        )
        self.assertIn(
            (
                "drive.steer_time",
                25,
                1000,
                {"speed": 100, "stop": 0, "wait": True},
            ),
            fake_est.events,
        )
        self.assertIn(("drive.steer", 0, {"speed": -100}), fake_est.events)

    def test_motor_start_failure_removes_failed_generation(self) -> None:
        runtime, _ = load_runtime()
        device = runtime.motor("A")

        def fail_run_power(_power):
            raise RuntimeError("native start failed")

        device.run_power = fail_run_power
        with self.assertRaisesRegex(RuntimeError, "native start failed"):
            runtime.motor_start_power("A", 50)
        self.assertNotIn("A", runtime._motor_commands)

    def test_timed_zero_speed_waits_full_duration_then_continues(self) -> None:
        runtime, fake_est = load_runtime()
        started_ms = fake_est.now_ms

        runtime.motor_run_for("A", "clockwise", 2, "seconds", speed=0)
        runtime.motor_set_speed("A", 75)
        runtime.motor_start("A", "clockwise")

        self.assertGreaterEqual(fake_est.now_ms - started_ms, 2000)
        self.assertIn(
            ("motor.run_time", "A", 2000, {"speed": 0, "stop": 0}),
            fake_est.events,
        )
        self.assertEqual(("motor.run_speed", "A", 75), fake_est.events[-1])

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

    def test_dual_speed_drive_continuous_clamps_and_retargets(self) -> None:
        runtime, fake_est = load_runtime()
        runtime.drive_set_pair("A", "C")

        runtime.drive_start_dual_speed(101, -101)
        runtime.drive_start_dual_speed(0, 50)
        runtime.drive_start_dual_speed(50, 0)
        runtime.drive_start_dual_speed(0, 0)

        self.assertEqual(
            [
                ("pair.run_speed", 100, -100),
                ("pair.run_speed", 0, 50),
                ("pair.run_speed", 50, 0),
                ("pair.run_speed", 0, 0),
            ],
            [event for event in fake_est.events if event[0] == "pair.run_speed"],
        )
        self.assertNotIn(("motor.stop", "A", 0), fake_est.events)
        self.assertNotIn(("motor.stop", "C", 0), fake_est.events)

    def test_dual_power_drive_clamps_and_retargets_without_speed_control(self) -> None:
        runtime, fake_est = load_runtime()
        runtime.drive_set_pair("B", "C")

        runtime.drive_start_dual_power(101, -101)
        first_generation = runtime._motor_commands["B"][1]
        self.assertEqual(first_generation, runtime._motor_commands["C"][1])
        runtime.drive_start_dual_power(25, 75)

        self.assertEqual(
            [
                ("motor.run_power", "B", 100),
                ("motor.run_power", "C", -100),
                ("motor.run_power", "B", 25),
                ("motor.run_power", "C", 75),
            ],
            [event for event in fake_est.events if event[0] == "motor.run_power"],
        )
        self.assertEqual(
            [event for event in fake_est.events if event[0] == "pair.run_speed"],
            [],
        )
        self.assertEqual(
            [event for event in fake_est.events if event[0] == "motor.stop"],
            [],
        )

    def test_dual_power_failure_coasts_both_ports(self) -> None:
        runtime, fake_est = load_runtime()
        runtime.drive_set_pair("B", "C")
        right_motor = runtime.motor("C")

        def fail_run_power(_power):
            raise RuntimeError("right motor start failed")

        right_motor.run_power = fail_run_power
        with self.assertRaisesRegex(RuntimeError, "right motor start failed"):
            runtime.drive_start_dual_power(40, 60)

        self.assertIn(("motor.run_power", "B", 40), fake_est.events)
        self.assertIn(("motor.stop", "B", 0), fake_est.events)
        self.assertIn(("motor.stop", "C", 0), fake_est.events)
        self.assertNotIn("B", runtime._motor_commands)
        self.assertNotIn("C", runtime._motor_commands)

    def test_pwm_line_follow_init_and_dual_pd_steps(self) -> None:
        runtime, fake_est = load_runtime()
        fake_est.millis_step = 20
        runtime.drive_set_pair("B", "C")

        runtime.line_follow_init()
        runtime.line_follow_dual_power_step(60, 40, 30, 30, 1, 0.01)
        runtime.line_follow_dual_power_step(50, 40, 30, 30, 1, 0.01)

        self.assertEqual(
            [
                ("motor.run_power", "B", 50),
                ("motor.run_power", "C", 10),
                ("motor.run_power", "B", 35),
                ("motor.run_power", "C", 25),
            ],
            [event for event in fake_est.events if event[0] == "motor.run_power"],
        )
        self.assertEqual(
            [event for event in fake_est.events if event[0] == "pair.run_speed"],
            [],
        )

    def test_pwm_line_follow_clamps_and_validates_power(self) -> None:
        runtime, fake_est = load_runtime()
        runtime.line_follow_init()
        runtime.line_follow_dual_power_step(100, -100, 90, -90, 2, 0)
        self.assertIn(("motor.run_power", "B", 100), fake_est.events)
        self.assertIn(("motor.run_power", "C", -100), fake_est.events)

        with self.assertRaisesRegex(ValueError, "left input must be a number"):
            runtime.line_follow_dual_power_step("bad", 0, 30, 30, 1, 0)

    def test_pwm_line_follow_new_owner_survives_old_task_cleanup(self) -> None:
        runtime, fake_est = load_runtime()

        @runtime.on_start
        async def old_owner():
            runtime.line_follow_init()
            runtime.line_follow_dual_power_step(60, 40, 30, 30, 1, 0)
            await runtime.yield_once()
            await runtime.yield_once()

        @runtime.on_start
        async def new_owner():
            await runtime.yield_once()
            runtime.line_follow_init()
            runtime.line_follow_dual_power_step(40, 60, 50, 50, 1, 0)
            await runtime.yield_once()

        runtime.run()

        takeover = fake_est.events.index(("motor.run_power", "B", 30))
        self.assertEqual(
            [
                ("motor.run_power", "B", 30),
                ("motor.run_power", "C", 70),
                ("motor.stop", "B", 0),
                ("motor.stop", "C", 0),
            ],
            fake_est.events[takeover:takeover + 4],
        )

    def test_dual_speed_drive_for_time_and_proportional_degrees(self) -> None:
        runtime, fake_est = load_runtime()
        runtime.drive_set_pair("B", "C")
        runtime.drive_set_stop_action("hold")

        runtime.drive_dual_speed_for(101, -50, 2, "rotations")
        runtime.drive_dual_speed_for(0, 50, 90, "degrees")
        runtime.drive_dual_speed_for(50, 0, 1.5, "seconds")
        runtime.drive_dual_speed_for(0, 0, 0.5, "seconds")

        self.assertIn(
            (
                "pair.run_angle", 720, -360,
                {"speed": 100, "stop": 2, "wait": True},
            ),
            fake_est.events,
        )
        self.assertIn(
            (
                "pair.run_angle", 0, 90,
                {"speed": 50, "stop": 2, "wait": True},
            ),
            fake_est.events,
        )
        self.assertIn(
            (
                "pair.run_time", 1500,
                {"left_speed": 50, "right_speed": 0, "stop": 2, "wait": True},
            ),
            fake_est.events,
        )
        self.assertIn(
            (
                "pair.run_time", 500,
                {"left_speed": 0, "right_speed": 0, "stop": 2, "wait": True},
            ),
            fake_est.events,
        )

    def test_new_dual_speed_owner_survives_old_task_cleanup(self) -> None:
        runtime, fake_est = load_runtime()

        @runtime.on_start
        async def old_owner():
            runtime.drive_start_dual_speed(20, 40)
            await runtime.yield_once()
            await runtime.yield_once()

        @runtime.on_start
        async def new_owner():
            await runtime.yield_once()
            runtime.drive_start_dual_speed(75, 50)
            await runtime.yield_once()

        runtime.run()

        takeover = fake_est.events.index(("pair.run_speed", 75, 50))
        self.assertEqual(
            [("motor.stop", "B", 0), ("motor.stop", "C", 0)],
            [event for event in fake_est.events[takeover + 1:] if event[0] == "motor.stop"],
        )

    def test_sensor_wrappers_and_cache(self) -> None:
        runtime, _ = load_runtime()
        wrappers = (
            runtime.color,
            runtime.touch,
            runtime.sound,
            runtime.gyro,
            runtime.temperature,
            runtime.ultrasonic,
            runtime.infrared,
        )
        for wrapper in wrappers:
            self.assertIs(wrapper("3"), wrapper(3))
        self.assertEqual(runtime.color("3").port, 3)
        self.assertEqual(runtime.color("3").reflection(), 42)
        self.assertTrue(runtime.touch("1").pressed())
        self.assertEqual(runtime.sound("1").db(), 35)
        self.assertEqual(runtime.gyro("2").angle(), 90)
        self.assertIs(runtime.temperature("2"), runtime.temperature(2))
        self.assertEqual(runtime.temperature("2").celsius(), 21.5)
        self.assertEqual(runtime.temperature("2").fahrenheit(), 70.7)
        self.assertEqual(runtime.ultrasonic("4").distance("centimeters"), 123.4)
        self.assertEqual(runtime.ultrasonic("4").distance("inches"), 4.8)
        self.assertEqual(runtime.infrared("4").proximity(), 31)
        self.assertEqual(runtime.infrared("4").beacon(), (-4, 22))
        for invalid_port in (0, "0", 5, "5"):
            with self.assertRaisesRegex(ValueError, "sensor port must be 1..4"):
                runtime.color(invalid_port)

    def test_motor_keeps_running_while_sensor_wrappers_read(self) -> None:
        runtime, fake_est = load_runtime()
        runtime.motor_set_speed("A", 30)
        runtime.motor_start("A", "clockwise")
        for _ in range(1200):
            self.assertEqual(runtime.color(4).reflection(), 42)
            self.assertEqual(
                runtime.ultrasonic("3").distance("centimeters"), 123.4
            )
        self.assertNotIn(("motor.stop", "A", 0), fake_est.events)

    def test_display_image_for_draws_refreshes_and_waits(self) -> None:
        runtime, fake_est = load_runtime()
        started = fake_est.now_ms
        runtime.display_image_for("Eyes/Neutral", 0.5)
        self.assertEqual(
            fake_est.events,
            [("display.image", "Eyes/Neutral"), ("display.refresh",)],
        )
        self.assertGreaterEqual(fake_est.now_ms - started, 500)
        with self.assertRaises(ValueError):
            runtime.display_image_for("Eyes/Neutral", -1)

    def test_display_text_converts_values_and_refreshes_once(self) -> None:
        runtime, fake_est = load_runtime()

        runtime.display_text("10", 20.9, 42, font="bold_white")

        self.assertEqual(
            fake_est.events,
            [
                ("display.text", 10, 20, "42", {"font": "bold_white"}),
                ("display.refresh",),
            ],
        )

    def test_display_text_line_converts_values_and_uses_native_refresh(self) -> None:
        runtime, fake_est = load_runtime()

        runtime.display_text_line("3", 21.5)

        self.assertEqual(fake_est.events, [("display.text_line", 3, "21.5")])

    def test_random_int_is_inclusive_and_accepts_reversed_bounds(self) -> None:
        runtime, _ = load_runtime()

        normal = [runtime.random_int(1, 10) for _ in range(64)]
        reversed_bounds = [runtime.random_int(10, 1) for _ in range(64)]

        self.assertTrue(all(1 <= value <= 10 for value in normal))
        self.assertTrue(all(1 <= value <= 10 for value in reversed_bounds))
        self.assertGreater(len(set(normal + reversed_bounds)), 1)
        self.assertEqual(runtime.random_int(7, 7), 7)


class RuntimeEventTests(unittest.TestCase):
    def test_event_listener_limit_is_separate_from_task_slots(self) -> None:
        runtime, _ = load_runtime()

        for index in range(16):
            runtime.on_condition(lambda index=index: False)(lambda: None)

        self.assertEqual(len(runtime._event_handlers), 16)
        self.assertEqual(len(runtime._tasks), 8)
        self.assertTrue(all(not task.active for task in runtime._tasks))
        with self.assertRaisesRegex(RuntimeError, "at most 16"):
            runtime.on_condition(lambda: False)(lambda: None)

    def test_start_tasks_and_async_condition_event_run_together(self) -> None:
        runtime, fake_est = load_runtime()
        state = {"ready": False}
        calls = []

        @runtime.on_condition(lambda: state["ready"])
        async def condition_handler():
            calls.append("event-start")
            await runtime.yield_once()
            calls.append("event-end")

        @runtime.on_start
        async def driver():
            calls.append("start")
            await runtime.yield_once()
            state["ready"] = True
            await runtime.wait_until(lambda: calls[-1:] == ["event-end"])
            runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["start", "event-start", "event-end"])

    def test_button_edges_debounce_rearm_and_center_alias(self) -> None:
        runtime, fake_est = load_runtime()
        fake_est.millis_step = 5
        calls = []

        @runtime.on_brick_button("center", "pressed")
        def pressed():
            calls.append("pressed")

        @runtime.on_brick_button("confirm", "released")
        def released():
            calls.append("released")

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            fake_est.button_mask = fake_est.buttons.CONFIRM
            for _ in range(6):
                await runtime.yield_once()
            for _ in range(4):
                await runtime.yield_once()
            fake_est.button_mask = 0
            for _ in range(6):
                await runtime.yield_once()
            runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["pressed", "released"])

    def test_button_registration_rejects_reserved_and_invalid_values(self) -> None:
        runtime, _ = load_runtime()
        for button in ("back", "none", "invalid"):
            with self.subTest(button=button):
                with self.assertRaises(ValueError):
                    runtime.on_brick_button(button, "pressed")
        with self.assertRaises(ValueError):
            runtime.on_brick_button("left", "held")

    def test_condition_edges_only_and_rearms(self) -> None:
        runtime, fake_est = load_runtime()
        state = {"value": False}
        calls = []

        @runtime.on_condition(lambda: state["value"])
        def handler():
            calls.append("edge")

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            state["value"] = True
            await runtime.yield_once()
            await runtime.yield_once()
            state["value"] = False
            await runtime.yield_once()
            state["value"] = True
            await runtime.yield_once()
            runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["edge", "edge"])

    def test_condition_initial_true_needs_false_before_first_edge(self) -> None:
        runtime, fake_est = load_runtime()
        state = {"value": True}
        calls = []

        @runtime.on_condition(lambda: state["value"])
        def handler():
            calls.append("edge")
            runtime.stop("all")

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            self.assertEqual(calls, [])
            state["value"] = False
            await runtime.yield_once()
            state["value"] = True

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["edge"])

    def test_timer_fires_once_per_generation_and_reset_rearms(self) -> None:
        runtime, fake_est = load_runtime()
        fake_est.millis_step = 50
        calls = []

        @runtime.on_timer_gt(0.1)
        def handler():
            calls.append("timer")
            if len(calls) == 1:
                runtime.reset_timer()
            else:
                runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["timer", "timer"])

    def test_timer_rejects_negative_nan_and_invalid_values(self) -> None:
        runtime, _ = load_runtime()
        for value in (-1, float("nan"), None, "invalid"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    runtime.on_timer_gt(value)

    def test_event_is_not_reentrant_and_records_only_one_pending_run(self) -> None:
        runtime, fake_est = load_runtime()
        fake_est.millis_step = 20
        state = {"value": False}
        calls = []

        @runtime.on_condition(lambda: state["value"])
        async def handler():
            calls.append("start")
            await runtime.sleep(0.2)
            calls.append("end")
            if calls.count("end") == 2:
                runtime.stop("all")

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            state["value"] = True
            await runtime.yield_once()
            state["value"] = False
            await runtime.yield_once()
            state["value"] = True
            await runtime.yield_once()
            state["value"] = False
            await runtime.yield_once()
            state["value"] = True

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["start", "end", "start", "end"])

    def test_full_task_pool_keeps_one_pending_event_until_slot_is_free(self) -> None:
        runtime, fake_est = load_runtime()
        state = {"value": False}
        calls = []

        @runtime.on_condition(lambda: state["value"])
        def handler():
            calls.append("event")
            runtime.stop("all")

        @runtime.on_start
        async def driver():
            state["value"] = True
            await runtime.yield_once()

        for _ in range(7):
            @runtime.on_start
            async def blocker():
                while True:
                    await runtime.yield_once()

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["event"])
        self.assertEqual(len(runtime._tasks), 8)

    def test_task_slots_are_reused_across_many_event_triggers(self) -> None:
        runtime, fake_est = load_runtime()
        state = {"value": False}
        calls = []
        task_ids = [id(task) for task in runtime._tasks]

        @runtime.on_condition(lambda: state["value"])
        def handler():
            calls.append(1)
            if len(calls) == 20:
                runtime.stop("all")

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            for _ in range(20):
                state["value"] = True
                await runtime.yield_once()
                state["value"] = False
                await runtime.yield_once()

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(len(calls), 20)
        self.assertEqual([id(task) for task in runtime._tasks], task_ids)

    def test_event_this_stack_can_trigger_again(self) -> None:
        runtime, fake_est = load_runtime()
        state = {"value": False}
        calls = []

        @runtime.on_condition(lambda: state["value"])
        async def handler():
            calls.append("run")
            runtime.stop("this_stack")

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            for _ in range(2):
                state["value"] = True
                await runtime.yield_once()
                state["value"] = False
                await runtime.yield_once()
            runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["run", "run"])

    def test_stop_other_stacks_keeps_event_listeners_registered(self) -> None:
        runtime, fake_est = load_runtime()
        fake_est.millis_step = 50
        state = {"value": False}
        calls = []

        @runtime.on_condition(lambda: state["value"])
        def condition_handler():
            calls.append("condition")
            if len(calls) == 1:
                runtime.stop_other_stacks()
                state["value"] = False
                runtime.reset_timer()
            else:
                runtime.stop("all")

        @runtime.on_timer_gt(0.1)
        def timer_handler():
            state["value"] = True

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            state["value"] = True
            while True:
                await runtime.yield_once()

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["condition", "condition"])
        self.assertEqual(len(runtime._event_handlers), 2)

    def test_event_task_cleanup_does_not_stop_new_motor_owner(self) -> None:
        runtime, fake_est = load_runtime()
        state = {"value": False}

        @runtime.on_condition(lambda: state["value"])
        async def old_owner():
            runtime.motor_set_speed("A", 20)
            runtime.motor_start("A", "clockwise")
            await runtime.yield_once()
            await runtime.yield_once()

        @runtime.on_start
        async def new_owner():
            await runtime.yield_once()
            state["value"] = True
            await runtime.yield_once()
            await runtime.yield_once()
            runtime.motor_set_speed("A", 75)
            runtime.motor_start("A", "clockwise")
            await runtime.yield_once()
            runtime.motor_set_speed("A", 80)
            runtime.motor_start("A", "clockwise")
            runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        events = fake_est.events
        speed_75 = events.index(("motor.run_speed", "A", 75))
        speed_80 = events.index(("motor.run_speed", "A", 80))
        self.assertNotIn(("motor.stop", "A", 0), events[speed_75 + 1:speed_80])

    def test_plain_handler_is_supported_but_waits_require_async(self) -> None:
        runtime, fake_est = load_runtime()
        state = {"value": False}

        @runtime.on_condition(lambda: state["value"])
        def handler():
            runtime.sleep(0.1)

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            state["value"] = True

        with self.assertRaisesRegex(RuntimeError, "requires an async"):
            runtime.run()
        self.assertEqual(fake_est.events, [])

    def test_only_event_hats_keep_run_alive(self) -> None:
        runtime, fake_est = load_runtime()
        calls = []

        @runtime.on_timer_gt(0.2)
        def handler():
            calls.append("event-only")
            runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["event-only"])

    def test_predicate_exception_escapes_as_program_error(self) -> None:
        runtime, _ = load_runtime()

        def broken_predicate():
            raise ValueError("predicate failed")

        @runtime.on_condition(broken_predicate)
        def handler():
            pass

        with self.assertRaisesRegex(ValueError, "predicate failed"):
            runtime.run()


class RuntimeSensorWaitTests(unittest.TestCase):
    def test_synchronous_waits_return_when_conditions_are_already_true(self) -> None:
        runtime, fake_est = load_runtime()

        runtime.wait_brick_button("confirm", "released")
        runtime.wait_color("3", "red")
        runtime.wait_touch(1, "pressed")
        runtime.wait_ultrasonic("4", "less", 200, "centimeters")
        runtime.wait_ir_proximity(4, "greater", 20)
        runtime.wait_ir_beacon_button("4", 1, "active")
        runtime.wait_gyro("2", "greater", 20)

        self.assertEqual(fake_est.now_ms, 200)

    def test_all_sensor_waits_cooperate_with_other_tasks(self) -> None:
        runtime, fake_est = load_runtime()
        fake_est.touch_pressed = False
        fake_est.color_value = 5
        fake_est.ultrasonic_distance_mm = 1000
        fake_est.infrared_proximity = 10
        fake_est.infrared_remote = 0
        fake_est.gyro_angle = 0
        calls = []

        @runtime.on_start
        async def waiter():
            await runtime.wait_brick_button("center", "pressed")
            calls.append("button")
            await runtime.wait_color(3, "blue")
            calls.append("color")
            await runtime.wait_touch("1", "pressed")
            calls.append("touch")
            await runtime.wait_ultrasonic(4, "less", 15, "centimeters")
            calls.append("ultrasonic")
            await runtime.wait_ir_proximity("4", "greater", 50)
            calls.append("ir-proximity")
            await runtime.wait_ir_beacon_button(4, "1", "top_left_pressed")
            calls.append("ir-remote")
            await runtime.wait_gyro(2, "greater", 45)
            calls.append("gyro")
            runtime.stop("all")

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            fake_est.button_mask = fake_est.buttons.CONFIRM
            while "button" not in calls:
                await runtime.yield_once()
            fake_est.color_value = 2
            while "color" not in calls:
                await runtime.yield_once()
            fake_est.touch_pressed = True
            while "touch" not in calls:
                await runtime.yield_once()
            fake_est.ultrasonic_distance_mm = 100
            while "ultrasonic" not in calls:
                await runtime.yield_once()
            fake_est.infrared_proximity = 60
            while "ir-proximity" not in calls:
                await runtime.yield_once()
            fake_est.infrared_remote = 1
            while "ir-remote" not in calls:
                await runtime.yield_once()
            fake_est.gyro_angle = 90

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(
            calls,
            [
                "button", "color", "touch", "ultrasonic",
                "ir-proximity", "ir-remote", "gyro",
            ],
        )

    def test_changed_waits_capture_a_baseline(self) -> None:
        runtime, fake_est = load_runtime()
        fake_est.color_value = 5
        fake_est.gyro_angle = 10
        calls = []

        @runtime.on_start
        async def waiter():
            await runtime.wait_color(3, "changed")
            calls.append("color")
            await runtime.wait_gyro(2, "changed", 5)
            calls.append("gyro")
            runtime.stop("all")

        @runtime.on_start
        async def driver():
            await runtime.yield_once()
            fake_est.color_value = 2
            while "color" not in calls:
                await runtime.yield_once()
            fake_est.gyro_angle = 16

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["color", "gyro"])

    def test_wait_parameter_errors_are_explicit(self) -> None:
        runtime, _ = load_runtime()

        with self.assertRaisesRegex(ValueError, "global program stop"):
            runtime.wait_brick_button("back", "pressed")
        with self.assertRaisesRegex(ValueError, "event must"):
            runtime.wait_touch(1, "held")
        with self.assertRaisesRegex(ValueError, "unknown color"):
            runtime.wait_color(3, "purple")
        with self.assertRaisesRegex(ValueError, "comparator"):
            runtime.wait_gyro(2, "around", 10)
        with self.assertRaisesRegex(ValueError, "distance unit"):
            runtime.wait_ultrasonic(4, "less", 10, "meters")
        with self.assertRaisesRegex(ValueError, "channel must be 1"):
            runtime.wait_ir_beacon_button(4, 2, "active")
        with self.assertRaisesRegex(ValueError, "unknown infrared"):
            runtime.wait_ir_beacon_button(4, 1, "held")


class RuntimeBroadcastTests(unittest.TestCase):
    def test_all_eight_messages_register_and_invalid_names_fail(self) -> None:
        runtime, _ = load_runtime()

        for index in range(1, 9):
            handler = lambda: None
            self.assertIs(
                runtime.on_broadcast(f"message_{index}")(handler),
                handler,
            )
        self.assertEqual(len(runtime._event_handlers), 8)
        with self.assertRaisesRegex(ValueError, "message_1..message_8"):
            runtime.on_broadcast("message_9")
        with self.assertRaisesRegex(ValueError, "message_1..message_8"):
            runtime.broadcast("hello")
        with self.assertRaisesRegex(ValueError, "True or False"):
            runtime.broadcast("message_1", wait="yes")
        with self.assertRaisesRegex(RuntimeError, "requires an async task"):
            runtime.broadcast("message_1", wait=True)

    def test_broadcast_without_wait_returns_before_async_receiver(self) -> None:
        runtime, fake_est = load_runtime()
        calls = []

        @runtime.on_broadcast("message_1")
        async def receiver():
            calls.append("receiver")

        @runtime.on_broadcast("message_2")
        def other_message():
            calls.append("wrong-message")

        @runtime.on_start
        async def sender():
            calls.append("before")
            runtime.broadcast("message_1", wait=False)
            calls.append("after")
            while "receiver" not in calls:
                await runtime.yield_once()
            runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["before", "after", "receiver"])

    def test_broadcast_wait_waits_for_all_matching_receivers(self) -> None:
        runtime, fake_est = load_runtime()
        calls = []

        @runtime.on_broadcast("message_1")
        async def slow_receiver():
            calls.append("slow-start")
            await runtime.sleep(0.3)
            calls.append("slow-end")

        @runtime.on_broadcast("message_1")
        def immediate_receiver():
            calls.append("immediate")

        @runtime.on_start
        async def sender():
            calls.append("before")
            await runtime.broadcast("message_1", wait=True)
            calls.append("after")
            runtime.stop("all")

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(
            calls,
            ["before", "immediate", "slow-start", "slow-end", "after"],
        )

    def test_repeated_broadcast_coalesces_one_pending_receiver_run(self) -> None:
        runtime, fake_est = load_runtime()
        calls = []

        @runtime.on_broadcast("message_1")
        async def receiver():
            calls.append("start")
            await runtime.sleep(0.2)
            calls.append("end")
            if calls.count("end") == 2:
                runtime.stop("all")

        @runtime.on_start
        async def sender():
            runtime.broadcast("message_1")
            runtime.broadcast("message_1")
            runtime.broadcast("message_1")
            while True:
                await runtime.yield_once()

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["start", "end", "start", "end"])

    def test_broadcast_wait_does_not_deadlock_its_own_event_hat(self) -> None:
        runtime, fake_est = load_runtime()
        calls = []

        @runtime.on_broadcast("message_1")
        async def receiver():
            calls.append("run")
            if len(calls) == 1:
                await runtime.broadcast("message_1", wait=True)
            else:
                runtime.stop("all")

        @runtime.on_start
        async def sender():
            runtime.broadcast("message_1")
            while True:
                await runtime.yield_once()

        with self.assertRaises(fake_est.GlobalProgramStop):
            runtime.run()
        self.assertEqual(calls, ["run", "run"])


class RuntimeContractTests(unittest.TestCase):
    def test_generator_runtime_names_are_explicit(self) -> None:
        runtime, _ = load_runtime()
        expected = {
            "boolean", "broadcast", "color", "compare", "display_image_for",
            "display_text", "display_text_line",
            "drive_dual_speed_for", "drive_move_for", "drive_set_pair",
            "drive_set_speed", "drive_set_stop_action",
            "drive_start_dual_power", "drive_start_dual_speed", "drive_start_steer",
            "drive_steer_for", "drive_stop", "gyro", "infrared",
            "motor", "motor_run_for",
            "line_follow_dual_power_step", "line_follow_init",
            "motor_set_speed", "motor_set_stop_action", "motor_stalled", "motor_start",
            "motor_start_power", "motor_start_speed", "motor_stop",
            "on_brick_button", "on_broadcast", "on_condition", "on_start",
            "on_timer_gt", "random_int", "repeat_count", "reset_timer", "run",
            "seconds_to_ms", "sleep", "stop", "stop_other_stacks",
            "temperature", "timer_seconds", "touch", "ultrasonic", "wait_brick_button",
            "wait_color", "wait_gyro", "wait_ir_beacon_button",
            "wait_ir_proximity", "wait_touch", "wait_ultrasonic",
            "wait_until", "yield_once",
        }
        self.assertEqual([name for name in expected if not hasattr(runtime, name)], [])

    def test_unsupported_interfaces_raise(self) -> None:
        runtime, _ = load_runtime()
        with self.assertRaisesRegex(
            NotImplementedError, "compare changed"
        ):
            runtime.compare(1, "changed", 2)

    def test_removed_unsupported_interfaces_are_absent(self) -> None:
        runtime, _ = load_runtime()
        removed = (
            "color_calibrate", "color_reset_calibration", "ir_beacon_compare",
            "line_follow_dual_step", "on_color", "on_gyro_angle",
            "on_ir_beacon_button", "on_ir_proximity", "on_touch",
            "on_ultrasonic",
        )
        self.assertEqual([name for name in removed if hasattr(runtime, name)], [])


if __name__ == "__main__":
    unittest.main()
