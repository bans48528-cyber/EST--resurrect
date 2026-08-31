#include <stddef.h>
#include <string.h>

#include "board_sensor_mode.h"

void board_sensor_mode_init(struct board_sensor_mode_tracker *tracker,
	uint8_t default_mode)
{
	if (tracker == NULL) {
		return;
	}
	memset(tracker, 0, sizeof(*tracker));
	tracker->requested_mode = default_mode;
	board_sensor_mode_reset_stream(tracker, default_mode);
}

void board_sensor_mode_reset_stream(struct board_sensor_mode_tracker *tracker,
	uint8_t default_mode)
{
	if (tracker == NULL) {
		return;
	}
	tracker->active_mode = default_mode;
	tracker->pending = true;
	tracker->command_sent = false;
	tracker->last_data_ms = 0U;
}

bool board_sensor_mode_request(struct board_sensor_mode_tracker *tracker,
	uint8_t mode)
{
	if (tracker == NULL || tracker->requested_mode == mode) {
		return false;
	}
	tracker->requested_mode = mode;
	tracker->pending = true;
	tracker->command_sent = false;
	return true;
}

bool board_sensor_mode_command_needed(
	const struct board_sensor_mode_tracker *tracker)
{
	return tracker != NULL && tracker->pending && !tracker->command_sent &&
		tracker->requested_mode != tracker->active_mode;
}

void board_sensor_mode_mark_command_sent(
	struct board_sensor_mode_tracker *tracker)
{
	if (tracker == NULL || tracker->command_sent) {
		return;
	}
	tracker->command_sent = true;
	tracker->mode_command_count++;
}

bool board_sensor_mode_accept_data(struct board_sensor_mode_tracker *tracker,
	uint8_t mode, uint32_t now_ms)
{
	bool requested;

	if (tracker == NULL) {
		return false;
	}
	tracker->active_mode = mode;
	tracker->data_generation++;
	tracker->last_data_ms = now_ms;
	requested = mode == tracker->requested_mode;
	if (requested) {
		tracker->pending = false;
	}
	return requested;
}
