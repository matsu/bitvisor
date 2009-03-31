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

#include <stdio.h>
#include <usb.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "boot.h"
#include "usbmem.h"

static void hexdump(FILE *fp, const char *title, const unsigned char *s, int l)
{
	int n;

	fprintf(fp, "%s",title);
	for (n=0; n<l; ++n)
	{
		if((n%16) == 0)
			fprintf(fp, "\n%04d",n);
		fprintf(fp, " %02x",s[n]);
	}
	fprintf(fp, "\n");
}

// komote-usb
typedef struct _cbw_packet {
	unsigned int		signature;
	unsigned int		tag;
	unsigned int		dataTransferLength;
	unsigned char	flags;
	unsigned char	lun;
	unsigned char	comLength;
	unsigned char	comData[16];  // SISI Command Data
} cbw_packet;

// komote-usb
void cbw_scsi_read_capacity(cbw_packet* packet)
{
	packet->signature = 0x43425355;
	packet->tag = -37;
	packet->dataTransferLength = 8;
	packet->flags = 0x80;					// inquery data will flow in
	packet->comLength = 10;				// scsi command of size

	// SCSI Command Packet
	packet->comData[0] = 0x25;			// read capacity operation code
	packet->comData[1] = 0;								
	packet->comData[2] = 0;				// lba 1
	packet->comData[3] = 0;				// lba 2
	packet->comData[4] = 0;				// lba 3
	packet->comData[5] = 0;				// lba 4
	packet->comData[6] = 0;				// Reserved
	packet->comData[7] = 0;				// Reserved
	packet->comData[8] = 0;				// Reserved
	packet->comData[9] = 0;				// control
}

// komote-usb
void cbw_scsi_read_sector(cbw_packet* packet, int lba, int sectorSize, int sectorCount)
{
	packet->signature = 0x43425355;
	packet->tag = -40;
	packet->dataTransferLength = sectorSize;
	packet->flags = 0x80;										// read data will flow in
	packet->comLength = 10;									// scsi command of size

	// SCSI Command Packet
	packet->comData[0] = 0x28;								// read operation code
	packet->comData[1] = 0;								
	packet->comData[2] = (lba & 0xFF000000) >> 24;		// lba 1 (MSB)
	packet->comData[3] = (lba & 0xFF0000) >> 16;			// lba 2
	packet->comData[4] = (lba & 0xFF00) >> 8;			// lba 3
	packet->comData[5] = (lba & 0xFF);						// lba 4 (LSB)
	packet->comData[6] = 0;									// Reserved
	packet->comData[7] = (sectorCount & 0xFF00) >> 8;	// Transfer length MSB
	packet->comData[8] = (sectorCount & 0xFF);			// Transfer length LSB
	packet->comData[9] = 0;									// control
}


int main_usbmem()
{
	int i, j;
	int success = 0;
	int usb_num_bus = 0;
	int usb_num_dev = 0;
	int usb_open_status, usb_stat, sector_size, capacity;
	int ep_addr_in, ep_addr_out;
	int in_flag = 0;
	int out_flag = 0;
	int ret, fd;
	struct usb_bus *usb_bus;
	struct usb_device *usb_dev;
	struct usb_device *fnd_dev;
	struct usb_dev_handle *udev;
	unsigned char disk_shared_key[32];
	char vpn_shared_key[16];
	unsigned char key_header[32] = {0x73, 0x76, 0x6d, 0x2d, 0x75, 0x73, 0x62, 0x6b, 0x65, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	unsigned char usb_buf[512];
	unsigned char sector_data[512];
	unsigned char temp[1024];
	int found_device = 0;
	cbw_packet	*cbw = (cbw_packet*)malloc(sizeof(cbw_packet));
	usb_init();
	usb_num_bus = usb_find_busses();
	usb_num_dev = usb_find_devices();
	//printf("number of busses: %d\n", usb_num_bus);
	//printf("number of devices: %d\n", usb_num_dev);

	// komote-usb
	for (usb_bus = usb_busses; usb_bus; usb_bus = usb_bus->next)
	{
		for (usb_dev = usb_bus->devices; usb_dev; usb_dev = usb_dev->next)
		{
			for (i=0;i<4;i++)
			{
				if (usb_dev->config->interface->altsetting->endpoint[i].bEndpointAddress > 0x80)
				{
					if (in_flag == 0)
					{
						ep_addr_in = usb_dev->config->interface->altsetting->endpoint[i].bEndpointAddress;
						in_flag = 1;
					}
				}
				else
				{
					if (out_flag == 0)
					{
						ep_addr_out = usb_dev->config->interface->altsetting->endpoint[i].bEndpointAddress;
						out_flag = 1;
					}
				}
			}
			in_flag = 0;
			out_flag = 0;

			//printf("(ep_in,ep_out) = (%x,%x)\n", ep_addr_in, ep_addr_out);
			//printf("USB's IDs are matched!!\n");
			fnd_dev = usb_dev;

			printf("%d\n", usb_dev->config->interface->altsetting->bInterfaceClass);
			if ( usb_dev->config->interface->altsetting->bInterfaceClass != USB_CLASS_MASS_STORAGE )
			{
				continue;
			}

			// Body
			sector_size = 8;
			udev = usb_open(fnd_dev);
			//printf("udev = %x\n", udev);

			if (udev)
			{
				// Setting
				memset(usb_buf, 0, sizeof(usb_buf));
				usb_open_status = usb_get_driver_np(udev, 0, usb_buf, sizeof(usb_buf));
				if (usb_open_status == 0)
				{
					//printf("interface 0 already claimed by driver: %s\n", usb_buf);
					usb_detach_kernel_driver_np(udev, usb_dev->config->interface->altsetting->bInterfaceNumber);
				}
				usb_open_status = usb_set_configuration(udev, usb_dev->config->bConfigurationValue);
				//printf("usb_set: %d\n", usb_open_status);
				usb_open_status = usb_claim_interface(udev, usb_dev->config->interface->altsetting->bInterfaceNumber);
				//printf("usb_set: %d\n", usb_open_status);
				usb_open_status = usb_set_altinterface(udev, usb_dev->config->interface->altsetting->bInterfaceNumber);
				//printf("usb_set: %d\n", usb_open_status);

				for (j=0;j<3;j++)
				{
					// make CBW Command (read_capacity)
					memset(usb_buf, 0, sizeof(usb_buf));
					cbw_scsi_read_capacity(cbw);

					// Sending (CBW)
					usb_stat = usb_bulk_write(udev, ep_addr_out, (char *)cbw, 31, 100);
					//printf("CBW: %d\n", usb_stat);
					//hexdump(stdout, "contents:", (char *)cbw, 31);

					// Receiving (DATA)
					capacity = usb_bulk_read(udev, ep_addr_in, (char *)usb_buf, 8, 100);
					printf("Capacity: %d\n", capacity);
					//hexdump(stdout, "contents:", (char *)usb_buf, 8);

					// Receiving (CSW)
					usb_stat = usb_bulk_read(udev, ep_addr_in, (char *)usb_buf, 13, 100);
					//printf("CSW: %d\n", usb_stat);
					//hexdump(stdout, "contents:", (char *)usb_buf, 13);

					if ( capacity==8 )
					{
						success = 1;
						break;
					}
					else if ( capacity==-110 || capacity==0 )
					{
						continue;
					}
					else
					{
						break;
					}
				}

				if ( success==1 )
				{
					success = 0;
				}
				else
				{
					continue;
				}

				for (j=0;j<3;j++)
				{
					// make CBW Command (read_sector)
					memset(usb_buf, 0, sizeof(usb_buf));
					cbw_scsi_read_sector(cbw, 5, 512, 1);  //lba=5

					// Sending (CBW)
					usb_stat = usb_bulk_write(udev, ep_addr_out, (char *)cbw, 31, 100);
					//printf("CBW: %d\n", usb_stat);
					//hexdump(stdout, "contents:", (char *)cbw, 31);

					// Receiving (DATA)
					sector_size = usb_bulk_read(udev, ep_addr_in, (char *)usb_buf, 512, 100);
					memcpy(sector_data, usb_buf, 512);
					printf("Sector: %d\n", sector_size);
					//hexdump(stdout, "contents:", (char *)sector_data, 512);

					// Receiving (CSW)
					usb_stat = usb_bulk_read(udev, ep_addr_in, (char *)usb_buf, 13, 100);
					//printf("CSW: %d\n", usb_stat);
					//hexdump(stdout, "contents:", (char *)usb_buf, 13);

					if ( sector_size==512 )
					{
						success = 1;
						break;
					}
					else if ( sector_size==-110 || sector_size==0 )
					{
						continue;
					}
					else
					{
						break;
					}
				}

				if ( usb_stat != 0 )
				{
					//printf("error: %s \n",usb_strerror());
				}
				if ( usb_stat < 0 )
				{
					if (errno == EAGAIN)
					{
						//printf("error is eagain");
					}
				}
        
				usb_stat = usb_release_interface(udev, 0);
				//printf ("released: %d\n", usb_stat);
				usb_close (udev);
			}
			/* strncpy(temp, sector_data+32, 32); */
			if (!memcmp(sector_data+0,key_header,32) && success==1 )
			{
				memcpy(disk_shared_key, sector_data+32, 32);
				memcpy(vpn_shared_key, sector_data+64, 16);

				memcpy (config.storage.keys[0],
					disk_shared_key, 32);
				boot_guest ();
				return 0;

				hexdump(stdout, "disk_shared_key", disk_shared_key, 32);
				hexdump(stdout, "vpn_shared_key", vpn_shared_key, 16);

				return 0;
			}
			else
			{
				printf("error: No USB key ");
				printf("(ID %04x:%04x)\n", usb_dev->descriptor.idVendor, 
					usb_dev->descriptor.idProduct);
			}
			success = 0;
			memset(sector_data, 0, sizeof(sector_data));
			// Body

		}
	}

	return 0;
}
