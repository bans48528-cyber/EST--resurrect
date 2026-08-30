#include <stdbool.h>

#include "est_screen.h"

static est_screen_owner_t current_owner;

void est_screen_init(est_screen_owner_t owner)
{
	current_owner = owner;
}

void est_screen_set_owner(est_screen_owner_t owner)
{
	current_owner = owner;
}

est_screen_owner_t est_screen_owner(void)
{
	return current_owner;
}

bool est_screen_is_owner(est_screen_owner_t owner)
{
	return current_owner == owner;
}
