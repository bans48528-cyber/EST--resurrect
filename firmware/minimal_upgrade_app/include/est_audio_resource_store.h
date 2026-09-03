#ifndef EST_AUDIO_RESOURCE_STORE_H
#define EST_AUDIO_RESOURCE_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_resources.h"

#define EST_AUDIO_RESOURCE_FLASH_SIZE 33554432U
#define EST_AUDIO_RESOURCE_REGION_START 0x01F81000U
#define EST_AUDIO_RESOURCE_REGION_SIZE 0x0004C000U
#define EST_AUDIO_RESOURCE_SLOT_SIZE 0x00004000U
#define EST_AUDIO_RESOURCE_HEADER_SIZE 128U
#define EST_AUDIO_RESOURCE_SLOT_COUNT \
	(EST_AUDIO_RESOURCE_REGION_SIZE / EST_AUDIO_RESOURCE_SLOT_SIZE)
#define EST_AUDIO_RESOURCE_NAME_MAX_BYTES 47U
#define EST_AUDIO_RESOURCE_DATA_MAX_BYTES \
	(EST_AUDIO_RESOURCE_SLOT_SIZE - EST_AUDIO_RESOURCE_HEADER_SIZE)

#define EST_AUDIO_RESOURCE_STATUS_SCHEMA_VERSION 1U
#define EST_AUDIO_RESOURCE_STATUS_FLAG_SUPPORTED_DEVICE 0x01U
#define EST_AUDIO_RESOURCE_STATUS_FLAG_OCCUPIED 0x02U
#define EST_AUDIO_RESOURCE_STATUS_FLAG_BUSY 0x04U

typedef enum {
	EST_AUDIO_RESOURCE_ERROR_NONE = 0,
	EST_AUDIO_RESOURCE_ERROR_UNSUPPORTED = 1,
	EST_AUDIO_RESOURCE_ERROR_INVALID_REQUEST = 2,
	EST_AUDIO_RESOURCE_ERROR_INVALID_NAME = 3,
	EST_AUDIO_RESOURCE_ERROR_TOO_LARGE = 4,
	EST_AUDIO_RESOURCE_ERROR_NO_SPACE = 5,
	EST_AUDIO_RESOURCE_ERROR_FLASH_IO = 6,
	EST_AUDIO_RESOURCE_ERROR_VERIFY = 7,
	EST_AUDIO_RESOURCE_ERROR_BUSY = 8,
	EST_AUDIO_RESOURCE_ERROR_INVALID_SLOT = 9,
	EST_AUDIO_RESOURCE_ERROR_NOT_FOUND = 10
} est_audio_resource_error_t;

typedef struct {
	bool supported;
	bool occupied;
	bool busy;
	uint8_t slot_id;
	uint8_t slot_count;
	uint8_t name_length;
	char name[EST_AUDIO_RESOURCE_NAME_MAX_BYTES + 1U];
	uint32_t region_start;
	uint32_t region_size;
	uint32_t slot_size;
	uint32_t data_max_bytes;
	uint32_t resource_length;
	uint32_t resource_crc32;
	uint32_t duration_ms;
	uint32_t occupied_mask;
	est_audio_resource_error_t last_error;
} est_audio_resource_status_t;

bool est_audio_resource_get_slot_status(uint8_t slot_id,
	est_audio_resource_status_t *status);
bool est_audio_resource_begin(const uint8_t *name, uint8_t name_length,
	uint32_t resource_length, uint32_t resource_crc32, uint32_t duration_ms,
	uint32_t now_ms, est_audio_resource_status_t *status);
bool est_audio_resource_write_chunk(uint32_t offset, const uint8_t *data,
	uint16_t length, uint32_t now_ms, est_audio_resource_status_t *status);
bool est_audio_resource_commit(est_audio_resource_status_t *status);
bool est_audio_resource_clear_slot(uint8_t slot_id,
	est_audio_resource_status_t *status);
bool est_audio_resource_find(const char *name, struct audio_resource *resource,
	char *name_buffer, size_t name_buffer_length);
void est_audio_resource_tick(uint32_t now_ms);

#endif
