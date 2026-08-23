#include <stdint.h>

#include "app_version.h"
#include "board_flash.h"
#include "board_lcd.h"
#include "board_led.h"
#include "board_keys.h"
#include "board_motor.h"
#include "board_power.h"
#include "platform.h"
#include "system_time.h"
#include "update_protocol.h"
#include "usb_hid.h"

int main(void)
{
	uint32_t last_diag_ms = 0U;
	uint8_t diag_phase = 0U;

	board_power_init();
	board_led_init();
	system_time_init();
	board_keys_init(system_time_millis());
	board_flash_init();
	board_motor_init();
	board_led_checkpoint(1U);
	update_protocol_init();
	board_led_checkpoint(2U);
	usb_hid_init();
	board_led_checkpoint(3U);
	platform_enable_interrupts();
	board_led_checkpoint(0U);
#ifndef DIAGNOSTIC_SKIP_LCD_STARTUP
	board_lcd_init();
	board_lcd_show_version(app_version_text);
#endif

	while (1) {
		uint32_t now_ms;

		usb_hid_poll();
		now_ms = system_time_millis();
		board_keys_tick(now_ms);
		board_motor_tick(now_ms);
		if ((now_ms - last_diag_ms) >= 500U) {
			last_diag_ms = now_ms;
			diag_phase++;
			board_led_diag_set(diag_phase);
		}
		update_protocol_tick(now_ms);
		if (usb_hid_power_off_requested() ||
		    update_protocol_power_off_due(now_ms)) {
			board_motor_stop();
			platform_disable_interrupts();
			board_led_all_off();
			board_power_off();
		}
	}
}
