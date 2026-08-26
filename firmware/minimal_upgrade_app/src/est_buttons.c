#include <stdbool.h>
#include <stdint.h>

#include "board_keys.h"
#include "est_buttons.h"

static uint8_t current_mask;
static uint8_t pressed_events;
static uint8_t released_events;
static uint8_t long_press_events;
static uint8_t long_press_reported;
static uint32_t pressed_since_ms[EST_BUTTON_COUNT];

static bool button_valid(est_button_t button)
{
	return (uint32_t)button < (uint32_t)EST_BUTTON_COUNT;
}

void est_buttons_init(uint32_t now_ms)
{
	uint8_t index;

	board_keys_init(now_ms);
	current_mask = board_keys_pressed_mask() & EST_BUTTON_MASK_ALL;
	pressed_events = 0U;
	released_events = 0U;
	long_press_events = 0U;
	long_press_reported = 0U;
	for (index = 0U; index < EST_BUTTON_COUNT; index++) {
		pressed_since_ms[index] = now_ms;
	}
}

void est_buttons_tick(uint32_t now_ms)
{
	uint8_t next_mask;
	uint8_t newly_pressed;
	uint8_t newly_released;
	uint8_t index;

	board_keys_tick(now_ms);
	next_mask = board_keys_pressed_mask() & EST_BUTTON_MASK_ALL;
	newly_pressed = next_mask & (uint8_t)~current_mask;
	newly_released = current_mask & (uint8_t)~next_mask;
	pressed_events |= newly_pressed;
	released_events |= newly_released;
	long_press_reported &= (uint8_t)~newly_released;
	for (index = 0U; index < EST_BUTTON_COUNT; index++) {
		uint8_t bit = (uint8_t)(1U << index);

		if ((newly_pressed & bit) != 0U) {
			pressed_since_ms[index] = now_ms;
		}
		if ((next_mask & bit) != 0U &&
		    (long_press_reported & bit) == 0U &&
		    (uint32_t)(now_ms - pressed_since_ms[index]) >=
			EST_BUTTON_LONG_PRESS_MS) {
			long_press_events |= bit;
			long_press_reported |= bit;
		}
	}
	current_mask = next_mask;
}

uint8_t est_buttons_pressed_mask(void)
{
	return current_mask;
}

bool est_button_is_pressed(est_button_t button)
{
	if (!button_valid(button)) {
		return false;
	}
	return (current_mask & (uint8_t)(1U << (uint8_t)button)) != 0U;
}

uint8_t est_buttons_take_pressed_events(void)
{
	uint8_t events = pressed_events;

	pressed_events = 0U;
	return events;
}

uint8_t est_buttons_take_released_events(void)
{
	uint8_t events = released_events;

	released_events = 0U;
	return events;
}

uint8_t est_buttons_take_long_press_events(void)
{
	uint8_t events = long_press_events;

	long_press_events = 0U;
	return events;
}
