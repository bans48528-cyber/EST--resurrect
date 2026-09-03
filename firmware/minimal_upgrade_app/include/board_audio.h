#ifndef BOARD_AUDIO_H
#define BOARD_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

enum board_audio_state {
	BOARD_AUDIO_IDLE = 0,
	BOARD_AUDIO_PLAYING = 1,
	BOARD_AUDIO_FAULT = 2
};

void board_audio_init(void);
bool board_audio_ready(void);
bool board_audio_play(const char *name, uint32_t now_ms);
bool board_audio_tone(uint8_t note, int32_t duration_ms, uint32_t now_ms);
bool board_audio_has_resource(const char *name);
void board_audio_stop(void);
enum board_audio_state board_audio_state(void);
uint32_t board_audio_generation(void);
uint32_t board_audio_bytes_sent(void);
uint16_t board_audio_decoder_status(void);
uint16_t board_audio_mode_register(void);
uint8_t board_audio_phase(void);
uint8_t board_audio_failure_phase(void);
uint32_t board_audio_stream_crc32(void);
uint32_t board_audio_dreq_waits(void);
bool board_audio_read_register(uint8_t reg, uint16_t *value);
bool board_audio_set_volume_percent(uint8_t percent);
uint8_t board_audio_volume_percent(void);
void board_audio_tick(uint32_t now_ms);

#endif
