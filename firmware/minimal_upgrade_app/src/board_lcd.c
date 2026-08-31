#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "board_lcd.h"
#include "board_lcd_image.h"
#include "board_lcd_text.h"
#include "est_ui_font.h"
#include "system_time.h"
#include "watchdog.h"

#define LCD_WIDTH BOARD_LCD_WIDTH
#define LCD_HEIGHT BOARD_LCD_HEIGHT
#define LCD_PAGES (LCD_HEIGHT / 8U)
#define LCD_TEXT_CELL_WIDTH 6U
#define LCD_MOTOR_COLUMN_X 72U

_Static_assert(LCD_MOTOR_COLUMN_X +
	BOARD_LCD_MOTOR_LINE_CHARACTERS * LCD_TEXT_CELL_WIDTH <= LCD_WIDTH,
	"motor display column exceeds the LCD width");
#define LCD_TRANSFER_COLUMNS 184U

#define LCD_CLOCK_PORT GPIOD
#define LCD_CLOCK_PIN GPIO14
#define LCD_DATA_PORT GPIOG
#define LCD_DATA_PIN GPIO2
#define LCD_RESET_PORT GPIOD
#define LCD_RESET_PIN GPIO15

/*
 * Some production LCD modules need substantially more time than the first
 * development unit before accepting UC1638 commands.  Keep these delays near
 * the conservative sequence used by the original EST firmware so a cold LCD
 * is fully powered before its controller is configured.
 */
#define LCD_RESET_IDLE_MS 50U
#define LCD_RESET_ASSERT_MS 30U
#define LCD_RESET_RELEASE_MS 250U
#define LCD_CONTROLLER_SETTLE_MS 100U
#define LCD_INIT_WATCHDOG_BUDGET_MS 2000U
#define LCD_REFRESH_WATCHDOG_BUDGET_MS 1000U

static uint8_t framebuffer[LCD_WIDTH][LCD_PAGES];

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

static void lcd_delay_ms(uint32_t duration_ms, watchdog_guard_t *guard)
{
	uint32_t started_ms = system_time_millis();

	while ((uint32_t)(system_time_millis() - started_ms) < duration_ms) {
		(void)watchdog_guard_progress(guard, system_time_millis());
	}
}

static void controller_init(void)
{
	static const uint8_t pump_setup[] = {0x2DU, 0x24U, 0xEBU};
	static const uint8_t display_setup[] = {
		0x40U, 0x50U, 0x86U, 0x89U, 0xC4U, 0xA3U, 0x95U
	};
	watchdog_guard_t guard;

	watchdog_guard_begin(&guard, system_time_millis(),
		LCD_INIT_WATCHDOG_BUDGET_MS);

	gpio_set(LCD_RESET_PORT, LCD_RESET_PIN);
	lcd_delay_ms(LCD_RESET_IDLE_MS, &guard);
	gpio_clear(LCD_RESET_PORT, LCD_RESET_PIN);
	lcd_delay_ms(LCD_RESET_ASSERT_MS, &guard);
	gpio_set(LCD_RESET_PORT, LCD_RESET_PIN);
	lcd_delay_ms(LCD_RESET_RELEASE_MS, &guard);

	write_control_byte(0xE1U);
	write_data(0xE2U);
	lcd_delay_ms(2U, &guard);
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
	lcd_delay_ms(LCD_CONTROLLER_SETTLE_MS, &guard);
	watchdog_guard_end(&guard);
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

static void refresh_framebuffer(board_lcd_refresh_hook_t hook)
{
	uint16_t column_group;
	uint8_t page;
	uint8_t offset;
	watchdog_guard_t guard;

	watchdog_guard_begin(&guard, system_time_millis(),
		LCD_REFRESH_WATCHDOG_BUDGET_MS);

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
			(void)watchdog_guard_progress(&guard, system_time_millis());
			if (hook != NULL) {
				hook();
			}
		}
	}
	watchdog_guard_end(&guard);
}

static void framebuffer_set_pixel(uint16_t x, uint16_t y, bool on)
{
	if (x < LCD_WIDTH && y < LCD_HEIGHT) {
		uint8_t bit = (uint8_t)(1U << (y % 8U));

		if (on) {
			framebuffer[x][y / 8U] |= bit;
		} else {
			framebuffer[x][y / 8U] &= (uint8_t)~bit;
		}
	}
}

static void draw_text_at(uint16_t x, uint16_t y, const char *text,
	uint8_t scale)
{
	(void)board_lcd_text_draw_legacy(&framebuffer[0][0], LCD_WIDTH,
		LCD_HEIGHT, x, y, text, scale);
}

static void draw_text_centered(uint16_t y, const char *text, uint8_t scale)
{
	size_t length = strlen(text);
	uint16_t width;
	uint16_t x;

	if (length == 0U) {
		return;
	}
	width = (uint16_t)(((length * LCD_TEXT_CELL_WIDTH) - 1U) * scale);
	x = width < LCD_WIDTH ? (uint16_t)((LCD_WIDTH - width) / 2U) : 0U;
	draw_text_at(x, y, text, scale);
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

void board_lcd_clear(void)
{
	memset(framebuffer, 0, sizeof(framebuffer));
}

bool board_lcd_set_pixel(uint16_t x, uint16_t y, bool on)
{
	if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
		return false;
	}
	framebuffer_set_pixel(x, y, on);
	return true;
}

bool board_lcd_draw_line(uint16_t x0, uint16_t y0,
	uint16_t x1, uint16_t y1, bool on)
{
	int32_t current_x = x0;
	int32_t current_y = y0;
	int32_t dx;
	int32_t dy;
	int32_t step_x;
	int32_t step_y;
	int32_t error;

	if (x0 >= LCD_WIDTH || x1 >= LCD_WIDTH ||
	    y0 >= LCD_HEIGHT || y1 >= LCD_HEIGHT) {
		return false;
	}
	dx = current_x < (int32_t)x1 ? (int32_t)x1 - current_x :
		current_x - (int32_t)x1;
	dy = current_y < (int32_t)y1 ? current_y - (int32_t)y1 :
		(int32_t)y1 - current_y;
	step_x = current_x < (int32_t)x1 ? 1 : -1;
	step_y = current_y < (int32_t)y1 ? 1 : -1;
	error = dx + dy;
	while (true) {
		int32_t doubled_error;

		framebuffer_set_pixel((uint16_t)current_x,
			(uint16_t)current_y, on);
		if (current_x == (int32_t)x1 && current_y == (int32_t)y1) {
			break;
		}
		doubled_error = 2 * error;
		if (doubled_error >= dy) {
			error += dy;
			current_x += step_x;
		}
		if (doubled_error <= dx) {
			error += dx;
			current_y += step_y;
		}
	}
	return true;
}

bool board_lcd_draw_rectangle(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, bool filled, bool on)
{
	uint16_t offset_x;
	uint16_t offset_y;

	if (width == 0U || height == 0U || x >= LCD_WIDTH || y >= LCD_HEIGHT ||
	    width > LCD_WIDTH - x || height > LCD_HEIGHT - y) {
		return false;
	}
	for (offset_y = 0U; offset_y < height; offset_y++) {
		for (offset_x = 0U; offset_x < width; offset_x++) {
			if (filled || offset_x == 0U || offset_x == width - 1U ||
			    offset_y == 0U || offset_y == height - 1U) {
				framebuffer_set_pixel((uint16_t)(x + offset_x),
					(uint16_t)(y + offset_y), on);
			}
		}
	}
	return true;
}

bool board_lcd_draw_text(uint16_t x, uint16_t y,
	const char *text, uint8_t scale)
{
	if (text == NULL || scale == 0U || scale > 4U ||
	    x >= LCD_WIDTH || y >= LCD_HEIGHT ||
	    (uint16_t)(7U * scale) > LCD_HEIGHT - y) {
		return false;
	}
	draw_text_at(x, y, text, scale);
	return true;
}

bool board_lcd_draw_text_style(uint16_t x, uint16_t y,
	const char *text, board_lcd_text_style_t style)
{
	return board_lcd_text_draw_style(&framebuffer[0][0], LCD_WIDTH,
		LCD_HEIGHT, x, y, text, style);
}

bool board_lcd_draw_ui_text(uint16_t x, uint16_t y,
	const char *text, est_ui_text_style_t style)
{
	return est_ui_font_draw(&framebuffer[0][0], LCD_WIDTH, LCD_HEIGHT,
		x, y, text, style);
}

bool board_lcd_draw_bitmap(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, const uint8_t *bitmap,
	size_t bitmap_size)
{
	size_t stride;
	uint16_t bitmap_x;
	uint16_t bitmap_y;

	if (bitmap == NULL || width == 0U || height == 0U ||
	    x >= LCD_WIDTH || y >= LCD_HEIGHT || width > LCD_WIDTH - x ||
	    height > LCD_HEIGHT - y) {
		return false;
	}
	stride = ((size_t)width + 7U) / 8U;
	if (bitmap_size < stride * height) {
		return false;
	}
	for (bitmap_y = 0U; bitmap_y < height; bitmap_y++) {
		for (bitmap_x = 0U; bitmap_x < width; bitmap_x++) {
			bool on = (bitmap[(size_t)bitmap_y * stride + bitmap_x / 8U] &
				(uint8_t)(0x80U >> (bitmap_x % 8U))) != 0U;

			framebuffer_set_pixel((uint16_t)(x + bitmap_x),
				(uint16_t)(y + bitmap_y), on);
		}
	}
	return true;
}

bool board_lcd_draw_image(const char *name)
{
	const board_lcd_image_resource_t *resource =
		board_lcd_image_find(name);
	uint16_t x;
	uint16_t y;

	if (resource == NULL || resource->width > LCD_WIDTH ||
	    resource->height > LCD_HEIGHT) {
		return false;
	}
	x = (uint16_t)((LCD_WIDTH - resource->width) / 2U);
	y = (uint16_t)((LCD_HEIGHT - resource->height) / 2U);
	return board_lcd_image_draw(&framebuffer[0][0], LCD_WIDTH, LCD_HEIGHT,
		x, y, resource);
}

void board_lcd_refresh(void)
{
	refresh_framebuffer(NULL);
}

void board_lcd_refresh_with_hook(board_lcd_refresh_hook_t hook)
{
	refresh_framebuffer(hook);
}

void board_lcd_show_version(const char *version)
{
	memset(framebuffer, 0, sizeof(framebuffer));
	draw_text_at(36U, 54U, version, 3U);
	refresh_framebuffer(NULL);
}

void board_lcd_show_sensor(const char *version, const char *mode,
	const char *reading)
{
	size_t reading_length = strlen(reading);
	uint8_t reading_scale = reading_length <= 5U ? 4U : 3U;

	memset(framebuffer, 0, sizeof(framebuffer));
	draw_text_centered(2U, version, 2U);
	draw_text_centered(27U, mode, 2U);
	draw_text_centered(58U, reading, reading_scale);
	draw_text_centered(112U, "ANY KEY", 1U);
	refresh_framebuffer(NULL);
}

void board_lcd_show_sensor_ports(const char *version, const char *mode,
	const char *const readings[4])
{
	uint8_t index;

	memset(framebuffer, 0, sizeof(framebuffer));
	draw_text_centered(1U, version, 2U);
	draw_text_centered(19U, mode, 1U);
	for (index = 0U; index < 4U; index++) {
		draw_text_centered((uint16_t)(33U + index * 20U),
			readings[index], 2U);
	}
	draw_text_centered(116U, "KEY MODE", 1U);
	refresh_framebuffer(NULL);
}

void board_lcd_show_io_ports(const char *version, const char *mode,
	const char *const sensor_readings[4],
	const char *const motor_readings[4], const char *status)
{
	uint8_t index;

	memset(framebuffer, 0, sizeof(framebuffer));
	draw_text_centered(1U, version, 2U);
	draw_text_at(4U, 19U, mode, 1U);
	draw_text_at(LCD_MOTOR_COLUMN_X, 19U, "MTR S% DEG REV", 1U);
	for (index = 0U; index < 4U; index++) {
		draw_text_at(4U, (uint16_t)(35U + index * 19U),
			sensor_readings[index], 1U);
		draw_text_at(LCD_MOTOR_COLUMN_X, (uint16_t)(35U + index * 19U),
			motor_readings[index], 1U);
	}
	draw_text_centered(116U, status, 1U);
	refresh_framebuffer(NULL);
}
