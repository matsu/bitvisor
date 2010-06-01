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
 int  IDMan_IPInitialize( char* ,unsigned long int* );
 int  IDMan_IPFinalize( unsigned long int );
 int  IDMan_IPFinalizeReader( void );
 int  IDMan_getCertificate ( unsigned long int , char* , char* , unsigned char* , unsigned short int * );
 int  IDMan_getCertificateByIndex ( unsigned long int , char , int , unsigned char* , unsigned short int* );
 int  IDMan_generateSignature ( unsigned long int , char* , char* ,unsigned char* , unsigned short int , unsigned char* , unsigned short int* , int );
 int  IDMan_generateSignatureByIndex ( unsigned long int , int , unsigned char* , unsigned short int , unsigned char* , unsigned short int* , int );
 int  IDMan_EncryptByIndex ( unsigned long int , int , unsigned char* , unsigned short int , unsigned char* , unsigned short int* , int  );
 int  IDMan_verifySignature ( unsigned long int , char* , char* , unsigned char* , unsigned short int , unsigned char* , unsigned short int ,unsigned char* , unsigned short int ,int  );
 int  IDMan_verifySignatureByIndex ( unsigned long int , int ,unsigned char* , unsigned short int ,unsigned char* , unsigned short int ,unsigned char* , unsigned short int ,int  );
 int  IDMan_userAuthPKCS11 ( unsigned long int , char* , char* , int  );
 int  IDMan_userAuthPKCS11ByIndex ( unsigned long int , int , int  );
 int  IDMan_getStaticPassword ( unsigned long int ,	 idPasswordList**  );
 int  IDMan_CmPadding( int, unsigned long int,  void * ,  unsigned long int ,  void *   );
 void IDMan_getMemoryFree ( unsigned char* , idPasswordList **  );


long IDMan_SCardEstablishContext(unsigned long* );
long IDMan_SCardReleaseContext(unsigned long );
long IDMan_SCardListReaders(unsigned long , char* , unsigned long* );
long IDMan_SCardConnect(unsigned long , const char* , unsigned long , unsigned long* , unsigned long* );
long IDMan_SCardDisconnect(unsigned long , unsigned long );
long IDMan_SCardTransmit(unsigned long , unsigned long* , const unsigned char* , unsigned long , unsigned char* , unsigned long* );
long IDMan_SCardStatus(unsigned long , char* , unsigned long* , unsigned long* , unsigned long* , unsigned char* , unsigned long* );
long IDMan_SCardSizeLongToChar( unsigned long *, unsigned char * );
long IDMan_SCardSetBinay(unsigned long,unsigned long *,unsigned char *,unsigned long, unsigned char *, unsigned long *);

int IDMan_CheckCardStatus (unsigned long int SessionHandle);
