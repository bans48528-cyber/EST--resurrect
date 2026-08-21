#include <stdint.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>

#include "app_config.h"
#include "platform.h"
#include "watchdog.h"

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
		watchdog_kick();
	}
}

void platform_prepare_from_bootloader(void)
{
	uint32_t index;

	watchdog_kick();
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
	watchdog_kick();
}

void platform_enable_interrupts(void)
{
	cm_enable_interrupts();
}

void platform_disable_interrupts(void)
{
	cm_disable_interrupts();
}
