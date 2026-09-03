#include <stddef.h>
#include <string.h>

#include "audio_resources.h"
#include "board_audio.h"
#include "board_audio_bus.h"
#include "board_flash.h"
#include "est_audio_resource_store.h"
#include "system_time.h"

#define AUDIO_BOOT_TIMEOUT_MS 500U
#define AUDIO_STALL_TIMEOUT_MS 1000U
#define AUDIO_RESET_HOLD_MS 10U
#define AUDIO_STREAM_CHUNK_SIZE 32U
#define AUDIO_CHUNKS_PER_TICK 4U
#define AUDIO_START_PREFILL_BYTES 2048U
#define AUDIO_MP3_END_FILL_BYTES 2048U
#define AUDIO_DAC_DRAIN_MS 150U
#define AUDIO_LEGACY_PIANO_PREFIX "Piano/"
#define AUDIO_MODE_NEW 0x0800U
#define AUDIO_CLOCK_NORMAL 0x9800U

enum audio_phase {
	AUDIO_IDLE, AUDIO_RESET, AUDIO_BOOT, AUDIO_STATUS, AUDIO_CLOCK,
	AUDIO_VOLUME, AUDIO_PREROLL, AUDIO_STREAMING, AUDIO_FLUSHING,
	AUDIO_DRAINING, AUDIO_ERROR
};

static enum audio_phase phase;
static enum audio_phase failure_phase;
static const struct audio_resource *resource;
static bool tone;
static bool ready;
static bool decoder_prefilled;
static bool volume_pending;
static bool stream_started;
static uint8_t volume_percent;
static uint8_t tone_note;
static int32_t tone_duration_ms;
static uint32_t generation;
static uint32_t phase_started_ms;
static uint32_t last_progress_ms;
static uint32_t stream_started_ms;
static uint32_t tone_started_ms;
static uint32_t stream_offset;
static uint32_t fill_offset;
static uint32_t tone_phase;
static uint32_t bytes_sent;
static uint32_t stream_crc;
static uint32_t dreq_waits;
static uint16_t decoder_status;
static uint16_t mode_register;
static uint16_t tone_sample;
static struct audio_resource flash_resource;
static char flash_resource_name[EST_AUDIO_RESOURCE_NAME_MAX_BYTES + 1U];

static bool write_volume_level(uint8_t percent);

/* A large PCM stream is ended by the duration timer or explicit stop/reset. */
static const uint8_t tone_header[44] = {
	'R', 'I', 'F', 'F', 0x24U, 0xFFU, 0xFFU, 0xFFU,
	'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
	16U, 0U, 0U, 0U, 1U, 0U, 1U, 0U,
	0U, 0x7DU, 0U, 0U, 0U, 0xFAU, 0U, 0U,
	2U, 0U, 16U, 0U, 'd', 'a', 't', 'a',
	0U, 0xFFU, 0xFFU, 0xFFU
};

static const struct audio_resource *find_resource(const char *name)
{
	uint32_t index;
	if (name == NULL) {
		return NULL;
	}
	for (index = 0U; index < audio_resource_count; index++) {
		if (strcmp(name, audio_resources[index].name) == 0) {
			return &audio_resources[index];
		}
	}
	if (est_audio_resource_find(name, &flash_resource, flash_resource_name,
		sizeof(flash_resource_name))) {
		return &flash_resource;
	}
	return NULL;
}

static bool has_job(void)
{
	return tone || resource != NULL;
}

static bool resource_is_legacy_piano(const struct audio_resource *item)
{
	return item != NULL &&
		strncmp(item->name, AUDIO_LEGACY_PIANO_PREFIX,
			sizeof(AUDIO_LEGACY_PIANO_PREFIX) - 1U) == 0;
}

static uint32_t resource_stream_limit(const struct audio_resource *item)
{
	uint32_t legacy_limit;
	if (!resource_is_legacy_piano(item)) {
		return item->length;
	}
	legacy_limit = (item->length / 3U) * 2U;
	legacy_limit = ((legacy_limit + AUDIO_STREAM_CHUNK_SIZE - 1U) /
		AUDIO_STREAM_CHUNK_SIZE) * AUDIO_STREAM_CHUNK_SIZE;
	return legacy_limit < item->length ? legacy_limit : item->length;
}

static void fail(void)
{
	failure_phase = phase;
	board_audio_bus_reset(true);
	ready = false;
	decoder_prefilled = false;
	phase = AUDIO_ERROR;
}

static void reset_stream_state(uint32_t now_ms)
{
	failure_phase = AUDIO_IDLE;
	phase_started_ms = now_ms;
	last_progress_ms = now_ms;
	stream_offset = 0U;
	fill_offset = 0U;
	bytes_sent = 0U;
	stream_crc = UINT32_MAX;
	dreq_waits = 0U;
	tone_phase = 0U;
	stream_started = false;
	volume_pending = true;
	generation++;
}

static void begin_reset(uint32_t now_ms)
{
	board_audio_bus_reset(true);
	ready = false;
	decoder_prefilled = false;
	phase = AUDIO_RESET;
	reset_stream_state(now_ms);
}

static void finish_playback(void)
{
	resource = NULL;
	tone = false;
	tone_started_ms = 0U;
	phase = AUDIO_IDLE;
	volume_pending = false;
	stream_started = false;
	decoder_prefilled = true;
	generation++;
}

void board_audio_init(void)
{
	board_audio_bus_init();
	resource = NULL;
	tone = false;
	ready = false;
	decoder_prefilled = false;
	volume_percent = 80U;
	generation = 0U;
	decoder_status = 0U;
	mode_register = 0U;
	tone_started_ms = 0U;
	begin_reset(system_time_millis());
}

bool board_audio_ready(void)
{
	return ready;
}

bool board_audio_has_resource(const char *name)
{
	return find_resource(name) != NULL;
}

bool board_audio_play(const char *name, uint32_t now_ms)
{
	const struct audio_resource *next = find_resource(name);
	bool pending_volume = volume_pending;
	uint8_t header[3];
	if (next == NULL) {
		return false;
	}
	if (next->data == NULL &&
	    (!board_flash_read_4byte(next->flash_address, header, sizeof(header)) ||
	     memcmp(header, "ID3", sizeof(header)) != 0)) {
		return false;
	}
	resource = next;
	tone = false;
	if (ready && phase == AUDIO_IDLE && board_audio_bus_ready()) {
		reset_stream_state(now_ms);
		volume_pending = pending_volume;
		phase = AUDIO_VOLUME;
	} else {
		begin_reset(now_ms);
	}
	return true;
}

bool board_audio_tone(uint8_t note, int32_t duration_ms, uint32_t now_ms)
{
	if (note > 127U || duration_ms < -1) {
		return false;
	}
	if (duration_ms == 0) {
		board_audio_stop();
		return true;
	}
	resource = NULL;
	tone = true;
	tone_note = note;
	tone_duration_ms = duration_ms;
	tone_started_ms = now_ms;
	begin_reset(now_ms);
	return true;
}

void board_audio_stop(void)
{
	/* Hardware reset stops the DAC even when DREQ or the decoder is stuck. */
	board_audio_bus_reset(true);
	ready = false;
	decoder_prefilled = false;
	resource = NULL;
	tone = false;
	tone_started_ms = 0U;
	phase = AUDIO_IDLE;
	volume_pending = false;
	generation++;
}

enum board_audio_state board_audio_state(void)
{
	if (phase == AUDIO_ERROR) {
		return BOARD_AUDIO_FAULT;
	}
	return has_job() ? BOARD_AUDIO_PLAYING : BOARD_AUDIO_IDLE;
}

uint32_t board_audio_generation(void)
{
	return generation;
}

uint32_t board_audio_bytes_sent(void)
{
	return bytes_sent;
}

uint16_t board_audio_decoder_status(void)
{
	return decoder_status;
}

uint16_t board_audio_mode_register(void)
{
	return mode_register;
}

uint8_t board_audio_phase(void)
{
	return (uint8_t)phase;
}

uint8_t board_audio_failure_phase(void)
{
	return (uint8_t)failure_phase;
}

uint32_t board_audio_stream_crc32(void)
{
	return stream_crc ^ UINT32_MAX;
}

uint32_t board_audio_dreq_waits(void)
{
	return dreq_waits;
}

bool board_audio_read_register(uint8_t reg, uint16_t *value)
{
	if (!ready || !board_audio_bus_ready()) {
		return false;
	}
	return board_audio_bus_read(reg, value);
}

static void checksum_payload(const uint8_t *data, uint32_t length)
{
	uint32_t index;
	for (index = 0U; index < length; index++) {
		uint8_t bit;
		stream_crc ^= data[index];
		for (bit = 0U; bit < 8U; bit++) {
			stream_crc = (stream_crc >> 1U) ^
				((stream_crc & 1U) != 0U ? 0xEDB88320UL : 0U);
		}
	}
}

bool board_audio_set_volume_percent(uint8_t percent)
{
	if (percent > 100U) {
		return false;
	}
	volume_percent = percent;
	volume_pending = true;
	if (ready && phase == AUDIO_IDLE && board_audio_bus_ready()) {
		(void)write_volume_level(percent);
	}
	return true;
}

uint8_t board_audio_volume_percent(void)
{
	return volume_percent;
}

static bool write_volume_level(uint8_t percent)
{
	uint8_t attenuation = percent == 0U ? 0xFEU :
		(uint8_t)((100U - percent) * 2U);
	if (!board_audio_bus_write(11U,
		(uint16_t)((uint16_t)attenuation << 8U) | attenuation)) {
		return false;
	}
	volume_pending = false;
	return true;
}

static bool write_volume(void)
{
	return write_volume_level(volume_percent);
}

static uint8_t next_tone_byte(void)
{
	uint8_t byte;
	if (stream_offset < sizeof(tone_header)) {
		return tone_header[stream_offset++];
	}
	if ((stream_offset & 1U) == 0U) {
		tone_sample = (uint16_t)audio_sine_samples[tone_phase >> 26U];
		tone_phase += audio_note_phase_steps[tone_note];
		byte = (uint8_t)tone_sample;
	} else {
		byte = (uint8_t)(tone_sample >> 8U);
	}
	/* Keep an indefinite stream beyond the header even after counter wrap. */
	stream_offset = stream_offset == UINT32_MAX ? 44U : stream_offset + 1U;
	return byte;
}

void board_audio_tick(uint32_t now_ms)
{
	uint32_t chunk;
	if (phase == AUDIO_IDLE) {
		if (ready && volume_pending && board_audio_bus_ready() &&
		    !write_volume()) {
			fail();
		}
		return;
	}
	if (phase == AUDIO_ERROR) {
		return;
	}
	if (tone && stream_started && tone_duration_ms >= 0 &&
	    now_ms - tone_started_ms >= (uint32_t)tone_duration_ms) {
		board_audio_stop();
		return;
	}
	if (phase == AUDIO_RESET) {
		if (now_ms - phase_started_ms >= AUDIO_RESET_HOLD_MS) {
			board_audio_bus_reset(false);
			phase = AUDIO_BOOT;
			phase_started_ms = now_ms;
		}
		return;
	}
	if (!board_audio_bus_ready()) {
		dreq_waits++;
		if (now_ms - last_progress_ms > (phase <= AUDIO_VOLUME ?
		    AUDIO_BOOT_TIMEOUT_MS : AUDIO_STALL_TIMEOUT_MS)) {
			fail();
		}
		return;
	}
	if (phase == AUDIO_BOOT) {
		if (!board_audio_bus_read(0U, &mode_register) ||
		    mode_register != AUDIO_MODE_NEW) {
			fail();
			return;
		}
		phase = AUDIO_STATUS;
	} else if (phase == AUDIO_STATUS) {
		if (!board_audio_bus_read(1U, &decoder_status) ||
		    decoder_status == 0xFFFFU) {
			fail();
			return;
		}
		phase = AUDIO_CLOCK;
	} else if (phase == AUDIO_CLOCK) {
		if (!board_audio_bus_write(3U, AUDIO_CLOCK_NORMAL)) {
			fail();
			return;
		}
		phase = AUDIO_VOLUME;
	} else if (phase == AUDIO_VOLUME) {
		if (has_job() && !decoder_prefilled) {
			if (volume_pending && !write_volume()) {
				fail();
				return;
			}
			ready = true;
			fill_offset = 0U;
			phase = AUDIO_PREROLL;
			return;
		}
		if (volume_pending && !write_volume()) {
			fail();
			return;
		}
		ready = true;
		phase = has_job() ? AUDIO_STREAMING : AUDIO_IDLE;
	} else if (volume_pending) {
		if (!write_volume()) {
			fail();
			return;
		}
	} else if (phase == AUDIO_DRAINING) {
		if (now_ms - phase_started_ms >= AUDIO_DAC_DRAIN_MS &&
		    now_ms - stream_started_ms >= resource->duration_ms +
			AUDIO_DAC_DRAIN_MS) {
			finish_playback();
		}
	} else if (phase == AUDIO_PREROLL) {
		for (chunk = 0U; chunk < AUDIO_CHUNKS_PER_TICK; chunk++) {
			uint8_t data[AUDIO_STREAM_CHUNK_SIZE];
			if (!board_audio_bus_ready()) {
				dreq_waits++;
				break;
			}
			memset(data, 0, sizeof(data));
			if (!board_audio_bus_send(data, sizeof(data))) {
				fail();
				return;
			}
			fill_offset += sizeof(data);
			last_progress_ms = now_ms;
			if (fill_offset >= AUDIO_START_PREFILL_BYTES) {
				fill_offset = 0U;
				decoder_prefilled = true;
				phase = AUDIO_VOLUME;
				break;
			}
		}
	} else {
		for (chunk = 0U; chunk < AUDIO_CHUNKS_PER_TICK; chunk++) {
			uint8_t data[AUDIO_STREAM_CHUNK_SIZE];
			uint32_t count = AUDIO_STREAM_CHUNK_SIZE;
			uint32_t index;
			if (!board_audio_bus_ready()) {
				dreq_waits++;
				break;
			}
			if (phase == AUDIO_FLUSHING) {
				memset(data, 0, sizeof(data));
			} else if (tone) {
				for (index = 0U; index < count; index++) {
					data[index] = next_tone_byte();
				}
			} else {
				uint32_t limit = resource_stream_limit(resource);
				uint32_t remaining = limit - stream_offset;
				if (remaining < count) {
					count = remaining;
				}
				if (resource->data != NULL) {
					memcpy(data, &resource->data[stream_offset], count);
				} else if (!board_flash_read_4byte(resource->flash_address +
				    stream_offset, data, count)) {
					fail();
					return;
				}
				stream_offset += count;
			}
			if (!board_audio_bus_send(data, count)) {
				fail();
				return;
			}
			if (!tone && phase == AUDIO_STREAMING) {
				checksum_payload(data, count);
			}
			bytes_sent += count;
			last_progress_ms = now_ms;
			if (!stream_started) {
				stream_started = true;
				stream_started_ms = now_ms;
			}
			if (phase == AUDIO_FLUSHING) {
				fill_offset += count;
				if (fill_offset >= AUDIO_MP3_END_FILL_BYTES) {
					phase = AUDIO_DRAINING;
					phase_started_ms = now_ms;
					break;
				}
			} else if (!tone &&
			    stream_offset == resource_stream_limit(resource) &&
			    phase == AUDIO_STREAMING) {
				phase = AUDIO_FLUSHING;
			}
		}
		return;
	}
	last_progress_ms = now_ms;
}
