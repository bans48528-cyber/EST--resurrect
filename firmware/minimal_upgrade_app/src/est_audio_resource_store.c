#include <string.h>

#include "board_flash.h"
#include "est_audio_resource_store.h"

#define AUDIO_RESOURCE_SECTOR_SIZE 4096U
#define AUDIO_RESOURCE_HEADER_MAGIC "EAUD"
#define AUDIO_RESOURCE_COMMIT_MAGIC "DONE"
#define AUDIO_RESOURCE_FORMAT_VERSION 1U
#define AUDIO_RESOURCE_NAME_OFFSET 32U
#define AUDIO_RESOURCE_COMMIT_OFFSET 124U
#define AUDIO_RESOURCE_UPLOAD_TIMEOUT_MS 15000U
#define AUDIO_RESOURCE_VERIFY_CHUNK_SIZE 64U

_Static_assert(EST_AUDIO_RESOURCE_REGION_SIZE ==
	(EST_AUDIO_RESOURCE_SLOT_COUNT * EST_AUDIO_RESOURCE_SLOT_SIZE),
	"Audio resource region must contain whole slots");
_Static_assert((EST_AUDIO_RESOURCE_REGION_START & (AUDIO_RESOURCE_SECTOR_SIZE - 1U)) == 0U,
	"Audio resource region must be sector aligned");
_Static_assert((EST_AUDIO_RESOURCE_SLOT_SIZE & (AUDIO_RESOURCE_SECTOR_SIZE - 1U)) == 0U,
	"Audio resource slots must be sector aligned");
_Static_assert((EST_AUDIO_RESOURCE_REGION_START + EST_AUDIO_RESOURCE_REGION_SIZE) <=
	0x01FD0000U,
	"Audio resource region must stop before persistent program slots");
_Static_assert(EST_AUDIO_RESOURCE_DATA_MAX_BYTES <= 0xFFFFU,
	"Audio resource chunk status uses 16-bit lengths");

struct audio_resource_header {
	uint8_t slot_id;
	uint8_t name_length;
	char name[EST_AUDIO_RESOURCE_NAME_MAX_BYTES + 1U];
	uint32_t resource_length;
	uint32_t resource_crc32;
	uint32_t duration_ms;
};

struct audio_resource_upload_session {
	bool active;
	uint8_t slot_id;
	uint8_t name_length;
	char name[EST_AUDIO_RESOURCE_NAME_MAX_BYTES + 1U];
	uint32_t resource_length;
	uint32_t resource_crc32;
	uint32_t duration_ms;
	uint32_t received_length;
	uint32_t last_activity_ms;
};

static struct audio_resource_upload_session upload_session;
static est_audio_resource_error_t last_error;
static struct audio_resource_header resource_catalog[EST_AUDIO_RESOURCE_SLOT_COUNT];
static bool resource_catalog_valid[EST_AUDIO_RESOURCE_SLOT_COUNT];
static bool resource_catalog_ready;

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

static bool supported_identity(struct board_flash_identity identity)
{
	return identity.manufacturer == 0xEFU && identity.memory_type == 0x40U &&
		identity.capacity == 0x19U;
}

static bool slot_is_valid(uint8_t slot_id)
{
	return slot_id < EST_AUDIO_RESOURCE_SLOT_COUNT;
}

static uint32_t slot_address(uint8_t slot_id)
{
	return EST_AUDIO_RESOURCE_REGION_START +
		((uint32_t)slot_id * EST_AUDIO_RESOURCE_SLOT_SIZE);
}

static uint32_t slot_data_address(uint8_t slot_id)
{
	return slot_address(slot_id) + EST_AUDIO_RESOURCE_HEADER_SIZE;
}

static bool name_is_valid(const uint8_t *name, uint8_t name_length)
{
	uint8_t index;

	if (name == NULL || name_length == 0U ||
	    name_length > EST_AUDIO_RESOURCE_NAME_MAX_BYTES ||
	    name[0] == '/' || name[name_length - 1U] == '/') {
		return false;
	}
	for (index = 0U; index < name_length; index++) {
		uint8_t ch = name[index];

		if (ch < 0x20U || ch >= 0x7FU || ch == '\\' || ch == ':') {
			return false;
		}
		if (ch == '.' && index + 1U < name_length && name[index + 1U] == '.') {
			return false;
		}
		if (ch == '/' && index + 1U < name_length && name[index + 1U] == '/') {
			return false;
		}
	}
	return true;
}

static bool read_header(uint8_t slot_id, struct audio_resource_header *header)
{
	uint8_t raw[EST_AUDIO_RESOURCE_HEADER_SIZE];
	uint8_t name_length;

	if (!slot_is_valid(slot_id) || header == NULL ||
	    !board_flash_read_4byte(slot_address(slot_id), raw, sizeof(raw))) {
		return false;
	}
	if (memcmp(raw, AUDIO_RESOURCE_HEADER_MAGIC, 4U) != 0 ||
	    memcmp(&raw[AUDIO_RESOURCE_COMMIT_OFFSET],
		AUDIO_RESOURCE_COMMIT_MAGIC, 4U) != 0 ||
	    raw[4] != AUDIO_RESOURCE_FORMAT_VERSION ||
	    raw[5] != EST_AUDIO_RESOURCE_HEADER_SIZE) {
		return false;
	}
	name_length = raw[6];
	if (!name_is_valid(&raw[AUDIO_RESOURCE_NAME_OFFSET], name_length)) {
		return false;
	}
	header->resource_length = read_u32_le(&raw[8]);
	header->resource_crc32 = read_u32_le(&raw[12]);
	header->duration_ms = read_u32_le(&raw[16]);
	if (header->resource_length == 0U ||
	    header->resource_length > EST_AUDIO_RESOURCE_DATA_MAX_BYTES) {
		return false;
	}
	header->slot_id = slot_id;
	header->name_length = name_length;
	memcpy(header->name, &raw[AUDIO_RESOURCE_NAME_OFFSET], name_length);
	header->name[name_length] = '\0';
	return true;
}

static void catalog_update_slot(uint8_t slot_id)
{
	resource_catalog_valid[slot_id] = read_header(slot_id,
		&resource_catalog[slot_id]);
}

void est_audio_resource_init(void)
{
	uint8_t slot;

	memset(resource_catalog_valid, 0, sizeof(resource_catalog_valid));
	for (slot = 0U; slot < EST_AUDIO_RESOURCE_SLOT_COUNT; slot++) {
		catalog_update_slot(slot);
	}
	resource_catalog_ready = true;
}

static void ensure_resource_catalog(void)
{
	if (!resource_catalog_ready) {
		est_audio_resource_init();
	}
}

static bool slot_header_is_erased(uint8_t slot_id)
{
	uint8_t raw[16];
	size_t index;

	if (!slot_is_valid(slot_id) ||
	    !board_flash_read_4byte(slot_address(slot_id), raw, sizeof(raw))) {
		return false;
	}
	for (index = 0U; index < sizeof(raw); index++) {
		if (raw[index] != 0xFFU) {
			return false;
		}
	}
	return true;
}

static bool find_slot_by_name(const uint8_t *name, uint8_t name_length,
	uint8_t *slot_id)
{
	uint8_t slot;

	for (slot = 0U; slot < EST_AUDIO_RESOURCE_SLOT_COUNT; slot++) {
		struct audio_resource_header header;

		if (read_header(slot, &header) &&
		    header.name_length == name_length &&
		    memcmp(header.name, name, name_length) == 0) {
			*slot_id = slot;
			return true;
		}
	}
	return false;
}

static bool find_free_slot(uint8_t *slot_id)
{
	uint8_t slot;

	for (slot = 0U; slot < EST_AUDIO_RESOURCE_SLOT_COUNT; slot++) {
		struct audio_resource_header header;

		if (!read_header(slot, &header) && slot_header_is_erased(slot)) {
			*slot_id = slot;
			return true;
		}
	}
	return false;
}

static uint32_t occupied_mask(void)
{
	uint8_t slot;
	uint32_t mask = 0U;

	for (slot = 0U; slot < EST_AUDIO_RESOURCE_SLOT_COUNT; slot++) {
		struct audio_resource_header header;

		if (slot < 32U && read_header(slot, &header)) {
			mask |= 1UL << slot;
		}
	}
	return mask;
}

static bool erase_slot(uint8_t slot_id)
{
	uint32_t address;
	uint32_t end;

	if (!slot_is_valid(slot_id)) {
		return false;
	}
	address = slot_address(slot_id);
	end = address + EST_AUDIO_RESOURCE_SLOT_SIZE;
	for (; address < end; address += AUDIO_RESOURCE_SECTOR_SIZE) {
		if (!board_flash_erase_sector_4byte(address)) {
			return false;
		}
	}
	return true;
}

static bool verify_bytes(uint32_t address, const uint8_t *data, uint16_t length)
{
	uint8_t buffer[AUDIO_RESOURCE_VERIFY_CHUNK_SIZE];
	uint16_t offset = 0U;

	while (offset < length) {
		uint16_t remaining = (uint16_t)(length - offset);
		uint16_t count = remaining < sizeof(buffer) ? remaining :
			(uint16_t)sizeof(buffer);

		if (!board_flash_read_4byte(address + offset, buffer, count) ||
		    memcmp(buffer, data + offset, count) != 0) {
			return false;
		}
		offset = (uint16_t)(offset + count);
	}
	return true;
}

static bool crc_matches(uint8_t slot_id, uint32_t length, uint32_t expected_crc32)
{
	uint8_t buffer[AUDIO_RESOURCE_VERIFY_CHUNK_SIZE];
	uint32_t offset = 0U;
	uint32_t crc = 0xFFFFFFFFU;
	uint32_t base = slot_data_address(slot_id);

	while (offset < length) {
		uint32_t remaining = length - offset;
		uint32_t count = remaining < sizeof(buffer) ? remaining :
			(uint32_t)sizeof(buffer);

		if (!board_flash_read_4byte(base + offset, buffer, count)) {
			return false;
		}
		crc = crc32_update(crc, buffer, count);
		offset += count;
	}
	return ~crc == expected_crc32;
}

static bool write_header(const struct audio_resource_upload_session *session)
{
	uint8_t raw[EST_AUDIO_RESOURCE_HEADER_SIZE];
	uint8_t commit_magic[4];
	struct audio_resource_header verify;

	memset(raw, 0xFF, sizeof(raw));
	memcpy(raw, AUDIO_RESOURCE_HEADER_MAGIC, 4U);
	raw[4] = AUDIO_RESOURCE_FORMAT_VERSION;
	raw[5] = EST_AUDIO_RESOURCE_HEADER_SIZE;
	raw[6] = session->name_length;
	write_u32_le(&raw[8], session->resource_length);
	write_u32_le(&raw[12], session->resource_crc32);
	write_u32_le(&raw[16], session->duration_ms);
	memcpy(&raw[AUDIO_RESOURCE_NAME_OFFSET], session->name,
		session->name_length);
	if (!board_flash_program_4byte(slot_address(session->slot_id), raw,
		sizeof(raw))) {
		return false;
	}
	memcpy(commit_magic, AUDIO_RESOURCE_COMMIT_MAGIC, sizeof(commit_magic));
	if (!board_flash_program_4byte(slot_address(session->slot_id) +
		AUDIO_RESOURCE_COMMIT_OFFSET, commit_magic, sizeof(commit_magic))) {
		return false;
	}
	return read_header(session->slot_id, &verify) &&
		verify.resource_length == session->resource_length &&
		verify.resource_crc32 == session->resource_crc32 &&
		verify.duration_ms == session->duration_ms &&
		verify.name_length == session->name_length &&
		memcmp(verify.name, session->name, session->name_length) == 0;
}

static void fill_status(uint8_t slot_id, est_audio_resource_error_t error,
	est_audio_resource_status_t *status)
{
	struct audio_resource_header header;
	bool valid_slot = slot_is_valid(slot_id);

	if (status == NULL) {
		return;
	}
	memset(status, 0, sizeof(*status));
	status->slot_id = slot_id;
	status->slot_count = EST_AUDIO_RESOURCE_SLOT_COUNT;
	status->region_start = EST_AUDIO_RESOURCE_REGION_START;
	status->region_size = EST_AUDIO_RESOURCE_REGION_SIZE;
	status->slot_size = EST_AUDIO_RESOURCE_SLOT_SIZE;
	status->data_max_bytes = EST_AUDIO_RESOURCE_DATA_MAX_BYTES;
	status->supported = supported_identity(board_flash_read_identity());
	status->busy = upload_session.active;
	status->last_error = error;
	if (status->supported) {
		status->occupied_mask = occupied_mask();
	}
	if (!valid_slot) {
		return;
	}
	if (read_header(slot_id, &header)) {
		status->occupied = true;
		status->name_length = header.name_length;
		status->resource_length = header.resource_length;
		status->resource_crc32 = header.resource_crc32;
		status->duration_ms = header.duration_ms;
		memcpy(status->name, header.name, header.name_length + 1U);
		return;
	}
	if (upload_session.active && upload_session.slot_id == slot_id) {
		status->name_length = upload_session.name_length;
		status->resource_length = upload_session.resource_length;
		status->resource_crc32 = upload_session.resource_crc32;
		status->duration_ms = upload_session.duration_ms;
		memcpy(status->name, upload_session.name,
			upload_session.name_length + 1U);
	}
}

bool est_audio_resource_get_slot_status(uint8_t slot_id,
	est_audio_resource_status_t *status)
{
	est_audio_resource_error_t error = slot_is_valid(slot_id) ?
		last_error : EST_AUDIO_RESOURCE_ERROR_INVALID_SLOT;

	fill_status(slot_id, error, status);
	return slot_is_valid(slot_id);
}

bool est_audio_resource_begin(const uint8_t *name, uint8_t name_length,
	uint32_t resource_length, uint32_t resource_crc32, uint32_t duration_ms,
	uint32_t now_ms, est_audio_resource_status_t *status)
{
	uint8_t slot_id = 0U;
	bool found;

	last_error = EST_AUDIO_RESOURCE_ERROR_NONE;
	if (upload_session.active) {
		last_error = EST_AUDIO_RESOURCE_ERROR_BUSY;
		fill_status(upload_session.slot_id, last_error, status);
		return false;
	}
	if (!supported_identity(board_flash_read_identity())) {
		last_error = EST_AUDIO_RESOURCE_ERROR_UNSUPPORTED;
		fill_status(0U, last_error, status);
		return false;
	}
	if (!name_is_valid(name, name_length)) {
		last_error = EST_AUDIO_RESOURCE_ERROR_INVALID_NAME;
		fill_status(0U, last_error, status);
		return false;
	}
	if (resource_length == 0U ||
	    resource_length > EST_AUDIO_RESOURCE_DATA_MAX_BYTES) {
		last_error = EST_AUDIO_RESOURCE_ERROR_TOO_LARGE;
		fill_status(0U, last_error, status);
		return false;
	}
	found = find_slot_by_name(name, name_length, &slot_id);
	if (!found && !find_free_slot(&slot_id)) {
		last_error = EST_AUDIO_RESOURCE_ERROR_NO_SPACE;
		fill_status(0U, last_error, status);
		return false;
	}
	if (!erase_slot(slot_id)) {
		last_error = EST_AUDIO_RESOURCE_ERROR_FLASH_IO;
		fill_status(slot_id, last_error, status);
		return false;
	}
	resource_catalog_valid[slot_id] = false;
	memset(&upload_session, 0, sizeof(upload_session));
	upload_session.active = true;
	upload_session.slot_id = slot_id;
	upload_session.name_length = name_length;
	memcpy(upload_session.name, name, name_length);
	upload_session.name[name_length] = '\0';
	upload_session.resource_length = resource_length;
	upload_session.resource_crc32 = resource_crc32;
	upload_session.duration_ms = duration_ms;
	upload_session.last_activity_ms = now_ms;
	fill_status(slot_id, EST_AUDIO_RESOURCE_ERROR_NONE, status);
	return true;
}

bool est_audio_resource_write_chunk(uint32_t offset, const uint8_t *data,
	uint16_t length, uint32_t now_ms, est_audio_resource_status_t *status)
{
	uint32_t address;

	last_error = EST_AUDIO_RESOURCE_ERROR_NONE;
	if (!upload_session.active) {
		last_error = EST_AUDIO_RESOURCE_ERROR_INVALID_REQUEST;
		fill_status(0U, last_error, status);
		return false;
	}
	if (data == NULL || length == 0U || offset != upload_session.received_length ||
	    offset + length > upload_session.resource_length) {
		last_error = EST_AUDIO_RESOURCE_ERROR_INVALID_REQUEST;
		fill_status(upload_session.slot_id, last_error, status);
		return false;
	}
	address = slot_data_address(upload_session.slot_id) + offset;
	if (!board_flash_program_4byte(address, data, length) ||
	    !verify_bytes(address, data, length)) {
		last_error = EST_AUDIO_RESOURCE_ERROR_VERIFY;
		fill_status(upload_session.slot_id, last_error, status);
		return false;
	}
	upload_session.received_length += length;
	upload_session.last_activity_ms = now_ms;
	fill_status(upload_session.slot_id, EST_AUDIO_RESOURCE_ERROR_NONE, status);
	return true;
}

bool est_audio_resource_commit(est_audio_resource_status_t *status)
{
	uint8_t slot_id;

	last_error = EST_AUDIO_RESOURCE_ERROR_NONE;
	if (!upload_session.active) {
		last_error = EST_AUDIO_RESOURCE_ERROR_INVALID_REQUEST;
		fill_status(0U, last_error, status);
		return false;
	}
	slot_id = upload_session.slot_id;
	if (upload_session.received_length != upload_session.resource_length) {
		last_error = EST_AUDIO_RESOURCE_ERROR_INVALID_REQUEST;
		fill_status(slot_id, last_error, status);
		return false;
	}
	if (!crc_matches(slot_id, upload_session.resource_length,
	    upload_session.resource_crc32) || !write_header(&upload_session)) {
		last_error = EST_AUDIO_RESOURCE_ERROR_VERIFY;
		memset(&upload_session, 0, sizeof(upload_session));
		fill_status(slot_id, last_error, status);
		return false;
	}
	catalog_update_slot(slot_id);
	memset(&upload_session, 0, sizeof(upload_session));
	fill_status(slot_id, EST_AUDIO_RESOURCE_ERROR_NONE, status);
	return true;
}

bool est_audio_resource_clear_slot(uint8_t slot_id,
	est_audio_resource_status_t *status)
{
	last_error = EST_AUDIO_RESOURCE_ERROR_NONE;
	if (upload_session.active) {
		last_error = EST_AUDIO_RESOURCE_ERROR_BUSY;
		fill_status(upload_session.slot_id, last_error, status);
		return false;
	}
	if (!slot_is_valid(slot_id)) {
		last_error = EST_AUDIO_RESOURCE_ERROR_INVALID_SLOT;
		fill_status(slot_id, last_error, status);
		return false;
	}
	if (!supported_identity(board_flash_read_identity())) {
		last_error = EST_AUDIO_RESOURCE_ERROR_UNSUPPORTED;
		fill_status(slot_id, last_error, status);
		return false;
	}
	if (!erase_slot(slot_id)) {
		last_error = EST_AUDIO_RESOURCE_ERROR_FLASH_IO;
		fill_status(slot_id, last_error, status);
		return false;
	}
	resource_catalog_valid[slot_id] = false;
	fill_status(slot_id, EST_AUDIO_RESOURCE_ERROR_NONE, status);
	return true;
}

bool est_audio_resource_find(const char *name, struct audio_resource *resource,
	char *name_buffer, size_t name_buffer_length)
{
	uint8_t slot;

	if (name == NULL || resource == NULL || name_buffer == NULL) {
		return false;
	}
	ensure_resource_catalog();
	for (slot = 0U; slot < EST_AUDIO_RESOURCE_SLOT_COUNT; slot++) {
		const struct audio_resource_header *header = &resource_catalog[slot];

		if (!resource_catalog_valid[slot] || strcmp(name, header->name) != 0) {
			continue;
		}
		if ((size_t)header->name_length + 1U > name_buffer_length) {
			return false;
		}
		memcpy(name_buffer, header->name, header->name_length + 1U);
		resource->name = name_buffer;
		resource->data = NULL;
		resource->length = header->resource_length;
		resource->duration_ms = header->duration_ms;
		resource->flash_address = slot_data_address(slot);
		return true;
	}
	return false;
}

void est_audio_resource_tick(uint32_t now_ms)
{
	if (upload_session.active &&
	    (uint32_t)(now_ms - upload_session.last_activity_ms) >=
		AUDIO_RESOURCE_UPLOAD_TIMEOUT_MS) {
		memset(&upload_session, 0, sizeof(upload_session));
		last_error = EST_AUDIO_RESOURCE_ERROR_BUSY;
	}
}
