#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_lcd_image.h"

#define MAX_STRIDE ((BOARD_LCD_IMAGE_MAX_WIDTH + 7U) / 8U)

const board_lcd_image_resource_t *board_lcd_image_find(const char *name)
{
	size_t index;

	if (name == NULL) {
		return NULL;
	}
	for (index = 0U; index < board_lcd_image_resource_count; index++) {
		if (strcmp(name, board_lcd_image_resources[index].name) == 0) {
			return &board_lcd_image_resources[index];
		}
	}
	return NULL;
}

static void draw_decoded_byte(uint8_t *framebuffer,
	uint16_t framebuffer_height, uint16_t x, uint16_t y,
	uint16_t image_width, size_t output_index, uint8_t value)
{
	size_t stride = ((size_t)image_width + 7U) / 8U;
	uint16_t row = (uint16_t)(output_index / stride);
	uint16_t byte_x = (uint16_t)(output_index % stride);
	uint8_t bit;

	for (bit = 0U; bit < 8U; bit++) {
		uint16_t image_x = (uint16_t)(byte_x * 8U + bit);
		uint16_t pixel_x;
		uint16_t pixel_y;
		size_t framebuffer_index;
		uint8_t framebuffer_bit;

		if (image_x >= image_width) {
			break;
		}
		pixel_x = (uint16_t)(x + image_x);
		pixel_y = (uint16_t)(y + row);
		framebuffer_index = (size_t)pixel_x * (framebuffer_height / 8U) +
			pixel_y / 8U;
		framebuffer_bit = (uint8_t)(1U << (pixel_y % 8U));
		if ((value & (uint8_t)(0x80U >> bit)) != 0U) {
			framebuffer[framebuffer_index] |= framebuffer_bit;
		} else {
			framebuffer[framebuffer_index] &=
				(uint8_t)~framebuffer_bit;
		}
	}
}

static bool decode_value(uint8_t *framebuffer,
	uint16_t framebuffer_height, uint16_t x, uint16_t y,
	uint16_t image_width, size_t expected_size, size_t *output_index,
	uint8_t *previous_row, uint8_t delta)
{
	size_t stride = ((size_t)image_width + 7U) / 8U;
	size_t byte_x;
	uint8_t value;

	if (*output_index >= expected_size) {
		return false;
	}
	byte_x = *output_index % stride;
	value = (uint8_t)(delta ^ previous_row[byte_x]);
	previous_row[byte_x] = value;
	draw_decoded_byte(framebuffer, framebuffer_height, x, y,
		image_width, *output_index, value);
	(*output_index)++;
	return true;
}

bool board_lcd_image_decode(uint8_t *framebuffer,
	uint16_t framebuffer_width, uint16_t framebuffer_height,
	uint16_t x, uint16_t y, uint16_t image_width, uint16_t image_height,
	const uint8_t *compressed, size_t compressed_size)
{
	uint8_t previous_row[MAX_STRIDE] = {0U};
	size_t expected_size;
	size_t input_index = 0U;
	size_t output_index = 0U;

	if (framebuffer == NULL || compressed == NULL || compressed_size == 0U ||
	    framebuffer_height == 0U || framebuffer_height % 8U != 0U ||
	    image_width == 0U || image_height == 0U ||
	    image_width > BOARD_LCD_IMAGE_MAX_WIDTH ||
	    image_height > BOARD_LCD_IMAGE_MAX_HEIGHT ||
	    x >= framebuffer_width || y >= framebuffer_height ||
	    image_width > framebuffer_width - x ||
	    image_height > framebuffer_height - y) {
		return false;
	}
	expected_size = (((size_t)image_width + 7U) / 8U) * image_height;
	while (input_index < compressed_size) {
		uint8_t control = compressed[input_index++];
		size_t count = (size_t)(control & 0x7FU) + 1U;
		size_t index;

		if (count > expected_size - output_index) {
			return false;
		}
		if ((control & 0x80U) != 0U) {
			uint8_t value;

			if (input_index >= compressed_size) {
				return false;
			}
			value = compressed[input_index++];
			for (index = 0U; index < count; index++) {
				if (!decode_value(framebuffer, framebuffer_height,
				    x, y, image_width, expected_size, &output_index,
				    previous_row, value)) {
					return false;
				}
			}
		} else {
			if (count > compressed_size - input_index) {
				return false;
			}
			for (index = 0U; index < count; index++) {
				if (!decode_value(framebuffer, framebuffer_height,
				    x, y, image_width, expected_size, &output_index,
				    previous_row, compressed[input_index++])) {
					return false;
				}
			}
		}
	}
	return output_index == expected_size;
}

bool board_lcd_image_draw(uint8_t *framebuffer,
	uint16_t framebuffer_width, uint16_t framebuffer_height,
	uint16_t x, uint16_t y,
	const board_lcd_image_resource_t *resource)
{
	if (resource == NULL ||
	    resource->data_offset > board_lcd_image_data_size ||
	    resource->data_size >
		board_lcd_image_data_size - resource->data_offset) {
		return false;
	}
	return board_lcd_image_decode(framebuffer, framebuffer_width,
		framebuffer_height, x, y, resource->width, resource->height,
		&board_lcd_image_data[resource->data_offset], resource->data_size);
}
