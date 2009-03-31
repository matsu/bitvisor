/*
 * USB HUB Class handler
 */
#include <core.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_log.h"
#include "usb_hub.h"
#include "uhci.h"
#include "uhci_hook.h"

DEFINE_ZALLOC_FUNC(usb_hub_device);
DEFINE_ZALLOC_FUNC(device_list);
DEFINE_ZALLOC_FUNC(usb_device_handle);
DEFINE_GET_U16_FROM_SETUP_FUNC(wIndex);

void
port_change(int port, int *tmp)
{
	int i;

	for (i = 4; i >= 0; i--)
		tmp[i] = (port >> 3 * i) & 0x0007;

	return;
}

static int
usbhub_get_portno(struct usb_device *hubdev, struct usb_buffer_list *ub)
{
	u16 port;

	port = get_wIndex_from_setup(ub);
	return ((hubdev->portno << 3) + port);
}

static int
usbhub_set_portno(struct usb_host *usbhc, 
		  struct usb_request_block *urb, void *arg)
{
	int portno;
	u8 devadr;

	devadr = urb->address;
	portno = usbhub_get_portno(urb->dev, urb->shadow->buffers);
	if (portno) {
		usbhc->last_changed_port = portno;
		dprintft(3, "HUB(%02x): HUB PORT(%d) status connect.\n",
			 devadr, portno & 0x0007);
	}

	return UHCI_HOOK_PASS;
}

static int
usbhub_device_disconnect(struct usb_host *usbhc, 
			 struct usb_request_block *urb, void *arg)
{
	u8 devadr;
	int port, hub_port, tmp[5];
	struct usb_device *tmp_dev, *dev = (struct usb_device *)NULL;
	struct usb_hub_device *h_dev = (struct usb_hub_device *)NULL;
	struct device_list *d_list;

	devadr = urb->address;
	port = usbhub_get_portno(urb->dev, urb->shadow->buffers);
	if (port){
		dprintft(3, "HUB(%02x): HUB PORT(%d) "
			 "status disconnect.\n", devadr, port & 0x0007);
		hub_port = (port & 0xFFF8) >> 3;
		for(tmp_dev = usbhc->device; 
		    tmp_dev; tmp_dev = tmp_dev->next){
			if (tmp_dev->portno == port) {
				port_change(port, &tmp[0]);
				dprintft(1, "PORTNO %d-%d-%d-%d-%d: "
					 "USB device disconnect.\n",
					 tmp[4], tmp[3], 
					 tmp[2], tmp[1], tmp[0]);
				dev = tmp_dev;
			} else if (tmp_dev->portno == hub_port) {
				h_dev = (struct usb_hub_device *)
					tmp_dev->handle->private_data;
			}

			if (dev && h_dev)
				break;
		}

		if (!dev || !h_dev) 
			return UHCI_HOOK_PASS;

		for (d_list = h_dev->list; d_list; d_list = d_list->next)
			if (d_list->device == dev){
				if (d_list->next)
					d_list->next->prev = d_list->prev;

				if (d_list->prev)
					d_list->prev->next = d_list->next;
				else
					h_dev->list = d_list->next;

				free(d_list);
				break;
			}
		free_device(usbhc, dev);
	}

	return UHCI_HOOK_PASS;
}


static void 
usbhub_clean_list(struct usb_device *dev, struct device_list *list)
{
	struct device_list *tmp_list, *nxt_list;
	int tmp[5];

	for(tmp_list = list; tmp_list; tmp_list = nxt_list){
		port_change(tmp_list->device->portno, &tmp[0]);
		dprintft(1, "PORTNO %d-%d-%d-%d-%d: USB device "
			 "disconnect.\n", 
			 tmp[4], tmp[3], tmp[2], tmp[1], tmp[0]);

		nxt_list = tmp_list->next;
		free_device(dev->host, tmp_list->device);
		free(tmp_list);
	}

	return;
}


static void
usbhub_remove(struct usb_device *dev)
{
	struct usb_hub_device *hubdev;

	dprintft(1, "HUB(%02x): HUB device disconnect.\n", dev->devnum);

	if (!dev || !dev->handle){
		dprintft(1, "HUB(%02x): No device entry.\n", dev->devnum);
		return;
	}

	hubdev = (struct usb_hub_device *)dev->handle->private_data;
	free(dev->handle);
	dev->handle = NULL;

	if(!hubdev || !hubdev->list){
		dprintft(1, "HUB(%02x): No device handling entry.\n",
			 dev->devnum);
		return;
	}

	usbhub_clean_list(dev, hubdev->list);
	free(hubdev);

	return;
}


static int 
init_hub_device(struct usb_host *usbhc, 
		struct usb_request_block *urb, void *arg)
{
	u8 devadr, cls;
	struct usb_device *dev;
	struct usb_hub_device *hub_dev;
	struct usb_device_handle *handler;

	devadr = urb->address;
	dev = urb->dev;

	/* an interface descriptor must exists */
	if (!dev || !dev->config || !dev->config->interface || 
	    !dev->config->interface->altsetting) {
		dprintft(1, "HUB(%02x): interface descriptor not found.\n",
			 devadr);
		return UHCI_HOOK_PASS;
	}

	/* only Hub devices interests */
	cls = dev->config->interface->altsetting->bInterfaceClass;
	if (cls != 0x09)
		return UHCI_HOOK_PASS;
	
	dprintft(1, "HUB(%02x): A Hub Class device found\n", devadr);

	if (dev->handle) {
		dprintft(1, "HUB(%02x): maybe reset.\n",  devadr);
		return UHCI_HOOK_PASS;
	}

	hub_dev = zalloc_usb_hub_device();
	handler = zalloc_usb_device_handle();

	if (!hub_dev){
		dprintft(1, "HUB(%02x): hub device can't use.\n", devadr);
		return UHCI_HOOK_DISCARD;
	}

	handler->private_data = (void *)hub_dev;
	handler->remove = usbhub_remove;
	dev->handle = handler;

	return UHCI_HOOK_PASS;
}

void 
usbhub_init_handle(struct uhci_host *host)
{
	const struct usb_hook_pattern pat_setconf = {
		.mask = 0x000000000000ffffULL,
		.pattern = 0x0000000000000900ULL,
		.offset = 0,
		.next = NULL
	};
	const struct usb_hook_pattern pat_conn = {
		.mask = 0x00000000ffffffffULL,
		.pattern = 0x000000000000140123ULL,
		.offset = 0,
		.next = NULL
	};
	const struct usb_hook_pattern pat_disconn = {
		.mask = 0x000000000000ffffffULL,
		.pattern = 0x000000000000100123ULL,
		.offset = 0,
		.next = NULL
	};

	/* Look a device class whenever SetConfigration() issued. */
	uhci_hook_register(host, USB_HOOK_REPLY, 
			   USB_HOOK_MATCH_ENDP | USB_HOOK_MATCH_DATA,
			   0, 0, &pat_setconf, init_hub_device);
	uhci_hook_register(host, USB_HOOK_REPLY, 
			   USB_HOOK_MATCH_ENDP | USB_HOOK_MATCH_DATA,
			   0, 0, &pat_conn, usbhub_set_portno);
	uhci_hook_register(host, USB_HOOK_REPLY, 
			   USB_HOOK_MATCH_ENDP | USB_HOOK_MATCH_DATA,
			   0, 0, &pat_disconn, usbhub_device_disconnect);

	printf("USB HUB Class handler registered.\n");

	return;
}

void 
hub_portdevice_register(struct uhci_host *host, 
			int hub_port, struct usb_device *dev)
{
        struct usb_device *tmp_dev;
	struct usb_hub_device *hub;
	struct device_list *d_list;

	for (tmp_dev = host->hc->device; tmp_dev; tmp_dev = tmp_dev->next)
		if (hub_port == tmp_dev->portno){
			hub = (struct usb_hub_device*)
				tmp_dev->handle->private_data;
			d_list = zalloc_device_list();
			d_list->device = dev;

			if (!hub->list){
				d_list->prev = (struct device_list *)NULL;
			} else {
				d_list->prev = hub->list->prev;
				hub->list->prev = d_list;
			}
			d_list->next = hub->list;
			hub->list = d_list;
			dprintft(3, "HUB(%02x): HUB PORT(%d) device "
				 "checked and registered.\n",
				 tmp_dev->devnum, dev->portno & 0x0007);
			break;
		}
	if (!tmp_dev)
		dprintft(1, "HUB(%02x): HUB device not found!?!?\n",
			 dev->devnum);

	return;
}
