#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "board_power.h"

void board_power_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOE);
	gpio_set(GPIOE, GPIO2);
	gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO2);
	gpio_set_output_options(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, GPIO2);
}

void board_power_off(void)
{
	gpio_clear(GPIOE, GPIO2);
	while (1) {
	}
}
