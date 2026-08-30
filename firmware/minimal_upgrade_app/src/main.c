#include <stdint.h>

#include "board_audio.h"
#include "board_flash.h"
#include "board_motor.h"
#include "board_sensor.h"
#include "est_backlight.h"
#include "est_battery.h"
#include "est_buttons.h"
#include "est_display.h"
#include "est_led.h"
#include "est_micropython.h"
#include "est_runtime.h"
#include "est_system.h"
#include "est_ui.h"
#include "platform.h"
#include "update_protocol.h"
#include "usb_hid.h"

int main(void)
{
	est_system_init();
	est_backlight_init();
	est_led_init();
	est_buttons_init(est_system_millis());
	board_flash_init();
	board_audio_init();
	board_motor_init();
	board_sensor_init(est_system_millis());
	est_battery_init(est_system_millis());
	update_protocol_init();
	est_runtime_init(est_system_millis());
	usb_hid_init();
	platform_enable_interrupts();
	est_display_init();
	est_micropython_init();
	est_ui_init(est_system_millis());
	(void)est_led_set(EST_LED_OFF);

	while (1) {
		uint32_t now_ms;

		usb_hid_poll();
		now_ms = est_system_millis();
		est_runtime_tick(now_ms);
		est_ui_tick(now_ms);
		est_micropython_tick();
		if (est_ui_power_off_requested() ||
		    usb_hid_power_off_requested() ||
		    update_protocol_power_off_due(now_ms)) {
			est_micropython_deinit();
			est_system_power_off();
		}
	}
}
