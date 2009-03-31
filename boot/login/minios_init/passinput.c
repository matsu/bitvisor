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
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "passinput.h"

void
display_file (char *filename)
{
	FILE *fp;
	int c;

	fp = fopen (filename, "r");
	if (!fp)
		return;
	while ((c = getc (fp)) != EOF)
		putc (c, stdout);
	fflush (stdout);
	fclose (fp);
}

void
password_screen (void)
{
	display_file ("passscreen-1");
}

void
clear_screen (void)
{
	display_file ("passscreen-3");
}

void
putpass (char *string)
{
	display_file ("passscreen-2");
	printf ("%s", string);
	fflush (stdout);
}

void
get_password (char *buf, int len)
{
	char c;
	int r;
	int i;
	int width = 45;

	i = 0;
	for (;;) {
		r = read (0, &c, 1);
		if (r != 1)
			continue;
		switch (c) {
		case '\n':
		case '\r':
			break;
		case '\0':
			continue;
		case '\b':
		case '\177':
			if (i > 0) {
				if (i < width)
					putpass ("\b \b");
				i--;
			}
			continue;
		case '\25':
			while (i > 0) {
				if (i < width)
					putpass ("\b \b");
				i--;
			}
			continue;
		default:
			if (i + 1 < len) {
				buf[i] = c;
				i++;
				if (i < width)
					putpass ("*");
			}
			continue;
		}
		break;
	}
	buf[i] = '\0';
}

int
passinput (char *buf, int len)
{
	struct termios a, b;

	if (ioctl (0, TCGETS, &a)) {
		perror ("ioctl TCGETS");
		return 1;
	}
	b = a;
	a.c_iflag = 0;
	a.c_oflag = 0;
	a.c_lflag = 0;
	if (ioctl (0, TCSETS, &a)) {
		perror ("ioctl TCSETS");
		return 1;
	}
	password_screen ();
	get_password (buf, len);
	clear_screen ();
	if (ioctl (0, TCSETS, &b)) {
		perror ("ioctl TCSETS");
		return 1;
	}
	return 0;
}
