#ifndef BOARD_KEYS_H
#define BOARD_KEYS_H

#include <stdint.h>

#define BOARD_KEY_COUNT 6U
#define BOARD_KEY_MASK_ALL ((1U << BOARD_KEY_COUNT) - 1U)

void board_keys_init(uint32_t now_ms);
void board_keys_tick(uint32_t now_ms);
uint8_t board_keys_pressed_mask(void);

#endif
