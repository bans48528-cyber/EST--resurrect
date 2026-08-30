#ifndef EST_KEY_EVENTS_H
#define EST_KEY_EVENTS_H

#include <stdint.h>

void est_key_events_init(void);
void est_key_events_tick(void);
void est_key_events_reset(void);
uint8_t est_key_events_take_short(void);
uint8_t est_key_events_take_long(void);

#endif
