#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_version.h"
#include "board_audio.h"
#include "board_backlight.h"
#include "board_battery.h"
#include "board_flash.h"
#include "board_lcd.h"
#include "board_led.h"
#include "board_keys.h"
#include "board_motor.h"
#include "board_power.h"
#include "board_sensor.h"
#include "platform.h"
#include "system_time.h"
#include "update_protocol.h"
#include "usb_hid.h"

#define SENSOR_DISPLAY_INTERVAL_MS 100U
#define SENSOR_DISPLAY_LINE_SIZE 12U
#define MOTOR_DISPLAY_LINE_SIZE 14U
#define STATUS_DISPLAY_LINE_SIZE 24U
#define SENSOR_READING_SIZE 10U
#define SENSOR_KIND_COLOR 0x01U
#define SENSOR_KIND_ULTRASONIC 0x02U
#define SENSOR_KIND_TEMPERATURE 0x04U
#define BACKLIGHT_DIM_AT_MS 1000U
#define BACKLIGHT_OFF_AT_MS 2500U
#define BACKLIGHT_RESTORE_AT_MS 3500U
#define AUDIO_TEST_START_AT_MS 4200U

static void format_u16(uint16_t value, char output[6])
{
	char reversed[5];
	uint8_t length = 0U;
	uint8_t index;

	do {
		reversed[length++] = (char)('0' + (value % 10U));
		value /= 10U;
	} while (value != 0U && length < sizeof(reversed));
	for (index = 0U; index < length; index++) {
		output[index] = reversed[length - index - 1U];
	}
	output[length] = '\0';
}

static void format_i32(int32_t value, char output[12])
{
	char reversed[10];
	uint32_t magnitude;
	uint8_t length = 0U;
	uint8_t output_index = 0U;
	uint8_t index;

	if (value < 0) {
		output[output_index++] = '-';
		magnitude = (uint32_t)(-(value + 1)) + 1U;
	} else {
		magnitude = (uint32_t)value;
	}
	do {
		reversed[length++] = (char)('0' + (magnitude % 10U));
		magnitude /= 10U;
	} while (magnitude != 0U && length < sizeof(reversed));
	for (index = 0U; index < length; index++) {
		output[output_index++] = reversed[length - index - 1U];
	}
	output[output_index] = '\0';
}

static const char *sensor_mode_text(uint8_t mode, uint8_t sensor_kinds)
{
	static const char *const color_names[] = {"REFLECT", "AMBIENT", "COLOR"};
	static const char *const ultrasonic_names[] = {
		"DIST CM", "DIST IN", "PRESENCE"
	};

	if (sensor_kinds != 0U &&
	    (sensor_kinds & (uint8_t)(sensor_kinds - 1U)) != 0U) {
		return "INPUTS";
	}
	if (sensor_kinds == SENSOR_KIND_TEMPERATURE) {
		return mode == BOARD_SENSOR_MODE_FAHRENHEIT ? "TEMP F" : "TEMP C";
	}
	if (sensor_kinds == SENSOR_KIND_ULTRASONIC) {
		return mode <= BOARD_SENSOR_MODE_PRESENCE ?
			ultrasonic_names[mode] : "SENSOR";
	}
	return mode <= BOARD_SENSOR_MODE_COLOR ? color_names[mode] : "SENSOR";
}

static const char *sensor_color_text(uint16_t value)
{
	static const char *const names[] = {
		"NONE", "BLACK", "BLUE", "GREEN",
		"YELLOW", "RED", "WHITE", "BROWN"
	};

	return value < (sizeof(names) / sizeof(names[0])) ? names[value] : NULL;
}

static void format_tenths(int16_t value, const char *unit,
	char output[SENSOR_READING_SIZE])
{
	char whole[6];
	uint16_t magnitude;
	uint8_t output_index = 0U;
	uint8_t input_index = 0U;

	if (value < 0) {
		output[output_index++] = '-';
		magnitude = (uint16_t)(-(int32_t)value);
	} else {
		magnitude = (uint16_t)value;
	}
	format_u16((uint16_t)(magnitude / 10U), whole);
	while (whole[input_index] != '\0' &&
	       output_index < SENSOR_READING_SIZE - 1U) {
		output[output_index++] = whole[input_index++];
	}
	if (output_index < SENSOR_READING_SIZE - 1U) {
		output[output_index++] = '.';
	}
	if (output_index < SENSOR_READING_SIZE - 1U) {
		output[output_index++] = (char)('0' + (magnitude % 10U));
	}
	input_index = 0U;
	while (unit[input_index] != '\0' &&
	       output_index < SENSOR_READING_SIZE - 1U) {
		output[output_index++] = unit[input_index++];
	}
	output[output_index] = '\0';
}

static const char *sensor_reading_text(
	const struct board_sensor_snapshot *snapshot,
	char formatted[SENSOR_READING_SIZE])
{
	const char *color;

	if (snapshot->sensor_type == BOARD_SENSOR_TYPE_TOUCH) {
		if (!snapshot->value_valid) {
			return "WAIT";
		}
		return snapshot->value != 0U ? "DOWN" : "UP";
	}
	if (!snapshot->value_valid) {
		return snapshot->state == BOARD_SENSOR_OFF ? "OFF" : "WAIT";
	}
	if (snapshot->sensor_type == BOARD_SENSOR_TYPE_ULTRASONIC) {
		if (snapshot->mode == BOARD_SENSOR_MODE_DISTANCE_CM) {
			format_tenths((int16_t)snapshot->value, "CM", formatted);
			return formatted;
		}
		if (snapshot->mode == BOARD_SENSOR_MODE_DISTANCE_INCH) {
			format_tenths((int16_t)snapshot->value, "IN", formatted);
			return formatted;
		}
		if (snapshot->mode == BOARD_SENSOR_MODE_PRESENCE) {
			return snapshot->value != 0U ? "YES" : "NO";
		}
	}
	if (snapshot->sensor_type == BOARD_SENSOR_TYPE_TEMPERATURE) {
		format_tenths((int16_t)snapshot->value,
			snapshot->mode == BOARD_SENSOR_MODE_FAHRENHEIT ? "F" : "C",
			formatted);
		return formatted;
	}
	if (snapshot->sensor_type == BOARD_SENSOR_TYPE_EV3_COLOR &&
	    snapshot->mode == BOARD_SENSOR_MODE_COLOR) {
		color = sensor_color_text(snapshot->value);
		if (color != NULL) {
			return color;
		}
	}
	format_u16(snapshot->value, formatted);
	return formatted;
}

static void format_sensor_line(uint8_t port_index,
	const struct board_sensor_snapshot *snapshot,
	char output[SENSOR_DISPLAY_LINE_SIZE])
{
	char formatted[SENSOR_READING_SIZE];
	const char *reading = sensor_reading_text(snapshot, formatted);
	uint8_t output_index = 0U;

	output[output_index++] = (char)('1' + port_index);
	output[output_index++] = ':';
	while (*reading != '\0' && output_index < SENSOR_DISPLAY_LINE_SIZE - 1U) {
		output[output_index++] = *reading++;
	}
	output[output_index] = '\0';
}

static void format_motor_line(uint8_t port_index,
	const struct board_motor_control_snapshot *snapshot,
	char output[MOTOR_DISPLAY_LINE_SIZE])
{
	char count[12];
	uint8_t output_index = 0U;
	uint8_t count_index = 0U;

	format_i32(snapshot->tacho_count, count);
	output[output_index++] = (char)('A' + port_index);
	output[output_index++] = ':';
	while (count[count_index] != '\0' &&
	       output_index < MOTOR_DISPLAY_LINE_SIZE - 1U) {
		output[output_index++] = count[count_index++];
	}
	output[output_index] = '\0';
}

static void append_status_text(char output[STATUS_DISPLAY_LINE_SIZE],
	uint8_t *output_index, const char *text)
{
	while (*text != '\0' && *output_index < STATUS_DISPLAY_LINE_SIZE - 1U) {
		output[(*output_index)++] = *text++;
	}
	output[*output_index] = '\0';
}

static void format_status_line(const struct board_battery_snapshot *battery,
	bool audio_test_attempted, bool audio_test_succeeded,
	char output[STATUS_DISPLAY_LINE_SIZE])
{
	char percent[6];
	const char *audio_status;
	uint8_t output_index = 0U;

	output[0] = '\0';
	append_status_text(output, &output_index, "BAT:");
	if (battery->valid) {
		format_u16((uint16_t)battery->level * 25U, percent);
		append_status_text(output, &output_index, percent);
		append_status_text(output, &output_index, "%");
	} else {
		append_status_text(output, &output_index, "WAIT");
	}
	append_status_text(output, &output_index, " SND:");
	if (!board_audio_ready()) {
		audio_status = "ERR";
	} else if (!audio_test_attempted) {
		audio_status = "READY";
	} else if (!audio_test_succeeded) {
		audio_status = "ERR";
	} else if (board_audio_test_active()) {
		audio_status = "PLAY";
	} else {
		audio_status = "DONE";
	}
	append_status_text(output, &output_index, audio_status);
}

int main(void)
{
	uint32_t last_diag_ms = 0U;
	uint32_t last_display_ms = 0U;
	uint8_t diag_phase = 0U;
	uint8_t last_key_mask;
	uint8_t selected_sensor_mode = BOARD_SENSOR_MODE_REFLECTED;
	uint8_t active_sensor_kinds = 0U;
	uint8_t displayed_mode = 0xFFU;
	uint8_t displayed_sensor_kinds = 0xFFU;
	uint8_t backlight_test_phase = 0U;
	bool audio_test_attempted = false;
	bool audio_test_succeeded = false;
	bool display_initialized = false;
	char displayed_lines[BOARD_SENSOR_PORT_COUNT][SENSOR_DISPLAY_LINE_SIZE] = {{0}};
	char displayed_motor_lines[BOARD_MOTOR_PORT_COUNT][MOTOR_DISPLAY_LINE_SIZE] = {{0}};
	char displayed_status_line[STATUS_DISPLAY_LINE_SIZE] = {0};

	board_power_init();
	board_backlight_init();
	board_led_init();
	system_time_init();
	board_keys_init(system_time_millis());
	board_flash_init();
	board_audio_init();
	board_motor_init();
	board_sensor_init(system_time_millis());
	board_battery_init(system_time_millis());
	board_led_checkpoint(1U);
	update_protocol_init();
	board_led_checkpoint(2U);
	usb_hid_init();
	board_led_checkpoint(3U);
	platform_enable_interrupts();
	board_led_checkpoint(0U);
#ifndef DIAGNOSTIC_SKIP_LCD_STARTUP
	board_lcd_init();
	board_lcd_show_version(app_version_text);
#endif
	last_key_mask = board_keys_pressed_mask();

	while (1) {
		uint32_t now_ms;
		uint8_t key_mask;

		usb_hid_poll();
		now_ms = system_time_millis();
		board_backlight_tick(now_ms);
		board_audio_tick(now_ms);
		if (backlight_test_phase == 0U && now_ms >= BACKLIGHT_DIM_AT_MS) {
			board_backlight_set_percent(20U);
			backlight_test_phase = 1U;
		} else if (backlight_test_phase == 1U &&
			   now_ms >= BACKLIGHT_OFF_AT_MS) {
			board_backlight_set_percent(0U);
			backlight_test_phase = 2U;
		} else if (backlight_test_phase == 2U &&
			   now_ms >= BACKLIGHT_RESTORE_AT_MS) {
			board_backlight_set_percent(100U);
			backlight_test_phase = 3U;
		}
		if (!audio_test_attempted && now_ms >= AUDIO_TEST_START_AT_MS) {
			audio_test_succeeded = board_audio_start_test(now_ms);
			audio_test_attempted = true;
		}
		board_keys_tick(now_ms);
		key_mask = board_keys_pressed_mask();
		if (key_mask != 0U && last_key_mask == 0U) {
			uint8_t mode_count = active_sensor_kinds ==
				SENSOR_KIND_TEMPERATURE ? 2U : 3U;

			selected_sensor_mode = (uint8_t)
				((selected_sensor_mode + 1U) % mode_count);
			(void)board_sensor_set_all_modes((enum board_sensor_mode)
				selected_sensor_mode, now_ms);
		}
		last_key_mask = key_mask;
		board_motor_tick(now_ms);
		board_sensor_tick(now_ms);
		board_battery_tick(now_ms);
#ifndef DIAGNOSTIC_SKIP_LCD_STARTUP
		if ((uint32_t)(now_ms - last_display_ms) >=
		    SENSOR_DISPLAY_INTERVAL_MS) {
			char lines[BOARD_SENSOR_PORT_COUNT][SENSOR_DISPLAY_LINE_SIZE];
			char motor_lines[BOARD_MOTOR_PORT_COUNT][MOTOR_DISPLAY_LINE_SIZE];
			char status_line[STATUS_DISPLAY_LINE_SIZE];
			const char *line_pointers[BOARD_SENSOR_PORT_COUNT];
			const char *motor_line_pointers[BOARD_MOTOR_PORT_COUNT];
			uint8_t sensor_kinds = 0U;
			uint8_t index;
			bool display_changed;
			struct board_battery_snapshot battery;

			last_display_ms = now_ms;
			for (index = 0U; index < BOARD_SENSOR_PORT_COUNT; index++) {
				struct board_sensor_snapshot sensor = {0};

				(void)board_sensor_get_snapshot(
					(enum board_sensor_port)index, &sensor);
				if (sensor.sensor_type == BOARD_SENSOR_TYPE_EV3_COLOR) {
					sensor_kinds |= SENSOR_KIND_COLOR;
				} else if (sensor.sensor_type ==
					   BOARD_SENSOR_TYPE_ULTRASONIC) {
					sensor_kinds |= SENSOR_KIND_ULTRASONIC;
				} else if (sensor.sensor_type ==
					   BOARD_SENSOR_TYPE_TEMPERATURE) {
					sensor_kinds |= SENSOR_KIND_TEMPERATURE;
				}
				format_sensor_line(index, &sensor, lines[index]);
				line_pointers[index] = lines[index];
				{
					struct board_motor_control_snapshot motor = {0};

					(void)board_motor_control_snapshot(
						(enum board_motor_port)index, &motor);
					format_motor_line(index, &motor, motor_lines[index]);
					motor_line_pointers[index] = motor_lines[index];
				}
			}
			active_sensor_kinds = sensor_kinds;
			battery = board_battery_snapshot();
			format_status_line(&battery, audio_test_attempted,
				audio_test_succeeded, status_line);
			display_changed = !display_initialized ||
				displayed_mode != selected_sensor_mode ||
				displayed_sensor_kinds != sensor_kinds;
			if (strcmp(displayed_status_line, status_line) != 0) {
				display_changed = true;
			}
			for (index = 0U; index < BOARD_SENSOR_PORT_COUNT; index++) {
				if (strcmp(displayed_lines[index], lines[index]) != 0) {
					display_changed = true;
				}
				if (strcmp(displayed_motor_lines[index],
					   motor_lines[index]) != 0) {
					display_changed = true;
				}
			}
			if (display_changed) {
				board_lcd_show_io_ports(app_version_text,
					sensor_mode_text(selected_sensor_mode, sensor_kinds),
					line_pointers, motor_line_pointers, status_line);
				memcpy(displayed_lines, lines, sizeof(displayed_lines));
				memcpy(displayed_motor_lines, motor_lines,
					sizeof(displayed_motor_lines));
				memcpy(displayed_status_line, status_line,
					sizeof(displayed_status_line));
				displayed_mode = selected_sensor_mode;
				displayed_sensor_kinds = sensor_kinds;
				display_initialized = true;
			}
		}
#endif
		if ((now_ms - last_diag_ms) >= 500U) {
			last_diag_ms = now_ms;
			diag_phase++;
			board_led_diag_set(diag_phase);
		}
		update_protocol_tick(now_ms);
		if (usb_hid_power_off_requested() ||
		    update_protocol_power_off_due(now_ms)) {
			board_motor_stop();
			board_sensor_stop();
			platform_disable_interrupts();
			board_led_all_off();
			board_power_off();
		}
	}
}
