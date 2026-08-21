#include <stdint.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/stm32/rcc.h>

#include "app_config.h"
#include "platform.h"

static volatile uint32_t milliseconds;

void sys_tick_handler(void);

static void reset_bootloader_peripherals(void)
{
	static const enum rcc_periph_rst resets[] = {
		RST_DMA1, RST_DMA2, RST_DMA2D,
		RST_OTGFS, RST_OTGHS,
		RST_TIM1, RST_TIM2, RST_TIM3, RST_TIM4, RST_TIM5,
		RST_TIM6, RST_TIM7, RST_TIM8, RST_TIM9, RST_TIM10,
		RST_TIM11, RST_TIM12, RST_TIM13, RST_TIM14,
		RST_SPI1, RST_SPI2, RST_SPI3, RST_SPI4, RST_SPI5, RST_SPI6,
		RST_USART1, RST_USART2, RST_USART3, RST_USART6,
		RST_UART4, RST_UART5, RST_UART7, RST_UART8,
		RST_I2C1, RST_I2C2, RST_I2C3,
		RST_ADC, RST_DAC, RST_SDIO, RST_DCMI, RST_FMC
	};
	uint32_t index;

	for (index = 0U; index < (sizeof(resets) / sizeof(resets[0])); index++) {
		rcc_periph_reset_pulse(resets[index]);
		platform_watchdog_kick();
	}
}

void platform_prepare_from_bootloader(void)
{
	uint32_t index;

	platform_watchdog_kick();
	STK_CSR = 0U;
	STK_RVR = 0U;
	STK_CVR = 0U;
	for (index = 0U; index < 8U; index++) {
		NVIC_ICER(index) = 0xFFFFFFFFU;
		NVIC_ICPR(index) = 0xFFFFFFFFU;
	}
	SCB_ICSR = SCB_ICSR_PENDSVCLR | SCB_ICSR_PENDSTCLR;

	reset_bootloader_peripherals();
	SCB_VTOR = APP_FLASH_START;

	/* The EST 3.0 bootloader already supplies this exact PLL clock tree. */
	rcc_ahb_frequency = 168000000U;
	rcc_apb1_frequency = 42000000U;
	rcc_apb2_frequency = 84000000U;
	SCB_VTOR = APP_FLASH_START;
	platform_watchdog_kick();
}

void platform_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOE);
	rcc_periph_clock_enable(RCC_GPIOF);
	rcc_periph_clock_enable(RCC_GPIOC);

	gpio_set(GPIOE, GPIO2);
	gpio_mode_setup(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO2);
	gpio_set_output_options(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, GPIO2);

	gpio_clear(GPIOF, GPIO2);
	gpio_clear(GPIOC, GPIO13);
	gpio_mode_setup(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO2);
	gpio_mode_setup(GPIOC, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO13);
	gpio_set_output_options(GPIOF, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO2);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO13);

	systick_set_frequency(1000U, 168000000U);
	systick_interrupt_enable();
	systick_counter_enable();
	platform_watchdog_kick();
}

void platform_enable_interrupts(void)
{
	cm_enable_interrupts();
}

uint32_t platform_millis(void)
{
	return milliseconds;
}

void platform_diag_set(uint8_t phase)
{
	gpio_clear(GPIOF, GPIO2);
	if ((phase & 1U) == 0U) {
		gpio_clear(GPIOC, GPIO13);
	} else {
		gpio_set(GPIOC, GPIO13);
	}
}

void platform_diag_checkpoint(uint8_t code)
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

void platform_watchdog_kick(void)
{
	iwdg_reset();
}

void platform_power_off(void)
{
	cm_disable_interrupts();
	gpio_clear(GPIOF, GPIO2);
	gpio_clear(GPIOC, GPIO13);
	gpio_clear(GPIOE, GPIO2);
	while (1) {
	}
}

void sys_tick_handler(void)
{
	milliseconds++;
	platform_watchdog_kick();
}
