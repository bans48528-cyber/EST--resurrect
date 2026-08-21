#ifndef BOARD_LED_H
#define BOARD_LED_H

#include <stdint.h>

void board_led_init(void);
void board_led_diag_set(uint8_t phase);
void board_led_checkpoint(uint8_t code);
void board_led_all_off(void);

#endif
