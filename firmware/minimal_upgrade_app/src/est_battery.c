#include <stddef.h>
#include <stdint.h>

#include "board_battery.h"
#include "est_battery.h"

_Static_assert(EST_BATTERY_LEVEL_MAX == BOARD_BATTERY_LEVEL_MAX,
	"public and board battery levels must match");

void est_battery_init(uint32_t now_ms)
{
	board_battery_init(now_ms);
}

void est_battery_tick(uint32_t now_ms)
{
	board_battery_tick(now_ms);
}

est_result_t est_battery_get_status(est_battery_status_t *status)
{
	struct board_battery_snapshot snapshot;

	if (status == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	snapshot = board_battery_snapshot();
	status->valid = snapshot.valid;
	status->level = snapshot.level;
	status->percent = (uint8_t)(((uint32_t)snapshot.level * 100U) /
		EST_BATTERY_LEVEL_MAX);
	status->adc_raw = snapshot.adc_raw;
	status->sample_mv = snapshot.sample_mv;
	status->low = snapshot.valid &&
		snapshot.level <= EST_BATTERY_LOW_LEVEL_MAX;
	return snapshot.valid ? EST_OK : EST_ERR_STATE;
}
