#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "board_lcd.h"
#include "system_time.h"
#include "watchdog.h"

#define LCD_WIDTH 180U
#define LCD_HEIGHT 128U
#define LCD_PAGES (LCD_HEIGHT / 8U)
#define LCD_TRANSFER_COLUMNS 184U

#define LCD_CLOCK_PORT GPIOD
#define LCD_CLOCK_PIN GPIO14
#define LCD_DATA_PORT GPIOG
#define LCD_DATA_PIN GPIO2
#define LCD_RESET_PORT GPIOD
#define LCD_RESET_PIN GPIO15

static uint8_t framebuffer[LCD_WIDTH][LCD_PAGES];

static const uint8_t glyph_blank[5] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
static const uint8_t glyph_dot[5] = {0x00U, 0x60U, 0x60U, 0x00U, 0x00U};
static const uint8_t glyph_a[5] = {0x7EU, 0x09U, 0x09U, 0x09U, 0x7EU};
static const uint8_t glyph_m[5] = {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU};
static const uint8_t glyph_digits[10][5] = {
	{0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU},
	{0x00U, 0x42U, 0x7FU, 0x40U, 0x00U},
	{0x42U, 0x61U, 0x51U, 0x49U, 0x46U},
	{0x21U, 0x41U, 0x45U, 0x4BU, 0x31U},
	{0x18U, 0x14U, 0x12U, 0x7FU, 0x10U},
	{0x27U, 0x45U, 0x45U, 0x45U, 0x39U},
	{0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U},
	{0x01U, 0x71U, 0x09U, 0x05U, 0x03U},
	{0x36U, 0x49U, 0x49U, 0x49U, 0x36U},
	{0x06U, 0x49U, 0x49U, 0x29U, 0x1EU},
};

static void bus_pause(void)
{
	__asm__ volatile ("nop\n\tnop\n\tnop\n\tnop");
}

static void clock_low(void)
{
	gpio_clear(LCD_CLOCK_PORT, LCD_CLOCK_PIN);
}

static void clock_high(void)
{
	gpio_set(LCD_CLOCK_PORT, LCD_CLOCK_PIN);
}

static void data_low(void)
{
	gpio_clear(LCD_DATA_PORT, LCD_DATA_PIN);
}

static void data_high(void)
{
	gpio_set(LCD_DATA_PORT, LCD_DATA_PIN);
}

static void bus_write_bit(bool high)
{
	if (high) {
		data_high();
	} else {
		data_low();
	}
	clock_high();
	bus_pause();
	clock_low();
	bus_pause();
}

static void bus_ack_clock(void)
{
	clock_high();
	bus_pause();
	clock_low();
	bus_pause();
}

static void bus_start(bool data_mode)
{
	data_high();
	clock_high();
	bus_pause();
	data_low();
	bus_pause();
	clock_low();

	bus_write_bit(false);
	bus_write_bit(true);
	bus_write_bit(true);
	bus_write_bit(true);
	bus_write_bit(true);
	bus_write_bit(false);
	bus_write_bit(data_mode);
	bus_write_bit(false);
	bus_ack_clock();
}

static void bus_send(uint8_t value)
{
	uint8_t bit;

	for (bit = 0U; bit < 8U; bit++) {
		bus_write_bit((value & 0x80U) != 0U);
		value <<= 1U;
	}
	bus_ack_clock();
}

static void bus_stop(void)
{
	data_low();
	clock_low();
	bus_pause();
	clock_high();
	bus_pause();
	data_high();
	bus_pause();
}

static void write_control(const uint8_t *values, size_t count)
{
	size_t index;

	bus_start(false);
	for (index = 0U; index < count; index++) {
		bus_send(values[index]);
	}
	bus_stop();
}

static void write_data(uint8_t value)
{
	bus_start(true);
	bus_send(value);
	bus_stop();
}

static void write_control_byte(uint8_t value)
{
	write_control(&value, 1U);
}

static void lcd_delay_ms(uint32_t duration_ms)
{
	uint32_t started_ms = system_time_millis();

	while ((uint32_t)(system_time_millis() - started_ms) < duration_ms) {
		watchdog_kick();
	}
}

static void controller_init(void)
{
	static const uint8_t pump_setup[] = {0x2DU, 0x24U, 0xEBU};
	static const uint8_t display_setup[] = {
		0x40U, 0x50U, 0x86U, 0x89U, 0xC4U, 0xA3U, 0x95U
	};

	lcd_delay_ms(10U);
	gpio_set(LCD_RESET_PORT, LCD_RESET_PIN);
	lcd_delay_ms(2U);
	gpio_clear(LCD_RESET_PORT, LCD_RESET_PIN);
	lcd_delay_ms(20U);
	gpio_set(LCD_RESET_PORT, LCD_RESET_PIN);
	lcd_delay_ms(40U);

	write_control_byte(0xE1U);
	write_data(0xE2U);
	lcd_delay_ms(2U);
	write_control_byte(0x04U);
	write_data(0x00U);
	write_control(pump_setup, sizeof(pump_setup));
	write_control_byte(0x81U);
	write_data(85U);
	write_control(display_setup, sizeof(display_setup));
	write_control_byte(0xC8U);
	write_data(57U);
	write_control_byte(0xF1U);
	write_data(127U);
	write_control_byte(0xF2U);
	write_data(0U);
	write_control_byte(0xF3U);
	write_data(127U);
	write_control_byte(0x85U);
	write_control_byte(0x95U);
	write_control_byte(0xC9U);
	write_data(0xADU);
}

static void set_address(uint16_t page, uint8_t column)
{
	uint8_t page_commands[2];

	write_control_byte(0x04U);
	write_data(column);
	page_commands[0] = (uint8_t)((page & 0xFFU) | 0x60U);
	page_commands[1] = (uint8_t)(((page >> 8U) & 0x03U) | 0x70U);
	write_control(page_commands, sizeof(page_commands));
	write_control_byte(0xF9U);
}

static void refresh(void)
{
	uint16_t column_group;
	uint8_t page;
	uint8_t offset;

	for (column_group = 0U;
	     column_group < (LCD_TRANSFER_COLUMNS / 8U);
	     column_group++) {
		for (page = 0U; page < LCD_PAGES; page++) {
			set_address((uint16_t)(32U + page),
			            (uint8_t)(column_group * 8U));
			for (offset = 0U; offset < 8U; offset++) {
				uint16_t column = (uint16_t)(column_group * 8U + offset);
				uint8_t value = 0U;

				if (column < LCD_WIDTH) {
					value = framebuffer[column][page];
				}
				write_control_byte(0x01U);
				write_data(value);
			}
			watchdog_kick();
		}
	}
}

static void draw_pixel(uint16_t x, uint16_t y)
{
	if (x < LCD_WIDTH && y < LCD_HEIGHT) {
		framebuffer[x][y / 8U] |= (uint8_t)(1U << (y % 8U));
	}
}

static const uint8_t *glyph_for(char character)
{
	if (character >= '0' && character <= '9') {
		return glyph_digits[(uint8_t)character - (uint8_t)'0'];
	}
	if (character == 'M') {
		return glyph_m;
	}
	if (character == 'A') {
		return glyph_a;
	}
	if (character == '.') {
		return glyph_dot;
	}
	return glyph_blank;
}

static void draw_character(uint16_t x, uint16_t y, char character, uint8_t scale)
{
	const uint8_t *glyph = glyph_for(character);
	uint8_t glyph_x;
	uint8_t glyph_y;
	uint8_t scale_x;
	uint8_t scale_y;

	for (glyph_x = 0U; glyph_x < 5U; glyph_x++) {
		for (glyph_y = 0U; glyph_y < 7U; glyph_y++) {
			if ((glyph[glyph_x] & (1U << glyph_y)) == 0U) {
				continue;
			}
			for (scale_x = 0U; scale_x < scale; scale_x++) {
				for (scale_y = 0U; scale_y < scale; scale_y++) {
					draw_pixel((uint16_t)(x + glyph_x * scale + scale_x),
					           (uint16_t)(y + glyph_y * scale + scale_y));
				}
			}
		}
	}
}

void board_lcd_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOD);
	rcc_periph_clock_enable(RCC_GPIOG);

	gpio_set(LCD_CLOCK_PORT, LCD_CLOCK_PIN);
	gpio_set(LCD_DATA_PORT, LCD_DATA_PIN);
	gpio_set(LCD_RESET_PORT, LCD_RESET_PIN);
	gpio_mode_setup(LCD_CLOCK_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
	                LCD_CLOCK_PIN);
	gpio_mode_setup(LCD_DATA_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
	                LCD_DATA_PIN);
	gpio_mode_setup(LCD_RESET_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
	                LCD_RESET_PIN);
	gpio_set_output_options(LCD_CLOCK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
	                        LCD_CLOCK_PIN);
	gpio_set_output_options(LCD_DATA_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
	                        LCD_DATA_PIN);
	gpio_set_output_options(LCD_RESET_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
	                        LCD_RESET_PIN);

	controller_init();
}

void board_lcd_show_version(const char *version)
{
	uint16_t x = 36U;

	memset(framebuffer, 0, sizeof(framebuffer));
	while (*version != '\0') {
		draw_character(x, 54U, *version, 3U);
		x = (uint16_t)(x + 18U);
		version++;
	}
	refresh();
}
