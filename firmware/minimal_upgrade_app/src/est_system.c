#include <stdint.h>

#include "board_power.h"
#include "board_audio.h"
#include "board_sensor.h"
#include "est_backlight.h"
#include "est_led.h"
#include "est_motor.h"
#include "est_system.h"
#include "platform.h"
#include "system_time.h"

void est_system_init(void)
{
	board_power_init();
	system_time_init();
}

uint32_t est_system_millis(void)
{
	return system_time_millis();
}

est_result_t est_system_emergency_stop(void)
{
	est_result_t result = est_motor_stop_all(EST_STOP_COAST);

	board_audio_stop();
	board_sensor_stop();
	return result;
}

est_result_t est_system_cleanup(void)
{
	return est_system_emergency_stop();
}

est_result_t est_system_reboot(void)
{
	/* The old bootloader path has not yet been verified for software reset. */
	return EST_ERR_NOT_SUPPORTED;
}

void est_system_power_off(void)
{
	(void)est_system_cleanup();
	(void)board_audio_set_volume_percent(0U);
	(void)est_backlight_set_percent(0U);
	(void)est_led_set(EST_LED_OFF);
	platform_disable_interrupts();
	board_power_off();
}
