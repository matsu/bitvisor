#ifndef _USB_HUB_H
#include <core.h>
#include "uhci.h"

struct usb_hub_device {
	spinlock_t lock;
	struct device_list *list;
};

struct device_list {
	struct device_list *prev;
	struct device_list *next;
	struct usb_device *device;
};

void
usbhub_init_handle(struct usb_host *host);
void
hub_portdevice_register(struct usb_host *host, u64 hub_port, 
			struct usb_device *dev);
#endif /* _USB_HUB_H */
