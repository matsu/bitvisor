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

// SeConfig.h
// 概要: SeConfig.c のヘッダ

#ifndef	SECONFIG_H
#define	SECONFIG_H

// 設定ファイルのデータ
struct SE_CONFIG_ENTRY
{
	char *Name;
	char *Value;
};


// 関数プロトタイプ
SE_LIST *SeLoadConfigEntryList(char *name);
SE_LIST *SeLoadConfigEntryListFromBuf(SE_BUF *b);
SE_CONFIG_ENTRY *SeGetConfigEntry(SE_LIST *o, char *name);
void SeFreeConfigEntryList(SE_LIST *o);
bool SeGetKeyAndValue(char *str, char *key, UINT key_size, char *value, UINT value_size, char *split_str);
bool SeIsSplitChar(char c, char *split_str);
char *SeGetConfigStr(SE_LIST *o, char *name);
bool SeGetConfigBool(SE_LIST *o, char *name);
UINT SeGetConfigInt(SE_LIST *o, char *name);
UINT64 SeGetConfigInt64(SE_LIST *o, char *name);
SE_BUF *SeGetConfigBin(SE_LIST *o, char *name);

#endif	// SECONFIG_H

