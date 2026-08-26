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

est_result_t est_display_bitmap(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, const uint8_t *bitmap,
	size_t bitmap_size)
{
	return board_lcd_draw_bitmap(x, y, width, height, bitmap,
		bitmap_size) ? EST_OK : EST_ERR_INVALID_ARGUMENT;
}

void est_display_refresh(void)
{
	board_lcd_refresh();
}
