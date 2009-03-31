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

#ifdef CARDSTATUS
#include "assert.h"
#include "config.h"
#include "current.h"
#include "iccard.h"
#include "initfunc.h"
#include "panic.h"
#include "printf.h"
#include "timer.h"
#include "vmmcall.h"

static enum {
	IS_OK,
	IS_NOCARD,
	IS_SHUTTING_DOWN,
} iccard_status;
static int shutdowntime;
static bool ready;

static void
iccard (void)
{
	if (iccard_status) {
		iccard_status = IS_SHUTTING_DOWN;
		current->vmctl.write_general_reg (GENERAL_REG_RAX, 1);
	} else {
		current->vmctl.write_general_reg (GENERAL_REG_RAX, 0);
	}
}

static void
iccard_timer (void *handle, void *data)
{
	int r;
	unsigned long idman_session;

	if (!config.vmm.iccard.status)
		return;
	if (!get_idman_session (&idman_session)) {
		timer_set (handle, 1000000);
		return;
	}
	if (!ready) {
		ready = true;
		printf ("Starting IC card status monitor\n");
	}
	if (!iccard_status) {
		r = IDMan_CheckCardStatus (idman_session);
		if (r) {
			shutdowntime = 0;
			iccard_status = IS_NOCARD;
			printf ("Stopping IC card status monitor\n");
		}
	}
	if (iccard_status) {
		shutdowntime++;
		if (shutdowntime > 10 && iccard_status == IS_NOCARD)
			panic ("no response from guest helper program");
		if (shutdowntime > 60)
			panic ("shutdown time out");
		timer_set (handle, 1000000);
	} else {
		timer_set (handle, 5000000);
	}
}

static void
vmmcall_iccard_init (void)
{
	void *handle;

	if (!config.vmm.iccard.status)
		return;
	iccard_status = IS_OK;
	ready = false;
	vmmcall_register ("iccard", iccard);
	handle = timer_new (iccard_timer, NULL);
	ASSERT (handle);
	timer_set (handle, 1000000);
}

INITFUNC ("driver5", vmmcall_iccard_init);
#endif

/************************************************************
#include <windows.h>

static int
vmcall_iccard (void)
{
	int n, r;

	asm volatile ("mov (%%ebx),%%eax; mov 4(%%ebx),%%eax; xor %%eax,%%eax;"
		      "vmcall" : "=a" (n) : "b" ("iccard\0"));
	if (!n)
		return -1;
	asm volatile ("vmcall" : "=a" (r)
		      : "a" (n));
	return r;
}

VOID CALLBACK
timerproc (HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	HANDLE t;
	LUID l;
	int r;

	r = vmcall_iccard ();
	if (r > 0) {
		if (OpenProcessToken (GetCurrentProcess(),
				      TOKEN_ADJUST_PRIVILEGES, &t)) {
			if (LookupPrivilegeValue (NULL, SE_SHUTDOWN_NAME,
						  &l)) {
				TOKEN_PRIVILEGES p = {
					.PrivilegeCount = 1,
					.Privileges = {
						{
							.Luid = l,
							.Attributes =
							SE_PRIVILEGE_ENABLED
						}
					}
				};
				AdjustTokenPrivileges (t, FALSE, &p,
						       0, NULL, NULL);
			}
			CloseHandle (t);
		}
		ExitWindowsEx (EWX_REBOOT | EWX_FORCE, 0);
		ExitProcess (0);
	}
	if (r < 0) {
		KillTimer (hwnd, 1);
		MessageBox (hwnd, "VMCALL failed", "Error", MB_OK);
		ExitProcess (0);
	}
}

int STDCALL
WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
	HWND hwnd;
	MSG msg;

	hwnd = CreateWindow (TEXT ("STATIC"), TEXT (""),
			     WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
			     CW_USEDEFAULT, CW_USEDEFAULT,
			     CW_USEDEFAULT, NULL, NULL, hInst, NULL);
	SetTimer (hwnd, 1, 1000, timerproc);
	while (GetMessage (&msg, NULL, 0, 0))
		DispatchMessage (&msg);
	return 0;
}
 ************************************************************/
