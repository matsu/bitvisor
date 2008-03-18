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

#include <lib_lineinput.h>
#include <lib_printf.h>
#include <lib_string.h>
#include <lib_syscalls.h>

void
builtin_reboot ()
{
	int d;

	d = msgopen ("reboot");
	if (d >= 0) {
		msgsendint (d, 0);
		msgclose (d);
		printf ("Reboot failed.\n");
	} else
		printf ("reboot not found.\n");
}

int
_start (int a1, int a2)
{
	int d;
	char buf[100];

	for (;;) {
		printf ("> ");
		lineinput (buf, 100);
		if (!strcmp (buf, ""))
			continue;
		if (!strcmp (buf, "reboot"))
			builtin_reboot ();
		if (!strcmp (buf, "exit"))
			exitprocess (0);
		d = newprocess (buf);
		if (d >= 0) {
			msgsenddesc (d, 0);
			msgsenddesc (d, 1);
			msgsendint (d, 0);
			msgclose (d);
		} else
			printf ("%s not found.\n", buf);
	}
}
