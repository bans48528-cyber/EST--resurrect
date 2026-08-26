#ifndef EST_LED_H
#define EST_LED_H

#include "est_types.h"

typedef enum {
	EST_LED_OFF = 0,
	EST_LED_RED = 1,
	EST_LED_BLUE = 2,
	EST_LED_RED_BLUE = 3
} est_led_color_t;

void est_led_init(void);
est_result_t est_led_set(est_led_color_t color);
est_led_color_t est_led_get(void);

#endif
