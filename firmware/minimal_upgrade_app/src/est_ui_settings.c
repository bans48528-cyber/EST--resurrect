#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "board_flash.h"
#include "est_ui_settings.h"

#define EST_UI_SETTINGS_RECORD_SIZE 32U
#define EST_UI_SETTINGS_FORMAT_VERSION 1U
#define EST_UI_SETTINGS_COMMIT_MARKER 0x544D4F43U

typedef char settings_banks_must_be_consecutive[
	EST_UI_SETTINGS_BANK1_ADDRESS ==
	(EST_UI_SETTINGS_BANK0_ADDRESS + EST_UI_SETTINGS_SECTOR_SIZE) ? 1 : -1];
typedef char settings_region_must_end_before_programs[
	EST_UI_SETTINGS_REGION_END ==
	(EST_UI_SETTINGS_BANK1_ADDRESS + EST_UI_SETTINGS_SECTOR_SIZE) ? 1 : -1];

typedef enum {
	SETTINGS_BANK_ERASED = 0,
	SETTINGS_BANK_VALID,
	SETTINGS_BANK_OWNED_INVALID,
	SETTINGS_BANK_FOREIGN,
	SETTINGS_BANK_IO_ERROR
} settings_bank_kind_t;

typedef struct {
	settings_bank_kind_t kind;
	uint32_t sequence;
	est_ui_settings_data_t settings;
} settings_bank_info_t;

static uint32_t read_u32_le(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] |
		((uint32_t)bytes[1] << 8U) |
		((uint32_t)bytes[2] << 16U) |
		((uint32_t)bytes[3] << 24U);
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
	bytes[0] = (uint8_t)value;
	bytes[1] = (uint8_t)(value >> 8U);
	bytes[2] = (uint8_t)(value >> 16U);
	bytes[3] = (uint8_t)(value >> 24U);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
	size_t index;
	uint8_t bit;

	for (index = 0U; index < length; index++) {
		crc ^= data[index];
		for (bit = 0U; bit < 8U; bit++) {
			crc = (crc >> 1U) ^
				((crc & 1U) != 0U ? 0xEDB88320U : 0U);
		}
	}
	return crc;
}

static uint32_t record_crc32(const uint8_t record[EST_UI_SETTINGS_RECORD_SIZE])
{
	uint32_t crc = crc32_update(0xFFFFFFFFU, record, 16U);

	crc = crc32_update(crc, &record[24],
		EST_UI_SETTINGS_RECORD_SIZE - 24U);
	return ~crc;
}

static bool supported_identity(struct board_flash_identity identity)
{
	return identity.manufacturer == 0xEFU && identity.memory_type == 0x40U &&
		identity.capacity == 0x19U;
}

static bool settings_valid(const est_ui_settings_data_t *settings)
{
	return settings != NULL && settings->backlight_percent >= 10U &&
		settings->backlight_percent <= 100U &&
		(settings->backlight_percent % 10U) == 0U &&
		settings->volume_percent <= 100U &&
		(settings->volume_percent % 10U) == 0U &&
		(settings->language == EST_UI_LANGUAGE_CHINESE ||
		 settings->language == EST_UI_LANGUAGE_ENGLISH ||
		 settings->language == EST_UI_LANGUAGE_PORTUGUESE) &&
		(settings->recent_program_slot < 8U ||
		 settings->recent_program_slot == EST_UI_SETTINGS_NO_RECENT_SLOT);
}

static bool bytes_are_erased(const uint8_t *bytes, size_t length)
{
	size_t index;

	for (index = 0U; index < length; index++) {
		if (bytes[index] != 0xFFU) {
			return false;
		}
	}
	return true;
}

static settings_bank_info_t scan_bank(uint32_t address)
{
	static const uint8_t magic[4] = {'E', 'S', 'T', 'C'};
	settings_bank_info_t info = {SETTINGS_BANK_IO_ERROR, 0U, {0U, 0U,
		EST_UI_LANGUAGE_ENGLISH, EST_UI_SETTINGS_NO_RECENT_SLOT}};
	uint8_t record[EST_UI_SETTINGS_RECORD_SIZE];
	uint32_t stored_crc;

	if (!board_flash_read_4byte(address, record, sizeof(record))) {
		return info;
	}
	if (bytes_are_erased(record, sizeof(record))) {
		info.kind = board_flash_sector_is_erased_4byte(address) ?
			SETTINGS_BANK_ERASED : SETTINGS_BANK_FOREIGN;
		return info;
	}
	if (memcmp(record, magic, sizeof(magic)) != 0) {
		info.kind = SETTINGS_BANK_FOREIGN;
		return info;
	}
	info.kind = SETTINGS_BANK_OWNED_INVALID;
	if (record[4] != EST_UI_SETTINGS_FORMAT_VERSION ||
	    record[5] != EST_UI_SETTINGS_RECORD_SIZE ||
	    read_u32_le(&record[20]) != EST_UI_SETTINGS_COMMIT_MARKER) {
		return info;
	}
	stored_crc = read_u32_le(&record[16]);
	if (stored_crc != record_crc32(record)) {
		return info;
	}
	info.sequence = read_u32_le(&record[12]);
	info.settings.backlight_percent = record[6];
	info.settings.volume_percent = record[7];
	info.settings.language = (est_ui_language_t)record[8];
	info.settings.recent_program_slot = record[9];
	if (info.sequence == 0U || !settings_valid(&info.settings)) {
		return info;
	}
	info.kind = SETTINGS_BANK_VALID;
	return info;
}

static bool sequence_newer(uint32_t left, uint32_t right)
{
	return (int32_t)(left - right) > 0;
}

static int8_t current_bank(const settings_bank_info_t banks[2])
{
	if (banks[0].kind != SETTINGS_BANK_VALID) {
		return banks[1].kind == SETTINGS_BANK_VALID ? 1 : -1;
	}
	if (banks[1].kind != SETTINGS_BANK_VALID) {
		return 0;
	}
	return sequence_newer(banks[1].sequence, banks[0].sequence) ? 1 : 0;
}

static bool settings_equal(const est_ui_settings_data_t *left,
	const est_ui_settings_data_t *right)
{
	return left->backlight_percent == right->backlight_percent &&
		left->volume_percent == right->volume_percent &&
		left->language == right->language &&
		left->recent_program_slot == right->recent_program_slot;
}

static void encode_record(uint8_t record[EST_UI_SETTINGS_RECORD_SIZE],
	const est_ui_settings_data_t *settings, uint32_t sequence)
{
	static const uint8_t magic[4] = {'E', 'S', 'T', 'C'};

	memset(record, 0xFF, EST_UI_SETTINGS_RECORD_SIZE);
	memcpy(record, magic, sizeof(magic));
	record[4] = EST_UI_SETTINGS_FORMAT_VERSION;
	record[5] = EST_UI_SETTINGS_RECORD_SIZE;
	record[6] = settings->backlight_percent;
	record[7] = settings->volume_percent;
	record[8] = (uint8_t)settings->language;
	record[9] = settings->recent_program_slot;
	record[10] = 0U;
	record[11] = 0U;
	write_u32_le(&record[12], sequence);
	write_u32_le(&record[16], record_crc32(record));
}

est_result_t est_ui_settings_load(est_ui_settings_data_t *settings)
{
	settings_bank_info_t banks[2];
	int8_t active;

	if (settings == NULL) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (!supported_identity(board_flash_read_identity())) {
		return EST_ERR_NOT_SUPPORTED;
	}
	banks[0] = scan_bank(EST_UI_SETTINGS_BANK0_ADDRESS);
	banks[1] = scan_bank(EST_UI_SETTINGS_BANK1_ADDRESS);
	if (banks[0].kind == SETTINGS_BANK_IO_ERROR ||
	    banks[1].kind == SETTINGS_BANK_IO_ERROR) {
		return EST_ERR_IO;
	}
	active = current_bank(banks);
	if (active < 0) {
		return EST_ERR_STATE;
	}
	*settings = banks[(uint8_t)active].settings;
	return EST_OK;
}

est_result_t est_ui_settings_save(const est_ui_settings_data_t *settings)
{
	settings_bank_info_t banks[2];
	uint8_t record[EST_UI_SETTINGS_RECORD_SIZE];
	uint8_t commit[4];
	uint32_t addresses[2] = {
		EST_UI_SETTINGS_BANK0_ADDRESS,
		EST_UI_SETTINGS_BANK1_ADDRESS
	};
	uint32_t sequence;
	uint8_t target;
	int8_t active;
	settings_bank_info_t verify;

	if (!settings_valid(settings)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (!supported_identity(board_flash_read_identity())) {
		return EST_ERR_NOT_SUPPORTED;
	}
	banks[0] = scan_bank(addresses[0]);
	banks[1] = scan_bank(addresses[1]);
	if (banks[0].kind == SETTINGS_BANK_IO_ERROR ||
	    banks[1].kind == SETTINGS_BANK_IO_ERROR) {
		return EST_ERR_IO;
	}
	if (banks[0].kind == SETTINGS_BANK_FOREIGN ||
	    banks[1].kind == SETTINGS_BANK_FOREIGN) {
		return EST_ERR_STATE;
	}
	active = current_bank(banks);
	if (active >= 0 && settings_equal(settings,
	    &banks[(uint8_t)active].settings)) {
		return EST_OK;
	}
	target = active < 0 ? 0U : (uint8_t)active ^ 1U;
	sequence = active < 0 ? 1U : banks[(uint8_t)active].sequence + 1U;
	if (sequence == 0U) {
		sequence = 1U;
	}
	if (banks[target].kind != SETTINGS_BANK_ERASED &&
	    !board_flash_erase_sector_4byte(addresses[target])) {
		return EST_ERR_IO;
	}
	if (!board_flash_sector_is_erased_4byte(addresses[target])) {
		return EST_ERR_IO;
	}
	encode_record(record, settings, sequence);
	if (!board_flash_program_4byte(addresses[target], record, sizeof(record))) {
		return EST_ERR_IO;
	}
	write_u32_le(commit, EST_UI_SETTINGS_COMMIT_MARKER);
	if (!board_flash_program_4byte(addresses[target] + 20U, commit,
	    sizeof(commit))) {
		return EST_ERR_IO;
	}
	verify = scan_bank(addresses[target]);
	if (verify.kind != SETTINGS_BANK_VALID || verify.sequence != sequence ||
	    !settings_equal(settings, &verify.settings)) {
		return EST_ERR_IO;
	}
	return EST_OK;
}
