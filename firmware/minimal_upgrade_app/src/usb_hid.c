#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/usb/usbd.h>

#include "app_config.h"
#include "est_system.h"
#include "update_protocol.h"
#include "usb_hs_ulpi_driver.h"
#include "usb_hid.h"

#define REPORT_QUEUE_DEPTH 4U

struct est_hid_descriptor {
	uint8_t length;
	uint8_t descriptor_type;
	uint16_t hid_version;
	uint8_t country_code;
	uint8_t descriptor_count;
	uint8_t report_descriptor_type;
	uint16_t report_descriptor_length;
} __attribute__((packed));

static const uint8_t hid_report_descriptor[] = {
	0x05, 0x8c, 0x09, 0x01, 0xa1, 0x01,
	0x09, 0x03, 0x15, 0x00, 0x26, 0x00, 0xff,
	0x75, 0x08, 0x96, 0x00, 0x04, 0x81, 0x02,
	0x09, 0x04, 0x15, 0x00, 0x26, 0x00, 0xff,
	0x75, 0x08, 0x96, 0x00, 0x04, 0x91, 0x02, 0xc0
};

static const struct est_hid_descriptor hid_descriptor = {
	.length = sizeof(struct est_hid_descriptor),
	.descriptor_type = USB_HID_DT_HID,
	.hid_version = 0x0101,
	.country_code = 0,
	.descriptor_count = 1,
	.report_descriptor_type = USB_HID_DT_REPORT,
	.report_descriptor_length = sizeof(hid_report_descriptor)
};

static const struct usb_endpoint_descriptor hid_endpoints[] = {
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = USB_HID_IN_ENDPOINT,
		.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
		.wMaxPacketSize = USB_HID_REPORT_SIZE,
		.bInterval = 1
	},
	{
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = USB_HID_OUT_ENDPOINT,
		.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
		.wMaxPacketSize = USB_HID_REPORT_SIZE,
		.bInterval = 1
	}
};

static const struct usb_interface_descriptor hid_interface_descriptor[] = {
	{
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 2,
		.bInterfaceClass = USB_CLASS_HID,
		.bInterfaceSubClass = 0,
		.bInterfaceProtocol = 0,
		.iInterface = 0,
		.endpoint = hid_endpoints,
		.extra = &hid_descriptor,
		.extralen = sizeof(hid_descriptor)
	}
};

static const struct usb_interface hid_interfaces[] = {
	{
		.num_altsetting = 1,
		.altsetting = hid_interface_descriptor
	}
};

static const struct usb_config_descriptor usb_config_descriptor = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = USB_CONFIG_ATTR_DEFAULT | USB_CONFIG_ATTR_SELF_POWERED |
		USB_CONFIG_ATTR_REMOTE_WAKEUP,
	.bMaxPower = 0x32,
	.interface = hid_interfaces
};

static const struct usb_device_descriptor usb_device_descriptor = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = USB_VENDOR_ID,
	.idProduct = USB_PRODUCT_ID,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1
};

static const char *const usb_strings[] = {
	"EST",
	"EST USB HS Mode",
	"00000000011B"
};

static const uint8_t usb_device_qualifier_descriptor[] = {
	10U, USB_DT_DEVICE_QUALIFIER, 0x00U, 0x02U, 0x00U,
	0x00U, 0x00U, 64U, 1U, 0U
};

static const uint8_t usb_other_speed_configuration[] = {
	9U, USB_DT_OTHER_SPEED_CONFIGURATION, 41U, 0U, 1U, 1U, 0U, 0xE0U, 0x32U,
	9U, USB_DT_INTERFACE, 0U, 0U, 2U, USB_CLASS_HID, 0U, 0U, 0U,
	9U, USB_HID_DT_HID, 0x01U, 0x01U, 0U, 1U, USB_HID_DT_REPORT,
	(uint8_t)sizeof(hid_report_descriptor), 0U,
	7U, USB_DT_ENDPOINT, USB_HID_IN_ENDPOINT, USB_ENDPOINT_ATTR_INTERRUPT,
	0x00U, 0x04U, 1U,
	7U, USB_DT_ENDPOINT, USB_HID_OUT_ENDPOINT, USB_ENDPOINT_ATTR_INTERRUPT,
	0x00U, 0x04U, 1U
};

static uint8_t control_buffer[128];
static uint8_t out_report[USB_HID_REPORT_SIZE];
static struct {
	uint8_t data[USB_HID_REPORT_SIZE];
	bool power_off_after;
} report_queue[REPORT_QUEUE_DEPTH];
static usbd_device *usb_device;
static uint8_t report_queue_head;
static uint8_t report_queue_tail;
static uint8_t report_queue_count;
static bool in_busy;
static bool in_power_off_after;
static bool power_off_requested;
static uint8_t hid_idle_state;
static uint8_t hid_protocol;
static bool host_configured;
static bool host_suspended;

static enum usbd_request_return_codes hid_control_request(
	usbd_device *device, struct usb_setup_data *request, uint8_t **buffer,
	uint16_t *length, usbd_control_complete_callback *complete);

static enum usbd_request_return_codes high_speed_descriptor_request(
	usbd_device *device, struct usb_setup_data *request, uint8_t **buffer,
	uint16_t *length, usbd_control_complete_callback *complete)
{
	uint8_t descriptor_type;

	(void)device;
	(void)complete;
	if (request->bRequest != USB_REQ_GET_DESCRIPTOR) {
		return USBD_REQ_NEXT_CALLBACK;
	}
	descriptor_type = (uint8_t)(request->wValue >> 8U);
	if (descriptor_type == USB_DT_DEVICE_QUALIFIER) {
		*buffer = (uint8_t *)(uintptr_t)usb_device_qualifier_descriptor;
		if (*length > sizeof(usb_device_qualifier_descriptor)) {
			*length = sizeof(usb_device_qualifier_descriptor);
		}
		return USBD_REQ_HANDLED;
	}
	if (descriptor_type == USB_DT_OTHER_SPEED_CONFIGURATION) {
		*buffer = (uint8_t *)(uintptr_t)usb_other_speed_configuration;
		if (*length > sizeof(usb_other_speed_configuration)) {
			*length = sizeof(usb_other_speed_configuration);
		}
		return USBD_REQ_HANDLED;
	}
	return USBD_REQ_NEXT_CALLBACK;
}

static void register_control_callbacks(usbd_device *device)
{
	(void)usbd_register_control_callback(device,
		USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_DEVICE,
		USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
		high_speed_descriptor_request);
	(void)usbd_register_control_callback(device,
		USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
		USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT, hid_control_request);
	(void)usbd_register_control_callback(device,
		USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
		USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT, hid_control_request);
}

static void try_send_report(void)
{
	if (usb_device != NULL && report_queue_count != 0U && !in_busy &&
	    usbd_ep_write_packet(usb_device, USB_HID_IN_ENDPOINT,
		report_queue[report_queue_head].data, USB_HID_REPORT_SIZE) ==
		USB_HID_REPORT_SIZE) {
		in_power_off_after = report_queue[report_queue_head].power_off_after;
		report_queue_head = (uint8_t)((report_queue_head + 1U) %
			REPORT_QUEUE_DEPTH);
		report_queue_count--;
		in_busy = true;
	}
}

static void hid_in_callback(usbd_device *device, uint8_t endpoint)
{
	(void)device;
	(void)endpoint;
	in_busy = false;
	if (in_power_off_after) {
		power_off_requested = true;
	}
	in_power_off_after = false;
}

static void hid_out_callback(usbd_device *device, uint8_t endpoint)
{
	uint16_t received;

	(void)endpoint;
	received = usbd_ep_read_packet(device, USB_HID_OUT_ENDPOINT,
		out_report, sizeof(out_report));
	if (received != 0U) {
		update_protocol_feed_report(out_report, received, est_system_millis());
	}
}

static enum usbd_request_return_codes hid_control_request(
	usbd_device *device, struct usb_setup_data *request, uint8_t **buffer,
	uint16_t *length, usbd_control_complete_callback *complete)
{
	uint8_t request_type = request->bmRequestType & USB_REQ_TYPE_TYPE;

	(void)device;
	(void)complete;
	if (request_type == USB_REQ_TYPE_STANDARD &&
	    request->bRequest == USB_REQ_GET_DESCRIPTOR) {
		uint8_t descriptor_type = (uint8_t)(request->wValue >> 8U);
		if (descriptor_type == USB_HID_DT_REPORT) {
			*buffer = (uint8_t *)(uintptr_t)hid_report_descriptor;
			if (*length > sizeof(hid_report_descriptor)) {
				*length = sizeof(hid_report_descriptor);
			}
			return USBD_REQ_HANDLED;
		}
		if (descriptor_type == USB_HID_DT_HID) {
			*buffer = (uint8_t *)(uintptr_t)&hid_descriptor;
			if (*length > sizeof(hid_descriptor)) {
				*length = sizeof(hid_descriptor);
			}
			return USBD_REQ_HANDLED;
		}
		return USBD_REQ_NEXT_CALLBACK;
	}

	if (request_type != USB_REQ_TYPE_CLASS) {
		return USBD_REQ_NEXT_CALLBACK;
	}
	switch (request->bRequest) {
	case USB_HID_REQ_TYPE_SET_PROTOCOL:
		hid_protocol = (uint8_t)request->wValue;
		return USBD_REQ_HANDLED;
	case USB_HID_REQ_TYPE_GET_PROTOCOL:
		*buffer = &hid_protocol;
		*length = 1U;
		return USBD_REQ_HANDLED;
	case USB_HID_REQ_TYPE_SET_IDLE:
		hid_idle_state = (uint8_t)(request->wValue >> 8U);
		return USBD_REQ_HANDLED;
	case USB_HID_REQ_TYPE_GET_IDLE:
		*buffer = &hid_idle_state;
		*length = 1U;
		return USBD_REQ_HANDLED;
	case USB_HID_REQ_TYPE_SET_REPORT:
	case USB_HID_REQ_TYPE_GET_REPORT:
	default:
		return USBD_REQ_NOTSUPP;
	}
}

static void hid_set_config(usbd_device *device, uint16_t value)
{
	host_configured = value != 0U;
	host_suspended = false;
	in_busy = false;
	in_power_off_after = false;
	report_queue_head = 0U;
	report_queue_tail = 0U;
	report_queue_count = 0U;
	if (!host_configured) {
		return;
	}
	usbd_ep_setup(device, USB_HID_OUT_ENDPOINT, USB_ENDPOINT_ATTR_INTERRUPT,
		USB_HID_REPORT_SIZE, hid_out_callback);
	usbd_ep_setup(device, USB_HID_IN_ENDPOINT, USB_ENDPOINT_ATTR_INTERRUPT,
		USB_HID_REPORT_SIZE, hid_in_callback);
	register_control_callbacks(device);
}

static void usb_reset_callback(void)
{
	host_configured = false;
	host_suspended = false;
}

static void usb_suspend_callback(void)
{
	host_suspended = true;
}

static void usb_resume_callback(void)
{
	host_suspended = false;
}

void usb_hid_init(void)
{
	const uint16_t gpioa_ulpi = GPIO3 | GPIO5;
	const uint16_t gpiob_ulpi = GPIO0 | GPIO1 | GPIO5 | GPIO10 | GPIO11 |
		GPIO12 | GPIO13;
	const uint16_t gpioc_ulpi = GPIO0 | GPIO2 | GPIO3;

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, gpioa_ulpi);
	gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, gpiob_ulpi);
	gpio_mode_setup(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, gpioc_ulpi);
	gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ,
		gpioa_ulpi);
	gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ,
		gpiob_ulpi);
	gpio_set_output_options(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ,
		gpioc_ulpi);
	gpio_set_af(GPIOA, GPIO_AF10, gpioa_ulpi);
	gpio_set_af(GPIOB, GPIO_AF10, gpiob_ulpi);
	gpio_set_af(GPIOC, GPIO_AF10, gpioc_ulpi);

	usb_device = usbd_init(&est_otghs_ulpi_usb_driver, &usb_device_descriptor,
		&usb_config_descriptor, usb_strings,
		(int)(sizeof(usb_strings) / sizeof(usb_strings[0])),
		control_buffer, sizeof(control_buffer));
	host_configured = false;
	host_suspended = false;
	register_control_callbacks(usb_device);
	(void)usbd_register_set_config_callback(usb_device, hid_set_config);
	usbd_register_reset_callback(usb_device, usb_reset_callback);
	usbd_register_suspend_callback(usb_device, usb_suspend_callback);
	usbd_register_resume_callback(usb_device, usb_resume_callback);
}

void usb_hid_poll(void)
{
	try_send_report();
	usbd_poll(usb_device);
	try_send_report();
}

bool usb_hid_queue_report(const uint8_t report[USB_HID_REPORT_SIZE],
	bool power_off_after_report)
{
	if (report_queue_count >= REPORT_QUEUE_DEPTH) {
		return false;
	}
	memcpy(report_queue[report_queue_tail].data, report, USB_HID_REPORT_SIZE);
	report_queue[report_queue_tail].power_off_after = power_off_after_report;
	report_queue_tail = (uint8_t)((report_queue_tail + 1U) %
		REPORT_QUEUE_DEPTH);
	report_queue_count++;
	return true;
}

bool usb_hid_power_off_requested(void)
{
	return power_off_requested;
}

bool usb_hid_host_connected(void)
{
	return host_configured && !host_suspended;
}
