#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_motor.h"
#include "est_drive.h"
#include "est_motor.h"
#include "system_time.h"

#define EST_DRIVE_PI_NUMERATOR 355LL
#define EST_DRIVE_PI_DENOMINATOR 113LL
#define EST_DRIVE_DEGREES_PER_ROTATION 360LL
#define EST_DRIVE_MAX_DURATION_MS 600000

enum est_drive_operation {
	EST_DRIVE_OPERATION_NONE = 0,
	EST_DRIVE_OPERATION_DISTANCE = 1,
	EST_DRIVE_OPERATION_DEGREES = 2,
	EST_DRIVE_OPERATION_TIME = 3,
	EST_DRIVE_OPERATION_STEER_DEGREES = 4,
	EST_DRIVE_OPERATION_STEER_TIME = 5
};

static est_drive_config_t drive_config;
static bool drive_configured;
static enum est_drive_operation drive_operation;
static int32_t drive_target_distance_mm;
static int32_t drive_target_value;
static uint8_t drive_requested_speed_percent;
static int8_t drive_steering;
static int8_t drive_signed_speed_percent;
static int8_t drive_left_requested_speed_percent;
static int8_t drive_right_requested_speed_percent;
static int32_t drive_left_target_degrees;
static int32_t drive_right_target_degrees;

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

static int32_t round_signed_ratio(int64_t numerator, int64_t denominator)
{
	if (numerator >= 0) {
		return (int32_t)((numerator + denominator / 2LL) / denominator);
	}
	return (int32_t)((numerator - denominator / 2LL) / denominator);
}

static int16_t clamp_percent(int16_t value)
{
	if (value > 100) {
		return 100;
	}
	if (value < -100) {
		return -100;
	}
	return value;
}

static bool closed_loop_speed_supported(int8_t speed_percent)
{
	int16_t magnitude = speed_percent < 0 ?
		-(int16_t)speed_percent : (int16_t)speed_percent;

	return magnitude >= 10 && magnitude <= 100;
}

static int32_t magnitude_i32(int32_t value)
{
	return value < 0 ? -value : value;
}

static int32_t scale_steering_degrees(int32_t target_degrees,
	int8_t wheel_speed_percent, int16_t maximum_speed_percent)
{
	int32_t degrees = round_signed_ratio(
		(int64_t)target_degrees * wheel_speed_percent,
		(int64_t)maximum_speed_percent);

	if (degrees == 0) {
		degrees = wheel_speed_percent < 0 ? -1 : 1;
	}
	return degrees;
}

static bool distance_to_wheel_degrees(int32_t distance_mm,
	uint16_t wheel_diameter_mm, int32_t *degrees)
{
	int64_t numerator = (int64_t)distance_mm *
		EST_DRIVE_DEGREES_PER_ROTATION * EST_DRIVE_PI_DENOMINATOR;
	int64_t denominator = (int64_t)wheel_diameter_mm *
		EST_DRIVE_PI_NUMERATOR;
	int32_t result = round_signed_ratio(numerator, denominator);

	if (result == 0 || result < -3600 || result > 3600) {
		return false;
	}
	*degrees = result;
	return true;
}

static int32_t wheel_degrees_to_distance(int32_t degrees,
	uint16_t wheel_diameter_mm)
{
	int64_t numerator = (int64_t)degrees * wheel_diameter_mm *
		EST_DRIVE_PI_NUMERATOR;
	int64_t denominator = EST_DRIVE_DEGREES_PER_ROTATION *
		EST_DRIVE_PI_DENOMINATOR;

	return round_signed_ratio(numerator, denominator);
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
	drive_operation = EST_DRIVE_OPERATION_NONE;
	drive_target_distance_mm = 0;
	return EST_OK;
}

est_result_t est_motor_pair_run_speeds(est_motor_port_t left_port,
	int8_t left_speed_percent, est_motor_port_t right_port,
	int8_t right_speed_percent)
{
	est_result_t result;
	int16_t left_magnitude;
	int16_t right_magnitude;

	if (!motor_port_valid(left_port) || !motor_port_valid(right_port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (left_port == right_port || left_speed_percent == 0 ||
	    right_speed_percent == 0 || left_speed_percent < -100 ||
	    left_speed_percent > 100 || right_speed_percent < -100 ||
	    right_speed_percent > 100) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	left_magnitude = left_speed_percent < 0 ?
		-(int16_t)left_speed_percent : left_speed_percent;
	right_magnitude = right_speed_percent < 0 ?
		-(int16_t)right_speed_percent : right_speed_percent;
	if (left_magnitude < 10 || right_magnitude < 10) {
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
	if (!board_motor_start_pair_speed(system_time_millis(),
	    (enum board_motor_port)left_port, left_speed_percent,
	    (enum board_motor_port)right_port, right_speed_percent)) {
		return EST_ERR_BUSY;
	}
	drive_operation = EST_DRIVE_OPERATION_NONE;
	drive_target_distance_mm = 0;
	return EST_OK;
}

est_result_t est_motor_pair_stop(est_stop_mode_t stop_mode)
{
	if (!stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (stop_mode == EST_STOP_HOLD) {
		return EST_ERR_NOT_SUPPORTED;
	}
	return board_motor_stop_pair_speed(board_stop_from_est(stop_mode)) ?
		EST_OK : EST_ERR_STATE;
}

est_result_t est_motor_pair_get_speed_status(
	est_motor_pair_speed_status_t *status)
{
	struct board_motor_pair_speed_snapshot snapshot;

	if (status == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	snapshot = board_motor_pair_speed_snapshot();
	memset(status, 0, sizeof(*status));
	status->state = snapshot.state == BOARD_MOTOR_PAIR_SPEED_RUNNING ?
		EST_DRIVE_RUNNING : snapshot.state == BOARD_MOTOR_PAIR_SPEED_COMPLETE ?
		EST_DRIVE_COMPLETE : EST_DRIVE_IDLE;
	status->left_port = (est_motor_port_t)snapshot.left_port;
	status->right_port = (est_motor_port_t)snapshot.right_port;
	status->left_requested_speed_percent =
		snapshot.left_requested_speed_percent;
	status->right_requested_speed_percent =
		snapshot.right_requested_speed_percent;
	status->left_measured_speed_percent =
		snapshot.left_measured_speed_percent;
	status->right_measured_speed_percent =
		snapshot.right_measured_speed_percent;
	status->left_power_percent = snapshot.left_power_percent;
	status->right_power_percent = snapshot.right_power_percent;
	status->left_actual_degrees = snapshot.left_current_count -
		snapshot.left_start_count;
	status->right_actual_degrees = snapshot.right_current_count -
		snapshot.right_start_count;
	status->synchronization_error_degrees =
		snapshot.synchronization_error_count;
	status->maximum_synchronization_error_degrees =
		snapshot.maximum_synchronization_error_count;
	status->error = EST_OK;
	return EST_OK;
}

est_result_t est_drive_run_degrees(est_motor_port_t left_port,
	est_motor_port_t right_port, int32_t degrees,
	uint8_t speed_percent, est_stop_mode_t stop_mode)
{
	est_result_t result = est_motor_pair_run_angles(left_port, degrees,
		right_port, degrees, speed_percent, stop_mode);

	if (result == EST_OK) {
		drive_operation = EST_DRIVE_OPERATION_DEGREES;
		drive_target_value = degrees;
		drive_requested_speed_percent = speed_percent;
	}
	return result;
}

est_result_t est_drive_run_time(est_motor_port_t left_port,
	est_motor_port_t right_port, int32_t duration_ms,
	uint8_t speed_percent, est_stop_mode_t stop_mode)
{
	est_result_t result;
	int8_t signed_speed;
	uint32_t duration_magnitude;

	if (!motor_port_valid(left_port) || !motor_port_valid(right_port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (left_port == right_port || duration_ms == 0 ||
	    duration_ms < -EST_DRIVE_MAX_DURATION_MS ||
	    duration_ms > EST_DRIVE_MAX_DURATION_MS ||
	    speed_percent < 10U || speed_percent > 100U ||
	    !stop_mode_valid(stop_mode) || stop_mode == EST_STOP_HOLD) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	result = require_tacho_motor(left_port);
	if (result != EST_OK) {
		return result;
	}
	result = require_tacho_motor(right_port);
	if (result != EST_OK) {
		return result;
	}
	signed_speed = duration_ms < 0 ? -(int8_t)speed_percent :
		(int8_t)speed_percent;
	duration_magnitude = duration_ms < 0 ?
		(uint32_t)(-duration_ms) : (uint32_t)duration_ms;
	if (!board_motor_start_pair_speed_for_time(system_time_millis(),
	    (enum board_motor_port)left_port, signed_speed,
	    (enum board_motor_port)right_port, signed_speed,
	    duration_magnitude, board_stop_from_est(stop_mode))) {
		return EST_ERR_BUSY;
	}
	drive_operation = EST_DRIVE_OPERATION_TIME;
	drive_target_value = duration_ms;
	drive_requested_speed_percent = speed_percent;
	return EST_OK;
}

est_result_t est_drive_get_motion_status(est_drive_motion_status_t *status)
{
	if (status == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	memset(status, 0, sizeof(*status));
	status->requested_speed_percent =
		(int8_t)drive_requested_speed_percent;
	status->target_value = drive_target_value;
	status->error = EST_OK;
	if (drive_operation == EST_DRIVE_OPERATION_DEGREES) {
		est_drive_status_t drive_status;

		(void)est_drive_get_status(&drive_status);
		status->state = drive_status.state;
		status->mode = EST_DRIVE_TARGET_DEGREES;
		status->left_port = drive_status.left_port;
		status->right_port = drive_status.right_port;
		status->left_actual_degrees =
			drive_status.left_actual_degrees;
		status->right_actual_degrees =
			drive_status.right_actual_degrees;
		status->actual_value = (drive_status.left_actual_degrees +
			drive_status.right_actual_degrees) / 2;
		status->synchronization_error_degrees =
			drive_status.synchronization_error_degrees;
		status->maximum_synchronization_error_degrees =
			drive_status.maximum_synchronization_error_degrees;
		status->error = drive_status.error;
	} else if (drive_operation == EST_DRIVE_OPERATION_TIME) {
		struct board_motor_pair_speed_snapshot snapshot =
			board_motor_pair_speed_snapshot();
		int32_t direction = drive_target_value < 0 ? -1 : 1;

		status->state = snapshot.state == BOARD_MOTOR_PAIR_SPEED_RUNNING ?
			EST_DRIVE_RUNNING :
			snapshot.state == BOARD_MOTOR_PAIR_SPEED_COMPLETE ?
			EST_DRIVE_COMPLETE : EST_DRIVE_IDLE;
		status->mode = EST_DRIVE_TARGET_TIME_MS;
		status->left_port = (est_motor_port_t)snapshot.left_port;
		status->right_port = (est_motor_port_t)snapshot.right_port;
		status->requested_speed_percent = direction *
			(int32_t)drive_requested_speed_percent;
		status->actual_value = direction * (int32_t)snapshot.elapsed_ms;
		status->left_actual_degrees = snapshot.left_current_count -
			snapshot.left_start_count;
		status->right_actual_degrees = snapshot.right_current_count -
			snapshot.right_start_count;
		status->synchronization_error_degrees =
			snapshot.synchronization_error_count;
		status->maximum_synchronization_error_degrees =
			snapshot.maximum_synchronization_error_count;
	} else {
		return EST_ERR_STATE;
	}
	return EST_OK;
}

est_result_t est_drive_mix_steering(int8_t steering,
	int8_t speed_percent, int8_t *left_speed_percent,
	int8_t *right_speed_percent)
{
	int16_t left_raw;
	int16_t right_raw;
	int8_t left_speed;
	int8_t right_speed;

	if (left_speed_percent == NULL || right_speed_percent == NULL ||
	    steering < -100 || steering > 100 || speed_percent == 0 ||
	    speed_percent < -100 || speed_percent > 100) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (steering == 100 || steering == -100) {
		left_raw = steering;
		right_raw = -steering;
	} else {
		left_raw = clamp_percent(100 + steering);
		right_raw = clamp_percent(100 - steering);
	}
	left_speed = (int8_t)round_signed_ratio(
		(int64_t)left_raw * speed_percent, 100LL);
	right_speed = (int8_t)round_signed_ratio(
		(int64_t)right_raw * speed_percent, 100LL);
	if (!closed_loop_speed_supported(left_speed) ||
	    !closed_loop_speed_supported(right_speed)) {
		return EST_ERR_NOT_SUPPORTED;
	}
	*left_speed_percent = left_speed;
	*right_speed_percent = right_speed;
	return EST_OK;
}

est_result_t est_drive_start_steer(est_motor_port_t left_port,
	est_motor_port_t right_port, int8_t steering, int8_t speed_percent)
{
	int8_t left_speed;
	int8_t right_speed;
	est_result_t result = est_drive_mix_steering(steering, speed_percent,
		&left_speed, &right_speed);

	if (result != EST_OK) {
		return result;
	}
	return est_motor_pair_run_speeds(left_port, left_speed,
		right_port, right_speed);
}

est_result_t est_drive_steer_for(est_motor_port_t left_port,
	est_motor_port_t right_port, est_drive_target_mode_t mode,
	int8_t steering, int8_t speed_percent, int32_t target_value,
	est_stop_mode_t stop_mode)
{
	est_result_t result;
	int8_t left_speed;
	int8_t right_speed;
	int16_t left_magnitude;
	int16_t right_magnitude;
	int16_t maximum_speed;
	int32_t left_degrees = 0;
	int32_t right_degrees = 0;

	if (!motor_port_valid(left_port) || !motor_port_valid(right_port)) {
		return EST_ERR_INVALID_PORT;
	}
	if (left_port == right_port ||
	    (mode != EST_DRIVE_TARGET_DEGREES &&
	     mode != EST_DRIVE_TARGET_TIME_MS) ||
	    target_value <= 0 || !stop_mode_valid(stop_mode) ||
	    stop_mode == EST_STOP_HOLD) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if ((mode == EST_DRIVE_TARGET_DEGREES && target_value > 3600) ||
	    (mode == EST_DRIVE_TARGET_TIME_MS &&
	     target_value > EST_DRIVE_MAX_DURATION_MS)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (mode == EST_DRIVE_TARGET_DEGREES && stop_mode != EST_STOP_COAST) {
		return EST_ERR_NOT_SUPPORTED;
	}
	result = est_drive_mix_steering(steering, speed_percent,
		&left_speed, &right_speed);
	if (result != EST_OK) {
		return result;
	}
	left_magnitude = left_speed < 0 ? -(int16_t)left_speed : left_speed;
	right_magnitude = right_speed < 0 ?
		-(int16_t)right_speed : right_speed;
	maximum_speed = left_magnitude > right_magnitude ?
		left_magnitude : right_magnitude;

	if (mode == EST_DRIVE_TARGET_DEGREES) {
		left_degrees = scale_steering_degrees(target_value, left_speed,
			maximum_speed);
		right_degrees = scale_steering_degrees(target_value, right_speed,
			maximum_speed);
		result = est_motor_pair_run_angles(left_port, left_degrees,
			right_port, right_degrees, (uint8_t)maximum_speed,
			stop_mode);
		if (result != EST_OK) {
			return result;
		}
		drive_operation = EST_DRIVE_OPERATION_STEER_DEGREES;
	} else {
		result = require_tacho_motor(left_port);
		if (result != EST_OK) {
			return result;
		}
		result = require_tacho_motor(right_port);
		if (result != EST_OK) {
			return result;
		}
		if (!board_motor_start_pair_speed_for_time(system_time_millis(),
		    (enum board_motor_port)left_port, left_speed,
		    (enum board_motor_port)right_port, right_speed,
		    (uint32_t)target_value, board_stop_from_est(stop_mode))) {
			return EST_ERR_BUSY;
		}
		drive_operation = EST_DRIVE_OPERATION_STEER_TIME;
	}
	drive_steering = steering;
	drive_signed_speed_percent = speed_percent;
	drive_left_requested_speed_percent = left_speed;
	drive_right_requested_speed_percent = right_speed;
	drive_target_value = target_value;
	drive_left_target_degrees = left_degrees;
	drive_right_target_degrees = right_degrees;
	return EST_OK;
}

est_result_t est_drive_get_steer_for_status(
	est_drive_steer_for_status_t *status)
{
	if (status == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	memset(status, 0, sizeof(*status));
	status->steering = drive_steering;
	status->requested_speed_percent = drive_signed_speed_percent;
	status->left_requested_speed_percent =
		drive_left_requested_speed_percent;
	status->right_requested_speed_percent =
		drive_right_requested_speed_percent;
	status->target_value = drive_target_value;
	status->left_target_degrees = drive_left_target_degrees;
	status->right_target_degrees = drive_right_target_degrees;
	status->error = EST_OK;

	if (drive_operation == EST_DRIVE_OPERATION_STEER_DEGREES) {
		est_drive_status_t drive_status;

		(void)est_drive_get_status(&drive_status);
		status->state = drive_status.state;
		status->mode = EST_DRIVE_TARGET_DEGREES;
		status->left_port = drive_status.left_port;
		status->right_port = drive_status.right_port;
		status->left_target_degrees = drive_status.left_target_degrees;
		status->right_target_degrees = drive_status.right_target_degrees;
		status->left_actual_degrees = drive_status.left_actual_degrees;
		status->right_actual_degrees = drive_status.right_actual_degrees;
		status->actual_value =
			magnitude_i32(status->left_target_degrees) >=
			magnitude_i32(status->right_target_degrees) ?
			magnitude_i32(status->left_actual_degrees) :
			magnitude_i32(status->right_actual_degrees);
		status->synchronization_error_degrees =
			drive_status.synchronization_error_degrees;
		status->maximum_synchronization_error_degrees =
			drive_status.maximum_synchronization_error_degrees;
		status->error = drive_status.error;
	} else if (drive_operation == EST_DRIVE_OPERATION_STEER_TIME) {
		struct board_motor_pair_speed_snapshot snapshot =
			board_motor_pair_speed_snapshot();

		status->state = snapshot.state == BOARD_MOTOR_PAIR_SPEED_RUNNING ?
			EST_DRIVE_RUNNING :
			snapshot.state == BOARD_MOTOR_PAIR_SPEED_COMPLETE ?
			EST_DRIVE_COMPLETE : EST_DRIVE_IDLE;
		status->mode = EST_DRIVE_TARGET_TIME_MS;
		status->left_port = (est_motor_port_t)snapshot.left_port;
		status->right_port = (est_motor_port_t)snapshot.right_port;
		status->actual_value = (int32_t)snapshot.elapsed_ms;
		status->left_actual_degrees = snapshot.left_current_count -
			snapshot.left_start_count;
		status->right_actual_degrees = snapshot.right_current_count -
			snapshot.right_start_count;
		status->synchronization_error_degrees =
			snapshot.synchronization_error_count;
		status->maximum_synchronization_error_degrees =
			snapshot.maximum_synchronization_error_count;
	} else {
		return EST_ERR_STATE;
	}
	return EST_OK;
}

est_result_t est_drive_straight(int32_t distance_mm,
	uint8_t speed_percent, est_stop_mode_t stop_mode)
{
	est_result_t result;
	int32_t wheel_degrees;

	if (!drive_configured || distance_mm == 0 || speed_percent < 10U ||
	    speed_percent > 100U || !stop_mode_valid(stop_mode)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (stop_mode != EST_STOP_COAST) {
		return EST_ERR_NOT_SUPPORTED;
	}
	if (!distance_to_wheel_degrees(distance_mm,
	    drive_config.wheel_diameter_mm, &wheel_degrees)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	result = est_motor_pair_run_angles(drive_config.left_port,
		wheel_degrees, drive_config.right_port, wheel_degrees,
		speed_percent, stop_mode);
	if (result == EST_OK) {
		drive_operation = EST_DRIVE_OPERATION_DISTANCE;
		drive_target_distance_mm = distance_mm;
	}
	return result;
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
	if (board_motor_stop_pair_position(board_stop_from_est(stop_mode)) ||
	    board_motor_stop_pair_speed(board_stop_from_est(stop_mode))) {
		return EST_OK;
	}
	return EST_ERR_STATE;
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
	if (drive_operation == EST_DRIVE_OPERATION_DISTANCE) {
		int32_t average_degrees = (status->left_actual_degrees +
			status->right_actual_degrees) / 2;

		status->target_distance_mm = drive_target_distance_mm;
		status->actual_distance_mm = wheel_degrees_to_distance(
			average_degrees, drive_config.wheel_diameter_mm);
	}
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
