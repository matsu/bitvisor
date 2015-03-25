/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2010 Igel Co., Ltd
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

#include <core.h>
#include <core/arith.h>
#include <core/mmio.h>
#include <core/process.h>
#include <core/time.h>
#include <core/strtol.h>
#include "pci.h"

/* PCI I/O monitor driver

   Driver options:
   bar0=<access>|<access> <offset> <size>[ <access> <offset> <size>][ ...]
   bar1=<access>|<access> <offset> <size>[ <access> <offset> <size>][ ...]
   bar2=<access>|<access> <offset> <size>[ <access> <offset> <size>][ ...]
   bar3=<access>|<access> <offset> <size>[ <access> <offset> <size>][ ...]
   bar4=<access>|<access> <offset> <size>[ <access> <offset> <size>][ ...]
   bar5=<access>|<access> <offset> <size>[ <access> <offset> <size>][ ...]
   config=<access>|<access> <offset> <size>[ <access> <offset> <size>][ ...]
   nentries=<number of entries>
   time=<start time>|
     <start time> <end time>[ <start time> <end time>][ ...][ <start time>]
   overwrite=<yes/no>

   <access>: r|w|rw|R|W|Rw|rW|RW
     r: read
     w: write
     R: read, grouped
     W: write, grouped
   <offset>: start offset from BARn
   <size>: length in bytes

   Example:
   - monitor AHCI MMIO access
     device=ahci, driver=monitor, bar5=rw
   - monitor AHCI port 0 MMIO access
     device=ahci, driver=monitor, bar5=rw 0x100 0x17f
   - monitor AHCI configuration access for 10 seconds from the VM started
     device=ahci, driver=monitor, config=rw, time=0 10

   The monitor command output format:
   - No arguments: list of monitored devices
     <number>: <slot>
   - A number argument: I/O logs
     <time> <barN/conf> <mem/i/o/---> <r/w> <address> <offset> <b/w/l/q>
        <value> <group count if grouped>

   Example of extracting I/O logs from dbgsh:
   > monitor
   0: 00:1F.2/8086:2929
   1: 04:00.0/8086:444E
   monitor> 0
               10000000 bar5 mem r 0xF2926100 0x00000100 l 0x00000001
               10000010 bar5 mem w 0xF2926100 0x00000100 l 0x00000000
   ...
   monitor>
 */

/* Note: same structure in process/monitor.c */
struct pci_monitor_log {
	unsigned long long time;
	unsigned long long value;
	unsigned long long base;
	unsigned int offset;
	unsigned int count;
	unsigned char n, rw, type, len;
};

struct pci_monitor_options {
	struct pci_monitor_options *next;
	struct pci_monitor_host *host;
	u32 offset, size;
	int io;
	void *mmio;
	u8 r, w, n;
};

struct pci_monitor_time {
	struct pci_monitor_time *next;
	u64 time;
};

struct pci_monitor_host {
	struct pci_monitor_host *next;
	struct pci_device *pci_device;
	struct pci_monitor_time *time;
	struct pci_monitor_log *log;
	int nentries;
	int overwrite;
	spinlock_t lock;
	int nextindex, nsaved;
	struct pci_monitor_options *config_opt;
	struct pci_monitor_options *bar_opt[PCI_CONFIG_BASE_ADDRESS_NUMS];
	spinlock_t bar_lock[PCI_CONFIG_BASE_ADDRESS_NUMS];
	struct pci_bar_info bar[PCI_CONFIG_BASE_ADDRESS_NUMS];
};

static struct pci_monitor_host *host_list, **host_list_last;
static spinlock_t host_list_lock;

static void
pci_monitor_logging (struct pci_monitor_host *host, int mode, int n, int rw,
		     int type, u64 base, u32 offset, u32 size, void *data)
{
	struct pci_monitor_log *log;
	int initial_count = 0;
	u64 time;
	struct pci_monitor_time **t, *tt;

	time = get_time ();
	t = &host->time;
	if (*t) {
		for (;;) {
			if (time >= (*t)->time) {
				if (!(*t)->next || time < (*t)->next->time)
					break;
				tt = *t;
				*t = (*t)->next->next;
				free (tt->next);
				free (tt);
			} else {
				if (!(*t)->next)
					return;
				t = &(*t)->next->next;
			}
			if (!*t)
				return;
		}
	}
	spinlock_lock (&host->lock);
	switch (mode) {
	case 2:
		if (host->nsaved == host->nentries && !host->overwrite &&
		    !host->nextindex)
			break;
		if (host->nsaved > 0) {
			if (!host->nextindex)
				log = &host->log[host->nentries - 1];
			else
				log = &host->log[host->nextindex - 1];
			if (log->rw == rw && log->offset == offset &&
			    log->len == size && log->base == base &&
			    log->type == type && log->n == n) {
				log->count++;
				break;
			}
		}
		initial_count = 1;
		/* Fall through */
	case 1:
		if (host->nextindex == host->nentries)
			host->nextindex = 0;
		if (host->nsaved == host->nentries && !host->overwrite)
			break;
		log = &host->log[host->nextindex];
		log->time = time;
		log->offset = offset;
		log->base = base;
		log->type = type;
		log->len = size;
		log->rw = rw;
		log->n = n;
		log->value = 0;
		if (size > sizeof log->value)
			size = sizeof log->value;
		memcpy (&log->value, data, size);
		log->count = initial_count;
		if (host->nsaved != host->nentries)
			host->nsaved++;
		host->nextindex++;
	}
	spinlock_unlock (&host->lock);
}

static int
iohandler (core_io_t io, union mem *data, void *arg)
{
	struct pci_monitor_options *o = arg;
	struct pci_monitor_host *host = o->host;

	core_io_handle_default (io, data);
	if (o->r && io.dir == CORE_IO_DIR_IN)
		pci_monitor_logging (host, o->r, o->n, 0, 1,
				     host->bar[o->n].base, io.port -
				     host->bar[o->n].base, io.size, data);
	else if (o->w && io.dir == CORE_IO_DIR_OUT)
		pci_monitor_logging (host, o->w, o->n, 1, 1,
				     host->bar[o->n].base, io.port -
				     host->bar[o->n].base, io.size, data);
	return CORE_IO_RET_DONE;
}

static int
mmhandler (void *data, phys_t gphys, bool wr, void *buf, uint len, u32 flags)
{
	struct pci_monitor_options *o = data;
	struct pci_monitor_host *host = o->host;
	void *p;

	p = mapmem_gphys (gphys, len, (wr ? MAPMEM_WRITE : 0) | flags);
	ASSERT (p);
	if (wr)
		memcpy (p, buf, len);
	else
		memcpy (buf, p, len);
	unmapmem (p, len);
	if (o->r && !wr)
		pci_monitor_logging (host, o->r, o->n, 0, 2,
				     host->bar[o->n].base, gphys -
				     host->bar[o->n].base, len, buf);
	else if (o->w && wr)
		pci_monitor_logging (host, o->w, o->n, 1, 2,
				     host->bar[o->n].base, gphys -
				     host->bar[o->n].base, len, buf);
	return 1;
}

static void
pci_monitor_register_handler (struct pci_bar_info *bar,
			      struct pci_monitor_options *o)
{
	while (o) {
		if (bar->type == PCI_BAR_INFO_TYPE_IO) {
			printf ("pci_monitor: register i/o 0x%08llX"
				" 0x%08X\n", bar->base + o->offset, o->size);
			o->io = core_io_register_handler
				(bar->base + o->offset, o->size, iohandler, o,
				 CORE_IO_PRIO_EXCLUSIVE, "pci_monitor");
		} else if (bar->type == PCI_BAR_INFO_TYPE_MEM) {
			printf ("pci_monitor: register mem 0x%08llX"
				" 0x%08X\n", bar->base + o->offset, o->size);
			o->mmio = mmio_register (bar->base + o->offset,
						 o->size, mmhandler, o);
		}
		o = o->next;
	}
}

static void
pci_monitor_unregister_handler (struct pci_bar_info *bar,
				struct pci_monitor_options *o)
{
	while (o) {
		if (bar->type == PCI_BAR_INFO_TYPE_IO) {
			printf ("pci_monitor: unregister i/o 0x%08llX"
				" 0x%08X\n", bar->base + o->offset, o->size);
			core_io_unregister_handler (o->io);
		} else if (bar->type == PCI_BAR_INFO_TYPE_MEM) {
			printf ("pci_monitor: unregister mem 0x%08llX"
				" 0x%08X\n", bar->base + o->offset, o->size);
			mmio_unregister (o->mmio);
		}
		o = o->next;
	}
}

int
pci_monitor_config_read (struct pci_device *pci_device, u8 iosize, u16 offset,
			 union mem *data)
{
	struct pci_monitor_host *host = pci_device->host;
	struct pci_monitor_options *o;

	pci_handle_default_config_read (pci_device, iosize, offset, data);
	for (o = host->config_opt; o; o = o->next) {
		if (!o->r)
			continue;
		if (offset + iosize <= o->offset)
			continue;
		if (o->offset + o->size <= offset)
			continue;
		pci_monitor_logging (host, o->r, 6, 0, 0, 0, offset, iosize,
				     data);
		break;
	}
	return CORE_IO_RET_DONE;
}

int
pci_monitor_config_write (struct pci_device *pci_device, u8 iosize, u16 offset,
			  union mem *data)
{
	struct pci_monitor_host *host = pci_device->host;
	struct pci_monitor_options *o;
	struct pci_bar_info bar_info;
	int i;

	if (offset >= 0x10 + 4 * PCI_CONFIG_BASE_ADDRESS_NUMS)
		goto skip_bar_test;
	for (i = 0; i < PCI_CONFIG_BASE_ADDRESS_NUMS; i++) {
		if (offset + iosize - 1 < 0x10 + 4 * i)
			goto skip_bar_test;
		if (host->bar_opt[i] && offset < 0x10 + 4 * i + 4)
			break;
	}
	if (i == PCI_CONFIG_BASE_ADDRESS_NUMS)
		goto skip_bar_test;
	i = pci_get_modifying_bar_info (pci_device, &bar_info, iosize, offset,
					data);
	if (i >= 0 && host->bar[i].base != bar_info.base) {
		spinlock_lock (&host->bar_lock[i]);
		pci_monitor_unregister_handler (&host->bar[i],
						host->bar_opt[i]);
		pci_monitor_register_handler (&bar_info, host->bar_opt[i]);
		host->bar[i] = bar_info;
		spinlock_unlock (&host->bar_lock[i]);
	}
skip_bar_test:
	pci_handle_default_config_write (pci_device, iosize, offset, data);
	for (o = host->config_opt; o; o = o->next) {
		if (!o->w)
			continue;
		if (offset + iosize <= o->offset)
			continue;
		if (o->offset + o->size <= offset)
			continue;
		pci_monitor_logging (host, o->w, 6, 1, 0, 0, offset, iosize,
				     data);
		break;
	}
	return CORE_IO_RET_DONE;
}

static void
pci_monitor_parse_options (struct pci_monitor_host *host, int n, u32 len,
			   char *option, struct pci_monitor_options **opt)
{
	char *p;
	int r, w, f = 1;
	u32 offset, size;
	struct pci_monitor_options *o;

	*opt = NULL;
	if (!option)
		return;
loop:
	switch (*option++) {
	case 'r':
		r = 1;
		break;
	case 'R':
		r = 2;
		break;
	default:
		r = 0;
		option--;
	}
	switch (*option++) {
	case 'w':
		w = 1;
		break;
	case 'W':
		w = 2;
		break;
	default:
		w = 0;
		option--;
	}
	if (!r && !w)
		goto err;
	switch (*option) {
	case '\0':
		if (!f)
			goto err;
		offset = 0;
		size = len;
		break;
	case ' ':
		offset = strtol (++option, &p, 0);
		if (p == option || *p != ' ')
			goto err;
		option = p;
		size = strtol (++option, &p, 0);
		if (p == option || (*p != ' ' && *p != '\0'))
			goto err;
		option = p;
		break;
	default:
	err:
		panic ("pci_monitor: Invalid option %s", option);
	}
	if (offset >= len)
		panic ("pci_monitor: Invalid offset 0x%X", offset);
	if (offset + size <= offset || offset + size > len)
		panic ("pci_monitor: Invalid size 0x%X", size);
	o = alloc (sizeof *o);
	o->host = host;
	o->n = n;
	o->r = r;
	o->w = w;
	o->offset = offset;
	o->size = size;
	o->next = *opt;
	*opt = o;
	if (*option++ == '\0')
		return;
	f = 0;
	goto loop;
}

static void
pci_monitor_parse_time (char *option, struct pci_monitor_time **time)
{
	char *p;
	u64 value, tmp[2];

	*time = NULL;
	if (!option)
		return;
	do {
		value = strtol (option, &p, 0);
		if (option == p || (*p != '\0' && *p != ' '))
			panic ("pci_monitor: Invalid time %s", option);
		mpumul_64_64 (value, 1000000ULL, tmp);
		*time = alloc (sizeof **time);
		(*time)->time = tmp[0];
		(*time)->next = NULL;
		time = &(*time)->next;
		option = p + 1;
	} while (*p != '\0');
}

void
pci_monitor_new (struct pci_device *pci_device)
{
	int i;
	char *p;
	int nentries;
	struct pci_monitor_host *host;

	printf ("[%02X:%02X.%u/%04X:%04X] I/O monitor\n",
		pci_device->address.bus_no, pci_device->address.device_no,
		pci_device->address.func_no,
		pci_device->config_space.vendor_id,
		pci_device->config_space.device_id);
	nentries = pci_device->driver_options[7] ?
		strtol (pci_device->driver_options[7], &p, 0) : 0;
	if (p == pci_device->driver_options[7] || nentries <= 0)
		nentries = 1024;
	host = alloc (sizeof *host);
	host->next = NULL;
	host->nentries = nentries;
	host->log = alloc (sizeof *host->log * nentries);
	host->nextindex = 0;
	host->nsaved = 0;
	host->overwrite = 1;
	if (pci_device->driver_options[9] &&
	    !pci_driver_option_get_bool (pci_device->driver_options[9], NULL))
		host->overwrite = 0;
	pci_monitor_parse_time (pci_device->driver_options[8], &host->time);
	host->pci_device = pci_device;
	spinlock_init (&host->lock);
	pci_device->host = host;
	for (i = 0; i < PCI_CONFIG_BASE_ADDRESS_NUMS; i++) {
		spinlock_init (&host->bar_lock[i]);
		pci_get_bar_info (pci_device, i, &host->bar[i]);
		pci_monitor_parse_options (host, i, host->bar[i].len,
					   pci_device->driver_options[i],
					   &host->bar_opt[i]);
		pci_monitor_register_handler (&host->bar[i], host->bar_opt[i]);
	}
	pci_monitor_parse_options (host, 6, 0x10000,
				   pci_device->driver_options[6],
				   &host->config_opt);
	spinlock_lock (&host_list_lock);
	*host_list_last = host;
	host_list_last = &host->next;
	spinlock_unlock (&host_list_lock);
}

static struct pci_driver pci_monitor_driver = {
	.name		= "monitor",
	.longname	= "PCI device monitor",
	.device		= "id=:", /* this matches no devices */
	.new		= pci_monitor_new,
	.config_read	= pci_monitor_config_read,
	.config_write	= pci_monitor_config_write,
	.driver_options	= "bar0,bar1,bar2,bar3,bar4,bar5,config,nentries,time"
			  ",overwrite",
};

static int
pci_monitor_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	int r, i;
	struct pci_monitor_host *host;
	struct pci_device *pci_device;
	struct pci_monitor_log *log;

	if (m == MSG_INT && c == -1) {
		r = 0;
		spinlock_lock (&host_list_lock);
		for (host = host_list; host; host = host->next)
			r++;
		spinlock_unlock (&host_list_lock);
		return r;
	}
	spinlock_lock (&host_list_lock);
	for (host = host_list; host; host = host->next, c--)
		if (!c)
			break;
	spinlock_unlock (&host_list_lock);
	if (!host)
		return -1;
	if (m == MSG_INT)
		return host->nentries;
	if (m != MSG_BUF)
		return -1;
	pci_device = host->pci_device;
	r = -1;
	if (bufcnt >= 1 && buf[0].rw)
		r = snprintf (buf[0].base, buf[0].len,
			      "%02X:%02X.%u/%04X:%04X",
			      pci_device->address.bus_no,
			      pci_device->address.device_no,
			      pci_device->address.func_no,
			      pci_device->config_space.vendor_id,
			      pci_device->config_space.device_id);
	if (bufcnt >= 2 && buf[1].rw) {
		spinlock_lock (&host->lock);
		i = host->nextindex - host->nsaved;
		if (i < 0)
			i += host->nentries;
		log = buf[1].base;
		for (r = 0; r < host->nsaved; r++) {
			if (buf[1].len < (r + 1) * sizeof *log)
				break;
			memcpy (&log[r], &host->log[i], sizeof *log);
			i++;
			if (i == host->nentries)
				i = 0;
		}
		spinlock_unlock (&host->lock);
	}
	return r;
}

static void
pci_monitor_init_msg (void)
{
	spinlock_init (&host_list_lock);
	host_list_last = &host_list;
	msgregister ("pci_monitor", pci_monitor_msghandler);
}

static void
pci_monitor_init (void)
{
	pci_register_driver (&pci_monitor_driver);
}

PCI_DRIVER_INIT (pci_monitor_init);
INITFUNC ("msg0", pci_monitor_init_msg);
