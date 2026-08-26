#include <stdint.h>

#include "board_backlight.h"
#include "est_backlight.h"

void est_backlight_init(void)
{
	board_backlight_init();
}

void est_backlight_tick(uint32_t now_ms)
{
	board_backlight_tick(now_ms);
}

est_result_t est_backlight_set_percent(uint8_t percent)
{
	if (percent > 100U) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	board_backlight_set_percent(percent);
	return EST_OK;
}

uint8_t est_backlight_get_percent(void)
{
	return board_backlight_percent();
}
