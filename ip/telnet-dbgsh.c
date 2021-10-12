/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2014 Igel Co., Ltd
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

#include <core/config.h>
#include <core/debug.h>
#include <core/initfunc.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/process.h>
#include <core/thread.h>
#include "telnet-server.h"

static int
telnet_dbgsh_ttyin_msghandler (int m, int c)
{
	int tmp;
	static int cr;

	if (m == 0) {
		for (;;) {
			tmp = telnet_server_input ();
			if (tmp == '\n' && cr) {
				cr = 0;
			} else if (tmp >= 0) {
				if (tmp == '\r')
					cr = 1;
				else
					cr = 0;
				return tmp;
			}
			schedule ();
		}
	}
	return 0;
}

static void
telnet_dbgsh_output (int c)
{
	while (telnet_server_output (c) < 0)
		schedule ();
}

static int
telnet_dbgsh_ttyout_msghandler (int m, int c)
{
	if (m == 0) {
		if (c <= 0 || c > 0x100)
			c = 0x100;
		if (c == '\n')
			telnet_dbgsh_output ('\r');
		telnet_dbgsh_output (c);
	}
	return 0;
}

static void
telnet_dbgsh_thread (void *arg)
{
	int ttyin, ttyout;

	msgregister ("telnet_dbgsh_i", telnet_dbgsh_ttyin_msghandler);
	msgregister ("telnet_dbgsh_o", telnet_dbgsh_ttyout_msghandler);
	ttyin = msgopen ("telnet_dbgsh_i");
	ttyout = msgopen ("telnet_dbgsh_o");
	for (;;) {
		debug_shell (ttyin, ttyout);
		telnet_dbgsh_output (-1);
		schedule ();
	}
}

static void
telnet_dbgsh_init (void)
{
	if (config.vmm.telnet_dbgsh) {
		telnet_server_init ("dbgsh");
		thread_new (telnet_dbgsh_thread, NULL, VMM_STACKSIZE);
	}
}

INITFUNC ("driver2", telnet_dbgsh_init);
