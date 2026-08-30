#include <stddef.h>
#include <stdint.h>

#include "board_battery.h"
#include "est_battery.h"

_Static_assert(EST_BATTERY_LEVEL_MAX == BOARD_BATTERY_LEVEL_MAX,
	"public and board battery levels must match");

#define BATTERY_DIVIDER_MULTIPLIER 4U

struct battery_soc_point {
	uint16_t pack_mv;
	uint8_t percent;
};

/*
 * Voltage-only state-of-charge estimate for the two-cell NCR18650B pack.
 * The points follow a typical Li-ion open-circuit curve; the board's eight
 * sample moving average limits display jitter while the motors are loaded.
 */
static const struct battery_soc_point battery_soc_curve[] = {
	{6000U, 0U},
	{6540U, 5U},
	{7220U, 10U},
	{7380U, 15U},
	{7420U, 20U},
	{7460U, 25U},
	{7500U, 30U},
	{7540U, 35U},
	{7580U, 40U},
	{7600U, 45U},
	{7640U, 50U},
	{7700U, 55U},
	{7740U, 60U},
	{7820U, 65U},
	{7900U, 70U},
	{7960U, 75U},
	{8040U, 80U},
	{8160U, 85U},
	{8220U, 90U},
	{8300U, 95U},
	{8400U, 100U},
};

static uint8_t percent_from_sample_mv(uint16_t sample_mv)
{
	uint32_t pack_mv = (uint32_t)sample_mv * BATTERY_DIVIDER_MULTIPLIER;
	size_t index;

	if (pack_mv <= battery_soc_curve[0].pack_mv) {
		return battery_soc_curve[0].percent;
	}
	for (index = 1U;
	     index < sizeof(battery_soc_curve) / sizeof(battery_soc_curve[0]);
	     index++) {
		const struct battery_soc_point *lower = &battery_soc_curve[index - 1U];
		const struct battery_soc_point *upper = &battery_soc_curve[index];
		uint32_t span_mv;
		uint32_t span_percent;
		uint32_t offset_mv;

		if (pack_mv > upper->pack_mv) {
			continue;
		}
		span_mv = (uint32_t)upper->pack_mv - lower->pack_mv;
		span_percent = (uint32_t)upper->percent - lower->percent;
		offset_mv = pack_mv - lower->pack_mv;
		return (uint8_t)((uint32_t)lower->percent +
			(offset_mv * span_percent + span_mv / 2U) / span_mv);
	}
	return 100U;
}

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
	status->percent = percent_from_sample_mv(snapshot.sample_mv);
	status->adc_raw = snapshot.adc_raw;
	status->sample_mv = snapshot.sample_mv;
	status->low = snapshot.valid &&
		snapshot.level <= EST_BATTERY_LOW_LEVEL_MAX;
	return snapshot.valid ? EST_OK : EST_ERR_STATE;
}
