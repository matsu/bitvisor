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

#define _WIN32_WINNT 0x500
#include <windows.h>
#include "../common/call_vmm.h"

static int
vmcall_iccard (void)
{
	call_vmm_function_t f;
	call_vmm_arg_t a;
	call_vmm_ret_t r;

	CALL_VMM_GET_FUNCTION ("iccard", &f);
	if (!call_vmm_function_callable (&f))
		return -1;
	call_vmm_call_function (&f, &a, &r);
	return (int)r.rax;
}

static void
timerproc (HWND hwnd)
{
	HANDLE t;
	LUID l;
	int r;

	r = vmcall_iccard ();
	if (r > 0) {
		LockWorkStation ();
#if 0
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
#endif
	}
	if (r < 0) {
		KillTimer (hwnd, 1);
		MessageBox (hwnd, "VMCALL failed", "Error", MB_OK);
		ExitProcess (0);
	}
}

static LRESULT CALLBACK
wndproc (HWND hwnd, UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	if (nMsg == WM_TIMER) {
		timerproc (hwnd);
		return 0;
	}
	return DefWindowProc (hwnd, nMsg, wParam, lParam);
}

int WINAPI
WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
	HWND hwnd;
	MSG msg;

	hwnd = CreateWindow (TEXT ("STATIC"), TEXT (""),
			     WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
			     CW_USEDEFAULT, CW_USEDEFAULT,
			     CW_USEDEFAULT, NULL, NULL, hInst, NULL);
	SetWindowLong (hwnd, GWL_WNDPROC, (LONG)wndproc);
	/* use the wndproc instead of a timer callback, because
	 * signal() did not work correctly in the timer callback. */
	SetTimer (hwnd, 1, 1000, NULL);
	while (GetMessage (&msg, NULL, 0, 0))
		DispatchMessage (&msg);
	return 0;
}
