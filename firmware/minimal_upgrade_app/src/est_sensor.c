#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_sensor.h"
#include "est_sensor.h"
#include "est_system.h"

static bool est_sensor_port_valid(est_sensor_port_t port)
{
	return (uint32_t)port < (uint32_t)EST_SENSOR_PORT_COUNT;
}

static bool est_sensor_mode_valid(est_sensor_mode_t mode)
{
	return (uint32_t)mode <= (uint32_t)EST_SENSOR_MODE_COLOR;
}

static bool est_sensor_mode_supported(est_sensor_type_t type,
	est_sensor_mode_t mode)
{
	switch (type) {
	case EST_SENSOR_TYPE_COLOR:
	case EST_SENSOR_TYPE_ULTRASONIC:
	case EST_SENSOR_TYPE_INFRARED:
		return true;
	case EST_SENSOR_TYPE_GYRO:
	case EST_SENSOR_TYPE_TEMPERATURE:
		return (uint32_t)mode <= 1U;
	case EST_SENSOR_TYPE_SOUND:
	case EST_SENSOR_TYPE_TOUCH:
		return mode == EST_SENSOR_MODE_REFLECTED;
	case EST_SENSOR_TYPE_NONE:
	case EST_SENSOR_TYPE_UNKNOWN:
	default:
		return false;
	}
}

static enum board_sensor_port board_port_from_est(est_sensor_port_t port)
{
	return (enum board_sensor_port)port;
}

static est_sensor_type_t est_type_from_board(uint8_t type)
{
	switch (type) {
	case 0U:
		return EST_SENSOR_TYPE_NONE;
	case BOARD_SENSOR_TYPE_SOUND:
		return EST_SENSOR_TYPE_SOUND;
	case BOARD_SENSOR_TYPE_TEMPERATURE:
		return EST_SENSOR_TYPE_TEMPERATURE;
	case BOARD_SENSOR_TYPE_TOUCH:
		return EST_SENSOR_TYPE_TOUCH;
	case BOARD_SENSOR_TYPE_EV3_COLOR:
		return EST_SENSOR_TYPE_COLOR;
	case BOARD_SENSOR_TYPE_ULTRASONIC:
		return EST_SENSOR_TYPE_ULTRASONIC;
	case BOARD_SENSOR_TYPE_GYRO:
		return EST_SENSOR_TYPE_GYRO;
	case BOARD_SENSOR_TYPE_INFRARED:
		return EST_SENSOR_TYPE_INFRARED;
	default:
		return EST_SENSOR_TYPE_UNKNOWN;
	}
}

static est_sensor_state_t est_state_from_board(enum board_sensor_state state)
{
	switch (state) {
	case BOARD_SENSOR_OFF:
		return EST_SENSOR_DISCONNECTED;
	case BOARD_SENSOR_SYNCING:
		return EST_SENSOR_SYNCING;
	case BOARD_SENSOR_STREAMING:
		return EST_SENSOR_STREAMING;
	case BOARD_SENSOR_STALE:
		return EST_SENSOR_STALE;
	default:
		return EST_SENSOR_ERROR;
	}
}

static est_value_format_t value_format_for(est_sensor_type_t type,
	est_sensor_mode_t mode, bool value_valid)
{
	if (!value_valid) {
		return EST_VALUE_NONE;
	}
	switch (type) {
	case EST_SENSOR_TYPE_TOUCH:
		return EST_VALUE_BOOLEAN;
	case EST_SENSOR_TYPE_SOUND:
		return EST_VALUE_INTEGER;
	case EST_SENSOR_TYPE_COLOR:
		return mode == EST_SENSOR_MODE_COLOR ?
			EST_VALUE_ENUM : EST_VALUE_PERCENT;
	case EST_SENSOR_TYPE_ULTRASONIC:
		if (mode == EST_SENSOR_MODE_PRESENCE) {
			return EST_VALUE_BOOLEAN;
		}
		return mode == EST_SENSOR_MODE_DISTANCE_CM ?
			EST_VALUE_MILLIMETERS : EST_VALUE_FIXED_TENTHS;
	case EST_SENSOR_TYPE_GYRO:
		return mode == EST_SENSOR_MODE_GYRO_RATE ?
			EST_VALUE_DEGREES_PER_SECOND : EST_VALUE_DEGREES;
	case EST_SENSOR_TYPE_TEMPERATURE:
		return mode == EST_SENSOR_MODE_CELSIUS ?
			EST_VALUE_CELSIUS_TENTHS : EST_VALUE_FIXED_TENTHS;
	case EST_SENSOR_TYPE_INFRARED:
		if (mode == EST_SENSOR_MODE_IR_PROXIMITY) {
			return EST_VALUE_PERCENT;
		}
		return mode == EST_SENSOR_MODE_IR_REMOTE ?
			EST_VALUE_ENUM : EST_VALUE_INTEGER;
	case EST_SENSOR_TYPE_NONE:
		return EST_VALUE_NONE;
	case EST_SENSOR_TYPE_UNKNOWN:
	default:
		return EST_VALUE_INTEGER;
	}
}

static int32_t value_from_board(const struct board_sensor_snapshot *snapshot,
	est_sensor_type_t type)
{
	if (type == EST_SENSOR_TYPE_TEMPERATURE ||
	    type == EST_SENSOR_TYPE_GYRO) {
		return (int32_t)(int16_t)snapshot->value;
	}
	return (int32_t)snapshot->value;
}

static est_result_t state_error(est_sensor_state_t state)
{
	switch (state) {
	case EST_SENSOR_DISCONNECTED:
		return EST_ERR_NOT_CONNECTED;
	case EST_SENSOR_SYNCING:
		return EST_ERR_BUSY;
	case EST_SENSOR_STALE:
		return EST_ERR_TIMEOUT;
	case EST_SENSOR_ERROR:
		return EST_ERR_STATE;
	case EST_SENSOR_STREAMING:
	default:
		return EST_OK;
	}
}

est_result_t est_sensor_get_type(est_sensor_port_t port,
	est_sensor_type_t *type)
{
	struct board_sensor_snapshot snapshot;

	if (!est_sensor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (type == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (!board_sensor_get_snapshot(board_port_from_est(port), &snapshot)) {
		return EST_ERR_IO;
	}
	*type = est_type_from_board(snapshot.sensor_type);
	return EST_OK;
}

est_result_t est_sensor_set_mode(est_sensor_port_t port,
	est_sensor_mode_t mode)
{
	est_sensor_type_t type;
	est_result_t result;

	if (!est_sensor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (!est_sensor_mode_valid(mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	result = est_sensor_get_type(port, &type);
	if (result != EST_OK) {
		return result;
	}
	if (type == EST_SENSOR_TYPE_NONE) {
		return EST_ERR_NOT_CONNECTED;
	}
	if (type == EST_SENSOR_TYPE_UNKNOWN) {
		return EST_ERR_TYPE_MISMATCH;
	}
	if (!est_sensor_mode_supported(type, mode)) {
		return EST_ERR_NOT_SUPPORTED;
	}
	if (!board_sensor_set_mode(board_port_from_est(port),
	    (enum board_sensor_mode)mode, est_system_millis())) {
		return EST_ERR_NOT_SUPPORTED;
	}
	return EST_OK;
}

est_result_t est_sensor_restart(est_sensor_port_t port)
{
	if (!est_sensor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (!board_sensor_restart(board_port_from_est(port),
	    est_system_millis())) {
		return EST_ERR_IO;
	}
	return EST_OK;
}

est_result_t est_sensor_get_status(est_sensor_port_t port,
	est_sensor_status_t *status)
{
	struct board_sensor_snapshot snapshot;

	if (!est_sensor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (status == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (!board_sensor_get_snapshot(board_port_from_est(port), &snapshot)) {
		return EST_ERR_IO;
	}
	memset(status, 0, sizeof(*status));
	status->port = port;
	status->type = est_type_from_board(snapshot.sensor_type);
	status->raw_type = snapshot.sensor_type;
	status->state = est_state_from_board(snapshot.state);
	status->mode = (est_sensor_mode_t)snapshot.mode;
	status->value_valid = snapshot.value_valid;
	status->raw_value = snapshot.value;
	status->value = value_from_board(&snapshot, status->type);
	status->value_format = value_format_for(status->type, status->mode,
		status->value_valid);
	status->adc0_raw = snapshot.adc0_raw;
	status->adc1_raw = snapshot.adc1_raw;
	status->digital_mask = snapshot.digital_mask;
	status->rx_count = snapshot.rx_count;
	status->data_generation = snapshot.data_generation;
	status->last_data_ms = snapshot.last_data_ms;
	status->mode_command_count = snapshot.mode_command_count;
	status->checksum_errors = snapshot.checksum_errors;
	status->requested_mode = (est_sensor_mode_t)snapshot.requested_mode;
	status->active_mode = (est_sensor_mode_t)snapshot.active_mode;
	status->mode_pending = snapshot.mode_pending;
	status->error = state_error(status->state);
	return EST_OK;
}
