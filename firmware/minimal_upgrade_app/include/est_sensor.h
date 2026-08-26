#ifndef EST_SENSOR_H
#define EST_SENSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "est_types.h"

typedef struct {
	est_sensor_port_t port;
	est_sensor_type_t type;
	est_sensor_state_t state;
	est_sensor_mode_t mode;
	est_value_format_t value_format;
	uint8_t raw_type;
	bool value_valid;
	int32_t value;
	uint16_t raw_value;
	uint16_t adc0_raw;
	uint16_t adc1_raw;
	uint8_t digital_mask;
	uint32_t rx_count;
	uint16_t checksum_errors;
	est_result_t error;
} est_sensor_status_t;

est_result_t est_sensor_get_type(est_sensor_port_t port,
	est_sensor_type_t *type);
est_result_t est_sensor_set_mode(est_sensor_port_t port,
	est_sensor_mode_t mode);
est_result_t est_sensor_restart(est_sensor_port_t port);
est_result_t est_sensor_get_status(est_sensor_port_t port,
	est_sensor_status_t *status);

#endif
