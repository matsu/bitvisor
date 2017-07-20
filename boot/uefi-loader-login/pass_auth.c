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

#include "pass_auth.h"
#include "randseed.h"

#define PASS_N_CHARS (4096)

#define PASS_BOX_COLS (70)
#define PASS_BOX_ROWS (9)

#define PASS_FIELD_COLS (66)

#define TXT_HEAD L"                            Enter password"
#define TXT_HINT L"                        Press Enter to submit"
#define TXT_INVALID L"                           Invalid password"
#define TXT_ERROR L"             Authentication fail, press any key to restart"

#define CHAR_REMOVE L"\x0008\x0020\x0008"

#define BACKGROUND	(EFI_BACKGROUND_BLACK)
#define BOX_BACKGROUND	(EFI_BACKGROUND_LIGHTGRAY)
#define BOX_TEXT	(EFI_BLACK | EFI_BACKGROUND_LIGHTGRAY)
#define BOX_INPUT	(EFI_BACKGROUND_BLUE)
#define BOX_INPUT_TEXT	(EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE)

static uint16_t pass[PASS_N_CHARS] = {0};
static uint16_t line[PASS_BOX_COLS + 1] = { 0 };
static uint16_t input_line[PASS_FIELD_COLS + 1] = { 0 };

static void
draw_password_box (EFI_SYSTEM_TABLE *systab, struct password_box *pwd_box,
		   CHAR16 *hint_txt)
{
	EFI_SIMPLE_TEXT_OUT_PROTOCOL *conout = systab->ConOut;

	int i;
	for (i = 0; i < pwd_box->cols; i++) {
		line[i] = ' ';
	}

	for (i = 0; i < PASS_FIELD_COLS; i++) {
		input_line[i] = ' ';
	}

	/* Draw box */
	conout->SetCursorPosition (conout,
				   pwd_box->col_offset,
				   pwd_box->row_offset);

	conout->SetAttribute (conout, BOX_BACKGROUND);

	UINTN row;
	for (row = 0; row < pwd_box->rows; row++) {
		conout->OutputString (conout, line);

		UINTN new_row = pwd_box->row_offset + row;
		conout->SetCursorPosition (conout,
					   pwd_box->col_offset,
					   new_row);
	}

	/* Print box title */
	conout->SetAttribute (conout, BOX_TEXT);

	conout->SetCursorPosition (conout,
				   pwd_box->col_offset,
				   pwd_box->row_offset + 1);

	conout->OutputString (conout, TXT_HEAD);

	/* Print hint */
	conout->SetCursorPosition (conout,
				   pwd_box->col_offset,
				   pwd_box->row_offset + 5);

	conout->OutputString (conout, hint_txt);

	/* Print input field */
	conout->SetAttribute (conout, BOX_INPUT);

	conout->SetCursorPosition (conout,
				   pwd_box->col_offset + 2,
				   pwd_box->row_offset + 3);

	conout->OutputString (conout, input_line);

	/* Move cursor to the first input position */
	conout->SetAttribute (conout, BOX_INPUT_TEXT);

	conout->SetCursorPosition (conout,
				   pwd_box->col_offset + 2,
				   pwd_box->row_offset + 3);

	conout->EnableCursor(conout, 1);
}

void
draw_password_box_initial (EFI_SYSTEM_TABLE *systab,
			   struct password_box *pwd_box)
{
	draw_password_box (systab, pwd_box, TXT_HINT);
}

void
draw_password_box_invalid (EFI_SYSTEM_TABLE *systab,
			   struct password_box *pwd_box)
{
	draw_password_box (systab, pwd_box, TXT_INVALID);
}

void
draw_password_box_error (EFI_SYSTEM_TABLE *systab,
			 struct password_box *pwd_box)
{
	draw_password_box (systab, pwd_box, TXT_ERROR);
}

void
get_password (EFI_SYSTEM_TABLE *systab, struct password_box *pwd_box,
	      uint8_t *pass_buf, UINTN buf_nbytes, UINTN *n_chars)
{
	*n_chars = 0;

	EFI_SIMPLE_TEXT_OUT_PROTOCOL *conout = systab->ConOut;
	EFI_SIMPLE_TEXT_IN_PROTOCOL  *conin  = systab->ConIn;

	EFI_INPUT_KEY input_key;
	UINTN event_idx;

	UINTN count = 0;
	EFI_STATUS error;

get_input:
	conout->EnableCursor(conout, count < PASS_FIELD_COLS);

	systab->BootServices->WaitForEvent (1, &conin->WaitForKey, &event_idx);

	error = conin->ReadKeyStroke (conin, &input_key);
	randseed_event (systab);

	if (error) {
		goto get_input;
	}

	switch (input_key.UnicodeChar) {
	case 0x0000:
	case 0x0009:
		goto get_input;
	case 0x0008: /* BS */
		pass_buf[count] = 0x00;
		if (count > 0) {
			count--;
			if (count < PASS_FIELD_COLS) {
				conout->SetAttribute (conout, BOX_INPUT);
				conout->OutputString (conout, CHAR_REMOVE);
				conout->SetAttribute (conout, BOX_INPUT_TEXT);
				/* Make sure that the cursor appears */
				conout->OutputString (conout, L"");
			}
		}

		goto get_input;
	default:
		if (count < (PASS_N_CHARS - 1)) {
			pass_buf[count] = input_key.UnicodeChar;
			if (count < PASS_FIELD_COLS) {
				conout->OutputString (conout, L"*");
			}
			count++;
		}

		if (count + 1 != buf_nbytes) {
			goto get_input;
		}
		/* Fallthrough */
	case 0x000A:
	case 0x000D:
		pass_buf[count + 1] = '\0';
		*n_chars = count;
		break;
	}
}

void
init_password_box (EFI_SYSTEM_TABLE *systab, struct password_box *pwd_box)
{
	EFI_SIMPLE_TEXT_OUT_PROTOCOL *conout = systab->ConOut;
	EFI_SIMPLE_TEXT_IN_PROTOCOL  *conin  = systab->ConIn;

	UINTN max_mode = systab->ConOut->Mode->MaxMode;

	EFI_STATUS error;
	UINTN cols = 0, rows = 0;

	UINTN max_cols = 0;
	UINTN max_rows = 0;
	UINTN best_mode = 0;

	int i;
	for (i = 0; i < max_mode; i++) {
		error = conout->QueryMode(systab->ConOut, i, &cols, &rows);

		if (!error	&&
		    cols != 0	&&
		    cols <= 512	&&
		    rows != 0	&&
		    rows <= 512) {
			if (max_cols < cols) {
				max_cols = cols;
				max_rows = rows;
				best_mode = i;
			} else if (max_cols == cols &&
				   max_rows < rows) {
				max_rows = rows;
				best_mode = i;
			}
		}
	}

	conout->SetMode (conout, best_mode);

	conout->SetAttribute (conout, BACKGROUND);

	conout->ClearScreen(conout);

	conout->EnableCursor(conout, 0);

	UINTN col_offset = (max_cols - PASS_BOX_COLS) / 2;
	UINTN row_offset = (max_rows - PASS_BOX_ROWS) / 2;

	pwd_box->cols = PASS_BOX_COLS;
	pwd_box->rows =	PASS_BOX_ROWS;
	pwd_box->col_offset = col_offset;
	pwd_box->row_offset = row_offset;
}

void
remove_password_box (EFI_SYSTEM_TABLE *systab)
{
	EFI_SIMPLE_TEXT_OUT_PROTOCOL *conout = systab->ConOut;

	conout->SetAttribute (conout, EFI_WHITE | EFI_BACKGROUND_BLACK);

	conout->ClearScreen(conout);

	conout->EnableCursor(conout, 1);
}
