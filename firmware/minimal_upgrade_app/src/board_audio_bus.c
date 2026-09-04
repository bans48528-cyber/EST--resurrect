#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

#include "board_audio_bus.h"

#define AUDIO_COMMAND_SELECT_PORT GPIOG
#define AUDIO_COMMAND_SELECT_PIN GPIO4
#define AUDIO_DREQ_PORT GPIOG
#define AUDIO_DREQ_PIN GPIO5
#define AUDIO_RESET_PORT GPIOG
#define AUDIO_RESET_PIN GPIO6
#define AUDIO_DATA_SELECT_PORT GPIOG
#define AUDIO_DATA_SELECT_PIN GPIO7
#define AUDIO_LEGACY_DREQ_PIN GPIO6
#define AUDIO_LEGACY_RESET_PIN GPIO7
#define AUDIO_LEGACY_DATA_SELECT_PIN GPIO5
#define AUDIO_SPI_POLL_LIMIT 10000U
#ifndef AUDIO_PIN_SETTLE_ITERATIONS
#define AUDIO_PIN_SETTLE_ITERATIONS 300000U
#endif

static uint16_t dreq_pin;
static uint16_t reset_pin;
static uint16_t data_select_pin;
static uint8_t pin_layout;
static uint8_t pin_probe;

static void pin_settle(void)
{
	/* Startup runs before interrupts: a finite delay, independent of SysTick. */
	volatile uint32_t count;
	for (count = 0U; count < AUDIO_PIN_SETTLE_ITERATIONS; count++) {
	}
}

static uint8_t probe_pin(uint16_t pin)
{
	uint8_t sample;
	gpio_mode_setup(GPIOG, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, pin);
	pin_settle();
	sample = gpio_get(GPIOG, pin) != 0U ? 1U : 0U;
	gpio_mode_setup(GPIOG, GPIO_MODE_INPUT, GPIO_PUPD_PULLDOWN, pin);
	pin_settle();
	if (gpio_get(GPIOG, pin) != 0U) {
		sample |= 2U;
	}
	gpio_mode_setup(GPIOG, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, pin);
	pin_settle();
	return sample;
}

static void deselect_all(void)
{
	gpio_set(GPIOA, GPIO15);
	gpio_set(GPIOG, AUDIO_COMMAND_SELECT_PIN | data_select_pin);
}

static bool wait_idle(void)
{
	uint32_t poll;
	for (poll = 0U; poll < AUDIO_SPI_POLL_LIMIT; poll++) {
		if ((SPI_SR(SPI3) & (SPI_SR_TXE | SPI_SR_BSY)) == SPI_SR_TXE) {
			return true;
		}
	}
	return false;
}

static bool configure_bus(void)
{
	if (!wait_idle()) {
		return false;
	}
	deselect_all();
	spi_disable(SPI3);
	/* 42 MHz / 32 is safe even before the decoder PLL is configured. */
	spi_set_baudrate_prescaler(SPI3, SPI_CR1_BAUDRATE_FPCLK_DIV_32);
	spi_set_clock_polarity_0(SPI3);
	spi_set_clock_phase_0(SPI3);
	spi_enable(SPI3);
	return true;
}

static bool restore_bus(void)
{
	/* RXNE precedes the final clock edge; keep CS asserted until BSY clears. */
	bool idle = wait_idle();
	if (!idle) {
		board_audio_bus_reset(true);
	}
	deselect_all();
	spi_disable(SPI3);
	spi_set_baudrate_prescaler(SPI3, SPI_CR1_BAUDRATE_FPCLK_DIV_64);
	spi_set_clock_polarity_0(SPI3);
	spi_set_clock_phase_1(SPI3);
	spi_enable(SPI3);
	return idle;
}

static bool transfer_byte(uint8_t output, uint8_t *input)
{
	uint32_t poll;
	for (poll = 0U; poll < AUDIO_SPI_POLL_LIMIT; poll++) {
		if ((SPI_SR(SPI3) & SPI_SR_TXE) != 0U) {
			break;
		}
	}
	if (poll == AUDIO_SPI_POLL_LIMIT) {
		return false;
	}
	SPI_DR(SPI3) = output;
	for (poll = 0U; poll < AUDIO_SPI_POLL_LIMIT; poll++) {
		if ((SPI_SR(SPI3) & SPI_SR_RXNE) != 0U) {
			*input = (uint8_t)SPI_DR(SPI3);
			return true;
		}
	}
	return false;
}

void board_audio_bus_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOG);
	pin_layout = 0U;
	dreq_pin = reset_pin = data_select_pin = 0U;
	/* Never drive either candidate DREQ output while identifying the board. */
	gpio_mode_setup(GPIOG, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP,
		GPIO5 | GPIO6 | GPIO7);
	gpio_set(GPIOG, AUDIO_COMMAND_SELECT_PIN);
	gpio_mode_setup(GPIOG, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
		AUDIO_COMMAND_SELECT_PIN);
	pin_settle();
	pin_probe = probe_pin(GPIO5);
	pin_probe |= (uint8_t)(probe_pin(GPIO6) << 2U);
	if (pin_probe == 7U) {
		pin_layout = 1U;
		dreq_pin = AUDIO_DREQ_PIN;
		reset_pin = AUDIO_RESET_PIN;
		data_select_pin = AUDIO_DATA_SELECT_PIN;
	} else if (pin_probe == 13U) {
		pin_layout = 2U;
		dreq_pin = AUDIO_LEGACY_DREQ_PIN;
		reset_pin = AUDIO_LEGACY_RESET_PIN;
		data_select_pin = AUDIO_LEGACY_DATA_SELECT_PIN;
	} else {
		return;
	}
	deselect_all();
	gpio_clear(GPIOG, reset_pin);
	gpio_mode_setup(GPIOG, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
		reset_pin | data_select_pin);
	gpio_set_output_options(GPIOG, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
		AUDIO_COMMAND_SELECT_PIN | reset_pin | data_select_pin);
	gpio_mode_setup(GPIOG, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP,
		dreq_pin);
}

void board_audio_bus_reset(bool asserted)
{
	if (asserted) {
		gpio_clear(AUDIO_RESET_PORT, reset_pin);
	} else {
		gpio_set(AUDIO_RESET_PORT, reset_pin);
	}
}

bool board_audio_bus_ready(void)
{
	return pin_layout != 0U && gpio_get(AUDIO_DREQ_PORT, dreq_pin) != 0U;
}

uint8_t board_audio_bus_pin_layout(void)
{
	return pin_layout;
}

uint8_t board_audio_bus_pin_probe(void)
{
	return pin_probe;
}

uint8_t board_audio_bus_pin_levels(void)
{
	return (uint8_t)(gpio_get(GPIOG, AUDIO_COMMAND_SELECT_PIN |
		AUDIO_DREQ_PIN | AUDIO_RESET_PIN | AUDIO_DATA_SELECT_PIN) >> 4U);
}

static bool register_command(uint8_t command, uint8_t reg, uint16_t *value)
{
	uint8_t ignored;
	uint8_t high;
	uint8_t low;
	bool ok;
	if (!board_audio_bus_ready()) {
		return false;
	}
	if (!configure_bus()) {
		return false;
	}
	gpio_clear(AUDIO_COMMAND_SELECT_PORT, AUDIO_COMMAND_SELECT_PIN);
	ok = transfer_byte(command, &ignored) && transfer_byte(reg, &ignored) &&
		transfer_byte((uint8_t)(*value >> 8U), &high) &&
		transfer_byte((uint8_t)*value, &low);
	if (!restore_bus()) {
		ok = false;
	}
	if (ok && command == 3U) {
		*value = (uint16_t)((uint16_t)high << 8U) | low;
	}
	return ok;
}

bool board_audio_bus_read(uint8_t reg, uint16_t *value)
{
	*value = 0xFFFFU;
	return register_command(3U, reg, value);
}

bool board_audio_bus_write(uint8_t reg, uint16_t value)
{
	return register_command(2U, reg, &value);
}

bool board_audio_bus_send(const uint8_t *data, size_t length)
{
	size_t index;
	uint8_t ignored;
	bool ok = true;
	if (length > 32U || !board_audio_bus_ready()) {
		return false;
	}
	if (!configure_bus()) {
		return false;
	}
	gpio_clear(AUDIO_DATA_SELECT_PORT, data_select_pin);
	for (index = 0U; index < length; index++) {
		if (!transfer_byte(data[index], &ignored)) {
			ok = false;
			break;
		}
	}
	if (!restore_bus()) {
		ok = false;
	}
	return ok;
}
