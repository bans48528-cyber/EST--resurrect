#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libopencm3/stm32/iwdg.h>

#include "watchdog.h"

#define WATCHDOG_MAX_BUDGET_MS 60000U

static bool main_time_seen;
static bool vm_time_seen;
static uint32_t main_last_ms;
static uint32_t vm_last_ms;
static uint8_t guard_depth;

static void reload(void)
{
	iwdg_reset();
}

static bool time_advanced(bool *seen, uint32_t *last_ms, uint32_t now_ms)
{
	if (*seen && now_ms == *last_ms) {
		return false;
	}
	*seen = true;
	*last_ms = now_ms;
	return true;
}

static bool deadline_pending(uint32_t now_ms, uint32_t deadline_ms)
{
	return (int32_t)(deadline_ms - now_ms) >= 0;
}

void watchdog_startup_progress(void)
{
	reload();
}

void watchdog_main_progress(uint32_t now_ms)
{
	if (guard_depth == 0U &&
	    time_advanced(&main_time_seen, &main_last_ms, now_ms)) {
		reload();
	}
}

void watchdog_vm_progress(uint32_t now_ms)
{
	if (guard_depth == 0U &&
	    time_advanced(&vm_time_seen, &vm_last_ms, now_ms)) {
		reload();
	}
}

void watchdog_guard_begin(watchdog_guard_t *guard, uint32_t now_ms,
	uint32_t budget_ms)
{
	if (guard == NULL) {
		return;
	}
	guard->active = false;
	if (budget_ms == 0U || budget_ms > WATCHDOG_MAX_BUDGET_MS) {
		return;
	}
	guard->deadline_ms = now_ms + budget_ms;
	guard->last_progress_ms = now_ms;
	guard->active = true;
	guard_depth++;
	reload();
}

bool watchdog_guard_progress(watchdog_guard_t *guard, uint32_t now_ms)
{
	if (guard == NULL || !guard->active || guard_depth == 0U ||
	    !deadline_pending(now_ms, guard->deadline_ms)) {
		return false;
	}
	if (now_ms != guard->last_progress_ms) {
		guard->last_progress_ms = now_ms;
		reload();
	}
	return true;
}

void watchdog_guard_end(watchdog_guard_t *guard)
{
	if (guard == NULL || !guard->active) {
		return;
	}
	guard->active = false;
	if (guard_depth > 0U) {
		guard_depth--;
	}
}

#ifdef WATCHDOG_TEST
void watchdog_test_reset_state(void)
{
	main_time_seen = false;
	vm_time_seen = false;
	main_last_ms = 0U;
	vm_last_ms = 0U;
	guard_depth = 0U;
}
#endif
