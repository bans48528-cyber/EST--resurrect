#include <libopencm3/stm32/rcc.h>
#include <libopencm3/usb/bos.h>
#include <libopencm3/usb/dwc/otg_hs.h>
#include <libopencm3/usb/usbd.h>

#include "usb_dwc_common.h"
#include "usb_hs_ulpi_driver.h"
#include "usb_private.h"

#define EST_HS_RX_FIFO_WORDS 512U
#define EST_HS_TURNAROUND_TIME (9U << 10U)

static usbd_device *est_otghs_ulpi_init(void);
static struct _usbd_device usb_device_state;

const usbd_driver est_otghs_ulpi_usb_driver = {
	.init = est_otghs_ulpi_init,
	.set_address = dwc_set_address,
	.ep_setup = dwc_ep_setup,
	.ep_reset = dwc_endpoints_reset,
	.ep_stall_set = dwc_ep_stall_set,
	.ep_stall_get = dwc_ep_stall_get,
	.ep_nak_set = dwc_ep_nak_set,
	.ep_write_packet = dwc_ep_write_packet,
	.ep_read_packet = dwc_ep_read_packet,
	.poll = dwc_poll,
	.disconnect = dwc_disconnect,
	.base_address = USB_OTG_HS_BASE,
	.set_address_before_status = true,
	.rx_fifo_size = EST_HS_RX_FIFO_WORDS,
};

static usbd_device *est_otghs_ulpi_init(void)
{
	rcc_periph_clock_enable(RCC_OTGHSULPI);
	rcc_periph_clock_enable(RCC_OTGHS);

	OTG_HS_GINTSTS = OTG_GINTSTS_MMIS;
	OTG_HS_GUSBCFG &= ~OTG_GUSBCFG_PHYSEL;
	OTG_HS_GCCFG &= ~(OTG_GCCFG_VBUSASEN | OTG_GCCFG_VBUSBSEN |
		OTG_GCCFG_PWRDWN);

	while ((OTG_HS_GRSTCTL & OTG_GRSTCTL_AHBIDL) == 0U) {
	}
	OTG_HS_GRSTCTL |= OTG_GRSTCTL_CSRST;
	while ((OTG_HS_GRSTCTL & OTG_GRSTCTL_CSRST) != 0U) {
	}

	OTG_HS_GUSBCFG = (OTG_HS_GUSBCFG &
		~(OTG_GUSBCFG_FHMOD | OTG_GUSBCFG_TRDT_MASK)) |
		OTG_GUSBCFG_FDMOD | EST_HS_TURNAROUND_TIME;
	OTG_HS_DCFG &= ~OTG_DCFG_DSPD;
	OTG_HS_PCGCCTL = 0U;
	OTG_HS_DCTL &= ~OTG_DCTL_SDIS;

	OTG_HS_GAHBCFG |= OTG_GAHBCFG_GINT;
	OTG_HS_GINTMSK = OTG_GINTMSK_RXFLVLM | OTG_GINTMSK_USBSUSPM |
		OTG_GINTMSK_USBRST | OTG_GINTMSK_ENUMDNEM |
		OTG_GINTMSK_IEPINT | OTG_GINTMSK_OEPINT | OTG_GINTMSK_WUIM;
	OTG_HS_DAINTMSK = 0x000F000FU;
	OTG_HS_DIEPMSK = OTG_DIEPMSK_XFRCM;
	OTG_HS_DOEPMSK = OTG_DOEPMSK_STUPM | OTG_DOEPMSK_XFRCM;

	return &usb_device_state;
}
