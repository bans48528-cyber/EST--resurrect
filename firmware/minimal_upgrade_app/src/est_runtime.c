#include <stddef.h>
#include <stdint.h>

#include "board_audio.h"
#include "board_motor.h"
#include "board_sensor.h"
#include "est_backlight.h"
#include "est_battery.h"
#include "est_buttons.h"
#include "est_runtime.h"
#include "update_protocol.h"

static est_runtime_status_t runtime_status;
static bool runtime_initialized;

void est_runtime_init(uint32_t now_ms)
{
	runtime_status.tick_count = 0U;
	runtime_status.last_tick_ms = now_ms - 1U;
	runtime_status.maximum_gap_ms = 0U;
	runtime_initialized = true;
}

void est_runtime_tick(uint32_t now_ms)
{
	uint32_t gap_ms;

	if (!runtime_initialized) {
		est_runtime_init(now_ms);
	}
	if (now_ms == runtime_status.last_tick_ms) {
		return;
	}
	gap_ms = now_ms - runtime_status.last_tick_ms;
	if (gap_ms > runtime_status.maximum_gap_ms) {
		runtime_status.maximum_gap_ms = gap_ms;
	}
	runtime_status.last_tick_ms = now_ms;
	runtime_status.tick_count++;

	est_backlight_tick(now_ms);
	board_audio_tick(now_ms);
	est_buttons_tick(now_ms);
	board_motor_tick(now_ms);
	board_sensor_tick(now_ms);
	est_battery_tick(now_ms);
	update_protocol_tick(now_ms);
}

bool est_runtime_get_status(est_runtime_status_t *status)
{
	if (status == NULL) {
		return false;
	}
	*status = runtime_status;
	return runtime_initialized;
}
