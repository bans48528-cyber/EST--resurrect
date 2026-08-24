#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include "board_sensor.h"

/* Verified V5 input port 1/A wiring. */
#define SENSOR_A_ADC0_PORT GPIOF
#define SENSOR_A_ADC0_PIN GPIO4
#define SENSOR_A_ADC1_PORT GPIOF
#define SENSOR_A_ADC1_PIN GPIO5
#define SENSOR_A_POWER_PORT GPIOF
#define SENSOR_A_POWER_PIN GPIO6
#define SENSOR_A_DIGITAL1_PORT GPIOF
#define SENSOR_A_DIGITAL1_PIN GPIO7
#define SENSOR_A_DIGITAL0_PORT GPIOB
#define SENSOR_A_DIGITAL0_PIN GPIO3
#define SENSOR_A_LEGACY_DETECT_PORT GPIOB
#define SENSOR_A_LEGACY_DETECT_PIN GPIO4
#define SENSOR_A_UART_PORT GPIOA
#define SENSOR_A_UART_TX_PIN GPIO0
#define SENSOR_A_UART_RX_PIN GPIO1
#define SENSOR_A_UART_ENABLE_PORT GPIOG
#define SENSOR_A_UART_ENABLE_PIN GPIO15

/* Verified V5 input port 2/B wiring. */
#define SENSOR_B_ADC0_PORT GPIOC
#define SENSOR_B_ADC0_PIN GPIO1
#define SENSOR_B_ADC1_PORT GPIOA
#define SENSOR_B_ADC1_PIN GPIO2
#define SENSOR_B_POWER_PORT GPIOF
#define SENSOR_B_POWER_PIN GPIO12
#define SENSOR_B_DIGITAL0_PORT GPIOF
#define SENSOR_B_DIGITAL0_PIN GPIO13
#define SENSOR_B_DIGITAL1_PORT GPIOF
#define SENSOR_B_DIGITAL1_PIN GPIO11
#define SENSOR_B_LEGACY_DETECT_PORT GPIOF
#define SENSOR_B_LEGACY_DETECT_PIN GPIO14
#define SENSOR_B_UART_PORT GPIOD
#define SENSOR_B_UART_TX_PIN GPIO5
#define SENSOR_B_UART_RX_PIN GPIO6
#define SENSOR_B_UART_ENABLE_PORT GPIOD
#define SENSOR_B_UART_ENABLE_PIN GPIO4

/* Verified V5 input port 3/C wiring. */
#define SENSOR_C_ADC0_PORT GPIOA
#define SENSOR_C_ADC0_PIN GPIO4
#define SENSOR_C_ADC1_PORT GPIOF
#define SENSOR_C_ADC1_PIN GPIO10
#define SENSOR_C_POWER_PORT GPIOF
#define SENSOR_C_POWER_PIN GPIO15
#define SENSOR_C_DIGITAL0_PORT GPIOG
#define SENSOR_C_DIGITAL0_PIN GPIO1
#define SENSOR_C_DIGITAL1_PORT GPIOG
#define SENSOR_C_DIGITAL1_PIN GPIO0
#define SENSOR_C_LEGACY_DETECT_PORT GPIOE
#define SENSOR_C_LEGACY_DETECT_PIN GPIO7
#define SENSOR_C_UART_PORT GPIOB
#define SENSOR_C_UART_TX_PIN GPIO6
#define SENSOR_C_UART_RX_PIN GPIO7
#define SENSOR_C_UART_ENABLE_PORT GPIOE
#define SENSOR_C_UART_ENABLE_PIN GPIO8

/* Verified V5 input port 4/D wiring. */
#define SENSOR_D_ADC0_PORT GPIOA
#define SENSOR_D_ADC0_PIN GPIO7
#define SENSOR_D_ADC1_PORT GPIOA
#define SENSOR_D_ADC1_PIN GPIO6
#define SENSOR_D_POWER_PORT GPIOE
#define SENSOR_D_POWER_PIN GPIO15
#define SENSOR_D_DIGITAL0_PORT GPIOE
#define SENSOR_D_DIGITAL0_PIN GPIO11
#define SENSOR_D_DIGITAL1_PORT GPIOE
#define SENSOR_D_DIGITAL1_PIN GPIO12
#define SENSOR_D_LEGACY_DETECT_PORT GPIOE
#define SENSOR_D_LEGACY_DETECT_PIN GPIO10
#define SENSOR_D_UART_PORT GPIOD
#define SENSOR_D_UART_TX_PIN GPIO8
#define SENSOR_D_UART_RX_PIN GPIO9
#define SENSOR_D_UART_ENABLE_PORT GPIOE
#define SENSOR_D_UART_ENABLE_PIN GPIO9

#define SENSOR_SYNC_BAUD 2400U
#define SENSOR_DEFAULT_DATA_BAUD 57600U
#define SENSOR_MIN_DATA_BAUD 2400U
#define SENSOR_MAX_DATA_BAUD 460800U
#define SENSOR_SYSTEM_NACK 0x02U
#define SENSOR_SYSTEM_ACK 0x04U
#define SENSOR_SELECT_MODE 0x43U
#define SENSOR_MESSAGE_TYPE_MASK 0xC0U
#define SENSOR_MESSAGE_SYSTEM 0x00U
#define SENSOR_MESSAGE_COMMAND 0x40U
#define SENSOR_MESSAGE_INFO 0x80U
#define SENSOR_MESSAGE_DATA 0xC0U
#define SENSOR_COMMAND_TYPE 0x00U
#define SENSOR_COMMAND_SPEED 0x02U
#define SENSOR_RX_MESSAGE_MAX 35U
#define SENSOR_ADC_SAMPLE_INTERVAL_MS 20U
#define SENSOR_KEEPALIVE_INTERVAL_MS 100U
#define SENSOR_STREAM_START_DELAY_MS 20U
#define SENSOR_STREAM_TIMEOUT_MS 1300U
#define SENSOR_SYNC_RESTART_MS 2500U
#define SENSOR_RX_RING_SIZE 256U
#define SENSOR_I2C_ADDRESS 0x4CU
#define SENSOR_I2C_IDENTIFY_REGISTER 0x01U
#define SENSOR_I2C_IDENTIFY_VALUE 0x60U
#define SENSOR_I2C_TEMPERATURE_REGISTER 0x00U
#define SENSOR_I2C_PROBE_INTERVAL_MS 250U
#define SENSOR_I2C_POLL_INTERVAL_MS 200U
#define SENSOR_I2C_FAILURE_LIMIT 3U
#define SENSOR_I2C_SWITCH_SETTLE_MS 20U
#define SENSOR_I2C_DETECT_ADC0_MIN_RAW 656U
#define SENSOR_I2C_DETECT_ADC1_MIN_RAW 984U
/* Match the original firmware's approximately 50 us I2C half-period. */
#define SENSOR_I2C_DELAY_CYCLES 1000U

/*
 * The original firmware converts a 12-bit ADC sample to millivolts with
 * raw * 5000 / 4096.  Keep its passive touch-sensor limits unchanged:
 * pin 1 identifies the 910-ohm sensor resistor, while pin 6 is low when
 * pressed and high when released.  Values between the two button limits
 * retain the previous state to provide hysteresis.
 */
#define SENSOR_TOUCH_ID_MIN_RAW 82U
#define SENSOR_TOUCH_ID_MAX_RAW 656U
#define SENSOR_TOUCH_PRESS_MAX_RAW 655U
#define SENSOR_TOUCH_RELEASE_MIN_RAW 1229U
#define SENSOR_TOUCH_CONNECT_SAMPLES 15U
#define SENSOR_TOUCH_DISCONNECT_SAMPLES 5U
#define SENSOR_TOUCH_DEBOUNCE_SAMPLES 2U

struct sensor_hardware {
	uint32_t uart;
	enum rcc_periph_clken uart_clock;
	enum rcc_periph_rst uart_reset;
	uint8_t uart_irq;
	uint32_t uart_port;
	uint16_t uart_tx_pin;
	uint16_t uart_rx_pin;
	uint8_t uart_af;
	uint32_t power_port;
	uint16_t power_pin;
	uint32_t enable_port;
	uint16_t enable_pin;
	uint32_t digital0_port;
	uint16_t digital0_pin;
	uint32_t digital1_port;
	uint16_t digital1_pin;
	uint32_t legacy_port;
	uint16_t legacy_pin;
	uint32_t adc0_port;
	uint16_t adc0_pin;
	uint32_t adc0;
	uint8_t adc0_channel;
	uint32_t adc1_port;
	uint16_t adc1_pin;
	uint32_t adc1;
	uint8_t adc1_channel;
};

struct sensor_runtime {
	struct board_sensor_snapshot snapshot;
	uint8_t rx_ring[SENSOR_RX_RING_SIZE];
	volatile uint8_t rx_head;
	volatile uint8_t rx_tail;
	volatile uint16_t rx_overflows;
	uint8_t rx_message[SENSOR_RX_MESSAGE_MAX];
	uint8_t rx_message_length;
	uint8_t rx_message_expected;
	enum board_sensor_mode requested_mode;
	uint32_t announced_baud;
	uint32_t last_rx_ms;
	uint32_t last_adc_ms;
	uint32_t last_i2c_ms;
	uint32_t last_keepalive_ms;
	uint32_t stream_started_ms;
	bool stream_commands_started;
	bool received_type;
	bool received_speed;
	uint8_t touch_id_samples;
	uint8_t touch_disconnect_samples;
	uint8_t touch_debounce_samples;
	uint8_t temperature_failures;
	bool touch_candidate_pressed;
	bool i2c_path_enabled;
};

static const struct sensor_hardware sensor_hardware[BOARD_SENSOR_PORT_COUNT] = {
	{
		UART4, RCC_UART4, RST_UART4, NVIC_UART4_IRQ,
		SENSOR_A_UART_PORT, SENSOR_A_UART_TX_PIN, SENSOR_A_UART_RX_PIN,
		GPIO_AF8, SENSOR_A_POWER_PORT, SENSOR_A_POWER_PIN,
		SENSOR_A_UART_ENABLE_PORT, SENSOR_A_UART_ENABLE_PIN,
		SENSOR_A_DIGITAL0_PORT, SENSOR_A_DIGITAL0_PIN,
		SENSOR_A_DIGITAL1_PORT, SENSOR_A_DIGITAL1_PIN,
		SENSOR_A_LEGACY_DETECT_PORT, SENSOR_A_LEGACY_DETECT_PIN,
		SENSOR_A_ADC0_PORT, SENSOR_A_ADC0_PIN, ADC3, ADC_CHANNEL14,
		SENSOR_A_ADC1_PORT, SENSOR_A_ADC1_PIN, ADC3, ADC_CHANNEL15
	},
	{
		USART2, RCC_USART2, RST_USART2, NVIC_USART2_IRQ,
		SENSOR_B_UART_PORT, SENSOR_B_UART_TX_PIN, SENSOR_B_UART_RX_PIN,
		GPIO_AF7, SENSOR_B_POWER_PORT, SENSOR_B_POWER_PIN,
		SENSOR_B_UART_ENABLE_PORT, SENSOR_B_UART_ENABLE_PIN,
		SENSOR_B_DIGITAL0_PORT, SENSOR_B_DIGITAL0_PIN,
		SENSOR_B_DIGITAL1_PORT, SENSOR_B_DIGITAL1_PIN,
		SENSOR_B_LEGACY_DETECT_PORT, SENSOR_B_LEGACY_DETECT_PIN,
		SENSOR_B_ADC0_PORT, SENSOR_B_ADC0_PIN, ADC3, ADC_CHANNEL11,
		SENSOR_B_ADC1_PORT, SENSOR_B_ADC1_PIN, ADC1, ADC_CHANNEL2
	},
	{
		USART1, RCC_USART1, RST_USART1, NVIC_USART1_IRQ,
		SENSOR_C_UART_PORT, SENSOR_C_UART_TX_PIN, SENSOR_C_UART_RX_PIN,
		GPIO_AF7, SENSOR_C_POWER_PORT, SENSOR_C_POWER_PIN,
		SENSOR_C_UART_ENABLE_PORT, SENSOR_C_UART_ENABLE_PIN,
		SENSOR_C_DIGITAL0_PORT, SENSOR_C_DIGITAL0_PIN,
		SENSOR_C_DIGITAL1_PORT, SENSOR_C_DIGITAL1_PIN,
		SENSOR_C_LEGACY_DETECT_PORT, SENSOR_C_LEGACY_DETECT_PIN,
		SENSOR_C_ADC0_PORT, SENSOR_C_ADC0_PIN, ADC1, ADC_CHANNEL4,
		SENSOR_C_ADC1_PORT, SENSOR_C_ADC1_PIN, ADC3, ADC_CHANNEL8
	},
	{
		USART3, RCC_USART3, RST_USART3, NVIC_USART3_IRQ,
		SENSOR_D_UART_PORT, SENSOR_D_UART_TX_PIN, SENSOR_D_UART_RX_PIN,
		GPIO_AF7, SENSOR_D_POWER_PORT, SENSOR_D_POWER_PIN,
		SENSOR_D_UART_ENABLE_PORT, SENSOR_D_UART_ENABLE_PIN,
		SENSOR_D_DIGITAL0_PORT, SENSOR_D_DIGITAL0_PIN,
		SENSOR_D_DIGITAL1_PORT, SENSOR_D_DIGITAL1_PIN,
		SENSOR_D_LEGACY_DETECT_PORT, SENSOR_D_LEGACY_DETECT_PIN,
		SENSOR_D_ADC0_PORT, SENSOR_D_ADC0_PIN, ADC1, ADC_CHANNEL7,
		SENSOR_D_ADC1_PORT, SENSOR_D_ADC1_PIN, ADC1, ADC_CHANNEL6
	}
};

static struct sensor_runtime sensor_runtime[BOARD_SENSOR_PORT_COUNT];

static bool port_valid(enum board_sensor_port port)
{
	return (uint8_t)port < BOARD_SENSOR_PORT_COUNT;
}

static void configure_digital_inputs(enum board_sensor_port port)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];

	gpio_mode_setup(hardware->digital0_port, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
		hardware->digital0_pin);
	gpio_mode_setup(hardware->digital1_port, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
		hardware->digital1_pin);
}

static void configure_i2c_gpio(enum board_sensor_port port)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];

	gpio_set(hardware->digital0_port, hardware->digital0_pin);
	gpio_set(hardware->digital1_port, hardware->digital1_pin);
	gpio_mode_setup(hardware->digital0_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
		hardware->digital0_pin);
	gpio_mode_setup(hardware->digital1_port, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
		hardware->digital1_pin);
	gpio_set_output_options(hardware->digital0_port, GPIO_OTYPE_OD,
		GPIO_OSPEED_50MHZ, hardware->digital0_pin);
	gpio_set_output_options(hardware->digital1_port, GPIO_OTYPE_OD,
		GPIO_OSPEED_50MHZ, hardware->digital1_pin);
}

static void i2c_delay(void)
{
	volatile uint32_t count;

	for (count = 0U; count < SENSOR_I2C_DELAY_CYCLES; count++) {
		__asm__ volatile ("nop");
	}
}

static void i2c_set_scl(enum board_sensor_port port, bool high)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];

	if (high) {
		gpio_set(hardware->digital0_port, hardware->digital0_pin);
	} else {
		gpio_clear(hardware->digital0_port, hardware->digital0_pin);
	}
	i2c_delay();
}

static void i2c_set_sda(enum board_sensor_port port, bool high)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];

	if (high) {
		gpio_set(hardware->digital1_port, hardware->digital1_pin);
	} else {
		gpio_clear(hardware->digital1_port, hardware->digital1_pin);
	}
	i2c_delay();
}

static bool i2c_read_sda(enum board_sensor_port port)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];

	return gpio_get(hardware->digital1_port, hardware->digital1_pin) != 0U;
}

static void i2c_start(enum board_sensor_port port)
{
	i2c_set_sda(port, true);
	i2c_set_scl(port, true);
	i2c_set_sda(port, false);
	i2c_set_scl(port, false);
}

static void i2c_stop(enum board_sensor_port port)
{
	i2c_set_sda(port, false);
	i2c_set_scl(port, true);
	i2c_set_sda(port, true);
}

static bool i2c_write_byte(enum board_sensor_port port, uint8_t value)
{
	uint8_t mask;
	bool acknowledged;

	for (mask = 0x80U; mask != 0U; mask >>= 1U) {
		i2c_set_sda(port, (value & mask) != 0U);
		i2c_set_scl(port, true);
		i2c_set_scl(port, false);
	}
	i2c_set_sda(port, true);
	i2c_set_scl(port, true);
	acknowledged = !i2c_read_sda(port);
	i2c_set_scl(port, false);
	return acknowledged;
}

static uint8_t i2c_read_byte(enum board_sensor_port port, bool acknowledge)
{
	uint8_t value = 0U;
	uint8_t index;

	i2c_set_sda(port, true);
	for (index = 0U; index < 8U; index++) {
		value <<= 1U;
		i2c_set_scl(port, true);
		if (i2c_read_sda(port)) {
			value |= 1U;
		}
		i2c_set_scl(port, false);
	}
	i2c_set_sda(port, !acknowledge);
	i2c_set_scl(port, true);
	i2c_set_scl(port, false);
	i2c_set_sda(port, true);
	return value;
}

static bool i2c_write_read(enum board_sensor_port port,
	const uint8_t *write_data, uint8_t write_length,
	uint8_t *read_data, uint8_t read_length)
{
	uint8_t index;

	configure_i2c_gpio(port);
	i2c_start(port);
	if (!i2c_write_byte(port, (uint8_t)(SENSOR_I2C_ADDRESS << 1U))) {
		i2c_stop(port);
		return false;
	}
	for (index = 0U; index < write_length; index++) {
		if (!i2c_write_byte(port, write_data[index])) {
			i2c_stop(port);
			return false;
		}
	}
	if (read_length != 0U) {
		i2c_start(port);
		if (!i2c_write_byte(port,
		    (uint8_t)((SENSOR_I2C_ADDRESS << 1U) | 1U))) {
			i2c_stop(port);
			return false;
		}
		for (index = 0U; index < read_length; index++) {
			read_data[index] = i2c_read_byte(port,
				index + 1U < read_length);
		}
	}
	i2c_stop(port);
	return true;
}

static bool temperature_probe(enum board_sensor_port port)
{
	const uint8_t request[2] = {
		SENSOR_I2C_IDENTIFY_REGISTER, SENSOR_I2C_IDENTIFY_VALUE
	};
	uint8_t response = 0U;
	bool detected = i2c_write_read(port, request, sizeof(request),
		&response, 1U) && response == SENSOR_I2C_IDENTIFY_VALUE;

	if (!detected) {
		configure_digital_inputs(port);
	}
	return detected;
}

static uint8_t temperature_mode(enum board_sensor_mode requested_mode)
{
	return requested_mode == BOARD_SENSOR_MODE_FAHRENHEIT ?
		BOARD_SENSOR_MODE_FAHRENHEIT : BOARD_SENSOR_MODE_CELSIUS;
}

static bool temperature_candidate(const struct board_sensor_snapshot *snapshot)
{
	/*
	 * Preserve the original connection priority: UART/touch sensors load pin 1,
	 * while an NXT-style I2C sensor leaves pin 1 floating and raises pin 6.
	 */
	return snapshot->adc0_raw >= SENSOR_I2C_DETECT_ADC0_MIN_RAW &&
		snapshot->adc1_raw >= SENSOR_I2C_DETECT_ADC1_MIN_RAW;
}

static bool temperature_read(enum board_sensor_port port, uint32_t now_ms)
{
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];
	const uint8_t request = SENSOR_I2C_TEMPERATURE_REGISTER;
	uint8_t response[2] = {0U};
	uint16_t raw_value;
	int16_t signed_raw;
	int32_t tenths;

	if (!i2c_write_read(port, &request, 1U, response, sizeof(response))) {
		return false;
	}
	raw_value = ((uint16_t)response[0] << 4U) |
		((uint16_t)response[1] >> 4U);
	signed_raw = (raw_value & 0x0800U) != 0U ?
		(int16_t)(raw_value | 0xF000U) : (int16_t)raw_value;
	if (runtime->snapshot.mode == BOARD_SENSOR_MODE_FAHRENHEIT) {
		tenths = ((int32_t)signed_raw * 18) / 16 + 320;
	} else {
		tenths = ((int32_t)signed_raw * 10) / 16;
	}
	runtime->snapshot.value = (uint16_t)(int16_t)tenths;
	runtime->snapshot.value_valid = true;
	runtime->snapshot.rx_count += sizeof(response);
	runtime->last_rx_ms = now_ms;
	return true;
}

static void enter_temperature_mode(enum board_sensor_port port,
	uint32_t now_ms)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];

	usart_disable_rx_interrupt(hardware->uart);
	usart_disable(hardware->uart);
	runtime->rx_head = 0U;
	runtime->rx_tail = 0U;
	runtime->rx_message_length = 0U;
	runtime->rx_message_expected = 0U;
	runtime->snapshot.state = BOARD_SENSOR_STREAMING;
	runtime->snapshot.sensor_type = BOARD_SENSOR_TYPE_TEMPERATURE;
	runtime->snapshot.mode = temperature_mode(runtime->requested_mode);
	runtime->snapshot.value_valid = false;
	runtime->temperature_failures = 0U;
	runtime->i2c_path_enabled = true;
	gpio_set(hardware->enable_port, hardware->enable_pin);
	runtime->last_i2c_ms = now_ms - SENSOR_I2C_POLL_INTERVAL_MS;
}

static void configure_uart(enum board_sensor_port port, uint32_t baud)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];

	usart_disable_rx_interrupt(hardware->uart);
	usart_disable(hardware->uart);
	usart_set_baudrate(hardware->uart, baud);
	usart_set_databits(hardware->uart, 8U);
	usart_set_stopbits(hardware->uart, USART_STOPBITS_1);
	usart_set_parity(hardware->uart, USART_PARITY_NONE);
	usart_set_flow_control(hardware->uart, USART_FLOWCONTROL_NONE);
	usart_set_mode(hardware->uart, USART_MODE_TX_RX);
	usart_enable(hardware->uart);
	usart_enable_rx_interrupt(hardware->uart);
}

static void send_uart_byte(enum board_sensor_port port, uint8_t value)
{
	usart_send_blocking(sensor_hardware[(uint8_t)port].uart, value);
}

static void wait_for_uart_transmit_complete(enum board_sensor_port port)
{
	uint32_t uart = sensor_hardware[(uint8_t)port].uart;

	while (!usart_get_flag(uart, USART_FLAG_TC)) {
		/* One byte at the 2400-baud handshake rate takes about 4 ms. */
	}
}

static void send_mode(enum board_sensor_port port,
	enum board_sensor_mode mode)
{
	uint8_t encoded_mode = (uint8_t)mode;

	send_uart_byte(port, SENSOR_SELECT_MODE);
	send_uart_byte(port, encoded_mode);
	send_uart_byte(port,
		(uint8_t)(SENSOR_SELECT_MODE ^ encoded_mode ^ 0xFFU));
}

static void start_sync(enum board_sensor_port port, uint32_t now_ms)
{
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];
	uint32_t uart = hardware->uart;

	usart_disable_rx_interrupt(uart);
	gpio_clear(hardware->enable_port, hardware->enable_pin);
	runtime->rx_head = 0U;
	runtime->rx_tail = 0U;
	configure_digital_inputs(port);
	configure_uart(port, SENSOR_SYNC_BAUD);
	runtime->rx_message_length = 0U;
	runtime->rx_message_expected = 0U;
	runtime->announced_baud = SENSOR_DEFAULT_DATA_BAUD;
	runtime->received_type = false;
	runtime->received_speed = false;
	runtime->stream_commands_started = false;
	runtime->touch_id_samples = 0U;
	runtime->touch_disconnect_samples = 0U;
	runtime->touch_debounce_samples = 0U;
	runtime->touch_candidate_pressed = false;
	runtime->temperature_failures = 0U;
	runtime->i2c_path_enabled = false;
	runtime->snapshot.state = BOARD_SENSOR_SYNCING;
	runtime->snapshot.sensor_type = 0U;
	runtime->snapshot.mode = (uint8_t)runtime->requested_mode;
	runtime->snapshot.value_valid = false;
	runtime->last_rx_ms = now_ms;
	runtime->last_i2c_ms = now_ms - SENSOR_I2C_PROBE_INTERVAL_MS;
}

static uint16_t read_adc_channel(uint32_t adc, uint8_t channel)
{
	uint8_t sequence[1] = {channel};

	adc_set_regular_sequence(adc, 1U, sequence);
	adc_start_conversion_regular(adc);
	while (!adc_eoc(adc)) {
		/* A 144-cycle sample is complete in far less than one millisecond. */
	}
	return (uint16_t)adc_read_regular(adc);
}

static void sample_port_pins(enum board_sensor_port port)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];
	struct board_sensor_snapshot *snapshot =
		&sensor_runtime[(uint8_t)port].snapshot;
	uint8_t digital_mask = 0U;

	snapshot->adc0_raw = read_adc_channel(hardware->adc0,
		hardware->adc0_channel);
	snapshot->adc1_raw = read_adc_channel(hardware->adc1,
		hardware->adc1_channel);
	if (gpio_get(hardware->digital0_port, hardware->digital0_pin) != 0U) {
		digital_mask |= 0x01U;
	}
	if (gpio_get(hardware->digital1_port, hardware->digital1_pin) != 0U) {
		digital_mask |= 0x02U;
	}
	if (gpio_get(hardware->legacy_port, hardware->legacy_pin) != 0U) {
		digital_mask |= 0x04U;
	}
	snapshot->digital_mask = digital_mask;
}

static bool touch_id_present(const struct board_sensor_snapshot *snapshot)
{
	return snapshot->adc0_raw >= SENSOR_TOUCH_ID_MIN_RAW &&
		snapshot->adc0_raw < SENSOR_TOUCH_ID_MAX_RAW;
}

static bool touch_level(const struct board_sensor_snapshot *snapshot,
	bool *pressed)
{
	if (snapshot->adc1_raw <= SENSOR_TOUCH_PRESS_MAX_RAW) {
		*pressed = true;
		return true;
	}
	if (snapshot->adc1_raw >= SENSOR_TOUCH_RELEASE_MIN_RAW) {
		*pressed = false;
		return true;
	}
	return false;
}

static void update_touch_value(struct sensor_runtime *runtime)
{
	bool pressed;

	if (!touch_level(&runtime->snapshot, &pressed)) {
		runtime->touch_debounce_samples = 0U;
		return;
	}
	if (runtime->touch_debounce_samples == 0U ||
	    runtime->touch_candidate_pressed != pressed) {
		runtime->touch_candidate_pressed = pressed;
		runtime->touch_debounce_samples = 1U;
		return;
	}
	if (runtime->touch_debounce_samples < SENSOR_TOUCH_DEBOUNCE_SAMPLES) {
		runtime->touch_debounce_samples++;
	}
	if (runtime->touch_debounce_samples >= SENSOR_TOUCH_DEBOUNCE_SAMPLES) {
		runtime->snapshot.value = pressed ? 1U : 0U;
		runtime->snapshot.value_valid = true;
	}
}

static void enter_touch_mode(enum board_sensor_port port)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];

	usart_disable_rx_interrupt(hardware->uart);
	usart_disable(hardware->uart);
	runtime->rx_head = 0U;
	runtime->rx_tail = 0U;
	runtime->rx_message_length = 0U;
	runtime->rx_message_expected = 0U;
	runtime->snapshot.state = BOARD_SENSOR_STREAMING;
	runtime->snapshot.sensor_type = BOARD_SENSOR_TYPE_TOUCH;
	runtime->snapshot.mode = BOARD_SENSOR_MODE_REFLECTED;
	runtime->snapshot.value_valid = false;
	runtime->touch_disconnect_samples = 0U;
	runtime->touch_debounce_samples = 0U;
}

static void update_touch_detection(enum board_sensor_port port,
	uint32_t now_ms)
{
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];
	bool id_present = touch_id_present(&runtime->snapshot);

	if (runtime->snapshot.sensor_type == BOARD_SENSOR_TYPE_TOUCH &&
	    runtime->snapshot.state == BOARD_SENSOR_STREAMING) {
		if (!id_present) {
			if (runtime->touch_disconnect_samples <
			    SENSOR_TOUCH_DISCONNECT_SAMPLES) {
				runtime->touch_disconnect_samples++;
			}
			if (runtime->touch_disconnect_samples >=
			    SENSOR_TOUCH_DISCONNECT_SAMPLES) {
				start_sync(port, now_ms);
			}
			return;
		}
		runtime->touch_disconnect_samples = 0U;
		update_touch_value(runtime);
		return;
	}

	if (runtime->snapshot.state == BOARD_SENSOR_STREAMING) {
		return;
	}
	if (!id_present) {
		runtime->touch_id_samples = 0U;
		return;
	}
	if (runtime->touch_id_samples < SENSOR_TOUCH_CONNECT_SAMPLES) {
		runtime->touch_id_samples++;
	}
	if (runtime->touch_id_samples >= SENSOR_TOUCH_CONNECT_SAMPLES) {
		enter_touch_mode(port);
	}
}

static uint8_t message_data_size(uint8_t header)
{
	return (uint8_t)(1U << ((header >> 3U) & 0x07U));
}

static bool message_checksum_valid(const struct sensor_runtime *runtime)
{
	uint8_t value = 0U;
	uint8_t index;

	for (index = 0U; index < runtime->rx_message_expected; index++) {
		value ^= runtime->rx_message[index];
	}
	return value == 0xFFU;
}

static void enter_streaming(enum board_sensor_port port, uint32_t now_ms)
{
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];

	send_uart_byte(port, SENSOR_SYSTEM_ACK);
	wait_for_uart_transmit_complete(port);
	configure_uart(port, runtime->announced_baud);
	runtime->rx_message_length = 0U;
	runtime->rx_message_expected = 0U;
	runtime->snapshot.state = BOARD_SENSOR_STREAMING;
	runtime->snapshot.mode = (uint8_t)runtime->requested_mode;
	runtime->snapshot.value_valid = false;
	runtime->stream_started_ms = now_ms;
	runtime->last_keepalive_ms = now_ms;
	runtime->stream_commands_started = false;
}

static void handle_system_byte(enum board_sensor_port port, uint8_t value,
	uint32_t now_ms)
{
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];

	if (value == SENSOR_SYSTEM_ACK &&
	    runtime->snapshot.state == BOARD_SENSOR_SYNCING &&
	    runtime->received_type && runtime->received_speed) {
		enter_streaming(port, now_ms);
	}
}

static void handle_command_message(struct sensor_runtime *runtime,
	uint8_t header)
{
	uint8_t command = header & 0x07U;
	uint8_t size = message_data_size(header);

	if (command == SENSOR_COMMAND_TYPE && size >= 1U) {
		runtime->snapshot.sensor_type = runtime->rx_message[1];
		runtime->received_type =
			runtime->rx_message[1] == BOARD_SENSOR_TYPE_EV3_COLOR ||
			runtime->rx_message[1] == BOARD_SENSOR_TYPE_ULTRASONIC;
	} else if (command == SENSOR_COMMAND_SPEED && size == 4U) {
		uint32_t baud = (uint32_t)runtime->rx_message[1] |
			((uint32_t)runtime->rx_message[2] << 8U) |
			((uint32_t)runtime->rx_message[3] << 16U) |
			((uint32_t)runtime->rx_message[4] << 24U);

		if (baud >= SENSOR_MIN_DATA_BAUD && baud <= SENSOR_MAX_DATA_BAUD) {
			runtime->announced_baud = baud;
			runtime->received_speed = true;
		}
	}
}

static void handle_data_message(struct sensor_runtime *runtime,
	uint8_t header, uint32_t now_ms)
{
	uint8_t mode = header & 0x07U;
	uint8_t size = message_data_size(header);
	uint16_t value;

	if (runtime->snapshot.state != BOARD_SENSOR_STREAMING || mode > 2U) {
		return;
	}
	value = runtime->rx_message[1];
	if (size >= 2U) {
		value |= (uint16_t)runtime->rx_message[2] << 8U;
	}
	runtime->snapshot.mode = mode;
	runtime->snapshot.value = value;
	runtime->snapshot.value_valid = true;
	runtime->last_rx_ms = now_ms;
}

static void handle_complete_message(struct sensor_runtime *runtime,
	uint32_t now_ms)
{
	uint8_t header = runtime->rx_message[0];
	uint8_t type = header & SENSOR_MESSAGE_TYPE_MASK;

	if (!message_checksum_valid(runtime)) {
		runtime->snapshot.checksum_errors++;
		return;
	}
	if (type == SENSOR_MESSAGE_COMMAND) {
		handle_command_message(runtime, header);
	} else if (type == SENSOR_MESSAGE_DATA) {
		handle_data_message(runtime, header, now_ms);
	}
}

static void receive_byte(enum board_sensor_port port, uint8_t value,
	uint32_t now_ms)
{
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];
	uint8_t type;
	uint8_t expected;

	runtime->snapshot.rx_count++;
	runtime->last_rx_ms = now_ms;
	if (runtime->rx_message_length == 0U) {
		type = value & SENSOR_MESSAGE_TYPE_MASK;
		if (type == SENSOR_MESSAGE_SYSTEM) {
			handle_system_byte(port, value, now_ms);
			return;
		}
		expected = (uint8_t)(message_data_size(value) + 2U);
		if (type == SENSOR_MESSAGE_INFO) {
			expected++;
		}
		if (expected > SENSOR_RX_MESSAGE_MAX) {
			runtime->snapshot.checksum_errors++;
			return;
		}
		runtime->rx_message_expected = expected;
	}
	runtime->rx_message[runtime->rx_message_length++] = value;
	if (runtime->rx_message_length == runtime->rx_message_expected) {
		handle_complete_message(runtime, now_ms);
		runtime->rx_message_length = 0U;
		runtime->rx_message_expected = 0U;
	}
}

static void configure_adc(uint32_t adc)
{
	adc_power_off(adc);
	adc_disable_scan_mode(adc);
	adc_set_single_conversion_mode(adc);
	adc_set_right_aligned(adc);
	adc_set_resolution(adc, ADC_CR1_RES_12BIT);
	adc_disable_external_trigger_regular(adc);
	adc_power_on(adc);
}

static void configure_port_gpio(enum board_sensor_port port)
{
	const struct sensor_hardware *hardware = &sensor_hardware[(uint8_t)port];

	gpio_mode_setup(hardware->adc0_port, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
		hardware->adc0_pin);
	gpio_mode_setup(hardware->adc1_port, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
		hardware->adc1_pin);
	configure_digital_inputs(port);
	gpio_mode_setup(hardware->legacy_port, GPIO_MODE_INPUT, GPIO_PUPD_NONE,
		hardware->legacy_pin);

	gpio_set(hardware->power_port, hardware->power_pin);
	gpio_mode_setup(hardware->power_port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP,
		hardware->power_pin);
	gpio_set_output_options(hardware->power_port, GPIO_OTYPE_PP,
		GPIO_OSPEED_100MHZ, hardware->power_pin);

	gpio_clear(hardware->enable_port, hardware->enable_pin);
	gpio_mode_setup(hardware->enable_port, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP,
		hardware->enable_pin);
	gpio_set_output_options(hardware->enable_port, GPIO_OTYPE_PP,
		GPIO_OSPEED_100MHZ, hardware->enable_pin);
	gpio_clear(hardware->enable_port, hardware->enable_pin);

	gpio_mode_setup(hardware->uart_port, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
		hardware->uart_tx_pin | hardware->uart_rx_pin);
	gpio_set_output_options(hardware->uart_port, GPIO_OTYPE_PP,
		GPIO_OSPEED_50MHZ, hardware->uart_tx_pin | hardware->uart_rx_pin);
	gpio_set_af(hardware->uart_port, hardware->uart_af,
		hardware->uart_tx_pin | hardware->uart_rx_pin);
}

void board_sensor_init(uint32_t now_ms)
{
	uint8_t index;

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_GPIOD);
	rcc_periph_clock_enable(RCC_GPIOE);
	rcc_periph_clock_enable(RCC_GPIOF);
	rcc_periph_clock_enable(RCC_GPIOG);
	rcc_periph_clock_enable(RCC_ADC1);
	rcc_periph_clock_enable(RCC_ADC3);
	rcc_periph_reset_pulse(RST_ADC);
	adc_set_clk_prescale(ADC_CCR_ADCPRE_BY4);
	configure_adc(ADC1);
	configure_adc(ADC3);

	memset(sensor_runtime, 0, sizeof(sensor_runtime));
	for (index = 0U; index < BOARD_SENSOR_PORT_COUNT; index++) {
		const struct sensor_hardware *hardware = &sensor_hardware[index];
		struct sensor_runtime *runtime = &sensor_runtime[index];
		enum board_sensor_port port = (enum board_sensor_port)index;

		rcc_periph_clock_enable(hardware->uart_clock);
		rcc_periph_reset_pulse(hardware->uart_reset);
		configure_port_gpio(port);
		adc_set_sample_time(hardware->adc0, hardware->adc0_channel,
			ADC_SMPR_SMP_144CYC);
		adc_set_sample_time(hardware->adc1, hardware->adc1_channel,
			ADC_SMPR_SMP_144CYC);
		runtime->requested_mode = BOARD_SENSOR_MODE_REFLECTED;
		runtime->last_adc_ms = now_ms - SENSOR_ADC_SAMPLE_INTERVAL_MS;
		start_sync(port, now_ms);
		nvic_enable_irq(hardware->uart_irq);
		gpio_clear(hardware->power_port, hardware->power_pin);
	}
}

static void tick_port(enum board_sensor_port port, uint32_t now_ms)
{
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];

	while (runtime->rx_tail != runtime->rx_head) {
		uint8_t value = runtime->rx_ring[runtime->rx_tail];

		runtime->rx_tail++;
		receive_byte(port, value, now_ms);
	}
	if (runtime->rx_overflows != 0U) {
		runtime->snapshot.checksum_errors = (uint16_t)
			(runtime->snapshot.checksum_errors + runtime->rx_overflows);
		runtime->rx_overflows = 0U;
	}

	if ((uint32_t)(now_ms - runtime->last_adc_ms) >=
	    SENSOR_ADC_SAMPLE_INTERVAL_MS) {
		runtime->last_adc_ms = now_ms;
		sample_port_pins(port);
		update_touch_detection(port, now_ms);
	}

	if (runtime->snapshot.sensor_type == BOARD_SENSOR_TYPE_TEMPERATURE &&
	    runtime->snapshot.state == BOARD_SENSOR_STREAMING) {
		if ((uint32_t)(now_ms - runtime->last_i2c_ms) >=
		    SENSOR_I2C_POLL_INTERVAL_MS) {
			runtime->last_i2c_ms = now_ms;
			if (temperature_read(port, now_ms)) {
				runtime->temperature_failures = 0U;
			} else {
				runtime->snapshot.checksum_errors++;
				runtime->temperature_failures++;
				if (runtime->temperature_failures >=
				    SENSOR_I2C_FAILURE_LIMIT) {
					start_sync(port, now_ms);
				}
			}
		}
		return;
	}

	if (runtime->snapshot.state == BOARD_SENSOR_SYNCING &&
	    temperature_candidate(&runtime->snapshot)) {
		const struct sensor_hardware *hardware =
			&sensor_hardware[(uint8_t)port];
		uint32_t probe_interval = runtime->temperature_failures == 0U ?
			SENSOR_I2C_SWITCH_SETTLE_MS : SENSOR_I2C_PROBE_INTERVAL_MS;

		if (!runtime->i2c_path_enabled) {
			/* The original firmware selects the I2C input path before probing. */
			gpio_set(hardware->enable_port, hardware->enable_pin);
			runtime->i2c_path_enabled = true;
			runtime->last_i2c_ms = now_ms;
			return;
		}
		if ((uint32_t)(now_ms - runtime->last_i2c_ms) >= probe_interval) {
			runtime->last_i2c_ms = now_ms;
			if (temperature_probe(port)) {
				enter_temperature_mode(port, now_ms);
				return;
			}
			runtime->temperature_failures = 1U;
		}
	} else if (runtime->i2c_path_enabled) {
		const struct sensor_hardware *hardware =
			&sensor_hardware[(uint8_t)port];

		gpio_clear(hardware->enable_port, hardware->enable_pin);
		configure_digital_inputs(port);
		runtime->i2c_path_enabled = false;
		runtime->temperature_failures = 0U;
	}

	if (runtime->snapshot.sensor_type == BOARD_SENSOR_TYPE_TOUCH &&
	    runtime->snapshot.state == BOARD_SENSOR_STREAMING) {
		return;
	}

	if (runtime->snapshot.state == BOARD_SENSOR_STREAMING) {
		if (!runtime->stream_commands_started &&
		    (uint32_t)(now_ms - runtime->stream_started_ms) >=
			SENSOR_STREAM_START_DELAY_MS) {
			if (runtime->requested_mode != BOARD_SENSOR_MODE_REFLECTED) {
				send_mode(port, runtime->requested_mode);
			}
			send_uart_byte(port, SENSOR_SYSTEM_NACK);
			runtime->last_keepalive_ms = now_ms;
			runtime->stream_commands_started = true;
		} else if (runtime->stream_commands_started &&
			   (uint32_t)(now_ms - runtime->last_keepalive_ms) >=
				SENSOR_KEEPALIVE_INTERVAL_MS) {
			send_uart_byte(port, SENSOR_SYSTEM_NACK);
			runtime->last_keepalive_ms = now_ms;
		}
		if ((uint32_t)(now_ms - runtime->last_rx_ms) >=
		    SENSOR_STREAM_TIMEOUT_MS) {
			runtime->snapshot.state = BOARD_SENSOR_STALE;
			start_sync(port, now_ms);
		}
	} else if (runtime->snapshot.state == BOARD_SENSOR_SYNCING &&
		   (uint32_t)(now_ms - runtime->last_rx_ms) >=
			SENSOR_SYNC_RESTART_MS) {
		start_sync(port, now_ms);
	}
}

void board_sensor_tick(uint32_t now_ms)
{
	uint8_t index;

	for (index = 0U; index < BOARD_SENSOR_PORT_COUNT; index++) {
		tick_port((enum board_sensor_port)index, now_ms);
	}
}

void board_sensor_stop(void)
{
	uint8_t index;

	for (index = 0U; index < BOARD_SENSOR_PORT_COUNT; index++) {
		const struct sensor_hardware *hardware = &sensor_hardware[index];
		struct sensor_runtime *runtime = &sensor_runtime[index];

		usart_disable_rx_interrupt(hardware->uart);
		nvic_disable_irq(hardware->uart_irq);
		usart_disable(hardware->uart);
		configure_digital_inputs((enum board_sensor_port)index);
		gpio_clear(hardware->enable_port, hardware->enable_pin);
		gpio_set(hardware->power_port, hardware->power_pin);
		runtime->snapshot.state = BOARD_SENSOR_OFF;
		runtime->snapshot.sensor_type = 0U;
		runtime->snapshot.value_valid = false;
		runtime->i2c_path_enabled = false;
	}
}

bool board_sensor_restart(enum board_sensor_port port, uint32_t now_ms)
{
	const struct sensor_hardware *hardware;

	if (!port_valid(port)) {
		return false;
	}
	hardware = &sensor_hardware[(uint8_t)port];
	gpio_clear(hardware->power_port, hardware->power_pin);
	gpio_clear(hardware->enable_port, hardware->enable_pin);
	start_sync(port, now_ms);
	nvic_enable_irq(hardware->uart_irq);
	return true;
}

bool board_sensor_set_mode(enum board_sensor_port port,
	enum board_sensor_mode mode, uint32_t now_ms)
{
	struct sensor_runtime *runtime;

	if (!port_valid(port) || mode > BOARD_SENSOR_MODE_COLOR) {
		return false;
	}
	runtime = &sensor_runtime[(uint8_t)port];
	if (runtime->snapshot.sensor_type == BOARD_SENSOR_TYPE_TEMPERATURE &&
	    runtime->snapshot.state == BOARD_SENSOR_STREAMING) {
		runtime->requested_mode = (enum board_sensor_mode)
			temperature_mode(mode);
		runtime->snapshot.mode = (uint8_t)runtime->requested_mode;
		runtime->snapshot.value_valid = false;
		runtime->last_i2c_ms = now_ms - SENSOR_I2C_POLL_INTERVAL_MS;
		return true;
	}
	runtime->requested_mode = mode;
	if (runtime->snapshot.sensor_type == BOARD_SENSOR_TYPE_TOUCH &&
	    runtime->snapshot.state == BOARD_SENSOR_STREAMING) {
		runtime->snapshot.mode = BOARD_SENSOR_MODE_REFLECTED;
		return true;
	}
	runtime->snapshot.mode = (uint8_t)mode;
	runtime->snapshot.value_valid = false;
	if (runtime->snapshot.state == BOARD_SENSOR_STREAMING) {
		send_mode(port, mode);
		send_uart_byte(port, SENSOR_SYSTEM_NACK);
		runtime->last_keepalive_ms = now_ms;
	}
	return true;
}

bool board_sensor_set_all_modes(enum board_sensor_mode mode, uint32_t now_ms)
{
	uint8_t index;
	bool result = true;

	for (index = 0U; index < BOARD_SENSOR_PORT_COUNT; index++) {
		if (!board_sensor_set_mode((enum board_sensor_port)index, mode,
		    now_ms)) {
			result = false;
		}
	}
	return result;
}

bool board_sensor_get_snapshot(enum board_sensor_port port,
	struct board_sensor_snapshot *snapshot)
{
	if (!port_valid(port) || snapshot == NULL) {
		return false;
	}
	*snapshot = sensor_runtime[(uint8_t)port].snapshot;
	return true;
}

static void sensor_uart_isr(enum board_sensor_port port)
{
	struct sensor_runtime *runtime = &sensor_runtime[(uint8_t)port];
	uint32_t uart = sensor_hardware[(uint8_t)port].uart;

	while (usart_get_flag(uart, USART_FLAG_RXNE)) {
		uint8_t value = (uint8_t)usart_recv(uart);
		uint8_t next_head = (uint8_t)(runtime->rx_head + 1U);

		if (next_head == runtime->rx_tail) {
			runtime->rx_overflows++;
		} else {
			runtime->rx_ring[runtime->rx_head] = value;
			runtime->rx_head = next_head;
		}
	}
}

void uart4_isr(void)
{
	sensor_uart_isr(BOARD_SENSOR_PORT_1);
}

void usart2_isr(void)
{
	sensor_uart_isr(BOARD_SENSOR_PORT_2);
}

void usart1_isr(void)
{
	sensor_uart_isr(BOARD_SENSOR_PORT_3);
}

void usart3_isr(void)
{
	sensor_uart_isr(BOARD_SENSOR_PORT_4);
}
