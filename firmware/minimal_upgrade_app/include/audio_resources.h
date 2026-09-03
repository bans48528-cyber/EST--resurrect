#ifndef AUDIO_RESOURCES_H
#define AUDIO_RESOURCES_H

#include <stdint.h>

struct audio_resource {
	const char *name;
	const uint8_t *data;
	uint32_t length;
	uint32_t duration_ms;
	uint32_t flash_address;
};

extern const struct audio_resource audio_resources[];
extern const uint32_t audio_resource_count;
extern const uint32_t audio_note_phase_steps[128];
extern const int16_t audio_sine_samples[64];

#endif
