#ifndef EST_DRIVE_H
#define EST_DRIVE_H

#include <stdint.h>

#include "est_types.h"

typedef struct {
	est_motor_port_t left_port;
	est_motor_port_t right_port;
	uint16_t wheel_diameter_mm;
	uint16_t axle_track_mm;
} est_drive_config_t;

typedef enum {
	EST_DRIVE_IDLE = 0,
	EST_DRIVE_RUNNING = 1,
	EST_DRIVE_COMPLETE = 2,
	EST_DRIVE_FAULT = 3
} est_drive_state_t;

typedef struct {
	est_drive_state_t state;
	est_motor_port_t left_port;
	est_motor_port_t right_port;
	int32_t left_target_degrees;
	int32_t right_target_degrees;
	int32_t left_actual_degrees;
	int32_t right_actual_degrees;
	int32_t synchronization_error_degrees;
	int32_t maximum_synchronization_error_degrees;
	int32_t target_distance_mm;
	int32_t actual_distance_mm;
	est_result_t error;
} est_drive_status_t;

typedef struct {
	est_drive_state_t state;
	est_motor_port_t left_port;
	est_motor_port_t right_port;
	int8_t left_requested_speed_percent;
	int8_t right_requested_speed_percent;
	int8_t left_measured_speed_percent;
	int8_t right_measured_speed_percent;
	int8_t left_power_percent;
	int8_t right_power_percent;
	int32_t left_actual_degrees;
	int32_t right_actual_degrees;
	int32_t synchronization_error_degrees;
	int32_t maximum_synchronization_error_degrees;
	est_result_t error;
} est_motor_pair_speed_status_t;

typedef enum {
	EST_DRIVE_TARGET_DEGREES = 0,
	EST_DRIVE_TARGET_TIME_MS = 1
} est_drive_target_mode_t;

typedef struct {
	est_drive_state_t state;
	est_drive_target_mode_t mode;
	est_motor_port_t left_port;
	est_motor_port_t right_port;
	int8_t requested_speed_percent;
	int32_t target_value;
	int32_t actual_value;
	int32_t left_actual_degrees;
	int32_t right_actual_degrees;
	int32_t synchronization_error_degrees;
	int32_t maximum_synchronization_error_degrees;
	est_result_t error;
} est_drive_motion_status_t;

est_result_t est_drive_config(const est_drive_config_t *config);
est_result_t est_motor_pair_run_angles(est_motor_port_t left_port,
	int32_t left_degrees, est_motor_port_t right_port,
	int32_t right_degrees, uint8_t maximum_speed_percent,
	est_stop_mode_t stop_mode);
est_result_t est_motor_pair_run_speeds(est_motor_port_t left_port,
	int8_t left_speed_percent, est_motor_port_t right_port,
	int8_t right_speed_percent);
est_result_t est_motor_pair_stop(est_stop_mode_t stop_mode);
est_result_t est_motor_pair_get_speed_status(
	est_motor_pair_speed_status_t *status);
est_result_t est_drive_run_degrees(est_motor_port_t left_port,
	est_motor_port_t right_port, int32_t degrees,
	uint8_t speed_percent, est_stop_mode_t stop_mode);
est_result_t est_drive_run_time(est_motor_port_t left_port,
	est_motor_port_t right_port, int32_t duration_ms,
	uint8_t speed_percent, est_stop_mode_t stop_mode);
est_result_t est_drive_get_motion_status(est_drive_motion_status_t *status);
est_result_t est_drive_mix_steering(int8_t steering,
	int8_t speed_percent, int8_t *left_speed_percent,
	int8_t *right_speed_percent);
est_result_t est_drive_start_steer(est_motor_port_t left_port,
	est_motor_port_t right_port, int8_t steering, int8_t speed_percent);
est_result_t est_drive_straight(int32_t distance_mm,
	uint8_t speed_percent, est_stop_mode_t stop_mode);
est_result_t est_drive_turn(int32_t angle_degrees,
	uint8_t speed_percent, est_stop_mode_t stop_mode);
est_result_t est_drive_arc(int32_t radius_mm, int32_t angle_degrees,
	uint8_t speed_percent, est_stop_mode_t stop_mode);
est_result_t est_drive_stop(est_stop_mode_t stop_mode);
est_result_t est_drive_get_status(est_drive_status_t *status);

#endif
