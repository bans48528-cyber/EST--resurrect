#ifndef BOARD_LCD_TEXT_H
#define BOARD_LCD_TEXT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	BOARD_LCD_TEXT_REGULAR_BLACK = 0,
	BOARD_LCD_TEXT_BOLD_BLACK = 1,
	BOARD_LCD_TEXT_LARGE_BLACK = 2,
	BOARD_LCD_TEXT_REGULAR_WHITE = 3,
	BOARD_LCD_TEXT_BOLD_WHITE = 4,
	BOARD_LCD_TEXT_LARGE_WHITE = 5
} board_lcd_text_style_t;

bool board_lcd_text_draw_legacy(uint8_t *framebuffer,
	uint16_t framebuffer_width, uint16_t framebuffer_height,
	uint16_t x, uint16_t y, const char *text, uint8_t scale);
bool board_lcd_text_draw_style(uint8_t *framebuffer,
	uint16_t framebuffer_width, uint16_t framebuffer_height,
	uint16_t x, uint16_t y, const char *text,
	board_lcd_text_style_t style);

#endif
