#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "board_led.h"

void board_led_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOF);
	rcc_periph_clock_enable(RCC_GPIOC);

	gpio_clear(GPIOF, GPIO2);
	gpio_clear(GPIOC, GPIO13);
	gpio_mode_setup(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO2);
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
	gpio_set_output_options(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO2);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO13);
}

void board_led_diag_set(uint8_t phase)
{
	gpio_clear(GPIOF, GPIO2);
	if ((phase & 1U) == 0U) {
		gpio_clear(GPIOC, GPIO13);
	} else {
		gpio_set(GPIOC, GPIO13);
	}
}

void board_led_checkpoint(uint8_t code)
{
	if ((code & 1U) != 0U) {
		gpio_set(GPIOF, GPIO2);
	} else {
		gpio_clear(GPIOF, GPIO2);
	}
	if ((code & 2U) != 0U) {
		gpio_set(GPIOC, GPIO13);
	} else {
		gpio_clear(GPIOC, GPIO13);
	}
}

void board_led_all_off(void)
{
	gpio_clear(GPIOF, GPIO2);
	gpio_clear(GPIOC, GPIO13);
}
