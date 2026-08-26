#ifndef EST_MOTOR_H
#define EST_MOTOR_H

#include <stdint.h>

#include "est_types.h"

typedef enum {
	EST_MOTOR_IDLE = 0,
	EST_MOTOR_POWER = 1,
	EST_MOTOR_SPEED = 2,
	EST_MOTOR_POSITION = 3,
	EST_MOTOR_TIMED = 4,
	EST_MOTOR_HOLDING = 5,
	EST_MOTOR_FAULT = 6
} est_motor_state_t;

typedef struct {
	est_motor_port_t port;
	est_motor_type_t type;
	est_motor_state_t state;
	est_stop_mode_t stop_mode;
	int8_t power_percent;
	int8_t target_speed_percent;
	int8_t actual_speed_percent;
	int32_t angle_degrees;
	est_result_t error;
} est_motor_status_t;

est_result_t est_motor_get_type(est_motor_port_t port,
	est_motor_type_t *type);
est_result_t est_motor_set_power(est_motor_port_t port,
	int8_t power_percent);
est_result_t est_motor_run_speed(est_motor_port_t port,
	int8_t speed_percent);
est_result_t est_motor_run_time(est_motor_port_t port,
	int8_t speed_percent, uint32_t duration_ms, est_stop_mode_t stop_mode);
est_result_t est_motor_run_angle(est_motor_port_t port, int32_t degrees,
	uint8_t maximum_speed_percent, est_stop_mode_t stop_mode);
est_result_t est_motor_stop(est_motor_port_t port,
	est_stop_mode_t stop_mode);
est_result_t est_motor_stop_all(est_stop_mode_t stop_mode);
est_result_t est_motor_reset_angle(est_motor_port_t port);
est_result_t est_motor_get_status(est_motor_port_t port,
	est_motor_status_t *status);

#endif
