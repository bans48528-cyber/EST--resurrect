#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "update_protocol.h"
#include "update_storage.h"
#include "usb_hid.h"

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
	if (report[2] == HEARTBEAT_COMMAND) {
		return data_length == 0U;
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
	static const char version[] = APP_VERSION_TEXT;
	uint8_t report[USB_HID_REPORT_SIZE] = {0};

	report[0] = FRAME_START_BYTE;
	report[1] = DEVICE_FRAME_DIRECTION;
	report[2] = HEARTBEAT_COMMAND;
	report[3] = 6U;
	report[4] = 0U;
	memcpy(&report[5], version, 6U);
	report[11] = checksum(report, 11U);
	report[12] = FRAME_END_BYTE;
	(void)usb_hid_queue_report(report, false);
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

	if (logical_frame[2] == HEARTBEAT_COMMAND && data_length == 0U) {
		queue_heartbeat();
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
