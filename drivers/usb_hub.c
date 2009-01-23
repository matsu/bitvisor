/*
 * USB HUB Class handler
 */
#if defined(USBHUB_HANDLE)

#include <core.h>
#include "uhci.h"
#include "usb.h"
#include "usb_hub.h"

DEFINE_ZALLOC_FUNC(usb_hub_device)
DEFINE_ZALLOC_FUNC(device_list);
DEFINE_ZALLOC_FUNC(usb_device_handle);
DEFINE_GET_U16_FROM_SETUP_FUNC(wIndex)
DEFINE_GET_U16_FROM_SETUP_FUNC(wValue)
DEFINE_GET_U16_FROM_SETUP_FUNC(bRequest)

void
port_change(int port, int *tmp)
{
	int i;

	for (i = 4; i >= 0; i--)
		tmp[i] = (port >> 3 * i) & 0x0007;

	return;
}

static int
usbhub_get_portno(struct uhci_host *host, struct uhci_td_meta *tdm)
{
	u8 devadr;
	u16 port;
	struct usb_device *dev;

	devadr = get_address_from_td(tdm->td);
	dev = get_device_by_address(host, devadr);

	if (!dev) {
		dprintft(1, "%04x: %s: HUB(%02x): hub port can't use. "
					"device not found.\n", host->iobase, 
					__FUNCTION__, devadr);
		return -1;
	}

	port = get_wIndex_from_setup(tdm->td);
	return ((dev->portno << 3) + port);
}

static int
usbhub_set_portno(struct uhci_host *host, struct uhci_hook *hook,
		    struct vm_usb_message *um, struct uhci_td_meta *tdm)
{
	int portno;
	u8 devadr;

	devadr = get_address_from_td(tdm->td);
	portno = usbhub_get_portno(host, tdm);
	if (portno) {
		host->last_change_port = portno;
		dprintft(3, "%04x: HUB(%02x): HUB PORT(%d) status connect.\n",
				host->iobase, devadr,portno & 0x0007);
	}

	return UHCI_HOOK_PASS;
}

static int
usbhub_device_disconnect(struct uhci_host *host, struct uhci_hook *hook,
		    struct vm_usb_message *um, struct uhci_td_meta *tdm)
{
	u8 devadr;
	int port, hub_port, tmp[5];
	struct usb_device *tmp_dev, *dev = (struct usb_device *)NULL;
	struct usb_hub_device *h_dev = (struct usb_hub_device *)NULL;
	struct device_list *d_list;

	devadr = get_address_from_td(tdm->td);
	port = usbhub_get_portno(host, tdm);
	if (port){
		dprintft(3, "%04x: HUB(%02x): HUB PORT(%d) status "
					"disconnect.\n",host->iobase,
					devadr, port & 0x0007);
		hub_port = (port & 0xFFF8) >> 3;
		for(tmp_dev = host->device; tmp_dev; tmp_dev = tmp_dev->next){
			if (tmp_dev->portno == port) {
				port_change(port, &tmp[0]);
				dprintft(1, "%04x: PORTNO %d-%d-%d-%d-%d: "
						"USB device disconnect.\n",
						host->iobase, tmp[4], tmp[3], 
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
		free_device(host, dev);
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
		dprintft(1, "%04x: PORTNO %d-%d-%d-%d-%d: USB device "
				"disconnect.\n", dev->host->iobase,
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

	dprintft(1, "%04x: HUB(%02x): HUB device disconnect.\n",
				   dev->host->iobase, dev->devnum);

	if (!dev || !dev->handle){
		dprintft(1, "%04x: HUB(%02x): No device entry.\n",
				   dev->host->iobase, dev->devnum);
		return;
	}

	hubdev = (struct usb_hub_device *)dev->handle->private_data;
	free(dev->handle);
	dev->handle = NULL;

	if(!hubdev || !hubdev->list){
		dprintft(1, "%04x: HUB(%02x): No device handling entry.\n",
				   dev->host->iobase, dev->devnum);
		return;
	}

	usbhub_clean_list(dev, hubdev->list);
	free(hubdev);

	return;
}


static int 
init_hub_device(struct uhci_host *host, struct uhci_hook *hook,
		    struct vm_usb_message *um, struct uhci_td_meta *tdm)
{
	u8 devadr, cls;
	struct usb_device *dev;
	struct usb_hub_device *hub_dev;
	struct usb_device_handle *handler;

	devadr = get_address_from_td(tdm->td);
	dev = get_device_by_address(host, devadr);

	/* an interface descriptor must exists */
	if (!dev || !dev->config || !dev->config->interface || 
	    !dev->config->interface->altsetting) {
		dprintft(1, "%04x: %s: interface descriptor not found.\n",
			 host->iobase, __FUNCTION__);
		return UHCI_HOOK_PASS;
	}

	/* only Hub devices interests */
	cls = dev->config->interface->altsetting->bInterfaceClass;
	if (cls != 0x09)
		return UHCI_HOOK_PASS;
	
	dprintft(1, "%04x: HUB(%02x): A Hub Class device found\n",
		 host->iobase, devadr);

	if (dev->handle) {
		dprintft(1, "%04x: HUB(%02x): maybe reset.\n", 
			 host->iobase, devadr);
		return UHCI_HOOK_PASS;
	}

	hub_dev = zalloc_usb_hub_device();
	handler = zalloc_usb_device_handle();

	if (!hub_dev){
		dprintft(1, "%04x: HUB(%02x): hub device can't use.\n", 
					 host->iobase, devadr);
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
	static struct uhci_hook_pattern pattern_setconfig[2] = {
		{
			.type = UHCI_PATTERN_32_TDTOKEN,
			.mask.dword = 0x000000ffU,
			.value.dword = UHCI_TD_TOKEN_PID_SETUP,
			.offset = 0
		},
		{
			.type = UHCI_PATTERN_64_DATA,
			.mask.qword = 0x000000000000ffffULL,
			.value.qword = 0x0000000000000900ULL,
			.offset = 0
		}
	};

	static struct uhci_hook_pattern h_pat_connect[2] = {
		{
			.type = UHCI_PATTERN_32_TDTOKEN,
			.mask.dword = 0x000000ffU,
			.value.dword = UHCI_TD_TOKEN_PID_SETUP,
			.offset = 0
		},
		{
			.type = UHCI_PATTERN_64_DATA,
			.mask.qword = 0x00000000ffffffffULL,
			.value.qword = 0x000000000000140123ULL,
			.offset = 0
		}
	};

	static struct uhci_hook_pattern h_pat_disconnect[2] = {
		{
			.type = UHCI_PATTERN_32_TDTOKEN,
			.mask.dword = 0x000000ffU,
			.value.dword = UHCI_TD_TOKEN_PID_SETUP,
			.offset = 0
		},
		{
			.type = UHCI_PATTERN_64_DATA,
			.mask.qword = 0x000000000000ffffffULL,
			.value.qword = 0x000000000000100123ULL,
			.offset = 0
		}
	};

	/* Look a device class whenever SetConfigration() issued. */
	uhci_register_hook(host, pattern_setconfig, 2, 
			   init_hub_device, UHCI_HOOK_POST);
	uhci_register_hook(host, h_pat_connect, 2,
			   usbhub_set_portno, UHCI_HOOK_POST);
	uhci_register_hook(host, h_pat_disconnect, 2, 
			   usbhub_device_disconnect, UHCI_HOOK_POST);

	printf("USB HUB Class handler registered.\n");

	return;
}

void 
hub_portdevice_register(struct uhci_host *host, int hub_port, 
		    struct usb_device *dev)
{
        struct usb_device *tmp_dev;
	struct usb_hub_device *hub;
	struct device_list *d_list;

	for (tmp_dev = host->device; tmp_dev; tmp_dev = tmp_dev->next)
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
			dprintft(3, "%04x: HUB(%02x): HUB PORT(%d) device "
					"checked and registered.\n",
					host->iobase, tmp_dev->devnum,
					dev->portno & 0x0007);
			break;
		}
	if (!tmp_dev)
		dprintft(1, "%04x: HUB(%02x): HUB device not found!?!?\n",
					host->iobase, dev->devnum);

	return;
}

#endif /* defined(USBMSC_HANDLE) */
