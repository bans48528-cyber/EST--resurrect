#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "board_motor.h"
#include "board_sensor.h"
#include "est_motor.h"
#include "est_ui_remote.h"

#define EST_UI_REMOTE_SENSOR_PORT BOARD_SENSOR_PORT_4
#define EST_UI_REMOTE_SIGNAL_TIMEOUT_MS 500U
#define EST_UI_REMOTE_MODE_RETRY_MS 250U

static bool active;
static bool ever_ready;
static bool output_enabled;
static bool codes_applied;
static uint8_t active_group;
static uint8_t last_codes[2];
static est_ui_remote_fault_t fault;
static uint32_t entered_ms;
static uint32_t next_mode_request_ms;

static void brake_all(void)
{
	(void)est_motor_stop_all(EST_STOP_BRAKE);
	output_enabled = false;
	codes_applied = false;
}

static bool motor_connected(uint8_t port)
{
	struct board_motor_control_snapshot snapshot = {0};
	bool connected = false;

	return board_motor_connection_present((enum board_motor_port)port,
		&connected) && connected &&
		board_motor_control_snapshot((enum board_motor_port)port,
		&snapshot) &&
		(snapshot.type == BOARD_MOTOR_TYPE_LARGE ||
		 snapshot.type == BOARD_MOTOR_TYPE_MEDIUM);
}

static void decode_remote_code(uint8_t code, int8_t *first, int8_t *second)
{
	*first = 0;
	*second = 0;
	switch (code) {
	case 1U:
		*first = 100;
		break;
	case 2U:
		*first = -100;
		break;
	case 3U:
		*second = 100;
		break;
	case 4U:
		*second = -100;
		break;
	case 5U:
		*first = 100;
		*second = 100;
		break;
	case 6U:
		*first = 100;
		*second = -100;
		break;
	case 7U:
		*first = -100;
		*second = 100;
		break;
	case 8U:
		*first = -100;
		*second = -100;
		break;
	default:
		break;
	}
}

static void decode_group_power(const uint8_t codes[2],
	int8_t power[BOARD_MOTOR_PORT_COUNT])
{
	decode_remote_code(codes[0], &power[BOARD_MOTOR_PORT_B],
		&power[BOARD_MOTOR_PORT_C]);
	decode_remote_code(codes[1], &power[BOARD_MOTOR_PORT_A],
		&power[BOARD_MOTOR_PORT_D]);
}

static bool commanded_motors_connected(const uint8_t codes[2])
{
	int8_t power[BOARD_MOTOR_PORT_COUNT] = {0};
	uint8_t port;

	decode_group_power(codes, power);
	for (port = 0U; port < BOARD_MOTOR_PORT_COUNT; port++) {
		if (power[port] != 0 && !motor_connected(port)) {
			return false;
		}
	}
	return true;
}

static bool apply_group_codes(const uint8_t codes[2])
{
	int8_t power[BOARD_MOTOR_PORT_COUNT] = {0};
	uint8_t port;

	decode_group_power(codes, power);
	for (port = 0U; port < BOARD_MOTOR_PORT_COUNT; port++) {
		est_result_t result = power[port] == 0 ?
			est_motor_stop((est_motor_port_t)port, EST_STOP_BRAKE) :
			est_motor_set_power((est_motor_port_t)port, power[port]);

		if (result != EST_OK) {
			return false;
		}
	}
	output_enabled = power[0] != 0 || power[1] != 0 ||
		power[2] != 0 || power[3] != 0;
	return true;
}

static void set_fault(est_ui_remote_fault_t next_fault)
{
	if (fault != next_fault || output_enabled || codes_applied) {
		brake_all();
	}
	fault = next_fault;
}

void est_ui_remote_init(void)
{
	active = false;
	ever_ready = false;
	output_enabled = false;
	codes_applied = false;
	active_group = 0U;
	memset(last_codes, 0, sizeof(last_codes));
	fault = EST_UI_REMOTE_FAULT_CONNECT_IR;
	entered_ms = 0U;
	next_mode_request_ms = 0U;
}

void est_ui_remote_enter(uint32_t now_ms, uint8_t group)
{
	brake_all();
	active = true;
	ever_ready = false;
	active_group = group & 1U;
	memset(last_codes, 0, sizeof(last_codes));
	fault = EST_UI_REMOTE_FAULT_CONNECT_IR;
	entered_ms = now_ms;
	next_mode_request_ms = now_ms;
}

void est_ui_remote_switch_group(uint32_t now_ms, uint8_t group)
{
	brake_all();
	active_group = group & 1U;
	memset(last_codes, 0, sizeof(last_codes));
	entered_ms = now_ms;
}

void est_ui_remote_leave(void)
{
	brake_all();
	active = false;
}

void est_ui_remote_tick(uint32_t now_ms, uint8_t group,
	est_ui_remote_view_t *view)
{
	struct board_sensor_snapshot sensor = {0};
	uint8_t first_channel;
	uint8_t codes[2] = {0};

	if (view == NULL) {
		return;
	}
	if (active_group != (group & 1U)) {
		est_ui_remote_switch_group(now_ms, group);
	}
	if (!active) {
		set_fault(EST_UI_REMOTE_FAULT_CONNECT_IR);
	} else if (!board_sensor_get_snapshot(EST_UI_REMOTE_SENSOR_PORT,
		&sensor) || sensor.sensor_type != BOARD_SENSOR_TYPE_INFRARED ||
		sensor.state == BOARD_SENSOR_OFF) {
		set_fault(ever_ready ? EST_UI_REMOTE_FAULT_DEVICE_LOST :
			EST_UI_REMOTE_FAULT_CONNECT_IR);
	} else if (sensor.state != BOARD_SENSOR_STREAMING) {
		set_fault(ever_ready ? EST_UI_REMOTE_FAULT_DEVICE_LOST :
			EST_UI_REMOTE_FAULT_CONNECT_IR);
	} else if (sensor.mode != BOARD_SENSOR_MODE_IR_REMOTE) {
		set_fault(EST_UI_REMOTE_FAULT_CONNECT_IR);
		if ((int32_t)(now_ms - next_mode_request_ms) >= 0) {
			(void)board_sensor_set_mode(EST_UI_REMOTE_SENSOR_PORT,
				BOARD_SENSOR_MODE_IR_REMOTE, now_ms);
			next_mode_request_ms = now_ms + EST_UI_REMOTE_MODE_RETRY_MS;
		}
	} else if (!sensor.value_valid || sensor.value_size < 4U ||
		   (sensor.last_data_ms == 0U ?
		    now_ms - entered_ms : now_ms - sensor.last_data_ms) >=
		   EST_UI_REMOTE_SIGNAL_TIMEOUT_MS) {
		set_fault(EST_UI_REMOTE_FAULT_SIGNAL_LOST);
	} else {
		ever_ready = true;
		fault = EST_UI_REMOTE_FAULT_NONE;
		first_channel = active_group == 0U ? 0U : 2U;
		codes[0] = sensor.value_bytes[first_channel];
		codes[1] = sensor.value_bytes[first_channel + 1U];
		if (!commanded_motors_connected(codes)) {
			set_fault(EST_UI_REMOTE_FAULT_DEVICE_LOST);
		} else if (!codes_applied || codes[0] != last_codes[0] ||
		    codes[1] != last_codes[1]) {
			if (!apply_group_codes(codes)) {
				set_fault(EST_UI_REMOTE_FAULT_DEVICE_LOST);
			} else {
				last_codes[0] = codes[0];
				last_codes[1] = codes[1];
				codes_applied = true;
			}
		}
	}
	view->group = active_group;
	view->codes[0] = last_codes[0];
	view->codes[1] = last_codes[1];
	view->fault = fault;
	view->output_enabled = output_enabled;
}
