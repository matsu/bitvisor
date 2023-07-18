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

#include <core/initfunc.h>
#include "current.h"
#include "initipi.h"
#include "int.h"
#include "sx_handler.h"

unsigned int
initipi_get_count (void)
{
	unsigned int r;

	/* Since the gs_init_count is CPU-thread local, no bus lock is
	 * needed.  However the gs_init_count may be incremented by
	 * the #SX handler while this function is running.  Use inline
	 * asm to update the gs_init_count properly even if the #SX
	 * handler is called between these instructions. */
	asm volatile ("movl %%gs:gs_init_count, %0\n"
		      "subl %0, %%gs:gs_init_count"
		      : "=&r" (r) : : "cc", "memory");
	return r;
}

/* On Intel processors, an INIT signal is handled as a VM exit.  This
 * function is called during handling the VM exit. */
void
initipi_inc_count (void)
{
	asm volatile ("incl %%gs:gs_init_count" : : : "cc", "memory");
}

static void
initipi_init_pcpu (void)
{
	/* #SX exception is only available on AMD processors.  The
	   sx_handler might be called on Intel processors as an
	   external interrupt.  The sx_handler jumps to the
	   int_handler if it is an interrupt, not an exception,
	   detected by the exception error code. */
	set_int_handler (EXCEPTION_SX, sx_handler);
	initipi_get_count ();	/* Clear gs_init_count */
}

static unsigned int
get_init_count (void)
{
	return 0;
}

static void
initipi_init (void)
{
	current->initipi.get_init_count = get_init_count;
}

INITFUNC ("pcpu0", initipi_init_pcpu);
INITFUNC ("vcpu0", initipi_init);
