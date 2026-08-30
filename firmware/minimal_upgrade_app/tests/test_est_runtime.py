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

        def run_speed(self, speed):
            self.states = []
            self.timed_until_ms = None
            module.events.append(("motor.run_speed", self.port, speed))

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

    class Display:
        @staticmethod
        def image(name):
            module.events.append(("display.image", name))

        @staticmethod
        def refresh():
            module.events.append(("display.refresh",))

    module.millis = millis
    module.Motor = Motor
    module.DriveBase = DriveBase
    module.TouchSensor = TouchSensor
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

    def test_motor_start_forwards_20_zero_75_without_interrupting(self) -> None:
        runtime, fake_est = load_runtime()

        runtime.motor_set_speed("A", 20)
        runtime.motor_start("A", "clockwise")
        runtime.motor_set_speed("A", 0)
        runtime.motor_start("A", "clockwise")
        runtime.motor_set_speed("A", 75)
        runtime.motor_start("A", "clockwise")

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
        with self.assertRaisesRegex(ValueError, "0..100"):
            runtime.motor_set_speed("A", 101)

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

    def test_sensor_wrappers_and_cache(self) -> None:
        runtime, _ = load_runtime()
        self.assertIs(runtime.color("3"), runtime.color("3"))
        self.assertEqual(runtime.color("3").reflection(), 42)
        self.assertTrue(runtime.touch("1").pressed())
        self.assertEqual(runtime.gyro("2").angle(), 90)
        self.assertIs(runtime.temperature("2"), runtime.temperature("2"))
        self.assertEqual(runtime.temperature("2").celsius(), 21.5)
        self.assertEqual(runtime.temperature("2").fahrenheit(), 70.7)
        self.assertEqual(runtime.ultrasonic("4").distance("centimeters"), 123.4)
        self.assertEqual(runtime.ultrasonic("4").distance("inches"), 4.8)
        self.assertEqual(runtime.infrared("4").proximity(), 31)
        self.assertEqual(runtime.infrared("4").beacon(), (-4, 22))
        with self.assertRaises(NotImplementedError):
            runtime.infrared("4").beacon_heading(1)

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
            "temperature", "timer_seconds", "touch", "ultrasonic", "wait_brick_button",
            "wait_color", "wait_gyro", "wait_ir_beacon_button",
            "wait_ir_proximity", "wait_touch", "wait_ultrasonic",
            "wait_until", "yield_once",
        }
        self.assertEqual([name for name in expected if not hasattr(runtime, name)], [])

    def test_unsupported_interfaces_raise(self) -> None:
        runtime, _ = load_runtime()
        unsupported = (
            "broadcast", "color_calibrate", "color_reset_calibration",
            "drive_dual_speed_for",
            "drive_start_dual_speed", "ir_beacon_compare",
            "on_broadcast", "on_color",
            "on_gyro_angle", "on_ir_beacon_button", "on_ir_proximity",
            "on_touch", "on_ultrasonic",
            "wait_brick_button", "wait_color",
            "wait_gyro", "wait_ir_beacon_button", "wait_ir_proximity",
            "wait_touch", "wait_ultrasonic",
        )
        for name in unsupported:
            with self.subTest(name=name):
                with self.assertRaisesRegex(NotImplementedError, name):
                    getattr(runtime, name)()


if __name__ == "__main__":
    unittest.main()
