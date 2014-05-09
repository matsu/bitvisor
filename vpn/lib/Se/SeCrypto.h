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

// SeCrypto.h
// 概要: SeCrypto.c のヘッダ

#ifndef	SECRYPTOLOW_H
#define	SECRYPTOLOW_H

// 定数
#define SE_DES_KEY_SIZE					8			// DES 鍵サイズ
#define	SE_DES_IV_SIZE					8			// DES IV サイズ
#define SE_DES_BLOCK_SIZE				8			// DES ブロックサイズ
#define SE_3DES_KEY_SIZE				(8 * 3)		// 3DES 鍵サイズ
#define SE_RSA_KEY_SIZE					128			// RSA 鍵サイズ
#define SE_DH_KEY_SIZE					128			// DH 鍵サイズ
#define	SE_RSA_MIN_SIGN_HASH_SIZE		(15 + SE_SHA1_HASH_SIZE)	// 最小 RSA ハッシュサイズ
#define	SE_RSA_SIGN_HASH_SIZE			(SE_RSA_MIN_SIGN_HASH_SIZE)	// RSA ハッシュサイズ
#define SE_MD5_HASH_SIZE				16			// MD5 ハッシュサイズ
#define SE_SHA1_HASH_SIZE				20			// SHA-1 ハッシュサイズ
#define SE_SHA1_BLOCK_SIZE				64			// SHA-1 ブロックサイズ
#define SE_HMAC_SHA1_96_KEY_SIZE		20			// HMAC-SHA-1-96 鍵サイズ
#define SE_HMAC_SHA1_96_HASH_SIZE		12			// HMAC-SHA-1-96 ハッシュサイズ
#define SE_HMAC_SHA1_SIZE				(SE_SHA1_HASH_SIZE)	// HMAC-SHA-1 ハッシュサイズ

#define SE_DH_GROUP2_PRIME_1024 \
	"FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" \
	"29024E088A67CC74020BBEA63B139B22514A08798E3404DD" \
	"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" \
	"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED" \
	"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381" \
	"FFFFFFFFFFFFFFFF"

// マクロ
#define SE_RSA_HASHED_DATA(p)			(((UCHAR *)(p)) + 15)

// 証明書
struct SE_CERT
{
	X509 *x509;
	bool DontFree;
	bool Is1024bit;
};

// 鍵
struct SE_KEY
{
	EVP_PKEY *pkey;
	bool IsPrivateKey;
};

// DES の鍵要素
struct SE_DES_KEY_VALUE
{
	DES_key_schedule *KeySchedule;
	UCHAR KeyValue[SE_DES_KEY_SIZE];
};

// DES 鍵
struct SE_DES_KEY
{
	SE_DES_KEY_VALUE *k1, *k2, *k3;
};

// DH
struct SE_DH
{
	DH *dh;
	SE_BUF *MyPublicKey;
	SE_BUF *MyPrivateKey;
	SE_BUF *YourPublicKey;
	UINT Size;
};

// 共通部
void SeInitCrypto(bool init_openssl);
void SeFreeCrypto();
void SeRand(void *buf, UINT size);
UINT64 SeRand64();
UINT SeRand32();
USHORT SeRand16();
UCHAR SeRand8();
bool SeRand1();

SE_DES_KEY_VALUE *SeDesNewKeyValue(void *value);
SE_DES_KEY_VALUE *SeDesRandKeyValue();
void SeDesFreeKeyValue(SE_DES_KEY_VALUE *v);
SE_DES_KEY *SeDes3NewKey(void *k1, void *k2, void *k3);
void SeDes3FreeKey(SE_DES_KEY *k);
SE_DES_KEY *SeDesNewKey(void *k1);
void SeDesFreeKey(SE_DES_KEY *k);
SE_DES_KEY *SeDes3RandKey();
SE_DES_KEY *SeDesRandKey();
void SeDes3Encrypt(void *dest, void *src, UINT size, SE_DES_KEY *key, void *ivec);
void SeDes3Decrypt(void *dest, void *src, UINT size, SE_DES_KEY *key, void *ivec);

void SeSha1(void *dst, void *src, UINT size);
void SeMd5(void *dst, void *src, UINT size);
void SeMacSha1(void *dst, void *key, UINT key_size, void *data, UINT data_size);
void SeMacSha196(void *dst, void *key, void *data, UINT data_size);

BIO *SeBufToBio(SE_BUF *b);
SE_BUF *SeBioToBuf(BIO *bio);
BIO *SeNewBio();
void SeFreeBio(BIO *bio);
SE_CERT *SeBioToCert(BIO *bio, bool text);
SE_CERT *SeBufToCert(SE_BUF *b, bool text);
void SeFreeX509(X509 *x509);
void SeFreeCert(SE_CERT *x);
BIO *SeCertToBio(SE_CERT *x, bool text);
SE_BUF *SeCertToBuf(SE_CERT *x, bool text);
SE_KEY *SeBioToKey(BIO *bio, bool private_key, bool text, char *password);
int SePKeyPasswordCallbackFunction(char *buf, int bufsize, int verify, void *param);
void SeFreePKey(EVP_PKEY *pkey);
void SeFreeKey(SE_KEY *k);
BIO *SeKeyToBio(SE_KEY *k, bool text, char *password);
SE_BUF *SeKeyToBuf(SE_KEY *k, bool text, char *password);
SE_CERT *SeLoadCert(char *filename);
bool SeIsBase64(SE_BUF *b);
bool SeSaveCert(SE_CERT *x, char *filename, bool text);
SE_KEY *SeLoadKey(char *filename, bool private_key, char *password);
SE_KEY *SeBufToKey(SE_BUF *b, bool private_key, bool text, char *password);
bool SeSaveKey(SE_KEY *k, char *filename, bool text, char *password);
bool SeCheckCertAndKey(SE_CERT *x, SE_KEY *k);
bool SeCompareCert(SE_CERT *x1, SE_CERT *x2);
SE_KEY *SeGetKeyFromCert(SE_CERT *x);
bool SeCheckSignature(SE_CERT *x, SE_KEY *k);
SE_CERT *SeX509ToCert(X509 *x509);
bool SeCheckCert(SE_CERT *x, SE_CERT *x_issuer);
SE_CERT *SeCloneCert(SE_CERT *x);
SE_KEY *SeCloneKey(SE_KEY *k);
SE_BUF *SeGetCertSubjectName(SE_CERT *x);
SE_BUF *SeGetCertIssuerName(SE_CERT *x);
bool SeIsCertSignedByCert(SE_CERT *target_cert, SE_CERT *issuer_cert);

SE_BUF *SeBigNumToBuf(BIGNUM *bn);
BIGNUM *SeBinToBigNum(void *data, UINT size);
BIGNUM *SeBufToBigNum(SE_BUF *b);
void SeGetCertSha1(SE_CERT *x, void *hash);
void SeGetCertMd5(SE_CERT *x, void *hash);
char *SeBigNumToStr(BIGNUM *bn);

bool SeRsaGen(SE_KEY **priv, SE_KEY **pub, UINT bit);
UINT SeRsaPublicSize(SE_KEY *k);
bool SeRsaPublicEncrypt(void *dst, void *src, UINT size, SE_KEY *k);
bool SeRsaPrivateDecrypt(void *dst, void *src, UINT size, SE_KEY *k);
bool SeRsaPrivateEncrypt(void *dst, void *src, UINT size, SE_KEY *k);
bool SeRsaPublicDecrypt(void *dst, void *src, UINT size, SE_KEY *k);
bool SeRsaSignWithSha1(void *dst, void *src, UINT size, SE_KEY *k);
bool SeRsaVerifyWithSha1(void *data, UINT data_size, void *sign, SE_KEY *k);
bool SeHashForSignBySHA1(void *dst, UINT dst_size, void *src, UINT src_size);
bool SeRsaVerifyWithPadding(void *data, UINT data_size, void *sign, UINT sign_size, SE_KEY *k);
SE_BUF *SeRsaSignWithPadding(void *src, UINT size, SE_KEY *k);
SE_BUF *SeRsaPublicToBuf(SE_KEY *k);
void SeRsaPublicToBin(SE_KEY *k, void *data);

SE_DH *SeDhNew(char *prime, UINT g);
SE_DH *SeDhNewGroup2();
void SeDhFree(SE_DH *dh);
bool SeDhCompute(SE_DH *dh, void *dst_priv_key, void *src_pub_key, UINT key_size);

#endif	// SECRYPTOLOW_H

