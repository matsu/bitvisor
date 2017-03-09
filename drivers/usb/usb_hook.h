#ifndef __USB_HOOK_H__
#define __USB_HOOK_H__

#define USB_HOOK_PASS         0x00010000U
#define USB_HOOK_DISCARD      0x00020000U

#define USB_HOOK_REQUEST      0x01U
#define USB_HOOK_REPLY        0x02U

#define USB_HOOK_MATCH_NONE   0x00U
#define USB_HOOK_MATCH_ADDR   0x10U
#define USB_HOOK_MATCH_ENDP   0x20U
#define USB_HOOK_MATCH_DATA   0x40U
#define USB_HOOK_MATCH_DEV    0x80U

struct usb_hook_pattern {
	u8          pid;
	u32         offset;
	u64         mask;
	u64         pattern;
	struct usb_hook_pattern *next;
};

struct usb_hook {
	/* flags */
	u8         match;

	/* match patterns */
	u8         devadr;
	u8         endpt;
	const struct usb_hook_pattern *data;

	/* callback */
        int (*callback)(struct usb_host *host, 
			struct usb_request_block *urb,
			void *arg);
	void       *cbarg;
	int (*before_callback) (struct usb_host *host,
				struct usb_request_block *urb,
				void *arg);
	int (*after_callback) (struct usb_host *host,
			       struct usb_request_block *urb,
			       void *arg);
	u8         exec_once;

	/* target device */
	struct usb_device *dev; /* NULL if hc-wide hook */

	/* for making list */
	struct usb_hook *next;
};

DEFINE_LIST_FUNC(usb_hook, usb_hook);

int 
usb_hook_process(struct usb_host *host, 
		 struct usb_request_block *urb, int phase);
void *
usb_hook_register(struct usb_host *host, 
		  u8 phase, u8 match, u8 devadr, u8 endpt, 
		  const struct usb_hook_pattern *data,
		  int (*callback)(struct usb_host *, 
				  struct usb_request_block *,
				  void *),
		  void *cbarg,
		  struct usb_device *dev);

void
usb_hook_unregister(struct usb_host *host, int phase, void *handle);

void *usb_hook_register_ex (struct usb_host *host, 
			    u8 phase, u8 match, u8 devadr, u8 endpt, 
			    const struct usb_hook_pattern *data,
			    int (*callback) (struct usb_host *, 
					     struct usb_request_block *,
					     void *),
			    void *cbarg,
			    struct usb_device *dev,
			    int (*before_callback) (struct usb_host *,
						    struct usb_request_block *,
						    void *),
			    int (*after_callback) (struct usb_host *,
						   struct usb_request_block *,
						   void *),
			    u8 try_exec_first, u8 exec_once);

#endif /* __USB_HOOK_H__ */
