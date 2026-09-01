#ifndef BOARD_SENSOR_MODE_H
#define BOARD_SENSOR_MODE_H

#include <stdbool.h>
#include <stdint.h>

struct board_sensor_mode_tracker {
	uint8_t requested_mode;
	uint8_t active_mode;
	uint8_t command_attempts;
	bool pending;
	bool command_sent;
	uint32_t data_generation;
	uint32_t last_data_ms;
	uint32_t mode_command_count;
	uint32_t last_command_ms;
};

void board_sensor_mode_init(struct board_sensor_mode_tracker *tracker,
	uint8_t default_mode);
void board_sensor_mode_reset_stream(struct board_sensor_mode_tracker *tracker,
	uint8_t default_mode);
bool board_sensor_mode_request(struct board_sensor_mode_tracker *tracker,
	uint8_t mode);
bool board_sensor_mode_command_needed(
	const struct board_sensor_mode_tracker *tracker, uint32_t now_ms);
void board_sensor_mode_mark_command_sent(
	struct board_sensor_mode_tracker *tracker, uint32_t now_ms);
bool board_sensor_mode_accept_data(struct board_sensor_mode_tracker *tracker,
	uint8_t mode, uint32_t now_ms);

#endif
