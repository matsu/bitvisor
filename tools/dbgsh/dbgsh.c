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
#include <stdlib.h>
#ifdef __MINGW32__
#include <conio.h>
#define NOECHO()
#define GETCHAR() getch ()
#define PUTCHAR(c) putch (c)
#define ECHO()
#else
#define NOECHO() system ("stty -echo -icanon")
#define GETCHAR() getchar ()
#define PUTCHAR(c) putchar (c), fflush (stdout)
#define ECHO() system ("stty echo icanon")
#endif
#include "../common/call_vmm.h"

static int
vmcall_dbgsh (int c)
{
	call_vmm_function_t f;
	call_vmm_arg_t a;
	call_vmm_ret_t r;

	CALL_VMM_GET_FUNCTION ("dbgsh", &f);
	if (!call_vmm_function_callable (&f)) {
		fprintf (stderr, "vmmcall \"dbgsh\" failed\n");
		exit (1);
	}
	a.rbx = (long)c;
	call_vmm_call_function (&f, &a, &r);
	return (int)r.rax;
}

void
e (void)
{
	ECHO ();
}

int
main (int argc, char **argv)
{
	int s, r;
	FILE *fp;

	if (argc >= 2) {
		fp = fopen (argv[1], "w");
	} else {
		fp = NULL;
	}
	vmcall_dbgsh (-1);
	atexit (e);
	NOECHO ();
	s = -1;
	for (;;) {
		r = vmcall_dbgsh (s);
		if (r == (0x100 | '\n')) {
			vmcall_dbgsh (0);
			exit (0);
		}
		s = -1;
		if (r == 0) {
			s = GETCHAR ();
			if (s == 0)
				s |= 0x100;
		} else if (r > 0) {
			r &= 0xFF;
			if (fp) {
				fprintf (fp, "%c", r);
				fflush (fp);
			}
			PUTCHAR (r);
			s = 0;
		}
	}
}
