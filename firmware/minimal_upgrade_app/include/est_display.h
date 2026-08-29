#ifndef EST_DISPLAY_H
#define EST_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "est_types.h"

#define EST_DISPLAY_WIDTH 180U
#define EST_DISPLAY_HEIGHT 128U

typedef void (*est_display_refresh_hook_t)(void);

typedef enum {
	EST_DISPLAY_FONT_REGULAR_BLACK = 0,
	EST_DISPLAY_FONT_BOLD_BLACK = 1,
	EST_DISPLAY_FONT_LARGE_BLACK = 2,
	EST_DISPLAY_FONT_REGULAR_WHITE = 3,
	EST_DISPLAY_FONT_BOLD_WHITE = 4,
	EST_DISPLAY_FONT_LARGE_WHITE = 5
} est_display_font_t;

void est_display_init(void);
void est_display_clear(void);
est_result_t est_display_pixel(uint16_t x, uint16_t y, bool on);
est_result_t est_display_line(uint16_t x0, uint16_t y0,
	uint16_t x1, uint16_t y1, bool on);
est_result_t est_display_rectangle(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, bool filled, bool on);
est_result_t est_display_text(uint16_t x, uint16_t y,
	const char *text, uint8_t scale);
est_result_t est_display_text_font(uint16_t x, uint16_t y,
	const char *text, est_display_font_t font);
est_result_t est_display_bitmap(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, const uint8_t *bitmap,
	size_t bitmap_size);
est_result_t est_display_image(const char *name);
void est_display_refresh(void);
void est_display_refresh_with_hook(est_display_refresh_hook_t hook);

#endif
