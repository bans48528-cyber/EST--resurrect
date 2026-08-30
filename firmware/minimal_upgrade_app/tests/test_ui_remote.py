from __future__ import annotations

import hashlib
import unittest
from pathlib import Path

try:
    from cffi import FFI
except ImportError:  # pragma: no cover
    FFI = None


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(FFI is None, "cffi is required for native UI tests")
class UiRemoteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ffi = FFI()
        cls.ffi.cdef(
            """
            typedef struct {
                uint8_t group;
                uint8_t codes[2];
                int fault;
                _Bool output_enabled;
            } est_ui_remote_view_t;
            void fake_reset(void);
            void fake_sensor(uint8_t, uint8_t, uint8_t, _Bool,
                uint8_t, uint32_t, const uint8_t *);
            void fake_motor_connected(uint8_t, _Bool);
            int8_t fake_motor_power(uint8_t);
            uint32_t fake_brake_all_count(void);
            void est_ui_remote_init(void);
            void est_ui_remote_enter(uint32_t, uint8_t);
            void est_ui_remote_switch_group(uint32_t, uint8_t);
            void est_ui_remote_leave(void);
            void est_ui_remote_tick(uint32_t, uint8_t, est_ui_remote_view_t *);
            """
        )
        source = ROOT / "src" / "est_ui_remote.c"
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()[:16]
        cls.remote = cls.ffi.verify(
            f"""
            #include <stdbool.h>
            #include <stdint.h>
            #include <string.h>
            #include "board_motor.h"
            #include "board_sensor.h"
            #include "est_motor.h"
            #include "est_ui_remote.h"

            static struct board_sensor_snapshot fake_snapshot;
            static enum board_motor_type fake_types[4];
            static int8_t fake_power[4];
            static uint32_t fake_brake_count;

            void fake_reset(void) {{
                uint8_t index;
                memset(&fake_snapshot, 0, sizeof(fake_snapshot));
                fake_snapshot.state = BOARD_SENSOR_OFF;
                for (index = 0U; index < 4U; index++) {{
                    fake_types[index] = BOARD_MOTOR_TYPE_LARGE;
                    fake_power[index] = 0;
                }}
                fake_brake_count = 0U;
            }}
            void fake_sensor(uint8_t state, uint8_t type, uint8_t mode,
                bool valid, uint8_t size, uint32_t last_data,
                const uint8_t *codes) {{
                fake_snapshot.state = (enum board_sensor_state)state;
                fake_snapshot.sensor_type = type;
                fake_snapshot.mode = mode;
                fake_snapshot.value_valid = valid;
                fake_snapshot.value_size = size;
                fake_snapshot.last_data_ms = last_data;
                if (codes != NULL) memcpy(fake_snapshot.value_bytes, codes, 4U);
            }}
            void fake_motor_connected(uint8_t port, bool connected) {{
                fake_types[port] = connected ? BOARD_MOTOR_TYPE_LARGE :
                    BOARD_MOTOR_TYPE_NONE;
            }}
            int8_t fake_motor_power(uint8_t port) {{ return fake_power[port]; }}
            uint32_t fake_brake_all_count(void) {{ return fake_brake_count; }}

            bool board_sensor_get_snapshot(enum board_sensor_port port,
                struct board_sensor_snapshot *snapshot) {{
                if (port != BOARD_SENSOR_PORT_4 || snapshot == NULL) return false;
                *snapshot = fake_snapshot;
                return true;
            }}
            bool board_sensor_set_mode(enum board_sensor_port port,
                enum board_sensor_mode mode, uint32_t now_ms) {{
                (void)now_ms;
                if (port != BOARD_SENSOR_PORT_4) return false;
                fake_snapshot.mode = (uint8_t)mode;
                return true;
            }}
            bool board_motor_control_snapshot(enum board_motor_port port,
                struct board_motor_control_snapshot *snapshot) {{
                if ((uint32_t)port >= 4U || snapshot == NULL) return false;
                memset(snapshot, 0, sizeof(*snapshot));
                snapshot->type = fake_types[(uint8_t)port];
                return true;
            }}
            bool board_motor_connection_present(enum board_motor_port port,
                bool *connected) {{
                if ((uint32_t)port >= 4U || connected == NULL) return false;
                *connected = fake_types[(uint8_t)port] != BOARD_MOTOR_TYPE_NONE;
                return true;
            }}
            est_result_t est_motor_stop_all(est_stop_mode_t mode) {{
                uint8_t index;
                if (mode == EST_STOP_BRAKE) fake_brake_count++;
                for (index = 0U; index < 4U; index++) fake_power[index] = 0;
                return EST_OK;
            }}
            est_result_t est_motor_stop(est_motor_port_t port,
                est_stop_mode_t mode) {{
                (void)mode;
                fake_power[(uint8_t)port] = 0;
                return EST_OK;
            }}
            est_result_t est_motor_set_power(est_motor_port_t port,
                int8_t power) {{
                if (fake_types[(uint8_t)port] == BOARD_MOTOR_TYPE_NONE)
                    return EST_ERR_NOT_CONNECTED;
                fake_power[(uint8_t)port] = power;
                return EST_OK;
            }}
            #define EST_UI_REMOTE_TEST_SOURCE_HASH "{source_hash}"
            """,
            include_dirs=[str(ROOT / "include"), str(ROOT / "src")],
            sources=[str(source)],
        )

    def setUp(self) -> None:
        self.remote.fake_reset()
        self.remote.est_ui_remote_init()

    def view(self):
        return self.ffi.new("est_ui_remote_view_t *")

    def set_remote(self, codes, last_data=100) -> None:
        values = self.ffi.new("uint8_t[4]", codes)
        self.remote.fake_sensor(2, 0x21, 2, True, 4, last_data, values)

    def test_group_mapping_switch_and_leave_always_brake(self) -> None:
        self.remote.est_ui_remote_enter(100, 0)
        self.assertEqual(self.remote.fake_brake_all_count(), 1)
        self.set_remote([5, 0, 0, 0])
        view = self.view()
        self.remote.est_ui_remote_tick(110, 0, view)
        self.assertEqual(view.fault, 0)
        self.assertTrue(view.output_enabled)
        self.assertEqual(self.remote.fake_motor_power(1), 100)
        self.assertEqual(self.remote.fake_motor_power(2), 100)

        self.remote.est_ui_remote_switch_group(120, 1)
        self.assertEqual(self.remote.fake_brake_all_count(), 2)
        self.set_remote([0, 0, 8, 0], last_data=120)
        self.remote.est_ui_remote_tick(130, 1, view)
        self.assertEqual(self.remote.fake_motor_power(1), -100)
        self.assertEqual(self.remote.fake_motor_power(2), -100)
        self.remote.est_ui_remote_leave()
        self.assertEqual(self.remote.fake_brake_all_count(), 3)
        self.assertEqual(self.remote.fake_motor_power(1), 0)

    def test_missing_ir_timeout_and_disconnect_are_safe_faults(self) -> None:
        view = self.view()
        self.remote.est_ui_remote_enter(100, 0)
        self.remote.est_ui_remote_tick(110, 0, view)
        self.assertEqual(view.fault, 1)

        self.set_remote([1, 0, 0, 0])
        self.remote.est_ui_remote_tick(120, 0, view)
        self.assertEqual(view.fault, 0)
        self.remote.est_ui_remote_tick(600, 0, view)
        self.assertEqual(view.fault, 2)
        self.assertEqual(self.remote.fake_motor_power(1), 0)

        self.set_remote([1, 0, 0, 0], last_data=610)
        self.remote.est_ui_remote_tick(620, 0, view)
        self.remote.fake_sensor(0, 0, 0, False, 0, 0, self.ffi.NULL)
        self.remote.est_ui_remote_tick(630, 0, view)
        self.assertEqual(view.fault, 3)

    def test_commanded_motor_disconnect_brakes_all(self) -> None:
        view = self.view()
        self.remote.est_ui_remote_enter(100, 0)
        self.set_remote([1, 0, 0, 0])
        self.remote.est_ui_remote_tick(110, 0, view)
        self.assertEqual(view.fault, 0)
        self.assertEqual(self.remote.fake_motor_power(1), 100)

        self.remote.fake_motor_connected(1, False)
        self.set_remote([1, 0, 0, 0], last_data=120)
        self.remote.est_ui_remote_tick(120, 0, view)
        self.assertEqual(view.fault, 3)
        self.assertFalse(view.output_enabled)
        for port in range(4):
            self.assertEqual(self.remote.fake_motor_power(port), 0)


if __name__ == "__main__":
    unittest.main()
