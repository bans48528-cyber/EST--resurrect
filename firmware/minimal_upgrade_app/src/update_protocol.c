#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "app_version.h"
#include "board_flash.h"
#include "board_motor.h"
#include "board_sensor.h"
#include "est_battery.h"
#include "est_buttons.h"
#include "est_drive.h"
#include "est_micropython.h"
#include "est_motor.h"
#include "est_program_store.h"
#include "est_sensor.h"
#include "est_system.h"
#include "update_protocol.h"
#include "update_storage.h"
#include "usb_hid.h"

#define DEVICE_STATUS_PAYLOAD_LENGTH 72U
#define DEVICE_STATUS_MOTOR_OFFSET 24U
#define DEVICE_STATUS_SENSOR_OFFSET 48U
#define DEVICE_STATUS_PORT_ENTRY_LENGTH 6U
#define MICROPYTHON_STATUS_PAYLOAD_LENGTH 28U
#define PYTHON_PROGRAM_STATUS_PAYLOAD_LENGTH 32U
#define PERSISTENT_PROGRAM_STATUS_PAYLOAD_LENGTH 76U
#define PERSISTENT_PROGRAM_STATUS_SCHEMA_VERSION 3U

enum update_ack_flag {
	UPDATE_ACK_FAILURE = 0x00,
	UPDATE_ACK_SUCCESS = 0x01,
	UPDATE_ACK_TIMEOUT = 0x02
};

struct update_session {
	bool active;
	uint16_t total_frames;
	uint16_t next_frame;
	uint32_t received_bytes;
	uint32_t last_activity_ms;
};

static struct update_session session;
static uint8_t logical_frame[UPDATE_FRAME_MAX_LENGTH];
static size_t logical_frame_length;
static size_t expected_frame_length;
static uint32_t logical_frame_last_report_ms;
static bool committed;
static uint32_t power_off_deadline_ms;

static uint16_t read_u16_le(const uint8_t *bytes)
{
	return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U);
}

static int32_t read_i32_le(const uint8_t *bytes)
{
	uint32_t value = (uint32_t)bytes[0] |
		((uint32_t)bytes[1] << 8U) |
		((uint32_t)bytes[2] << 16U) |
		((uint32_t)bytes[3] << 24U);

	return (int32_t)value;
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] |
		((uint32_t)bytes[1] << 8U) |
		((uint32_t)bytes[2] << 16U) |
		((uint32_t)bytes[3] << 24U);
}

static void write_i32_le(uint8_t *bytes, int32_t value)
{
	uint32_t encoded = (uint32_t)value;

	bytes[0] = (uint8_t)encoded;
	bytes[1] = (uint8_t)(encoded >> 8U);
	bytes[2] = (uint8_t)(encoded >> 16U);
	bytes[3] = (uint8_t)(encoded >> 24U);
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

static uint8_t checksum(const uint8_t *bytes, size_t length)
{
	uint8_t sum = 0U;
	size_t index;

	for (index = 0U; index < length; index++) {
		sum = (uint8_t)(sum + bytes[index]);
	}
	return sum;
}

static bool report_starts_logical_frame(const uint8_t *report, size_t length)
{
	uint16_t data_length;

	if (length < 5U || report[0] != FRAME_START_BYTE ||
	    report[1] != HOST_FRAME_DIRECTION) {
		return false;
	}
	data_length = read_u16_le(&report[3]);
	if (report[2] == HEARTBEAT_COMMAND || report[2] == KEY_STATUS_COMMAND ||
	    report[2] == FLASH_ID_COMMAND || report[2] == FLASH_SCAN_COMMAND ||
	    report[2] == FLASH_TEST_COMMAND || report[2] == FLASH_STATUS_COMMAND ||
	    report[2] == FLASH_MODE_PROBE_COMMAND ||
	    report[2] == DEVICE_STATUS_COMMAND ||
	    report[2] == MICROPYTHON_STATUS_COMMAND ||
	    report[2] == MOTOR_TYPE_COMMAND) {
		return data_length == 0U;
	}
	if (report[2] == MOTOR_TEST_COMMAND) {
		return data_length == 1U;
	}
	if (report[2] == MOTOR_TACHO_TEST_COMMAND) {
		return data_length == 1U || data_length == 2U || data_length == 3U;
	}
	if (report[2] == MOTOR_STOP_TEST_COMMAND) {
		return data_length == 1U || data_length == 3U || data_length == 4U;
	}
	if (report[2] == MOTOR_DUAL_TEST_COMMAND) {
		return data_length == 1U || data_length == 2U;
	}
	if (report[2] == MOTOR_CONTROL_COMMAND) {
		return data_length == 2U || data_length == 3U;
	}
	if (report[2] == MOTOR_POSITION_COMMAND) {
		return data_length == 2U || data_length == 7U;
	}
	if (report[2] == MOTOR_PAIR_POSITION_COMMAND) {
		return data_length == 1U || data_length == 12U;
	}
	if (report[2] == MOTOR_PAIR_SPEED_COMMAND) {
		return data_length == 1U || data_length == 5U;
	}
	if (report[2] == DRIVE_STRAIGHT_COMMAND) {
		return data_length == 1U || data_length == 13U;
	}
	if (report[2] == DRIVE_RUN_COMMAND) {
		return data_length == 1U || data_length == 10U;
	}
	if (report[2] == DRIVE_STEER_FOR_COMMAND) {
		return data_length == 1U || data_length == 11U;
	}
	if (report[2] == PYTHON_PROGRAM_COMMAND) {
		return data_length >= 1U && data_length <= 1010U;
	}
	if (report[2] == PERSISTENT_PROGRAM_COMMAND) {
		return data_length >= 1U && data_length <= 34U;
	}
	if (report[2] == INPUT_SENSOR_COMMAND) {
		return data_length == 2U || data_length == 3U;
	}
	return report[2] == UPDATE_COMMAND && data_length >= 4U &&
		data_length <= UPDATE_FRAME_MAX_DATA_LENGTH;
}

static void queue_update_ack(uint16_t total_frames, uint16_t frame_index,
	enum update_ack_flag flag, bool power_off_after_report)
{
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = UPDATE_COMMAND;
	report[3] = 5U;
	report[4] = 0U;
	report[5] = (uint8_t)total_frames;
	report[6] = (uint8_t)(total_frames >> 8U);
	report[7] = (uint8_t)frame_index;
	report[8] = (uint8_t)(frame_index >> 8U);
	report[9] = (uint8_t)flag;
	report[10] = checksum(report, 10U);
	report[11] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, power_off_after_report);
}

static void queue_heartbeat(void)
{
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = HEARTBEAT_COMMAND;
	report[3] = 6U;
	report[4] = 0U;
	memcpy(&report[5], app_version_text, 6U);
	report[11] = checksum(report, 11U);
	report[12] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_key_status(void)
{
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = KEY_STATUS_COMMAND;
	report[3] = 1U;
	report[4] = 0U;
	report[5] = est_buttons_pressed_mask();
	report[6] = checksum(report, 6U);
	report[7] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_flash_identity(void)
{
	struct board_flash_identity identity = board_flash_read_identity();
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = FLASH_ID_COMMAND;
	report[3] = 3U;
	report[4] = 0U;
	report[5] = identity.manufacturer;
	report[6] = identity.memory_type;
	report[7] = identity.capacity;
	report[8] = checksum(report, 8U);
	report[9] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static bool flash_supports_4byte_test(void)
{
	struct board_flash_identity identity = board_flash_read_identity();

	return identity.manufacturer == 0xEFU && identity.memory_type == 0x40U &&
		identity.capacity == 0x19U;
}

static void queue_flash_scan(void)
{
	uint8_t report[USB_HID_REPORT_SIZE] = {0};
	bool supported = flash_supports_4byte_test();
	bool erased = supported &&
		board_flash_sector_is_erased_4byte(EXTERNAL_FLASH_TEST_ADDRESS);

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = FLASH_SCAN_COMMAND;
	report[3] = 6U;
	report[4] = 0U;
	report[5] = supported ? 1U : 0U;
	report[6] = erased ? 1U : 0U;
	report[7] = (uint8_t)EXTERNAL_FLASH_TEST_ADDRESS;
	report[8] = (uint8_t)(EXTERNAL_FLASH_TEST_ADDRESS >> 8U);
	report[9] = (uint8_t)(EXTERNAL_FLASH_TEST_ADDRESS >> 16U);
	report[10] = (uint8_t)(EXTERNAL_FLASH_TEST_ADDRESS >> 24U);
	report[11] = checksum(report, 11U);
	report[12] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_flash_test(void)
{
	uint8_t report[USB_HID_REPORT_SIZE] = {0};
	enum board_flash_test_result result;
	bool restored;

	if (flash_supports_4byte_test()) {
		result = board_flash_test_empty_sector_4byte(EXTERNAL_FLASH_TEST_ADDRESS);
	} else {
		result = BOARD_FLASH_TEST_UNSUPPORTED_DEVICE;
	}
	restored = result != BOARD_FLASH_TEST_UNSUPPORTED_DEVICE &&
		board_flash_sector_is_erased_4byte(EXTERNAL_FLASH_TEST_ADDRESS);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = FLASH_TEST_COMMAND;
	report[3] = 6U;
	report[4] = 0U;
	report[5] = (uint8_t)result;
	report[6] = restored ? 1U : 0U;
	report[7] = (uint8_t)EXTERNAL_FLASH_TEST_ADDRESS;
	report[8] = (uint8_t)(EXTERNAL_FLASH_TEST_ADDRESS >> 8U);
	report[9] = (uint8_t)(EXTERNAL_FLASH_TEST_ADDRESS >> 16U);
	report[10] = (uint8_t)(EXTERNAL_FLASH_TEST_ADDRESS >> 24U);
	report[11] = checksum(report, 11U);
	report[12] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_flash_status(void)
{
	struct board_flash_status status = board_flash_read_status();
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = FLASH_STATUS_COMMAND;
	report[3] = 3U;
	report[4] = 0U;
	report[5] = status.status1;
	report[6] = status.status2;
	report[7] = status.status3;
	report[8] = checksum(report, 8U);
	report[9] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_flash_mode_probe(void)
{
	struct board_flash_mode_probe probe = board_flash_probe_modes();
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = FLASH_MODE_PROBE_COMMAND;
	report[3] = 6U;
	report[4] = 0U;
	report[5] = probe.status1_before;
	report[6] = probe.status1_write_enabled;
	report[7] = probe.status1_write_disabled;
	report[8] = probe.status3_before;
	report[9] = probe.status3_four_byte;
	report[10] = probe.status3_restored;
	report[11] = checksum(report, 11U);
	report[12] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_test_result(uint8_t result)
{
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MOTOR_TEST_COMMAND;
	report[3] = 2U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)board_motor_test_state();
	report[7] = checksum(report, 7U);
	report[8] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_tacho_test_result(uint8_t result)
{
	struct board_motor_tacho_snapshot snapshot = board_motor_tacho_snapshot();
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MOTOR_TACHO_TEST_COMMAND;
	report[3] = 14U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)board_motor_test_state();
	write_i32_le(&report[7], snapshot.total_count);
	write_i32_le(&report[11], snapshot.forward_count);
	write_i32_le(&report[15], snapshot.reverse_count);
	report[19] = checksum(report, 19U);
	report[20] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_stop_test_result(uint8_t result)
{
	struct board_motor_stop_test_snapshot snapshot =
		board_motor_stop_test_snapshot();
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MOTOR_STOP_TEST_COMMAND;
	report[3] = 15U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)snapshot.state;
	report[7] = (uint8_t)snapshot.mode;
	write_i32_le(&report[8], snapshot.total_count);
	write_i32_le(&report[12], snapshot.powered_count);
	write_i32_le(&report[16], snapshot.stopped_count);
	report[20] = checksum(report, 20U);
	report[21] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_dual_test_result(uint8_t result)
{
	struct board_motor_dual_test_snapshot snapshot =
		board_motor_dual_test_snapshot();
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MOTOR_DUAL_TEST_COMMAND;
	report[3] = 18U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)snapshot.state;
	write_i32_le(&report[7], snapshot.a_forward_count);
	write_i32_le(&report[11], snapshot.b_forward_count);
	write_i32_le(&report[15], snapshot.a_reverse_count);
	write_i32_le(&report[19], snapshot.b_reverse_count);
	report[23] = checksum(report, 23U);
	report[24] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_control_result(uint8_t result,
	enum board_motor_port port)
{
	struct board_motor_control_snapshot snapshot = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	(void)board_motor_control_snapshot(port, &snapshot);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MOTOR_CONTROL_COMMAND;
	report[3] = 8U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)port;
	report[7] = (uint8_t)snapshot.state;
	report[8] = (uint8_t)snapshot.power_percent;
	write_i32_le(&report[9], snapshot.tacho_count);
	report[13] = checksum(report, 13U);
	report[14] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_input_sensor_result(uint8_t result, uint8_t port)
{
	est_sensor_status_t status = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	(void)est_sensor_get_status((est_sensor_port_t)port, &status);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = INPUT_SENSOR_COMMAND;
	report[3] = 19U;
	report[4] = 0U;
	report[5] = result;
	report[6] = port;
	report[7] = (uint8_t)status.state;
	report[8] = status.raw_type;
	report[9] = (uint8_t)status.mode;
	report[10] = status.value_valid ? 1U : 0U;
	write_u16_le(&report[11], status.raw_value);
	write_u16_le(&report[13], status.adc0_raw);
	write_u16_le(&report[15], status.adc1_raw);
	report[17] = status.digital_mask;
	write_u32_le(&report[18], status.rx_count);
	write_u16_le(&report[22], status.checksum_errors);
	report[24] = checksum(report, 24U);
	report[25] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_type_result(uint8_t result)
{
	uint8_t report[USB_HID_REPORT_SIZE] = {0};
	uint8_t index;

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MOTOR_TYPE_COMMAND;
	report[3] = 57U;
	report[4] = 0U;
	report[5] = result;
	for (index = 0U; index < BOARD_MOTOR_PORT_COUNT; index++) {
		struct board_motor_control_snapshot motor = {0};
		uint8_t *entry = &report[6U + (index * 14U)];

		(void)board_motor_control_snapshot((enum board_motor_port)index,
			&motor);
		entry[0] = (uint8_t)motor.type;
		write_u16_le(&entry[1], motor.id_adc_raw);
		write_u16_le(&entry[3], motor.id_mv);
		write_u16_le(&entry[5], motor.id_pin6_low_adc_raw);
		write_u16_le(&entry[7], motor.id_pin6_low_mv);
		write_u16_le(&entry[9], motor.id_pin5_pullup_adc_raw);
		write_u16_le(&entry[11], motor.id_pin5_pullup_mv);
		entry[13] = motor.id_pin5_pullup_high;
	}
	report[62] = checksum(report, 62U);
	report[63] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_position_result(uint8_t result,
	enum board_motor_port port)
{
	struct board_motor_position_snapshot snapshot = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	(void)board_motor_position_snapshot_for_port(port, &snapshot);

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MOTOR_POSITION_COMMAND;
	report[3] = 22U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)snapshot.port;
	report[7] = (uint8_t)snapshot.state;
	report[8] = (uint8_t)snapshot.type;
	report[9] = (uint8_t)snapshot.requested_speed_percent;
	report[10] = (uint8_t)snapshot.measured_speed_percent;
	write_i32_le(&report[11], snapshot.start_count);
	write_i32_le(&report[15], snapshot.target_count);
	write_i32_le(&report[19], snapshot.current_count);
	write_i32_le(&report[23], snapshot.target_count - snapshot.current_count);
	report[27] = checksum(report, 27U);
	report[28] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_speed_result(uint8_t result,
	enum board_motor_port port)
{
	struct board_motor_speed_snapshot snapshot = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	(void)board_motor_speed_snapshot_for_port(port, &snapshot);

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MOTOR_SPEED_COMMAND;
	report[3] = 12U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)snapshot.port;
	report[7] = (uint8_t)snapshot.state;
	report[8] = (uint8_t)snapshot.output_state;
	report[9] = (uint8_t)snapshot.type;
	report[10] = (uint8_t)snapshot.requested_speed_percent;
	report[11] = (uint8_t)snapshot.measured_speed_percent;
	report[12] = (uint8_t)snapshot.power_percent;
	write_i32_le(&report[13], snapshot.tacho_count);
	report[17] = checksum(report, 17U);
	report[18] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_pair_position_result(uint8_t result)
{
	est_drive_status_t status = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	(void)est_drive_get_status(&status);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MOTOR_PAIR_POSITION_COMMAND;
	report[3] = 29U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)status.state;
	report[7] = (uint8_t)status.left_port;
	report[8] = (uint8_t)status.right_port;
	write_i32_le(&report[9], status.left_target_degrees);
	write_i32_le(&report[13], status.right_target_degrees);
	write_i32_le(&report[17], status.left_actual_degrees);
	write_i32_le(&report[21], status.right_actual_degrees);
	write_i32_le(&report[25], status.synchronization_error_degrees);
	write_i32_le(&report[29],
		status.maximum_synchronization_error_degrees);
	report[33] = (uint8_t)(int8_t)status.error;
	report[34] = checksum(report, 34U);
	report[35] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_motor_pair_speed_result(uint8_t command, uint8_t result)
{
	est_motor_pair_speed_status_t status = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	(void)est_motor_pair_get_speed_status(&status);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = command;
	report[3] = 27U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)status.state;
	report[7] = (uint8_t)status.left_port;
	report[8] = (uint8_t)status.right_port;
	report[9] = (uint8_t)status.left_requested_speed_percent;
	report[10] = (uint8_t)status.right_requested_speed_percent;
	report[11] = (uint8_t)status.left_measured_speed_percent;
	report[12] = (uint8_t)status.right_measured_speed_percent;
	report[13] = (uint8_t)status.left_power_percent;
	report[14] = (uint8_t)status.right_power_percent;
	write_i32_le(&report[15], status.left_actual_degrees);
	write_i32_le(&report[19], status.right_actual_degrees);
	write_i32_le(&report[23], status.synchronization_error_degrees);
	write_i32_le(&report[27],
		status.maximum_synchronization_error_degrees);
	report[31] = (uint8_t)(int8_t)status.error;
	report[32] = checksum(report, 32U);
	report[33] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_drive_straight_result(uint8_t result)
{
	est_drive_status_t status = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	(void)est_drive_get_status(&status);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = DRIVE_STRAIGHT_COMMAND;
	report[3] = 37U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)status.state;
	report[7] = (uint8_t)status.left_port;
	report[8] = (uint8_t)status.right_port;
	write_i32_le(&report[9], status.target_distance_mm);
	write_i32_le(&report[13], status.actual_distance_mm);
	write_i32_le(&report[17], status.left_target_degrees);
	write_i32_le(&report[21], status.right_target_degrees);
	write_i32_le(&report[25], status.left_actual_degrees);
	write_i32_le(&report[29], status.right_actual_degrees);
	write_i32_le(&report[33], status.synchronization_error_degrees);
	write_i32_le(&report[37],
		status.maximum_synchronization_error_degrees);
	report[41] = (uint8_t)(int8_t)status.error;
	report[42] = checksum(report, 42U);
	report[43] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_drive_run_result(uint8_t result)
{
	est_drive_motion_status_t status = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	(void)est_drive_get_motion_status(&status);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = DRIVE_RUN_COMMAND;
	report[3] = 31U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)status.state;
	report[7] = (uint8_t)status.mode;
	report[8] = (uint8_t)status.left_port;
	report[9] = (uint8_t)status.right_port;
	report[10] = (uint8_t)status.requested_speed_percent;
	write_i32_le(&report[11], status.target_value);
	write_i32_le(&report[15], status.actual_value);
	write_i32_le(&report[19], status.left_actual_degrees);
	write_i32_le(&report[23], status.right_actual_degrees);
	write_i32_le(&report[27], status.synchronization_error_degrees);
	write_i32_le(&report[31],
		status.maximum_synchronization_error_degrees);
	report[35] = (uint8_t)(int8_t)status.error;
	report[36] = checksum(report, 36U);
	report[37] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_drive_steer_for_result(uint8_t result)
{
	est_drive_steer_for_status_t status = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	(void)est_drive_get_steer_for_status(&status);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = DRIVE_STEER_FOR_COMMAND;
	report[3] = 42U;
	report[4] = 0U;
	report[5] = result;
	report[6] = (uint8_t)status.state;
	report[7] = (uint8_t)status.mode;
	report[8] = (uint8_t)status.left_port;
	report[9] = (uint8_t)status.right_port;
	report[10] = (uint8_t)status.steering;
	report[11] = (uint8_t)status.requested_speed_percent;
	report[12] = (uint8_t)status.left_requested_speed_percent;
	report[13] = (uint8_t)status.right_requested_speed_percent;
	write_i32_le(&report[14], status.target_value);
	write_i32_le(&report[18], status.actual_value);
	write_i32_le(&report[22], status.left_target_degrees);
	write_i32_le(&report[26], status.right_target_degrees);
	write_i32_le(&report[30], status.left_actual_degrees);
	write_i32_le(&report[34], status.right_actual_degrees);
	write_i32_le(&report[38], status.synchronization_error_degrees);
	write_i32_le(&report[42],
		status.maximum_synchronization_error_degrees);
	report[46] = (uint8_t)(int8_t)status.error;
	report[47] = checksum(report, 47U);
	report[48] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_device_status(uint32_t now_ms)
{
	est_battery_status_t battery = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};
	uint8_t *payload = &report[5];
	uint8_t index;
	uint32_t capabilities = DEVICE_CAPABILITY_UPDATE |
		DEVICE_CAPABILITY_MOTOR_CONTROL | DEVICE_CAPABILITY_MOTOR_TACHO |
		DEVICE_CAPABILITY_INPUT_SENSOR | DEVICE_CAPABILITY_BATTERY |
		DEVICE_CAPABILITY_KEYS | DEVICE_CAPABILITY_MOTOR_TYPE |
		DEVICE_CAPABILITY_MOTOR_POSITION |
		DEVICE_CAPABILITY_MOTOR_PAIR_POSITION |
		DEVICE_CAPABILITY_MOTOR_PAIR_SPEED |
		DEVICE_CAPABILITY_DRIVE_STRAIGHT |
		DEVICE_CAPABILITY_DRIVE_RUN |
		DEVICE_CAPABILITY_DRIVE_STEER |
		DEVICE_CAPABILITY_DRIVE_STEER_FOR |
		DEVICE_CAPABILITY_MICROPYTHON |
		DEVICE_CAPABILITY_PYTHON_PROGRAM |
		DEVICE_CAPABILITY_PERSISTENT_PROGRAM |
		DEVICE_CAPABILITY_FROZEN_EST_RUNTIME |
		DEVICE_CAPABILITY_UNLIMITED_PYTHON_RUN |
		DEVICE_CAPABILITY_DISPLAY_FONT_STYLES |
		DEVICE_CAPABILITY_ZERO_SPEED_MOTOR_CONTROL |
		DEVICE_CAPABILITY_HOLD_POSITION_CONTROL |
		DEVICE_CAPABILITY_RUNTIME_TEMPERATURE |
		DEVICE_CAPABILITY_COOPERATIVE_MULTITASK |
		DEVICE_CAPABILITY_RUNTIME_BASIC_EVENT_HATS;

	(void)est_battery_get_status(&battery);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = DEVICE_STATUS_COMMAND;
	report[3] = DEVICE_STATUS_PAYLOAD_LENGTH;
	report[4] = 0U;
	payload[0] = DEVICE_PROTOCOL_MAJOR;
	payload[1] = DEVICE_PROTOCOL_MINOR;
	memcpy(&payload[2], app_version_text, 6U);
	payload[8] = BOARD_MOTOR_PORT_COUNT;
	payload[9] = BOARD_SENSOR_PORT_COUNT;
	payload[10] = est_buttons_pressed_mask();
	payload[11] = battery.valid ? battery.level : 0U;
	write_u16_le(&payload[12], battery.adc_raw);
	write_u16_le(&payload[14], battery.sample_mv);
	write_u32_le(&payload[16], capabilities);
	write_u32_le(&payload[20], now_ms);

	for (index = 0U; index < BOARD_MOTOR_PORT_COUNT; index++) {
		struct board_motor_control_snapshot motor = {0};
		uint8_t *entry = &payload[DEVICE_STATUS_MOTOR_OFFSET +
			(index * DEVICE_STATUS_PORT_ENTRY_LENGTH)];

		(void)board_motor_control_snapshot((enum board_motor_port)index,
			&motor);
		entry[0] = (uint8_t)motor.state;
		entry[1] = (uint8_t)motor.power_percent;
		write_i32_le(&entry[2], motor.tacho_count);
	}
	for (index = 0U; index < EST_SENSOR_PORT_COUNT; index++) {
		est_sensor_status_t sensor = {0};
		uint8_t *entry = &payload[DEVICE_STATUS_SENSOR_OFFSET +
			(index * DEVICE_STATUS_PORT_ENTRY_LENGTH)];

		(void)est_sensor_get_status((est_sensor_port_t)index, &sensor);
		entry[0] = (uint8_t)sensor.state;
		entry[1] = sensor.raw_type;
		entry[2] = (uint8_t)sensor.mode;
		entry[3] = sensor.value_valid ? 1U : 0U;
		write_u16_le(&entry[4], sensor.raw_value);
	}
	report[5U + DEVICE_STATUS_PAYLOAD_LENGTH] = checksum(report,
		5U + DEVICE_STATUS_PAYLOAD_LENGTH);
	report[6U + DEVICE_STATUS_PAYLOAD_LENGTH] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_micropython_status(void)
{
	est_micropython_status_t status = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};
	uint8_t *payload = &report[5];

	(void)est_micropython_get_status(&status);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = MICROPYTHON_STATUS_COMMAND;
	report[3] = MICROPYTHON_STATUS_PAYLOAD_LENGTH;
	report[4] = 0U;
	payload[0] = 1U;
	payload[1] = (uint8_t)status.state;
	payload[2] = status.flags;
	payload[3] = 0U;
	write_u32_le(&payload[4], status.heap_total_bytes);
	write_u32_le(&payload[8], status.heap_used_bytes);
	write_u32_le(&payload[12], status.heap_free_bytes);
	write_u32_le(&payload[16], status.startup_duration_ms);
	write_u32_le(&payload[20], status.maximum_gc_pause_us);
	write_u16_le(&payload[24], status.gc_count);
	write_u16_le(&payload[26], status.self_test_value);
	report[5U + MICROPYTHON_STATUS_PAYLOAD_LENGTH] = checksum(report,
		5U + MICROPYTHON_STATUS_PAYLOAD_LENGTH);
	report[6U + MICROPYTHON_STATUS_PAYLOAD_LENGTH] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void queue_python_program_status(uint8_t result)
{
	est_micropython_program_status_t status = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};
	uint8_t *payload = &report[5];

	(void)est_micropython_program_get_status(&status);
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = PYTHON_PROGRAM_COMMAND;
	report[3] = PYTHON_PROGRAM_STATUS_PAYLOAD_LENGTH;
	report[4] = 0U;
	payload[0] = 1U;
	payload[1] = result;
	payload[2] = (uint8_t)status.state;
	payload[3] = (uint8_t)status.error;
	payload[4] = status.flags;
	payload[5] = 0U;
	write_u16_le(&payload[6], status.expected_length);
	write_u16_le(&payload[8], status.received_length);
	write_u16_le(&payload[10], status.run_count);
	write_u32_le(&payload[12], status.expected_crc32);
	write_u32_le(&payload[16], status.actual_crc32);
	write_u32_le(&payload[20], status.duration_ms);
	write_u32_le(&payload[24], status.timeout_ms);
	write_i32_le(&payload[28], status.result_value);
	report[5U + PYTHON_PROGRAM_STATUS_PAYLOAD_LENGTH] = checksum(report,
		5U + PYTHON_PROGRAM_STATUS_PAYLOAD_LENGTH);
	report[6U + PYTHON_PROGRAM_STATUS_PAYLOAD_LENGTH] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void handle_python_program(const uint8_t *data, uint16_t data_length)
{
	est_result_t operation_result = EST_OK;
	uint8_t result = 1U;

	if (data[0] == PYTHON_PROGRAM_ACTION_STATUS && data_length == 1U) {
		/* Status is available during upload and execution. */
	} else if (data[0] == PYTHON_PROGRAM_ACTION_BEGIN &&
		   data_length == 7U) {
		operation_result = est_micropython_program_begin(
			read_u16_le(&data[1]), read_u32_le(&data[3]));
	} else if (data[0] == PYTHON_PROGRAM_ACTION_CHUNK &&
		   data_length >= 4U) {
		operation_result = est_micropython_program_write(
			read_u16_le(&data[1]), &data[3],
			(uint16_t)(data_length - 3U));
	} else if (data[0] == PYTHON_PROGRAM_ACTION_RUN &&
		   data_length == 5U) {
		operation_result = est_micropython_program_run(
			read_u32_le(&data[1]));
	} else if (data[0] == PYTHON_PROGRAM_ACTION_STOP &&
		   data_length == 1U) {
		operation_result = est_micropython_program_stop();
	} else if (data[0] == PYTHON_PROGRAM_ACTION_CLEAR &&
		   data_length == 1U) {
		operation_result = est_micropython_program_clear();
	} else {
		operation_result = EST_ERR_INVALID_ARGUMENT;
	}
	if (operation_result == EST_ERR_BUSY) {
		result = 2U;
	} else if (operation_result != EST_OK) {
		result = 0U;
	}
	queue_python_program_status(result);
}

static void queue_persistent_program_status(uint8_t result,
	uint8_t program_slot_id, est_program_store_error_t response_error)
{
	est_program_store_status_t status = {0};
	uint8_t report[USB_HID_REPORT_SIZE] = {0};
	uint8_t *payload = &report[5];
	uint32_t program_region_start = 0U;

	if (!est_program_store_get_slot_status(program_slot_id, &status)) {
		status.program_slot_id = program_slot_id;
		status.active_bank = 0xFFU;
		result = 0U;
		if (response_error == EST_PROGRAM_STORE_ERROR_NONE) {
			response_error = EST_PROGRAM_STORE_ERROR_INVALID_SLOT;
		}
	} else {
		program_region_start = EST_PROGRAM_STORE_FLASH_SIZE -
			(((uint32_t)program_slot_id + 1U) *
			 EST_PROGRAM_STORE_PROGRAM_SLOT_SIZE);
	}
	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = PERSISTENT_PROGRAM_COMMAND;
	report[3] = PERSISTENT_PROGRAM_STATUS_PAYLOAD_LENGTH;
	report[4] = 0U;
	payload[0] = PERSISTENT_PROGRAM_STATUS_SCHEMA_VERSION;
	payload[1] = result;
	payload[2] = (uint8_t)status.state;
	payload[3] = status.flags;
	payload[4] = status.program_slot_id;
	payload[5] = EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT;
	payload[6] = status.active_bank;
	payload[7] = (uint8_t)status.record_type;
	write_u32_le(&payload[8], status.generation);
	write_u16_le(&payload[12], status.source_length);
	payload[14] = status.name_length;
	payload[15] = response_error != EST_PROGRAM_STORE_ERROR_NONE ?
		(uint8_t)response_error : (uint8_t)status.last_error;
	write_u32_le(&payload[16], status.source_crc32);
	write_u32_le(&payload[20], program_region_start);
	write_u32_le(&payload[24], EST_PROGRAM_STORE_PROGRAM_SLOT_SIZE);
	write_u32_le(&payload[28], EST_PROGRAM_STORE_BANK_SIZE);
	write_u32_le(&payload[32], EST_PROGRAM_STORE_FLASH_SIZE);
	payload[36] = status.identity.manufacturer;
	payload[37] = status.identity.memory_type;
	payload[38] = status.identity.capacity;
	payload[39] = status.erased_sector_mask;
	payload[40] = status.occupied_sector_mask;
	memcpy(&payload[41], status.name, EST_PROGRAM_STORE_NAME_MAX_BYTES);
	payload[72] = EST_PROGRAM_STORE_BANK_COUNT;
	payload[73] = EST_PROGRAM_STORE_SECTORS_PER_BANK;
	report[5U + PERSISTENT_PROGRAM_STATUS_PAYLOAD_LENGTH] = checksum(report,
		5U + PERSISTENT_PROGRAM_STATUS_PAYLOAD_LENGTH);
	report[6U + PERSISTENT_PROGRAM_STATUS_PAYLOAD_LENGTH] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
}

static void handle_persistent_program(const uint8_t *data,
	uint16_t data_length)
{
	est_result_t operation_result = EST_OK;
	est_program_store_error_t response_error = EST_PROGRAM_STORE_ERROR_NONE;
	uint8_t program_slot_id = 0U;
	uint8_t result = 1U;
	const uint8_t *name = NULL;
	uint8_t name_length = 0U;

	if (data_length == 0U) {
		operation_result = EST_ERR_INVALID_ARGUMENT;
	} else if (data_length >= 2U) {
		program_slot_id = data[1];
	}
	if (operation_result == EST_OK &&
	    program_slot_id >= EST_PROGRAM_STORE_PROGRAM_SLOT_COUNT) {
		operation_result = EST_ERR_INVALID_ARGUMENT;
		response_error = EST_PROGRAM_STORE_ERROR_INVALID_SLOT;
	} else if (data[0] == PERSISTENT_PROGRAM_ACTION_STATUS) {
		if (data_length != 1U && data_length != 2U) {
			operation_result = EST_ERR_INVALID_ARGUMENT;
		}
	} else if (data[0] == PERSISTENT_PROGRAM_ACTION_SAVE) {
		if (data_length == 1U) {
			static const uint8_t legacy_name[] = "Program 0";

			name = legacy_name;
			name_length = sizeof(legacy_name) - 1U;
		} else if (data_length >= 4U && data[2] > 0U &&
			   data[2] <= EST_PROGRAM_STORE_NAME_MAX_BYTES &&
			   data_length == (uint16_t)(3U + data[2])) {
			name_length = data[2];
			name = &data[3];
		} else {
			operation_result = EST_ERR_INVALID_ARGUMENT;
			response_error = EST_PROGRAM_STORE_ERROR_INVALID_NAME;
		}
		if (operation_result == EST_OK) {
			operation_result = est_program_store_save(program_slot_id,
				name, name_length);
			if (operation_result == EST_ERR_INVALID_ARGUMENT) {
				response_error = EST_PROGRAM_STORE_ERROR_INVALID_NAME;
			}
		}
	} else if (data[0] == PERSISTENT_PROGRAM_ACTION_LOAD) {
		if (data_length != 1U && data_length != 2U) {
			operation_result = EST_ERR_INVALID_ARGUMENT;
		} else {
			operation_result = est_program_store_load(program_slot_id);
		}
	} else if (data[0] == PERSISTENT_PROGRAM_ACTION_CLEAR) {
		if (data_length != 1U && data_length != 2U) {
			operation_result = EST_ERR_INVALID_ARGUMENT;
		} else {
			operation_result = est_program_store_clear(program_slot_id);
		}
	} else {
		operation_result = EST_ERR_INVALID_ARGUMENT;
	}
	if (operation_result == EST_ERR_BUSY) {
		result = 2U;
	} else if (operation_result != EST_OK) {
		result = 0U;
	}
	queue_persistent_program_status(result, program_slot_id, response_error);
}

static uint8_t apply_motor_test_action(uint8_t action, uint32_t now_ms,
	uint8_t power_percent, bool custom_power)
{
	uint8_t result = 1U;

	if (action == MOTOR_TEST_ACTION_START) {
		bool started = custom_power ?
			board_motor_start_test_with_power(now_ms, power_percent) :
			board_motor_start_test(now_ms);
		if (!started) {
			result = 2U;
		}
	} else if (action == MOTOR_TEST_ACTION_STOP) {
		board_motor_stop();
	} else if (action != MOTOR_TEST_ACTION_STATUS) {
		result = 0U;
	}
	return result;

}

static void handle_motor_test(uint8_t action, uint32_t now_ms)
{
	queue_motor_test_result(apply_motor_test_action(action, now_ms, 0U, false));
}

static void handle_motor_tacho_test(const uint8_t *data, uint16_t data_length,
	uint32_t now_ms)
{
	uint8_t action = data[0];
	bool custom_power = data_length >= 2U;
	uint8_t power_percent = custom_power ? data[1] : 0U;
	enum board_motor_port port = data_length == 3U ?
		(enum board_motor_port)data[2] : BOARD_MOTOR_PORT_A;
	uint8_t result;

	if (custom_power && action != MOTOR_TEST_ACTION_START) {
		result = 0U;
	} else if (custom_power) {
		result = board_motor_start_port_test_with_power(now_ms, port,
			power_percent) ? 1U : 2U;
	} else {
		result = apply_motor_test_action(action, now_ms, 0U, false);
	}
	queue_motor_tacho_test_result(result);
}

static void handle_motor_stop_test(const uint8_t *data, uint16_t data_length,
	uint32_t now_ms)
{
	uint8_t action = data[0];
	uint8_t result = 1U;

	if (action == MOTOR_TEST_ACTION_START &&
	    (data_length == 3U || data_length == 4U)) {
		enum board_motor_port port = data_length == 4U ?
			(enum board_motor_port)data[3] : BOARD_MOTOR_PORT_A;

		if (!board_motor_start_port_stop_test(now_ms, port,
		    (enum board_motor_stop_mode)data[1], data[2])) {
			result = 2U;
		}
	} else if (action == MOTOR_TEST_ACTION_STOP && data_length == 1U) {
		board_motor_stop();
	} else if (action != MOTOR_TEST_ACTION_STATUS || data_length != 1U) {
		result = 0U;
	}
	queue_motor_stop_test_result(result);
}

static void handle_motor_dual_test(const uint8_t *data, uint16_t data_length,
	uint32_t now_ms)
{
	uint8_t action = data[0];
	uint8_t result = 1U;

	if (action == MOTOR_TEST_ACTION_START && data_length == 2U) {
		if (!board_motor_start_dual_test(now_ms, data[1])) {
			result = 2U;
		}
	} else if (action == MOTOR_TEST_ACTION_STOP && data_length == 1U) {
		board_motor_stop();
	} else if (action != MOTOR_TEST_ACTION_STATUS || data_length != 1U) {
		result = 0U;
	}
	queue_motor_dual_test_result(result);
}

static void handle_motor_control(const uint8_t *data, uint16_t data_length)
{
	uint8_t action = data[0];
	enum board_motor_port port = (enum board_motor_port)data[1];
	uint8_t result = 1U;

	if (data[1] >= BOARD_MOTOR_PORT_COUNT) {
		result = 0U;
	} else if (action == MOTOR_CONTROL_ACTION_STATUS && data_length == 2U) {
		/* Status remains readable while one of the older diagnostics runs. */
	} else if (action == MOTOR_CONTROL_ACTION_SET_POWER && data_length == 3U) {
		int8_t power_percent = (int8_t)data[2];

		if (power_percent < -100 || power_percent > 100) {
			result = 0U;
		} else if (est_motor_set_power((est_motor_port_t)port,
		    power_percent) != EST_OK) {
			result = 2U;
		}
	} else if (action == MOTOR_CONTROL_ACTION_COAST && data_length == 2U) {
		if (est_motor_stop((est_motor_port_t)port,
		    EST_STOP_COAST) != EST_OK) {
			result = 2U;
		}
	} else if (action == MOTOR_CONTROL_ACTION_BRAKE && data_length == 2U) {
		if (est_motor_stop((est_motor_port_t)port,
		    EST_STOP_BRAKE) != EST_OK) {
			result = 2U;
		}
	} else if (action == MOTOR_CONTROL_ACTION_RESET_TACHO &&
		   data_length == 2U) {
		if (est_motor_reset_angle((est_motor_port_t)port) != EST_OK) {
			result = 2U;
		}
	} else {
		result = 0U;
	}
	queue_motor_control_result(result, port);
}

static void handle_motor_position(const uint8_t *data, uint16_t data_length)
{
	uint8_t action = data[0];
	enum board_motor_port port = (enum board_motor_port)data[1];
	uint8_t result = 1U;

	if (data[1] >= BOARD_MOTOR_PORT_COUNT) {
		result = 0U;
	} else if (action == MOTOR_POSITION_ACTION_STATUS && data_length == 2U) {
		/* Status is always available, including after completion or timeout. */
	} else if (action == MOTOR_POSITION_ACTION_START && data_length == 7U) {
		if (est_motor_run_angle((est_motor_port_t)port,
		    read_i32_le(&data[3]), data[2], EST_STOP_COAST) != EST_OK) {
			result = 2U;
		}
	} else if (action == MOTOR_POSITION_ACTION_STOP && data_length == 2U) {
		if (est_motor_stop((est_motor_port_t)port,
		    EST_STOP_COAST) != EST_OK) {
			result = 2U;
		}
	} else {
		result = 0U;
	}
	queue_motor_position_result(result, port);
}

static void handle_motor_type(const uint8_t *data, uint16_t data_length,
	uint32_t now_ms)
{
	uint8_t result = 1U;

	if (data_length == 0U) {
		/* Preserve the V1.1 read-only all-port query. */
	} else if (data_length == 2U && data[0] == MOTOR_TYPE_ACTION_REFRESH &&
		   data[1] < BOARD_MOTOR_PORT_COUNT) {
		if (!board_motor_refresh_identification(now_ms,
		    (enum board_motor_port)data[1])) {
			result = 2U;
		}
	} else {
		result = 0U;
	}
	queue_motor_type_result(result);
}

static void handle_motor_speed(const uint8_t *data, uint16_t data_length)
{
	uint8_t action = data[0];
	enum board_motor_port port = (enum board_motor_port)data[1];
	uint8_t result = 1U;

	if (data[1] >= BOARD_MOTOR_PORT_COUNT) {
		result = 0U;
	} else if (action == MOTOR_SPEED_ACTION_STATUS && data_length == 2U) {
		/* Status remains available while closed-loop speed control runs. */
	} else if (action == MOTOR_SPEED_ACTION_START && data_length == 3U) {
		int8_t speed_percent = (int8_t)data[2];

		if (est_motor_run_speed((est_motor_port_t)port,
		    speed_percent) != EST_OK) {
			result = 2U;
		}
	} else if (action == MOTOR_SPEED_ACTION_COAST && data_length == 2U) {
		if (est_motor_stop((est_motor_port_t)port,
		    EST_STOP_COAST) != EST_OK) {
			result = 2U;
		}
	} else if (action == MOTOR_SPEED_ACTION_BRAKE && data_length == 2U) {
		if (est_motor_stop((est_motor_port_t)port,
		    EST_STOP_BRAKE) != EST_OK) {
			result = 2U;
		}
	} else {
		result = 0U;
	}
	queue_motor_speed_result(result, port);
}

static void handle_motor_pair_position(const uint8_t *data,
	uint16_t data_length)
{
	uint8_t action = data[0];
	uint8_t result = 1U;
	est_result_t operation_result = EST_OK;

	if (action == MOTOR_PAIR_POSITION_ACTION_STATUS && data_length == 1U) {
		/* The latest pair status remains readable after completion or fault. */
	} else if (action == MOTOR_PAIR_POSITION_ACTION_START &&
		   data_length == 12U) {
		operation_result = est_motor_pair_run_angles(
			(est_motor_port_t)data[1], read_i32_le(&data[4]),
			(est_motor_port_t)data[2], read_i32_le(&data[8]), data[3],
			EST_STOP_COAST);
	} else if (action == MOTOR_PAIR_POSITION_ACTION_STOP &&
		   data_length == 1U) {
		operation_result = est_drive_stop(EST_STOP_COAST);
	} else {
		result = 0U;
	}
	if (operation_result != EST_OK) {
		result = operation_result == EST_ERR_INVALID_ARGUMENT ||
			operation_result == EST_ERR_INVALID_PORT ||
			operation_result == EST_ERR_NOT_SUPPORTED ? 0U : 2U;
	}
	queue_motor_pair_position_result(result);
}

static void handle_motor_pair_speed(const uint8_t *data,
	uint16_t data_length)
{
	uint8_t action = data[0];
	uint8_t result = 1U;
	est_result_t operation_result = EST_OK;

	if (action == MOTOR_PAIR_SPEED_ACTION_STATUS && data_length == 1U) {
		/* Status remains readable after the explicit stop. */
	} else if (action == MOTOR_PAIR_SPEED_ACTION_START &&
		   data_length == 5U) {
		operation_result = est_motor_pair_run_speeds(
			(est_motor_port_t)data[1], (int8_t)data[3],
			(est_motor_port_t)data[2], (int8_t)data[4]);
	} else if (action == MOTOR_PAIR_SPEED_ACTION_COAST &&
		   data_length == 1U) {
		operation_result = est_motor_pair_stop(EST_STOP_COAST);
	} else if (action == MOTOR_PAIR_SPEED_ACTION_BRAKE &&
		   data_length == 1U) {
		operation_result = est_motor_pair_stop(EST_STOP_BRAKE);
	} else {
		result = 0U;
	}
	if (operation_result != EST_OK) {
		result = operation_result == EST_ERR_INVALID_ARGUMENT ||
			operation_result == EST_ERR_INVALID_PORT ||
			operation_result == EST_ERR_NOT_SUPPORTED ? 0U : 2U;
	}
	queue_motor_pair_speed_result(MOTOR_PAIR_SPEED_COMMAND, result);
}

static void handle_drive_steer(const uint8_t *data, uint16_t data_length)
{
	uint8_t action = data[0];
	uint8_t result = 1U;
	est_result_t operation_result = EST_OK;

	if (action == DRIVE_STEER_ACTION_STATUS && data_length == 1U) {
		/* Effective wheel speeds remain readable after an explicit stop. */
	} else if (action == DRIVE_STEER_ACTION_START && data_length == 5U) {
		operation_result = est_drive_start_steer(
			(est_motor_port_t)data[1], (est_motor_port_t)data[2],
			(int8_t)data[3], (int8_t)data[4]);
	} else if (action == DRIVE_STEER_ACTION_COAST &&
		   data_length == 1U) {
		operation_result = est_motor_pair_stop(EST_STOP_COAST);
	} else if (action == DRIVE_STEER_ACTION_BRAKE &&
		   data_length == 1U) {
		operation_result = est_motor_pair_stop(EST_STOP_BRAKE);
	} else {
		result = 0U;
	}
	if (operation_result != EST_OK) {
		result = operation_result == EST_ERR_INVALID_ARGUMENT ||
			operation_result == EST_ERR_INVALID_PORT ||
			operation_result == EST_ERR_NOT_SUPPORTED ? 0U : 2U;
	}
	queue_motor_pair_speed_result(DRIVE_STEER_COMMAND, result);
}

static void handle_drive_straight(const uint8_t *data,
	uint16_t data_length)
{
	uint8_t action = data[0];
	uint8_t result = 1U;
	est_result_t operation_result = EST_OK;

	if (action == DRIVE_STRAIGHT_ACTION_STATUS && data_length == 1U) {
		/* The latest distance and wheel progress remain readable. */
	} else if (action == DRIVE_STRAIGHT_ACTION_START &&
		   data_length == 13U) {
		est_drive_config_t config = {
			.left_port = (est_motor_port_t)data[1],
			.right_port = (est_motor_port_t)data[2],
			.wheel_diameter_mm = read_u16_le(&data[3]),
			.axle_track_mm = read_u16_le(&data[5])
		};

		operation_result = est_drive_config(&config);
		if (operation_result == EST_OK) {
			operation_result = est_drive_straight(
				read_i32_le(&data[7]), data[11],
				(est_stop_mode_t)data[12]);
		}
	} else if (action == DRIVE_STRAIGHT_ACTION_STOP &&
		   data_length == 1U) {
		operation_result = est_drive_stop(EST_STOP_COAST);
	} else {
		result = 0U;
	}
	if (operation_result != EST_OK) {
		result = operation_result == EST_ERR_INVALID_ARGUMENT ||
			operation_result == EST_ERR_INVALID_PORT ||
			operation_result == EST_ERR_NOT_SUPPORTED ? 0U : 2U;
	}
	queue_drive_straight_result(result);
}

static void handle_drive_run(const uint8_t *data, uint16_t data_length)
{
	uint8_t action = data[0];
	uint8_t result = 1U;
	est_result_t operation_result = EST_OK;

	if (action == DRIVE_RUN_ACTION_STATUS && data_length == 1U) {
		/* The latest angle or timed-run state remains readable. */
	} else if (action == DRIVE_RUN_ACTION_START && data_length == 10U) {
		if (data[3] == DRIVE_RUN_MODE_DEGREES) {
			operation_result = est_drive_run_degrees(
				(est_motor_port_t)data[1],
				(est_motor_port_t)data[2], read_i32_le(&data[4]),
				data[8], (est_stop_mode_t)data[9]);
		} else if (data[3] == DRIVE_RUN_MODE_TIME_MS) {
			operation_result = est_drive_run_time(
				(est_motor_port_t)data[1],
				(est_motor_port_t)data[2], read_i32_le(&data[4]),
				data[8], (est_stop_mode_t)data[9]);
		} else {
			operation_result = EST_ERR_INVALID_ARGUMENT;
		}
	} else if (action == DRIVE_RUN_ACTION_STOP && data_length == 1U) {
		operation_result = est_drive_stop(EST_STOP_COAST);
	} else {
		result = 0U;
	}
	if (operation_result != EST_OK) {
		result = operation_result == EST_ERR_INVALID_ARGUMENT ||
			operation_result == EST_ERR_INVALID_PORT ||
			operation_result == EST_ERR_NOT_SUPPORTED ? 0U : 2U;
	}
	queue_drive_run_result(result);
}

static void handle_drive_steer_for(const uint8_t *data,
	uint16_t data_length)
{
	uint8_t action = data[0];
	uint8_t result = 1U;
	est_result_t operation_result = EST_OK;

	if (action == DRIVE_STEER_FOR_ACTION_STATUS && data_length == 1U) {
		/* The latest finite steering progress remains readable. */
	} else if (action == DRIVE_STEER_FOR_ACTION_START &&
		   data_length == 11U) {
		if (data[3] == DRIVE_STEER_FOR_MODE_DEGREES ||
		    data[3] == DRIVE_STEER_FOR_MODE_TIME_MS) {
			operation_result = est_drive_steer_for(
				(est_motor_port_t)data[1],
				(est_motor_port_t)data[2],
				(est_drive_target_mode_t)data[3],
				(int8_t)data[4], (int8_t)data[5],
				read_i32_le(&data[6]),
				(est_stop_mode_t)data[10]);
		} else {
			operation_result = EST_ERR_INVALID_ARGUMENT;
		}
	} else if (action == DRIVE_STEER_FOR_ACTION_STOP &&
		   data_length == 1U) {
		operation_result = est_drive_stop(EST_STOP_COAST);
	} else {
		result = 0U;
	}
	if (operation_result != EST_OK) {
		result = operation_result == EST_ERR_INVALID_ARGUMENT ||
			operation_result == EST_ERR_INVALID_PORT ||
			operation_result == EST_ERR_NOT_SUPPORTED ? 0U : 2U;
	}
	queue_drive_steer_for_result(result);
}

static void handle_input_sensor(const uint8_t *data, uint16_t data_length)
{
	uint8_t action = data[0];
	uint8_t port = data[1];
	uint8_t result = 1U;

	if (port >= EST_SENSOR_PORT_COUNT) {
		result = 0U;
	} else if (action == INPUT_SENSOR_ACTION_STATUS && data_length == 2U) {
		/* Status is always available, including during synchronisation. */
	} else if (action == INPUT_SENSOR_ACTION_SET_MODE && data_length == 3U) {
		if (data[2] > EST_SENSOR_MODE_COLOR) {
			result = 0U;
		} else if (est_sensor_set_mode((est_sensor_port_t)port,
			   (est_sensor_mode_t)data[2]) != EST_OK) {
			result = 2U;
		}
	} else if (action == INPUT_SENSOR_ACTION_RESTART && data_length == 2U) {
		if (est_sensor_restart((est_sensor_port_t)port) != EST_OK) {
			result = 0U;
		}
	} else {
		result = 0U;
	}
	queue_input_sensor_result(result, port);
}

static void abort_session(void)
{
	update_storage_abort();
	memset(&session, 0, sizeof(session));
}

static void handle_update_frame(const uint8_t *frame, uint16_t data_length,
	uint32_t now_ms)
{
	uint16_t total_frames;
	uint16_t frame_index;
	uint16_t payload_length;
	const uint8_t *payload;
	bool is_last;

	if (data_length < 4U) {
		return;
	}
	(void)est_system_emergency_stop();
	total_frames = read_u16_le(&frame[5]);
	frame_index = read_u16_le(&frame[7]);
	payload_length = data_length - 4U;
	payload = &frame[9];

	if (total_frames == 0U || frame_index >= total_frames ||
	    payload_length > UPDATE_FRAME_MAX_PAYLOAD || payload_length == 0U) {
		queue_update_ack(total_frames, frame_index, UPDATE_ACK_FAILURE, false);
		return;
	}

	if (!session.active) {
		if (frame_index != 0U || payload_length < 4U ||
		    memcmp(payload, "APP=", 4U) != 0) {
			queue_update_ack(total_frames, frame_index, UPDATE_ACK_FAILURE, false);
			return;
		}
		if (!update_storage_begin()) {
			queue_update_ack(total_frames, frame_index, UPDATE_ACK_FAILURE, false);
			return;
		}
		session.active = true;
		session.total_frames = total_frames;
		session.next_frame = 0U;
		session.received_bytes = 0U;
	}

	if (total_frames != session.total_frames) {
		queue_update_ack(total_frames, frame_index, UPDATE_ACK_FAILURE, false);
		return;
	}
	if (session.next_frame > 0U && frame_index == (session.next_frame - 1U)) {
		session.last_activity_ms = now_ms;
		queue_update_ack(total_frames, frame_index, UPDATE_ACK_SUCCESS, false);
		return;
	}
	if (frame_index != session.next_frame ||
	    session.received_bytes >= UPDATE_MAX_PACKAGE_SIZE ||
	    payload_length >= (UPDATE_MAX_PACKAGE_SIZE - session.received_bytes)) {
		queue_update_ack(total_frames, frame_index, UPDATE_ACK_FAILURE, false);
		return;
	}
	if (!update_storage_write(session.received_bytes, payload, payload_length)) {
		abort_session();
		queue_update_ack(total_frames, frame_index, UPDATE_ACK_FAILURE, false);
		return;
	}

	session.received_bytes += payload_length;
	session.next_frame++;
	session.last_activity_ms = now_ms;
	is_last = session.next_frame == session.total_frames;
	if (!is_last) {
		queue_update_ack(total_frames, frame_index, UPDATE_ACK_SUCCESS, false);
		return;
	}

	if (!update_storage_validate_image(session.received_bytes) ||
	    !update_storage_commit(session.received_bytes)) {
		abort_session();
		queue_update_ack(total_frames, frame_index, UPDATE_ACK_FAILURE, false);
		return;
	}
	memset(&session, 0, sizeof(session));
	committed = true;
	power_off_deadline_ms = now_ms + FINAL_ACK_POWER_OFF_TIMEOUT_MS;
	queue_update_ack(total_frames, frame_index, UPDATE_ACK_SUCCESS, true);
}

static void handle_logical_frame(uint32_t now_ms)
{
	uint16_t data_length = read_u16_le(&logical_frame[3]);
	size_t checksum_index = (size_t)data_length + 5U;

	if (logical_frame[0] != FRAME_START_BYTE ||
	    logical_frame[1] != HOST_FRAME_DIRECTION ||
	    logical_frame[checksum_index + 1U] != FRAME_END_BYTE ||
	    checksum(logical_frame, checksum_index) != logical_frame[checksum_index]) {
		if (logical_frame[2] == UPDATE_COMMAND && data_length >= 4U) {
			queue_update_ack(read_u16_le(&logical_frame[5]),
				read_u16_le(&logical_frame[7]), UPDATE_ACK_FAILURE, false);
		}
		return;
	}
	if (est_micropython_program_is_executing() &&
	    logical_frame[2] != PYTHON_PROGRAM_COMMAND) {
		return;
	}

	if (logical_frame[2] == HEARTBEAT_COMMAND && data_length == 0U) {
		queue_heartbeat();
	} else if (logical_frame[2] == KEY_STATUS_COMMAND && data_length == 0U) {
		queue_key_status();
	} else if (logical_frame[2] == FLASH_ID_COMMAND && data_length == 0U) {
		queue_flash_identity();
	} else if (logical_frame[2] == FLASH_SCAN_COMMAND && data_length == 0U) {
		queue_flash_scan();
	} else if (logical_frame[2] == FLASH_TEST_COMMAND && data_length == 0U) {
		queue_flash_test();
	} else if (logical_frame[2] == FLASH_STATUS_COMMAND && data_length == 0U) {
		queue_flash_status();
	} else if (logical_frame[2] == FLASH_MODE_PROBE_COMMAND && data_length == 0U) {
		queue_flash_mode_probe();
	} else if (logical_frame[2] == MOTOR_TEST_COMMAND && data_length == 1U) {
		handle_motor_test(logical_frame[5], now_ms);
	} else if (logical_frame[2] == MOTOR_TACHO_TEST_COMMAND &&
		   (data_length == 1U || data_length == 2U || data_length == 3U)) {
		handle_motor_tacho_test(&logical_frame[5], data_length, now_ms);
	} else if (logical_frame[2] == MOTOR_STOP_TEST_COMMAND &&
		   (data_length == 1U || data_length == 3U || data_length == 4U)) {
		handle_motor_stop_test(&logical_frame[5], data_length, now_ms);
	} else if (logical_frame[2] == MOTOR_DUAL_TEST_COMMAND &&
		   (data_length == 1U || data_length == 2U)) {
		handle_motor_dual_test(&logical_frame[5], data_length, now_ms);
	} else if (logical_frame[2] == MOTOR_CONTROL_COMMAND &&
		   (data_length == 2U || data_length == 3U)) {
		handle_motor_control(&logical_frame[5], data_length);
	} else if (logical_frame[2] == INPUT_SENSOR_COMMAND &&
		   (data_length == 2U || data_length == 3U)) {
		handle_input_sensor(&logical_frame[5], data_length);
	} else if (logical_frame[2] == DEVICE_STATUS_COMMAND && data_length == 0U) {
		queue_device_status(now_ms);
	} else if (logical_frame[2] == MICROPYTHON_STATUS_COMMAND &&
		   data_length == 0U) {
		queue_micropython_status();
	} else if (logical_frame[2] == PYTHON_PROGRAM_COMMAND &&
		   data_length >= 1U && data_length <= 1010U) {
		handle_python_program(&logical_frame[5], data_length);
	} else if (logical_frame[2] == PERSISTENT_PROGRAM_COMMAND &&
		   data_length >= 1U && data_length <= 34U) {
		handle_persistent_program(&logical_frame[5], data_length);
	} else if (logical_frame[2] == MOTOR_TYPE_COMMAND &&
		   (data_length == 0U || data_length == 2U)) {
		handle_motor_type(&logical_frame[5], data_length, now_ms);
	} else if (logical_frame[2] == MOTOR_POSITION_COMMAND &&
		   (data_length == 2U || data_length == 7U)) {
		handle_motor_position(&logical_frame[5], data_length);
	} else if (logical_frame[2] == MOTOR_SPEED_COMMAND &&
		   (data_length == 2U || data_length == 3U)) {
		handle_motor_speed(&logical_frame[5], data_length);
	} else if (logical_frame[2] == MOTOR_PAIR_POSITION_COMMAND &&
		   (data_length == 1U || data_length == 12U)) {
		handle_motor_pair_position(&logical_frame[5], data_length);
	} else if (logical_frame[2] == MOTOR_PAIR_SPEED_COMMAND &&
		   (data_length == 1U || data_length == 5U)) {
		handle_motor_pair_speed(&logical_frame[5], data_length);
	} else if (logical_frame[2] == DRIVE_STRAIGHT_COMMAND &&
		   (data_length == 1U || data_length == 13U)) {
		handle_drive_straight(&logical_frame[5], data_length);
	} else if (logical_frame[2] == DRIVE_RUN_COMMAND &&
		   (data_length == 1U || data_length == 10U)) {
		handle_drive_run(&logical_frame[5], data_length);
	} else if (logical_frame[2] == DRIVE_STEER_COMMAND &&
		   (data_length == 1U || data_length == 5U)) {
		handle_drive_steer(&logical_frame[5], data_length);
	} else if (logical_frame[2] == DRIVE_STEER_FOR_COMMAND &&
		   (data_length == 1U || data_length == 11U)) {
		handle_drive_steer_for(&logical_frame[5], data_length);
	} else if (logical_frame[2] == UPDATE_COMMAND) {
		handle_update_frame(logical_frame, data_length, now_ms);
	}
}

void update_protocol_init(void)
{
	memset(&session, 0, sizeof(session));
	logical_frame_length = 0U;
	expected_frame_length = 0U;
	logical_frame_last_report_ms = 0U;
	committed = false;
}

void update_protocol_feed_report(const uint8_t *report, size_t length,
	uint32_t now_ms)
{
	size_t index;

	if (report == NULL || length == 0U || committed) {
		return;
	}
	if (logical_frame_length != 0U &&
	    (uint32_t)(now_ms - logical_frame_last_report_ms) >=
		LOGICAL_FRAME_RESTART_GAP_MS &&
	    report_starts_logical_frame(report, length)) {
		logical_frame_length = 0U;
		expected_frame_length = 0U;
	}
	for (index = 0U; index < length; index++) {
		if (logical_frame_length == 0U) {
			if (report[index] != FRAME_START_BYTE) {
				continue;
			}
			logical_frame_last_report_ms = now_ms;
		}
		if (logical_frame_length >= sizeof(logical_frame)) {
			logical_frame_length = 0U;
			expected_frame_length = 0U;
			return;
		}
		logical_frame[logical_frame_length++] = report[index];
		if (logical_frame_length == 5U) {
			uint16_t data_length = read_u16_le(&logical_frame[3]);
			if (data_length > UPDATE_FRAME_MAX_DATA_LENGTH) {
				logical_frame_length = 0U;
				expected_frame_length = 0U;
				return;
			}
			expected_frame_length = (size_t)data_length + 7U;
		}
		if (expected_frame_length != 0U &&
		    logical_frame_length == expected_frame_length) {
			handle_logical_frame(now_ms);
			logical_frame_length = 0U;
			expected_frame_length = 0U;
			return;
		}
	}
	if (logical_frame_length != 0U) {
		logical_frame_last_report_ms = now_ms;
	}
}

void update_protocol_tick(uint32_t now_ms)
{
	if (logical_frame_length != 0U &&
	    (uint32_t)(now_ms - logical_frame_last_report_ms) >=
		LOGICAL_FRAME_TIMEOUT_MS) {
		logical_frame_length = 0U;
		expected_frame_length = 0U;
	}
	if (session.active &&
	    (uint32_t)(now_ms - session.last_activity_ms) >= UPDATE_SESSION_TIMEOUT_MS) {
		uint16_t total_frames = session.total_frames;
		uint16_t frame_index = session.next_frame;
		abort_session();
		queue_update_ack(total_frames, frame_index, UPDATE_ACK_TIMEOUT, false);
	}
}

bool update_protocol_power_off_due(uint32_t now_ms)
{
	return committed && (int32_t)(now_ms - power_off_deadline_ms) >= 0;
}
