CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_VTD_TRANS) += -DVTD_TRANS

CONSTANTS-$(CONFIG_SHADOW_UHCI) += -DSHADOW_UHCI
CONSTANTS-$(CONFIG_HANDLE_USBMSC) += -DHANDLE_USBMSC
CONSTANTS-$(CONFIG_HANDLE_USBHUB) += -DHANDLE_USBHUB
CONSTANTS-$(CONFIG_CONCEAL_USBCCID) += -DCONCEAL_USBCCID
CONSTANTS-$(CONFIG_PS2KBD_F10USB) += -DF10USBTEST
CONSTANTS-$(CONFIG_PS2KBD_F12USB) += -DF12DUMPEHCI
CONSTANTS-y += -DENABLE_DPRINTF

CFLAGS += -Idrivers/include

objs-$(CONFIG_SHADOW_UHCI) += uhci.o uhci_debug.o uhci_shadow.o uhci_trans.o
objs-$(CONFIG_SHADOW_EHCI) += ehci.o ehci_debug.o ehci_shadow.o ehci_trans.o
objs-$(CONFIG_SHADOW_XHCI) += xhci.o xhci_shadow.o xhci_trans.o
objs-$(CONFIG_HANDLE_USBMSC) += usb_mscd.o
objs-$(CONFIG_HANDLE_USBHUB) += usb_hub.o
objs-$(CONFIG_CONCEAL_USBCCID) += usb_ccid.o
objs-y += usb.o usb_device.o usb_hid.o usb_hook.o usb_log.o
