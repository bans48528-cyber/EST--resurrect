#ifndef EST_UI_SETTINGS_H
#define EST_UI_SETTINGS_H

#include <stdint.h>

#include "est_types.h"
#include "est_ui_text.h"

#define EST_UI_SETTINGS_SECTOR_SIZE 4096U
#define EST_UI_SETTINGS_BANK0_ADDRESS 0x01FCE000U
#define EST_UI_SETTINGS_BANK1_ADDRESS 0x01FCF000U
#define EST_UI_SETTINGS_REGION_END 0x01FD0000U
#define EST_UI_SETTINGS_NO_RECENT_SLOT 0xFFU

typedef struct {
	uint8_t backlight_percent;
	uint8_t volume_percent;
	est_ui_language_t language;
	uint8_t recent_program_slot;
} est_ui_settings_data_t;

est_result_t est_ui_settings_load(est_ui_settings_data_t *settings);
est_result_t est_ui_settings_save(const est_ui_settings_data_t *settings);

#endif
