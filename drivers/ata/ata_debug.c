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

/**
 * @file	drivers/ata_debug.c
 * @brief	ATA debug driver
 * @author	T. Shinagawa
 */

#include "debug.h"
#include <core.h>
#include "security.h"
#include "ata.h"
#include "atapi.h"
#include "ata_debug.h"
#include "ata_time.h"

#define DEBUG_ATA_PUTLOG(fmt, name, value, title, ...) do		\
	{								\
		struct time_t cputime  = ata_get_time(0ULL);		\
		printf("ATA  : %06lld.%06lld  ata%02x %-8s %02xh %-32s" fmt, \
		       cputime.sec, cputime.us, ata_get_busid(channel),	\
		       name, value, title, __VA_ARGS__);		\
	} while(0)

#define DEBUG_ATA_PUTLOG2(fmt, ...) do					\
	{								\
		printf("ATA  :               ata%02x              " fmt, \
		       ata_get_busid(channel), __VA_ARGS__);		\
	} while(0)

#define DEBUG_ATA_CHECK_REPEAT(name, value)	\
	{					\
		u8 busid = ata_get_busid(channel);	\
		static typeof(value) last[16];		\
		static int repeat[16];			\
							\
		if (value == last[busid]) {		\
			repeat[busid]++;		\
			goto end;			\
		}					\
		last[busid] = value;			\
		if (repeat[busid] > 0)					\
			DEBUG_ATA_PUTLOG2("last %s repeated %d times\n", name, repeat[busid]);	\
		repeat[busid] = 0;					\
	}

static char *ata_cmd_name[256] = {
	[0x00] = "NOP",
	[0x08] = "DEVICE RESET",
	[0x10] = "RECALIBRATE",
	[0x20] = "READ SECTOR",
	[0x21] = "READ SECTOR NORETRY",
	[0x24] = "READ SECTOR EXT",
	[0x25] = "READ DMA EXT",
	[0x27] = "READ NATIVE MAX ADDRESS EXT",
	[0x29] = "READ SECTOR MULTIPLE EXT",
	[0x30] = "WRITE SECTOR",
	[0x34] = "WRITE SECTOR EXT",
	[0x35] = "WRITE DMA EXT",
	[0x39] = "WRITE SECTOR MULTIPLE EXT",
	[0x3D] = "WRITE DMA FUA EXT",
	[0x40] = "READ VERIFY SECTOR",
	[0x42] = "READ VERIFY SECTOR EXT",
	[0x91] = "INITIALIZE DEVICE PARAMETERS",
	[0xA0] = "PACKET",
	[0xA1] = "IDENTIFY PACKET DEVICE",
	[0xB0] = "SMART",
	[0xC4] = "READ SECTOR MULTIPLE",
	[0xC5] = "WRITE SECTOR MULTIPLE",
	[0xC6] = "SET MULTIPLE MODE",
	[0xC8] = "READ DMA",
	[0xCA] = "WRITE DMA",
	[0xE1] = "IDLE IMMEDIATE",
	[0xE0] = "STANDBY IMMEDIATE",
	[0xE3] = "IDLE",
	[0xE2] = "STANDBY",
	[0xE7] = "FLUSH CACHE",
	[0xEA] = "FLUSH CACHE EXT",
	[0xEC] = "IDENTIFY DEVICE",
	[0xEF] = "SET FEATURES",
	[0xF5] = "SECURITY FREEZE LOCK",
};

static char *atapi_cmd_name[256] = {
	[0x00] = "TEST UNIT READY",
	[0x03] = "REQUEST SENSE",
	[0x04] = "FORMAT UNIT",
	[0x12] = "INQUIRY",
	[0x15] = "MODE SELECT (6)",
	[0x1A] = "MODE SENSE (6)",
	[0x1B] = "START STOP UNIT",
	[0x1C] = "RECEIVE DIAGNOSTIC RESULTS",
	[0x1D] = "SEND DIAGNOSTIC RESULTS",
	[0x1E] = "PREVENT/ALLOW MEDIUM REMOVAL",
	[0x23] = "READ FORMAT CAPACITIES",
	[0x25] = "READ CAPACITY",
	[0x28] = "READ (10)",
	[0x2A] = "WRITE (10)",
	[0x2B] = "SEEK (10)",
	[0x2E] = "WRITE AND VERIFY (10)",
	[0x2F] = "VERIFY (10)",
	[0x35] = "SYNCHRONIZE CACHE",
	[0x3B] = "WRITE BUFFER",
	[0x3C] = "READ BUFFER",
	[0x43] = "READ TOC/PMA/ATIP",
	[0x46] = "GET CONFIGURATION",
	[0x4A] = "GET EVENT/STATUS NOTIFICATION",
	[0x4C] = "LOG SELECT",
	[0x4D] = "LOG SENSE",
	[0x51] = "READ DISC INFORMATION",
	[0x52] = "READ TRACK INFORMATION",
	[0x53] = "RESERVE TRACK",
	[0x54] = "SEND OPC INFORMATION",
	[0x55] = "MODE SELECT (10)",
	[0x58] = "REPAIR TRACK",
	[0x5A] = "MODE SENSE (10)",
	[0x5B] = "CLOSE TRACK SESSION",
	[0x5C] = "READ BUFFER CAPACITY",
	[0x5D] = "SEND CUE SHEET",
	[0x5E] = "PERSISTENT RESERVE IN",
	[0x5F] = "PERSISTENT RESERVE OUT",
	[0x83] = "EXTENDED COPY",
	[0x84] = "RECEIVE COPY RESULTS",
	[0x86] = "ACCESS CONTROL IN",
	[0x87] = "ACCESS CONTROL OUT",
	[0x8C] = "READ ATTRIBUTE",
	[0x8D] = "WRITE ATTRIBUTE",
	[0xA0] = "REPORT LUNS",
	[0xA1] = "BLANK",
	[0xA2] = "SECURITY PROTOCOL IN",
	[0xA3] = "*",
	[0xA4] = "*",
	[0xA6] = "LOAD/UNLOAD MEDIUM",
	[0xA7] = "SET READ AHEAD",
	[0xA8] = "READ (12)",
	[0xAA] = "WRITE (12)",
	[0xAB] = "*",
	[0xAC] = "GET PERFORMANCE",
	[0xAD] = "READ DISC STRUCTURE",
	[0xB5] = "SECURITY PROTOCOL OUT",
	[0xB6] = "SET STREAMING",
	[0xB9] = "READ CD MSF",
	[0xBB] = "SET CD SPEED",
	[0xBD] = "MECHANISM STATUS",
	[0xBE] = "READ CD",
	[0xBF] = "SEND DISC STRUCTURE",
};

typedef int (*debug_cmdblk_handler)(struct ata_channel *channel, core_io_t io, union mem *data);
extern debug_cmdblk_handler ata_cmdblk_handler_table[2][ATA_CMD_PORT_NUMS];
static debug_cmdblk_handler debug_saved_ata_handle_data; 
static debug_cmdblk_handler debug_saved_atapi_handle_interrupt_reason;
static debug_cmdblk_handler debug_saved_ata_handle_command;
static debug_cmdblk_handler debug_saved_ata_handle_status; 

static int debug_ata_handle_data(struct ata_channel *channel, core_io_t io, union mem *data)
{
	int ret, state = channel->state;
	int is_packet = (state == ATA_STATE_PIO_DATA && channel->pio_block_size <= 16);

	if (state == ATA_STATE_PIO_READY && channel->command != 0xA0)
		DEBUG_ATA_PUTLOG("%s %dbytes\n", "data", data->word, " ",
				 io.dir == CORE_IO_DIR_IN ? "in" : "out", channel->pio_block_size);

	ret = debug_saved_ata_handle_data(channel, io, data);

#ifdef DEBUG_ATAPI_PACKET
	if (is_packet && channel->pio_buf_index == 0 && channel->atapi_device->atapi_flag != 2) {
		int command = channel->pio_buf[0];
		channel->atapi_device->atapi_flag = 2;

		DEBUG_ATA_PUTLOG("[%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x]\n",
				 "packet", command, atapi_cmd_name[command],
				 channel->pio_buf[0], channel->pio_buf[1], channel->pio_buf[2], channel->pio_buf[3],
				 channel->pio_buf[4], channel->pio_buf[5], channel->pio_buf[6], channel->pio_buf[7],
				 channel->pio_buf[8], channel->pio_buf[9], channel->pio_buf[10], channel->pio_buf[11]
			);
	}
#endif
	return ret;
}



static int debug_atapi_handle_interrupt_reason(struct ata_channel *channel, core_io_t io, union mem *data)
{
	int ret;
	ata_interrupt_reason_t interrupt_reason;

	ret = debug_saved_atapi_handle_interrupt_reason(channel, io, data);

	if (channel->atapi_device->atapi_flag)
		goto end;
	interrupt_reason.value = data->byte;
#ifdef DEBUG_ATA_OMIT_REPEATED_INTERRUPT_REASON
	DEBUG_ATA_CHECK_REPEAT("interrupt reason", interrupt_reason.value);
#endif

	DEBUG_ATA_PUTLOG("cd=%d, io=%d, rel=%d, tag=%d\n", "int", data->byte, " ",
			 interrupt_reason.cd, interrupt_reason.io,
			 interrupt_reason.rel, interrupt_reason.tag);
end:
	return ret;
}

static int status_off[16] = {0};

static int debug_ata_handle_command(struct ata_channel *channel, core_io_t io, union mem *data)
{
	u8 hob, command = data->byte;
	u8 busid = ata_get_busid(channel);
	ata_lba_regs_t lba_regs[2];
	struct { u16 value; u8 hob[2]; } sector_count;

	if (command != 0xA0)
#ifdef DEBUG_ATA_OMIT_REPEATED_COMMAND
		DEBUG_ATA_CHECK_REPEAT("command", command);
#endif
	status_off[busid] = 0;
#ifdef DEBUG_ATA_OMIT_PACKET
	if (command == 0xA0)
		goto end2;
#endif

	for (hob = 0; hob < 2; hob++) {
		ata_set_hob(channel, hob);
		sector_count.hob[hob] = ata_read_reg(channel, ATA_SectorCount);
		lba_regs[hob].lba_low = ata_read_reg(channel, ATA_LBA_Low);
		lba_regs[hob].lba_mid = ata_read_reg(channel, ATA_LBA_Mid);
		lba_regs[hob].lba_high = ata_read_reg(channel, ATA_LBA_High);
		lba_regs[hob].device = ata_read_reg(channel, ATA_Device);
	}

	DEBUG_ATA_PUTLOG("[%02x %02x %02x %02x %02x][%02x %02x %02x %02x %02x]\n",
			 "command", command, ata_cmd_name[command],
			 sector_count.hob[0],
			 lba_regs[0].lba_low, lba_regs[0].lba_mid, lba_regs[0].lba_high, lba_regs[0].device,
			 sector_count.hob[1],
			 lba_regs[1].lba_low, lba_regs[1].lba_mid, lba_regs[1].lba_high, lba_regs[1].device
		);

	switch (command) {
	case 0x91: // INITIALIZE DEVICE PARAMETERS
	{
		u8 sectors;
		ata_device_reg_t device_reg;

		sectors = ata_read_reg(channel, ATA_SectorCount);
		device_reg = ata_read_device(channel);
		DEBUG_ATA_PUTLOG2("sectors=%d, heads=%d\n", sectors, device_reg.head + 1);
		break;
	}

	case 0xA0: // PACKET
	{
		struct atapi_features features;

		features.value = channel->features.hob[0];
		DEBUG_ATA_PUTLOG2("ovl=%d dma=%d\n", features.ovl, features.dma);
	}

	}
end2:
	return debug_saved_ata_handle_command(channel, io, data);

end:
	status_off[busid] = 1;
	goto end2;
}

static int debug_ata_handle_status(struct ata_channel *channel, core_io_t io, union mem *data)
{
	ata_status_t status;
	u8 busid = ata_get_busid(channel);

	if (status_off[busid] != 0)
		goto end;

	status = ata_read_status(channel);
#ifdef DEBUG_ATA_OMIT_REPEATED_STATUS
	DEBUG_ATA_CHECK_REPEAT("status", status.value);
#endif
	DEBUG_ATA_PUTLOG("err=%d, drq=%d, drdy=%d, bsy=%d\n",
			 "status", status.value, " ",
			 status.err, status.drq, status.drdy, status.bsy);
end:
	return debug_saved_ata_handle_status(channel, io, data);
}


static void debug_ata_init(void)
{
#define HOOK_HANDLER(handler, dir, regname)	\
	debug_saved_##handler = ata_cmdblk_handler_table[dir][regname]; \
	ata_cmdblk_handler_table[dir][regname] = debug_##handler;
	
#ifdef DEBUG_ATA_DATA
	HOOK_HANDLER(ata_handle_data, CORE_IO_DIR_IN, ATA_Data);
	HOOK_HANDLER(ata_handle_data, CORE_IO_DIR_OUT, ATA_Data);
#endif

#ifdef DEBUG_ATA_INTERRUPT_REASON
	HOOK_HANDLER(atapi_handle_interrupt_reason, CORE_IO_DIR_IN, ATAPI_InterruptReason);
#endif

#ifdef DEBUG_ATA_COMMAND
	HOOK_HANDLER(ata_handle_command, CORE_IO_DIR_OUT, ATA_Command);
#endif

#ifdef DEBUG_ATA_STATUS
	HOOK_HANDLER(ata_handle_status, CORE_IO_DIR_IN, ATA_Status);
#endif

	printf("ATA debugging mode is started\n");
	return;
}
DEBUG_DRIVER_INIT(debug_ata_init);


#ifdef DEBUG_ATA_BM_HANDLER
// Obsolete functions
int debug_ata_bm_handler(core_io_t io, union mem *data, void *arg);
int ata_bm_handler(core_io_t io, union mem *data, void *arg)
{
	int ret;
	struct ata_channel *channel = arg;
	u16 busid = channel->device[ata_get_dev(channel)].storage_device.bus.value;

	switch (io.port & 0x7) {
	case 0:
		if (io.dir == CORE_IO_DIR_OUT && data->byte & 0x01)
			printf("%s: busid=%04x: staring DMA: state=%s, data=%02x\n", __func__, busid, ata_state_name[channel->state], data->byte);
		break;

	case 4:
		if (io.dir == CORE_IO_DIR_OUT)
			printf("%s: busid=%04x: write PRD table pointer: %08x\n", __func__, busid, data->dword);
		break;
	}

	ret = debug_ata_bm_handler(io, data, arg);
	if (ret == CORE_IO_RET_DEFAULT) {
		core_io_handle_default(io, data);
		ret = CORE_IO_RET_DONE;
	}

	switch (io.port & 0x7) {
	case 2:
		if (io.dir == CORE_IO_DIR_IN && channel->state == ATA_STATE_READY)
			printf("%s: busid=%04x: status read: data=%02x\n", __func__, busid, data->byte);
		break;
	}
	return ret;
} 
#define ata_bm_handler debug_ata_bm_handler
#endif


#if 0
#undef DEBUG_ATA_CONFIG_WRITE

static inline void ata_print_config_rw(struct pci_device *pci_device, core_io_t io, u8 offset, union mem *data)
{
	printf("%s: %04x:%04x %02x:%08x => %02x:%08x\n", __func__,
	       pci_device->config_space.vendor_id, pci_device->config_space.device_id,
	       offset, data->dword, offset & 0xFC, pci_read_config_data_port());
}

#ifdef DEBUG_ATA_CONFIG_WRITE
int debug_ata_config_write(struct pci_device *pci_device, core_io_t io, u8 offset, union mem *data);
int ata_config_write(struct pci_device *pci_device, core_io_t io, u8 offset, union mem *data)
{
	ata_print_config_rw(pci_device, io, offset, data);
	return debug_ata_config_write(pci_device, io, offset, data);
}
#define ata_config_write debug_ata_config_write
#endif
#endif
