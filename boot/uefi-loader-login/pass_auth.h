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

#ifndef _PASS_AUTH_H
#define _PASS_AUTH_H

struct password_box {
	UINTN cols;
	UINTN rows;
	UINTN col_offset;
	UINTN row_offset;
};

void init_password_box (EFI_SYSTEM_TABLE *systab,
			struct password_box *pwd_box);
void get_password (EFI_SYSTEM_TABLE *systab, struct password_box *pwd_box,
		   UINT8 *pass_buf, UINTN buf_nbytes, UINTN *n_chars);
void draw_password_box_initial (EFI_SYSTEM_TABLE *systab,
				struct password_box *pwd_box);
void draw_password_box_invalid (EFI_SYSTEM_TABLE *systab,
				struct password_box *pwd_box);
void draw_password_box_error (EFI_SYSTEM_TABLE *systab,
			      struct password_box *pwd_box);
void remove_password_box (EFI_SYSTEM_TABLE *systab);

#endif
