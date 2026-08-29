#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_lcd_text.h"

#define LCD_TEXT_GLYPH_WIDTH 5U
#define LCD_TEXT_GLYPH_HEIGHT 7U
#define LCD_TEXT_CELL_WIDTH 6U

static const uint8_t glyph_blank[5] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
static const uint8_t glyph_dot[5] = {0x00U, 0x60U, 0x60U, 0x00U, 0x00U};
static const uint8_t glyph_colon[5] = {0x00U, 0x36U, 0x36U, 0x00U, 0x00U};
static const uint8_t glyph_dash[5] = {0x08U, 0x08U, 0x08U, 0x08U, 0x08U};
static const uint8_t glyph_percent[5] = {0x63U, 0x13U, 0x08U, 0x64U, 0x63U};
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
static const uint8_t glyph_letters[26][5] = {
	{0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU},
	{0x7FU, 0x49U, 0x49U, 0x49U, 0x36U},
	{0x3EU, 0x41U, 0x41U, 0x41U, 0x22U},
	{0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU},
	{0x7FU, 0x49U, 0x49U, 0x49U, 0x41U},
	{0x7FU, 0x09U, 0x09U, 0x09U, 0x01U},
	{0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU},
	{0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU},
	{0x00U, 0x41U, 0x7FU, 0x41U, 0x00U},
	{0x20U, 0x40U, 0x41U, 0x3FU, 0x01U},
	{0x7FU, 0x08U, 0x14U, 0x22U, 0x41U},
	{0x7FU, 0x40U, 0x40U, 0x40U, 0x40U},
	{0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU},
	{0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU},
	{0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU},
	{0x7FU, 0x09U, 0x09U, 0x09U, 0x06U},
	{0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU},
	{0x7FU, 0x09U, 0x19U, 0x29U, 0x46U},
	{0x46U, 0x49U, 0x49U, 0x49U, 0x31U},
	{0x01U, 0x01U, 0x7FU, 0x01U, 0x01U},
	{0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU},
	{0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU},
	{0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU},
	{0x63U, 0x14U, 0x08U, 0x14U, 0x63U},
	{0x07U, 0x08U, 0x70U, 0x08U, 0x07U},
	{0x61U, 0x51U, 0x49U, 0x45U, 0x43U},
};

static void set_pixel(uint8_t *framebuffer, uint16_t width, uint16_t height,
	size_t x, size_t y, bool on)
{
	size_t pages;
	size_t index;
	uint8_t bit;

	if (x >= width || y >= height) {
		return;
	}
	pages = ((size_t)height + 7U) / 8U;
	index = x * pages + y / 8U;
	bit = (uint8_t)(1U << (y % 8U));
	if (on) {
		framebuffer[index] |= bit;
	} else {
		framebuffer[index] &= (uint8_t)~bit;
	}
}

static const uint8_t *glyph_for(char character)
{
	if (character >= '0' && character <= '9') {
		return glyph_digits[(uint8_t)character - (uint8_t)'0'];
	}
	if (character >= 'A' && character <= 'Z') {
		return glyph_letters[(uint8_t)character - (uint8_t)'A'];
	}
	if (character >= 'a' && character <= 'z') {
		return glyph_letters[(uint8_t)character - (uint8_t)'a'];
	}
	if (character == '.') {
		return glyph_dot;
	}
	if (character == ':') {
		return glyph_colon;
	}
	if (character == '-') {
		return glyph_dash;
	}
	if (character == '%') {
		return glyph_percent;
	}
	return glyph_blank;
}

static void fill_region(uint8_t *framebuffer, uint16_t width,
	uint16_t height, size_t x, size_t y, size_t region_width,
	size_t region_height, bool on)
{
	size_t offset_x;
	size_t offset_y;

	for (offset_y = 0U;
	     offset_y < region_height && y + offset_y < height;
	     offset_y++) {
		for (offset_x = 0U;
		     offset_x < region_width && x + offset_x < width;
		     offset_x++) {
			set_pixel(framebuffer, width, height,
				x + offset_x, y + offset_y, on);
		}
	}
}

static void fill_text_background(uint8_t *framebuffer, uint16_t width,
	uint16_t height, uint16_t x, uint16_t y, size_t text_width,
	size_t text_height, uint8_t padding, bool on)
{
	size_t start_x = x >= padding ? (size_t)x - padding : 0U;
	size_t start_y = y >= padding ? (size_t)y - padding : 0U;
	size_t end_x = (size_t)x + text_width + padding;
	size_t end_y = (size_t)y + text_height + padding;

	if (end_x > width) {
		end_x = width;
	}
	if (end_y > height) {
		end_y = height;
	}
	fill_region(framebuffer, width, height, start_x, start_y,
		end_x - start_x, end_y - start_y, on);
}

static void draw_character(uint8_t *framebuffer, uint16_t width,
	uint16_t height, size_t x, size_t y, char character, uint8_t scale,
	bool bold, bool foreground_on)
{
	const uint8_t *glyph = glyph_for(character);
	uint8_t glyph_x;
	uint8_t glyph_y;
	uint8_t scale_x;
	uint8_t scale_y;

	for (glyph_x = 0U; glyph_x < LCD_TEXT_GLYPH_WIDTH; glyph_x++) {
		for (glyph_y = 0U; glyph_y < LCD_TEXT_GLYPH_HEIGHT; glyph_y++) {
			if ((glyph[glyph_x] & (1U << glyph_y)) == 0U) {
				continue;
			}
			for (scale_x = 0U; scale_x < scale; scale_x++) {
				for (scale_y = 0U; scale_y < scale; scale_y++) {
					size_t pixel_x = x + (size_t)glyph_x * scale +
						scale_x;
					size_t pixel_y = y + (size_t)glyph_y * scale +
						scale_y;

					set_pixel(framebuffer, width, height, pixel_x,
						pixel_y, foreground_on);
					if (bold) {
						set_pixel(framebuffer, width, height,
							pixel_x + 1U, pixel_y, foreground_on);
					}
				}
			}
		}
	}
}

static void draw_text(uint8_t *framebuffer, uint16_t width, uint16_t height,
	uint16_t x, uint16_t y, const char *text, uint8_t scale, bool bold,
	bool foreground_on)
{
	size_t current_x = x;

	while (*text != '\0' && current_x < width) {
		draw_character(framebuffer, width, height, current_x, y, *text,
			scale, bold, foreground_on);
		current_x += (size_t)LCD_TEXT_CELL_WIDTH * scale;
		text++;
	}
}

bool board_lcd_text_draw_legacy(uint8_t *framebuffer,
	uint16_t framebuffer_width, uint16_t framebuffer_height,
	uint16_t x, uint16_t y, const char *text, uint8_t scale)
{
	if (framebuffer == NULL || text == NULL || scale == 0U || scale > 4U ||
	    x >= framebuffer_width || y >= framebuffer_height ||
	    (uint16_t)(LCD_TEXT_GLYPH_HEIGHT * scale) >
		framebuffer_height - y) {
		return false;
	}
	draw_text(framebuffer, framebuffer_width, framebuffer_height,
		x, y, text, scale, false, true);
	return true;
}

bool board_lcd_text_draw_style(uint8_t *framebuffer,
	uint16_t framebuffer_width, uint16_t framebuffer_height,
	uint16_t x, uint16_t y, const char *text,
	board_lcd_text_style_t style)
{
	uint8_t scale;
	bool bold;
	bool foreground_on;
	size_t length;
	size_t region_width;
	uint8_t background_padding;

	if (framebuffer == NULL || text == NULL || x >= framebuffer_width ||
	    y >= framebuffer_height || style > BOARD_LCD_TEXT_LARGE_WHITE) {
		return false;
	}
	scale = (style == BOARD_LCD_TEXT_LARGE_BLACK ||
		style == BOARD_LCD_TEXT_LARGE_WHITE) ? 2U : 1U;
	bold = style == BOARD_LCD_TEXT_BOLD_BLACK ||
		style == BOARD_LCD_TEXT_BOLD_WHITE;
	foreground_on = style <= BOARD_LCD_TEXT_LARGE_BLACK;
	length = strlen(text);
	if (length == 0U) {
		return true;
	}
	region_width = length * LCD_TEXT_CELL_WIDTH * scale;
	if (!bold) {
		region_width -= scale;
	}
	background_padding = foreground_on ? 0U : scale;
	fill_text_background(framebuffer, framebuffer_width, framebuffer_height,
		x, y, region_width, LCD_TEXT_GLYPH_HEIGHT * scale,
		background_padding, !foreground_on);
	draw_text(framebuffer, framebuffer_width, framebuffer_height,
		x, y, text, scale, bold, foreground_on);
	return true;
}
