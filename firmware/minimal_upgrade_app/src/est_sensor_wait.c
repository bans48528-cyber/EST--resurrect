#include <stddef.h>

#include "est_sensor_wait.h"

est_sensor_wait_decision_t est_sensor_wait_decide(
	const est_sensor_status_t *status, est_sensor_type_t expected_type,
	est_sensor_mode_t requested_mode, uint32_t request_generation,
	bool require_new_generation)
{
	if (status == NULL) {
		return EST_SENSOR_WAIT_DISCONNECTED;
	}
	if (status->type == expected_type) {
		if (status->state == EST_SENSOR_STREAMING &&
		    status->active_mode == requested_mode &&
		    !status->mode_pending && status->value_valid &&
		    status->error == EST_OK &&
		    (!require_new_generation ||
		     status->data_generation != request_generation)) {
			return EST_SENSOR_WAIT_READY;
		}
		return EST_SENSOR_WAIT_PENDING;
	}
	if (status->state == EST_SENSOR_SYNCING ||
	    status->state == EST_SENSOR_STALE) {
		return EST_SENSOR_WAIT_PENDING;
	}
	return status->type == EST_SENSOR_TYPE_NONE ?
		EST_SENSOR_WAIT_DISCONNECTED : EST_SENSOR_WAIT_TYPE_MISMATCH;
}

est_result_t est_sensor_wait_timeout_error(
	const est_sensor_status_t *status, est_sensor_type_t expected_type)
{
	if (status == NULL || status->type == EST_SENSOR_TYPE_NONE) {
		return EST_ERR_NOT_CONNECTED;
	}
	if (status->type != expected_type) {
		return EST_ERR_TYPE_MISMATCH;
	}
	return EST_ERR_TIMEOUT;
}
