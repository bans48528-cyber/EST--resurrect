#ifndef EST_SENSOR_WAIT_H
#define EST_SENSOR_WAIT_H

#include <stdbool.h>
#include <stdint.h>

#include "est_sensor.h"

typedef enum {
	EST_SENSOR_WAIT_PENDING = 0,
	EST_SENSOR_WAIT_READY = 1,
	EST_SENSOR_WAIT_DISCONNECTED = 2,
	EST_SENSOR_WAIT_TYPE_MISMATCH = 3
} est_sensor_wait_decision_t;

est_sensor_wait_decision_t est_sensor_wait_decide(
	const est_sensor_status_t *status, est_sensor_type_t expected_type,
	est_sensor_mode_t requested_mode, uint32_t request_generation,
	bool require_new_generation);
est_result_t est_sensor_wait_timeout_error(
	const est_sensor_status_t *status, est_sensor_type_t expected_type);

#endif
