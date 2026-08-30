#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "est_ui_programs.h"

#define PROGRAM_SCAN_INTERVAL_MS 20U
#define PROGRAM_SCAN_ERROR_BASE 1000U

static est_ui_program_entry_t entries[EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT];
static est_ui_programs_state_t scan_state;
static uint8_t entry_count;
static uint8_t scan_slot;
static uint8_t recent_slot;
static uint32_t next_scan_ms;
static uint32_t observed_change_sequence;
static uint16_t scan_error_code;

static bool time_due(uint32_t now_ms, uint32_t deadline_ms)
{
	return (int32_t)(now_ms - deadline_ms) >= 0;
}

static void clear_entries(void)
{
	memset(entries, 0, sizeof(entries));
	entry_count = 0U;
}

void est_ui_programs_init(uint32_t now_ms)
{
	clear_entries();
	scan_state = EST_UI_PROGRAMS_SCANNING;
	scan_slot = 0U;
	recent_slot = EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT;
	next_scan_ms = now_ms;
	observed_change_sequence = 0U;
	scan_error_code = 0U;
}

void est_ui_programs_request_scan(uint32_t now_ms)
{
	clear_entries();
	scan_state = EST_UI_PROGRAMS_SCANNING;
	scan_slot = 0U;
	next_scan_ms = now_ms;
	scan_error_code = 0U;
}

static void fail_scan(est_program_store_error_t error)
{
	clear_entries();
	scan_state = EST_UI_PROGRAMS_ERROR;
	scan_error_code = (uint16_t)(PROGRAM_SCAN_ERROR_BASE + (uint16_t)error);
}

static void append_entry(uint8_t slot_id,
	const est_program_store_status_t *status)
{
	est_ui_program_entry_t *entry;

	if (entry_count >= EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT) {
		return;
	}
	entry = &entries[entry_count++];
	entry->slot_id = slot_id;
	memcpy(entry->name, status->name, sizeof(entry->name));
	entry->name[EST_PROGRAM_STORE_NAME_MAX_BYTES] = '\0';
}

static bool scan_one_slot(uint32_t now_ms)
{
	est_program_store_status_t status = {0};

	if (!est_program_store_get_slot_status(scan_slot, &status)) {
		fail_scan(EST_PROGRAM_STORE_ERROR_FLASH_IO);
		return true;
	}
	if (status.state == EST_PROGRAM_STORE_UNSUPPORTED) {
		fail_scan(EST_PROGRAM_STORE_ERROR_UNSUPPORTED);
		return true;
	}
	if (status.state == EST_PROGRAM_STORE_OCCUPIED) {
		fail_scan(status.last_error == EST_PROGRAM_STORE_ERROR_NONE ?
			EST_PROGRAM_STORE_ERROR_FOREIGN_DATA : status.last_error);
		return true;
	}
	if (status.state == EST_PROGRAM_STORE_SAVED &&
	    status.record_type == EST_PROGRAM_STORE_RECORD_PROGRAM) {
		append_entry(scan_slot, &status);
	}
	scan_slot++;
	next_scan_ms = now_ms + PROGRAM_SCAN_INTERVAL_MS;
	if (scan_slot >= EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT) {
		scan_state = EST_UI_PROGRAMS_READY;
		return true;
	}
	return false;
}

static bool check_store_change(uint32_t now_ms)
{
	uint32_t sequence;
	uint8_t slot_id;
	est_program_store_record_type_t record_type;

	if (!est_program_store_last_change(&sequence, &slot_id, &record_type) ||
	    sequence == observed_change_sequence) {
		return false;
	}
	observed_change_sequence = sequence;
	if (record_type == EST_PROGRAM_STORE_RECORD_PROGRAM) {
		recent_slot = slot_id;
	} else if (record_type == EST_PROGRAM_STORE_RECORD_TOMBSTONE &&
		   recent_slot == slot_id) {
		recent_slot = EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT;
	}
	est_ui_programs_request_scan(now_ms);
	return true;
}

bool est_ui_programs_tick(uint32_t now_ms)
{
	if (check_store_change(now_ms)) {
		return true;
	}
	if (scan_state != EST_UI_PROGRAMS_SCANNING ||
	    !time_due(now_ms, next_scan_ms)) {
		return false;
	}
	return scan_one_slot(now_ms);
}

est_ui_programs_state_t est_ui_programs_state(void)
{
	return scan_state;
}

uint8_t est_ui_programs_count(void)
{
	return entry_count;
}

const est_ui_program_entry_t *est_ui_programs_entry(uint8_t index)
{
	return index < entry_count ? &entries[index] : NULL;
}

const est_ui_program_entry_t *est_ui_programs_recent(void)
{
	uint8_t index;

	for (index = 0U; index < entry_count; index++) {
		if (entries[index].slot_id == recent_slot) {
			return &entries[index];
		}
	}
	return NULL;
}

void est_ui_programs_set_recent_slot(uint8_t slot_id)
{
	if (slot_id < EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT) {
		recent_slot = slot_id;
	}
}

void est_ui_programs_clear_recent(void)
{
	recent_slot = EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT;
}

uint16_t est_ui_programs_error_code(void)
{
	return scan_error_code;
}
