#include <stddef.h>
#include <string.h>

#include "board_flash.h"
#include "est_micropython.h"
#include "est_program_store.h"

#define EST_PROGRAM_STORE_SECTOR_SIZE 4096U
#define EST_PROGRAM_STORE_ALL_SECTORS_MASK 0x3FU
#define EST_PROGRAM_STORE_BANK_SECTORS_MASK 0x07U
#define EST_PROGRAM_STORE_LEGACY_HEADER_SIZE 32U
#define EST_PROGRAM_STORE_HEADER_SIZE 64U
#define EST_PROGRAM_STORE_DATA_OFFSET EST_PROGRAM_STORE_HEADER_SIZE
#define EST_PROGRAM_STORE_LEGACY_FORMAT_VERSION 1U
#define EST_PROGRAM_STORE_FORMAT_VERSION 2U
#define EST_PROGRAM_STORE_COMMIT_MARKER 0x544D4F43U
#define EST_PROGRAM_STORE_IO_CHUNK_SIZE 256U

_Static_assert(EST_PROGRAM_STORE_BANK_SIZE ==
	(EST_PROGRAM_STORE_SECTOR_SIZE * EST_PROGRAM_STORE_SECTORS_PER_BANK),
	"Persistent program bank must contain exactly three sectors");
_Static_assert(EST_PROGRAM_STORE_PROGRAM_SLOT_SIZE ==
	(EST_PROGRAM_STORE_BANK_SIZE * EST_PROGRAM_STORE_BANK_COUNT),
	"Each program slot must contain two atomic banks");
_Static_assert(EST_PROGRAM_STORE_REGION_SIZE ==
	(EST_PROGRAM_STORE_PROGRAM_SLOT_SIZE *
	 EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT),
	"Persistent program region must contain all logical program slots");
_Static_assert((EST_PROGRAM_STORE_REGION_START + EST_PROGRAM_STORE_REGION_SIZE) ==
	EST_PROGRAM_STORE_FLASH_SIZE,
	"Persistent program region must end at the top of external flash");
_Static_assert((EST_PROGRAM_STORE_DATA_OFFSET +
	EST_MICROPYTHON_PROGRAM_MAX_SIZE) <= EST_PROGRAM_STORE_BANK_SIZE,
	"Each atomic bank must contain the maximum Python source");

typedef enum {
	BANK_ERASED = 0,
	BANK_VALID_PROGRAM = 1,
	BANK_VALID_TOMBSTONE = 2,
	BANK_OWNED_INCOMPLETE = 3,
	BANK_FOREIGN = 4
} bank_kind_t;

struct bank_info {
	bank_kind_t kind;
	uint32_t generation;
	uint16_t source_length;
	uint16_t data_offset;
	uint32_t source_crc32;
	uint8_t name_length;
	char name[EST_PROGRAM_STORE_NAME_MAX_BYTES + 1U];
};

static est_program_store_error_t
	last_errors[EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT];
static uint32_t change_sequence;
static uint8_t last_changed_slot;
static est_program_store_record_type_t last_changed_record_type;

static uint16_t read_u16_le(const uint8_t *bytes)
{
	return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U);
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] |
		((uint32_t)bytes[1] << 8U) |
		((uint32_t)bytes[2] << 16U) |
		((uint32_t)bytes[3] << 24U);
}

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
	bytes[0] = (uint8_t)value;
	bytes[1] = (uint8_t)(value >> 8U);
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

static uint32_t crc32_bytes(const uint8_t *data, size_t length)
{
	return ~crc32_update(0xFFFFFFFFU, data, length);
}

static uint32_t current_header_crc32(const uint8_t *header)
{
	uint32_t crc = crc32_update(0xFFFFFFFFU, header, 20U);

	crc = crc32_update(crc, &header[28],
		EST_PROGRAM_STORE_HEADER_SIZE - 28U);
	return ~crc;
}

static bool supported_identity(struct board_flash_identity identity)
{
	return identity.manufacturer == 0xEFU && identity.memory_type == 0x40U &&
		identity.capacity == 0x19U;
}

static bool valid_program_slot(uint8_t program_slot_id)
{
	return program_slot_id < EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT;
}

static uint32_t program_slot_address(uint8_t program_slot_id)
{
	return EST_PROGRAM_STORE_FLASH_SIZE -
		(((uint32_t)program_slot_id + 1U) *
		 EST_PROGRAM_STORE_PROGRAM_SLOT_SIZE);
}

static uint32_t bank_address(uint8_t program_slot_id, uint8_t bank)
{
	return program_slot_address(program_slot_id) +
		((uint32_t)bank * EST_PROGRAM_STORE_BANK_SIZE);
}

static bool generation_is_newer(uint32_t candidate, uint32_t reference)
{
	return (int32_t)(candidate - reference) > 0;
}

static bool source_crc_matches(uint32_t address, uint16_t length,
	uint32_t expected_crc32)
{
	uint8_t buffer[EST_PROGRAM_STORE_IO_CHUNK_SIZE];
	uint16_t offset = 0U;
	uint32_t crc = 0xFFFFFFFFU;

	while (offset < length) {
		uint16_t remaining = (uint16_t)(length - offset);
		uint16_t chunk_length = remaining < sizeof(buffer) ?
			remaining : sizeof(buffer);

		if (!board_flash_read_4byte(address + offset, buffer, chunk_length)) {
			return false;
		}
		crc = crc32_update(crc, buffer, chunk_length);
		offset = (uint16_t)(offset + chunk_length);
	}
	return ~crc == expected_crc32;
}

static bool name_bytes_valid(const uint8_t *name, uint8_t name_length)
{
	uint8_t index;

	if (name == NULL || name_length == 0U ||
	    name_length > EST_PROGRAM_STORE_NAME_MAX_BYTES) {
		return false;
	}
	for (index = 0U; index < name_length; index++) {
		if (name[index] == 0U) {
			return false;
		}
	}
	return true;
}

static void inspect_legacy_bank(uint8_t program_slot_id, const uint8_t *header,
	struct bank_info *info)
{
	static const char legacy_name[] = "Program 0";
	uint8_t record_type;

	if (program_slot_id != 0U || header[6] != 0U ||
	    header[7] != EST_PROGRAM_STORE_LEGACY_HEADER_SIZE ||
	    read_u32_le(&header[20]) != crc32_bytes(header, 20U) ||
	    read_u32_le(&header[24]) != EST_PROGRAM_STORE_COMMIT_MARKER) {
		return;
	}
	record_type = header[5];
	info->generation = read_u32_le(&header[8]);
	info->source_length = read_u16_le(&header[12]);
	info->source_crc32 = read_u32_le(&header[16]);
	info->data_offset = EST_PROGRAM_STORE_LEGACY_HEADER_SIZE;
	if (record_type == EST_PROGRAM_STORE_RECORD_TOMBSTONE &&
	    info->source_length == 0U && info->source_crc32 == 0U) {
		info->kind = BANK_VALID_TOMBSTONE;
		return;
	}
	if (record_type != EST_PROGRAM_STORE_RECORD_PROGRAM ||
	    info->source_length == 0U ||
	    info->source_length > EST_MICROPYTHON_PROGRAM_MAX_SIZE) {
		return;
	}
	info->name_length = (uint8_t)(sizeof(legacy_name) - 1U);
	memcpy(info->name, legacy_name, info->name_length + 1U);
}

static void inspect_current_bank(uint8_t program_slot_id,
	const uint8_t *header, struct bank_info *info)
{
	uint8_t record_type;

	if (header[6] != program_slot_id ||
	    header[7] != EST_PROGRAM_STORE_HEADER_SIZE || header[15] != 0U ||
	    header[14] > EST_PROGRAM_STORE_NAME_MAX_BYTES ||
	    read_u32_le(&header[20]) != current_header_crc32(header) ||
	    read_u32_le(&header[24]) != EST_PROGRAM_STORE_COMMIT_MARKER) {
		return;
	}
	record_type = header[5];
	info->generation = read_u32_le(&header[8]);
	info->source_length = read_u16_le(&header[12]);
	info->name_length = header[14];
	info->source_crc32 = read_u32_le(&header[16]);
	info->data_offset = EST_PROGRAM_STORE_DATA_OFFSET;
	if (info->name_length > 0U) {
		memcpy(info->name, &header[28], info->name_length);
		info->name[info->name_length] = '\0';
	}
	if (record_type == EST_PROGRAM_STORE_RECORD_TOMBSTONE &&
	    info->source_length == 0U && info->source_crc32 == 0U &&
	    info->name_length == 0U) {
		info->kind = BANK_VALID_TOMBSTONE;
		return;
	}
	if (record_type != EST_PROGRAM_STORE_RECORD_PROGRAM ||
	    info->source_length == 0U ||
	    info->source_length > EST_MICROPYTHON_PROGRAM_MAX_SIZE ||
	    !name_bytes_valid((const uint8_t *)info->name, info->name_length)) {
		return;
	}
}

static void inspect_bank(uint8_t program_slot_id, uint8_t bank, bool erased,
	struct bank_info *info)
{
	static const uint8_t magic[4] = {'E', 'S', 'T', 'P'};
	uint8_t header[EST_PROGRAM_STORE_HEADER_SIZE];
	uint32_t address = bank_address(program_slot_id, bank);

	memset(info, 0, sizeof(*info));
	if (erased) {
		info->kind = BANK_ERASED;
		return;
	}
	if (!board_flash_read_4byte(address, header, sizeof(header)) ||
	    memcmp(header, magic, sizeof(magic)) != 0) {
		info->kind = BANK_FOREIGN;
		return;
	}
	info->kind = BANK_OWNED_INCOMPLETE;
	if (header[4] == EST_PROGRAM_STORE_LEGACY_FORMAT_VERSION) {
		inspect_legacy_bank(program_slot_id, header, info);
	} else if (header[4] == EST_PROGRAM_STORE_FORMAT_VERSION) {
		inspect_current_bank(program_slot_id, header, info);
	} else {
		return;
	}
	if (info->kind == BANK_OWNED_INCOMPLETE &&
	    info->source_length > 0U &&
	    source_crc_matches(address + info->data_offset, info->source_length,
		info->source_crc32)) {
		info->kind = BANK_VALID_PROGRAM;
	}
}

static int8_t newest_valid_bank(const struct bank_info banks[2])
{
	bool bank0_valid = banks[0].kind == BANK_VALID_PROGRAM ||
		banks[0].kind == BANK_VALID_TOMBSTONE;
	bool bank1_valid = banks[1].kind == BANK_VALID_PROGRAM ||
		banks[1].kind == BANK_VALID_TOMBSTONE;

	if (!bank0_valid && !bank1_valid) {
		return -1;
	}
	if (!bank0_valid) {
		return 1;
	}
	if (!bank1_valid) {
		return 0;
	}
	return generation_is_newer(banks[1].generation,
		banks[0].generation) ? 1 : 0;
}

static bool scan_program_slot(uint8_t program_slot_id,
	est_program_store_status_t *status, struct bank_info banks[2])
{
	uint8_t index;
	int8_t active;
	bool owned_incomplete = false;
	uint32_t base_address;

	if (!valid_program_slot(program_slot_id)) {
		return false;
	}
	memset(status, 0, sizeof(*status));
	memset(banks, 0, sizeof(struct bank_info) * 2U);
	status->program_slot_id = program_slot_id;
	status->active_bank = 0xFFU;
	status->last_error = last_errors[program_slot_id];
	status->identity = board_flash_read_identity();
	if (!supported_identity(status->identity)) {
		status->state = EST_PROGRAM_STORE_UNSUPPORTED;
		status->last_error = EST_PROGRAM_STORE_ERROR_UNSUPPORTED;
		return true;
	}

	status->flags = EST_PROGRAM_STORE_FLAG_SUPPORTED_DEVICE |
		EST_PROGRAM_STORE_FLAG_WRITE_SUPPORTED;
	base_address = program_slot_address(program_slot_id);
	for (index = 0U; index < EST_PROGRAM_STORE_SECTORS_PER_PROGRAM; index++) {
		uint32_t address = base_address +
			((uint32_t)index * EST_PROGRAM_STORE_SECTOR_SIZE);

		if (board_flash_sector_is_erased_4byte(address)) {
			status->erased_sector_mask |= (uint8_t)(1U << index);
		}
	}
	status->flags |= EST_PROGRAM_STORE_FLAG_SCAN_COMPLETE;
	status->occupied_sector_mask = (uint8_t)(
		(~status->erased_sector_mask) & EST_PROGRAM_STORE_ALL_SECTORS_MASK);
	if (status->erased_sector_mask == EST_PROGRAM_STORE_ALL_SECTORS_MASK) {
		status->flags |= EST_PROGRAM_STORE_FLAG_ALL_ERASED;
	}
	for (index = 0U; index < EST_PROGRAM_STORE_BANK_COUNT; index++) {
		uint8_t erased_mask = (uint8_t)(EST_PROGRAM_STORE_BANK_SECTORS_MASK <<
			(index * EST_PROGRAM_STORE_SECTORS_PER_BANK));
		bool erased = (status->erased_sector_mask & erased_mask) == erased_mask;

		inspect_bank(program_slot_id, index, erased, &banks[index]);
		if (banks[index].kind == BANK_FOREIGN) {
			status->state = EST_PROGRAM_STORE_OCCUPIED;
			status->last_error = EST_PROGRAM_STORE_ERROR_FOREIGN_DATA;
			return true;
		}
		owned_incomplete |= banks[index].kind == BANK_OWNED_INCOMPLETE;
	}
	active = newest_valid_bank(banks);
	if (active < 0) {
		status->state = EST_PROGRAM_STORE_READY;
		if (owned_incomplete) {
			status->flags |= EST_PROGRAM_STORE_FLAG_OWNED;
		}
		return true;
	}
	status->active_bank = (uint8_t)active;
	status->flags |= EST_PROGRAM_STORE_FLAG_OWNED;
	status->generation = banks[active].generation;
	status->source_length = banks[active].source_length;
	status->source_crc32 = banks[active].source_crc32;
	status->name_length = banks[active].name_length;
	memcpy(status->name, banks[active].name, sizeof(status->name));
	if (banks[active].kind == BANK_VALID_PROGRAM) {
		status->state = EST_PROGRAM_STORE_SAVED;
		status->record_type = EST_PROGRAM_STORE_RECORD_PROGRAM;
		status->flags |= EST_PROGRAM_STORE_FLAG_SAVED_PRESENT;
	} else {
		status->state = EST_PROGRAM_STORE_READY;
		status->record_type = EST_PROGRAM_STORE_RECORD_TOMBSTONE;
	}
	return true;
}

bool est_program_store_get_slot_status(uint8_t program_slot_id,
	est_program_store_status_t *status)
{
	struct bank_info banks[2];

	if (status == NULL) {
		return false;
	}
	return scan_program_slot(program_slot_id, status, banks);
}

bool est_program_store_get_status(est_program_store_status_t *status)
{
	return est_program_store_get_slot_status(0U, status);
}

static est_result_t fail(uint8_t program_slot_id,
	est_program_store_error_t error, est_result_t result)
{
	if (valid_program_slot(program_slot_id)) {
		last_errors[program_slot_id] = error;
	}
	return result;
}

static bool erase_bank(uint8_t program_slot_id, uint8_t bank)
{
	uint8_t sector;
	uint32_t address = bank_address(program_slot_id, bank);

	for (sector = 0U; sector < EST_PROGRAM_STORE_SECTORS_PER_BANK; sector++) {
		uint32_t sector_address = address +
			((uint32_t)sector * EST_PROGRAM_STORE_SECTOR_SIZE);

		if (!board_flash_erase_sector_4byte(sector_address) ||
		    !board_flash_sector_is_erased_4byte(sector_address)) {
			return false;
		}
	}
	return true;
}

static bool write_record(uint8_t program_slot_id, uint8_t bank,
	est_program_store_record_type_t type, uint32_t generation,
	uint16_t source_length, uint32_t source_crc32, const uint8_t *name,
	uint8_t name_length)
{
	static const uint8_t magic[4] = {'E', 'S', 'T', 'P'};
	uint8_t header[EST_PROGRAM_STORE_HEADER_SIZE];
	uint8_t buffer[EST_PROGRAM_STORE_IO_CHUNK_SIZE];
	uint8_t commit[4];
	uint16_t offset = 0U;
	uint32_t address = bank_address(program_slot_id, bank);

	memset(header, 0xFF, sizeof(header));
	memcpy(header, magic, sizeof(magic));
	header[4] = EST_PROGRAM_STORE_FORMAT_VERSION;
	header[5] = (uint8_t)type;
	header[6] = program_slot_id;
	header[7] = EST_PROGRAM_STORE_HEADER_SIZE;
	write_u32_le(&header[8], generation);
	write_u16_le(&header[12], source_length);
	header[14] = name_length;
	header[15] = 0U;
	write_u32_le(&header[16], source_crc32);
	if (name_length > 0U) {
		memcpy(&header[28], name, name_length);
	}
	write_u32_le(&header[20], current_header_crc32(header));
	if (!board_flash_program_4byte(address, header, sizeof(header))) {
		return false;
	}
	while (offset < source_length) {
		uint16_t remaining = (uint16_t)(source_length - offset);
		uint16_t chunk_length = remaining < sizeof(buffer) ?
			remaining : sizeof(buffer);

		if (est_micropython_program_read(offset, buffer, chunk_length) != EST_OK ||
		    !board_flash_program_4byte(address + EST_PROGRAM_STORE_DATA_OFFSET +
			offset, buffer, chunk_length)) {
			return false;
		}
		offset = (uint16_t)(offset + chunk_length);
	}
	if (source_length > 0U &&
	    !source_crc_matches(address + EST_PROGRAM_STORE_DATA_OFFSET,
		source_length, source_crc32)) {
		return false;
	}
	write_u32_le(commit, EST_PROGRAM_STORE_COMMIT_MARKER);
	return board_flash_program_4byte(address + 24U, commit, sizeof(commit));
}

static uint8_t target_bank(const est_program_store_status_t *status,
	const struct bank_info banks[2])
{
	if (status->active_bank < EST_PROGRAM_STORE_BANK_COUNT) {
		return (uint8_t)(status->active_bank ^ 1U);
	}
	if (banks[0].kind == BANK_ERASED) {
		return 0U;
	}
	if (banks[1].kind == BANK_ERASED) {
		return 1U;
	}
	return 0U;
}

static est_result_t commit_record(uint8_t program_slot_id,
	est_program_store_record_type_t type, uint16_t source_length,
	uint32_t source_crc32, const uint8_t *name, uint8_t name_length)
{
	est_program_store_status_t status;
	struct bank_info banks[2];
	uint8_t target;
	uint32_t generation;

	if (!scan_program_slot(program_slot_id, &status, banks)) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_FLASH_IO,
			EST_ERR_IO);
	}
	if (status.state == EST_PROGRAM_STORE_UNSUPPORTED) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_UNSUPPORTED,
			EST_ERR_NOT_SUPPORTED);
	}
	if (status.state == EST_PROGRAM_STORE_OCCUPIED) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_FOREIGN_DATA,
			EST_ERR_STATE);
	}
	target = target_bank(&status, banks);
	generation = status.generation + 1U;
	if (generation == 0U) {
		generation = 1U;
	}
	if (banks[target].kind != BANK_ERASED &&
	    !erase_bank(program_slot_id, target)) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_FLASH_IO,
			EST_ERR_IO);
	}
	if (!write_record(program_slot_id, target, type, generation, source_length,
		source_crc32, name, name_length)) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_VERIFY, EST_ERR_IO);
	}
	if (!scan_program_slot(program_slot_id, &status, banks) ||
	    status.active_bank != target || status.generation != generation ||
	    status.record_type != type) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_VERIFY, EST_ERR_IO);
	}
	last_errors[program_slot_id] = EST_PROGRAM_STORE_ERROR_NONE;
	change_sequence++;
	last_changed_slot = program_slot_id;
	last_changed_record_type = type;
	return EST_OK;
}

bool est_program_store_last_change(uint32_t *sequence, uint8_t *program_slot_id,
	est_program_store_record_type_t *record_type)
{
	if (sequence == NULL || program_slot_id == NULL || record_type == NULL) {
		return false;
	}
	*sequence = change_sequence;
	*program_slot_id = last_changed_slot;
	*record_type = last_changed_record_type;
	return true;
}

est_result_t est_program_store_save(uint8_t program_slot_id,
	const uint8_t *name, uint8_t name_length)
{
	est_micropython_program_status_t program = {0};

	if (!valid_program_slot(program_slot_id) ||
	    !name_bytes_valid(name, name_length)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (est_micropython_program_is_executing()) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_BUSY, EST_ERR_BUSY);
	}
	if (!est_micropython_program_get_status(&program) ||
	    (program.flags & EST_MICROPYTHON_PROGRAM_FLAG_VALID) == 0U ||
	    program.expected_length == 0U ||
	    program.received_length != program.expected_length) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_NO_RAM_PROGRAM,
			EST_ERR_STATE);
	}
	return commit_record(program_slot_id, EST_PROGRAM_STORE_RECORD_PROGRAM,
		program.expected_length, program.actual_crc32, name, name_length);
}

est_result_t est_program_store_load(uint8_t program_slot_id)
{
	est_program_store_status_t status;
	struct bank_info banks[2];
	uint8_t buffer[EST_PROGRAM_STORE_IO_CHUNK_SIZE];
	uint16_t offset = 0U;
	uint32_t address;
	est_result_t result;

	if (!valid_program_slot(program_slot_id)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (est_micropython_program_is_executing()) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_BUSY, EST_ERR_BUSY);
	}
	if (!scan_program_slot(program_slot_id, &status, banks)) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_FLASH_IO,
			EST_ERR_IO);
	}
	if (status.state != EST_PROGRAM_STORE_SAVED ||
	    status.active_bank >= EST_PROGRAM_STORE_BANK_COUNT) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_NO_SAVED_PROGRAM,
			EST_ERR_STATE);
	}
	result = est_micropython_program_begin_saved(status.source_length,
		status.source_crc32);
	if (result != EST_OK) {
		return fail(program_slot_id,
			result == EST_ERR_BUSY ? EST_PROGRAM_STORE_ERROR_BUSY :
			EST_PROGRAM_STORE_ERROR_NO_RAM_PROGRAM, result);
	}
	address = bank_address(program_slot_id, status.active_bank) +
		banks[status.active_bank].data_offset;
	while (offset < status.source_length) {
		uint16_t remaining = (uint16_t)(status.source_length - offset);
		uint16_t chunk_length = remaining < sizeof(buffer) ?
			remaining : sizeof(buffer);

		if (!board_flash_read_4byte(address + offset, buffer, chunk_length) ||
		    est_micropython_program_write(offset, buffer, chunk_length) != EST_OK) {
			(void)est_micropython_program_clear();
			return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_FLASH_IO,
				EST_ERR_IO);
		}
		offset = (uint16_t)(offset + chunk_length);
	}
	last_errors[program_slot_id] = EST_PROGRAM_STORE_ERROR_NONE;
	return EST_OK;
}

est_result_t est_program_store_clear(uint8_t program_slot_id)
{
	est_program_store_status_t status;
	struct bank_info banks[2];

	if (!valid_program_slot(program_slot_id)) {
		return EST_ERR_INVALID_ARGUMENT;
	}
	if (est_micropython_program_is_executing()) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_BUSY, EST_ERR_BUSY);
	}
	if (!scan_program_slot(program_slot_id, &status, banks)) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_FLASH_IO,
			EST_ERR_IO);
	}
	if (status.state == EST_PROGRAM_STORE_UNSUPPORTED) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_UNSUPPORTED,
			EST_ERR_NOT_SUPPORTED);
	}
	if (status.state == EST_PROGRAM_STORE_OCCUPIED) {
		return fail(program_slot_id, EST_PROGRAM_STORE_ERROR_FOREIGN_DATA,
			EST_ERR_STATE);
	}
	if (status.state != EST_PROGRAM_STORE_SAVED) {
		last_errors[program_slot_id] = EST_PROGRAM_STORE_ERROR_NONE;
		return EST_OK;
	}
	return commit_record(program_slot_id, EST_PROGRAM_STORE_RECORD_TOMBSTONE,
		0U, 0U, NULL, 0U);
}
