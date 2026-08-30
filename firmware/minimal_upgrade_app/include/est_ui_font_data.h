#ifndef EST_UI_FONT_DATA_H
#define EST_UI_FONT_DATA_H

#include <stddef.h>
#include <stdint.h>

#define EST_UI_FONT_CJK_WIDTH 16U
#define EST_UI_FONT_CJK_HEIGHT 16U

typedef struct {
	uint32_t codepoint;
	uint16_t rows[EST_UI_FONT_CJK_HEIGHT];
} est_ui_font_glyph_t;

extern const est_ui_font_glyph_t est_ui_font_glyphs[];
extern const size_t est_ui_font_glyph_count;

#endif
