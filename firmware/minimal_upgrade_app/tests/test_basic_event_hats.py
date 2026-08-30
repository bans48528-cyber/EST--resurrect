from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNTIME_PATH = ROOT / "micropython_port" / "modules" / "est_runtime.py"
SOURCE_DIR = ROOT / "src"
TOOLS_ROOT = ROOT.parents[1] / "tools" / "est_hid_sender"


class BasicEventHatsContractTests(unittest.TestCase):
    def test_event_records_have_bounded_registration_and_required_state(self) -> None:
        runtime = RUNTIME_PATH.read_text(encoding="utf-8")

        self.assertIn("_MAX_EVENTS = 16", runtime)
        self.assertIn("_MAX_TASKS = 8", runtime)
        self.assertIn("_tasks = [_Task(task_id)", runtime)
        for field in (
            "self.handler", "self.type", "self.last_value", "self.armed",
            "self.active_task", "self.pending",
        ):
            self.assertIn(field, runtime)
        self.assertNotIn("uasyncio", runtime)
        self.assertNotIn("threading", runtime)

    def test_first_stage_hats_are_implemented_and_later_hats_stay_explicit(self) -> None:
        runtime = RUNTIME_PATH.read_text(encoding="utf-8")

        self.assertIn("def on_brick_button(button, event):", runtime)
        self.assertIn("def on_condition(predicate):", runtime)
        self.assertIn("def on_timer_gt(seconds):", runtime)
        self.assertNotIn('on_brick_button = _unsupported', runtime)
        self.assertNotIn('on_condition = _unsupported', runtime)
        self.assertNotIn('on_timer_gt = _unsupported', runtime)
        for name in (
            "on_touch", "on_color", "on_ultrasonic", "on_ir_proximity",
            "on_gyro_angle", "on_broadcast", "on_ir_beacon_button",
        ):
            self.assertIn(f'{name} = _unsupported("{name}")', runtime)

    def test_event_sampling_does_not_use_blocking_sensor_waits(self) -> None:
        runtime = RUNTIME_PATH.read_text(encoding="utf-8")

        sample = runtime.split("def _sample_events", 1)[1].split(
            "def _register_event", 1
        )[0]
        self.assertNotIn("Sensor(", sample)
        self.assertNotIn("wait_value", sample)
        self.assertNotIn("sleep(", sample)

    def test_protocol_125_and_bit24_are_reported_by_firmware_and_tool(self) -> None:
        config = (ROOT / "include" / "app_config.h").read_text(
            encoding="utf-8"
        )
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(
            encoding="utf-8"
        )
        constants = (TOOLS_ROOT / "constants.py").read_text(encoding="utf-8")
        cli = (TOOLS_ROOT / "cli.py").read_text(encoding="utf-8")

        self.assertIn("DEVICE_PROTOCOL_MINOR           25U", config)
        self.assertIn(
            "DEVICE_CAPABILITY_RUNTIME_BASIC_EVENT_HATS (1UL << 24U)",
            config,
        )
        self.assertIn("DEVICE_CAPABILITY_RUNTIME_BASIC_EVENT_HATS", protocol)
        self.assertIn("DEVICE_PROTOCOL_MINOR = 25", constants)
        self.assertIn("DEVICE_CAPABILITY_RUNTIME_BASIC_EVENT_HATS = 1 << 24", constants)
        self.assertIn('"runtime-basic-event-hats"', cli)


if __name__ == "__main__":
    unittest.main()
