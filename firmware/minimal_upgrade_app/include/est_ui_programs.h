#ifndef EST_UI_PROGRAMS_H
#define EST_UI_PROGRAMS_H

#include <stdbool.h>
#include <stdint.h>

#include "est_program_store.h"

typedef enum {
	EST_UI_PROGRAMS_SCANNING = 0,
	EST_UI_PROGRAMS_READY = 1,
	EST_UI_PROGRAMS_ERROR = 2
} est_ui_programs_state_t;

typedef struct {
	uint8_t slot_id;
	char name[EST_PROGRAM_STORE_NAME_MAX_BYTES + 1U];
} est_ui_program_entry_t;

void est_ui_programs_init(uint32_t now_ms);
void est_ui_programs_request_scan(uint32_t now_ms);
bool est_ui_programs_tick(uint32_t now_ms);
est_ui_programs_state_t est_ui_programs_state(void);
uint8_t est_ui_programs_count(void);
const est_ui_program_entry_t *est_ui_programs_entry(uint8_t index);
const est_ui_program_entry_t *est_ui_programs_recent(void);
void est_ui_programs_set_recent_slot(uint8_t slot_id);
void est_ui_programs_clear_recent(void);
uint16_t est_ui_programs_error_code(void);

#endif
