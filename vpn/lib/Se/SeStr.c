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

// Secure VM Project
// VPN Client Module (IPsec Driver) Source Code
// 
// Developed by Daiyuu Nobori (dnobori@cs.tsukuba.ac.jp)

// SeStr.c
// 概要: 文字列操作

#define SE_INTERNAL
#include <chelp.h>
#include <Se/Se.h>

// 1 行を読み込む
char *SeReadNextLine(SE_BUF *b)
{
	char *tmp;
	char *buf;
	UINT len;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	// 次の改行までの文字数を調査
	tmp = (char *)b->Buf + b->Current;
	if ((b->Size - b->Current) == 0)
	{
		// 最後まで読んだ
		return NULL;
	}
	len = 0;
	while (true)
	{
		if (tmp[len] == 13 || tmp[len] == 10)
		{
			if (tmp[len] == 13)
			{
				if (len < (b->Size - b->Current))
				{
					len++;
				}
			}
			break;
		}
		len++;
		if (len >= (b->Size - b->Current))
		{
			break;
		}
	}

	// len だけ読み込む
	buf = SeZeroMalloc(len + 1);
	SeReadBuf(b, buf, len);
	SeSeekBuf(b, 1, 1);

	if (SeStrLen(buf) >= 1)
	{
		if (buf[SeStrLen(buf) - 1] == 13)
		{
			buf[SeStrLen(buf) - 1] = 0;
		}
	}

	return buf;
}

// MAC アドレスを文字列に変換する
void SeMacToStr(char *str, UINT str_size, UCHAR *mac_address)
{
	// 引数チェック
	if (str == NULL || mac_address == NULL)
	{
		if (str != NULL)
		{
			str[0] = '\0';
		}
		return;
	}

	SeFormat(str, str_size, "%02X-%02X-%02X-%02X-%02X-%02X",
		mac_address[0],
		mac_address[1],
		mac_address[2],
		mac_address[3],
		mac_address[4],
		mac_address[5]);
}

// 文字列を MAC アドレスに変換する
bool SeStrToMac(UCHAR *mac_address, char *str)
{
	return SeStrToBinEx(mac_address, 6, str);
}

// 16 進文字列をバイナリデータに変換する
SE_BUF *SeStrToBin(char *str)
{
	SE_BUF *b;
	UINT len, i;
	char tmp[3];
	// 引数チェック
	if (str == NULL)
	{
		return NULL;
	}

	len = SeStrLen(str);
	tmp[0] = 0;
	b = SeNewBuf();
	for (i = 0;i < len;i++)
	{
		char c = str[i];
		c = SeToUpper(c);
		if (('0' <= c && c <= '9') || ('A' <= c && c <= 'F'))
		{
			if (tmp[0] == 0)
			{
				tmp[0] = c;
				tmp[1] = 0;
			}
			else if (tmp[1] == 0)
			{
				UCHAR data;
				char tmp2[64];
				tmp[1] = c;
				tmp[2] = 0;
				SeStrCpy(tmp2, sizeof(tmp2), tmp);
				data = SeHexToInt(tmp2);
				SeWriteBuf(b, &data, 1);
				SeZero(tmp, sizeof(tmp));	
			}
		}
		else if (c == ' ' || c == ',' || c == '-' || c == ':')
		{
			// 何もしない
		}
		else
		{
			break;
		}
	}

	return b;
}
bool SeStrToBinEx(void *dst, UINT dst_size, char *str)
{
	SE_BUF *b;
	bool ret = false;
	// 引数チェック
	if (dst == NULL)
	{
		return false;
	}
	if (str == NULL)
	{
		str = "";
	}

	b = SeStrToBin(str);

	if (dst_size == b->Size)
	{
		ret = true;
		SeCopy(dst, b->Buf, dst_size);
	}

	SeFreeBuf(b);

	return ret;
}

// バイナリデータを 16 進文字列に変換する
void SeBinToStrEx(char *str, UINT str_size, void *data, UINT data_size)
{
	char *tmp;
	UCHAR *buf = (UCHAR *)data;
	UINT size;
	UINT i;
	// 引数チェック
	if (str == NULL || data == NULL)
	{
		return;
	}

	// サイズの計算
	size = data_size * 3 * sizeof(char) + 1;
	// メモリ確保
	tmp = SeZeroMalloc(size);
	// 変換
	for (i = 0;i < data_size;i++)
	{
		SeFormat(&tmp[i * 3], 0, "%02X ", buf[i]);
	}
	SeTrim(tmp);
	// コピー
	SeStrCpy(str, str_size, tmp);
	// メモリ解放
	SeFree(tmp);
}
void SeBinToStr(char *str, UINT str_size, void *data, UINT data_size)
{
	char *tmp;
	UCHAR *buf = (UCHAR *)data;
	UINT size;
	UINT i;
	// 引数チェック
	if (str == NULL || data == NULL)
	{
		return;
	}

	// サイズの計算
	size = data_size * 2 * sizeof(char) + 1;
	// メモリ確保
	tmp = SeZeroMalloc(size);
	// 変換
	for (i = 0;i < data_size;i++)
	{
		SeFormat(&tmp[i * 2], 0, "%02X", buf[i]);
	}
	SeTrim(tmp);
	// コピー
	SeStrCpy(str, str_size, tmp);
	// メモリ解放
	SeFree(tmp);
}

// 文字列をフォーマットする
char *SeFormatEx(char *fmt, va_list arg_list)
{
	UINT i, len;
	char *tmp;
	UINT tmp_size;
	SE_LIST *o;
	UINT mode = 0;
	UINT wp;
	UINT total_size;
	char *ret;
	UINT n;
	// 引数チェック
	if (fmt == NULL)
	{
		return NULL;
	}

	len = SeStrLen(fmt);
	tmp_size = SeStrSize(fmt);
	tmp = SeMalloc(tmp_size);

	o = SeNewList(NULL);

	mode = 0;
	wp = 0;
	n = 0;

	for (i = 0;i < len;i++)
	{
		char c = fmt[i];

		if (mode == 0)
		{
			// 通常モード
			switch (c)
			{
			case '%':
				// 書式指定の開始
				if (fmt[i + 1] == '%')
				{
					// '%' 文字を出力
					i++;
					tmp[wp++] = c;
				}
				else
				{
					// 状態遷移
					mode = 1;
					tmp[wp++] = 0;
					wp = 0;
					SeInsert(o, SeCopyStr(tmp));
					tmp[wp++] = c;
				}
				break;

			default:
				// 通常の文字
				tmp[wp++] = c;
				break;
			}
		}
		else
		{
			// 書式指定モード
			char *target_str;
			char *padding_str;
			bool left_padding;
			bool zero_padding;
			UINT target_str_len;
			UINT total_len;
			char *output_str;
			UINT padding;
			bool pointer_data;
			UINT value;
			UINT64 value64;
			void *pvalue;
			bool longlong_data;

			switch (c)
			{
			case 'c':
			case 'C':
			case 'd':
			case 'i':
			case 'u':
			case 'X':
			case 'x':
			case 's':
			case 'S':
				// データ型
				tmp[wp++] = c;
				tmp[wp++] = 0;
				pointer_data = false;
				longlong_data = false;
				value = 0;
				value64 = 0;
				pvalue = NULL;

				switch (c)
				{
				case 's':
				case 'S':
					// ポインタ型
					pointer_data = true;
					break;
				}

				if ((SeStrLen(tmp) >= 5 && tmp[SeStrLen(tmp) - 4] == 'I' &&
					tmp[SeStrLen(tmp) - 3] == '6' && tmp[SeStrLen(tmp) - 2] == '4') ||
					(SeStrLen(tmp) >= 4 && tmp[SeStrLen(tmp) - 3] == 'l' &&
					tmp[SeStrLen(tmp) - 2] == 'l'))
				{
					// 64 bit データ型
					longlong_data = true;
				}

				pvalue = va_arg(arg_list, void *);
				value = (UINT)((unsigned long)pvalue);
				value64 = (UINT64)value;

				if (longlong_data)
				{
					void *pvalue2;
					pvalue2 = va_arg(arg_list, void *);
					value64 = value64 + (UINT64)(UINT)((unsigned long)pvalue2) * 4294967296ULL;
				}

				switch (c)
				{
				case 's':
				case 'S':
					// 文字列型
					if (pvalue == NULL)
					{
						target_str = SeCopyStr("(null)");
					}
					else
					{
						target_str = SeCopyStr(pvalue);
					}
					break;

				case 'u':
				case 'U':
				case 'i':
				case 'I':
				case 'd':
				case 'D':
					// 整数型
					if (longlong_data == false)
					{
						target_str = SeZeroMalloc(12);
						SeToStr(target_str, value);
					}
					else
					{
						target_str = SeZeroMalloc(24);
						SeToStr64(target_str, value64);
					}
					break;

				case 'x':
				case 'X':
					// HEX 型
					target_str = SeZeroMalloc(20);
					SeToHex(target_str, value);

					if (c == 'X')
					{
						SeStrUpper(target_str);
					}
					break;
				}

				// 書式統一
				padding = 0;
				zero_padding = false;
				left_padding = false;

				if (tmp[1] == '-')
				{
					// 左詰め
					if (SeStrLen(tmp) >= 3)
					{
						if (tmp[2] == '0')
						{
							zero_padding = true;
							padding = SeToInt(&tmp[3]);
						}
						else
						{
							padding = SeToInt(&tmp[2]);
						}
					}
					left_padding = true;
				}
				else
				{
					// 右詰め
					if (SeStrLen(tmp) >= 2)
					{
						if (tmp[1] == '0')
						{
							zero_padding = true;
							padding = SeToInt(&tmp[2]);
						}
						else
						{
							padding = SeToInt(&tmp[1]);
						}
					}
				}

				target_str_len = SeStrLen(target_str);

				if (padding > target_str_len)
				{
					UINT len = padding - target_str_len;

					padding_str = SeMakeCharArray((zero_padding ? '0' : ' '), len);
				}
				else
				{
					padding_str = SeZeroMalloc(sizeof(char));
				}

				total_len = sizeof(char) * (SeStrLen(padding_str) + SeStrLen(target_str) + 1);
				output_str = SeZeroMalloc(total_len);
				output_str[0] = 0;

				if (left_padding == false)
				{
					SeStrCat(output_str, total_len, padding_str);
				}
				SeStrCat(output_str, total_len, target_str);
				if (left_padding)
				{
					SeStrCat(output_str, total_len, padding_str);
				}

				SeAdd(o, output_str);

				SeFree(target_str);
				SeFree(padding_str);

				wp = 0;
				mode = 0;
				break;

			default:
				// 通常の文字列
				tmp[wp++] = c;
				break;
			}
		}
	}
	tmp[wp++] = 0;
	wp = 0;

	if (SeStrLen(tmp) >= 1)
	{
		SeAdd(o, SeCopyStr(tmp));
	}

	total_size = sizeof(char);
	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		char *s = SE_LIST_DATA(o, i);
		total_size += SeStrLen(s) * sizeof(char);
	}

	ret = SeZeroMalloc(total_size);
	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		char *s = SE_LIST_DATA(o, i);
		SeStrCat(ret, total_size, s);
		SeFree(s);
	}

	SeFreeList(o);

	SeFree(tmp);

	return ret;
}

// 文字列をフォーマットする
void SeFormatArgs(char *buf, UINT size, char *fmt, va_list args)
{
	char *ret;
	// 引数チェック
	if (buf == NULL || fmt == NULL)
	{
		return;
	}
	if (size == 1)
	{
		buf[0] = 0;
		return;
	}

	ret = SeFormatEx(fmt, args);

	SeStrCpy(buf, size, ret);

	SeFree(ret);
}
void SeFormat(char *buf, UINT size, char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	SeFormatArgs(buf, size, fmt, args);

	va_end(args);
}

// 指定した文字の列を生成する
char *SeMakeCharArray(char c, UINT count)
{
	UINT i;
	char *ret = SeMalloc(count + 1);

	for (i = 0;i < count;i++)
	{
		ret[i] = c;
	}

	ret[count] = 0;

	return ret;
}

// 指定した文字列がすべて小文字かどうかチェックする
bool SeIsAllLowerStr(char *str)
{
	UINT i, len;
	// 引数チェック
	if (str == NULL)
	{
		return false;
	}

	len = SeStrLen(str);

	for (i = 0;i < len;i++)
	{
		char c = str[i];

		if ((c >= '0' && c <= '9') ||
			(c >= 'a' && c <= 'z'))
		{
		}
		else
		{
			return false;
		}
	}

	return true;
}

// 指定した文字列がすべて大文字かどうかチェックする
bool SeIsAllUpperStr(char *str)
{
	UINT i, len;
	// 引数チェック
	if (str == NULL)
	{
		return false;
	}

	len = SeStrLen(str);

	for (i = 0;i < len;i++)
	{
		char c = str[i];

		if ((c >= '0' && c <= '9') ||
			(c >= 'A' && c <= 'Z'))
		{
		}
		else
		{
			return false;
		}
	}

	return true;
}

// トークンリストを重複の無いものに変換する
SE_TOKEN_LIST *SeUniqueToken(SE_TOKEN_LIST *t)
{
	UINT i, num, j, n;
	SE_TOKEN_LIST *ret;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	num = 0;
	for (i = 0;i < t->NumTokens;i++)
	{
		bool exists = false;

		for (j = 0;j < i;j++)
		{
			if (SeStrCmpi(t->Token[j], t->Token[i]) == 0)
			{
				exists = true;
				break;
			}
		}

		if (exists == false)
		{
			num++;
		}
	}

	ret = SeZeroMalloc(sizeof(SE_TOKEN_LIST));
	ret->Token = SeZeroMalloc(sizeof(char *) * num);
	ret->NumTokens = num;

	n = 0;

	for (i = 0;i < t->NumTokens;i++)
	{
		bool exists = false;

		for (j = 0;j < i;j++)
		{
			if (SeStrCmpi(t->Token[j], t->Token[i]) == 0)
			{
				exists = true;
				break;
			}
		}

		if (exists == false)
		{
			ret->Token[n++] = SeCopyStr(t->Token[i]);
		}
	}

	return ret;
}

// 数字を文字列に変換して 3 桁ずつカンマで区切る
void SeToStr3(char *str, UINT size, UINT64 v)
{
	char tmp[128];
	char tmp2[128];
	UINT i, len, wp;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	SeToStr64(tmp, v);

	wp = 0;
	len = SeStrLen(tmp);

	for (i = len - 1;((int)i) >= 0;i--)
	{
		tmp2[wp++] = tmp[i];
	}
	tmp2[wp++] = 0;

	wp = 0;

	for (i = 0;i < len;i++)
	{
		if (i != 0 && (i % 3) == 0)
		{
			tmp[wp++] = ',';
		}
		tmp[wp++] = tmp2[i];
	}
	tmp[wp++] = 0;
	wp = 0;
	len = SeStrLen(tmp);

	for (i = len - 1;((int)i) >= 0;i--)
	{
		tmp2[wp++] = tmp[i];
	}
	tmp2[wp++] = 0;

	SeStrCpy(str, size, tmp2);
}

// トークンリストの解放
void SeFreeToken(SE_TOKEN_LIST *t)
{
	UINT i;
	if (t == NULL)
	{
		return;
	}

	for (i = 0;i < t->NumTokens;i++)
	{
		if (t->Token[i] != 0)
		{
			SeFree(t->Token[i]);
		}
	}
	SeFree(t->Token);
	SeFree(t);
}

// 文字列からトークンを切り出す (区切り文字間の空間を無視しない)
SE_TOKEN_LIST *SeParseTokenWithNullStr(char *str, char *split_chars)
{
	SE_LIST *o;
	UINT i, len;
	SE_BUF *b;
	char zero = 0;
	SE_TOKEN_LIST *t;
	// 引数チェック
	if (str == NULL)
	{
		return SeNullTokenList();
	}
	if (split_chars == NULL)
	{
		split_chars = SeDefaultTokenSplitChars();
	}

	b = SeNewBuf();
	o = SeNewList(NULL);

	len = SeStrLen(str);

	for (i = 0;i < (len + 1);i++)
	{
		char c = str[i];
		bool flag = SeIsCharInStr(split_chars, c);

		if (c == '\0')
		{
			flag = true;
		}

		if (flag == false)
		{
			SeWriteBuf(b, &c, sizeof(char));
		}
		else
		{
			SeWriteBuf(b, &zero, sizeof(char));

			SeInsert(o, SeCopyStr((char *)b->Buf));
			SeClearBuf(b);
		}
	}

	t = SeZeroMalloc(sizeof(SE_TOKEN_LIST));
	t->NumTokens = SE_LIST_NUM(o);
	t->Token = SeZeroMalloc(sizeof(char *) * t->NumTokens);

	for (i = 0;i < t->NumTokens;i++)
	{
		t->Token[i] = SE_LIST_DATA(o, i);
	}

	SeFreeList(o);
	SeFreeBuf(b);

	return t;
}

// 文字列からトークンを切り出す (区切り文字間の空間を無視する)
SE_TOKEN_LIST *SeParseToken(char *str, char *split_chars)
{
	SE_LIST *o;
	UINT i, len;
	bool last_flag;
	SE_BUF *b;
	char zero = 0;
	SE_TOKEN_LIST *t;
	// 引数チェック
	if (str == NULL)
	{
		return SeNullTokenList();
	}
	if (split_chars == NULL)
	{
		split_chars = SeDefaultTokenSplitChars();
	}

	b = SeNewBuf();
	o = SeNewList(NULL);

	len = SeStrLen(str);
	last_flag = false;

	for (i = 0;i < (len + 1);i++)
	{
		char c = str[i];
		bool flag = SeIsCharInStr(split_chars, c);

		if (c == '\0')
		{
			flag = true;
		}

		if (flag == false)
		{
			SeWriteBuf(b, &c, sizeof(char));
		}
		else
		{
			if (last_flag == false)
			{
				SeWriteBuf(b, &zero, sizeof(char));

				if ((SeStrLen((char *)b->Buf)) != 0)
				{
					SeInsert(o, SeCopyStr((char *)b->Buf));
				}
				SeClearBuf(b);
			}
		}

		last_flag = flag;
	}

	t = SeZeroMalloc(sizeof(SE_TOKEN_LIST));
	t->NumTokens = SE_LIST_NUM(o);
	t->Token = SeZeroMalloc(sizeof(char *) * t->NumTokens);

	for (i = 0;i < t->NumTokens;i++)
	{
		t->Token[i] = SE_LIST_DATA(o, i);
	}

	SeFreeList(o);
	SeFreeBuf(b);

	return t;
}

// 指定された文字が文字列に含まれるかどうかチェック
bool SeIsCharInStr(char *str, char c)
{
	UINT i, len;
	// 引数チェック
	if (str == NULL)
	{
		return false;
	}

	len = SeStrLen(str);
	for (i = 0;i < len;i++)
	{
		if (str[i] == c)
		{
			return true;
		}
	}

	return false;
}

// 空白のトークンリストの作成
SE_TOKEN_LIST *SeNullTokenList()
{
	SE_TOKEN_LIST *t = SeZeroMalloc(sizeof(SE_TOKEN_LIST));

	t->NumTokens = 0;
	t->Token = SeZeroMalloc(0);

	return t;
}

// 標準のトークン区切り文字を取得する
char *SeDefaultTokenSplitChars()
{
	return " ,\t\r\n";
}

// 改行コードを正規化する
char *SeNormalizeCrlf(char *str)
{
	char *ret;
	UINT ret_size, i, len, wp;
	// 引数チェック
	if (str == NULL)
	{
		return NULL;
	}

	len = SeStrLen(str);
	ret_size = sizeof(char) * (len + 32) * 2;
	ret = SeMalloc(ret_size);

	wp = 0;

	for (i = 0;i < len;i++)
	{
		char c = str[i];

		switch (c)
		{
		case '\r':
			if (str[i + 1] == '\n')
			{
				i++;
			}
			ret[wp++] = '\r';
			ret[wp++] = '\n';
			break;

		case '\n':
			ret[wp++] = '\r';
			ret[wp++] = '\n';
			break;

		default:
			ret[wp++] = c;
			break;
		}
	}

	ret[wp++] = 0;

	return ret;
}

// 文字列が空かどうか調べる
bool SeIsEmptyStr(char *str)
{
	char *s;
	// 引数チェック
	if (str == NULL)
	{
		return true;
	}

	s = SeTrimCopy(str);

	if (SeStrLen(s) == 0)
	{
		SeFree(s);
		return true;
	}
	else
	{
		SeFree(s);
		return false;
	}
}

// str が key で終了するかどうかチェック
bool SeEndWith(char *str, char *key)
{
	UINT str_len;
	UINT key_len;
	// 引数チェック
	if (str == NULL || key == NULL)
	{
		return false;
	}

	// 比較
	str_len = SeStrLen(str);
	key_len = SeStrLen(key);
	if (str_len < key_len)
	{
		return false;
	}

	if (SeStrCmpi(str + (str_len - key_len), key) == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

// str が key で始まるかどうかチェック
bool SeStartWith(char *str, char *key)
{
	UINT str_len;
	UINT key_len;
	char *tmp;
	bool ret;
	// 引数チェック
	if (str == NULL || key == NULL)
	{
		return false;
	}

	// 比較
	str_len = SeStrLen(str);
	key_len = SeStrLen(key);
	if (str_len < key_len)
	{
		return false;
	}
	if (str_len == 0 || key_len == 0)
	{
		return false;
	}
	tmp = SeCopyStr(str);
	tmp[key_len] = 0;

	if (SeStrCmpi(tmp, key) == 0)
	{
		ret = true;
	}
	else
	{
		ret = false;
	}

	SeFree(tmp);

	return ret;
}

// 末尾の \r \n を削除する
void SeTrimCrlf(char *str)
{
	UINT len;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}
	len = SeStrLen(str);
	if (len == 0)
	{
		return;
	}

	if (str[len - 1] == '\n')
	{
		if (len >= 2 && str[len - 2] == '\n')
		{
			str[len - 2] = 0;
		}
		str[len - 1] = 0;
	}
	else if (str[len - 1] == '\r')
	{
		str[len - 1] = 0;
	}
}

// 文字列の置換 (大文字小文字を区別しない)
UINT SeReplaceStri(char *dst, UINT size, char *string, char *old_keyword, char *new_keyword)
{
	return SeReplaceStrEx(dst, size, string, old_keyword, new_keyword, false);
}

// 文字列の置換 (大文字小文字を区別する)
UINT SeReplaceStr(char *dst, UINT size, char *string, char *old_keyword, char *new_keyword)
{
	return SeReplaceStrEx(dst, size, string, old_keyword, new_keyword, true);
}

// 文字列の置換
UINT SeReplaceStrEx(char *dst, UINT size, char *string, char *old_keyword, char *new_keyword, bool case_sensitive)
{
	UINT i, j, num;
	UINT len_string, len_old, len_new;
	UINT len_ret;
	UINT wp;
	char *ret;
	// 引数チェック
	if (string == NULL || old_keyword == NULL || new_keyword == NULL)
	{
		return 0;
	}

	// 文字列長の取得
	len_string = SeStrLen(string);
	len_old = SeStrLen(old_keyword);
	len_new = SeStrLen(new_keyword);

	// 最終文字列長の計算
	len_ret = SeCalcReplaceStrEx(string, old_keyword, new_keyword, case_sensitive);
	// メモリ確保
	ret = SeMalloc(len_ret + 1);
	ret[len_ret] = '\0';

	// 検索と置換
	i = 0;
	j = 0;
	num = 0;
	wp = 0;
	while (true)
	{
		i = SeSearchStrEx(string, old_keyword, i, case_sensitive);
		if (i == INFINITE)
		{
			SeCopy(ret + wp, string + j, len_string - j);
			wp += len_string - j;
			break;
		}
		num++;
		SeCopy(ret + wp, string + j, i - j);
		wp += i - j;
		SeCopy(ret + wp, new_keyword, len_new);
		wp += len_new;
		i += len_old;
		j = i;
	}

	// 検索結果のコピー
	SeStrCpy(dst, size, ret);

	// メモリ解放
	SeFree(ret);

	return num;
}

// 文字列の置換後の文字列長を計算する
UINT SeCalcReplaceStrEx(char *string, char *old_keyword, char *new_keyword, bool case_sensitive)
{
	UINT i, num;
	UINT len_string, len_old, len_new;
	// 引数チェック
	if (string == NULL || old_keyword == NULL || new_keyword == NULL)
	{
		return 0;
	}

	// 文字列長の取得
	len_string = SeStrLen(string);
	len_old = SeStrLen(old_keyword);
	len_new = SeStrLen(new_keyword);

	if (len_old == len_new)
	{
		return len_string;
	}

	// 検索処理
	num = 0;
	i = 0;
	while (true)
	{
		i = SeSearchStrEx(string, old_keyword, i, case_sensitive);
		if (i == INFINITE)
		{
			break;
		}
		i += len_old;
		num++;
	}

	// 計算
	return len_string + len_new * num - len_old * num;
}

// 文字列の検索 (大文字 / 小文字を区別する)
UINT SeSearchStr(char *string, char *keyword, UINT start)
{
	return SeSearchStrEx(string, keyword, start, true);
}

// 文字列の検索 (大文字 / 小文字を区別しない)
UINT SeSearchStri(char *string, char *keyword, UINT start)
{
	return SeSearchStrEx(string, keyword, start, false);
}

// 文字列 string から文字列 keyword を検索して最初に見つかった文字の場所を返す
// (1文字目に見つかったら 0, 見つからなかったら INFINITE)
UINT SeSearchStrEx(char *string, char *keyword, UINT start, bool case_sensitive)
{
	UINT len_string, len_keyword;
	UINT i;
	char *cmp_string, *cmp_keyword;
	bool found;
	// 引数チェック
	if (string == NULL || keyword == NULL)
	{
		return INFINITE;
	}

	// string の長さを取得
	len_string = SeStrLen(string);
	if (len_string <= start)
	{
		// start の値が不正
		return INFINITE;
	}

	// keyword の長さを取得
	len_keyword = SeStrLen(keyword);
	if (len_keyword == 0)
	{
		// キーワードが無い
		return INFINITE;
	}

	if ((len_string - start) < len_keyword)
	{
		// キーワードの長さのほうが長い
		return INFINITE;
	}

	if (case_sensitive)
	{
		cmp_string = string;
		cmp_keyword = keyword;
	}
	else
	{
		cmp_string = SeMalloc(len_string + 1);
		SeStrCpy(cmp_string, len_string + 1, string);
		cmp_keyword = SeMalloc(len_keyword + 1);
		SeStrCpy(cmp_keyword, len_keyword + 1, keyword);
		SeStrUpper(cmp_string);
		SeStrUpper(cmp_keyword);
	}

	// 検索
	found = false;
	for (i = start;i < (len_string - len_keyword + 1);i++)
	{
		// 比較する
		if (!SeStrnCmp(&cmp_string[i], cmp_keyword, len_keyword))
		{
			// 発見した
			found = true;
			break;
		}
	}

	if (case_sensitive == false)
	{
		// メモリ解放
		SeFree(cmp_keyword);
		SeFree(cmp_string);
	}

	if (found == false)
	{
		return INFINITE;
	}
	return i;
}

// 文字列をコピーする
char *SeCopyStr(char *str)
{
	UINT len;
	char *dst;
	// 引数チェック
	if (str == NULL)
	{
		return NULL;
	}

	len = SeStrLen(str);
	dst = SeMalloc(len + 1);
	SeStrCpy(dst, len + 1, str);
	return dst;
}

// 文字列の左右の空白を削除してコピー
char *SeTrimCopy(char *str)
{
	char *ret;
	// 引数チェック
	if (str == NULL)
	{
		return NULL;
	}

	ret = SeCopyStr(str);
	SeTrim(ret);

	return ret;
}

// 文字列の左右の空白を削除
void SeTrim(char *str)
{
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	// 左側を trim
	SeTrimLeft(str);

	// 右側を trim
	SeTrimRight(str);
}

// 文字列の右側の空白を削除
void SeTrimRight(char *str)
{
	char *buf, *tmp;
	UINT len, i, wp, wp2;
	bool flag;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}
	len = SeStrLen(str);
	if (len == 0)
	{
		return;
	}
	if (str[len - 1] != ' ' && str[len - 1] != '\t')
	{
		return;
	}

	buf = SeMalloc(len + 1);
	tmp = SeMalloc(len + 1);
	flag = false;
	wp = 0;
	wp2 = 0;
	for (i = 0;i < len;i++)
	{
		if (str[i] != ' ' && str[i] != '\t')
		{
			SeCopy(buf + wp, tmp, wp2);
			wp += wp2;
			wp2 = 0;
			buf[wp++] = str[i];
		}
		else
		{
			tmp[wp2++] = str[i];
		}
	}
	buf[wp] = 0;
	SeStrCpy(str, 0, buf);
	SeFree(buf);
	SeFree(tmp);
}

// 文字列の左側の空白を削除
void SeTrimLeft(char *str)
{
	char *buf;
	UINT len, i, wp;
	bool flag;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}
	len = SeStrLen(str);
	if (len == 0)
	{
		return;
	}
	if (str[0] != ' ' && str[0] != '\t')
	{
		return;
	}

	buf = SeMalloc(len + 1);
	flag = false;
	wp = 0;
	for (i = 0;i < len;i++)
	{
		if (str[i] != ' ' && str[i] != '\t')
		{
			flag = TRUE;
		}
		if (flag)
		{
			buf[wp++] = str[i];
		}
	}
	buf[wp] = 0;
	SeStrCpy(str, 0, buf);
	SeFree(buf);
}

// HEX 文字列を 64 bit 整数に変換
UINT64 SeHexToInt64(char *str)
{
	UINT len, i;
	UINT64 ret = 0;
	// 引数チェック
	if (str == NULL)
	{
		return 0;
	}

	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
	}

	len = SeStrLen(str);
	for (i = 0;i < len;i++)
	{
		char c = str[i];

		if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
		{
			ret = ret * 16ULL + (UINT64)SeHexTo4Bit(c);
		}
		else
		{
			break;
		}
	}

	return ret;
}

// HEX 文字列を 32 bit 整数に変換
UINT SeHexToInt(char *str)
{
	UINT len, i;
	UINT ret = 0;
	// 引数チェック
	if (str == NULL)
	{
		return 0;
	}

	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
	{
		str += 2;
	}

	len = SeStrLen(str);
	for (i = 0;i < len;i++)
	{
		char c = str[i];

		if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
		{
			ret = ret * 16 + (UINT)SeHexTo4Bit(c);
		}
		else
		{
			break;
		}
	}

	return ret;
}

// 64 bit 整数を HEX に変換
void SeToHex64(char *str, UINT64 value)
{
	char tmp[MAX_SIZE];
	UINT wp = 0;
	UINT len, i;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	// 空文字に設定
	SeStrCpy(tmp, 0, "");

	// 末尾桁から追加する
	while (true)
	{
		UINT a = (UINT)(value % (UINT64)16);
		value = value / (UINT)16;
		tmp[wp++] = Se4BitToHex(a);
		if (value == 0)
		{
			tmp[wp++] = 0;
			break;
		}
	}

	// 逆順にする
	len = SeStrLen(tmp);
	for (i = 0;i < len;i++)
	{
		str[len - i - 1] = tmp[i];
	}
	str[len] = 0;
}

// 32 bit 整数を HEX に変換
void SeToHex(char *str, UINT value)
{
	char tmp[MAX_SIZE];
	UINT wp = 0;
	UINT len, i;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	// 空文字に設定
	SeStrCpy(tmp, 0, "");

	// 末尾桁から追加する
	while (true)
	{
		UINT a = (UINT)(value % (UINT)16);
		value = value / (UINT)16;
		tmp[wp++] = Se4BitToHex(a);
		if (value == 0)
		{
			tmp[wp++] = 0;
			break;
		}
	}

	// 逆順にする
	len = SeStrLen(tmp);
	for (i = 0;i < len;i++)
	{
		str[len - i - 1] = tmp[i];
	}
	str[len] = 0;
}

// 4 bit 数値を 16 進文字列に変換
char Se4BitToHex(UINT value)
{
	value = value % 16;

	if (value <= 9)
	{
		return '0' + value;
	}
	else
	{
		return 'a' + (value - 10);
	}
}

// 16 進文字列を 4 bit 整数に変換
UINT SeHexTo4Bit(char c)
{
	if ('0' <= c && c <= '9')
	{
		return c - '0';
	}
	else if ('a' <= c && c <= 'f')
	{
		return c - 'a' + 10;
	}
	else if ('A' <= c && c <= 'F')
	{
		return c - 'A' + 10;
	}
	else
	{
		return 0;
	}
}

// 32 bit 整数を文字列に変換
void SeToStr(char *str, UINT value)
{
	char tmp[MAX_SIZE];
	UINT wp = 0;
	UINT len, i;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	// 空文字に設定
	SeStrCpy(tmp, 0, "");

	// 末尾桁から追加する
	while (true)
	{
		UINT a = (UINT)(value % (UINT)10);
		value = value / (UINT)10;
		tmp[wp++] = (char)('0' + a);
		if (value == 0)
		{
			tmp[wp++] = 0;
			break;
		}
	}

	// 逆順にする
	len = SeStrLen(tmp);
	for (i = 0;i < len;i++)
	{
		str[len - i - 1] = tmp[i];
	}
	str[len] = 0;
}

// 64 bit 整数を文字列に変換
void SeToStr64(char *str, UINT64 value)
{
	char tmp[MAX_SIZE];
	UINT wp = 0;
	UINT len, i;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	// 空文字に設定
	SeStrCpy(tmp, 0, "");

	// 末尾桁から追加する
	while (true)
	{
		UINT a = chelp_mod_64_32_32(value, 10);
		value = chelp_div_64_32_64(value, 10);
		tmp[wp++] = (char)('0' + a);
		if (value == 0)
		{
			tmp[wp++] = 0;
			break;
		}
	}

	// 逆順にする
	len = SeStrLen(tmp);
	for (i = 0;i < len;i++)
	{
		str[len - i - 1] = tmp[i];
	}
	str[len] = 0;
}

// 文字列を 32 bit 整数に変換
UINT SeToInt(char *str)
{
	UINT len, i;
	UINT ret = 0;
	// 引数チェック
	if (str == NULL)
	{
		return 0;
	}

	len = SeStrLen(str);
	for (i = 0;i < len;i++)
	{
		char c = str[i];
		if (c != ',')
		{
			if ('0' <= c && c <= '9')
			{
				ret = ret * (UINT)10 + (UINT)(c - '0');
			}
			else
			{
				break;
			}
		}
	}

	return ret;
}

// 文字列を 64 bit 整数に変換
UINT64 SeToInt64(char *str)
{
	UINT len, i;
	UINT64 ret = 0;
	// 引数チェック
	if (str == NULL)
	{
		return 0;
	}

	len = SeStrLen(str);
	for (i = 0;i < len;i++)
	{
		char c = str[i];
		if (c != ',')
		{
			if ('0' <= c && c <= '9')
			{
				ret = ret * (UINT64)10 + (UINT64)(c - '0');
			}
			else
			{
				break;
			}
		}
	}

	return ret;
}

// 文字列を大文字・小文字を区別せずに比較する
int SeStrCmpi(char *str1, char *str2)
{
	UINT i;
	// 引数チェック
	if (str1 == NULL && str2 == NULL)
	{
		return 0;
	}
	if (str1 == NULL)
	{
		return 1;
	}
	if (str2 == NULL)
	{
		return -1;
	}

	// 文字列比較
	i = 0;
	while (true)
	{
		char c1, c2;

		c1 = SeToUpper(str1[i]);
		c2 = SeToUpper(str2[i]);
		if (c1 > c2)
		{
			return 1;
		}
		else if (c1 < c2)
		{
			return -1;
		}
		if (str1[i] == 0 || str2[i] == 0)
		{
			return 0;
		}
		i++;
	}
}
int SeStrnCmpi(char *str1, char *str2, UINT count)
{
	UINT i;
	// 引数チェック
	if (str1 == NULL && str2 == NULL)
	{
		return 0;
	}
	if (str1 == NULL)
	{
		return 1;
	}
	if (str2 == NULL)
	{
		return -1;
	}

	// 文字列比較
	i = 0;
	while (true)
	{
		char c1, c2;

		if (i >= count)
		{
			return 0;
		}

		c1 = SeToUpper(str1[i]);
		c2 = SeToUpper(str2[i]);
		if (c1 > c2)
		{
			return 1;
		}
		else if (c1 < c2)
		{
			return -1;
		}
		if (str1[i] == 0 || str2[i] == 0)
		{
			return 0;
		}
		i++;
	}
}

// 文字列を大文字・小文字を区別して比較する
int SeStrCmp(char *str1, char *str2)
{
	UINT i;
	// 引数チェック
	if (str1 == NULL && str2 == NULL)
	{
		return 0;
	}
	if (str1 == NULL)
	{
		return 1;
	}
	if (str2 == NULL)
	{
		return -1;
	}

	// 文字列比較
	i = 0;
	while (true)
	{
		char c1 = str1[i];
		char c2 = str2[i];

		if (c1 > c2)
		{
			return 1;
		}
		else if (c1 < c2)
		{
			return -1;
		}
		if (str1[i] == 0 || str2[i] == 0)
		{
			return 0;
		}
		i++;
	}
}
int SeStrnCmp(char *str1, char *str2, UINT count)
{
	UINT i;
	// 引数チェック
	if (str1 == NULL && str2 == NULL)
	{
		return 0;
	}
	if (str1 == NULL)
	{
		return 1;
	}
	if (str2 == NULL)
	{
		return -1;
	}

	// 文字列比較
	i = 0;
	while (true)
	{
		char c1 = str1[i];
		char c2 = str2[i];

		if (i >= count)
		{
			return 0;
		}

		if (c1 > c2)
		{
			return 1;
		}
		else if (c1 < c2)
		{
			return -1;
		}
		if (str1[i] == 0 || str2[i] == 0)
		{
			return 0;
		}
		i++;
	}
}

// 文字列を小文字にする
void SeStrLower(char *str)
{
	UINT len, i;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	len = SeStrLen(str);
	for (i = 0;i < len;i++)
	{
		str[i] = SeToLower(str[i]);
	}
}

// 文字列を大文字にする
void SeStrUpper(char *str)
{
	UINT len, i;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	len = SeStrLen(str);
	for (i = 0;i < len;i++)
	{
		str[i] = SeToUpper(str[i]);
	}
}

// 文字を大文字にする
char SeToUpper(char c)
{
	if ('a' <= c && c <= 'z')
	{
		c += 'Z' - 'z';
	}
	return c;
}

// 文字を小文字にする
char SeToLower(char c)
{
	if ('A' <= c && c <= 'Z')
	{
		c += 'z' - 'Z';
	}
	return c;
}

// 文字列の結合
UINT SeStrCat(char *dst, UINT size, char *src)
{
	UINT len1, len2, len_test;
	// 引数チェック
	if (dst == NULL || src == NULL)
	{
		return 0;
	}
	if (size == 0)
	{
		size = 0x7fffffff;
	}

	len1 = SeStrLen(dst);
	len2 = SeStrLen(src);
	len_test = len1 + len2 + 1;
	if (len_test > size)
	{
		if (len2 <= (len_test - size))
		{
			return 0;
		}
		len2 -= len_test - size;
	}

	SeCopy(dst + len1, src, len2);
	dst[len1 + len2] = '\0';

	return len1 + len2;
}

// 文字列のコピー
UINT SeStrCpy(char *dst, UINT size, char *src)
{
	UINT len;
	// 引数チェック
	if (dst == src)
	{
		return SeStrLen(src);
	}
	if (dst == NULL || src == NULL)
	{
		if (src == NULL && dst != NULL)
		{
			dst[0] = '\0';
		}
		return 0;
	}
	if (size == 1)
	{
		dst[0] = '\0';
		return 0;
	}
	if (size == 0)
	{
		size = 0x7fffffff;
	}

	len = SeStrLen(src);
	if (len <= (size - 1))
	{
		SeCopy(dst, src, len + 1);
	}
	else
	{
		len = size - 1;
		SeCopy(dst, src, len);
		dst[len] = '\0';
	}

	return len;
}

// 文字列を含むメモリサイズが指定したサイズ以下であることをチェック
bool SeStrCheckSize(char *str, UINT size)
{
	// 引数チェック
	if (str == NULL || size == 0)
	{
		return false;
	}

	return SeStrCheckLen(str, size - 1);
}

// 文字列の長さが指定した長さ以下であることをチェック
bool SeStrCheckLen(char *str, UINT len)
{
	UINT n, i;
	// 引数チェック
	if (str == NULL)
	{
		return false;
	}

	n = 0;

	for (i = 0;;i++)
	{
		if (str[i] == '\0')
		{
			return true;
		}
		n++;
		if (n > len)
		{
			return false;
		}
	}
}

// 文字列を含むメモリサイズの取得
UINT SeStrSize(char *str)
{
	// 引数チェック
	if (str == NULL)
	{
		return 0;
	}

	return SeStrLen(str) + sizeof(char);
}

// 文字列の長さの取得
UINT SeStrLen(char *str)
{
	UINT n;
	// 引数チェック
	if (str == NULL)
	{
		return 0;
	}

	n = 0;

	while (*(str++) != '\0')
	{
		n++;
	}

	return n;
}

