/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (C) 2007, 2008
 *      National Institute of Information and Communications Technology
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

#include "crypt.h"
#include "mm.h"
#include "printf.h"
#include "string.h"

// 提供システムコール: RSA 署名操作
bool
crypt_sys_rsa_sign (void *key_data, UINT key_size, void *data, UINT data_size,
		    void *sign, UINT *sign_buf_size)
{
	SE_BUF *ret_buf;
	SE_KEY *key;
	SE_BUF *key_file_buf;
	SE_BUF *tmp_buf;

	// 引数チェック
	if (key_data == NULL || data == NULL || data_size == 0 || sign == NULL || sign_buf_size == NULL)
	{
		return false;
	}

	// 秘密鍵データのコピー
	key_file_buf = SeNewBuf();
	SeWriteBuf(key_file_buf, key_data, key_size);

	tmp_buf = SeMemToBuf(key_file_buf->Buf, key_file_buf->Size);
	SeFreeBuf(key_file_buf);

	key = SeBufToKey(tmp_buf, true, true, NULL);

	if (key == NULL)
	{
		key = SeBufToKey(tmp_buf, true, false, NULL);
	}

	if (key == NULL)
	{
		printf("crypt.c: Loading Key Failed.\n");
		SeFreeBuf(tmp_buf);
		*sign_buf_size = 0;
		return false;
	}

	SeFreeBuf(tmp_buf);

	ret_buf = SeRsaSignWithPadding(data, data_size, key);
	if (ret_buf == NULL)
	{
		printf("crypt.c: SeRsaSignWithPadding() Failed.\n");
		SeFreeKey(key);
		*sign_buf_size = 0;
		return false;
	}

	if (*sign_buf_size < ret_buf->Size)
	{
		printf("crypt.c: Not Enough Buffer Space.\n");
		*sign_buf_size = ret_buf->Size;
		SeFreeBuf(ret_buf);
		SeFreeKey(key);
		*sign_buf_size = ret_buf->Size;
		return false;
	}

	*sign_buf_size = ret_buf->Size;

	SeCopy(sign, ret_buf->Buf, ret_buf->Size);
	SeFreeBuf(ret_buf);
	SeFreeKey(key);

	return true;
}
