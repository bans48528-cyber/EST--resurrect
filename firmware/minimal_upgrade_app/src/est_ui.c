#include <stdbool.h>
#include <stdint.h>

#include "app_version.h"
#include "board_audio.h"
#include "est_backlight.h"
#include "est_battery.h"
#include "est_buttons.h"
#include "est_key_events.h"
#include "est_micropython.h"
#include "est_motor.h"
#include "est_program_store.h"
#include "est_screen.h"
#include "est_ui.h"
#include "est_ui_ports.h"
#include "est_ui_programs.h"
#include "est_ui_remote.h"
#include "est_ui_renderer.h"
#include "est_ui_settings.h"
#include "est_ui_state.h"
#include "usb_hid.h"

#define EST_UI_BOOTLOADER_VERSION "03.00B"
#define EST_UI_PORT_REFRESH_MS 100U
#define EST_UI_SETTINGS_SAVE_DELAY_MS 750U
#define EST_UI_SETTINGS_RETRY_DELAY_MS 5000U

static est_ui_state_t ui_state;
static est_ui_view_t ui_view;
static bool power_off_requested;
static bool view_initialized;
static uint32_t ui_now_ms;
static uint32_t next_port_refresh_ms;
static uint32_t settings_save_due_ms;
static uint8_t settings_recent_slot;
static bool settings_dirty;

static const uint8_t motor_output_power_levels[4] = {25U, 50U, 75U, 100U};

static bool update_view(void)
{
	est_battery_status_t battery = {0};
	bool changed = false;
	bool usb_connected = usb_hid_host_connected();

	(void)est_battery_get_status(&battery);
	if (!view_initialized || ui_view.usb_connected != usb_connected ||
	    ui_view.battery_valid != battery.valid ||
	    ui_view.battery_percent != battery.percent ||
	    ui_view.battery_low != battery.low) {
		changed = true;
	}
	ui_view.usb_connected = usb_connected;
	ui_view.battery_valid = battery.valid;
	ui_view.battery_percent = battery.percent;
	ui_view.battery_low = battery.low;
	view_initialized = true;
	return changed;
}

static void stop_everything(void)
{
	est_ui_remote_leave();
	(void)est_micropython_program_stop();
	(void)est_motor_stop_all(EST_STOP_COAST);
}

static void brake_all_menu_motors(void)
{
	(void)est_motor_stop_all(EST_STOP_BRAKE);
}

static void apply_motor_output(void)
{
	uint8_t port;
	int8_t magnitude = (int8_t)motor_output_power_levels[
		ui_state.motor_output_power_index];

	for (port = 0U; port < EST_UI_MOTOR_OUTPUT_PORT_COUNT; port++) {
		est_ui_motor_output_state_t state =
			ui_state.motor_output_states[port];
		est_result_t result;

		if (state == EST_UI_MOTOR_OUTPUT_STOP) {
			result = est_motor_stop((est_motor_port_t)port,
				EST_STOP_BRAKE);
		} else {
			int8_t power = state == EST_UI_MOTOR_OUTPUT_FORWARD ?
				magnitude : (int8_t)-magnitude;

			result = est_motor_set_power((est_motor_port_t)port, power);
		}
		if (result != EST_OK) {
			ui_state.motor_output_states[port] =
				EST_UI_MOTOR_OUTPUT_STOP;
			(void)est_motor_stop((est_motor_port_t)port,
				EST_STOP_BRAKE);
			est_ui_state_invalidate(&ui_state);
		}
	}
}

static void schedule_settings_save(void)
{
	settings_dirty = true;
	settings_save_due_ms = ui_now_ms + EST_UI_SETTINGS_SAVE_DELAY_MS;
}

static est_result_t save_settings_now(void)
{
	est_ui_settings_data_t settings = {
		ui_state.backlight_percent,
		ui_state.volume_percent,
		ui_state.language,
		settings_recent_slot
	};
	est_result_t result;

	if (!settings_dirty) {
		return EST_OK;
	}
	result = est_ui_settings_save(&settings);
	if (result == EST_OK) {
		settings_dirty = false;
	} else {
		settings_save_due_ms = ui_now_ms +
			EST_UI_SETTINGS_RETRY_DELAY_MS;
	}
	return result;
}

static void update_program_view(void)
{
	const est_ui_program_entry_t *recent;
	uint8_t index;

	for (index = 0U; index < EST_UI_VIEW_PROGRAM_COUNT; index++) {
		ui_view.program_names[index] = NULL;
		ui_view.program_slots[index] = 0U;
	}
	ui_view.program_count = est_ui_programs_count();
	ui_view.programs_error_code = est_ui_programs_error_code();
	switch (est_ui_programs_state()) {
	case EST_UI_PROGRAMS_READY:
		ui_view.programs_state = EST_UI_VIEW_PROGRAMS_READY;
		break;
	case EST_UI_PROGRAMS_ERROR:
		ui_view.programs_state = EST_UI_VIEW_PROGRAMS_ERROR;
		break;
	case EST_UI_PROGRAMS_SCANNING:
	default:
		ui_view.programs_state = EST_UI_VIEW_PROGRAMS_SCANNING;
		break;
	}
	for (index = 0U; index < ui_view.program_count; index++) {
		const est_ui_program_entry_t *entry = est_ui_programs_entry(index);

		if (entry != NULL) {
			ui_view.program_names[index] = entry->name;
			ui_view.program_slots[index] = entry->slot_id;
		}
	}
	recent = est_ui_programs_recent();
	ui_view.recent_program_name = recent != NULL ? recent->name : NULL;
	est_ui_state_set_recent(&ui_state, recent != NULL);
	est_ui_state_set_program_count(&ui_state, ui_view.program_count);
}

static void show_program_error(uint16_t error_code)
{
	est_ui_state_set_error(&ui_state, error_code, EST_UI_PAGE_PROGRAMS);
}

static void run_program_entry(const est_ui_program_entry_t *entry)
{
	est_result_t result;

	if (entry == NULL) {
		show_program_error(1101U);
		return;
	}
	if (ui_view.battery_valid && ui_view.battery_low) {
		show_program_error(1201U);
		return;
	}
	result = est_program_store_load(entry->slot_id);
	if (result == EST_OK) {
		result = est_micropython_program_run(
			EST_MICROPYTHON_PROGRAM_NO_TIMEOUT_MS);
	}
	if (result != EST_OK) {
		show_program_error(1101U);
		return;
	}
	est_ui_programs_set_recent_slot(entry->slot_id);
	ui_view.recent_program_name = entry->name;
	est_ui_state_set_recent(&ui_state, true);
	est_ui_state_set_running(&ui_state);
}

static void delete_selected_program(void)
{
	const est_ui_program_entry_t *entry =
		est_ui_programs_entry(ui_state.program_item);
	const est_ui_program_entry_t *recent = est_ui_programs_recent();

	if (entry == NULL || est_program_store_clear(entry->slot_id) != EST_OK) {
		show_program_error(1004U);
		return;
	}
	if (recent != NULL && recent->slot_id == entry->slot_id) {
		est_ui_programs_clear_recent();
	}
	est_ui_programs_request_scan(ui_now_ms);
	update_program_view();
}

static void handle_action(est_ui_action_t action)
{
	switch (action) {
	case EST_UI_ACTION_POWER_OFF:
		(void)save_settings_now();
		stop_everything();
		power_off_requested = true;
		break;
	case EST_UI_ACTION_EMERGENCY_STOP:
	case EST_UI_ACTION_STOP_PROGRAM:
		stop_everything();
		break;
	case EST_UI_ACTION_APPLY_BACKLIGHT:
		(void)est_backlight_set_percent(ui_state.backlight_percent);
		schedule_settings_save();
		break;
	case EST_UI_ACTION_RUN_RECENT:
		run_program_entry(est_ui_programs_recent());
		break;
	case EST_UI_ACTION_REFRESH_PROGRAMS:
		est_ui_programs_request_scan(ui_now_ms);
		update_program_view();
		break;
	case EST_UI_ACTION_RUN_SELECTED:
		run_program_entry(est_ui_programs_entry(ui_state.program_item));
		break;
	case EST_UI_ACTION_DELETE_SELECTED:
		delete_selected_program();
		break;
	case EST_UI_ACTION_RETRY:
		est_ui_state_set_programs(&ui_state);
		est_ui_programs_request_scan(ui_now_ms);
		update_program_view();
		break;
	case EST_UI_ACTION_CYCLE_SENSOR_MODE:
		if (ui_state.port_item >= 4U &&
		    est_ui_ports_cycle_sensor_mode(
			(uint8_t)(ui_state.port_item - 4U), ui_now_ms)) {
			next_port_refresh_ms = ui_now_ms;
		}
		break;
	case EST_UI_ACTION_ENTER_REMOTE:
		est_ui_remote_enter(ui_now_ms, ui_state.remote_motor_group);
		break;
	case EST_UI_ACTION_SWITCH_REMOTE_MOTOR_GROUP:
		est_ui_remote_switch_motor_group(ui_now_ms,
			ui_state.remote_motor_group);
		break;
	case EST_UI_ACTION_EXIT_REMOTE:
		est_ui_remote_leave();
		break;
	case EST_UI_ACTION_ENTER_MOTOR_OUTPUT:
		brake_all_menu_motors();
		break;
	case EST_UI_ACTION_UPDATE_MOTOR_OUTPUT:
		apply_motor_output();
		break;
	case EST_UI_ACTION_EXIT_MOTOR_OUTPUT:
		brake_all_menu_motors();
		break;
	case EST_UI_ACTION_APPLY_VOLUME:
		(void)board_audio_set_volume_percent(ui_state.volume_percent);
		schedule_settings_save();
		break;
	case EST_UI_ACTION_SAVE_SETTINGS:
		schedule_settings_save();
		break;
	case EST_UI_ACTION_NONE:
	default:
		break;
	}
}

static void handle_event_mask(uint8_t events, bool long_press)
{
	uint8_t index;

	for (index = 0U; index < EST_BUTTON_COUNT; index++) {
		uint8_t bit = (uint8_t)(1U << index);

		if ((events & bit) != 0U) {
			est_ui_action_t action = long_press ?
				est_ui_state_handle_long(&ui_state,
					(est_button_t)index) :
				est_ui_state_handle_short(&ui_state,
					(est_button_t)index);

			handle_action(action);
		}
	}
}

static void update_transfer_state(void)
{
	est_micropython_program_status_t status = {0};
	bool transfer_active = false;
	uint8_t progress = 0U;

	if (est_micropython_program_get_status(&status) &&
	    status.state == EST_MICROPYTHON_PROGRAM_RECEIVING) {
		transfer_active = true;
		if (status.expected_length != 0U) {
			progress = (uint8_t)(((uint32_t)status.received_length * 100U) /
				status.expected_length);
		}
	}
	if (ui_view.transfer_active != transfer_active ||
	    ui_view.transfer_progress != progress) {
		ui_view.transfer_active = transfer_active;
		ui_view.transfer_progress = progress;
		est_ui_state_invalidate(&ui_state);
	}
}

static void update_remote_state(void)
{
	est_ui_remote_view_t remote = {0};

	if (ui_state.page != EST_UI_PAGE_REMOTE) {
		return;
	}
	est_ui_remote_tick(ui_now_ms, ui_state.remote_motor_group, &remote);
	if (ui_view.remote_motor_group != remote.motor_group ||
	    ui_view.remote_code != remote.code ||
	    ui_view.remote_fault != (uint8_t)remote.fault ||
	    ui_view.remote_output_enabled != remote.output_enabled) {
		ui_view.remote_motor_group = remote.motor_group;
		ui_view.remote_code = remote.code;
		ui_view.remote_fault = (uint8_t)remote.fault;
		ui_view.remote_output_enabled = remote.output_enabled;
		est_ui_state_invalidate(&ui_state);
	}
}

static void update_program_state(void)
{
	est_micropython_program_status_t status = {0};

	if (!est_micropython_program_get_status(&status)) {
		return;
	}
	if (status.state == EST_MICROPYTHON_PROGRAM_QUEUED &&
	    ui_state.page != EST_UI_PAGE_RUNNING) {
		est_ui_state_set_running(&ui_state);
		est_screen_set_owner(EST_SCREEN_OWNER_MENU);
	}
	if (ui_state.page != EST_UI_PAGE_RUNNING ||
	    status.state == EST_MICROPYTHON_PROGRAM_QUEUED ||
	    status.state == EST_MICROPYTHON_PROGRAM_RUNNING) {
		return;
	}
	est_screen_set_owner(EST_SCREEN_OWNER_MENU);
	est_key_events_reset();
	if (status.state == EST_MICROPYTHON_PROGRAM_EXCEPTION ||
	    status.state == EST_MICROPYTHON_PROGRAM_TIMED_OUT ||
	    status.state == EST_MICROPYTHON_PROGRAM_INVALID) {
		if (status.error ==
		    EST_MICROPYTHON_PROGRAM_ERROR_PYTHON_EXCEPTION) {
			est_ui_state_set_python_error(&ui_state,
				status.exception_line, EST_UI_PAGE_PROGRAMS);
		} else {
			est_ui_state_set_error(&ui_state, (uint16_t)status.error,
				EST_UI_PAGE_PROGRAMS);
		}
	} else {
		est_ui_state_set_home(&ui_state);
	}
}

void est_ui_init(uint32_t now_ms)
{
	est_ui_settings_data_t settings = {
		100U,
		80U,
		EST_UI_LANGUAGE_ENGLISH,
		EST_UI_SETTINGS_NO_RECENT_SLOT
	};

	est_ui_state_init(&ui_state);
	if (est_ui_settings_load(&settings) == EST_OK) {
		ui_state.backlight_percent = settings.backlight_percent;
		ui_state.volume_percent = settings.volume_percent;
		ui_state.language = settings.language;
	}
	settings_recent_slot = settings.recent_program_slot;
	settings_dirty = false;
	settings_save_due_ms = now_ms;
	est_key_events_init();
	est_screen_init(EST_SCREEN_OWNER_MENU);
	ui_view.app_version = app_version_text;
	ui_view.bootloader_version = EST_UI_BOOTLOADER_VERSION;
	ui_view.recent_program_name = NULL;
	ui_view.program_count = 0U;
	ui_view.programs_state = EST_UI_VIEW_PROGRAMS_SCANNING;
	ui_view.programs_error_code = 0U;
	ui_view.remote_motor_group = 0U;
	ui_view.remote_code = 0U;
	ui_view.remote_fault = (uint8_t)EST_UI_REMOTE_FAULT_CONNECT_IR;
	ui_view.remote_output_enabled = false;
	ui_view.transfer_active = false;
	ui_view.transfer_progress = 0U;
	ui_view.battery_percent = 0U;
	ui_view.battery_valid = false;
	ui_view.battery_low = false;
	ui_view.usb_connected = false;
	power_off_requested = false;
	view_initialized = false;
	ui_now_ms = now_ms;
	est_ui_programs_init(now_ms);
	est_ui_remote_init();
	update_program_view();
	est_ui_ports_capture(&ui_view.ports);
	(void)est_backlight_set_percent(ui_state.backlight_percent);
	(void)board_audio_set_volume_percent(ui_state.volume_percent);
	next_port_refresh_ms = now_ms + EST_UI_PORT_REFRESH_MS;
	(void)update_view();
}

void est_ui_tick(uint32_t now_ms)
{
	bool view_changed;
	bool dirty;
	est_ui_page_t page_before_events;

	ui_now_ms = now_ms;
	est_key_events_tick();
	update_transfer_state();
	update_program_state();
	update_remote_state();
	if (est_ui_programs_tick(now_ms)) {
		update_program_view();
	}
	if ((int32_t)(now_ms - next_port_refresh_ms) >= 0) {
		est_ui_ports_capture(&ui_view.ports);
		next_port_refresh_ms = now_ms + EST_UI_PORT_REFRESH_MS;
		if (ui_state.page == EST_UI_PAGE_PORTS) {
			est_ui_state_invalidate(&ui_state);
		}
	}
	if (ui_view.transfer_active) {
		est_key_events_reset();
	} else if (ui_state.page != EST_UI_PAGE_RUNNING) {
		page_before_events = ui_state.page;
		handle_event_mask(est_key_events_take_long(), true);
		handle_event_mask(est_key_events_take_short(), false);
		if (page_before_events == EST_UI_PAGE_SETTINGS &&
		    ui_state.page != EST_UI_PAGE_SETTINGS && settings_dirty) {
			settings_save_due_ms = now_ms;
		}
	}
	if (settings_dirty &&
	    (int32_t)(now_ms - settings_save_due_ms) >= 0) {
		(void)save_settings_now();
	}
	view_changed = update_view();
	dirty = est_ui_state_take_dirty(&ui_state);
	if (est_screen_is_owner(EST_SCREEN_OWNER_MENU) &&
	    (dirty || view_changed)) {
		est_ui_renderer_render(&ui_state, &ui_view);
		if (ui_state.page == EST_UI_PAGE_RUNNING) {
			est_screen_set_owner(EST_SCREEN_OWNER_PROGRAM);
		}
	}
}

bool est_ui_power_off_requested(void)
{
	return power_off_requested;
}
