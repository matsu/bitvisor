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
	STORAGE_IO_INIT,
	STORAGE_IO_DEINIT,
	STORAGE_IO_GET_NUM_DEVICES,
	STORAGE_IO_AGET_SIZE,
	STORAGE_IO_AREADWRITE,
};

enum {
	STORAGE_IO_RGET_SIZE,
	STORAGE_IO_RREADWRITE,
};

struct storage_io_msg_init {
	int retval;
};

struct storage_io_msg_deinit {
	int id;
};

struct storage_io_msg_get_num_devices {
	int id;
	int retval;
};

struct storage_io_msg_aget_size {
	int id;
	int devno;
	void *callback;
	void *data;
	char msgname[32];
	int retval;
};

struct storage_io_msg_areadwrite {
	int id;
	int devno;
	int write;
	long long offset;
	void *callback;
	void *data;
	char msgname[32];
	int retval;
	void *buf;
	int len;
	void *tmpbuf;
};

struct storage_io_msg_rget_size {
	void (*callback) (void *data, long long size);
	void *data;
	long long size;
};

struct storage_io_msg_rreadwrite {
	void (*callback) (void *data, int len);
	void *data;
	int len;
	void *buf;
};

int storage_io_init (void);
void storage_io_deinit (int id);
int storage_io_get_num_devices (int id);
int storage_io_aget_size (int id, int devno,
			  void (*callback) (void *data, long long size),
			  void *data);
int storage_io_aread (int id, int devno, void *buf, int len, long long offset,
		      void (*callback) (void *data, int len), void *data);
int storage_io_awrite (int id, int devno, void *buf, int len, long long offset,
		       void (*callback) (void *data, int len), void *data);
