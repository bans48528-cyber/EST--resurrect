#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	uint32_t deadline_ms;
	uint32_t last_progress_ms;
	bool active;
} watchdog_guard_t;

void watchdog_startup_progress(void);
void watchdog_main_progress(uint32_t now_ms);
void watchdog_vm_progress(uint32_t now_ms);
void watchdog_guard_begin(watchdog_guard_t *guard, uint32_t now_ms,
	uint32_t budget_ms);
bool watchdog_guard_progress(watchdog_guard_t *guard, uint32_t now_ms);
void watchdog_guard_end(watchdog_guard_t *guard);

#endif
