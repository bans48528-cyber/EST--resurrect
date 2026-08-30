#ifndef EST_SCREEN_H
#define EST_SCREEN_H

#include <stdbool.h>

typedef enum {
	EST_SCREEN_OWNER_MENU = 0,
	EST_SCREEN_OWNER_PROGRAM = 1,
	EST_SCREEN_OWNER_UPGRADE = 2,
	EST_SCREEN_OWNER_MAINTENANCE = 3
} est_screen_owner_t;

void est_screen_init(est_screen_owner_t owner);
void est_screen_set_owner(est_screen_owner_t owner);
est_screen_owner_t est_screen_owner(void);
bool est_screen_is_owner(est_screen_owner_t owner);

#endif
