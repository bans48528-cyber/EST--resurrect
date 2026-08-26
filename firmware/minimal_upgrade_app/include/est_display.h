#ifndef EST_DISPLAY_H
#define EST_DISPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "est_types.h"

#define EST_DISPLAY_WIDTH 180U
#define EST_DISPLAY_HEIGHT 128U

void est_display_init(void);
void est_display_clear(void);
est_result_t est_display_pixel(uint16_t x, uint16_t y, bool on);
est_result_t est_display_line(uint16_t x0, uint16_t y0,
	uint16_t x1, uint16_t y1, bool on);
est_result_t est_display_rectangle(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, bool filled, bool on);
est_result_t est_display_text(uint16_t x, uint16_t y,
	const char *text, uint8_t scale);
est_result_t est_display_bitmap(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, const uint8_t *bitmap,
	size_t bitmap_size);
void est_display_refresh(void);

#endif
