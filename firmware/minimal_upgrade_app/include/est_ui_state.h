#ifndef EST_UI_STATE_H
#define EST_UI_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "est_buttons.h"
#include "est_ui_text.h"

#define EST_UI_HOME_ITEM_COUNT 6U
#define EST_UI_HOME_VISIBLE_ITEM_COUNT 4U
#define EST_UI_SETTINGS_ITEM_COUNT 4U
#define EST_UI_MOTOR_OUTPUT_PORT_COUNT 4U
#define EST_UI_ERROR_SOURCE_LINE_FLAG 0x8000U
#define EST_UI_ERROR_SOURCE_LINE_MASK 0x7FFFU

typedef enum {
	EST_UI_PAGE_HOME = 0,
	EST_UI_PAGE_POWER_CONFIRM,
	EST_UI_PAGE_PROGRAMS,
	EST_UI_PAGE_DELETE_CONFIRM,
	EST_UI_PAGE_RUNNING,
	EST_UI_PAGE_PORTS,
	EST_UI_PAGE_REMOTE,
	EST_UI_PAGE_MOTOR_OUTPUT,
	EST_UI_PAGE_SETTINGS,
	EST_UI_PAGE_DEVICE_INFO,
	EST_UI_PAGE_ERROR
} est_ui_page_t;

typedef enum {
	EST_UI_ACTION_NONE = 0,
	EST_UI_ACTION_RUN_RECENT,
	EST_UI_ACTION_REFRESH_PROGRAMS,
	EST_UI_ACTION_RUN_SELECTED,
	EST_UI_ACTION_DELETE_SELECTED,
	EST_UI_ACTION_STOP_PROGRAM,
	EST_UI_ACTION_POWER_OFF,
	EST_UI_ACTION_EMERGENCY_STOP,
	EST_UI_ACTION_APPLY_BACKLIGHT,
	EST_UI_ACTION_APPLY_VOLUME,
	EST_UI_ACTION_SAVE_SETTINGS,
	EST_UI_ACTION_RETRY,
	EST_UI_ACTION_CYCLE_SENSOR_MODE,
	EST_UI_ACTION_ENTER_REMOTE,
	EST_UI_ACTION_SWITCH_REMOTE_MOTOR_GROUP,
	EST_UI_ACTION_EXIT_REMOTE,
	EST_UI_ACTION_ENTER_MOTOR_OUTPUT,
	EST_UI_ACTION_UPDATE_MOTOR_OUTPUT,
	EST_UI_ACTION_EXIT_MOTOR_OUTPUT
} est_ui_action_t;

typedef enum {
	EST_UI_MOTOR_OUTPUT_STOP = 0,
	EST_UI_MOTOR_OUTPUT_FORWARD = 1,
	EST_UI_MOTOR_OUTPUT_REVERSE = 2
} est_ui_motor_output_state_t;

typedef struct {
	est_ui_page_t page;
	est_ui_page_t error_return_page;
	est_ui_language_t language;
	uint8_t home_item;
	uint8_t home_first_item;
	uint8_t settings_item;
	uint8_t program_item;
	uint8_t program_count;
	uint8_t delete_choice;
	uint8_t port_item;
	uint8_t remote_motor_group;
	uint8_t motor_output_power_index;
	est_ui_motor_output_state_t motor_output_states[
		EST_UI_MOTOR_OUTPUT_PORT_COUNT];
	uint8_t backlight_percent;
	uint8_t volume_percent;
	uint16_t error_code;
	bool has_recent_program;
	bool dirty;
} est_ui_state_t;

void est_ui_state_init(est_ui_state_t *state);
void est_ui_state_set_recent(est_ui_state_t *state, bool available);
void est_ui_state_set_program_count(est_ui_state_t *state, uint8_t count);
void est_ui_state_set_home(est_ui_state_t *state);
void est_ui_state_set_programs(est_ui_state_t *state);
void est_ui_state_set_running(est_ui_state_t *state);
void est_ui_state_set_error(est_ui_state_t *state, uint16_t error_code,
	est_ui_page_t return_page);
void est_ui_state_set_python_error(est_ui_state_t *state, uint16_t source_line,
	est_ui_page_t return_page);
est_ui_action_t est_ui_state_handle_short(est_ui_state_t *state,
	est_button_t button);
est_ui_action_t est_ui_state_handle_long(est_ui_state_t *state,
	est_button_t button);
bool est_ui_state_take_dirty(est_ui_state_t *state);
void est_ui_state_invalidate(est_ui_state_t *state);

#endif
