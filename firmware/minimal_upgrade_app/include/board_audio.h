#ifndef BOARD_AUDIO_H
#define BOARD_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

void board_audio_init(void);
bool board_audio_ready(void);
bool board_audio_start_test(uint32_t now_ms);
bool board_audio_test_active(void);
void board_audio_tick(uint32_t now_ms);

#endif
