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

/** メンバ名 */
#define		CRL				"CRL"
#define		TRUSTANCHORCERT	"TrustAnchorCert"
#define		RANDOMSEEDSIZE	"RandomSeedSize"
#define		MAXPINLEN		"MaxPinLen"
#define		MINPINLEN		"MinPinLen"

/** ハッシュアルゴリズム */
#define  ALGORITHM_SHA1			0x220			//SHA1ハッシュアルゴリズム
#define  ALGORITHM_SHA256		0x250			//SHA256ハッシュアルゴリズム
#define  ALGORITHM_SHA512		0x270			//SHA512ハッシュアルゴリズム

/** ハッシュレングス */
#define  HASH_LEN_SHA1			20				//SHA1ハッシュレングス
#define  HASH_LEN_SHA256		32				//SHA256ハッシュレングス
#define  HASH_LEN_SHA512		64				//SHA512ハッシュレングス

/** アルゴリズムRSAF4 */
#define  ALGORITHM_RSAF4		65537			//RSAF4アルゴリズム

#ifdef IDMAN_STDIO
	#define EXTERN
	#undef IDMAN_STDIO
#else
	#define EXTERN extern
#endif

typedef unsigned char ID_UNUM8;
typedef unsigned short ID_UNUM16;
typedef unsigned int ID_UNUM32;
typedef unsigned long long ID_UNUM64;
typedef unsigned long int ID_SIZE_T;

#define ID_PATH_MAX 4096

struct ID_USB_DEV_HANDLE {
  int fd;
  struct ID_USB_BUS *bus;
  struct ID_USB_DEVICE *device;
  int config;
  int interface;
  int altsetting;
  void *impl_info;
};
#if 0

struct ID_USB_BUS {
  struct ID_USB_BUS *next, *prev;
  char dirname[ID_PATH_MAX + 1];
  struct ID_USB_DEVICE *devices;
  ID_UNUM32 location;
  struct ID_USB_DEVICE *root_dev;
};

struct ID_USB_DEVICE_DESCRIPTOR {
	ID_UNUM8  bLength;
	ID_UNUM8  bDescriptorType;
	ID_UNUM16 bcdUSB;
	ID_UNUM8  bDeviceClass;
	ID_UNUM8  bDeviceSubClass;
	ID_UNUM8  bDeviceProtocol;
	ID_UNUM8  bMaxPacketSize0;
	ID_UNUM16 idVendor;
	ID_UNUM16 idProduct;
	ID_UNUM16 bcdDevice;
	ID_UNUM8  iManufacturer;
	ID_UNUM8  iProduct;
	ID_UNUM8  iSerialNumber;
	ID_UNUM8  bNumConfigurations;
};


struct ID_USB_DEVICE {
  struct ID_USB_DEVICE *next, *prev;
  char filename[ID_PATH_MAX + 1];
  struct ID_USB_BUS *bus;
  struct ID_USB_DEVICE_DESCRIPTOR descriptor;
  struct ID_USB_CONFIG_DESCRIPTOR *config;
  void *dev;
  ID_UNUM8 devnum;
  unsigned char num_children;
  struct ID_USB_DEVICE **children;
};

struct ID_USB_CONFIG_DESCRIPTOR {
	ID_UNUM8  bLength;
	ID_UNUM8  bDescriptorType;
	ID_UNUM16 wTotalLength;
	ID_UNUM8  bNumInterfaces;
	ID_UNUM8  bConfigurationValue;
	ID_UNUM8  iConfiguration;
	ID_UNUM8  bmAttributes;
	ID_UNUM8  MaxPower;
	struct ID_USB_INTERFACE *interface;
	unsigned char *extra;
	int extralen;
};

struct ID_USB_INTERFACE {
	struct ID_USB_INTERFACE_DESCRIPTOR *altsetting;
	int num_altsetting;
};

struct ID_USB_INTERFACE_DESCRIPTOR {
	ID_UNUM8  bLength;
	ID_UNUM8  bDescriptorType;
	ID_UNUM8  bInterfaceNumber;
	ID_UNUM8  bAlternateSetting;
	ID_UNUM8  bNumEndpoints;
	ID_UNUM8  bInterfaceClass;
	ID_UNUM8  bInterfaceSubClass;
	ID_UNUM8  bInterfaceProtocol;
	ID_UNUM8  iInterface;
	struct ID_USB_ENDPOINT_DESCRIPTOR *endpoint;
	unsigned char *extra;
	int extralen;
};

struct ID_USB_ENDPOINT_DESCRIPTOR {
	ID_UNUM8  bLength;
	ID_UNUM8  bDescriptorType;
	ID_UNUM8  bEndpointAddress;
	ID_UNUM8  bmAttributes;
	ID_UNUM16 wMaxPacketSize;
	ID_UNUM8  bInterval;
	ID_UNUM8  bRefresh;
	ID_UNUM8  bSynchAddress;
	unsigned char *extra;
	int extralen;
};

/** プロトタイプ宣言(libUSB系) */
EXTERN int IDMan_StUsbClose(struct ID_USB_DEV_HANDLE *);
EXTERN int IDMan_StUsbFindDevices(void);
EXTERN int IDMan_StUsbGetStringSimple(struct ID_USB_DEV_HANDLE *, int , char *,ID_SIZE_T );
EXTERN struct ID_USB_DEV_HANDLE *IDMan_StUsbOpen(struct ID_USB_DEVICE *);
EXTERN int IDMan_StUsbBulkWrite(struct ID_USB_DEV_HANDLE *, int , char *, int , int );
EXTERN int IDMan_StUsbReleaseInterface(struct ID_USB_DEV_HANDLE *, int );
EXTERN int IDMan_StUsbFindBusses(void);
EXTERN struct ID_USB_BUS *IDMan_StUsbGetBusses(void);
EXTERN void IDMan_StUsbInit(void);
EXTERN int IDMan_StUsbBulkRead(struct ID_USB_DEV_HANDLE *, int , char *, int , int );
EXTERN int IDMan_StUsbClaimInterface(struct ID_USB_DEV_HANDLE *, int );
EXTERN int IDMan_StUsbReset(struct ID_USB_DEV_HANDLE *);
EXTERN int IDMan_StUsbControlMsg(struct ID_USB_DEV_HANDLE *, int , int request, int , int , char *, int , int );

#else
#define ID_USB_DEV_HANDLE usb_dev_handle
#define ID_USB_BUS usb_bus
#define ID_USB_DEVICE_DESCRIPTOR usb_device_descriptor
#define ID_USB_DEVICE usb_device
#define ID_USB_CONFIG_DESCRIPTOR usb_config_descriptor
#define ID_USB_INTERFACE usb_interface
#define ID_USB_INTERFACE_DESCRIPTOR usb_interface_descriptor
#define ID_USB_ENDPOINT_DESCRIPTOR usb_endpoint_descriptor

#define IDMan_StUsbClose usb_close
#define IDMan_StUsbFindDevices usb_find_devices
#define IDMan_StUsbGetStringSimple usb_get_string_simple
#define IDMan_StUsbOpen usb_open
#define IDMan_StUsbBulkWrite usb_bulk_write
#define IDMan_StUsbReleaseInterface usb_release_interface
#define IDMan_StUsbFindBusses usb_find_busses
#define IDMan_StUsbGetBusses usb_get_busses
#define IDMan_StUsbInit usb_init
#define IDMan_StUsbBulkRead usb_bulk_read
#define IDMan_StUsbClaimInterface usb_claim_interface
#define IDMan_StUsbReset usb_reset
#define IDMan_StUsbControlMsg usb_control_msg

#endif

/** プロトタイプ宣言(glibc系) */
EXTERN void * IDMan_StMemcpy(void *, const void *, ID_SIZE_T );
EXTERN void * IDMan_StMemset(void *, int, ID_SIZE_T );
EXTERN int IDMan_StMemcmp(const void *, const void *, ID_SIZE_T );
EXTERN unsigned long int IDMan_StStrlen(const char *);
EXTERN char * IDMan_StStrcat(char * , const char * );
EXTERN char * IDMan_StStrchr(const char *, int );
EXTERN char * IDMan_StStrcpy(char * , const char * );
EXTERN int IDMan_StStrcmp(const char *, const char *);
EXTERN int IDMan_StAtoi(const char *);
EXTERN long int IDMan_StAtol(const char *);
EXTERN void IDMan_StMsleep(unsigned long int );



/** プロトタイプ宣言 */
EXTERN void *IDMan_StMalloc ( unsigned long int );
EXTERN void IDMan_StFree ( void * );
int IDMan_StReadSetData(char *pMemberName,char *pInfo,unsigned long int* len);
EXTERN long IDMan_CmMkHash ( void *, long, long, void *, long *);
EXTERN long IDMan_CmChgRSA(  void *, long , void *, long , void *, long * );
EXTERN long IDMan_CmGetDN( void *,long ,char *,char *);
EXTERN long IDMan_CmGetCertificateSign( void *,long ,void *,long *,void *pSign,long *);
EXTERN long IDMan_CmGetPublicKey ( void *,long ,void *,unsigned long *);
EXTERN long IDMan_CmSetFileRead( char *,char *,char *);
EXTERN long IDMan_CmMakeRand( long, void *);
EXTERN long IDMan_CmCheckCRL(void *, long , void * ,unsigned long int  );

#ifdef ZZZ
#undef EXTERN

#define	memcpy	IDMan_StMemcpy
#define	memset	IDMan_StMemset
#define	memcmp	IDMan_StMemcmp
#define	strlen	IDMan_StStrlen
#define	strcat	IDMan_StStrcat
#define	strchr	IDMan_StStrchr
#define	strcpy	IDMan_StStrcpy
#define	strcmp	IDMan_StStrcmp
#define	atoi	IDMan_StAtoi
#define	atol	IDMan_StAtol

#else /* ZZZ */
#undef strcpy
#undef atoi
#undef atol

#define strcpy  IDMan_StStrcpy
#define	atoi	IDMan_StAtoi
#define	atol	IDMan_StAtol

#endif /* ZZZ */

/* macros for -m64 */
#define DW_UL(DWVAL, CODE) \
	do { \
		unsigned long DWVAL##_TMP; \
		DWVAL##_TMP = *DWVAL; \
		{ \
			unsigned long *DWVAL; \
			DWVAL = &DWVAL##_TMP; \
			CODE \
		} \
		*DWVAL = DWVAL##_TMP; \
	} while (0)
#define UL_DW(ULVAL, CODE) \
	do { \
		DWORD ULVAL##_TMP; \
		ULVAL##_TMP = *ULVAL; \
		{ \
			DWORD *ULVAL; \
			ULVAL = &ULVAL##_TMP; \
			CODE \
		} \
		*ULVAL = ULVAL##_TMP; \
	} while (0)
