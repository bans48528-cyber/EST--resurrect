#ifndef BOARD_AUDIO_BUS_H
#define BOARD_AUDIO_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void board_audio_bus_init(void);
void board_audio_bus_reset(bool asserted);
bool board_audio_bus_ready(void);
uint8_t board_audio_bus_pin_levels(void);
uint8_t board_audio_bus_pin_layout(void);
uint8_t board_audio_bus_pin_probe(void);
bool board_audio_bus_read(uint8_t reg, uint16_t *value);
bool board_audio_bus_write(uint8_t reg, uint16_t value);
bool board_audio_bus_send(const uint8_t *data, size_t length);

#endif
