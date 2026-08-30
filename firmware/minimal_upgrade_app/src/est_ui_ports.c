#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "board_motor.h"
#include "board_sensor.h"
#include "est_ui_ports.h"

static uint8_t append_text(char *output, uint8_t index, uint8_t capacity,
	const char *text)
{
	while (*text != '\0' && index + 1U < capacity) {
		output[index++] = *text++;
	}
	output[index] = '\0';
	return index;
}

static uint8_t append_uint(char *output, uint8_t index, uint8_t capacity,
	uint32_t value)
{
	char reversed[10];
	uint8_t length = 0U;

	do {
		reversed[length++] = (char)('0' + (value % 10U));
		value /= 10U;
	} while (value != 0U && length < sizeof(reversed));
	while (length != 0U && index + 1U < capacity) {
		output[index++] = reversed[--length];
	}
	output[index] = '\0';
	return index;
}

static uint8_t append_int(char *output, uint8_t index, uint8_t capacity,
	int32_t value)
{
	uint32_t magnitude;

	if (value < 0) {
		if (index + 1U < capacity) {
			output[index++] = '-';
		}
		magnitude = (uint32_t)(-(value + 1)) + 1U;
	} else {
		magnitude = (uint32_t)value;
	}
	return append_uint(output, index, capacity, magnitude);
}

static void format_int(int32_t value, const char *unit, char output[20])
{
	uint8_t index = append_int(output, 0U, 20U, value);

	(void)append_text(output, index, 20U, unit);
}

static void format_tenths(int16_t value, const char *unit, char output[20])
{
	uint16_t magnitude;
	uint8_t index = 0U;

	if (value < 0) {
		output[index++] = '-';
		magnitude = (uint16_t)(-(int32_t)value);
	} else {
		magnitude = (uint16_t)value;
	}
	index = append_uint(output, index, 20U, magnitude / 10U);
	if (index + 1U < 20U) {
		output[index++] = '.';
		output[index++] = (char)('0' + magnitude % 10U);
		output[index] = '\0';
	}
	(void)append_text(output, index, 20U, unit);
}

static const char *color_name(uint16_t value)
{
	static const char *const names[] = {
		"None", "Black", "Blue", "Green",
		"Yellow", "Red", "White", "Brown"
	};

	return value < sizeof(names) / sizeof(names[0]) ? names[value] : "--";
}

static void capture_motor(uint8_t index, est_ui_motor_port_view_t *view)
{
	struct board_motor_control_snapshot snapshot = {0};

	if (!board_motor_control_snapshot((enum board_motor_port)index, &snapshot) ||
	    snapshot.type == BOARD_MOTOR_TYPE_NONE) {
		view->connection = EST_UI_PORT_DISCONNECTED;
		view->kind = EST_UI_MOTOR_NONE;
		view->state = EST_UI_MOTOR_COAST;
		view->speed_percent = 0;
		view->angle_degrees = 0;
		return;
	}
	if (snapshot.type == BOARD_MOTOR_TYPE_UNKNOWN) {
		view->connection = EST_UI_PORT_DETECTING;
		view->kind = EST_UI_MOTOR_NONE;
		view->state = EST_UI_MOTOR_COAST;
		view->speed_percent = 0;
		view->angle_degrees = 0;
		return;
	}
	view->connection = EST_UI_PORT_CONNECTED;
	view->kind = snapshot.type == BOARD_MOTOR_TYPE_LARGE ?
		EST_UI_MOTOR_LARGE : EST_UI_MOTOR_MEDIUM;
	view->state = snapshot.state == BOARD_MOTOR_OUTPUT_DRIVE ?
		EST_UI_MOTOR_DRIVE : snapshot.state == BOARD_MOTOR_OUTPUT_BRAKE ?
		EST_UI_MOTOR_BRAKE : EST_UI_MOTOR_COAST;
	view->speed_percent = snapshot.speed_percent;
	view->angle_degrees = snapshot.tacho_count;
}

static void format_sensor_value(const struct board_sensor_snapshot *snapshot,
	est_ui_sensor_port_view_t *view)
{
	if (!snapshot->value_valid) {
		strcpy(view->value, "--");
		return;
	}
	if (snapshot->sensor_type == BOARD_SENSOR_TYPE_TOUCH) {
		strcpy(view->value, snapshot->value != 0U ? "Pressed" : "Released");
	} else if (snapshot->sensor_type == BOARD_SENSOR_TYPE_ULTRASONIC &&
		   snapshot->mode == BOARD_SENSOR_MODE_DISTANCE_CM) {
		format_tenths((int16_t)snapshot->value, "cm", view->value);
	} else if (snapshot->sensor_type == BOARD_SENSOR_TYPE_ULTRASONIC &&
		   snapshot->mode == BOARD_SENSOR_MODE_DISTANCE_INCH) {
		format_tenths((int16_t)snapshot->value, "in", view->value);
	} else if (snapshot->sensor_type == BOARD_SENSOR_TYPE_ULTRASONIC) {
		strcpy(view->value, snapshot->value != 0U ? "Yes" : "No");
	} else if (snapshot->sensor_type == BOARD_SENSOR_TYPE_TEMPERATURE) {
		format_tenths((int16_t)snapshot->value,
			snapshot->mode == BOARD_SENSOR_MODE_FAHRENHEIT ? "F" : "C",
			view->value);
	} else if (snapshot->sensor_type == BOARD_SENSOR_TYPE_GYRO) {
		format_int((int32_t)(int16_t)snapshot->value,
			snapshot->mode == BOARD_SENSOR_MODE_GYRO_RATE ? "d/s" : "deg",
			view->value);
	} else if (snapshot->sensor_type == BOARD_SENSOR_TYPE_SOUND) {
		format_int(snapshot->value, "dB", view->value);
	} else if (snapshot->sensor_type == BOARD_SENSOR_TYPE_EV3_COLOR &&
		   snapshot->mode == BOARD_SENSOR_MODE_COLOR) {
		strncpy(view->value, color_name(snapshot->value),
			sizeof(view->value) - 1U);
		view->value[sizeof(view->value) - 1U] = '\0';
	} else if (snapshot->sensor_type == BOARD_SENSOR_TYPE_INFRARED &&
		   snapshot->mode == BOARD_SENSOR_MODE_IR_BEACON) {
		uint8_t heading = (uint8_t)snapshot->value;
		uint8_t distance = (uint8_t)(snapshot->value >> 8U);
		int16_t signed_heading = heading <= 180U ?
			(int16_t)heading : -(int16_t)(255U - heading);
		uint8_t output_index = append_text(view->value, 0U, 20U, "H:");

		output_index = append_int(view->value, output_index, 20U,
			signed_heading);
		output_index = append_text(view->value, output_index, 20U, " D:");
		(void)append_uint(view->value, output_index, 20U, distance);
	} else if (snapshot->sensor_type == BOARD_SENSOR_TYPE_INFRARED &&
		   snapshot->mode == BOARD_SENSOR_MODE_IR_REMOTE &&
		   snapshot->value_size >= 4U) {
		uint8_t channel;
		uint8_t output_index = 0U;

		for (channel = 0U; channel < 4U; channel++) {
			if (channel != 0U) {
				output_index = append_text(view->value, output_index,
					20U, " ");
			}
			output_index = append_uint(view->value, output_index, 20U,
				(uint32_t)channel + 1U);
			output_index = append_text(view->value, output_index, 20U,
				":");
			output_index = append_uint(view->value, output_index, 20U,
				snapshot->value_bytes[channel]);
		}
	} else {
		format_int(snapshot->value, "", view->value);
	}
}

static const char *sensor_mode_label(const struct board_sensor_snapshot *snapshot)
{
	switch (snapshot->sensor_type) {
	case BOARD_SENSOR_TYPE_SOUND:
		return "DB";
	case BOARD_SENSOR_TYPE_TOUCH:
		return "TOUCH";
	case BOARD_SENSOR_TYPE_EV3_COLOR:
		return snapshot->mode == BOARD_SENSOR_MODE_REFLECTED ? "REFL" :
			snapshot->mode == BOARD_SENSOR_MODE_AMBIENT ? "AMBI" : "COLOR";
	case BOARD_SENSOR_TYPE_ULTRASONIC:
		return snapshot->mode == BOARD_SENSOR_MODE_DISTANCE_CM ? "CM" :
			snapshot->mode == BOARD_SENSOR_MODE_DISTANCE_INCH ? "IN" : "PRES";
	case BOARD_SENSOR_TYPE_GYRO:
		return snapshot->mode == BOARD_SENSOR_MODE_GYRO_RATE ? "RATE" :
			"ANGLE";
	case BOARD_SENSOR_TYPE_INFRARED:
		return snapshot->mode == BOARD_SENSOR_MODE_IR_PROXIMITY ? "PROX" :
			snapshot->mode == BOARD_SENSOR_MODE_IR_BEACON ? "BEACON" :
			"REMOTE";
	case BOARD_SENSOR_TYPE_TEMPERATURE:
		return snapshot->mode == BOARD_SENSOR_MODE_FAHRENHEIT ? "F" : "C";
	default:
		return "VALUE";
	}
}

static const char *sensor_model(const struct board_sensor_snapshot *snapshot)
{
	switch (snapshot->sensor_type) {
	case BOARD_SENSOR_TYPE_TOUCH:
		return "Touch";
	case BOARD_SENSOR_TYPE_EV3_COLOR:
		return "Color";
	case BOARD_SENSOR_TYPE_ULTRASONIC:
		return "Ultrasonic";
	case BOARD_SENSOR_TYPE_TEMPERATURE:
		return "Temperature";
	case BOARD_SENSOR_TYPE_GYRO:
		return "Gyro";
	case BOARD_SENSOR_TYPE_SOUND:
		return "Sound";
	case BOARD_SENSOR_TYPE_INFRARED:
		return "Infrared";
	default:
		return "Sensor";
	}
}

static void capture_sensor(uint8_t index, est_ui_sensor_port_view_t *view)
{
	struct board_sensor_snapshot snapshot = {0};

	if (!board_sensor_get_snapshot((enum board_sensor_port)index, &snapshot) ||
	    snapshot.state == BOARD_SENSOR_OFF) {
		view->connection = EST_UI_PORT_DISCONNECTED;
		view->model = NULL;
		view->mode = NULL;
		strcpy(view->value, "--");
		return;
	}
	if (snapshot.state != BOARD_SENSOR_STREAMING) {
		view->connection = EST_UI_PORT_DETECTING;
		view->model = NULL;
		view->mode = NULL;
		strcpy(view->value, "--");
		return;
	}
	view->connection = EST_UI_PORT_CONNECTED;
	view->model = sensor_model(&snapshot);
	view->mode = sensor_mode_label(&snapshot);
	format_sensor_value(&snapshot, view);
}

bool est_ui_ports_cycle_sensor_mode(uint8_t sensor_port, uint32_t now_ms)
{
	struct board_sensor_snapshot snapshot = {0};
	uint8_t mode_count;
	uint8_t next_mode;

	if (sensor_port >= BOARD_SENSOR_PORT_COUNT ||
	    !board_sensor_get_snapshot((enum board_sensor_port)sensor_port,
		&snapshot) || snapshot.state != BOARD_SENSOR_STREAMING) {
		return false;
	}
	switch (snapshot.sensor_type) {
	case BOARD_SENSOR_TYPE_EV3_COLOR:
	case BOARD_SENSOR_TYPE_ULTRASONIC:
	case BOARD_SENSOR_TYPE_INFRARED:
		mode_count = 3U;
		break;
	case BOARD_SENSOR_TYPE_GYRO:
	case BOARD_SENSOR_TYPE_TEMPERATURE:
		mode_count = 2U;
		break;
	default:
		return false;
	}
	next_mode = (uint8_t)((snapshot.mode + 1U) % mode_count);
	return board_sensor_set_mode((enum board_sensor_port)sensor_port,
		(enum board_sensor_mode)next_mode, now_ms);
}

void est_ui_ports_capture(est_ui_ports_view_t *view)
{
	uint8_t index;

	if (view == NULL) {
		return;
	}
	for (index = 0U; index < 4U; index++) {
		capture_motor(index, &view->motors[index]);
		capture_sensor(index, &view->sensors[index]);
	}
}
