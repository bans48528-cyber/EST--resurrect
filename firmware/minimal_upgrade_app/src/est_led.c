#include <stdint.h>

#include "board_led.h"
#include "est_led.h"

static est_led_color_t current_color;

void est_led_init(void)
{
	board_led_init();
	board_led_all_off();
	current_color = EST_LED_OFF;
}

est_result_t est_led_set(est_led_color_t color)
{
	if ((uint32_t)color > (uint32_t)EST_LED_RED_BLUE) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	board_led_checkpoint((uint8_t)color);
	current_color = color;
	return EST_OK;
}

est_led_color_t est_led_get(void)
{
	return current_color;
}
