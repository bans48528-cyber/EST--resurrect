#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_motor.h"
#include "est_drive.h"
#include "est_motor.h"
#include "system_time.h"

static est_drive_config_t drive_config;
static bool drive_configured;

static bool motor_port_valid(est_motor_port_t port)
{
	return (uint32_t)port < (uint32_t)EST_MOTOR_PORT_COUNT;
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

static est_result_t require_tacho_motor(est_motor_port_t port)
{
	est_motor_type_t type;
	est_result_t result = est_motor_get_type(port, &type);

	if (result != EST_OK) {
		return result;
	}
	if (type == EST_MOTOR_TYPE_NONE) {
		return EST_ERR_NOT_CONNECTED;
	}
	if (type != EST_MOTOR_TYPE_LARGE && type != EST_MOTOR_TYPE_MEDIUM) {
		return EST_ERR_TYPE_MISMATCH;
	}
	return EST_OK;
}

est_result_t est_drive_config(const est_drive_config_t *config)
{
	if (config == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (!motor_port_valid(config->left_port) ||
	    !motor_port_valid(config->right_port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (config->left_port == config->right_port ||
	    config->wheel_diameter_mm == 0U || config->axle_track_mm == 0U) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	drive_config = *config;
	drive_configured = true;
	return EST_OK;
}

est_result_t est_motor_pair_run_angles(est_motor_port_t left_port,
	int32_t left_degrees, est_motor_port_t right_port,
	int32_t right_degrees, uint8_t maximum_speed_percent,
	est_stop_mode_t stop_mode)
{
	est_result_t result;
	int32_t left_magnitude;
	int32_t right_magnitude;

	if (!motor_port_valid(left_port) || !motor_port_valid(right_port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (left_port == right_port || left_degrees == 0 || right_degrees == 0 ||
	    left_degrees < -3600 || left_degrees > 3600 ||
	    right_degrees < -3600 || right_degrees > 3600 ||
	    maximum_speed_percent < 10U || maximum_speed_percent > 100U ||
	    !stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (stop_mode != EST_STOP_COAST) {
		return EST_ERR_NOT_SUPPORTED;
	}
	left_magnitude = left_degrees < 0 ? -left_degrees : left_degrees;
	right_magnitude = right_degrees < 0 ? -right_degrees : right_degrees;
	if (left_magnitude != right_magnitude) {
		return EST_ERR_NOT_SUPPORTED;
	}
	result = require_tacho_motor(left_port);
	if (result != EST_OK) {
		return result;
	}
	result = require_tacho_motor(right_port);
	if (result != EST_OK) {
		return result;
	}
	if (!board_motor_start_pair_position(system_time_millis(),
	    (enum board_motor_port)left_port, left_degrees,
	    (enum board_motor_port)right_port, right_degrees,
	    maximum_speed_percent)) {
		return EST_ERR_BUSY;
	}
	return EST_OK;
}

est_result_t est_drive_straight(int32_t distance_mm,
	uint8_t speed_percent, est_stop_mode_t stop_mode)
{
	if (!drive_configured || distance_mm == 0 || speed_percent < 10U ||
	    speed_percent > 100U || !stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	return EST_ERR_NOT_SUPPORTED;
}

est_result_t est_drive_turn(int32_t angle_degrees,
	uint8_t speed_percent, est_stop_mode_t stop_mode)
{
	if (!drive_configured || angle_degrees == 0 || speed_percent < 10U ||
	    speed_percent > 100U || !stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	return EST_ERR_NOT_SUPPORTED;
}

est_result_t est_drive_arc(int32_t radius_mm, int32_t angle_degrees,
	uint8_t speed_percent, est_stop_mode_t stop_mode)
{
	if (!drive_configured || radius_mm == 0 || angle_degrees == 0 ||
	    speed_percent < 10U || speed_percent > 100U ||
	    !stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	return EST_ERR_NOT_SUPPORTED;
}

est_result_t est_drive_stop(est_stop_mode_t stop_mode)
{
	if (!stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (stop_mode == EST_STOP_HOLD) {
		return EST_ERR_NOT_SUPPORTED;
	}
	return board_motor_stop_pair_position(board_stop_from_est(stop_mode)) ?
		EST_OK : EST_ERR_STATE;
}

est_result_t est_drive_get_status(est_drive_status_t *status)
{
	struct board_motor_pair_position_snapshot snapshot;

	if (status == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	snapshot = board_motor_pair_position_snapshot();
	memset(status, 0, sizeof(*status));
	status->left_port = (est_motor_port_t)snapshot.left_port;
	status->right_port = (est_motor_port_t)snapshot.right_port;
	status->left_target_degrees = snapshot.left_target_count -
		snapshot.left_start_count;
	status->right_target_degrees = snapshot.right_target_count -
		snapshot.right_start_count;
	status->left_actual_degrees = snapshot.left_current_count -
		snapshot.left_start_count;
	status->right_actual_degrees = snapshot.right_current_count -
		snapshot.right_start_count;
	status->synchronization_error_degrees =
		snapshot.synchronization_error_count;
	status->maximum_synchronization_error_degrees =
		snapshot.maximum_synchronization_error_count;
	status->error = EST_OK;
	switch (snapshot.state) {
	case BOARD_MOTOR_PAIR_POSITION_RUNNING:
		status->state = EST_DRIVE_RUNNING;
		break;
	case BOARD_MOTOR_PAIR_POSITION_COMPLETE:
		status->state = EST_DRIVE_COMPLETE;
		break;
	case BOARD_MOTOR_PAIR_POSITION_TIMEOUT:
		status->state = EST_DRIVE_FAULT;
		status->error = EST_ERR_TIMEOUT;
		break;
	case BOARD_MOTOR_PAIR_POSITION_IDLE:
	default:
		status->state = EST_DRIVE_IDLE;
		break;
	}
	return EST_OK;
}
