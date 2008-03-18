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

#include "asm.h"
#include "initfunc.h"
#include "io_io.h"
#include "process.h"
#include "serial.h"
#include "types.h"

#define PORT			0x3F8 /* COM1, ttyS0 */
#define NUM_OF_PORT		8
#define PORT_DATA		0x0
#define PORT_RATELSB		0x0
#define PORT_RATEMSB		0x1
#define PORT_INTR		0x1
#define PORT_FIFOCTL		0x2
#define PORT_LINECTL		0x3
#define PORT_MODEMCTL		0x4
#define PORT_LINESTATUS		0x5
#define PORT_MODEMSTATUS	0x6

#define RATELSB_115200		0x1
#define RATELSB_57600		0x2
#define RATELSB_38400		0x3
#define RATELSB_19200		0x6
#define RATELSB_9600		0xC
#define RATELSB_4800		0x18
#define RATELSB_2400		0x30
#define RATELSB_1200		0x60
#define RATELSB_600		0xC0
#define LINECTL_DATA8BIT	0x3
#define LINECTL_STOP2BIT	0x4
#define LINECTL_PARITY_ENABLE	0x8
#define LINECTL_PARITY_EVEN_BIT	0x10
#define LINECTL_PARITY		0x20
#define LINECTL_BREAK_BIT	0x40
#define LINECTL_DLAB_BIT	0x80
#define LINESTATUS_RX_BIT	0x1
#define LINESTATUS_OVERRUN_BIT	0x2
#define LINESTATUS_PARITY_BIT	0x4
#define LINESTATUS_FRAMING_BIT	0x8
#define LINESTATUS_BREAK_BIT	0x10
#define LINESTATUS_TXSTART_BIT	0x20
#define LINESTATUS_TX_BIT	0x40
#define LINESTATUS_RXFIFO_BIT	0x80

#define INIT_RATELSB		RATELSB_115200
#define INIT_RATEMSB		0x0
#define INIT_INTR		0x0
#define INIT_FIFOCTL		0x0
#define INIT_LINECTL		LINECTL_DATA8BIT
#define INIT_MODEMCTL		0x3 /* RTS | DTR */

static bool initialized;

static void
serial_init (unsigned int port)
{
	asm_outb (port + PORT_LINECTL, LINECTL_DLAB_BIT);
	asm_outb (port + PORT_RATELSB, INIT_RATELSB);
	asm_outb (port + PORT_RATEMSB, INIT_RATEMSB);
	asm_outb (port + PORT_LINECTL, INIT_LINECTL);
	asm_outb (port + PORT_MODEMCTL, INIT_MODEMCTL);
	asm_outb (port + PORT_INTR, INIT_INTR);
	asm_outb (port + PORT_FIFOCTL, INIT_FIFOCTL);
}

static void
serial_send (unsigned int port, unsigned char data)
{
	u8 status;

	asm_outb (port + PORT_DATA, data);
	do
		asm_inb (port + PORT_LINESTATUS, &status);
	while (!(status & LINESTATUS_TX_BIT));
}

static unsigned char
serial_recv (unsigned int port)
{
	u8 status, data;

	do
		asm_inb (port + PORT_LINESTATUS, &status);
	while (!(status & LINESTATUS_RX_BIT));
	asm_inb (port + PORT_DATA, &data);
	return data;
}

static int
serialin_msghandler (int m, int c)
{
	if (!initialized) {
		serial_init (PORT);
		initialized = true;
	}
	if (m == 0)
		return serial_recv (PORT);
	return 0;
}

static int
serialout_msghandler (int m, int c)
{
	if (!initialized) {
		serial_init (PORT);
		initialized = true;
	}
	if (m == 0)
		serial_send (PORT, (unsigned char)c);
	return 0;
}

void
serial_putDebugChar (int c)
{
	serialout_msghandler (0, c);
}

int
serial_getDebugChar (void)
{
	return serialin_msghandler (0, 0);
}

void
serial_putchar (unsigned char c)
{
	if (!initialized) {
		serial_init (PORT);
		initialized = true;
	}
	if (c == '\n')
		serial_send (PORT, '\r');
	else if (c == '\0')
		c = ' ';
	serial_send (PORT, c);
}

void
serial_init_iohook (void)
{
	unsigned int i;

	for (i = PORT; i < PORT + NUM_OF_PORT; i++)
		set_iofunc (i, do_io_nothing);
}

static void
serial_init_msg (void)
{
	initialized = false;
	msgregister ("serialin", serialin_msghandler);
	msgregister ("serialout", serialout_msghandler);
}

INITFUNC ("msg0", serial_init_msg);
