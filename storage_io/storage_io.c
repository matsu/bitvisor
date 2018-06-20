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

#include <core.h>
#include <core/list.h>
#include <storage_io.h>
#include <core/process.h>
#include "storage_io_msg.h"

struct storage_hc {
	LIST1_DEFINE (struct storage_hc);
	struct storage_hc_driver *driver;
	struct storage_hc_hook *hook;
};

struct storage_hc_dev {
	LIST1_DEFINE (struct storage_hc_dev);
	struct storage_hc *handle;
	int port_no, dev_no;
};

struct storage_hc_driver {
	LIST1_DEFINE (struct storage_hc_driver);
	int index;
	struct storage_hc_addr addr;
	struct storage_hc_driver_func func;
	void *drvdata;
};

struct storage_hc_hook {
	LIST1_DEFINE (struct storage_hc_hook);
	storage_hc_hook_callback_t *callback;
	void *data;
};

struct storage_io_devices {
	LIST1_DEFINE (struct storage_io_devices);
	int devno;
	struct storage_hc *hc;
	struct storage_hc_dev *dev;
};

struct storage_io_get_num_devices_data {
	int *r;
	struct storage_hc *hc;
	int port_no;
	int n;
};

struct storage_io_aget_size_data {
	void (*callback) (void *data, long long size);
	void *data;
	struct storage_hc_dev *dev;
};

struct storage_io_areadwrite_data {
	void (*callback) (void *data, int len);
	void *data;
};
	
static spinlock_t handle_lock, driver_lock, dev_lock;
static rw_spinlock_t hook_lock;
static int num_driver;
static LIST1_DEFINE_HEAD (struct storage_hc, handle_list);
static LIST1_DEFINE_HEAD (struct storage_hc_driver, driver_list);
static LIST1_DEFINE_HEAD (struct storage_hc_hook, hook_list);
static LIST1_DEFINE_HEAD (struct storage_hc_dev, dev_list);
static int storage_io_desc;
static int storage_io_id;
static LIST1_DEFINE_HEAD (struct storage_io_devices, io_dev_list);

int
storage_get_num_hc (void)
{
	int ret;

	spinlock_lock (&driver_lock);
	ret = num_driver;
	spinlock_unlock (&driver_lock);
	return ret;
}

struct storage_hc *
storage_hc_open (int index, struct storage_hc_addr *addr,
		 struct storage_hc_hook *hook)
{
	struct storage_hc *handle;
	struct storage_hc_driver *driver;

	spinlock_lock (&driver_lock);
	LIST1_FOREACH (driver_list, driver) {
		if (index == driver->index)
			break;
	}
	if (driver) {
		if (addr)
			memcpy (addr, &driver->addr, sizeof *addr);
		handle = alloc (sizeof *handle);
		handle->driver = driver;
		handle->hook = hook;
		spinlock_lock (&handle_lock);
		LIST1_ADD (handle_list, handle);
		spinlock_unlock (&handle_lock);
	} else {
		handle = NULL;
	}
	spinlock_unlock (&driver_lock);
	return handle;
}

void
storage_hc_close (struct storage_hc *handle)
{
	spinlock_lock (&handle_lock);
	LIST1_DEL (handle_list, handle);
	spinlock_unlock (&handle_lock);
	free (handle);
}

struct storage_hc_hook *
storage_hc_hook_register (storage_hc_hook_callback_t *callback, void *data)
{
	struct storage_hc_hook *hook;

	hook = alloc (sizeof *hook);
	hook->callback = callback;
	hook->data = data;
	rw_spinlock_lock_ex (&hook_lock);
	LIST1_ADD (hook_list, hook);
	rw_spinlock_unlock_ex (&hook_lock);
	return hook;
}

void
storage_hc_hook_unregister (struct storage_hc_hook *hook)
{
	struct storage_hc *handle;

	rw_spinlock_lock_ex (&hook_lock);
	LIST1_DEL (hook_list, hook);
	rw_spinlock_unlock_ex (&hook_lock);
	spinlock_lock (&handle_lock);
	LIST1_FOREACH (handle_list, handle) {
		if (handle->hook == hook)
			handle->hook = NULL;
	}
	spinlock_unlock (&handle_lock);
	free (hook);
}

struct storage_hc_driver *
storage_hc_register (struct storage_hc_addr *addr,
		     struct storage_hc_driver_func *func, void *data)
{
	struct storage_hc_driver *new_driver, *driver;
	struct storage_hc_hook *hook;
	struct storage_hc *handle;
	int index;

	new_driver = alloc (sizeof *new_driver);
	memcpy (&new_driver->addr, addr, sizeof new_driver->addr);
	memcpy (&new_driver->func, func, sizeof new_driver->func);
	new_driver->drvdata = data;
	index = 0;
	spinlock_lock (&driver_lock);
	LIST1_FOREACH (driver_list, driver) {
		if (index != driver->index)
			break;
		index++;
	}
	new_driver->index = index;
	LIST1_INSERT (driver_list, driver, new_driver);
	num_driver++;
	spinlock_unlock (&driver_lock);
	rw_spinlock_lock_sh (&hook_lock);
	LIST1_FOREACH (hook_list, hook) {
		handle = alloc (sizeof *handle);
		handle->driver = new_driver;
		handle->hook = hook;
		spinlock_lock (&handle_lock);
		LIST1_ADD (handle_list, handle);
		spinlock_unlock (&handle_lock);
		hook->callback (hook->data, handle, &new_driver->addr);
	}
	rw_spinlock_unlock_sh (&hook_lock);
	return new_driver;
}

void
storage_hc_unregister (struct storage_hc_driver *driver)
{
	struct storage_hc_hook *hook;
	struct storage_hc *handle;

	spinlock_lock (&driver_lock);
	LIST1_DEL (driver_list, driver);
	num_driver--;
	spinlock_unlock (&driver_lock);
	do {
		hook = NULL;
		rw_spinlock_lock_sh (&hook_lock);
		spinlock_lock (&handle_lock);
		LIST1_FOREACH (handle_list, handle) {
			if (handle->driver == driver) {
				handle->driver = NULL;
				if (handle->hook) {
					hook = handle->hook;
					break;
				}
			}
		}
		spinlock_unlock (&handle_lock);
		if (hook)
			hook->callback (hook->data, handle, NULL);
		rw_spinlock_unlock_sh (&hook_lock);
	} while (hook);
	free (driver);
}

int
storage_hc_scandev (struct storage_hc *handle, int port_no,
		    storage_hc_scandev_callback_t *callback, void *data)
{
	struct storage_hc_driver *driver;

	spinlock_lock (&handle_lock);
	driver = handle->driver;
	spinlock_unlock (&handle_lock);
	if (driver)
		return driver->func.scandev (driver->drvdata, port_no,
					     callback, data);
	else
		return -1;
}

struct storage_hc_dev *
storage_hc_dev_open (struct storage_hc *handle, int port_no, int dev_no)
{
	struct storage_hc_driver *driver;
	struct storage_hc_dev *dev;

	spinlock_lock (&handle_lock);
	driver = handle->driver;
	spinlock_unlock (&handle_lock);
	if (driver) {
		if (!driver->func.openable (driver->drvdata, port_no, dev_no))
			return NULL;
		dev = alloc (sizeof *dev);
		dev->handle = handle;
		dev->port_no = port_no;
		dev->dev_no = dev_no;
		spinlock_lock (&dev_lock);
		LIST1_ADD (dev_list, dev);
		spinlock_unlock (&dev_lock);
		return dev;
	} else {
		return NULL;
	}
}

bool
storage_hc_dev_atacommand (struct storage_hc_dev *dev,
			   struct storage_hc_dev_atacmd *cmd, int cmdsize)
{
	struct storage_hc_driver *driver;

	spinlock_lock (&handle_lock);
	driver = dev->handle->driver;
	spinlock_unlock (&handle_lock);
	if (driver && driver->func.atacommand)
		return driver->func.atacommand (driver->drvdata, dev->port_no,
						dev->dev_no, cmd, cmdsize);
	else
		return false;
}

void
storage_hc_dev_close (struct storage_hc_dev *dev)
{
	spinlock_lock (&dev_lock);
	LIST1_DEL (dev_list, dev);
	spinlock_unlock (&dev_lock);
}

static void
storage_io_closeall (void)
{
	struct storage_io_devices *d;
	struct storage_hc *hc;

	LIST1_FOREACH (io_dev_list, d)
		storage_hc_dev_close (d->dev);
	hc = NULL;
	while ((d = LIST1_POP (io_dev_list))) {
		if (d->hc != hc) {
			storage_hc_close (d->hc);
			hc = d->hc;
		}
		free (d);
	}
}

int
storage_io_init (void)
{
	return ++storage_io_id;
}

void
storage_io_deinit (int id)
{
	storage_io_closeall ();
}

static bool
storage_io_get_num_devices_sub (void *data, int dev_no)
{
	struct storage_io_get_num_devices_data *d;
	struct storage_hc_dev *dev;
	struct storage_io_devices *p;

	d = data;
	dev = storage_hc_dev_open (d->hc, d->port_no, dev_no);
	if (dev) {
		p = alloc (sizeof *p);
		p->devno = (*d->r)++;
		p->hc = d->hc;
		p->dev = dev;
		LIST1_ADD (io_dev_list, p);
		d->n++;
	}
	return true;
}

int
storage_io_get_num_devices (int id)
{
	int r = 0;
	int n, i, j;
	struct storage_hc_addr addr;
	struct storage_hc *hc;
	struct storage_io_get_num_devices_data data;

	if (storage_io_id != id)
		return -1;
	storage_io_closeall ();
	n = storage_get_num_hc ();
	for (i = 0; i < n; i++) {
		hc = storage_hc_open (i, &addr, NULL);
		if (!hc)
			continue;
		data.hc = hc;
		data.r = &r;
		data.n = 0;
		for (j = 0; j < addr.num_ports; j++) {
			data.port_no = j;
			storage_hc_scandev (hc, j,
					    storage_io_get_num_devices_sub,
					    &data);
		}
		if (!data.n)
			storage_hc_close (hc);
	}
	return r;
}

static void
storage_io_aget_size_sub2 (void *data, struct storage_hc_dev_atacmd *cmd)
{
	struct storage_io_aget_size_data *arg;
	long long size;

	arg = data;
	if (cmd->timeout_ready < 0 || cmd->timeout_complete < 0) {
		arg->callback (arg->data, -1);
		free (arg);
		free (cmd);
		return;
	}
	size = cmd->cyl_high_exp;
	size = (size << 8) | cmd->cyl_low_exp;
	size = (size << 8) | cmd->sector_number_exp;
	size = (size << 8) | cmd->cyl_high;
	size = (size << 8) | cmd->cyl_low;
	size = (size << 8) | cmd->sector_number;
	size = (size + 1) * 512;
	arg->callback (arg->data, size);
	free (arg);
	free (cmd);
}

static void
storage_io_aget_size_sub1 (void *data, struct storage_hc_dev_atacmd *cmd)
{
	struct storage_io_aget_size_data *arg;

	arg = data;
	if (cmd->timeout_ready < 0 || cmd->timeout_complete < 0) {
		arg->callback (arg->data, -1);
		free (arg);
		free (cmd);
	} else if (cmd->cyl_low == 0 && cmd->cyl_high == 0) {
		memset (cmd, 0, sizeof *cmd);
		cmd->command_status = 0x27; /* READ NATIVE MAX ADDRESS EXT */
		cmd->pio = true;
		cmd->callback = storage_io_aget_size_sub2;
		cmd->data = arg;
		cmd->dev_head = 0x40;
		cmd->timeout_ready = 1000000;
		cmd->timeout_complete = 1000000;
		if (!storage_hc_dev_atacommand (arg->dev, cmd, sizeof *cmd)) {
			arg->callback (arg->data, -1);
			free (arg);
			free (cmd);
		}
	} else if (cmd->cyl_low == 0x14 && cmd->cyl_high == 0xEB) {
		arg->callback (arg->data, 0);
		free (arg);
		free (cmd);
	} else {
		arg->callback (arg->data, -1);
		free (arg);
		free (cmd);
	}
}

int
storage_io_aget_size (int id, int devno,
		      void (*callback) (void *data, long long size),
		      void *data)
{
	struct storage_io_devices *d;
	struct storage_hc_dev_atacmd *cmd;
	struct storage_io_aget_size_data *arg;

	if (storage_io_id != id)
		return -1;
	LIST1_FOREACH (io_dev_list, d) {
		if (d->devno == devno)
			break;
	}
	if (!d)
		return -1;
	cmd = alloc (sizeof *cmd);
	arg = alloc (sizeof *arg);
	arg->callback = callback;
	arg->data = data;
	arg->dev = d->dev;
	memset (cmd, 0, sizeof *cmd);
	cmd->command_status = 0x90; /* EXECUTE DEVICE DIAGNOSTIC */
	cmd->pio = true;
	cmd->callback = storage_io_aget_size_sub1;
	cmd->data = arg;
	cmd->dev_head = 0x40;
	cmd->timeout_ready = 1000000;
	cmd->timeout_complete = 1000000;
	if (!storage_hc_dev_atacommand (d->dev, cmd, sizeof *cmd)) {
		free (arg);
		free (cmd);
		return -1;
	}
	return 0;
}

static void
storage_io_areadwrite_sub (void *data, struct storage_hc_dev_atacmd *cmd)
{
	struct storage_io_areadwrite_data *arg;

	arg = data;
	if (cmd->timeout_ready < 0 || cmd->timeout_complete < 0) {
		arg->callback (arg->data, -1);
		free (arg);
		free (cmd);
		return;
	}
	arg->callback (arg->data, cmd->buf_len);
	free (arg);
	free (cmd);
}

int
storage_io_aread (int id, int devno, void *buf, int len, long long offset,
		  void (*callback) (void *data, int len), void *data)
{
	struct storage_io_devices *d;
	struct storage_hc_dev_atacmd *cmd;
	struct storage_io_areadwrite_data *arg;

	if (storage_io_id != id)
		return -1;
	if (offset & 511)
		return -1;
	if (len & 511)
		return -1;
	if (len <= 511)
		return -1;
	if (len >= 512 * 256)
		return -1;
	LIST1_FOREACH (io_dev_list, d) {
		if (d->devno == devno)
			break;
	}
	if (!d)
		return -1;
	cmd = alloc (sizeof *cmd);
	arg = alloc (sizeof *arg);
	arg->callback = callback;
	arg->data = data;
	memset (cmd, 0, sizeof *cmd);
	cmd->command_status = 0x25; /* READ DMA EXT */
	cmd->sector_count = len / 512;
	offset /= 512;
	cmd->sector_number = (offset >> 0) & 255;
	cmd->cyl_low = (offset >> 8) & 255;
	cmd->cyl_high = (offset >> 16) & 255;
	cmd->sector_number_exp = (offset >> 24) & 255;
	cmd->cyl_low_exp = (offset >> 32) & 255;
	cmd->cyl_high_exp = (offset >> 40) & 255;
	cmd->dev_head = 0x40;
	cmd->pio = false;
	cmd->callback = storage_io_areadwrite_sub;
	cmd->data = arg;
	cmd->buf = buf;
	cmd->buf_len = len;
	cmd->timeout_ready = 1000000;
	cmd->timeout_complete = 1000000;
	if (!storage_hc_dev_atacommand (d->dev, cmd, sizeof *cmd)) {
		free (arg);
		free (cmd);
		return -1;
	}
	return 0;
}

int
storage_io_awrite (int id, int devno, void *buf, int len, long long offset,
		   void (*callback) (void *data, int len), void *data)
{
	struct storage_io_devices *d;
	struct storage_hc_dev_atacmd *cmd;
	struct storage_io_areadwrite_data *arg;

	if (storage_io_id != id)
		return -1;
	if (offset & 511)
		return -1;
	if (len & 511)
		return -1;
	if (len <= 511)
		return -1;
	if (len >= 512 * 256)
		return -1;
	LIST1_FOREACH (io_dev_list, d) {
		if (d->devno == devno)
			break;
	}
	if (!d)
		return -1;
	cmd = alloc (sizeof *cmd);
	arg = alloc (sizeof *arg);
	arg->callback = callback;
	arg->data = data;
	memset (cmd, 0, sizeof *cmd);
	cmd->command_status = 0x35; /* WRITE DMA EXT */
	cmd->sector_count = len / 512;
	offset /= 512;
	cmd->sector_number = (offset >> 0) & 255;
	cmd->cyl_low = (offset >> 8) & 255;
	cmd->cyl_high = (offset >> 16) & 255;
	cmd->sector_number_exp = (offset >> 24) & 255;
	cmd->cyl_low_exp = (offset >> 32) & 255;
	cmd->cyl_high_exp = (offset >> 40) & 255;
	cmd->dev_head = 0x40;
	cmd->pio = false;
	cmd->callback = storage_io_areadwrite_sub;
	cmd->data = arg;
	cmd->buf = buf;
	cmd->buf_len = len;
	cmd->write = true;
	cmd->timeout_ready = 1000000;
	cmd->timeout_complete = 1000000;
	if (!storage_hc_dev_atacommand (d->dev, cmd, sizeof *cmd)) {
		free (arg);
		free (cmd);
		return -1;
	}
	return 0;
}

static void
aget_size_callback (void *data, long long size)
{
	struct storage_io_msg_aget_size *arg;
	struct storage_io_msg_rget_size *buf;
	struct msgbuf m;
	int d;

	arg = data;
	buf = alloc (sizeof *buf);
	buf->callback = arg->callback;
	buf->data = arg->data;
	buf->size = size;
	setmsgbuf (&m, buf, sizeof *buf, 0);
	d = msgopen (arg->msgname);
	if (d >= 0)
		msgsendbuf (d, STORAGE_IO_RGET_SIZE, &m, 1);
	msgclose (d);
	free (buf);
	free (arg);
}

static void
areadwrite_callback (void *data, int len)
{
	struct storage_io_msg_areadwrite *arg;
	struct storage_io_msg_rreadwrite *buf;
	struct msgbuf m[2];
	int d, n = 1;

	arg = data;
	buf = alloc (sizeof *buf);
	buf->callback = arg->callback;
	buf->data = arg->data;
	buf->len = len;
	buf->buf = arg->buf;
	setmsgbuf (&m[0], buf, sizeof *buf, 0);
	if (!arg->write) {
		setmsgbuf (&m[1], arg->tmpbuf, arg->len, 0);
		n = 2;
	}
	d = msgopen (arg->msgname);
	if (d >= 0)
		msgsendbuf (d, STORAGE_IO_RREADWRITE, m, n);
	msgclose (d);
	free (buf);
	free (arg);
}

static int
storage_io_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (c == STORAGE_IO_INIT) {
		struct storage_io_msg_init *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = storage_io_init ();
		return 0;
	} else if (c == STORAGE_IO_DEINIT) {
		struct storage_io_msg_deinit *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		storage_io_deinit (arg->id);
		return 0;
	} else if (c == STORAGE_IO_GET_NUM_DEVICES) {
		struct storage_io_msg_get_num_devices *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = storage_io_get_num_devices (arg->id);
		return 0;
	} else if (c == STORAGE_IO_AGET_SIZE) {
		struct storage_io_msg_aget_size *arg, *a;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		a = buf[0].base;
		arg = alloc (sizeof *arg);
		memcpy (arg, a, sizeof *arg);
		a->retval = storage_io_aget_size (arg->id, arg->devno,
						  aget_size_callback, arg);
		if (a->retval < 0)
			free (arg);
		return 0;
	} else if (c == STORAGE_IO_AREADWRITE) {
		struct storage_io_msg_areadwrite *arg, *a;

		if (bufcnt != 2)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		a = buf[0].base;
		arg = alloc (sizeof *arg);
		memcpy (arg, a, sizeof *arg);
		arg->tmpbuf = alloc (arg->len);
		if (arg->write) {
			memcpy (arg->tmpbuf, buf[1].base, arg->len);
			a->retval = storage_io_awrite (arg->id, arg->devno,
						       arg->tmpbuf, arg->len,
						       arg->offset,
						       areadwrite_callback,
						       arg);
		 } else {
			a->retval = storage_io_aread (arg->id, arg->devno,
						      arg->tmpbuf, arg->len,
						      arg->offset,
						      areadwrite_callback,
						      arg);
		}
		if (a->retval < 0) {
			free (arg->tmpbuf);
			free (arg);
		}
		return 0;
	} else {
		return -1;
	}
}

static void
storage_io_initfunc (void)
{
	spinlock_init (&handle_lock);
	spinlock_init (&driver_lock);
	rw_spinlock_init (&hook_lock);
	spinlock_init (&dev_lock);
	LIST1_HEAD_INIT (handle_list);
	LIST1_HEAD_INIT (driver_list);
	LIST1_HEAD_INIT (hook_list);
	LIST1_HEAD_INIT (dev_list);
	LIST1_HEAD_INIT (io_dev_list);
	storage_io_desc = msgregister ("storage_io", storage_io_msghandler);
	if (storage_io_desc < 0)
		panic ("register storage_io_desc");
}

INITFUNC ("driver2", storage_io_initfunc);
