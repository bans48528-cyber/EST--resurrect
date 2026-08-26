#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_motor.h"
#include "est_motor.h"
#include "system_time.h"

static bool est_motor_port_valid(est_motor_port_t port)
{
	return (uint32_t)port < (uint32_t)EST_MOTOR_PORT_COUNT;
}

static enum board_motor_port board_port_from_est(est_motor_port_t port)
{
	return (enum board_motor_port)port;
}

static est_motor_type_t est_type_from_board(enum board_motor_type type)
{
	switch (type) {
	case BOARD_MOTOR_TYPE_NONE:
		return EST_MOTOR_TYPE_NONE;
	case BOARD_MOTOR_TYPE_LARGE:
		return EST_MOTOR_TYPE_LARGE;
	case BOARD_MOTOR_TYPE_MEDIUM:
		return EST_MOTOR_TYPE_MEDIUM;
	case BOARD_MOTOR_TYPE_UNKNOWN:
	default:
		return EST_MOTOR_TYPE_UNKNOWN;
	}
}

static est_result_t require_tacho_motor(est_motor_port_t port)
{
	struct board_motor_control_snapshot snapshot;

	if (!board_motor_control_snapshot(board_port_from_est(port), &snapshot)) {
		return EST_ERR_IO;
	}
	if (snapshot.type == BOARD_MOTOR_TYPE_NONE) {
		return EST_ERR_NOT_CONNECTED;
	}
	if (snapshot.type != BOARD_MOTOR_TYPE_LARGE &&
	    snapshot.type != BOARD_MOTOR_TYPE_MEDIUM) {
		return EST_ERR_TYPE_MISMATCH;
	}
	return EST_OK;
}

static bool stop_mode_valid(est_stop_mode_t stop_mode)
{
	return stop_mode == EST_STOP_COAST || stop_mode == EST_STOP_BRAKE ||
		stop_mode == EST_STOP_HOLD;
}

static enum board_motor_stop_mode board_stop_from_est(
	est_stop_mode_t stop_mode)
{
	return stop_mode == EST_STOP_BRAKE ?
		BOARD_MOTOR_STOP_HIGH_PUSH_PULL :
		BOARD_MOTOR_STOP_LOW_OPEN_DRAIN;
}

est_result_t est_motor_get_type(est_motor_port_t port,
	est_motor_type_t *type)
{
	struct board_motor_control_snapshot snapshot;

	if (!est_motor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (type == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (!board_motor_control_snapshot(board_port_from_est(port), &snapshot)) {
		return EST_ERR_IO;
	}
	*type = est_type_from_board(snapshot.type);
	return EST_OK;
}

est_result_t est_motor_set_power(est_motor_port_t port,
	int8_t power_percent)
{
	if (!est_motor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (power_percent < -100 || power_percent > 100) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (!board_motor_set_power(board_port_from_est(port), power_percent)) {
		return EST_ERR_BUSY;
	}
	return EST_OK;
}

est_result_t est_motor_run_speed(est_motor_port_t port,
	int8_t speed_percent)
{
	est_result_t result;
	int16_t magnitude = speed_percent;

	if (!est_motor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (magnitude < 0) {
		magnitude = -magnitude;
	}
	if (magnitude < 10 || magnitude > 100) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	result = require_tacho_motor(port);
	if (result != EST_OK) {
		return result;
	}
	if (!board_motor_start_speed(system_time_millis(),
	    board_port_from_est(port), speed_percent)) {
		return EST_ERR_BUSY;
	}
	return EST_OK;
}

est_result_t est_motor_run_time(est_motor_port_t port,
	int8_t speed_percent, uint32_t duration_ms, est_stop_mode_t stop_mode)
{
	if (!est_motor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (speed_percent < -100 || speed_percent > 100 || speed_percent == 0 ||
	    duration_ms == 0U || !stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	return EST_ERR_NOT_SUPPORTED;
}

est_result_t est_motor_run_angle(est_motor_port_t port, int32_t degrees,
	uint8_t maximum_speed_percent, est_stop_mode_t stop_mode)
{
	est_result_t result;

	if (!est_motor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (degrees == 0 || degrees < -3600 || degrees > 3600 ||
	    maximum_speed_percent < 10U || maximum_speed_percent > 100U ||
	    !stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (stop_mode != EST_STOP_COAST) {
		return EST_ERR_NOT_SUPPORTED;
	}
	result = require_tacho_motor(port);
	if (result != EST_OK) {
		return result;
	}
	if (!board_motor_start_position(system_time_millis(),
	    board_port_from_est(port), maximum_speed_percent, degrees)) {
		return EST_ERR_BUSY;
	}
	return EST_OK;
}

est_result_t est_motor_stop(est_motor_port_t port,
	est_stop_mode_t stop_mode)
{
	enum board_motor_port board_port;
	struct board_motor_speed_snapshot speed;
	struct board_motor_position_snapshot position;

	if (!est_motor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (!stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (stop_mode == EST_STOP_HOLD) {
		return EST_ERR_NOT_SUPPORTED;
	}
	board_port = board_port_from_est(port);
	(void)board_motor_speed_snapshot_for_port(board_port, &speed);
	if (speed.state == BOARD_MOTOR_SPEED_RUNNING && speed.port == board_port) {
		return board_motor_stop_speed(board_port,
			board_stop_from_est(stop_mode)) ? EST_OK : EST_ERR_BUSY;
	}
	(void)board_motor_position_snapshot_for_port(board_port, &position);
	if (position.state == BOARD_MOTOR_POSITION_RUNNING &&
	    position.port == board_port) {
		return board_motor_stop_position(board_port,
			board_stop_from_est(stop_mode)) ? EST_OK : EST_ERR_BUSY;
	}
	if (stop_mode == EST_STOP_BRAKE) {
		return board_motor_brake(board_port) ? EST_OK : EST_ERR_STATE;
	}
	return board_motor_coast(board_port) ? EST_OK : EST_ERR_STATE;
}

est_result_t est_motor_stop_all(est_stop_mode_t stop_mode)
{
	est_motor_port_t port;

	if (!stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (stop_mode == EST_STOP_HOLD) {
		return EST_ERR_NOT_SUPPORTED;
	}
	board_motor_stop();
	if (stop_mode == EST_STOP_COAST) {
		return EST_OK;
	}
	for (port = EST_MOTOR_PORT_A; port < EST_MOTOR_PORT_COUNT; port++) {
		if (!board_motor_brake(board_port_from_est(port))) {
			board_motor_stop();
			return EST_ERR_STATE;
		}
	}
	return EST_OK;
}

est_result_t est_motor_reset_angle(est_motor_port_t port)
{
	if (!est_motor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (!board_motor_reset_tacho(board_port_from_est(port))) {
		return EST_ERR_BUSY;
	}
	return EST_OK;
}

est_result_t est_motor_get_status(est_motor_port_t port,
	est_motor_status_t *status)
{
	enum board_motor_port board_port;
	struct board_motor_control_snapshot control;
	struct board_motor_position_snapshot position;
	struct board_motor_speed_snapshot speed;

	if (!est_motor_port_valid(port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (status == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	board_port = board_port_from_est(port);
	if (!board_motor_control_snapshot(board_port, &control)) {
		return EST_ERR_IO;
	}
	memset(status, 0, sizeof(*status));
	status->port = port;
	status->type = est_type_from_board(control.type);
	status->power_percent = control.power_percent;
	status->actual_speed_percent = control.speed_percent;
	status->angle_degrees = control.tacho_count;
	status->stop_mode = control.state == BOARD_MOTOR_OUTPUT_BRAKE ?
		EST_STOP_BRAKE : EST_STOP_COAST;
	status->state = control.state == BOARD_MOTOR_OUTPUT_DRIVE ?
		EST_MOTOR_POWER : EST_MOTOR_IDLE;
	status->error = EST_OK;

	(void)board_motor_speed_snapshot_for_port(board_port, &speed);
	if (speed.state == BOARD_MOTOR_SPEED_RUNNING && speed.port == board_port) {
		status->state = EST_MOTOR_SPEED;
		status->target_speed_percent = speed.requested_speed_percent;
	}
	(void)board_motor_position_snapshot_for_port(board_port, &position);
	if (position.port == board_port) {
		if (position.state == BOARD_MOTOR_POSITION_RUNNING) {
			status->state = EST_MOTOR_POSITION;
			status->target_speed_percent =
				position.requested_speed_percent;
		} else if (position.state == BOARD_MOTOR_POSITION_TIMEOUT &&
		    status->state == EST_MOTOR_IDLE) {
			status->state = EST_MOTOR_FAULT;
			status->error = EST_ERR_TIMEOUT;
		}
	}
	return EST_OK;
}
