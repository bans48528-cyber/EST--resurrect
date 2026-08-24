#include <stdint.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>

#include "board_backlight.h"

#define BACKLIGHT_PORT GPIOA
#define BACKLIGHT_PIN GPIO8
#define BACKLIGHT_PWM_PERIOD 999U
#define BACKLIGHT_PWM_COUNTS 1000U

static uint8_t brightness_percent;

void board_backlight_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_TIM1);
	gpio_mode_setup(BACKLIGHT_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
		BACKLIGHT_PIN);
	gpio_set_output_options(BACKLIGHT_PORT, GPIO_OTYPE_PP,
		GPIO_OSPEED_50MHZ, BACKLIGHT_PIN);
	gpio_set_af(BACKLIGHT_PORT, GPIO_AF1, BACKLIGHT_PIN);
	rcc_periph_reset_pulse(RST_TIM1);
	timer_set_mode(TIM1, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE,
		TIM_CR1_DIR_UP);
	timer_set_prescaler(TIM1, 167U);
	timer_set_period(TIM1, BACKLIGHT_PWM_PERIOD);
	timer_set_oc_mode(TIM1, TIM_OC1, TIM_OCM_PWM1);
	timer_set_oc_polarity_high(TIM1, TIM_OC1);
	timer_enable_oc_preload(TIM1, TIM_OC1);
	timer_set_oc_value(TIM1, TIM_OC1, BACKLIGHT_PWM_COUNTS);
	timer_enable_oc_output(TIM1, TIM_OC1);
	timer_enable_break_main_output(TIM1);
	timer_enable_preload(TIM1);
	timer_generate_event(TIM1, TIM_EGR_UG);
	timer_enable_counter(TIM1);
	brightness_percent = 100U;
}

void board_backlight_set_percent(uint8_t percent)
{
	brightness_percent = percent <= 100U ? percent : 100U;
	timer_set_oc_value(TIM1, TIM_OC1,
		(uint32_t)brightness_percent * 10U);
}

uint8_t board_backlight_percent(void)
{
	return brightness_percent;
}

void board_backlight_tick(uint32_t now_ms)
{
	(void)now_ms;
}
