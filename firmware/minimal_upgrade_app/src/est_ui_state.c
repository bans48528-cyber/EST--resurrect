#include <stdbool.h>
#include <stdint.h>

#include "est_ui_state.h"

#define EST_UI_DEFAULT_BACKLIGHT_PERCENT 100U
#define EST_UI_DEFAULT_VOLUME_PERCENT 80U
#define EST_UI_SETTING_STEP_PERCENT 10U

static uint8_t previous_wrapped(uint8_t value, uint8_t count)
{
	return value == 0U ? (uint8_t)(count - 1U) : (uint8_t)(value - 1U);
}

static uint8_t next_wrapped(uint8_t value, uint8_t count)
{
	return (uint8_t)((value + 1U) % count);
}

static void update_home_window(est_ui_state_t *state)
{
	if (state->home_item < state->home_first_item) {
		state->home_first_item = state->home_item;
	} else if (state->home_item >= state->home_first_item +
		   EST_UI_HOME_VISIBLE_ITEM_COUNT) {
		state->home_first_item = (uint8_t)(state->home_item -
			EST_UI_HOME_VISIBLE_ITEM_COUNT + 1U);
	}
}

static void set_page(est_ui_state_t *state, est_ui_page_t page)
{
	state->page = page;
	state->dirty = true;
}

void est_ui_state_init(est_ui_state_t *state)
{
	if (state == NULL) {
		return;
	}
	state->page = EST_UI_PAGE_HOME;
	state->error_return_page = EST_UI_PAGE_HOME;
	state->language = EST_UI_LANGUAGE_ENGLISH;
	state->home_item = 0U;
	state->home_first_item = 0U;
	state->settings_item = 0U;
	state->program_item = 0U;
	state->program_count = 0U;
	state->delete_choice = 0U;
	state->port_item = 0U;
	state->remote_group = 0U;
	state->motor_output_power_index = 0U;
	state->motor_output_states[0] = EST_UI_MOTOR_OUTPUT_STOP;
	state->motor_output_states[1] = EST_UI_MOTOR_OUTPUT_STOP;
	state->motor_output_states[2] = EST_UI_MOTOR_OUTPUT_STOP;
	state->motor_output_states[3] = EST_UI_MOTOR_OUTPUT_STOP;
	state->backlight_percent = EST_UI_DEFAULT_BACKLIGHT_PERCENT;
	state->volume_percent = EST_UI_DEFAULT_VOLUME_PERCENT;
	state->error_code = 0U;
	state->has_recent_program = false;
	state->dirty = true;
}

void est_ui_state_set_recent(est_ui_state_t *state, bool available)
{
	if (state != NULL && state->has_recent_program != available) {
		state->has_recent_program = available;
		state->dirty = true;
	}
}

void est_ui_state_set_program_count(est_ui_state_t *state, uint8_t count)
{
	if (state == NULL) {
		return;
	}
	state->program_count = count;
	if (count == 0U) {
		state->program_item = 0U;
	} else if (state->program_item >= count) {
		state->program_item = (uint8_t)(count - 1U);
	}
	state->dirty = true;
}

void est_ui_state_set_home(est_ui_state_t *state)
{
	if (state != NULL) {
		set_page(state, EST_UI_PAGE_HOME);
	}
}

void est_ui_state_set_programs(est_ui_state_t *state)
{
	if (state != NULL) {
		set_page(state, EST_UI_PAGE_PROGRAMS);
	}
}

void est_ui_state_set_running(est_ui_state_t *state)
{
	if (state != NULL) {
		set_page(state, EST_UI_PAGE_RUNNING);
	}
}

void est_ui_state_set_error(est_ui_state_t *state, uint16_t error_code,
	est_ui_page_t return_page)
{
	if (state == NULL) {
		return;
	}
	state->error_code = error_code;
	state->error_return_page = return_page;
	set_page(state, EST_UI_PAGE_ERROR);
}

static est_ui_action_t handle_home(est_ui_state_t *state, est_button_t button)
{
	if (button == EST_BUTTON_BACK) {
		set_page(state, EST_UI_PAGE_POWER_CONFIRM);
	} else if (button == EST_BUTTON_LEFT || button == EST_BUTTON_UP) {
		state->home_item = previous_wrapped(state->home_item,
			EST_UI_HOME_ITEM_COUNT);
		update_home_window(state);
		state->dirty = true;
	} else if (button == EST_BUTTON_RIGHT || button == EST_BUTTON_DOWN) {
		state->home_item = next_wrapped(state->home_item,
			EST_UI_HOME_ITEM_COUNT);
		update_home_window(state);
		state->dirty = true;
	} else if (button == EST_BUTTON_CONFIRM) {
		if (state->home_item == 0U) {
			return state->has_recent_program ?
				EST_UI_ACTION_RUN_RECENT : EST_UI_ACTION_NONE;
		}
		if (state->home_item == 1U) {
			set_page(state, EST_UI_PAGE_PROGRAMS);
			return EST_UI_ACTION_REFRESH_PROGRAMS;
		}
		if (state->home_item == 2U) {
			set_page(state, EST_UI_PAGE_PORTS);
		} else if (state->home_item == 3U) {
			state->remote_group = 0U;
			set_page(state, EST_UI_PAGE_REMOTE);
			return EST_UI_ACTION_ENTER_REMOTE;
		} else if (state->home_item == 4U) {
			uint8_t index;

			for (index = 0U; index < EST_UI_MOTOR_OUTPUT_PORT_COUNT;
			     index++) {
				state->motor_output_states[index] =
					EST_UI_MOTOR_OUTPUT_STOP;
			}
			state->motor_output_power_index = 0U;
			set_page(state, EST_UI_PAGE_MOTOR_OUTPUT);
			return EST_UI_ACTION_ENTER_MOTOR_OUTPUT;
		} else {
			set_page(state, EST_UI_PAGE_SETTINGS);
		}
	}
	return EST_UI_ACTION_NONE;
}

static est_ui_action_t handle_programs(est_ui_state_t *state,
	est_button_t button)
{
	if (button == EST_BUTTON_BACK) {
		set_page(state, EST_UI_PAGE_HOME);
	} else if (state->program_count != 0U && button == EST_BUTTON_UP) {
		state->program_item = previous_wrapped(state->program_item,
			state->program_count);
		state->dirty = true;
	} else if (state->program_count != 0U && button == EST_BUTTON_DOWN) {
		state->program_item = next_wrapped(state->program_item,
			state->program_count);
		state->dirty = true;
	} else if (state->program_count != 0U && button == EST_BUTTON_CONFIRM) {
		return EST_UI_ACTION_RUN_SELECTED;
	} else if (state->program_count != 0U && button == EST_BUTTON_RIGHT) {
		state->delete_choice = 0U;
		set_page(state, EST_UI_PAGE_DELETE_CONFIRM);
	}
	return EST_UI_ACTION_NONE;
}

static est_ui_action_t handle_delete_confirm(est_ui_state_t *state,
	est_button_t button)
{
	if (button == EST_BUTTON_BACK) {
		set_page(state, EST_UI_PAGE_PROGRAMS);
	} else if (button == EST_BUTTON_LEFT || button == EST_BUTTON_RIGHT) {
		state->delete_choice ^= 1U;
		state->dirty = true;
	} else if (button == EST_BUTTON_CONFIRM) {
		set_page(state, EST_UI_PAGE_PROGRAMS);
		if (state->delete_choice != 0U) {
			return EST_UI_ACTION_DELETE_SELECTED;
		}
	}
	return EST_UI_ACTION_NONE;
}

static est_ui_action_t handle_ports(est_ui_state_t *state,
	est_button_t button)
{
	if (button == EST_BUTTON_BACK) {
		set_page(state, EST_UI_PAGE_HOME);
	} else if (button == EST_BUTTON_LEFT) {
		state->port_item = previous_wrapped(state->port_item, 8U);
		state->dirty = true;
	} else if (button == EST_BUTTON_RIGHT) {
		state->port_item = next_wrapped(state->port_item, 8U);
		state->dirty = true;
	} else if (button == EST_BUTTON_CONFIRM && state->port_item >= 4U) {
		return EST_UI_ACTION_CYCLE_SENSOR_MODE;
	}
	return EST_UI_ACTION_NONE;
}

static est_ui_action_t handle_remote(est_ui_state_t *state,
	est_button_t button)
{
	if (button == EST_BUTTON_BACK) {
		set_page(state, EST_UI_PAGE_HOME);
		return EST_UI_ACTION_EXIT_REMOTE;
	}
	if (button == EST_BUTTON_CONFIRM) {
		state->remote_group ^= 1U;
		state->dirty = true;
		return EST_UI_ACTION_SWITCH_REMOTE_GROUP;
	}
	return EST_UI_ACTION_NONE;
}

static est_ui_action_t handle_motor_output(est_ui_state_t *state,
	est_button_t button)
{
	uint8_t port;

	if (button == EST_BUTTON_BACK) {
		set_page(state, EST_UI_PAGE_HOME);
		return EST_UI_ACTION_EXIT_MOTOR_OUTPUT;
	}
	if (button == EST_BUTTON_CONFIRM) {
		state->motor_output_power_index =
			(uint8_t)((state->motor_output_power_index + 1U) % 4U);
		state->dirty = true;
		return EST_UI_ACTION_UPDATE_MOTOR_OUTPUT;
	}
	if (button == EST_BUTTON_UP) {
		port = 0U;
	} else if (button == EST_BUTTON_LEFT) {
		port = 1U;
	} else if (button == EST_BUTTON_RIGHT) {
		port = 2U;
	} else if (button == EST_BUTTON_DOWN) {
		port = 3U;
	} else {
		return EST_UI_ACTION_NONE;
	}
	state->motor_output_states[port] =
		(est_ui_motor_output_state_t)((state->motor_output_states[port] +
		1U) % 3U);
	state->dirty = true;
	return EST_UI_ACTION_UPDATE_MOTOR_OUTPUT;
}

static est_ui_action_t change_setting(est_ui_state_t *state,
	bool increase)
{
	if (state->settings_item == 0U) {
		uint8_t value = state->backlight_percent;

		if (increase && value < 100U) {
			value = (uint8_t)(value + EST_UI_SETTING_STEP_PERCENT);
		} else if (!increase && value > 10U) {
			value = (uint8_t)(value - EST_UI_SETTING_STEP_PERCENT);
		}
		if (value != state->backlight_percent) {
			state->backlight_percent = value;
			state->dirty = true;
			return EST_UI_ACTION_APPLY_BACKLIGHT;
		}
	} else if (state->settings_item == 1U) {
		uint8_t value = state->volume_percent;

		if (increase && value < 100U) {
			value = (uint8_t)(value + EST_UI_SETTING_STEP_PERCENT);
		} else if (!increase && value > 0U) {
			value = (uint8_t)(value - EST_UI_SETTING_STEP_PERCENT);
		}
		if (value != state->volume_percent) {
			state->volume_percent = value;
			state->dirty = true;
			return EST_UI_ACTION_APPLY_VOLUME;
		}
	} else if (state->settings_item == 2U) {
		state->language = (est_ui_language_t)(increase ?
			next_wrapped((uint8_t)state->language, EST_UI_LANGUAGE_COUNT) :
			previous_wrapped((uint8_t)state->language,
				EST_UI_LANGUAGE_COUNT));
		state->dirty = true;
		return EST_UI_ACTION_SAVE_SETTINGS;
	}
	return EST_UI_ACTION_NONE;
}

static est_ui_action_t handle_settings(est_ui_state_t *state,
	est_button_t button)
{
	if (button == EST_BUTTON_BACK) {
		set_page(state, EST_UI_PAGE_HOME);
	} else if (button == EST_BUTTON_UP) {
		state->settings_item = previous_wrapped(state->settings_item,
			EST_UI_SETTINGS_ITEM_COUNT);
		state->dirty = true;
	} else if (button == EST_BUTTON_DOWN) {
		state->settings_item = next_wrapped(state->settings_item,
			EST_UI_SETTINGS_ITEM_COUNT);
		state->dirty = true;
	} else if (button == EST_BUTTON_LEFT) {
		return change_setting(state, false);
	} else if (button == EST_BUTTON_RIGHT) {
		return change_setting(state, true);
	} else if (button == EST_BUTTON_CONFIRM && state->settings_item == 3U) {
		set_page(state, EST_UI_PAGE_DEVICE_INFO);
	}
	return EST_UI_ACTION_NONE;
}

est_ui_action_t est_ui_state_handle_short(est_ui_state_t *state,
	est_button_t button)
{
	if (state == NULL || (uint32_t)button >= EST_BUTTON_COUNT) {
		return EST_UI_ACTION_NONE;
	}
	switch (state->page) {
	case EST_UI_PAGE_HOME:
		return handle_home(state, button);
	case EST_UI_PAGE_POWER_CONFIRM:
		if (button == EST_BUTTON_BACK) {
			set_page(state, EST_UI_PAGE_HOME);
		} else if (button == EST_BUTTON_CONFIRM) {
			return EST_UI_ACTION_POWER_OFF;
		}
		break;
	case EST_UI_PAGE_PROGRAMS:
		return handle_programs(state, button);
	case EST_UI_PAGE_DELETE_CONFIRM:
		return handle_delete_confirm(state, button);
	case EST_UI_PAGE_RUNNING:
		return button == EST_BUTTON_BACK ?
			EST_UI_ACTION_STOP_PROGRAM : EST_UI_ACTION_NONE;
	case EST_UI_PAGE_PORTS:
		return handle_ports(state, button);
	case EST_UI_PAGE_REMOTE:
		return handle_remote(state, button);
	case EST_UI_PAGE_MOTOR_OUTPUT:
		return handle_motor_output(state, button);
	case EST_UI_PAGE_SETTINGS:
		return handle_settings(state, button);
	case EST_UI_PAGE_DEVICE_INFO:
		if (button == EST_BUTTON_BACK) {
			set_page(state, EST_UI_PAGE_SETTINGS);
		}
		break;
	case EST_UI_PAGE_ERROR:
		if (button == EST_BUTTON_BACK) {
			set_page(state, state->error_return_page);
		} else if (button == EST_BUTTON_CONFIRM) {
			return EST_UI_ACTION_RETRY;
		}
		break;
	default:
		break;
	}
	return EST_UI_ACTION_NONE;
}

est_ui_action_t est_ui_state_handle_long(est_ui_state_t *state,
	est_button_t button)
{
	if (state == NULL || button != EST_BUTTON_BACK) {
		return EST_UI_ACTION_NONE;
	}
	set_page(state, EST_UI_PAGE_HOME);
	return EST_UI_ACTION_EMERGENCY_STOP;
}

bool est_ui_state_take_dirty(est_ui_state_t *state)
{
	bool dirty;

	if (state == NULL) {
		return false;
	}
	dirty = state->dirty;
	state->dirty = false;
	return dirty;
}

void est_ui_state_invalidate(est_ui_state_t *state)
{
	if (state != NULL) {
		state->dirty = true;
	}
}
