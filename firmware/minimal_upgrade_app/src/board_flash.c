#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

#include "board_flash.h"
#include "watchdog.h"

#define FLASH_CHIP_SELECT_PORT GPIOA
#define FLASH_CHIP_SELECT_PIN GPIO15
#define FLASH_CLOCK_PORT GPIOC
#define FLASH_CLOCK_PIN GPIO10
#define FLASH_MISO_PORT GPIOC
#define FLASH_MISO_PIN GPIO11
#define FLASH_MOSI_PORT GPIOC
#define FLASH_MOSI_PIN GPIO12

#define READ_JEDEC_ID_COMMAND 0x9FU
#define READ_DATA_4BYTE_COMMAND 0x13U
#define READ_DATA_COMMAND 0x03U
#define PAGE_PROGRAM_COMMAND 0x02U
#define SECTOR_ERASE_COMMAND 0x20U
#define WRITE_ENABLE_COMMAND 0x06U
#define WRITE_DISABLE_COMMAND 0x04U
#define ENTER_4BYTE_MODE_COMMAND 0xB7U
#define EXIT_4BYTE_MODE_COMMAND 0xE9U
#define READ_STATUS_COMMAND 0x05U
#define READ_STATUS_2_COMMAND 0x35U
#define READ_STATUS_3_COMMAND 0x15U

#define FLASH_SECTOR_SIZE 4096U
#define FLASH_PAGE_SIZE 256U
#define FLASH_CAPACITY_BYTES 33554432U
#define FLASH_TEST_PATTERN_SIZE 32U
#define FLASH_BUSY_POLL_LIMIT 2000000U

static uint8_t transfer_byte(uint8_t value)
{
	return (uint8_t)spi_xfer(SPI3, value);
}

static void select_flash(void)
{
	gpio_clear(FLASH_CHIP_SELECT_PORT, FLASH_CHIP_SELECT_PIN);
}

static void deselect_flash(void)
{
	gpio_set(FLASH_CHIP_SELECT_PORT, FLASH_CHIP_SELECT_PIN);
}

static void send_address_4byte(uint32_t address)
{
	(void)transfer_byte((uint8_t)(address >> 24U));
	(void)transfer_byte((uint8_t)(address >> 16U));
	(void)transfer_byte((uint8_t)(address >> 8U));
	(void)transfer_byte((uint8_t)address);
}

static void read_data_4byte(uint32_t address, uint8_t *buffer, size_t length)
{
	size_t index;

	select_flash();
	(void)transfer_byte(READ_DATA_4BYTE_COMMAND);
	send_address_4byte(address);
	for (index = 0U; index < length; index++) {
		buffer[index] = transfer_byte(0xFFU);
	}
	deselect_flash();
}

static void read_data_in_4byte_mode(uint32_t address, uint8_t *buffer,
	size_t length)
{
	size_t index;

	select_flash();
	(void)transfer_byte(READ_DATA_COMMAND);
	send_address_4byte(address);
	for (index = 0U; index < length; index++) {
		buffer[index] = transfer_byte(0xFFU);
	}
	deselect_flash();
}

static void write_enable(void)
{
	select_flash();
	(void)transfer_byte(WRITE_ENABLE_COMMAND);
	deselect_flash();
}

static void send_command(uint8_t command)
{
	select_flash();
	(void)transfer_byte(command);
	deselect_flash();
}

static bool wait_ready(void)
{
	uint32_t poll;
	uint8_t status;

	for (poll = 0U; poll < FLASH_BUSY_POLL_LIMIT; poll++) {
		if ((poll & 0xFFU) == 0U) {
			watchdog_kick();
		}
		select_flash();
		(void)transfer_byte(READ_STATUS_COMMAND);
		status = transfer_byte(0xFFU);
		deselect_flash();
		if ((status & 0x01U) == 0U) {
			return true;
		}
	}
	return false;
}

static uint8_t read_status_register(uint8_t command)
{
	uint8_t status;

	select_flash();
	(void)transfer_byte(command);
	status = transfer_byte(0xFFU);
	deselect_flash();
	return status;
}

static bool write_enable_latched(void)
{
	write_enable();
	return (read_status_register(READ_STATUS_COMMAND) & 0x02U) != 0U;
}

static bool page_program_in_4byte_mode(uint32_t address, const uint8_t *data,
	size_t length)
{
	size_t index;

	select_flash();
	(void)transfer_byte(PAGE_PROGRAM_COMMAND);
	send_address_4byte(address);
	for (index = 0U; index < length; index++) {
		(void)transfer_byte(data[index]);
	}
	deselect_flash();
	return wait_ready();
}

static bool erase_sector_in_4byte_mode(uint32_t address)
{
	select_flash();
	(void)transfer_byte(SECTOR_ERASE_COMMAND);
	send_address_4byte(address);
	deselect_flash();
	return wait_ready();
}

static bool sector_is_erased_in_4byte_mode(uint32_t address)
{
	uint8_t buffer[32];
	uint32_t offset;
	size_t index;

	for (offset = 0U; offset < FLASH_SECTOR_SIZE; offset += sizeof(buffer)) {
		watchdog_kick();
		read_data_in_4byte_mode(address + offset, buffer, sizeof(buffer));
		for (index = 0U; index < sizeof(buffer); index++) {
			if (buffer[index] != 0xFFU) {
				return false;
			}
		}
	}
	return true;
}

static bool range_is_valid(uint32_t address, size_t length)
{
	return length > 0U && address < FLASH_CAPACITY_BYTES &&
		length <= (size_t)(FLASH_CAPACITY_BYTES - address);
}

void board_flash_init(void)
{
	const uint16_t spi_pins = FLASH_CLOCK_PIN | FLASH_MISO_PIN | FLASH_MOSI_PIN;

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOC);
	rcc_periph_clock_enable(RCC_SPI3);

	/* Keep the flash deselected before enabling its shared SPI3 bus. */
	gpio_set(FLASH_CHIP_SELECT_PORT, FLASH_CHIP_SELECT_PIN);
	gpio_mode_setup(FLASH_CHIP_SELECT_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
		FLASH_CHIP_SELECT_PIN);
	gpio_set_output_options(FLASH_CHIP_SELECT_PORT, GPIO_OTYPE_PP,
		GPIO_OSPEED_50MHZ, FLASH_CHIP_SELECT_PIN);

	gpio_mode_setup(FLASH_CLOCK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, spi_pins);
	gpio_set_output_options(FLASH_CLOCK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
		spi_pins);
	gpio_set_af(FLASH_CLOCK_PORT, GPIO_AF6, spi_pins);

	(void)spi_init_master(SPI3, SPI_CR1_BAUDRATE_FPCLK_DIV_64,
		SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE, SPI_CR1_CPHA_CLK_TRANSITION_1,
		SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);
	spi_enable_software_slave_management(SPI3);
	spi_set_nss_high(SPI3);
	spi_enable(SPI3);
}

struct board_flash_identity board_flash_read_identity(void)
{
	struct board_flash_identity identity;

	gpio_clear(FLASH_CHIP_SELECT_PORT, FLASH_CHIP_SELECT_PIN);
	(void)transfer_byte(READ_JEDEC_ID_COMMAND);
	identity.manufacturer = transfer_byte(0xFFU);
	identity.memory_type = transfer_byte(0xFFU);
	identity.capacity = transfer_byte(0xFFU);
	gpio_set(FLASH_CHIP_SELECT_PORT, FLASH_CHIP_SELECT_PIN);

	return identity;
}

struct board_flash_status board_flash_read_status(void)
{
	struct board_flash_status status;

	status.status1 = read_status_register(READ_STATUS_COMMAND);
	status.status2 = read_status_register(READ_STATUS_2_COMMAND);
	status.status3 = read_status_register(READ_STATUS_3_COMMAND);
	return status;
}

struct board_flash_mode_probe board_flash_probe_modes(void)
{
	struct board_flash_mode_probe probe;

	probe.status1_before = read_status_register(READ_STATUS_COMMAND);
	write_enable();
	probe.status1_write_enabled = read_status_register(READ_STATUS_COMMAND);
	send_command(WRITE_DISABLE_COMMAND);
	probe.status1_write_disabled = read_status_register(READ_STATUS_COMMAND);

	probe.status3_before = read_status_register(READ_STATUS_3_COMMAND);
	send_command(ENTER_4BYTE_MODE_COMMAND);
	probe.status3_four_byte = read_status_register(READ_STATUS_3_COMMAND);
	send_command(EXIT_4BYTE_MODE_COMMAND);
	probe.status3_restored = read_status_register(READ_STATUS_3_COMMAND);
	return probe;
}

bool board_flash_sector_is_erased_4byte(uint32_t address)
{
	uint8_t buffer[32];
	uint32_t offset;
	size_t index;

	for (offset = 0U; offset < FLASH_SECTOR_SIZE; offset += sizeof(buffer)) {
		watchdog_kick();
		read_data_4byte(address + offset, buffer, sizeof(buffer));
		for (index = 0U; index < sizeof(buffer); index++) {
			if (buffer[index] != 0xFFU) {
				return false;
			}
		}
	}
	return true;
}

bool board_flash_read_4byte(uint32_t address, uint8_t *buffer, size_t length)
{
	if (buffer == NULL || !range_is_valid(address, length)) {
		return false;
	}
	read_data_4byte(address, buffer, length);
	return true;
}

bool board_flash_program_4byte(uint32_t address, const uint8_t *data,
	size_t length)
{
	bool success = false;
	bool mode_change_attempted = false;

	if (data == NULL || !range_is_valid(address, length)) {
		return false;
	}
	send_command(ENTER_4BYTE_MODE_COMMAND);
	mode_change_attempted = true;
	if ((read_status_register(READ_STATUS_3_COMMAND) & 0x01U) == 0U) {
		goto cleanup;
	}
	while (length > 0U) {
		size_t page_remaining = FLASH_PAGE_SIZE -
			(size_t)(address & (FLASH_PAGE_SIZE - 1U));
		size_t chunk_length = length < page_remaining ? length : page_remaining;

		if (!write_enable_latched() ||
		    !page_program_in_4byte_mode(address, data, chunk_length)) {
			goto cleanup;
		}
		address += (uint32_t)chunk_length;
		data += chunk_length;
		length -= chunk_length;
	}
	success = true;

cleanup:
	if (mode_change_attempted) {
		send_command(EXIT_4BYTE_MODE_COMMAND);
		if ((read_status_register(READ_STATUS_3_COMMAND) & 0x01U) != 0U) {
			success = false;
		}
	}
	return success;
}

bool board_flash_erase_sector_4byte(uint32_t address)
{
	bool success = false;
	bool mode_change_attempted = false;

	if ((address & (FLASH_SECTOR_SIZE - 1U)) != 0U ||
	    !range_is_valid(address, FLASH_SECTOR_SIZE)) {
		return false;
	}
	send_command(ENTER_4BYTE_MODE_COMMAND);
	mode_change_attempted = true;
	if ((read_status_register(READ_STATUS_3_COMMAND) & 0x01U) == 0U ||
	    !write_enable_latched() ||
	    !erase_sector_in_4byte_mode(address)) {
		goto cleanup;
	}
	success = true;

cleanup:
	if (mode_change_attempted) {
		send_command(EXIT_4BYTE_MODE_COMMAND);
		if ((read_status_register(READ_STATUS_3_COMMAND) & 0x01U) != 0U) {
			success = false;
		}
	}
	return success;
}

enum board_flash_test_result board_flash_test_empty_sector_4byte(uint32_t address)
{
	static const uint8_t pattern[FLASH_TEST_PATTERN_SIZE] = {
		0x45U, 0x53U, 0x54U, 0x2DU, 0x34U, 0x42U, 0x59U, 0x54U,
		0x45U, 0x2DU, 0x4DU, 0x4FU, 0x44U, 0x45U, 0x2DU, 0x54U,
		0xA5U, 0x5AU, 0x3CU, 0xC3U, 0x96U, 0x69U, 0x0FU, 0xF0U,
		0x12U, 0x34U, 0x56U, 0x78U, 0x87U, 0x65U, 0x43U, 0x21U
	};
	uint8_t alias_before[FLASH_TEST_PATTERN_SIZE];
	uint8_t alias_after[FLASH_TEST_PATTERN_SIZE];
	uint8_t verify[FLASH_TEST_PATTERN_SIZE];
	uint32_t alias_address = address & 0x00FFFFFFU;
	enum board_flash_test_result result = BOARD_FLASH_TEST_SUCCESS;
	bool write_attempted = false;
	bool mode_change_attempted = false;

	if (!board_flash_sector_is_erased_4byte(address)) {
		return BOARD_FLASH_TEST_NOT_ERASED;
	}
	read_data_4byte(alias_address, alias_before, sizeof(alias_before));

	send_command(ENTER_4BYTE_MODE_COMMAND);
	mode_change_attempted = true;
	if ((read_status_register(READ_STATUS_3_COMMAND) & 0x01U) == 0U) {
		result = BOARD_FLASH_TEST_ENTER_4BYTE_FAILED;
		goto cleanup;
	}
	if (!write_enable_latched()) {
		result = BOARD_FLASH_TEST_WRITE_ENABLE_FAILED;
		goto cleanup;
	}
	write_attempted = true;
	if (!page_program_in_4byte_mode(address, pattern, sizeof(pattern))) {
		result = BOARD_FLASH_TEST_PROGRAM_TIMEOUT;
		goto cleanup;
	}
	read_data_in_4byte_mode(address, verify, sizeof(verify));
	if (memcmp(verify, pattern, sizeof(pattern)) != 0) {
		result = BOARD_FLASH_TEST_PROGRAM_VERIFY_FAILED;
		goto cleanup;
	}
	read_data_in_4byte_mode(alias_address, alias_after, sizeof(alias_after));
	if (memcmp(alias_before, alias_after, sizeof(alias_before)) != 0) {
		result = BOARD_FLASH_TEST_ALIAS_CHANGED;
	}

cleanup:
	if (write_attempted) {
		if (!write_enable_latched()) {
			result = BOARD_FLASH_TEST_WRITE_ENABLE_FAILED;
		} else if (!erase_sector_in_4byte_mode(address)) {
			result = BOARD_FLASH_TEST_ERASE_TIMEOUT;
		} else if (!sector_is_erased_in_4byte_mode(address)) {
			result = BOARD_FLASH_TEST_ERASE_VERIFY_FAILED;
		}
	}
	if (mode_change_attempted) {
		send_command(EXIT_4BYTE_MODE_COMMAND);
		if ((read_status_register(READ_STATUS_3_COMMAND) & 0x01U) != 0U) {
			result = BOARD_FLASH_TEST_RESTORE_3BYTE_FAILED;
		}
	}
	return result;
}
