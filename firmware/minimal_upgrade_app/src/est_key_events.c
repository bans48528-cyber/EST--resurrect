#include <stdint.h>

#include "est_buttons.h"
#include "est_key_events.h"

static uint8_t short_events;
static uint8_t long_events;
static uint8_t suppress_short_mask;

static void drain_source_events(void)
{
	(void)est_buttons_take_pressed_events();
	(void)est_buttons_take_released_events();
	(void)est_buttons_take_long_press_events();
}

void est_key_events_init(void)
{
	short_events = 0U;
	long_events = 0U;
	suppress_short_mask = est_buttons_pressed_mask();
	drain_source_events();
}

void est_key_events_tick(void)
{
	uint8_t pressed = est_buttons_take_pressed_events();
	uint8_t released = est_buttons_take_released_events();
	uint8_t held = est_buttons_pressed_mask();
	uint8_t long_pressed = est_buttons_take_long_press_events();
	uint8_t back_bit = (uint8_t)(1U << (uint8_t)EST_BUTTON_BACK);
	uint8_t immediate = pressed & (uint8_t)~back_bit;

	/* Navigation feels immediate on the debounced press. Back remains a
	 * release action so its long-press emergency-stop gesture stays distinct. */
	short_events |= immediate;
	suppress_short_mask |= immediate;
	long_events |= long_pressed;
	suppress_short_mask |= long_pressed;
	short_events |= released & (uint8_t)~suppress_short_mask;
	suppress_short_mask &= (uint8_t)~released;
	suppress_short_mask &= held | released;
}

void est_key_events_reset(void)
{
	short_events = 0U;
	long_events = 0U;
	suppress_short_mask = est_buttons_pressed_mask();
	drain_source_events();
}

uint8_t est_key_events_take_short(void)
{
	uint8_t events = short_events;

	short_events = 0U;
	return events;
}

uint8_t est_key_events_take_long(void)
{
	uint8_t events = long_events;

	long_events = 0U;
	return events;
}
