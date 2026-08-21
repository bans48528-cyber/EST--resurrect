#include <stdint.h>

#include <libopencm3/cm3/systick.h>

#include "system_time.h"
#include "watchdog.h"

static volatile uint32_t milliseconds;

void sys_tick_handler(void);

void system_time_init(void)
{
	systick_set_frequency(1000U, 168000000U);
	systick_interrupt_enable();
	systick_counter_enable();
	watchdog_kick();
}

uint32_t system_time_millis(void)
{
	return milliseconds;
}

void sys_tick_handler(void)
{
	milliseconds++;
	watchdog_kick();
}
