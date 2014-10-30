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

#define UNICODE
#define _WIN32_WINNT 0x500

#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include "../common/call_vmm.h"

#define UPDATE_ELAPSE	1000
#define TIMERID		1

static char buf[16384];
static TCHAR buf2[16384];

static int
vmcall_getstatus (char *buf, int len)
{
	call_vmm_function_t f;
	call_vmm_arg_t a;
	call_vmm_ret_t r;

	CALL_VMM_GET_FUNCTION ("get_status", &f);
	if (!call_vmm_function_callable (&f))
		return -1;
	a.rbx = (intptr_t)buf;
	a.rcx = (long)(len - 1);
	call_vmm_call_function (&f, &a, &r);
	if ((int)r.rax)
		return -1;
	buf[(int)r.rcx] = '\0';
	return 0;
}

static void
getstatus (TCHAR **st1, TCHAR **st2)
{
	int i = 0;

	*st1 = TEXT ("Unknown");
	*st2 = TEXT ("");
	ZeroMemory (buf, sizeof buf);
	if (vmcall_getstatus (buf, sizeof buf))
		return;
	*st1 = TEXT ("Running");
	do
		buf2[i] = (unsigned char)buf[i];
	while (buf[i++] != '\0');
	*st2 = buf2;
}

INT_PTR CALLBACK
status_dialog (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	TCHAR *status1, *status2;
	RECT rect;

	switch (msg) {
	case WM_INITDIALOG:
		SetTimer (hwnd, TIMERID, UPDATE_ELAPSE, NULL);
		return TRUE;
	case WM_COMMAND:
		switch (wparam) {
		case IDCANCEL:
			EndDialog (hwnd, 0);
			break;
		}
		return TRUE;
	case WM_TIMER:
		getstatus (&status1, &status2);
		SetWindowText (GetDlgItem (hwnd, IDC_STATUS1), status1);
		SetWindowText (GetDlgItem (hwnd, IDC_STATUS2), status2);
		SetWindowLongPtr (hwnd, DWLP_MSGRESULT, 0);
		return TRUE;
	case WM_SIZE:
		rect.left = 16;
		rect.top = 16;
		rect.right = 32;
		rect.bottom = 32;
		MapDialogRect (hwnd, &rect);
		MoveWindow (GetDlgItem (hwnd, IDC_STATUS1),
			    LOWORD (lparam) - rect.left * 4, rect.top,
			    rect.left * 3, rect.top, TRUE);
		MoveWindow (GetDlgItem (hwnd, IDC_STATUS2), rect.left,
			    rect.top * 3, LOWORD (lparam) - rect.left * 2,
			    HIWORD (lparam) - rect.top * 6, TRUE);
		MoveWindow (GetDlgItem (hwnd, IDCANCEL),
			    LOWORD (lparam) - rect.left * 4,
			    HIWORD (lparam) - rect.top * 2,
			    rect.left * 3, rect.top, TRUE);
		return TRUE;
	}
	return FALSE;
}

int WINAPI
WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
	HWND hwndp;

	InitCommonControls ();
	hwndp = CreateWindow (TEXT ("STATIC"), TEXT (""),
			      WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
			      CW_USEDEFAULT, CW_USEDEFAULT,
			      CW_USEDEFAULT, NULL, NULL, hInst, NULL);
	DialogBox (GetModuleHandle (NULL), TEXT ("STATUS"), hwndp,
		   status_dialog);
	return 0;
}
