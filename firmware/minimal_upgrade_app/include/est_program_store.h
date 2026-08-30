#ifndef EST_PROGRAM_STORE_H
#define EST_PROGRAM_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "board_flash.h"
#include "est_types.h"

#define EST_PROGRAM_STORE_FLASH_SIZE 33554432U
#define EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT 8U
#define EST_PROGRAM_STORE_PROGRAM_SLOT_SIZE 24576U
#define EST_PROGRAM_STORE_REGION_START 0x01FD0000U
#define EST_PROGRAM_STORE_REGION_SIZE 196608U
#define EST_PROGRAM_STORE_BANK_SIZE 12288U
#define EST_PROGRAM_STORE_BANK_COUNT 2U
#define EST_PROGRAM_STORE_SECTORS_PER_BANK 3U
#define EST_PROGRAM_STORE_SECTORS_PER_PROGRAM 6U
#define EST_PROGRAM_STORE_NAME_MAX_BYTES 31U

#define EST_PROGRAM_STORE_FLAG_SUPPORTED_DEVICE 0x01U
#define EST_PROGRAM_STORE_FLAG_SCAN_COMPLETE 0x02U
#define EST_PROGRAM_STORE_FLAG_ALL_ERASED 0x04U
#define EST_PROGRAM_STORE_FLAG_READ_ONLY 0x08U
#define EST_PROGRAM_STORE_FLAG_WRITE_SUPPORTED 0x10U
#define EST_PROGRAM_STORE_FLAG_SAVED_PRESENT 0x20U
#define EST_PROGRAM_STORE_FLAG_OWNED 0x40U

typedef enum {
	EST_PROGRAM_STORE_UNSUPPORTED = 0,
	EST_PROGRAM_STORE_OCCUPIED = 1,
	EST_PROGRAM_STORE_READY = 2,
	EST_PROGRAM_STORE_SAVED = 3
} est_program_store_state_t;

typedef enum {
	EST_PROGRAM_STORE_ERROR_NONE = 0,
	EST_PROGRAM_STORE_ERROR_UNSUPPORTED = 1,
	EST_PROGRAM_STORE_ERROR_FOREIGN_DATA = 2,
	EST_PROGRAM_STORE_ERROR_NO_RAM_PROGRAM = 3,
	EST_PROGRAM_STORE_ERROR_FLASH_IO = 4,
	EST_PROGRAM_STORE_ERROR_VERIFY = 5,
	EST_PROGRAM_STORE_ERROR_INVALID_RECORD = 6,
	EST_PROGRAM_STORE_ERROR_BUSY = 7,
	EST_PROGRAM_STORE_ERROR_NO_SAVED_PROGRAM = 8,
	EST_PROGRAM_STORE_ERROR_INVALID_SLOT = 9,
	EST_PROGRAM_STORE_ERROR_INVALID_NAME = 10
} est_program_store_error_t;

typedef enum {
	EST_PROGRAM_STORE_RECORD_NONE = 0,
	EST_PROGRAM_STORE_RECORD_PROGRAM = 1,
	EST_PROGRAM_STORE_RECORD_TOMBSTONE = 2
} est_program_store_record_type_t;

typedef struct {
	est_program_store_state_t state;
	uint8_t flags;
	uint8_t erased_sector_mask;
	uint8_t occupied_sector_mask;
	uint8_t program_slot_id;
	uint8_t active_bank;
	uint8_t name_length;
	char name[EST_PROGRAM_STORE_NAME_MAX_BYTES + 1U];
	est_program_store_record_type_t record_type;
	est_program_store_error_t last_error;
	uint32_t generation;
	uint16_t source_length;
	uint32_t source_crc32;
	struct board_flash_identity identity;
} est_program_store_status_t;

bool est_program_store_get_status(est_program_store_status_t *status);
bool est_program_store_get_slot_status(uint8_t program_slot_id,
	est_program_store_status_t *status);
est_result_t est_program_store_save(uint8_t program_slot_id,
	const uint8_t *name, uint8_t name_length);
est_result_t est_program_store_load(uint8_t program_slot_id);
est_result_t est_program_store_clear(uint8_t program_slot_id);
bool est_program_store_last_change(uint32_t *sequence, uint8_t *program_slot_id,
	est_program_store_record_type_t *record_type);

#endif
