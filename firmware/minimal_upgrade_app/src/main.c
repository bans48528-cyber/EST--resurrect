#include <stdint.h>

#include "platform.h"
#include "update_protocol.h"
#include "usb_hid.h"

int main(void)
{
	uint32_t last_diag_ms = 0U;
	uint8_t diag_phase = 0U;

	platform_init();
	platform_diag_checkpoint(1U);
	update_protocol_init();
	platform_diag_checkpoint(2U);
	usb_hid_init();
	platform_diag_checkpoint(3U);
	platform_enable_interrupts();
	platform_diag_checkpoint(0U);

	while (1) {
		uint32_t now_ms;

		usb_hid_poll();
		now_ms = platform_millis();
		if ((now_ms - last_diag_ms) >= 500U) {
			last_diag_ms = now_ms;
			diag_phase++;
			platform_diag_set(diag_phase);
		}
		update_protocol_tick(now_ms);
		if (usb_hid_power_off_requested() ||
		    update_protocol_power_off_due(now_ms)) {
			platform_power_off();
		}
	}
}
