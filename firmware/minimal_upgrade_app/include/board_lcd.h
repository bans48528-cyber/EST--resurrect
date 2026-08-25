#ifndef BOARD_LCD_H
#define BOARD_LCD_H

#define BOARD_LCD_MOTOR_LINE_CHARACTERS 18U

void board_lcd_init(void);
void board_lcd_show_version(const char *version);
void board_lcd_show_sensor(const char *version, const char *mode,
	const char *reading);
void board_lcd_show_sensor_ports(const char *version, const char *mode,
	const char *const readings[4]);
void board_lcd_show_io_ports(const char *version, const char *mode,
	const char *const sensor_readings[4],
	const char *const motor_readings[4], const char *status);

#endif
