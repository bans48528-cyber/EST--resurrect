#ifndef BOARD_LCD_H
#define BOARD_LCD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOARD_LCD_WIDTH 180U
#define BOARD_LCD_HEIGHT 128U
#define BOARD_LCD_MOTOR_LINE_CHARACTERS 18U

void board_lcd_init(void);
void board_lcd_clear(void);
bool board_lcd_set_pixel(uint16_t x, uint16_t y, bool on);
bool board_lcd_draw_line(uint16_t x0, uint16_t y0,
	uint16_t x1, uint16_t y1, bool on);
bool board_lcd_draw_rectangle(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, bool filled, bool on);
bool board_lcd_draw_text(uint16_t x, uint16_t y,
	const char *text, uint8_t scale);
bool board_lcd_draw_bitmap(uint16_t x, uint16_t y,
	uint16_t width, uint16_t height, const uint8_t *bitmap,
	size_t bitmap_size);
void board_lcd_refresh(void);
void board_lcd_show_version(const char *version);
void board_lcd_show_sensor(const char *version, const char *mode,
	const char *reading);
void board_lcd_show_sensor_ports(const char *version, const char *mode,
	const char *const readings[4]);
void board_lcd_show_io_ports(const char *version, const char *mode,
	const char *const sensor_readings[4],
	const char *const motor_readings[4], const char *status);

#endif
