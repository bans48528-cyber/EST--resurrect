#include <stdbool.h>
#include <stdint.h>

#include "board_audio.h"
#include "est_feedback.h"

#define EST_FEEDBACK_TRANSFER_START_WINDOW_MS 1200U

static bool transfer_sound_recent;
static uint32_t transfer_sound_started_ms;

static void play_feedback(const char *resource_name, uint32_t now_ms)
{
	transfer_sound_recent = false;
	(void)board_audio_play(resource_name, now_ms);
}

void est_feedback_init(void)
{
	transfer_sound_recent = false;
	transfer_sound_started_ms = 0U;
}

void est_feedback_button(uint32_t now_ms)
{
	transfer_sound_recent = false;
	(void)board_audio_feedback_tone(now_ms);
}

void est_feedback_usb_connected(uint32_t now_ms)
{
	play_feedback("System/Connect", now_ms);
}

void est_feedback_transfer_complete(uint32_t now_ms)
{
	transfer_sound_recent = board_audio_play("System/Download", now_ms);
	transfer_sound_started_ms = now_ms;
}

void est_feedback_program_start(bool requires_host, uint32_t now_ms)
{
	if (requires_host && transfer_sound_recent &&
	    now_ms - transfer_sound_started_ms <=
		EST_FEEDBACK_TRANSFER_START_WINDOW_MS) {
		transfer_sound_recent = false;
		return;
	}
	play_feedback("System/Download", now_ms);
}
