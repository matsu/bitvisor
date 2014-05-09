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

#ifndef _PCI_H
#define _PCI_H
#include <core.h>

struct idmask { u32 id, mask; };
#define idmask_match(a, b) ((a & b.mask) == b.id)

#define PCI_ID_ANY	0
#define PCI_ID_ANY_MASK	0

#define PCI_CONFIG_REGS8_NUM	256
#define PCI_CONFIG_REGS32_NUM	(PCI_CONFIG_REGS8_NUM / sizeof(u32))

// configuration address
typedef struct {
	union {
		u32 value;
		struct {
			unsigned int type:		2;
			unsigned int reg_no:		6;
			unsigned int func_no:		3;
			unsigned int device_no:		5;
			unsigned int bus_no:		8;
			unsigned int reserved:		7;
			unsigned int allow:		1;
		};
	};
} pci_config_address_t;

// configuration space
struct pci_config_space {
	union {
		u8 regs8[PCI_CONFIG_REGS8_NUM];
		u32 regs32[PCI_CONFIG_REGS32_NUM];
		struct {
			u16 vendor_id;
			u16 device_id;
			u16 command;
			u16 status;
			u8 revision_id;
			union {
				struct {
					unsigned int class_code:	24;
				} __attribute__ ((packed)) ;
				struct {
					u8 programming_interface;
					u8 sub_class;
					u8 base_class;
				} __attribute__ ((packed)) ;
			};
			u8  cacheline_size;
			u8  latency_timer;
			union {
				u8  header_type;
				struct {
					unsigned int type:		7;
					unsigned int multi_function:	1;
				} __attribute__ ((packed)) ;
			};
			u8  bist;
			u32 base_address[6];
			u32 cis;
			u16 sub_vendor_id;
			u16 sub_device_id;
			u32 ext_rom_base;
			u32 dummy[2];
			struct {
				u8 interrupt_line;
				u8 dummy2[3];
			} __attribute__ ((packed));
		} __attribute__ ((packed));
	};
};

#define PCI_CONFIG_BASE_ADDRESS_SPACEMASK	0x00000001
#define PCI_CONFIG_BASE_ADDRESS_MEMSPACE	0
#define PCI_CONFIG_BASE_ADDRESS_IOSPACE		1
#define PCI_CONFIG_BASE_ADDRESS_IOMASK		0x0000FFFC
#define PCI_CONFIG_BASE_ADDRESS_MEMMASK		0xFFFFFFF0

#define PCI_CONFIG_SPACE_GET_OFFSET(regname) offsetof(struct pci_config_space, regname)
#define PCI_CONFIG_ADDRESS_GET_REG_NO(regname) (offsetof(struct pci_config_space, regname) / sizeof(u32))

#define PCI_CONFIG_BASE_ADDRESS_NUMS 6
#define PCI_CONFIG_BASE_ADDRESS0 PCI_CONFIG_SPACE_GET_OFFSET(base_address[0])
#define PCI_CONFIG_BASE_ADDRESS1 PCI_CONFIG_SPACE_GET_OFFSET(base_address[1])
#define PCI_CONFIG_BASE_ADDRESS2 PCI_CONFIG_SPACE_GET_OFFSET(base_address[2])
#define PCI_CONFIG_BASE_ADDRESS3 PCI_CONFIG_SPACE_GET_OFFSET(base_address[3])
#define PCI_CONFIG_BASE_ADDRESS4 PCI_CONFIG_SPACE_GET_OFFSET(base_address[4])
#define PCI_CONFIG_BASE_ADDRESS5 PCI_CONFIG_SPACE_GET_OFFSET(base_address[5])
#define PCI_CONFIG_COMMAND PCI_CONFIG_SPACE_GET_OFFSET(command)

#define PCI_CONFIG_COMMAND_IOENABLE	0x1
#define PCI_CONFIG_COMMAND_MEMENABLE	0x2

struct pci_config_mmio_data;

// data structures
struct pci_device {
	LIST_DEFINE(pci_device_list);
	pci_config_address_t address;
	void *host;
	struct pci_driver *driver;
	struct pci_config_space config_space;
	u32 base_address_mask[PCI_CONFIG_BASE_ADDRESS_NUMS+1];
	u8 in_base_address_mask_emulation;
	bool conceal;
	struct pci_config_mmio_data *config_mmio;
};

struct pci_driver {
	LIST_DEFINE(pci_driver_list);
	struct idmask id;
	struct idmask class;
	void (*new)(struct pci_device *dev);
	int (*config_read) (struct pci_device *dev, u8 iosize, u16 offset,
			    union mem *data);
	int (*config_write) (struct pci_device *dev, u8 iosize, u16 offset,
			     union mem *data);
	struct {
		unsigned int use_base_address_mask_emulation: 1;
	} options;
	const char *name, *longname;
};

// exported functions
extern void pci_register_driver (struct pci_driver *driver);
extern void pci_handle_default_config_read (struct pci_device *pci_device,
					    u8 iosize, u16 offset,
					    union mem *data);
extern void pci_handle_default_config_write (struct pci_device *pci_device,
					     u8 iosize, u16 offset,
					     union mem *data);
extern u32  pci_read_config_data_port();
extern void pci_write_config_data_port(u32 data);

extern u8 pci_read_config_data8(pci_config_address_t addr, int offset);
extern u16 pci_read_config_data16(pci_config_address_t addr, int offset);
extern u32 pci_read_config_data32(pci_config_address_t addr, int offset);
extern void pci_write_config_data8(pci_config_address_t addr, int offset, u8 data);
extern void pci_write_config_data16(pci_config_address_t addr, int offset, u16 data);
extern void pci_write_config_data32(pci_config_address_t addr, int offset, u32 data);

struct pci_device *pci_possible_new_device (pci_config_address_t addr);
void pci_readwrite_config_mmio (struct pci_config_mmio_data *p, bool wr,
				uint bus_no, uint device_no, uint func_no,
				uint offset, uint iosize, void *data);
void pci_read_config_mmio (struct pci_config_mmio_data *p, uint bus_no,
			   uint device_no, uint func_no, uint offset,
			   uint iosize, void *data);
void pci_write_config_mmio (struct pci_config_mmio_data *p, uint bus_no,
			    uint device_no, uint func_no, uint offset,
			    uint iosize, void *data);

#endif
