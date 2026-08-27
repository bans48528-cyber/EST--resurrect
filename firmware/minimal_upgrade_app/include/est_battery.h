#ifndef EST_BATTERY_H
#define EST_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#include "est_types.h"

#define EST_BATTERY_LEVEL_MAX 4U
#define EST_BATTERY_LOW_LEVEL_MAX 1U

typedef struct {
	bool valid;
	bool low;
	uint8_t level;
	uint8_t percent;
	uint16_t adc_raw;
	/* Divider sample point, not the battery-pack terminal voltage. */
	uint16_t sample_mv;
} est_battery_status_t;

void est_battery_init(uint32_t now_ms);
void est_battery_tick(uint32_t now_ms);
est_result_t est_battery_get_status(est_battery_status_t *status);

#endif
