#ifndef EST_UI_FONT_H
#define EST_UI_FONT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EST_UI_FONT_HEIGHT 16U

typedef enum {
	EST_UI_TEXT_NORMAL = 0,
	EST_UI_TEXT_INVERSE = 1,
	EST_UI_TEXT_COMPACT = 2,
	EST_UI_TEXT_COMPACT_INVERSE = 3
} est_ui_text_style_t;

bool est_ui_font_draw(uint8_t *framebuffer, uint16_t framebuffer_width,
	uint16_t framebuffer_height, uint16_t x, uint16_t y,
	const char *text, est_ui_text_style_t style);
uint16_t est_ui_font_measure(const char *text, est_ui_text_style_t style);

#endif
