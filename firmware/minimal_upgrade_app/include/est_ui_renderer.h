#ifndef EST_UI_RENDERER_H
#define EST_UI_RENDERER_H

#include <stdbool.h>
#include <stdint.h>

#include "est_ui_state.h"
#include "est_ui_ports.h"

#define EST_UI_VIEW_PROGRAM_COUNT 8U

typedef enum {
	EST_UI_VIEW_PROGRAMS_SCANNING = 0,
	EST_UI_VIEW_PROGRAMS_READY = 1,
	EST_UI_VIEW_PROGRAMS_ERROR = 2
} est_ui_view_programs_state_t;

typedef struct {
	const char *app_version;
	const char *bootloader_version;
	const char *recent_program_name;
	const char *program_names[EST_UI_VIEW_PROGRAM_COUNT];
	uint8_t program_slots[EST_UI_VIEW_PROGRAM_COUNT];
	uint8_t program_count;
	est_ui_view_programs_state_t programs_state;
	uint16_t programs_error_code;
	est_ui_ports_view_t ports;
	uint8_t remote_motor_group;
	uint8_t remote_code;
	uint8_t remote_fault;
	bool remote_output_enabled;
	bool transfer_active;
	bool transfer_complete;
	uint8_t transfer_progress;
	uint8_t transfer_complete_frame;
	uint8_t battery_percent;
	bool battery_valid;
	bool battery_low;
	bool usb_connected;
} est_ui_view_t;

void est_ui_renderer_render(const est_ui_state_t *state,
	const est_ui_view_t *view);

#endif
