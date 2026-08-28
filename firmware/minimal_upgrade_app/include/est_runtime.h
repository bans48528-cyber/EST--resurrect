#ifndef EST_RUNTIME_H
#define EST_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	uint32_t tick_count;
	uint32_t last_tick_ms;
	uint32_t maximum_gap_ms;
} est_runtime_status_t;

void est_runtime_init(uint32_t now_ms);
void est_runtime_tick(uint32_t now_ms);
bool est_runtime_get_status(est_runtime_status_t *status);

#endif
