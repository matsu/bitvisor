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
 * @file	drivers/usb_log.c
 * @brief	logging functions for USB drivers
 * @author	K. Matsubara
 */
#include <core.h>
#include <core/timer.h>

int usb_log_level = 1;

#if defined(ENABLE_DPRINTF)
int 
dprintf(int level, char *format, ...) 
{
	int ret = 0;
	va_list ptrs;

	if (level <= usb_log_level) {
		va_start(ptrs, format);
		ret = vprintf(format, ptrs);
		va_end(ptrs);
	}

	return ret;
}

int 
dprintft(int level, char *format, ...)
{
	int ret = 0;
	u64 t;
	static u64 lastt;
	va_list ptrs;

	if (level <= usb_log_level) {
		t = get_cpu_time();
		t = (t >> 8) & 0x0000000003ffffffULL;
		lastt = t;
		printf("[%8lld] ", t);
		va_start(ptrs, format);
		ret = vprintf(format, ptrs);
		va_end(ptrs);
	}

	return ret;
}
#endif /* ENABLE_DPRINTF */
