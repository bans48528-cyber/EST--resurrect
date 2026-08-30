#ifndef EST_UI_H
#define EST_UI_H

#include <stdbool.h>
#include <stdint.h>

void est_ui_init(uint32_t now_ms);
void est_ui_tick(uint32_t now_ms);
bool est_ui_power_off_requested(void);

#endif
