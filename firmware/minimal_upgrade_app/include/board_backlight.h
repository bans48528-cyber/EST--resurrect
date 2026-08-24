#ifndef BOARD_BACKLIGHT_H
#define BOARD_BACKLIGHT_H

#include <stdint.h>

void board_backlight_init(void);
void board_backlight_set_percent(uint8_t percent);
uint8_t board_backlight_percent(void);
void board_backlight_tick(uint32_t now_ms);

#endif
