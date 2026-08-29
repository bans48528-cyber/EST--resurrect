#ifndef BOARD_LCD_IMAGE_H
#define BOARD_LCD_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOARD_LCD_IMAGE_MAX_WIDTH 180U
#define BOARD_LCD_IMAGE_MAX_HEIGHT 128U

typedef struct {
	const char *name;
	uint16_t width;
	uint16_t height;
	uint32_t data_offset;
	uint32_t data_size;
} board_lcd_image_resource_t;

extern const uint8_t board_lcd_image_data[];
extern const size_t board_lcd_image_data_size;
extern const board_lcd_image_resource_t board_lcd_image_resources[];
extern const size_t board_lcd_image_resource_count;

const board_lcd_image_resource_t *board_lcd_image_find(const char *name);
bool board_lcd_image_decode(uint8_t *framebuffer,
	uint16_t framebuffer_width, uint16_t framebuffer_height,
	uint16_t x, uint16_t y, uint16_t image_width, uint16_t image_height,
	const uint8_t *compressed, size_t compressed_size);
bool board_lcd_image_draw(uint8_t *framebuffer,
	uint16_t framebuffer_width, uint16_t framebuffer_height,
	uint16_t x, uint16_t y,
	const board_lcd_image_resource_t *resource);

#endif
