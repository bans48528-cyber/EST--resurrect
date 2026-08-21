#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <libopencm3/stm32/flash.h>

#include "app_config.h"
#include "watchdog.h"
#include "update_storage.h"

#define FLASH_ERROR_MASK (FLASH_SR_PGSERR | FLASH_SR_PGPERR | \
	FLASH_SR_PGAERR | FLASH_SR_WRPERR | FLASH_SR_OPERR)

static bool storage_open;

static void clear_flash_status(void)
{
	FLASH_SR = FLASH_ERROR_MASK | FLASH_SR_EOP;
}

static bool flash_ok(void)
{
	return (FLASH_SR & FLASH_ERROR_MASK) == 0U;
}

static bool range_is_erased(uint32_t start, uint32_t end)
{
	uint32_t address;

	for (address = start; address < end; address += sizeof(uint32_t)) {
		if (*(const volatile uint32_t *)address != 0xFFFFFFFFU) {
			return false;
		}
		watchdog_kick();
	}
	return true;
}

bool update_storage_begin(void)
{
	uint8_t sector;

	storage_open = false;
	flash_unlock();
	clear_flash_status();
	for (sector = 8U; sector <= 11U; sector++) {
		watchdog_kick();
		flash_erase_sector(sector, FLASH_CR_PROGRAM_X32);
		watchdog_kick();
		if (!flash_ok()) {
			flash_lock();
			return false;
		}
	}

	if (!range_is_erased(UPDATE_FLASH_START, UPDATE_FLASH_PHYSICAL_END)) {
		flash_lock();
		return false;
	}
	storage_open = true;
	return true;
}

bool update_storage_write(uint32_t offset, const uint8_t *data, size_t length)
{
	uint32_t address;
	size_t index = 0U;

	if (!storage_open || data == NULL || length == 0U ||
	    offset >= UPDATE_MAX_PACKAGE_SIZE ||
	    length > (UPDATE_MAX_PACKAGE_SIZE - offset)) {
		return false;
	}

	address = UPDATE_FLASH_START + offset;
	clear_flash_status();
	if (((address & 1U) != 0U) && index < length) {
		flash_program_byte(address, data[index]);
		address++;
		index++;
	}
	while ((length - index) >= 2U) {
		uint16_t value = (uint16_t)data[index] |
			((uint16_t)data[index + 1U] << 8U);
		flash_program_half_word(address, value);
		address += 2U;
		index += 2U;
		watchdog_kick();
	}
	if (index < length) {
		flash_program_byte(address, data[index]);
	}

	if (!flash_ok()) {
		return false;
	}
	for (index = 0U; index < length; index++) {
		if (*(const volatile uint8_t *)(UPDATE_FLASH_START + offset + index) !=
		    data[index]) {
			return false;
		}
	}
	return true;
}

bool update_storage_validate_image(uint32_t package_length)
{
	const volatile uint8_t *header = (const volatile uint8_t *)UPDATE_FLASH_START;
	uint32_t initial_msp;
	uint32_t reset_handler;
	uint32_t reset_address;
	uint32_t index;
	bool msp_in_sram;
	bool msp_in_ccm;

	if (!storage_open || package_length < 12U ||
	    package_length >= UPDATE_MAX_PACKAGE_SIZE ||
	    (package_length & 1U) != 0U) {
		return false;
	}
	if (header[0] != 'A' || header[1] != 'P' ||
	    header[2] != 'P' || header[3] != '=') {
		return false;
	}

	for (index = 0U; index < package_length; index += 4096U) {
		watchdog_kick();
	}

	initial_msp = *(const volatile uint32_t *)(UPDATE_FLASH_START + 4U);
	reset_handler = *(const volatile uint32_t *)(UPDATE_FLASH_START + 8U);
	reset_address = reset_handler & ~1U;
	msp_in_sram = initial_msp >= 0x20000000U && initial_msp <= 0x20030000U;
	msp_in_ccm = initial_msp >= 0x10000000U && initial_msp <= 0x10010000U;

	return (msp_in_sram || msp_in_ccm) &&
		((reset_handler & 1U) != 0U) &&
		reset_address >= APP_FLASH_START && reset_address < APP_FLASH_END;
}

bool update_storage_commit(uint32_t package_length)
{
	uint32_t stored_length;

	if (!update_storage_validate_image(package_length)) {
		return false;
	}
	stored_length = package_length - 1U;

	clear_flash_status();
	watchdog_kick();
	flash_erase_sector(UPDATE_STATUS_FLASH_SECTOR, FLASH_CR_PROGRAM_X32);
	watchdog_kick();
	if (!flash_ok()) {
		flash_lock();
		storage_open = false;
		return false;
	}

	flash_program_half_word(UPDATE_LENGTH_LOW_ADDRESS,
		(uint16_t)(stored_length & 0xFFFFU));
	flash_program_half_word(UPDATE_LENGTH_HIGH_ADDRESS,
		(uint16_t)(stored_length >> 16U));
	if (!flash_ok() ||
	    *(const volatile uint16_t *)UPDATE_LENGTH_LOW_ADDRESS !=
		(uint16_t)(stored_length & 0xFFFFU) ||
	    *(const volatile uint16_t *)UPDATE_LENGTH_HIGH_ADDRESS !=
		(uint16_t)(stored_length >> 16U)) {
		flash_lock();
		storage_open = false;
		return false;
	}

	flash_program_half_word(UPDATE_STATUS_ADDRESS, UPDATE_STATUS_PENDING);
	flash_lock();
	storage_open = false;
	return flash_ok() &&
		*(const volatile uint16_t *)UPDATE_STATUS_ADDRESS == UPDATE_STATUS_PENDING;
}

void update_storage_abort(void)
{
	flash_lock();
	storage_open = false;
}
