#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "board_lcd.h"
#include "est_display.h"

void est_display_init(void)
{
	board_lcd_init();
}

void est_display_clear(void)
{
	board_lcd_clear();
}

est_result_t est_display_pixel(uint16_t x, uint16_t y, bool on)
{
	return board_lcd_set_pixel(x, y, on) ? EST_OK :
		EST_ERR_INVALID_ARGUMENT;
}

est_result_t est_display_line(uint16_t x0, uint16_t y0,
	uint16_t x1, uint16_t y1, bool on)
{
	return board_lcd_draw_line(x0, y0, x1, y1, on) ? EST_OK :
		EST_ERR_INVALID_ARGUMENT;
}

est_result_t est_display_rectangle(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, bool filled, bool on)
{
	return board_lcd_draw_rectangle(x, y, width, height, filled, on) ?
		EST_OK : EST_ERR_INVALID_ARGUMENT;
}

est_result_t est_display_text(uint16_t x, uint16_t y,
	const char *text, uint8_t scale)
{
	return board_lcd_draw_text(x, y, text, scale) ? EST_OK :
		EST_ERR_INVALID_ARGUMENT;
}

est_result_t est_display_text_font(uint16_t x, uint16_t y,
	const char *text, est_display_font_t font)
{
	board_lcd_text_style_t style;

	switch (font) {
	case EST_DISPLAY_FONT_REGULAR_BLACK:
		style = BOARD_LCD_TEXT_REGULAR_BLACK;
		break;
	case EST_DISPLAY_FONT_BOLD_BLACK:
		style = BOARD_LCD_TEXT_BOLD_BLACK;
		break;
	case EST_DISPLAY_FONT_LARGE_BLACK:
		style = BOARD_LCD_TEXT_LARGE_BLACK;
		break;
	case EST_DISPLAY_FONT_REGULAR_WHITE:
		style = BOARD_LCD_TEXT_REGULAR_WHITE;
		break;
	case EST_DISPLAY_FONT_BOLD_WHITE:
		style = BOARD_LCD_TEXT_BOLD_WHITE;
		break;
	case EST_DISPLAY_FONT_LARGE_WHITE:
		style = BOARD_LCD_TEXT_LARGE_WHITE;
		break;
	default:
		return EST_ERR_INVALID_ARGUMENT;
	}
	return board_lcd_draw_text_style(x, y, text, style) ? EST_OK :
		EST_ERR_INVALID_ARGUMENT;
}

est_result_t est_display_bitmap(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, const uint8_t *bitmap,
	size_t bitmap_size)
{
	return board_lcd_draw_bitmap(x, y, width, height, bitmap,
		bitmap_size) ? EST_OK : EST_ERR_INVALID_ARGUMENT;
}

est_result_t est_display_image(const char *name)
{
	return board_lcd_draw_image(name) ? EST_OK : EST_ERR_INVALID_ARGUMENT;
}

void est_display_refresh(void)
{
	board_lcd_refresh();
}

void est_display_refresh_with_hook(est_display_refresh_hook_t hook)
{
	board_lcd_refresh_with_hook(hook);
}
