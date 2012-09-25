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

// Secure VM Project
// VPN Client Module (IPsec Driver) Source Code
// 
// Developed by Daiyuu Nobori (dnobori@cs.tsukuba.ac.jp)

// SeConfig.c
// 概要: 設定ファイル読み込み

#define SE_INTERNAL
#include <Se/Se.h>

// 設定ファイルからバイナリデータを取得する
SE_BUF *SeGetConfigBin(SE_LIST *o, char *name)
{
	char *s;
	SE_BUF *b;
	// 引数チェック
	if (o == NULL || name == NULL)
	{
		return NULL;
	}

	s = SeGetConfigStr(o, name);

	b = SeStrToBin(s);

	return b;
}

// 設定ファイルから 64 bit 数値を取得する
UINT64 SeGetConfigInt64(SE_LIST *o, char *name)
{
	// 引数チェック
	if (o == NULL || name == NULL)
	{
		return 0;
	}

	return SeToInt64(SeGetConfigStr(o, name));
}

// 設定ファイルから 32 bit 数値を取得する
UINT SeGetConfigInt(SE_LIST *o, char *name)
{
	// 引数チェック
	if (o == NULL || name == NULL)
	{
		return 0;
	}

	return SeToInt(SeGetConfigStr(o, name));
}

// 設定ファイルから bool 値を取得する
bool SeGetConfigBool(SE_LIST *o, char *name)
{
	char *s;
	// 引数チェック
	if (o == NULL || name == NULL)
	{
		return 0;
	}

	s = SeGetConfigStr(o, name);

	if (SeToInt(s) != 0)
	{
		return true;
	}

	if (SeIsEmptyStr(s) == false)
	{
		if (SeStartWith("true", s) || SeStartWith("yes", s) || SeStartWith("on", s) || SeStartWith("enable", s))
		{
			return true;
		}
	}

	return false;
}

// 設定ファイルから文字列を取得する
char *SeGetConfigStr(SE_LIST *o, char *name)
{
	SE_CONFIG_ENTRY *e;
	// 引数チェック
	if (o == NULL || name == NULL)
	{
		return NULL;
	}

	e = SeGetConfigEntry(o, name);

	if (e == NULL)
	{
		return "";
	}

	return e->Value;
}

// 設定ファイルを解放する
void SeFreeConfigEntryList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_CONFIG_ENTRY *e = SE_LIST_DATA(o, i);

		SeFree(e->Name);
		SeFree(e->Value);

		SeFree(e);
	}

	SeFreeList(o);
}

// 設定ファイルのエントリを取得する
SE_CONFIG_ENTRY *SeGetConfigEntry(SE_LIST *o, char *name)
{
	UINT i;
	// 引数チェック
	if (o == NULL || name == NULL)
	{
		return NULL;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_CONFIG_ENTRY *e = SE_LIST_DATA(o, i);

		if (SeStrCmpi(e->Name, name) == 0)
		{
			return e;
		}
	}

	return NULL;
}

// ファイルから設定ファイルを読み込む
SE_LIST *SeLoadConfigEntryList(char *name)
{
	SE_BUF *b;
	SE_LIST *o;
	// 引数チェック
	if (name == NULL)
	{
		return NULL;
	}

	b = SeReadDump(name);

	o = SeLoadConfigEntryListFromBuf(b);

	SeFreeBuf(b);

	return o;
}

// バッファから設定ファイルを読み込む
SE_LIST *SeLoadConfigEntryListFromBuf(SE_BUF *b)
{
	SE_LIST *o;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	o = SeNewList(NULL);

	SeSeekBuf(b, 0, 0);

	while (true)
	{
		char *line = SeReadNextLine(b);
		UINT i;

		if (line == NULL)
		{
			break;
		}

		// "#", ";" または "//" より後の部分を削除する
		i = SeSearchStrEx(line, "#", 0, false);
		if (i != INFINITE)
		{
			line[i] = 0;
		}
		i = SeSearchStrEx(line, ";", 0, false);
		if (i != INFINITE)
		{
			line[i] = 0;
		}
		i = SeSearchStrEx(line, "//", 0, false);
		if (i != INFINITE)
		{
			line[i] = 0;
		}

		SeTrim(line);

		if (SeIsEmptyStr(line) == false)
		{
			char *key, *value;
			UINT size = SeStrLen(line) + 1;

			key = SeZeroMalloc(size);
			value = SeZeroMalloc(size);

			if (SeGetKeyAndValue(line, key, size, value, size, NULL))
			{
				SE_CONFIG_ENTRY *e = SeZeroMalloc(sizeof(SE_CONFIG_ENTRY));

				e->Name = SeCopyStr(key);
				e->Value = SeCopyStr(value);

				SeAdd(o, e);
			}

			SeFree(key);
			SeFree(value);
		}

		SeFree(line);
	}

	return o;
}

// 指定された文字が区切り文字かどうかチェック
bool SeIsSplitChar(char c, char *split_str)
{
	UINT i, len;
	char c_upper = SeToUpper(c);
	if (split_str == NULL)
	{
		split_str = SeDefaultTokenSplitChars();
	}

	len = SeStrLen(split_str);

	for (i = 0;i < len;i++)
	{
		if (SeToUpper(split_str[i]) == c_upper)
		{
			return true;
		}
	}

	return false;
}

// 文字列からキーと値を取得する
bool SeGetKeyAndValue(char *str, char *key, UINT key_size, char *value, UINT value_size, char *split_str)
{
	UINT mode = 0;
	UINT wp1 = 0, wp2 = 0;
	UINT i, len;
	char *key_tmp, *value_tmp;
	bool ret = false;
	if (split_str == NULL)
	{
		split_str = SeDefaultTokenSplitChars();
	}

	len = SeStrLen(str);

	key_tmp = SeZeroMalloc(len + 1);
	value_tmp = SeZeroMalloc(len + 1);

	for (i = 0;i < len;i++)
	{
		char c = str[i];

		switch (mode)
		{
		case 0:
			if (SeIsSplitChar(c, split_str) == false)
			{
				mode = 1;
				key_tmp[wp1] = c;
				wp1++;
			}
			break;

		case 1:
			if (SeIsSplitChar(c, split_str) == false)
			{
				key_tmp[wp1] = c;
				wp1++;
			}
			else
			{
				mode = 2;
			}
			break;

		case 2:
			if (SeIsSplitChar(c, split_str) == false)
			{
				mode = 3;
				value_tmp[wp2] = c;
				wp2++;
			}
			break;

		case 3:
			value_tmp[wp2] = c;
			wp2++;
			break;
		}
	}

	if (mode != 0)
	{
		ret = true;
		SeStrCpy(key, key_size, key_tmp);
		SeStrCpy(value, value_size, value_tmp);
	}

	SeFree(key_tmp);
	SeFree(value_tmp);

	return ret;
}
