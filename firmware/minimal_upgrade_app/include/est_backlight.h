#ifndef EST_BACKLIGHT_H
#define EST_BACKLIGHT_H

#include <stdint.h>

#include "est_types.h"

void est_backlight_init(void);
void est_backlight_tick(uint32_t now_ms);
est_result_t est_backlight_set_percent(uint8_t percent);
uint8_t est_backlight_get_percent(void);

#endif
