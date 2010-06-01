/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

enum {
	IDMAN_MSG_ENCRYPTBYINDEX,
	IDMAN_MSG_IPINITIALIZEREADER,
	IDMAN_MSG_IPINITIALIZE,
	IDMAN_MSG_IPFINALIZE,
	IDMAN_MSG_IPFINALIZEREADER,
	IDMAN_MSG_CHECKCARDSTATUS,
};

enum {
	USB_MSG_BULK_READ,
	USB_MSG_BULK_WRITE,
	USB_MSG_CLAIM_INTERFACE,
	USB_MSG_CLOSE,
	USB_MSG_CONTROL_MSG,
	USB_MSG_FIND_BUSSES,
	USB_MSG_FIND_DEVICES,
	USB_MSG_GET_BUSSES,
	USB_MSG_GET_STRING_SIMPLE,
	USB_MSG_INIT,
	USB_MSG_OPEN,
	USB_MSG_RELEASE_INTERFACE,
	USB_MSG_RESET,
	USB_MSG_CLOSEALL,
};

struct idman_msg_encryptbyindex {
	unsigned long int SessionHandle;
	int PkcxIndex;
	unsigned char datad[1024];
	unsigned short int dataLen;
	unsigned short int signatureLend;
	int algorithm;
	int retval;
};

struct idman_msg_ipinitializereader {
	int retval;
};

struct idman_msg_ipinitialize {
	char PINd[256];
	unsigned long int SessionHandled;
	int retval;
};

struct idman_msg_ipfinalize {
	unsigned long int SessionHandle;
	int retval;
};

struct idman_msg_ipfinalizereader {
	int retval;
};

struct idman_msg_checkcardstatus {
	unsigned long int SessionHandle;
	int retval;
};

struct usb_msg_bulk_read {
	int handle;
	int ep;
	int timeout;
	int retval;
};

struct usb_msg_bulk_write {
	int handle;
	int ep;
	int timeout;
	int retval;
};

struct usb_msg_claim_interface {
	int handle;
	int interface;
	int retval;
};

struct usb_msg_close {
	int handle;
	int retval;
};

struct usb_msg_control_msg {
	int handle;
	int requesttype;
	int request;
	int value;
	int index;
	int timeout;
	int retval;
};

struct usb_msg_find_busses {
	int retval;
};

struct usb_msg_find_devices {
	int retval;
};

struct usb_msg_get_busses {
	int err;
};

struct usb_msg_get_string_simple {
	int handle;
	int index;
	int retval;
};

struct usb_msg_open {
	struct usb_device *dev;
	int retval;
};

struct usb_msg_release_interface {
	int handle;
	int interface;
	int retval;
};

struct usb_msg_reset {
	int handle;
	int retval;
};

int idman_user_init (struct config_data_idman *idman, char *seed, int len);
