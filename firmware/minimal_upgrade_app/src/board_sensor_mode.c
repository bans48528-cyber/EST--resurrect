#include <stddef.h>
#include <string.h>

#include "board_sensor_mode.h"

#define BOARD_SENSOR_MODE_SELECT_ATTEMPTS 5U
#define BOARD_SENSOR_MODE_SELECT_RETRY_MS 100U

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
	tracker->command_attempts = 0U;
	tracker->pending = true;
	tracker->command_sent = false;
	tracker->last_data_ms = 0U;
	tracker->last_command_ms = 0U;
}

bool board_sensor_mode_request(struct board_sensor_mode_tracker *tracker,
	uint8_t mode)
{
	if (tracker == NULL || tracker->requested_mode == mode) {
		return false;
	}
	tracker->requested_mode = mode;
	tracker->command_attempts = 0U;
	tracker->pending = true;
	tracker->command_sent = false;
	tracker->last_command_ms = 0U;
	return true;
}

bool board_sensor_mode_command_needed(
	const struct board_sensor_mode_tracker *tracker, uint32_t now_ms)
{
	if (tracker == NULL || !tracker->pending || tracker->command_sent ||
	    tracker->requested_mode == tracker->active_mode) {
		return false;
	}
	return tracker->command_attempts == 0U ||
		(uint32_t)(now_ms - tracker->last_command_ms) >=
		BOARD_SENSOR_MODE_SELECT_RETRY_MS;
}

void board_sensor_mode_mark_command_sent(
	struct board_sensor_mode_tracker *tracker, uint32_t now_ms)
{
	if (tracker == NULL || tracker->command_sent) {
		return;
	}
	tracker->command_sent = true;
	tracker->command_attempts++;
	tracker->mode_command_count++;
	tracker->last_command_ms = now_ms;
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
	} else if (tracker->pending && tracker->command_sent &&
	    tracker->command_attempts < BOARD_SENSOR_MODE_SELECT_ATTEMPTS) {
		/* Retry only after a complete frame confirms the old mode is active. */
		tracker->command_sent = false;
	}
	return requested;
}
