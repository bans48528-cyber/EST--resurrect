from __future__ import annotations

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "src"
PORT_DIR = ROOT / "micropython_port"
TOOLS_ROOT = ROOT.parents[1] / "tools" / "est_hid_sender"


class CooperativeMultitaskContractTests(unittest.TestCase):
    def test_frozen_runtime_uses_one_vm_and_bounded_cooperative_tasks(self) -> None:
        runtime = (PORT_DIR / "modules" / "est_runtime.py").read_text(
            encoding="utf-8"
        )
        port = (PORT_DIR / "mpconfigport.h").read_text(encoding="utf-8")

        self.assertIn("_MAX_TASKS = 8", runtime)
        self.assertIn("task.iterator.send(None)", runtime)
        self.assertIn("class _ConditionWait", runtime)
        self.assertIn("class _OperationWait", runtime)
        self.assertIn('if scope == "this_stack":', runtime)
        self.assertIn("def stop_other_stacks():", runtime)
        self.assertIn('if scope == "all":', runtime)
        self.assertNotIn("uasyncio", runtime)
        self.assertNotIn("threading", runtime)
        self.assertNotIn("_thread", runtime)
        self.assertIn("MICROPY_ENABLE_SCHEDULER (0)", port)
        self.assertIn("MICROPY_ENABLE_VM_ABORT (1)", port)
        self.assertIn("MICROPY_PY_ASYNC_AWAIT (1)", port)

    def test_native_global_stop_is_immediate_and_not_python_catchable(self) -> None:
        native = (PORT_DIR / "est_micropython.c").read_text(encoding="utf-8")
        module = (PORT_DIR / "modest.c").read_text(encoding="utf-8")

        self.assertIn("nlr_set_abort(&nlr);", native)
        self.assertIn("mp_sched_vm_abort();", native)
        self.assertIn(
            "mp_handle_pending(MP_HANDLE_PENDING_CALLBACKS_AND_EXCEPTIONS);",
            native,
        )
        self.assertIn("est_motor_stop_all(EST_STOP_COAST)", native)
        self.assertLess(
            native.index("est_motor_stop_all(EST_STOP_COAST)"),
            native.index("mp_sched_vm_abort();"),
        )
        self.assertIn("est_micropython_program_stop_from_vm();", module)
        self.assertIn("MP_QSTR__stop_user_program", module)

    def test_back_button_and_online_disconnect_request_global_stop(self) -> None:
        native = (PORT_DIR / "est_micropython.c").read_text(encoding="utf-8")
        usb = (SOURCE_DIR / "usb_hid.c").read_text(encoding="utf-8")
        store = (SOURCE_DIR / "est_program_store.c").read_text(encoding="utf-8")

        self.assertIn("est_button_is_pressed(EST_BUTTON_BACK)", native)
        self.assertIn("program_requires_host && !usb_hid_host_connected()", native)
        self.assertIn("host_configured && !host_suspended", usb)
        self.assertIn("usbd_register_reset_callback", usb)
        self.assertIn("usbd_register_suspend_callback", usb)
        self.assertIn("usbd_register_resume_callback", usb)
        self.assertIn("est_micropython_program_begin_saved", store)

    def test_confirm_name_and_center_compatibility_share_the_physical_key(self) -> None:
        buttons = (ROOT / "include" / "est_buttons.h").read_text(
            encoding="utf-8"
        )
        module = (PORT_DIR / "modest.c").read_text(encoding="utf-8")

        self.assertIn("EST_BUTTON_CONFIRM = 5", buttons)
        self.assertIn("#define EST_BUTTON_CENTER EST_BUTTON_CONFIRM", buttons)
        self.assertIn("MP_QSTR_CONFIRM", module)
        self.assertIn("MP_QSTR_CENTER", module)

    def test_protocol_and_capability_are_advertised_by_firmware_and_tool(self) -> None:
        config = (ROOT / "include" / "app_config.h").read_text(
            encoding="utf-8"
        )
        protocol = (SOURCE_DIR / "update_protocol.c").read_text(
            encoding="utf-8"
        )
        constants = (TOOLS_ROOT / "constants.py").read_text(encoding="utf-8")
        cli = (TOOLS_ROOT / "cli.py").read_text(encoding="utf-8")

        self.assertIn("DEVICE_PROTOCOL_MINOR           26U", config)
        self.assertIn(
            "DEVICE_CAPABILITY_COOPERATIVE_MULTITASK (1UL << 23U)", config
        )
        self.assertIn("DEVICE_CAPABILITY_COOPERATIVE_MULTITASK", protocol)
        self.assertIn("DEVICE_PROTOCOL_MINOR = 26", constants)
        self.assertIn("DEVICE_CAPABILITY_COOPERATIVE_MULTITASK = 1 << 23", constants)
        self.assertIn('"cooperative-multitask"', cli)


if __name__ == "__main__":
    unittest.main()
