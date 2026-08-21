#include <stdint.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "board_keys.h"

#define KEY_DEBOUNCE_MS 25U

static uint8_t stable_mask;
static uint8_t candidate_mask;
static uint32_t candidate_since_ms;

static uint8_t read_raw_pressed_mask(void)
{
	uint8_t mask = 0U;

	if (gpio_get(GPIOC, GPIO14) == 0U) {
		mask |= (1U << 0U);
	}
	if (gpio_get(GPIOC, GPIO15) == 0U) {
		mask |= (1U << 1U);
	}
	if (gpio_get(GPIOF, GPIO0) == 0U) {
		mask |= (1U << 2U);
	}
	if (gpio_get(GPIOE, GPIO3) == 0U) {
		mask |= (1U << 3U);
	}
	if (gpio_get(GPIOF, GPIO1) == 0U) {
		mask |= (1U << 4U);
	}
	if (gpio_get(GPIOE, GPIO4) == 0U) {
		mask |= (1U << 5U);
	}
	return mask;
}

void board_keys_init(uint32_t now_ms)
{
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_GPIOE);
	rcc_periph_clock_enable(RCC_GPIOF);

	gpio_mode_setup(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO14 | GPIO15);
	gpio_mode_setup(GPIOF, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO0 | GPIO1);
	gpio_mode_setup(GPIOE, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO3 | GPIO4);

	candidate_mask = read_raw_pressed_mask();
	stable_mask = candidate_mask;
	candidate_since_ms = now_ms;
}

void board_keys_tick(uint32_t now_ms)
{
	uint8_t raw_mask = read_raw_pressed_mask();

	if (raw_mask != candidate_mask) {
		candidate_mask = raw_mask;
		candidate_since_ms = now_ms;
		return;
	}
	if (stable_mask != candidate_mask &&
	    (uint32_t)(now_ms - candidate_since_ms) >= KEY_DEBOUNCE_MS) {
		stable_mask = candidate_mask;
	}
}

uint8_t board_keys_pressed_mask(void)
{
	return stable_mask & BOARD_KEY_MASK_ALL;
}
