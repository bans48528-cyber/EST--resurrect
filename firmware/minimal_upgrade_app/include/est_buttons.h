#ifndef EST_BUTTONS_H
#define EST_BUTTONS_H

#include <stdbool.h>
#include <stdint.h>

#define EST_BUTTON_COUNT 6U
#define EST_BUTTON_MASK_ALL ((1U << EST_BUTTON_COUNT) - 1U)
#define EST_BUTTON_LONG_PRESS_MS 1000U

typedef enum {
	EST_BUTTON_BACK = 0,
	EST_BUTTON_LEFT = 1,
	EST_BUTTON_UP = 2,
	EST_BUTTON_DOWN = 3,
	EST_BUTTON_RIGHT = 4,
	EST_BUTTON_CONFIRM = 5
} est_button_t;

#define EST_BUTTON_CENTER EST_BUTTON_CONFIRM

void est_buttons_init(uint32_t now_ms);
void est_buttons_tick(uint32_t now_ms);
uint8_t est_buttons_pressed_mask(void);
bool est_button_is_pressed(est_button_t button);
uint8_t est_buttons_take_pressed_events(void);
uint8_t est_buttons_take_released_events(void);
uint8_t est_buttons_take_long_press_events(void);

#endif
