#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_lcd.h"
#include "est_menu_icons_32x32.h"
#include "est_ui_renderer.h"
#include "est_ui_remote.h"
#include "est_ui_text.h"

#define HOME_ZONE_WIDTH 44U
#define HOME_ZONE_X 2U
#define HOME_ZONE_Y 23U
#define HOME_ZONE_HEIGHT 55U
#define HOME_ICON_Y 26U
#define HOME_LABEL_Y 59U
#define HOME_DETAIL_Y 84U
#define STATUS_Y 115U
#define CONTENT_TOP 21U
#define ROW_HEIGHT 23U

static est_ui_view_t ui_view;

static void draw_text_right(uint16_t y, const est_ui_state_t *state,
	est_ui_string_id_t text);

static const uint8_t *const home_icons[EST_UI_HOME_ITEM_COUNT] = {
	est_menu_icon_quick_run,
	est_menu_icon_programs,
	est_menu_icon_ports,
	est_menu_icon_remote,
	est_menu_icon_motor,
	est_menu_icon_settings,
};

static const est_ui_string_id_t home_labels[EST_UI_HOME_ITEM_COUNT] = {
	EST_UI_STRING_HOME_RECENT_LABEL,
	EST_UI_STRING_HOME_PROGRAMS,
	EST_UI_STRING_HOME_PORTS,
	EST_UI_STRING_HOME_REMOTE,
	EST_UI_STRING_HOME_MOTOR,
	EST_UI_STRING_HOME_SETTINGS,
};

static uint16_t centered_x(uint16_t width)
{
	return width < BOARD_LCD_WIDTH ?
		(uint16_t)((BOARD_LCD_WIDTH - width) / 2U) : 0U;
}

static uint16_t centered_in_zone(uint16_t zone_x, uint16_t zone_width,
	uint16_t width)
{
	uint16_t center = (uint16_t)(zone_x + zone_width / 2U);
	uint16_t x = center > width / 2U ?
		(uint16_t)(center - width / 2U) : 0U;

	if (width < BOARD_LCD_WIDTH && x > BOARD_LCD_WIDTH - width) {
		x = (uint16_t)(BOARD_LCD_WIDTH - width);
	}
	return x;
}

static void draw_text_centered(uint16_t y, est_ui_language_t language,
	est_ui_string_id_t string_id, est_ui_text_style_t style)
{
	uint16_t width = est_ui_text_width(language, string_id, style);

	(void)est_ui_text_draw(centered_x(width), y, language, string_id, style);
}

static uint8_t utf8_sequence_length(uint8_t first)
{
	if (first < 0x80U) {
		return 1U;
	}
	if ((first & 0xE0U) == 0xC0U) {
		return 2U;
	}
	if ((first & 0xF0U) == 0xE0U) {
		return 3U;
	}
	if ((first & 0xF8U) == 0xF0U) {
		return 4U;
	}
	return 1U;
}

static void copy_text(char *output, size_t capacity, const char *text)
{
	size_t index = 0U;

	if (capacity == 0U) {
		return;
	}
	while (text[index] != '\0' && index + 1U < capacity) {
		output[index] = text[index];
		index++;
	}
	output[index] = '\0';
}

static void fit_text(const char *text, uint16_t maximum_width,
	est_ui_text_style_t style, char output[40])
{
	size_t source_index = 0U;
	size_t output_index = 0U;
	size_t length;

	if (text == NULL) {
		output[0] = '\0';
		return;
	}
	if (est_ui_font_measure(text, style) <= maximum_width) {
		copy_text(output, 40U, text);
		return;
	}
	length = strlen(text);
	while (source_index < length && output_index + 4U < 40U) {
		uint8_t sequence = utf8_sequence_length((uint8_t)text[source_index]);
		char candidate[40];

		if (source_index + sequence > length ||
		    output_index + sequence + 3U >= sizeof(candidate)) {
			break;
		}
		memcpy(output + output_index, text + source_index, sequence);
		output_index += sequence;
		output[output_index] = '\0';
		memcpy(candidate, output, output_index);
		memcpy(candidate + output_index, "...", 4U);
		if (est_ui_font_measure(candidate, style) > maximum_width) {
			output_index -= sequence;
			break;
		}
		source_index += sequence;
	}
	memcpy(output + output_index, "...", 4U);
}

static void draw_raw_fitted(uint16_t x, uint16_t y, const char *text,
	uint16_t maximum_width, est_ui_text_style_t style)
{
	char fitted[40];

	fit_text(text, maximum_width, style, fitted);
	(void)est_ui_text_draw_raw(x, y, fitted, style);
}

static void draw_raw_right(uint16_t y, const char *text,
	est_ui_text_style_t style)
{
	uint16_t width = est_ui_font_measure(text, style);

	(void)est_ui_text_draw_raw(width + 3U < BOARD_LCD_WIDTH ?
		(uint16_t)(BOARD_LCD_WIDTH - width - 3U) : 0U, y, text, style);
}

static uint8_t append_uint(char *output, uint8_t index, uint8_t capacity,
	uint32_t value)
{
	char reversed[5];
	uint8_t length = 0U;

	do {
		reversed[length++] = (char)('0' + (value % 10U));
		value /= 10U;
	} while (value != 0U && length < sizeof(reversed));
	while (length != 0U && index + 1U < capacity) {
		output[index++] = reversed[--length];
	}
	output[index] = '\0';
	return index;
}

static uint8_t append_int(char *output, uint8_t index, uint8_t capacity,
	int32_t value)
{
	uint32_t magnitude;

	if (value < 0) {
		if (index + 1U < capacity) {
			output[index++] = '-';
		}
		magnitude = (uint32_t)(-(value + 1)) + 1U;
	} else {
		magnitude = (uint32_t)value;
	}
	return append_uint(output, index, capacity, magnitude);
}

static void format_percent(uint8_t percent, char output[6])
{
	uint8_t index = append_uint(output, 0U, 6U, percent);

	if (index + 1U < 6U) {
		output[index++] = '%';
		output[index] = '\0';
	}
}

static void format_error(uint16_t error_code, char output[8])
{
	uint8_t index = 0U;

	output[index++] = 'E';
	output[index++] = ':';
	(void)append_uint(output, index, 8U, error_code);
}

static void draw_header(const est_ui_state_t *state,
	const est_ui_view_t *view)
{
	char battery[10] = "BAT --";
	uint16_t battery_width;

	(void)est_ui_text_draw_raw(3U, 1U, "EST", EST_UI_TEXT_NORMAL);
	if (view->battery_valid) {
		uint8_t index = 4U;

		index = append_uint(battery, index, sizeof(battery),
			view->battery_percent);
		if (index + 1U < sizeof(battery)) {
			battery[index++] = '%';
			battery[index] = '\0';
		}
	}
	battery_width = est_ui_font_measure(battery, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw((uint16_t)(BOARD_LCD_WIDTH -
		battery_width - 3U), 1U, battery, view->battery_low ?
		EST_UI_TEXT_COMPACT_INVERSE : EST_UI_TEXT_COMPACT);
	(void)state;
	(void)board_lcd_draw_line(0U, 18U, BOARD_LCD_WIDTH - 1U, 18U, true);
}

static void draw_status(const est_ui_view_t *view)
{
	const char *usb = view->usb_connected ? "USB:ON" : "USB:OFF";
	uint16_t version_width = est_ui_font_measure(view->app_version,
		EST_UI_TEXT_COMPACT);

	(void)est_ui_text_draw_raw(3U, STATUS_Y, usb, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw((uint16_t)(BOARD_LCD_WIDTH -
		version_width - 3U), STATUS_Y, view->app_version,
		EST_UI_TEXT_COMPACT);
}

static bool bitmap_pixel(const uint8_t *bitmap, uint8_t x, uint8_t y)
{
	return (bitmap[(size_t)y * 4U + x / 8U] &
		(uint8_t)(0x80U >> (x % 8U))) != 0U;
}

static void draw_home_icon(uint16_t x, uint16_t y, const uint8_t *bitmap,
	bool inverse)
{
	uint8_t icon_x;
	uint8_t icon_y;

	for (icon_y = 0U; icon_y < EST_MENU_ICON_HEIGHT; icon_y++) {
		for (icon_x = 0U; icon_x < EST_MENU_ICON_WIDTH; icon_x++) {
			bool on = bitmap_pixel(bitmap, icon_x, icon_y);

			(void)board_lcd_set_pixel((uint16_t)(x + icon_x),
				(uint16_t)(y + icon_y), inverse ? !on : on);
		}
	}
}

static void draw_home(const est_ui_state_t *state, const est_ui_view_t *view)
{
	uint8_t slot;

	draw_header(state, view);
	for (slot = 0U; slot < EST_UI_HOME_VISIBLE_ITEM_COUNT; slot++) {
		uint8_t index = (uint8_t)(state->home_first_item + slot);
		uint16_t zone_x = (uint16_t)(HOME_ZONE_X + slot * HOME_ZONE_WIDTH);
		uint16_t icon_x = (uint16_t)(zone_x +
			(HOME_ZONE_WIDTH - EST_MENU_ICON_WIDTH) / 2U);
		bool selected = index == state->home_item;
		est_ui_text_style_t label_style = selected ?
			EST_UI_TEXT_COMPACT_INVERSE : EST_UI_TEXT_COMPACT;
		uint16_t label_width = est_ui_text_width(state->language,
			home_labels[index], label_style);

		if (selected) {
			(void)board_lcd_draw_rectangle(zone_x, HOME_ZONE_Y,
				HOME_ZONE_WIDTH, HOME_ZONE_HEIGHT, true, true);
		}
		draw_home_icon(icon_x, HOME_ICON_Y, home_icons[index], selected);
		(void)est_ui_text_draw(centered_in_zone(zone_x, HOME_ZONE_WIDTH,
			label_width), HOME_LABEL_Y,
			state->language, home_labels[index], label_style);
	}
	if (state->home_item == 0U) {
		if (state->has_recent_program && view->recent_program_name != NULL) {
			char fitted[40];
			uint16_t width;

			fit_text(view->recent_program_name, BOARD_LCD_WIDTH - 6U,
				EST_UI_TEXT_COMPACT, fitted);
			width = est_ui_font_measure(fitted, EST_UI_TEXT_COMPACT);
			(void)est_ui_text_draw_raw(centered_x(width), HOME_DETAIL_Y,
				fitted, EST_UI_TEXT_COMPACT);
		} else {
			draw_text_centered(HOME_DETAIL_Y, state->language,
				EST_UI_STRING_HOME_RECENT_EMPTY, EST_UI_TEXT_COMPACT);
		}
	}
	for (slot = 0U; slot < 3U; slot++) {
		bool selected_window = state->home_first_item == slot;

		(void)board_lcd_draw_rectangle((uint16_t)(78U + slot * 9U), 105U,
			6U, 2U, true, selected_window);
	}
	draw_status(view);
}

static void draw_panel(uint16_t x, uint16_t y, uint16_t width,
	uint16_t height)
{
	(void)board_lcd_draw_rectangle((uint16_t)(x + 3U), (uint16_t)(y + 3U),
		width, height, true, true);
	(void)board_lcd_draw_rectangle(x, y, width, height, true, false);
	(void)board_lcd_draw_rectangle(x, y, width, height, false, true);
}

static void draw_confirm_mark(uint16_t x, uint16_t y)
{
	(void)board_lcd_draw_line(x, (uint16_t)(y + 5U), (uint16_t)(x + 3U),
		(uint16_t)(y + 8U), true);
	(void)board_lcd_draw_line((uint16_t)(x + 3U), (uint16_t)(y + 8U),
		(uint16_t)(x + 9U), y, true);
}

static void draw_power_confirm(const est_ui_state_t *state)
{
	const uint16_t x = 18U;
	const uint16_t y = 38U;
	uint16_t title_width = est_ui_text_width(state->language,
		EST_UI_STRING_POWER_TITLE, EST_UI_TEXT_NORMAL);

	draw_panel(x, y, 144U, 52U);
	(void)est_ui_text_draw(centered_in_zone(x, 144U, title_width), y + 8U,
		state->language, EST_UI_STRING_POWER_TITLE, EST_UI_TEXT_NORMAL);
	(void)board_lcd_draw_line(x + 7U, y + 31U, x + 136U, y + 31U, true);
	(void)est_ui_text_draw_raw(x + 18U, y + 34U, "<", EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw(x + 30U, y + 34U, state->language,
		EST_UI_STRING_CANCEL, EST_UI_TEXT_COMPACT);
	draw_confirm_mark(x + 83U, y + 35U);
	(void)est_ui_text_draw(x + 96U, y + 34U, state->language,
		EST_UI_STRING_POWER_OFF_SHORT, EST_UI_TEXT_COMPACT);
}

static void draw_page_title(const est_ui_state_t *state,
	est_ui_string_id_t title)
{
	(void)est_ui_text_draw(3U, 1U, state->language, title,
		EST_UI_TEXT_NORMAL);
	(void)board_lcd_draw_line(0U, 18U, BOARD_LCD_WIDTH - 1U, 18U, true);
}

static void draw_programs(const est_ui_state_t *state)
{
	uint8_t first;
	uint8_t row;

	draw_page_title(state, EST_UI_STRING_PROGRAMS_TITLE);
	draw_text_right(1U, state, EST_UI_STRING_DELETE_HINT);
	if (ui_view.programs_state == EST_UI_VIEW_PROGRAMS_SCANNING) {
		draw_text_centered(56U, state->language,
			EST_UI_STRING_PROGRAMS_LOADING, EST_UI_TEXT_NORMAL);
		return;
	}
	if (ui_view.programs_state == EST_UI_VIEW_PROGRAMS_ERROR) {
		char error[8];

		format_error(ui_view.programs_error_code, error);
		draw_text_centered(38U, state->language,
			EST_UI_STRING_STORAGE_ERROR, EST_UI_TEXT_NORMAL);
		draw_text_centered(62U, state->language,
			EST_UI_STRING_PROGRAM_LIST_UNREADABLE,
			EST_UI_TEXT_COMPACT);
		(void)est_ui_text_draw_raw(centered_x(est_ui_font_measure(error,
			EST_UI_TEXT_NORMAL)), 88U, error, EST_UI_TEXT_NORMAL);
		return;
	}
	if (ui_view.program_count == 0U) {
		draw_text_centered(56U, state->language,
			EST_UI_STRING_PROGRAMS_EMPTY, EST_UI_TEXT_NORMAL);
		return;
	}
	first = state->program_item < 5U ? 0U :
		(uint8_t)(state->program_item - 4U);
	for (row = 0U; row < 5U && first + row < ui_view.program_count; row++) {
		uint8_t index = (uint8_t)(first + row);
		uint16_t y = (uint16_t)(22U + row * 20U);
		bool selected = index == state->program_item;
		est_ui_text_style_t style = selected ?
			EST_UI_TEXT_COMPACT_INVERSE : EST_UI_TEXT_COMPACT;
		char slot[3] = {(char)('0' + ui_view.program_slots[index]), ':', '\0'};

		if (selected) {
			(void)board_lcd_draw_rectangle(1U, y,
				BOARD_LCD_WIDTH - 2U, EST_UI_FONT_HEIGHT, true, true);
		}
		(void)est_ui_text_draw_raw(4U, y, slot, style);
		draw_raw_fitted(22U, y, ui_view.program_names[index],
			BOARD_LCD_WIDTH - 26U, style);
	}
}

static void draw_delete_confirm(const est_ui_state_t *state)
{
	draw_page_title(state, EST_UI_STRING_DELETE_TITLE);
	if (state->program_item < ui_view.program_count) {
		char fitted[40];
		uint16_t width;

		fit_text(ui_view.program_names[state->program_item],
			BOARD_LCD_WIDTH - 8U, EST_UI_TEXT_COMPACT, fitted);
		width = est_ui_font_measure(fitted, EST_UI_TEXT_COMPACT);
		(void)est_ui_text_draw_raw(centered_x(width), 27U, fitted,
			EST_UI_TEXT_COMPACT);
	}
	draw_text_centered(49U, state->language,
		EST_UI_STRING_DELETE_IRREVERSIBLE, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw(23U, 94U, state->language,
		EST_UI_STRING_CANCEL, state->delete_choice == 0U ?
		EST_UI_TEXT_INVERSE : EST_UI_TEXT_NORMAL);
	(void)est_ui_text_draw(112U, 94U, state->language,
		EST_UI_STRING_DELETE, state->delete_choice != 0U ?
		EST_UI_TEXT_INVERSE : EST_UI_TEXT_NORMAL);
}

static void draw_running(const est_ui_state_t *state)
{
	draw_text_centered(56U, state->language, EST_UI_STRING_RUNNING,
		EST_UI_TEXT_NORMAL);
}

static void draw_port_selector(uint8_t selected)
{
	uint8_t index;

	for (index = 0U; index < 8U; index++) {
		char label[2] = {index < 4U ?
			(char)((uint8_t)'A' + index) :
			(char)((uint8_t)'1' + index - 4U), '\0'};
		uint16_t x = (uint16_t)(2U + index * 22U);
		bool inverse = selected == index;

		if (inverse) {
			(void)board_lcd_draw_rectangle(x, 21U, 20U, 16U, true, true);
		}
		(void)est_ui_text_draw_raw((uint16_t)(x + 7U), 21U, label, inverse ?
			EST_UI_TEXT_INVERSE : EST_UI_TEXT_NORMAL);
	}
	(void)board_lcd_draw_line(0U, 38U, BOARD_LCD_WIDTH - 1U, 38U, true);
}

static void draw_text_right(uint16_t y, const est_ui_state_t *state,
	est_ui_string_id_t text)
{
	uint16_t width = est_ui_text_width(state->language, text,
		EST_UI_TEXT_COMPACT);

	(void)est_ui_text_draw((uint16_t)(BOARD_LCD_WIDTH - width - 3U), y,
		state->language, text, EST_UI_TEXT_COMPACT);
}

static void draw_motor_detail(const est_ui_state_t *state,
	uint8_t port, const est_ui_motor_port_view_t *motor)
{
	char port_label[3] = {(char)('A' + port), ':', '\0'};
	char speed[9] = "--";
	char angle[14] = "--";
	uint8_t index;
	est_ui_string_id_t motor_state = EST_UI_STRING_DISCONNECTED;
	int32_t display_angle = motor->angle_degrees;
	const char *kind = "--";

	if (motor->connection == EST_UI_PORT_DETECTING) {
		motor_state = EST_UI_STRING_DETECTING;
	} else if (motor->connection == EST_UI_PORT_CONNECTED) {
		kind = est_ui_text(state->language,
			motor->kind == EST_UI_MOTOR_LARGE ? EST_UI_STRING_LARGE_MOTOR :
			EST_UI_STRING_MEDIUM_MOTOR);
		motor_state = motor->state == EST_UI_MOTOR_DRIVE ?
			EST_UI_STRING_ACTIVE : motor->state == EST_UI_MOTOR_BRAKE ?
			EST_UI_STRING_BRAKING : EST_UI_STRING_STOPPED;
		index = append_int(speed, 0U, sizeof(speed), motor->speed_percent);
		if (index + 1U < sizeof(speed)) {
			speed[index++] = '%';
			speed[index] = '\0';
		}
		if (display_angle > 999) {
			display_angle = 999;
		} else if (display_angle < -999) {
			display_angle = -999;
		}
		(void)append_int(angle, 0U, sizeof(angle), display_angle);
	}
	(void)est_ui_text_draw_raw(4U, 47U, port_label, EST_UI_TEXT_COMPACT);
	draw_raw_fitted(24U, 47U, kind, 96U, EST_UI_TEXT_COMPACT);
	draw_text_right(47U, state, motor_state);
	(void)est_ui_text_draw_raw(4U, 70U, "SPEED:", EST_UI_TEXT_COMPACT);
	draw_raw_right(70U, speed, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw(4U, 94U, "ANGLE:", EST_UI_TEXT_COMPACT);
	draw_raw_right(94U, angle, EST_UI_TEXT_COMPACT);
}

static void draw_sensor_detail(const est_ui_state_t *state,
	uint8_t port, const est_ui_sensor_port_view_t *sensor)
{
	char port_label[3] = {(char)('1' + port), ':', '\0'};
	est_ui_string_id_t connection = EST_UI_STRING_DISCONNECTED;
	const char *model = "--";
	const char *mode = "--";
	const char *value = "--";
	char fitted[40];
	uint16_t width;

	if (sensor->connection == EST_UI_PORT_DETECTING) {
		connection = EST_UI_STRING_DETECTING;
	} else if (sensor->connection == EST_UI_PORT_CONNECTED &&
		   sensor->model != NULL) {
		connection = EST_UI_STRING_CONNECTED;
		model = sensor->model;
		mode = sensor->mode != NULL ? sensor->mode : "VALUE";
		value = sensor->value;
	}
	(void)est_ui_text_draw_raw(4U, 47U, port_label, EST_UI_TEXT_COMPACT);
	draw_raw_fitted(24U, 47U, model, 96U, EST_UI_TEXT_COMPACT);
	draw_text_right(47U, state, connection);
	(void)est_ui_text_draw_raw(4U, 68U, "TYPE:", EST_UI_TEXT_COMPACT);
	draw_raw_right(68U, mode, EST_UI_TEXT_COMPACT);
	fit_text(value, BOARD_LCD_WIDTH - 8U, EST_UI_TEXT_NORMAL, fitted);
	width = est_ui_font_measure(fitted, EST_UI_TEXT_NORMAL);
	(void)est_ui_text_draw_raw(centered_x(width), 87U, fitted,
		EST_UI_TEXT_NORMAL);
	if (sensor->connection == EST_UI_PORT_CONNECTED) {
		draw_raw_right(107U, "OK TYPE", EST_UI_TEXT_COMPACT);
	}
}

static void draw_ports(const est_ui_state_t *state)
{
	draw_page_title(state, EST_UI_STRING_PORTS_TITLE);
	draw_port_selector(state->port_item);
	if (state->port_item < 4U) {
		draw_motor_detail(state, state->port_item,
			&ui_view.ports.motors[state->port_item]);
	} else {
		uint8_t sensor_port = (uint8_t)(state->port_item - 4U);

		draw_sensor_detail(state, sensor_port,
			&ui_view.ports.sensors[sensor_port]);
	}
}

static void draw_remote_header(const est_ui_state_t *state,
	const est_ui_view_t *view)
{
	char battery[10] = "BAT --";
	const char *channel = "CH1";
	uint16_t battery_width;

	(void)est_ui_text_draw(3U, 1U, state->language,
		EST_UI_STRING_REMOTE_TITLE, EST_UI_TEXT_NORMAL);
	if (view->battery_valid) {
		uint8_t index = append_uint(battery, 4U, sizeof(battery),
			view->battery_percent);

		if (index + 1U < sizeof(battery)) {
			battery[index++] = '%';
			battery[index] = '\0';
		}
	}
	battery_width = est_ui_font_measure(battery, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw((uint16_t)(BOARD_LCD_WIDTH - battery_width -
		est_ui_font_measure(channel, EST_UI_TEXT_COMPACT) - 12U), 1U,
		channel, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw((uint16_t)(BOARD_LCD_WIDTH - battery_width - 3U),
		1U, battery, view->battery_low ? EST_UI_TEXT_COMPACT_INVERSE :
		EST_UI_TEXT_COMPACT);
	(void)board_lcd_draw_line(0U, 18U, BOARD_LCD_WIDTH - 1U, 18U, true);
}

static void draw_remote_row(uint16_t y, const char *label, const char *value)
{
	(void)est_ui_text_draw_raw(4U, y, label, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw((uint16_t)(4U +
		est_ui_font_measure(label, EST_UI_TEXT_COMPACT)), y, ":",
		EST_UI_TEXT_COMPACT);
	draw_raw_right(y, value, EST_UI_TEXT_COMPACT);
}

static void draw_remote(const est_ui_state_t *state,
	const est_ui_view_t *view)
{
	const char *motor_group = state->remote_motor_group == 0U ? "B / C" :
		"A / D";
	const char *next_group = state->remote_motor_group == 0U ? "A / D" :
		"B / C";

	draw_remote_header(state, view);
	if (view->remote_fault != EST_UI_REMOTE_FAULT_NONE) {
		est_ui_string_id_t message = view->remote_fault ==
			EST_UI_REMOTE_FAULT_SIGNAL_LOST ?
			EST_UI_STRING_REMOTE_SIGNAL_LOST : view->remote_fault ==
			EST_UI_REMOTE_FAULT_DEVICE_LOST ?
			EST_UI_STRING_REMOTE_DEVICE_LOST :
			EST_UI_STRING_REMOTE_CONNECT_IR;

		draw_text_centered(43U, state->language, message, EST_UI_TEXT_NORMAL);
		draw_text_centered(71U, state->language,
			EST_UI_STRING_REMOTE_BRAKE_ALL, EST_UI_TEXT_COMPACT);
		(void)est_ui_text_draw_raw(66U, 94U, "PWR: --",
			EST_UI_TEXT_COMPACT);
	} else {
		draw_remote_row(25U, "IR 4", est_ui_text(state->language,
			EST_UI_STRING_CONNECTED));
		draw_remote_row(42U, "CH1", motor_group);
		draw_remote_row(65U, "PWR", "100%");
		draw_remote_row(88U, "STOP", est_ui_text(state->language,
			EST_UI_STRING_BRAKING));
	}
	draw_raw_right(112U, next_group, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw((uint16_t)(BOARD_LCD_WIDTH -
		est_ui_font_measure(next_group, EST_UI_TEXT_COMPACT) - 22U), 112U,
		"OK", EST_UI_TEXT_COMPACT);
}

static void draw_motor_output_box(const est_ui_state_t *state, uint8_t port,
	uint16_t x, uint16_t y)
{
	est_ui_motor_output_state_t output = state->motor_output_states[port];
	est_ui_string_id_t state_text = output == EST_UI_MOTOR_OUTPUT_FORWARD ?
		EST_UI_STRING_MOTOR_FORWARD : output == EST_UI_MOTOR_OUTPUT_REVERSE ?
		EST_UI_STRING_MOTOR_REVERSE : EST_UI_STRING_MOTOR_STOP;
	const char *translated = est_ui_text(state->language, state_text);
	char label[24];
	uint16_t width;
	bool inverse = output != EST_UI_MOTOR_OUTPUT_STOP;

	label[0] = (char)('A' + port);
	label[1] = ' ';
	copy_text(&label[2], sizeof(label) - 2U, translated);
	(void)board_lcd_draw_rectangle(x, y, 48U, 21U, false, true);
	if (inverse) {
		(void)board_lcd_draw_rectangle(x + 1U, y + 1U, 46U, 19U,
			true, true);
	}
	width = est_ui_font_measure(label, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw(centered_in_zone(x, 48U, width), y + 3U,
		label, inverse ? EST_UI_TEXT_COMPACT_INVERSE : EST_UI_TEXT_COMPACT);
}

static void draw_motor_output(const est_ui_state_t *state)
{
	char power[6];

	draw_page_title(state, EST_UI_STRING_MOTOR_OUTPUT_TITLE);
	draw_motor_output_box(state, 0U, 66U, 27U);
	draw_motor_output_box(state, 1U, 12U, 55U);
	draw_motor_output_box(state, 2U, 120U, 55U);
	draw_motor_output_box(state, 3U, 66U, 83U);
	format_percent((uint8_t)(25U *
		(state->motor_output_power_index + 1U)), power);
	(void)est_ui_text_draw_raw(35U, 111U, "OK", EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw(57U, 111U, state->language,
		EST_UI_STRING_MOTOR_POWER, EST_UI_TEXT_COMPACT);
	draw_raw_right(111U, power, EST_UI_TEXT_COMPACT);
}

static void draw_program_transfer(const est_ui_state_t *state,
	const est_ui_view_t *view)
{
	const uint16_t x = 20U;
	const uint16_t y = 39U;
	uint16_t title_width = est_ui_text_width(state->language,
		EST_UI_STRING_RECEIVING_PROGRAM, EST_UI_TEXT_NORMAL);
	uint16_t body_width = est_ui_font_measure("USB -> EST",
		EST_UI_TEXT_COMPACT);
	uint16_t fill_width = (uint16_t)((uint32_t)106U *
		view->transfer_progress / 100U);

	draw_panel(x, y, 140U, 52U);
	(void)est_ui_text_draw(centered_in_zone(x, 140U, title_width), y + 8U,
		state->language, EST_UI_STRING_RECEIVING_PROGRAM,
		EST_UI_TEXT_NORMAL);
	(void)est_ui_text_draw_raw(centered_in_zone(x, 140U, body_width), y + 25U,
		"USB -> EST", EST_UI_TEXT_COMPACT);
	(void)board_lcd_draw_rectangle(x + 16U, y + 40U, 108U, 5U, false, true);
	if (fill_width != 0U) {
		(void)board_lcd_draw_rectangle(x + 17U, y + 41U, fill_width, 3U,
			true, true);
	}
}

static void draw_setting_row(const est_ui_state_t *state, uint8_t index,
	est_ui_string_id_t label, const char *value)
{
	uint16_t y = (uint16_t)(CONTENT_TOP + index * ROW_HEIGHT);
	bool selected = state->settings_item == index;
	est_ui_text_style_t style = selected ? EST_UI_TEXT_INVERSE :
		EST_UI_TEXT_NORMAL;
	uint16_t value_width = est_ui_font_measure(value, style);

	if (selected) {
		(void)board_lcd_draw_rectangle(1U, y, BOARD_LCD_WIDTH - 2U,
			EST_UI_FONT_HEIGHT, true, true);
	}
	(void)est_ui_text_draw(4U, y, state->language, label, style);
	(void)est_ui_text_draw_raw((uint16_t)(BOARD_LCD_WIDTH - value_width - 4U),
		y, value, style);
}

static void draw_settings(const est_ui_state_t *state)
{
	char backlight[6];
	char volume[6];
	const char *language = "English";

	if (state->language == EST_UI_LANGUAGE_CHINESE) {
		language = est_ui_text(EST_UI_LANGUAGE_CHINESE,
			EST_UI_STRING_CHINESE);
	} else if (state->language == EST_UI_LANGUAGE_PORTUGUESE) {
		language = est_ui_text(EST_UI_LANGUAGE_PORTUGUESE,
			EST_UI_STRING_PORTUGUESE);
	}

	format_percent(state->backlight_percent, backlight);
	format_percent(state->volume_percent, volume);
	draw_page_title(state, EST_UI_STRING_SETTINGS_TITLE);
	draw_setting_row(state, 0U, EST_UI_STRING_BACKLIGHT, backlight);
	draw_setting_row(state, 1U, EST_UI_STRING_VOLUME, volume);
	draw_setting_row(state, 2U, EST_UI_STRING_LANGUAGE, language);
	draw_setting_row(state, 3U, EST_UI_STRING_DEVICE_INFO, ">");
}

static void draw_info_row(const est_ui_state_t *state, uint16_t y,
	est_ui_string_id_t label, const char *value)
{
	(void)est_ui_text_draw(5U, y, state->language, label, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw(80U, y, value, EST_UI_TEXT_COMPACT);
}

static void draw_device_info(const est_ui_state_t *state,
	const est_ui_view_t *view)
{
	char battery[6] = "--";

	if (view->battery_valid) {
		format_percent(view->battery_percent, battery);
	}
	draw_page_title(state, EST_UI_STRING_DEVICE_INFO);
	draw_info_row(state, 24U, EST_UI_STRING_MODEL, "EST 3.0");
	draw_info_row(state, 44U, EST_UI_STRING_FIRMWARE, view->app_version);
	draw_info_row(state, 64U, EST_UI_STRING_BOOTLOADER,
		view->bootloader_version);
	draw_info_row(state, 84U, EST_UI_STRING_USB,
		view->usb_connected ? "USB ON" : "USB OFF");
	draw_info_row(state, 104U, EST_UI_STRING_BATTERY, battery);
}

static void draw_error(const est_ui_state_t *state)
{
	char error[8];

	format_error(state->error_code, error);
	draw_text_centered(30U, state->language,
		EST_UI_STRING_PROGRAM_STOPPED, EST_UI_TEXT_NORMAL);
	draw_text_centered(53U, state->language,
		EST_UI_STRING_MOTORS_SAFE, EST_UI_TEXT_COMPACT);
	(void)est_ui_text_draw_raw(centered_x(est_ui_font_measure(error,
		EST_UI_TEXT_NORMAL)), 78U, error, EST_UI_TEXT_NORMAL);
	draw_text_centered(105U, state->language, EST_UI_STRING_RETRY,
		EST_UI_TEXT_COMPACT);
}

void est_ui_renderer_render(const est_ui_state_t *state,
	const est_ui_view_t *view)
{
	if (state == NULL || view == NULL || view->app_version == NULL ||
	    view->bootloader_version == NULL) {
		return;
	}
	ui_view = *view;
	board_lcd_clear();
	switch (state->page) {
	case EST_UI_PAGE_HOME:
		draw_home(state, view);
		break;
	case EST_UI_PAGE_POWER_CONFIRM:
		draw_home(state, view);
		draw_power_confirm(state);
		break;
	case EST_UI_PAGE_PROGRAMS:
		draw_programs(state);
		break;
	case EST_UI_PAGE_DELETE_CONFIRM:
		draw_delete_confirm(state);
		break;
	case EST_UI_PAGE_RUNNING:
		draw_running(state);
		break;
	case EST_UI_PAGE_PORTS:
		draw_ports(state);
		break;
	case EST_UI_PAGE_REMOTE:
		draw_remote(state, view);
		break;
	case EST_UI_PAGE_MOTOR_OUTPUT:
		draw_motor_output(state);
		break;
	case EST_UI_PAGE_SETTINGS:
		draw_settings(state);
		break;
	case EST_UI_PAGE_DEVICE_INFO:
		draw_device_info(state, view);
		break;
	case EST_UI_PAGE_ERROR:
		draw_error(state);
		break;
	default:
		break;
	}
	if (view->transfer_active) {
		draw_program_transfer(state, view);
	}
	board_lcd_refresh();
}
