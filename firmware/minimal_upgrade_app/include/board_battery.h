#ifndef BOARD_BATTERY_H
#define BOARD_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD_BATTERY_LEVEL_MAX 4U

struct board_battery_snapshot {
	bool valid;
	uint8_t level;
	uint16_t adc_raw;
	uint16_t sample_mv;
};

void board_battery_init(uint32_t now_ms);
void board_battery_tick(uint32_t now_ms);
struct board_battery_snapshot board_battery_snapshot(void);

#endif
