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

// SeStr.h
// 概要: SeStr.c のヘッダ

#ifndef	SESTR_H
#define	SESTR_H

// 文字列トークン
struct SE_TOKEN_LIST
{
	UINT NumTokens;
	char **Token;
};

// 関数プロトタイプ
UINT SeStrLen(char *str);
UINT SeStrSize(char *str);
bool SeStrCheckLen(char *str, UINT len);
bool SeStrCheckSize(char *str, UINT size);
UINT SeStrCpy(char *dst, UINT size, char *src);
UINT SeStrCat(char *dst, UINT size, char *src);
char SeToUpper(char c);
char SeToLower(char c);
void SeStrUpper(char *str);
void SeStrLower(char *str);
int SeStrCmp(char *str1, char *str2);
int SeStrnCmp(char *str1, char *str2, UINT count);
int SeStrCmpi(char *str1, char *str2);
int SeStrnCmpi(char *str1, char *str2, UINT count);
UINT64 SeToInt64(char *str);
UINT SeToInt(char *str);
void SeToStr64(char *str, UINT64 value);
void SeToStr(char *str, UINT value);
void SeToHex(char *str, UINT value);
void SeToHex64(char *str, UINT64 value);
UINT SeHexToInt(char *str);
UINT64 SeHexToInt64(char *str);
char Se4BitToHex(UINT value);
UINT SeHexTo4Bit(char c);
void SeTrim(char *str);
void SeTrimRight(char *str);
void SeTrimLeft(char *str);
char *SeCopyStr(char *str);
char *SeTrimCopy(char *str);
UINT SeReplaceStri(char *dst, UINT size, char *string, char *old_keyword, char *new_keyword);
UINT SeReplaceStr(char *dst, UINT size, char *string, char *old_keyword, char *new_keyword);
UINT SeReplaceStrEx(char *dst, UINT size, char *string, char *old_keyword, char *new_keyword, bool case_sensitive);
UINT SeCalcReplaceStrEx(char *string, char *old_keyword, char *new_keyword, bool case_sensitive);
UINT SeSearchStr(char *string, char *keyword, UINT start);
UINT SeSearchStri(char *string, char *keyword, UINT start);
UINT SeSearchStrEx(char *string, char *keyword, UINT start, bool case_sensitive);
void SeTrimCrlf(char *str);
bool SeEndWith(char *str, char *key);
bool SeStartWith(char *str, char *key);
bool SeIsEmptyStr(char *str);
char *SeNormalizeCrlf(char *str);
SE_TOKEN_LIST *SeParseToken(char *str, char *split_chars);
SE_TOKEN_LIST *SeParseTokenWithNullStr(char *str, char *split_chars);
void SeFreeToken(SE_TOKEN_LIST *t);
char *SeDefaultTokenSplitChars();
bool SeIsCharInStr(char *str, char c);
SE_TOKEN_LIST *SeNullTokenList();
void SeToStr3(char *str, UINT size, UINT64 v);
SE_TOKEN_LIST *SeUniqueToken(SE_TOKEN_LIST *t);
bool SeIsAllUpperStr(char *str);
bool SeIsAllLowerStr(char *str);
char *SeMakeCharArray(char c, UINT count);
char *SeFormatEx(char *fmt, va_list arg_list);
void SeFormatArgs(char *buf, UINT size, char *fmt, va_list args);
void SeFormat(char *buf, UINT size, char *fmt, ...);
void SeBinToStrEx(char *str, UINT str_size, void *data, UINT data_size);
void SeBinToStr(char *str, UINT str_size, void *data, UINT data_size);
SE_BUF *SeStrToBin(char *str);
bool SeStrToBinEx(void *dst, UINT dst_size, char *str);
bool SeStrToMac(UCHAR *mac_address, char *str);
void SeMacToStr(char *str, UINT str_size, UCHAR *mac_address);
char *SeReadNextLine(SE_BUF *b);

#endif	// SESTR_H

