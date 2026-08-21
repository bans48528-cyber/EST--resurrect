#ifndef USB_HID_H
#define USB_HID_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"

void usb_hid_init(void);
void usb_hid_poll(void);
bool usb_hid_queue_report(const uint8_t report[USB_HID_REPORT_SIZE],
	bool power_off_after_report);
bool usb_hid_power_off_requested(void);

#endif
