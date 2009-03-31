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
usbhub_init_handle(struct uhci_host *host);
void
hub_device_register(struct uhci_host *host, int hub_port,
                    struct usb_device *dev);
void
port_change(int port, int *tmp);
void
hub_portdevice_register(struct uhci_host *host, int hub_port, 
		    struct usb_device *dev);
#endif /* _USB_HUB_H */
