#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

#include "board_audio.h"

#define AUDIO_COMMAND_SELECT_PORT GPIOG
#define AUDIO_COMMAND_SELECT_PIN GPIO4
#define AUDIO_DREQ_PORT GPIOG
#define AUDIO_DREQ_PIN GPIO5
#define AUDIO_RESET_PORT GPIOG
#define AUDIO_RESET_PIN GPIO6
#define AUDIO_DATA_SELECT_PORT GPIOG
#define AUDIO_DATA_SELECT_PIN GPIO7
#define FLASH_CHIP_SELECT_PORT GPIOA
#define FLASH_CHIP_SELECT_PIN GPIO15

#define AUDIO_WRITE_COMMAND 0x02U
#define AUDIO_READ_COMMAND 0x03U
#define AUDIO_MODE_REGISTER 0x00U
#define AUDIO_CLOCK_REGISTER 0x03U
#define AUDIO_VOLUME_REGISTER 0x0BU
#define AUDIO_MODE_NEW 0x0800U
#define AUDIO_CLOCK_NORMAL 0x9800U
#define AUDIO_VOLUME_TEST 0x0000U
#define AUDIO_READY_POLL_LIMIT 500000U
#define AUDIO_STREAM_CHUNK_SIZE 32U
#define AUDIO_PCM_SAMPLE_RATE 8000U
#define AUDIO_PCM_SAMPLE_COUNT 40000U
#define AUDIO_WAV_HEADER_SIZE 44U
#define AUDIO_STREAM_SIZE (AUDIO_WAV_HEADER_SIZE + AUDIO_PCM_SAMPLE_COUNT)
#define AUDIO_DRAIN_DURATION_MS 600U

enum audio_test_state {
	AUDIO_TEST_IDLE = 0,
	AUDIO_TEST_STREAMING,
	AUDIO_TEST_DRAINING
};

static bool audio_is_ready;
static enum audio_test_state test_state;
static uint32_t test_deadline_ms;
static uint32_t stream_offset;

static const uint8_t wav_header[AUDIO_WAV_HEADER_SIZE] = {
	'R', 'I', 'F', 'F', 0x64U, 0x9CU, 0x00U, 0x00U,
	'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
	0x10U, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U, 0x01U, 0x00U,
	0x40U, 0x1FU, 0x00U, 0x00U, 0x40U, 0x1FU, 0x00U, 0x00U,
	0x01U, 0x00U, 0x08U, 0x00U, 'd', 'a', 't', 'a',
	0x40U, 0x9CU, 0x00U, 0x00U
};

static void short_delay(void)
{
	volatile uint32_t count;

	for (count = 0U; count < 500000U; count++) {
		__asm__("nop");
	}
}

static bool wait_dreq(void)
{
	uint32_t count;

	for (count = 0U; count < AUDIO_READY_POLL_LIMIT; count++) {
		if (gpio_get(AUDIO_DREQ_PORT, AUDIO_DREQ_PIN) != 0U) {
			return true;
		}
	}
	return false;
}

static uint8_t transfer_byte(uint8_t value)
{
	return (uint8_t)spi_xfer(SPI3, value);
}

static void configure_audio_bus(void)
{
	spi_disable(SPI3);
	spi_set_baudrate_prescaler(SPI3, SPI_CR1_BAUDRATE_FPCLK_DIV_32);
	spi_set_clock_polarity_1(SPI3);
	spi_set_clock_phase_1(SPI3);
	spi_enable(SPI3);
}

static void restore_flash_bus(void)
{
	spi_disable(SPI3);
	spi_set_baudrate_prescaler(SPI3, SPI_CR1_BAUDRATE_FPCLK_DIV_64);
	spi_set_clock_polarity_0(SPI3);
	spi_set_clock_phase_0(SPI3);
	spi_enable(SPI3);
}

static void deselect_all(void)
{
	gpio_set(FLASH_CHIP_SELECT_PORT, FLASH_CHIP_SELECT_PIN);
	gpio_set(AUDIO_COMMAND_SELECT_PORT, AUDIO_COMMAND_SELECT_PIN);
	gpio_set(AUDIO_DATA_SELECT_PORT, AUDIO_DATA_SELECT_PIN);
}

static bool write_register(uint8_t reg, uint16_t value)
{
	if (!wait_dreq()) {
		return false;
	}
	configure_audio_bus();
	deselect_all();
	gpio_clear(AUDIO_COMMAND_SELECT_PORT, AUDIO_COMMAND_SELECT_PIN);
	(void)transfer_byte(AUDIO_WRITE_COMMAND);
	(void)transfer_byte(reg);
	(void)transfer_byte((uint8_t)(value >> 8U));
	(void)transfer_byte((uint8_t)value);
	deselect_all();
	restore_flash_bus();
	return true;
}

static bool read_register(uint8_t reg, uint16_t *value)
{
	uint16_t result;

	if (value == NULL || !wait_dreq()) {
		return false;
	}
	configure_audio_bus();
	deselect_all();
	gpio_clear(AUDIO_COMMAND_SELECT_PORT, AUDIO_COMMAND_SELECT_PIN);
	(void)transfer_byte(AUDIO_READ_COMMAND);
	(void)transfer_byte(reg);
	result = (uint16_t)transfer_byte(0xFFU) << 8U;
	result |= transfer_byte(0xFFU);
	deselect_all();
	restore_flash_bus();
	*value = result;
	return true;
}

static uint8_t pcm_sample(uint32_t sample_index)
{
	uint32_t frequency;
	uint32_t segment_index;
	uint32_t phase;

	if (sample_index < 6400U) {
		frequency = 440U;
		segment_index = sample_index;
	} else if (sample_index < 8000U) {
		return 128U;
	} else if (sample_index < 14400U) {
		frequency = 554U;
		segment_index = sample_index - 8000U;
	} else if (sample_index < 16000U) {
		return 128U;
	} else if (sample_index < 22400U) {
		frequency = 660U;
		segment_index = sample_index - 16000U;
	} else if (sample_index < 24000U) {
		return 128U;
	} else if (sample_index < 30400U) {
		frequency = 880U;
		segment_index = sample_index - 24000U;
	} else if (sample_index < 32000U) {
		return 128U;
	} else {
		frequency = 660U;
		segment_index = sample_index - 32000U;
	}
	phase = (segment_index * frequency) % AUDIO_PCM_SAMPLE_RATE;
	return phase < (AUDIO_PCM_SAMPLE_RATE / 2U) ? 240U : 16U;
}

static uint8_t stream_byte(uint32_t offset)
{
	if (offset < AUDIO_WAV_HEADER_SIZE) {
		return wav_header[offset];
	}
	return pcm_sample(offset - AUDIO_WAV_HEADER_SIZE);
}

static void send_stream_chunk(void)
{
	uint32_t remaining = AUDIO_STREAM_SIZE - stream_offset;
	uint32_t count = remaining < AUDIO_STREAM_CHUNK_SIZE ?
		remaining : AUDIO_STREAM_CHUNK_SIZE;
	uint32_t index;

	configure_audio_bus();
	deselect_all();
	gpio_clear(AUDIO_DATA_SELECT_PORT, AUDIO_DATA_SELECT_PIN);
	for (index = 0U; index < count; index++) {
		(void)transfer_byte(stream_byte(stream_offset++));
	}
	deselect_all();
	restore_flash_bus();
}

void board_audio_init(void)
{
	uint16_t mode = 0xFFFFU;

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOG);
	deselect_all();
	gpio_clear(AUDIO_RESET_PORT, AUDIO_RESET_PIN);
	gpio_mode_setup(AUDIO_COMMAND_SELECT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
		AUDIO_COMMAND_SELECT_PIN | AUDIO_RESET_PIN | AUDIO_DATA_SELECT_PIN);
	gpio_set_output_options(AUDIO_COMMAND_SELECT_PORT, GPIO_OTYPE_PP,
		GPIO_OSPEED_50MHZ,
		AUDIO_COMMAND_SELECT_PIN | AUDIO_RESET_PIN | AUDIO_DATA_SELECT_PIN);
	gpio_mode_setup(AUDIO_DREQ_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP,
		AUDIO_DREQ_PIN);
	short_delay();
	gpio_set(AUDIO_RESET_PORT, AUDIO_RESET_PIN);
	short_delay();
	audio_is_ready = wait_dreq() && read_register(AUDIO_MODE_REGISTER, &mode) &&
		mode != 0xFFFFU;
	test_state = AUDIO_TEST_IDLE;
	test_deadline_ms = 0U;
	stream_offset = 0U;
}

bool board_audio_ready(void)
{
	return audio_is_ready;
}

bool board_audio_start_test(uint32_t now_ms)
{
	if (!audio_is_ready || test_state != AUDIO_TEST_IDLE ||
	    !write_register(AUDIO_MODE_REGISTER, AUDIO_MODE_NEW) ||
	    !write_register(AUDIO_CLOCK_REGISTER, AUDIO_CLOCK_NORMAL) ||
	    !write_register(AUDIO_VOLUME_REGISTER, AUDIO_VOLUME_TEST)) {
		return false;
	}
	(void)now_ms;
	stream_offset = 0U;
	test_state = AUDIO_TEST_STREAMING;
	return true;
}

bool board_audio_test_active(void)
{
	return test_state != AUDIO_TEST_IDLE;
}

void board_audio_tick(uint32_t now_ms)
{
	if (test_state == AUDIO_TEST_IDLE) {
		return;
	}
	if (test_state == AUDIO_TEST_STREAMING) {
		if (gpio_get(AUDIO_DREQ_PORT, AUDIO_DREQ_PIN) == 0U) {
			return;
		}
		send_stream_chunk();
		if (stream_offset == AUDIO_STREAM_SIZE) {
			test_state = AUDIO_TEST_DRAINING;
			test_deadline_ms = now_ms + AUDIO_DRAIN_DURATION_MS;
		}
		return;
	}
	if ((int32_t)(now_ms - test_deadline_ms) >= 0) {
		test_state = AUDIO_TEST_IDLE;
	}
}
