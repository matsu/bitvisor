/*
 * Copyright (c) 2017 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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

#include <EfiCommon.h>
#include <EfiApi.h>
#include "randseed.h"

char randseed[4096];
int randseedsize = sizeof randseed;

static int randseed_initialized;
static EFI_EVENT randseed_timer_event;

/* FIXME: Not enough entropy for cryptography */

static VOID EFIAPI
randseed_timer_callback (EFI_EVENT Event, VOID *Context)
{
	static int seedoff;
	uint32_t a, d;
	int i;

	asm volatile ("rdtsc" : "=a" (a), "=d" (d));
	for (i = 0; i < 4; i++) {
		randseed[seedoff] += a;
		seedoff = (seedoff + 1) % randseedsize;
		a >>= 8;
	}
}

EFI_STATUS
randseed_init (EFI_SYSTEM_TABLE *systab)
{
	EFI_STATUS status;

	if (randseed_initialized)
		return EFI_SUCCESS;
	status = systab->BootServices->CreateEvent (EFI_EVENT_TIMER |
						    EFI_EVENT_NOTIFY_SIGNAL,
						    EFI_TPL_NOTIFY,
						    randseed_timer_callback,
						    NULL,
						    &randseed_timer_event);
	if (EFI_ERROR (status))
		return status;
	status = systab->BootServices->SetTimer (randseed_timer_event,
						 TimerPeriodic, 0);
	if (EFI_ERROR (status)) {
		systab->BootServices->CloseEvent (randseed_timer_event);
		return status;
	}
	randseed_initialized = 1;
	return status;
}

EFI_STATUS
randseed_deinit (EFI_SYSTEM_TABLE *systab)
{
	EFI_STATUS status;

	if (!randseed_initialized)
		return EFI_SUCCESS;
	systab->BootServices->SetTimer (randseed_timer_event, TimerCancel, 0);
	status = systab->BootServices->CloseEvent (randseed_timer_event);
	randseed_initialized = 0;
	return status;
}

EFI_STATUS
randseed_event (EFI_SYSTEM_TABLE *systab)
{
	if (!randseed_initialized)
		return EFI_NOT_READY;
	return systab->BootServices->SignalEvent (randseed_timer_event);
}
