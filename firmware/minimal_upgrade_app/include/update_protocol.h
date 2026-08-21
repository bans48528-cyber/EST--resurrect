#ifndef UPDATE_PROTOCOL_H
#define UPDATE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void update_protocol_init(void);
void update_protocol_feed_report(const uint8_t *report, size_t length,
	uint32_t now_ms);
void update_protocol_tick(uint32_t now_ms);
bool update_protocol_power_off_due(uint32_t now_ms);

#endif
