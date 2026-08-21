#include <libopencm3/stm32/iwdg.h>

#include "watchdog.h"

void watchdog_kick(void)
{
	iwdg_reset();
}
