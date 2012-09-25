/*
 * USB HUB Class handler
 */
#include <core.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_hook.h"
#include "usb_log.h"
#include "usb_hub.h"

DEFINE_GET_U16_FROM_SETUP_FUNC(wIndex);
DEFINE_GET_U16_FROM_SETUP_FUNC(wValue);

static int
usbhub_set_portno(struct usb_host *usbhc, u8 devadr, u64 port)
{
	usbhc->last_changed_port = port;
	dprintft(3, "HUB(%02x): HUB PORT(%d) status connect.\n",
		 devadr, port & USB_PORT_MASK);

	return USB_HOOK_PASS;
}

static int
usbhub_device_disconnect(struct usb_host *usbhc, u8 devadr, u64 port)
{
	struct usb_device *dev;

	dprintft(3, "HUB(%02x): HUB PORT(%d) status disconnect.\n",
				 devadr, (int)(port & USB_PORT_MASK));
	dev = get_device_by_port (usbhc, port);
	if (dev)
		free_device (usbhc, dev);
	return USB_HOOK_PASS;
}

static int
usbhub_get_portno(struct usb_device *hubdev, struct usb_buffer_list *ub)
{
	u16 port;

	port = get_wIndex_from_setup(ub);
	return ((hubdev->portno << USB_HUB_SHIFT) + (port & USB_PORT_MASK));
}

static int
usbhub_connect_changed(struct usb_host *usbhc, 
		       struct usb_request_block *urb, void *arg)
{
	u16 feature;
	u8 devadr;
	u64 port;
	int ret = USB_HOOK_PASS;

	devadr = urb->address;
	port = usbhub_get_portno(urb->dev, urb->shadow->buffers);
	if (!port)
		return ret;
	
	feature = get_wValue_from_setup(urb->shadow->buffers);
	switch (feature) {
	case 0x0014: /* C_PORT_RESET */
		ret = usbhub_set_portno(usbhc, devadr, port);
		break;
	case 0x0010: /* C_PORT_CONNECTION */
		ret = usbhub_device_disconnect(usbhc, devadr, port);
		break;
	default:
		break;
	}

	return ret;
}

static void
usbhub_clean_list (struct usb_device *hubdev)
{
	struct usb_device *dev;
	struct usb_host *usbhc;

	usbhc = hubdev->host;
	if (!usbhc) {
		dprintft (1, "hubdev->host NULL.\n");
		return;
	}
again:
	for (dev = usbhc->device; dev; dev = dev->next) {
		if (dev->parent == hubdev) {
			/* if dev is hub, dev->next may be freed
			 * during free_device() */
			free_device (usbhc, dev);
			goto again;
		}
	}
}


static void
usbhub_remove(struct usb_device *dev)
{
	dprintft(1, "HUB(%02x): HUB device disconnect.\n", dev->devnum);

	if (!dev || !dev->handle) {
		dprintft(1, "HUB(%02x): No device entry.\n", dev->devnum);
		return;
	}

	free(dev->handle);
	dev->handle = NULL;

	usbhub_clean_list (dev);
	return;
}


static int 
init_hub_device(struct usb_host *usbhc, 
		struct usb_request_block *urb, void *arg)
{
	u8 devadr, cls;
	struct usb_device *dev;
	struct usb_device_handle *handler;
	static const struct usb_hook_pattern pat_clrpf = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x000000000000000123ULL,
		.offset = 0,
		.next = NULL
	};

	devadr = urb->address;
	dev = urb->dev;

	/* an interface descriptor must exists */
	if (!dev || !dev->config || !dev->config->interface || 
	    !dev->config->interface->altsetting) {
		dprintft(1, "HUB(%02x): interface descriptor not found.\n",
			 devadr);
		return USB_HOOK_PASS;
	}

	/* only Hub devices interests */
	cls = dev->config->interface->altsetting->bInterfaceClass;
	if (cls != 0x09)
		return USB_HOOK_PASS;
	
	dprintft(1, "HUB(%02x): A Hub Class device found\n", devadr);

	if (dev->handle) {
		dprintft(1, "HUB(%02x): maybe reset.\n",  devadr);
		return USB_HOOK_PASS;
	}

	handler = usb_new_dev_handle (usbhc, dev);
	handler->remove = usbhub_remove;
	dev->handle = handler;

	/* notify whenever ClearPortFeature() issued. */
	spinlock_lock(&usbhc->lock_hk);
	usb_hook_register (usbhc, USB_HOOK_REPLY, USB_HOOK_MATCH_DEV |
			   USB_HOOK_MATCH_ENDP | USB_HOOK_MATCH_DATA,
			   devadr, 0, &pat_clrpf, usbhub_connect_changed,
			   NULL, dev);
	spinlock_unlock(&usbhc->lock_hk);

	return USB_HOOK_PASS;
}

void 
usbhub_init_handle(struct usb_host *host)
{
	static const struct usb_hook_pattern pat_setconf = {
		.pid = USB_PID_SETUP,
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000900ULL,
		.offset = 0,
		.next = NULL
	};

	/* check a device class whenever SetConfigration() issued. */
	spinlock_lock(&host->lock_hk);
	usb_hook_register(host, USB_HOOK_REPLY, 
			  USB_HOOK_MATCH_ENDP | USB_HOOK_MATCH_DATA,
			  0, 0, &pat_setconf, init_hub_device,
			  NULL, NULL);
	spinlock_unlock(&host->lock_hk);

	printf("USB HUB Class handler registered.\n");

	return;
}

void 
hub_portdevice_register(struct usb_host *host, 
			u64 hub_port, struct usb_device *dev)
{
	struct usb_device *hubdev;

	for (hubdev = host->device; hubdev; hubdev = hubdev->next)
		if (hub_port == hubdev->portno) {
			dev->parent = hubdev;
			dprintft(3, "HUB(%02x): HUB PORT(%d) device "
				 "checked and registered.\n",
				 hubdev->devnum,
				(int)dev->portno & USB_PORT_MASK);
			break;
		}
	if (!hubdev)
		dprintft(1, "HUB(%02x): HUB device not found!?!?\n",
						 dev->devnum);

	return;
}
