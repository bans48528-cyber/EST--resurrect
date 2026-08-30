#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_lcd_text.h"
#include "est_ui_font.h"
#include "est_ui_font_data.h"

#define ASCII_GLYPH_WIDTH 5U
#define ASCII_CELL_WIDTH 7U
#define ASCII_COMPACT_CELL_WIDTH 6U
#define ASCII_GLYPH_Y 4U
#define ASCII_TEMP_HEIGHT 16U
#define ASCII_TEMP_PAGES 2U

typedef enum {
	LATIN_ACCENT_ACUTE = 0,
	LATIN_ACCENT_GRAVE,
	LATIN_ACCENT_CIRCUMFLEX,
	LATIN_ACCENT_TILDE,
	LATIN_ACCENT_CEDILLA,
	LATIN_ACCENT_DIAERESIS
} latin_accent_t;

typedef struct {
	uint32_t codepoint;
	char base;
	latin_accent_t accent;
} latin_accent_glyph_t;

static const latin_accent_glyph_t latin_accent_glyphs[] = {
	{0x00C0U, 'A', LATIN_ACCENT_GRAVE},
	{0x00C1U, 'A', LATIN_ACCENT_ACUTE},
	{0x00C2U, 'A', LATIN_ACCENT_CIRCUMFLEX},
	{0x00C3U, 'A', LATIN_ACCENT_TILDE},
	{0x00C4U, 'A', LATIN_ACCENT_DIAERESIS},
	{0x00C7U, 'C', LATIN_ACCENT_CEDILLA},
	{0x00C8U, 'E', LATIN_ACCENT_GRAVE},
	{0x00C9U, 'E', LATIN_ACCENT_ACUTE},
	{0x00CAU, 'E', LATIN_ACCENT_CIRCUMFLEX},
	{0x00CBU, 'E', LATIN_ACCENT_DIAERESIS},
	{0x00CCU, 'I', LATIN_ACCENT_GRAVE},
	{0x00CDU, 'I', LATIN_ACCENT_ACUTE},
	{0x00CEU, 'I', LATIN_ACCENT_CIRCUMFLEX},
	{0x00CFU, 'I', LATIN_ACCENT_DIAERESIS},
	{0x00D2U, 'O', LATIN_ACCENT_GRAVE},
	{0x00D3U, 'O', LATIN_ACCENT_ACUTE},
	{0x00D4U, 'O', LATIN_ACCENT_CIRCUMFLEX},
	{0x00D5U, 'O', LATIN_ACCENT_TILDE},
	{0x00D6U, 'O', LATIN_ACCENT_DIAERESIS},
	{0x00D9U, 'U', LATIN_ACCENT_GRAVE},
	{0x00DAU, 'U', LATIN_ACCENT_ACUTE},
	{0x00DBU, 'U', LATIN_ACCENT_CIRCUMFLEX},
	{0x00DCU, 'U', LATIN_ACCENT_DIAERESIS},
	{0x00E0U, 'a', LATIN_ACCENT_GRAVE},
	{0x00E1U, 'a', LATIN_ACCENT_ACUTE},
	{0x00E2U, 'a', LATIN_ACCENT_CIRCUMFLEX},
	{0x00E3U, 'a', LATIN_ACCENT_TILDE},
	{0x00E4U, 'a', LATIN_ACCENT_DIAERESIS},
	{0x00E7U, 'c', LATIN_ACCENT_CEDILLA},
	{0x00E8U, 'e', LATIN_ACCENT_GRAVE},
	{0x00E9U, 'e', LATIN_ACCENT_ACUTE},
	{0x00EAU, 'e', LATIN_ACCENT_CIRCUMFLEX},
	{0x00EBU, 'e', LATIN_ACCENT_DIAERESIS},
	{0x00ECU, 'i', LATIN_ACCENT_GRAVE},
	{0x00EDU, 'i', LATIN_ACCENT_ACUTE},
	{0x00EEU, 'i', LATIN_ACCENT_CIRCUMFLEX},
	{0x00EFU, 'i', LATIN_ACCENT_DIAERESIS},
	{0x00F2U, 'o', LATIN_ACCENT_GRAVE},
	{0x00F3U, 'o', LATIN_ACCENT_ACUTE},
	{0x00F4U, 'o', LATIN_ACCENT_CIRCUMFLEX},
	{0x00F5U, 'o', LATIN_ACCENT_TILDE},
	{0x00F6U, 'o', LATIN_ACCENT_DIAERESIS},
	{0x00F9U, 'u', LATIN_ACCENT_GRAVE},
	{0x00FAU, 'u', LATIN_ACCENT_ACUTE},
	{0x00FBU, 'u', LATIN_ACCENT_CIRCUMFLEX},
	{0x00FCU, 'u', LATIN_ACCENT_DIAERESIS},
};

static bool style_valid(est_ui_text_style_t style)
{
	return style <= EST_UI_TEXT_COMPACT_INVERSE;
}

static bool style_inverse(est_ui_text_style_t style)
{
	return style == EST_UI_TEXT_INVERSE ||
		style == EST_UI_TEXT_COMPACT_INVERSE;
}

static uint8_t ascii_cell_width(est_ui_text_style_t style)
{
	return style == EST_UI_TEXT_COMPACT ||
		style == EST_UI_TEXT_COMPACT_INVERSE ?
		ASCII_COMPACT_CELL_WIDTH : ASCII_CELL_WIDTH;
}

static const latin_accent_glyph_t *find_latin_accent_glyph(uint32_t codepoint)
{
	size_t index;

	for (index = 0U;
	     index < sizeof(latin_accent_glyphs) /
		     sizeof(latin_accent_glyphs[0]);
	     index++) {
		if (latin_accent_glyphs[index].codepoint == codepoint) {
			return &latin_accent_glyphs[index];
		}
	}
	return NULL;
}

static bool latin_symbol_supported(uint32_t codepoint)
{
	return codepoint == 0x00AAU || codepoint == 0x00BAU ||
		codepoint == 0x00ABU || codepoint == 0x00BBU ||
		codepoint == 0x00B0U || codepoint == 0x20ACU;
}

static bool combining_accent(uint32_t codepoint, latin_accent_t *accent)
{
	switch (codepoint) {
	case 0x0300U:
		*accent = LATIN_ACCENT_GRAVE;
		return true;
	case 0x0301U:
		*accent = LATIN_ACCENT_ACUTE;
		return true;
	case 0x0302U:
		*accent = LATIN_ACCENT_CIRCUMFLEX;
		return true;
	case 0x0303U:
		*accent = LATIN_ACCENT_TILDE;
		return true;
	case 0x0308U:
		*accent = LATIN_ACCENT_DIAERESIS;
		return true;
	case 0x0327U:
		*accent = LATIN_ACCENT_CEDILLA;
		return true;
	default:
		return false;
	}
}

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

static bool get_pixel(const uint8_t *framebuffer, uint16_t height,
	uint8_t x, uint8_t y)
{
	size_t pages = ((size_t)height + 7U) / 8U;

	return (framebuffer[(size_t)x * pages + y / 8U] &
		(uint8_t)(1U << (y % 8U))) != 0U;
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
			set_pixel(framebuffer, width, height, x + offset_x,
				y + offset_y, on);
		}
	}
}

static const est_ui_font_glyph_t *find_glyph(uint32_t codepoint)
{
	size_t low = 0U;
	size_t high = est_ui_font_glyph_count;

	while (low < high) {
		size_t middle = low + (high - low) / 2U;

		if (est_ui_font_glyphs[middle].codepoint < codepoint) {
			low = middle + 1U;
		} else {
			high = middle;
		}
	}
	if (low < est_ui_font_glyph_count &&
	    est_ui_font_glyphs[low].codepoint == codepoint) {
		return &est_ui_font_glyphs[low];
	}
	return NULL;
}

static void draw_replacement(uint8_t *framebuffer, uint16_t width,
	uint16_t height, size_t x, size_t y, bool on)
{
	uint8_t index;

	for (index = 2U; index < 14U; index++) {
		set_pixel(framebuffer, width, height, x + index, y + 2U, on);
		set_pixel(framebuffer, width, height, x + index, y + 13U, on);
		set_pixel(framebuffer, width, height, x + 2U, y + index, on);
		set_pixel(framebuffer, width, height, x + 13U, y + index, on);
	}
}

static void draw_cjk(uint8_t *framebuffer, uint16_t width, uint16_t height,
	size_t x, size_t y, uint32_t codepoint, bool on)
{
	const est_ui_font_glyph_t *glyph = find_glyph(codepoint);
	uint8_t row;
	uint8_t column;

	if (glyph == NULL) {
		draw_replacement(framebuffer, width, height, x, y, on);
		return;
	}
	for (row = 0U; row < EST_UI_FONT_CJK_HEIGHT; row++) {
		for (column = 0U; column < EST_UI_FONT_CJK_WIDTH; column++) {
			if ((glyph->rows[row] &
		     (uint16_t)(1U << (15U - column))) != 0U) {
				set_pixel(framebuffer, width, height, x + column,
					y + row, on);
			}
		}
	}
}

static void draw_ascii(uint8_t *framebuffer, uint16_t width,
	uint16_t height, size_t x, size_t y, char character, bool on)
{
	uint8_t glyph_framebuffer[ASCII_GLYPH_WIDTH * ASCII_TEMP_PAGES];
	char text[2] = {character, '\0'};
	uint8_t column;
	uint8_t row;

	memset(glyph_framebuffer, 0, sizeof(glyph_framebuffer));
	(void)board_lcd_text_draw_legacy(glyph_framebuffer,
		ASCII_GLYPH_WIDTH, ASCII_TEMP_HEIGHT, 0U, ASCII_GLYPH_Y,
		text, 1U);
	for (column = 0U; column < ASCII_GLYPH_WIDTH; column++) {
		for (row = 0U; row < ASCII_TEMP_HEIGHT; row++) {
			if (get_pixel(glyph_framebuffer, ASCII_TEMP_HEIGHT,
			    column, row)) {
				set_pixel(framebuffer, width, height, x + column,
					y + row, on);
			}
		}
	}
}

static void draw_latin_accent(uint8_t *framebuffer, uint16_t width,
	uint16_t height, size_t x, size_t y, latin_accent_t accent, bool on)
{
	switch (accent) {
	case LATIN_ACCENT_ACUTE:
		set_pixel(framebuffer, width, height, x + 3U, y, on);
		set_pixel(framebuffer, width, height, x + 2U, y + 1U, on);
		set_pixel(framebuffer, width, height, x + 1U, y + 2U, on);
		break;
	case LATIN_ACCENT_GRAVE:
		set_pixel(framebuffer, width, height, x + 1U, y, on);
		set_pixel(framebuffer, width, height, x + 2U, y + 1U, on);
		set_pixel(framebuffer, width, height, x + 3U, y + 2U, on);
		break;
	case LATIN_ACCENT_CIRCUMFLEX:
		set_pixel(framebuffer, width, height, x + 2U, y, on);
		set_pixel(framebuffer, width, height, x + 1U, y + 1U, on);
		set_pixel(framebuffer, width, height, x + 3U, y + 1U, on);
		break;
	case LATIN_ACCENT_TILDE:
		set_pixel(framebuffer, width, height, x + 1U, y + 1U, on);
		set_pixel(framebuffer, width, height, x + 2U, y, on);
		set_pixel(framebuffer, width, height, x + 3U, y, on);
		set_pixel(framebuffer, width, height, x + 4U, y + 1U, on);
		break;
	case LATIN_ACCENT_CEDILLA:
		set_pixel(framebuffer, width, height, x + 2U, y + 11U, on);
		set_pixel(framebuffer, width, height, x + 3U, y + 12U, on);
		set_pixel(framebuffer, width, height, x + 2U, y + 13U, on);
		set_pixel(framebuffer, width, height, x + 1U, y + 13U, on);
		break;
	case LATIN_ACCENT_DIAERESIS:
		set_pixel(framebuffer, width, height, x + 1U, y + 1U, on);
		set_pixel(framebuffer, width, height, x + 3U, y + 1U, on);
		break;
	}
}

static void draw_latin_symbol(uint8_t *framebuffer, uint16_t width,
	uint16_t height, size_t x, size_t y, uint32_t codepoint, bool on)
{
	uint8_t row;

	if (codepoint == 0x00B0U) {
		set_pixel(framebuffer, width, height, x + 2U, y, on);
		set_pixel(framebuffer, width, height, x + 1U, y + 1U, on);
		set_pixel(framebuffer, width, height, x + 3U, y + 1U, on);
		set_pixel(framebuffer, width, height, x + 2U, y + 2U, on);
	} else if (codepoint == 0x00AAU) {
		set_pixel(framebuffer, width, height, x + 2U, y + 2U, on);
		set_pixel(framebuffer, width, height, x + 3U, y + 2U, on);
		set_pixel(framebuffer, width, height, x + 1U, y + 3U, on);
		set_pixel(framebuffer, width, height, x + 3U, y + 3U, on);
		set_pixel(framebuffer, width, height, x + 2U, y + 4U, on);
		set_pixel(framebuffer, width, height, x + 3U, y + 4U, on);
		for (row = 1U; row < 5U; row++) {
			set_pixel(framebuffer, width, height, x + row, y + 6U, on);
		}
	} else if (codepoint == 0x00BAU) {
		set_pixel(framebuffer, width, height, x + 2U, y + 2U, on);
		set_pixel(framebuffer, width, height, x + 1U, y + 3U, on);
		set_pixel(framebuffer, width, height, x + 3U, y + 3U, on);
		set_pixel(framebuffer, width, height, x + 2U, y + 4U, on);
		for (row = 1U; row < 5U; row++) {
			set_pixel(framebuffer, width, height, x + row, y + 6U, on);
		}
	} else if (codepoint == 0x00ABU || codepoint == 0x00BBU) {
		uint8_t left = codepoint == 0x00ABU ? 0U : 4U;
		int8_t direction = codepoint == 0x00ABU ? 1 : -1;

		for (row = 0U; row < 3U; row++) {
			size_t column = (size_t)((int8_t)left + direction *
				(int8_t)row);

			set_pixel(framebuffer, width, height, x + column,
				y + 5U + row, on);
			set_pixel(framebuffer, width, height, x + column,
				y + 9U - row, on);
		}
	} else if (codepoint == 0x20ACU) {
		for (row = 4U; row <= 10U; row++) {
			set_pixel(framebuffer, width, height, x + 1U, y + row, on);
		}
		for (row = 1U; row < 5U; row++) {
			set_pixel(framebuffer, width, height, x + row, y + 4U, on);
			set_pixel(framebuffer, width, height, x + row, y + 10U, on);
		}
		for (row = 0U; row < 4U; row++) {
			set_pixel(framebuffer, width, height, x + row, y + 6U, on);
			set_pixel(framebuffer, width, height, x + row, y + 8U, on);
		}
	}
}

static void draw_latin(uint8_t *framebuffer, uint16_t width,
	uint16_t height, size_t x, size_t y, uint32_t codepoint, bool on)
{
	const latin_accent_glyph_t *glyph = find_latin_accent_glyph(codepoint);

	if (glyph != NULL) {
		draw_ascii(framebuffer, width, height, x, y, glyph->base, on);
		draw_latin_accent(framebuffer, width, height, x, y,
			glyph->accent, on);
	} else {
		draw_latin_symbol(framebuffer, width, height, x, y,
			codepoint, on);
	}
}

static bool decode_utf8(const uint8_t **cursor, uint32_t *codepoint)
{
	const uint8_t *text = *cursor;
	uint32_t value;
	uint8_t remaining;
	uint8_t index;

	if (*text < 0x80U) {
		*codepoint = *text;
		*cursor = text + 1U;
		return true;
	}
	if ((*text & 0xE0U) == 0xC0U) {
		value = *text & 0x1FU;
		remaining = 1U;
		if (value < 2U) {
			remaining = 0U;
		}
	} else if ((*text & 0xF0U) == 0xE0U) {
		value = *text & 0x0FU;
		remaining = 2U;
	} else if ((*text & 0xF8U) == 0xF0U) {
		value = *text & 0x07U;
		remaining = 3U;
	} else {
		remaining = 0U;
		value = 0U;
	}
	if (remaining == 0U) {
		*codepoint = 0xFFFDU;
		*cursor = text + 1U;
		return false;
	}
	for (index = 1U; index <= remaining; index++) {
		if ((text[index] & 0xC0U) != 0x80U) {
			*codepoint = 0xFFFDU;
			*cursor = text + 1U;
			return false;
		}
		value = (value << 6U) | (text[index] & 0x3FU);
	}
	if ((remaining == 2U && value < 0x800U) ||
	    (remaining == 3U && value < 0x10000U) ||
	    value > 0x10FFFFU ||
	    (value >= 0xD800U && value <= 0xDFFFU)) {
		*codepoint = 0xFFFDU;
		*cursor = text + 1U;
		return false;
	}
	*codepoint = value;
	*cursor = text + remaining + 1U;
	return true;
}

uint16_t est_ui_font_measure(const char *text, est_ui_text_style_t style)
{
	const uint8_t *cursor = (const uint8_t *)text;
	uint32_t width = 0U;

	if (text == NULL || !style_valid(style)) {
		return 0U;
	}
	while (*cursor != '\0') {
		uint32_t codepoint;
		latin_accent_t accent;

		(void)decode_utf8(&cursor, &codepoint);
		if (combining_accent(codepoint, &accent)) {
			continue;
		}
		width += codepoint < 0x80U ||
			find_latin_accent_glyph(codepoint) != NULL ||
			latin_symbol_supported(codepoint) ?
			ascii_cell_width(style) : EST_UI_FONT_CJK_WIDTH;
		if (width > UINT16_MAX) {
			return UINT16_MAX;
		}
	}
	return (uint16_t)width;
}

bool est_ui_font_draw(uint8_t *framebuffer, uint16_t framebuffer_width,
	uint16_t framebuffer_height, uint16_t x, uint16_t y,
	const char *text, est_ui_text_style_t style)
{
	const uint8_t *cursor = (const uint8_t *)text;
	size_t current_x = x;
	bool inverse;

	if (framebuffer == NULL || text == NULL || !style_valid(style) ||
	    x >= framebuffer_width || y >= framebuffer_height) {
		return false;
	}
	inverse = style_inverse(style);
	if (inverse) {
		fill_region(framebuffer, framebuffer_width, framebuffer_height,
			x, y, est_ui_font_measure(text, style),
			EST_UI_FONT_HEIGHT, true);
	}
	while (*cursor != '\0' && current_x < framebuffer_width) {
		uint32_t codepoint;
		latin_accent_t accent;

		(void)decode_utf8(&cursor, &codepoint);
		if (codepoint < 0x80U) {
			draw_ascii(framebuffer, framebuffer_width,
				framebuffer_height, current_x, y,
				(char)codepoint, !inverse);
			current_x += ascii_cell_width(style);
		} else if (find_latin_accent_glyph(codepoint) != NULL ||
			   latin_symbol_supported(codepoint)) {
			draw_latin(framebuffer, framebuffer_width,
				framebuffer_height, current_x, y, codepoint, !inverse);
			current_x += ascii_cell_width(style);
		} else if (combining_accent(codepoint, &accent)) {
			uint8_t cell_width = ascii_cell_width(style);

			if (current_x >= (size_t)x + cell_width) {
				draw_latin_accent(framebuffer, framebuffer_width,
					framebuffer_height, current_x - cell_width,
					y, accent, !inverse);
			}
		} else {
			draw_cjk(framebuffer, framebuffer_width,
				framebuffer_height, current_x, y,
				codepoint, !inverse);
			current_x += EST_UI_FONT_CJK_WIDTH;
		}
	}
	return true;
}
