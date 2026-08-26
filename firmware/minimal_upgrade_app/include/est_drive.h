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
	int32_t left_target_degrees;
	int32_t right_target_degrees;
	int32_t left_actual_degrees;
	int32_t right_actual_degrees;
	int32_t synchronization_error_degrees;
	est_result_t error;
} est_drive_status_t;

est_result_t est_drive_config(const est_drive_config_t *config);
est_result_t est_motor_pair_run_angles(est_motor_port_t left_port,
	int32_t left_degrees, est_motor_port_t right_port,
	int32_t right_degrees, uint8_t maximum_speed_percent,
	est_stop_mode_t stop_mode);
est_result_t est_drive_straight(int32_t distance_mm,
	uint8_t speed_percent, est_stop_mode_t stop_mode);
est_result_t est_drive_turn(int32_t angle_degrees,
	uint8_t speed_percent, est_stop_mode_t stop_mode);
est_result_t est_drive_arc(int32_t radius_mm, int32_t angle_degrees,
	uint8_t speed_percent, est_stop_mode_t stop_mode);
est_result_t est_drive_stop(est_stop_mode_t stop_mode);
est_result_t est_drive_get_status(est_drive_status_t *status);

#endif
