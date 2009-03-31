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

 typedef struct idPasswordList {
	char *ID;
	unsigned char* password;
	unsigned short int passwordLen;
	struct  idPasswordList *next;
 } idPasswordList;


/** プロトタイプ宣言 */
 int  IDMan_IPInitializeReader(void);

 int  IDMan_IPInitialize( char* PIN,
			unsigned long int* SessionHandle);

 int  IDMan_IPFinalize( unsigned long int SessionHandle);

 int  IDMan_IPFinalizeReader( void );

 int  IDMan_getCertificate ( unsigned long int SessionHandle,
	 char* subjectDN,
	 char* issuerDN,
	 unsigned char* Cert,
	 unsigned short int * CertLen);

 int IDMan_getCertificateByIndex ( unsigned long int SessionHandle,
	 char PkcType,
	 int PkcIndex,
	 unsigned char* Cert,
	 unsigned short int* CertLen);

 int  IDMan_generateSignature ( unsigned long int SessionHandle,
	 char* subjectDN,
	 char* issuerDN,
	unsigned char* data,
	 unsigned short int dataLen,
	 unsigned char* signature,
	 unsigned short int* signatureLen,
	 int algorithm );

 int  IDMan_generateSignatureByIndex ( unsigned long int SessionHandle,
	 int PkcxIndex,
	unsigned char* data,
	 unsigned short int dataLen,
	 unsigned char* signature,
	 unsigned short int* signatureLen,
	 int algorithm );

 int  IDMan_EncryptByIndex ( unsigned long int SessionHandle,
	 int PkcxIndex,
	unsigned char* data,
	 unsigned short int dataLen,
	 unsigned char* signature,
	 unsigned short int* signatureLen,
	 int algorithm );

 int  IDMan_verifySignature ( unsigned long int SessionHandle,
	 char* subjectDN,
	 char* issuerDN,
	unsigned char* data,
	 unsigned short int dataLen,
	unsigned char* signature,
	 unsigned short int signatureLen,
	unsigned char* PKC,
	 unsigned short int PKCLen,
	int algorithm );

 int  IDMan_verifySignatureByIndex ( unsigned long int SessionHandle,
	 int PkczIndex,
	unsigned char* data,
	 unsigned short int dataLen,
	unsigned char* signature,
	 unsigned short int signatureLen,
	unsigned char* PKC,
	 unsigned short int PKCLen,
	int algorithm );

 int  IDMan_userAuthPKCS11 ( unsigned long int SessionHandle,
	 char* subjectDN,
	 char* issuerDN,
	 int algorithm );

 int IDMan_userAuthPKCS11ByIndex ( unsigned long int SessionHandle,
	 int PkcxIndex,
	 int algorithm );

 int  IDMan_getStaticPassword ( unsigned long int SessionHandle,
	 idPasswordList** list );

int IDMan_CmPadding( int  algorithm,
					  unsigned long int size,
					  void * data,
					  unsigned long int len,
					  void * PaddingData  );
int IDMan_CmPadding2( int  algorithm,
					  unsigned long int size,
					  void * data,
					  unsigned long int len,
					  void * PaddingData  );

 void IDMan_getMemoryFree ( unsigned char* data,
			 idPasswordList ** list );

int IDMan_CheckCardStatus ( unsigned long int SessionHandle ) ;

