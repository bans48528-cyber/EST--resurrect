#ifndef BOARD_FLASH_H
#define BOARD_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct board_flash_identity {
	uint8_t manufacturer;
	uint8_t memory_type;
	uint8_t capacity;
};

struct board_flash_status {
	uint8_t status1;
	uint8_t status2;
	uint8_t status3;
};

struct board_flash_mode_probe {
	uint8_t status1_before;
	uint8_t status1_write_enabled;
	uint8_t status1_write_disabled;
	uint8_t status3_before;
	uint8_t status3_four_byte;
	uint8_t status3_restored;
};

enum board_flash_test_result {
	BOARD_FLASH_TEST_SUCCESS = 1,
	BOARD_FLASH_TEST_NOT_ERASED = 2,
	BOARD_FLASH_TEST_PROGRAM_TIMEOUT = 3,
	BOARD_FLASH_TEST_PROGRAM_VERIFY_FAILED = 4,
	BOARD_FLASH_TEST_ALIAS_CHANGED = 5,
	BOARD_FLASH_TEST_ERASE_TIMEOUT = 6,
	BOARD_FLASH_TEST_ERASE_VERIFY_FAILED = 7,
	BOARD_FLASH_TEST_UNSUPPORTED_DEVICE = 8,
	BOARD_FLASH_TEST_WRITE_ENABLE_FAILED = 9,
	BOARD_FLASH_TEST_ENTER_4BYTE_FAILED = 10,
	BOARD_FLASH_TEST_RESTORE_3BYTE_FAILED = 11
};

void board_flash_init(void);
struct board_flash_identity board_flash_read_identity(void);
struct board_flash_status board_flash_read_status(void);
struct board_flash_mode_probe board_flash_probe_modes(void);
bool board_flash_sector_is_erased_4byte(uint32_t address);
bool board_flash_read_4byte(uint32_t address, uint8_t *buffer, size_t length);
bool board_flash_program_4byte(uint32_t address, const uint8_t *data,
	size_t length);
bool board_flash_erase_sector_4byte(uint32_t address);
enum board_flash_test_result board_flash_test_empty_sector_4byte(uint32_t address);

#endif
