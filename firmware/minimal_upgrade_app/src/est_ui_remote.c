#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "board_motor.h"
#include "board_sensor.h"
#include "est_motor.h"
#include "est_ui_remote.h"

#define EST_UI_REMOTE_SENSOR_PORT BOARD_SENSOR_PORT_4
#define EST_UI_REMOTE_SIGNAL_TIMEOUT_MS 500U
#define EST_UI_REMOTE_MODE_RETRY_MS 250U
#define EST_UI_REMOTE_RELEASE_DEBOUNCE_MS 350U
#define EST_UI_REMOTE_MOTOR_CHECK_INTERVAL_MS 20U
#define EST_UI_REMOTE_MOTOR_DISCONNECT_SAMPLES 3U

static bool active;
static bool ever_ready;
static bool output_enabled;
static bool code_applied;
static bool release_pending;
static uint8_t active_motor_group;
static uint8_t last_code;
static est_ui_remote_fault_t fault;
static uint32_t entered_ms;
static uint32_t next_mode_request_ms;
static uint32_t release_started_ms;
static uint32_t next_motor_check_ms;
static uint8_t motor_disconnect_samples;
static bool motor_group_ready;

static void brake_all(void)
{
	(void)est_motor_stop_all(EST_STOP_BRAKE);
	output_enabled = false;
	code_applied = false;
	release_pending = false;
	last_code = 0U;
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

static void decode_group_power(uint8_t code, uint8_t motor_group,
	int8_t power[BOARD_MOTOR_PORT_COUNT])
{
	if ((motor_group & 1U) == 0U) {
		decode_remote_code(code, &power[BOARD_MOTOR_PORT_B],
			&power[BOARD_MOTOR_PORT_C]);
	} else {
		decode_remote_code(code, &power[BOARD_MOTOR_PORT_A],
			&power[BOARD_MOTOR_PORT_D]);
	}
}

static bool motor_group_connected(uint8_t motor_group)
{
	if ((motor_group & 1U) == 0U) {
		return motor_connected(BOARD_MOTOR_PORT_B) &&
			motor_connected(BOARD_MOTOR_PORT_C);
	}
	return motor_connected(BOARD_MOTOR_PORT_A) &&
		motor_connected(BOARD_MOTOR_PORT_D);
}

static bool motor_group_connection_ready(uint32_t now_ms,
	uint8_t motor_group)
{
	bool connected;

	if ((int32_t)(now_ms - next_motor_check_ms) < 0) {
		return motor_group_ready;
	}
	next_motor_check_ms = now_ms + EST_UI_REMOTE_MOTOR_CHECK_INTERVAL_MS;
	connected = motor_group_connected(motor_group);
	if (connected) {
		motor_disconnect_samples = 0U;
		motor_group_ready = true;
		return true;
	}
	if (!motor_group_ready) {
		return false;
	}
	if (motor_disconnect_samples <
	    EST_UI_REMOTE_MOTOR_DISCONNECT_SAMPLES) {
		motor_disconnect_samples++;
	}
	if (motor_disconnect_samples >=
	    EST_UI_REMOTE_MOTOR_DISCONNECT_SAMPLES) {
		motor_group_ready = false;
	}
	return motor_group_ready;
}

static bool apply_group_code(uint8_t code, uint8_t motor_group)
{
	int8_t power[BOARD_MOTOR_PORT_COUNT] = {0};
	uint8_t port;

	decode_group_power(code, motor_group, power);
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

static bool apply_sampled_code(uint32_t now_ms, uint8_t code)
{
	if (code != 0U) {
		release_pending = false;
	} else if (code_applied && last_code != 0U) {
		if (!release_pending) {
			release_pending = true;
			release_started_ms = now_ms;
			return true;
		}
		if ((uint32_t)(now_ms - release_started_ms) <
		    EST_UI_REMOTE_RELEASE_DEBOUNCE_MS) {
			return true;
		}
		release_pending = false;
	}
	if (code_applied && code == last_code) {
		return true;
	}
	if (!apply_group_code(code, active_motor_group)) {
		return false;
	}
	last_code = code;
	code_applied = true;
	return true;
}

static void set_fault(est_ui_remote_fault_t next_fault)
{
	if (fault != next_fault || output_enabled || code_applied) {
		brake_all();
	}
	fault = next_fault;
}

void est_ui_remote_init(void)
{
	active = false;
	ever_ready = false;
	output_enabled = false;
	code_applied = false;
	release_pending = false;
	active_motor_group = 0U;
	last_code = 0U;
	fault = EST_UI_REMOTE_FAULT_CONNECT_IR;
	entered_ms = 0U;
	next_mode_request_ms = 0U;
	release_started_ms = 0U;
	next_motor_check_ms = 0U;
	motor_disconnect_samples = 0U;
	motor_group_ready = false;
}

void est_ui_remote_enter(uint32_t now_ms, uint8_t motor_group)
{
	brake_all();
	active = true;
	ever_ready = false;
	active_motor_group = motor_group & 1U;
	last_code = 0U;
	fault = EST_UI_REMOTE_FAULT_CONNECT_IR;
	entered_ms = now_ms;
	next_mode_request_ms = now_ms;
	next_motor_check_ms = now_ms;
	motor_disconnect_samples = 0U;
	motor_group_ready = false;
}

void est_ui_remote_switch_motor_group(uint32_t now_ms, uint8_t motor_group)
{
	brake_all();
	active_motor_group = motor_group & 1U;
	last_code = 0U;
	entered_ms = now_ms;
	next_motor_check_ms = now_ms;
	motor_disconnect_samples = 0U;
	motor_group_ready = false;
}

void est_ui_remote_leave(void)
{
	brake_all();
	active = false;
}

void est_ui_remote_tick(uint32_t now_ms, uint8_t motor_group,
	est_ui_remote_view_t *view)
{
	struct board_sensor_snapshot sensor = {0};
	uint8_t code = 0U;

	if (view == NULL) {
		return;
	}
	if (active_motor_group != (motor_group & 1U)) {
		est_ui_remote_switch_motor_group(now_ms, motor_group);
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
	} else if (!sensor.value_valid || sensor.value_size < 1U ||
		   (sensor.last_data_ms == 0U ?
		    now_ms - entered_ms : now_ms - sensor.last_data_ms) >=
		   EST_UI_REMOTE_SIGNAL_TIMEOUT_MS) {
		set_fault(EST_UI_REMOTE_FAULT_SIGNAL_LOST);
	} else {
		ever_ready = true;
		fault = EST_UI_REMOTE_FAULT_NONE;
		code = sensor.value_bytes[0];
		if (!motor_group_connection_ready(now_ms, active_motor_group)) {
			set_fault(EST_UI_REMOTE_FAULT_DEVICE_LOST);
		} else if (!apply_sampled_code(now_ms, code)) {
			set_fault(EST_UI_REMOTE_FAULT_DEVICE_LOST);
		}
	}
	view->motor_group = active_motor_group;
	view->code = last_code;
	view->fault = fault;
	view->output_enabled = output_enabled;
}
