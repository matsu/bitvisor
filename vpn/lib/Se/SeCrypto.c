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

// SeCrypto.c
// 概要: 暗号化ライブラリ

#define SE_INTERNAL
#define SECRYPTO_C

#define	CHELP_OPENSSL_SOURCE

#include <chelp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/pkcs7.h>
#include <openssl/pkcs12.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/des.h>
#include <openssl/dh.h>
#include <openssl/pem.h>
#include <Se/Se.h>

// コールバック関数用
typedef struct CB_PARAM
{
	char *password;
} CB_PARAM;

// HMAC-SHA-1-96 の計算
void SeMacSha196(void *dst, void *key, void *data, UINT data_size)
{
	UCHAR tmp[SE_HMAC_SHA1_SIZE];
	// 引数チェック
	if (dst == NULL || key == NULL || data == NULL)
	{
		return;
	}

	SeMacSha1(tmp, key, SE_HMAC_SHA1_96_KEY_SIZE, data, data_size);

	SeCopy(dst, tmp, SE_HMAC_SHA1_96_HASH_SIZE);
}

// HMAC-SHA-1 の計算
void SeMacSha1(void *dst, void *key, UINT key_size, void *data, UINT data_size)
{
	UCHAR key_plus[SE_SHA1_BLOCK_SIZE];
	UCHAR key_plus2[SE_SHA1_BLOCK_SIZE];
	UCHAR key_plus5[SE_SHA1_BLOCK_SIZE];
	UCHAR hash4[SE_SHA1_HASH_SIZE];
	UINT i;
	SE_BUF *buf3;
	SE_BUF *buf6;
	// 引数チェック
	if (dst == NULL || key == NULL || data == NULL)
	{
		return;
	}

	SeZero(key_plus, sizeof(key_plus));
	if (key_size <= SE_SHA1_BLOCK_SIZE)
	{
		SeCopy(key_plus, key, key_size);
	}
	else
	{
		SeSha1(key_plus, key, key_size);
	}

	for (i = 0;i < sizeof(key_plus);i++)
	{
		key_plus2[i] = key_plus[i] ^ 0x36;
	}

	buf3 = SeNewBuf();
	SeWriteBuf(buf3, key_plus2, sizeof(key_plus2));
	SeWriteBuf(buf3, data, data_size);

	SeSha1(hash4, buf3->Buf, buf3->Size);

	for (i = 0;i < sizeof(key_plus);i++)
	{
		key_plus5[i] = key_plus[i] ^ 0x5c;
	}

	buf6 = SeNewBuf();
	SeWriteBuf(buf6, key_plus5, sizeof(key_plus5));
	SeWriteBuf(buf6, hash4, sizeof(hash4));

	SeSha1(dst, buf6->Buf, buf6->Size);

	SeFreeBuf(buf3);
	SeFreeBuf(buf6);
}

// DH 計算
bool SeDhCompute(SE_DH *dh, void *dst_priv_key, void *src_pub_key, UINT key_size)
{
	int i;
	BIGNUM *bn;
	bool ret = false;
	// 引数チェック
	if (dh == NULL || dst_priv_key == NULL || src_pub_key == NULL)
	{
		return false;
	}
	if (key_size != dh->Size)
	{
		return false;
	}

	bn = SeBinToBigNum(src_pub_key, key_size);

	i = DH_compute_key(dst_priv_key, bn, dh->dh);

	if (i == key_size)
	{
		ret = true;
	}

	BN_free(bn);

	return ret;
}

// DH GROUP2 の作成
SE_DH *SeDhNewGroup2()
{
	return SeDhNew(SE_DH_GROUP2_PRIME_1024, 2);
}

// 新しい DH の作成
SE_DH *SeDhNew(char *prime, UINT g)
{
	SE_DH *dh;
	SE_BUF *buf;
	// 引数チェック
	if (prime == NULL || g == 0)
	{
		return NULL;
	}

	buf = SeStrToBin(prime);

	dh = SeZeroMalloc(sizeof(SE_DH));

	dh->dh = DH_new();

	dh->dh->p = SeBinToBigNum(buf->Buf, buf->Size);

	dh->dh->g = BN_new();
	BN_set_word(dh->dh->g, g);

	DH_generate_key(dh->dh);

	dh->MyPublicKey = SeBigNumToBuf(dh->dh->pub_key);
	dh->MyPrivateKey = SeBigNumToBuf(dh->dh->priv_key);

	dh->Size = buf->Size;

	SeFreeBuf(buf);

	return dh;
}

// DH の解放
void SeDhFree(SE_DH *dh)
{
	// 引数チェック
	if (dh == NULL)
	{
		return;
	}

	DH_free(dh->dh);

	SeFreeBuf(dh->MyPrivateKey);
	SeFreeBuf(dh->MyPublicKey);
	SeFreeBuf(dh->YourPublicKey);

	SeFree(dh);
}

// BIGNUM を文字列に変換
char *SeBigNumToStr(BIGNUM *bn)
{
	BIO *bio;
	SE_BUF *b;
	char *ret;
	// 引数チェック
	if (bn == NULL)
	{
		return NULL;
	}

	bio = SeNewBio();

	BN_print(bio, bn);

	b = SeBioToBuf(bio);

	SeFreeBio(bio);

	ret = SeZeroMalloc(b->Size + 1);
	SeCopy(ret, b->Buf, b->Size);
	SeFreeBuf(b);

	return ret;
}

// 公開鍵をバッファに変換
SE_BUF *SeRsaPublicToBuf(SE_KEY *k)
{
	SE_BUF *b;
	// 引数チェック
	if (k == NULL || k->pkey == NULL || k->pkey->pkey.rsa == NULL
		|| k->pkey->pkey.rsa->n == NULL)
	{
		return NULL;
	}

	b = SeBigNumToBuf(k->pkey->pkey.rsa->n);
	if (b == NULL)
	{
		return NULL;
	}

	return b;
}

// 公開鍵をバイナリに変換
void SeRsaPublicToBin(SE_KEY *k, void *data)
{
	SE_BUF *b;
	// 引数チェック
	if (k == NULL || k->pkey == NULL || k->pkey->pkey.rsa == NULL
		|| k->pkey->pkey.rsa->n == NULL || data == NULL)
	{
		return;
	}

	b = SeBigNumToBuf(k->pkey->pkey.rsa->n);
	if (b == NULL)
	{
		return;
	}

	SeCopy(data, b->Buf, b->Size);

	SeFreeBuf(b);
}

// SHA-1 による署名データの生成
bool SeHashForSignBySHA1(void *dst, UINT dst_size, void *src, UINT src_size)
{
	UCHAR *buf = (UCHAR *)dst;
	UCHAR sign_data[] =
	{
		0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2B, 0x0E,
		0x03, 0x02, 0x1A, 0x05, 0x00, 0x04, 0x14,
	};
	// 引数チェック
	if (dst == NULL || src == NULL || src_size == 0 || SE_RSA_MIN_SIGN_HASH_SIZE > dst_size)
	{
		return false;
	}

	// ヘッダ部分
	SeCopy(buf, sign_data, sizeof(sign_data));

	// ハッシュ
	SeSha1(SE_RSA_HASHED_DATA(buf), src, src_size);

	return true;
}

// RSA 署名の検査 (SHA-1 ハッシュ)
bool SeRsaVerifyWithSha1(void *data, UINT data_size, void *sign, SE_KEY *k)
{
	UCHAR hash_data[SE_RSA_SIGN_HASH_SIZE];
	UCHAR decrypt_data[SE_RSA_SIGN_HASH_SIZE];
	// 引数チェック
	if (data == NULL || sign == NULL || k == NULL || k->IsPrivateKey != false)
	{
		return false;
	}

	// データをハッシュ
	if (SeHashForSignBySHA1(hash_data, sizeof(hash_data), data, data_size) == false)
	{
		return false;
	}

	// 署名を解読
	if (RSA_public_decrypt(128, sign, decrypt_data, k->pkey->pkey.rsa, RSA_PKCS1_PADDING) <= 0)
	{
		return false;
	}

	// 比較
	if (SeCmp(decrypt_data, hash_data, sizeof(SE_RSA_SIGN_HASH_SIZE)) != 0)
	{
		return false;
	}

	return true;
}

// RSA 署名の検査 (パディングのみ)
bool SeRsaVerifyWithPadding(void *data, UINT data_size, void *sign, UINT sign_size, SE_KEY *k)
{
	UINT rsa_size;
	UCHAR *tmp;
	int ret;
	bool b;
	// 引数チェック
	if (data == NULL || sign == NULL || k == NULL || k->IsPrivateKey != false)
	{
		return false;
	}

	rsa_size = RSA_size(k->pkey->pkey.rsa);
	tmp = SeMalloc(rsa_size);

	// 署名を解読
	ret = RSA_public_decrypt(sign_size, sign, tmp, k->pkey->pkey.rsa, RSA_PKCS1_PADDING);

	if (ret <= 0 || ret != (int)data_size)
	{
		SeFree(tmp);
		return false;
	}

	b = ((SeCmp(tmp, data, data_size) == 0) ? true : false);

	SeFree(tmp);

	return b;
}

// RSA 署名 (パディングのみ)
SE_BUF *SeRsaSignWithPadding(void *src, UINT size, SE_KEY *k)
{
	UINT rsa_size;
	UCHAR *tmp;
	int ret;
	SE_BUF *b;
	// 引数チェック
	if (src == NULL || k == NULL || k->IsPrivateKey == false ||  k->pkey->type != EVP_PKEY_RSA)
	{
		return NULL;
	}

	rsa_size = RSA_size(k->pkey->pkey.rsa);
	tmp = SeMalloc(rsa_size);

	// データを暗号化
	ret = RSA_private_encrypt(size, src, tmp, k->pkey->pkey.rsa, RSA_PKCS1_PADDING);
	if (ret <= 0 || ret != (int)rsa_size)
	{
		SeFree(tmp);
		return NULL;
	}

	b = SeMemToBuf(tmp, rsa_size);
	SeFree(tmp);

	return b;
}

// RSA 署名 (SHA-1 ハッシュ)
bool SeRsaSignWithSha1(void *dst, void *src, UINT size, SE_KEY *k)
{
	UCHAR hash[SE_RSA_SIGN_HASH_SIZE];
	// 引数チェック
	if (dst == NULL || src == NULL || k->pkey->type != EVP_PKEY_RSA)
	{
		return false;
	}

	SeZero(dst, 128);

	// ハッシュ
	if (SeHashForSignBySHA1(hash, sizeof(hash), src, size) == false)
	{
		return false;
	}

	// 署名
	if (RSA_private_encrypt(sizeof(hash), hash, dst, k->pkey->pkey.rsa, RSA_PKCS1_PADDING) <= 0)
	{
		return false;
	}

	return true;
}

// RSA 公開鍵による復号化
bool SeRsaPublicDecrypt(void *dst, void *src, UINT size, SE_KEY *k)
{
	void *tmp;
	int ret;
	// 引数チェック
	if (src == NULL || size == 0 || k == NULL)
	{
		return false;
	}

	tmp = SeZeroMalloc(size);

	ret = RSA_public_decrypt(size, src, tmp, k->pkey->pkey.rsa, RSA_NO_PADDING);

	if (ret <= 0)
	{
		SeFree(tmp);
		return false;
	}

	SeCopy(dst, tmp, size);
	SeFree(tmp);

	return true;
}

// RSA 秘密鍵による暗号化
bool SeRsaPrivateEncrypt(void *dst, void *src, UINT size, SE_KEY *k)
{
	void *tmp;
	int ret;
	// 引数チェック
	if (src == NULL || size == 0 || k == NULL)
	{
		return false;
	}

	tmp = SeZeroMalloc(size);

	ret = RSA_private_encrypt(size, src, tmp, k->pkey->pkey.rsa, RSA_NO_PADDING);

	if (ret <= 0)
	{
		SeFree(tmp);
		return false;
	}

	SeCopy(dst, tmp, size);
	SeFree(tmp);

	return true;
}

// RSA 秘密鍵による復号化
bool SeRsaPrivateDecrypt(void *dst, void *src, UINT size, SE_KEY *k)
{
	void *tmp;
	int ret;
	// 引数チェック
	if (src == NULL || size == 0 || k == NULL)
	{
		return false;
	}

	tmp = SeZeroMalloc(size);
	ret = RSA_private_decrypt(size, src, tmp, k->pkey->pkey.rsa, RSA_NO_PADDING);
	if (ret <= 0)
	{
		return false;
	}

	SeCopy(dst, tmp, size);
	SeFree(tmp);

	return true;
}

// RSA 公開鍵による暗号化
bool SeRsaPublicEncrypt(void *dst, void *src, UINT size, SE_KEY *k)
{
	void *tmp;
	int ret;
	// 引数チェック
	if (src == NULL || size == 0 || k == NULL)
	{
		return false;
	}

	tmp = SeZeroMalloc(size);
	ret = RSA_public_encrypt(size, src, tmp, k->pkey->pkey.rsa, RSA_NO_PADDING);
	if (ret <= 0)
	{
		return false;
	}

	SeCopy(dst, tmp, size);
	SeFree(tmp);

	return true;
}

// 公開鍵サイズの取得
UINT SeRsaPublicSize(SE_KEY *k)
{
	SE_BUF *b;
	UINT ret;
	// 引数チェック
	if (k == NULL || k->pkey == NULL || k->pkey->pkey.rsa == NULL
		|| k->pkey->pkey.rsa->n == NULL)
	{
		return 0;
	}

	b = SeBigNumToBuf(k->pkey->pkey.rsa->n);
	if (b == NULL)
	{
		return 0;
	}

	ret = b->Size;

	SeFreeBuf(b);

	return ret;
}

// RSA 鍵の生成
bool SeRsaGen(SE_KEY **priv, SE_KEY **pub, UINT bit)
{
	RSA *rsa;
	SE_KEY *priv_key, *pub_key;
	BIO *bio;
	UINT size = 0;
	// 引数チェック
	if (priv == NULL || pub == NULL)
	{
		return false;
	}
	if (bit == 0)
	{
		bit = 1024;
	}

	// 鍵生成
	rsa = RSA_generate_key(bit, RSA_F4, NULL, NULL);

	// 秘密鍵
	bio = SeNewBio();
	i2d_RSAPrivateKey_bio(bio, rsa);
	BIO_seek(bio, 0);
	priv_key = SeBioToKey(bio, true, false, NULL);
	SeFreeBio(bio);

	// 公開鍵
	bio = SeNewBio();
	i2d_RSA_PUBKEY_bio(bio, rsa);
	BIO_seek(bio, 0);
	pub_key = SeBioToKey(bio, false, false, NULL);
	SeFreeBio(bio);

	*priv = priv_key;
	*pub = pub_key;

	RSA_free(rsa);

	size = SeRsaPublicSize(*pub);

	if (size != ((bit + 7) / 8))
	{
		SeFreeKey(*priv);
		SeFreeKey(*pub);

		return SeRsaGen(priv, pub, bit);
	}

	return true;
}

// 証明書の SHA-1 ハッシュの取得
void SeGetCertSha1(SE_CERT *x, void *hash)
{
	UINT size = SE_SHA1_HASH_SIZE;
	// 引数チェック
	if (x == NULL || hash == NULL)
	{
		return;
	}

	X509_digest(x->x509, EVP_sha1(), hash, (unsigned int *)&size);
}

// 証明書の MD5 ハッシュの取得
void SeGetCertMd5(SE_CERT *x, void *hash)
{
	UINT size = SE_MD5_HASH_SIZE;
	// 引数チェック
	if (x == NULL || hash == NULL)
	{
		return;
	}

	X509_digest(x->x509, EVP_md5(), hash, (unsigned int *)&size);
}

// バッファを BIGNUM に変換
BIGNUM *SeBufToBigNum(SE_BUF *b)
{
	if (b == NULL)
	{
		return NULL;
	}

	return SeBinToBigNum(b->Buf, b->Size);
}

// バイナリを BIGNUM に変換
BIGNUM *SeBinToBigNum(void *data, UINT size)
{
	BIGNUM *bn;
	// 引数チェック
	if (data == NULL)
	{
		return NULL;
	}

	bn = BN_new();
	BN_bin2bn(data, size, bn);

	return bn;
}

// BIGNUM をバッファに変換
SE_BUF *SeBigNumToBuf(BIGNUM *bn)
{
	UINT size;
	UCHAR *tmp;
	SE_BUF *b;
	// 引数チェック
	if (bn == NULL)
	{
		return NULL;
	}

	size = BN_num_bytes(bn);
	tmp = SeZeroMalloc(size);
	BN_bn2bin(bn, tmp);

	b = SeNewBuf();
	SeWriteBuf(b, tmp, size);
	SeFree(tmp);

	SeSeekBuf(b, 0, 0);

	return b;
}

// 鍵のクローン
SE_KEY *SeCloneKey(SE_KEY *k)
{
	SE_BUF *b;
	SE_KEY *ret;
	// 引数チェック
	if (k == NULL)
	{
		return NULL;
	}

	b = SeKeyToBuf(k, false, NULL);
	if (b == NULL)
	{
		return NULL;
	}

	ret = SeBufToKey(b, k->IsPrivateKey, false, NULL);
	SeFreeBuf(b);

	return ret;
}

// 証明書のクローン
SE_CERT *SeCloneCert(SE_CERT *x)
{
	SE_BUF *b;
	SE_CERT *ret;
	// 引数チェック
	if (x == NULL)
	{
		return NULL;
	}

	b = SeCertToBuf(x, false);
	if (b == NULL)
	{
		return NULL;
	}

	ret = SeBufToCert(b, false);
	SeFreeBuf(b);

	return ret;
}

// 証明書 X が証明書 x_issuer の発行者によって署名されているかどうか確認する
bool SeCheckCert(SE_CERT *x, SE_CERT *x_issuer)
{
	SE_KEY *k;
	bool ret;
	// 引数チェック
	if (x == NULL || x_issuer == NULL)
	{
		return false;
	}

	k = SeGetKeyFromCert(x_issuer);
	if (k == NULL)
	{
		return false;
	}

	ret = SeCheckSignature(x, k);
	SeFreeKey(k);

	return ret;
}

// BUF を CERT に変換する
SE_CERT *SeBufToCert(SE_BUF *b, bool text)
{
	SE_CERT *x;
	BIO *bio;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	bio = SeBufToBio(b);
	if (bio == NULL)
	{
		return NULL;
	}

	x = SeBioToCert(bio, text);

	SeFreeBio(bio);

	return x;
}

// X509 を CERT に変換する
SE_CERT *SeX509ToCert(X509 *x509)
{
	SE_CERT *x;
	SE_KEY *k;
	SE_BUF *b;
	UINT size;
	UINT type;
	// 引数チェック
	if (x509 == NULL)
	{
		return NULL;
	}

	x = SeZeroMalloc(sizeof(SE_CERT));
	x->x509 = x509;

	k = SeGetKeyFromCert(x);
	if (k == NULL)
	{
		SeFreeCert(x);
		return NULL;
	}

	b = SeKeyToBuf(k, false, NULL);

	size = b->Size;
	type = k->pkey->type;

	SeFreeBuf(b);

	SeFreeKey(k);

	if ((size == 162 || size == 294) && type == EVP_PKEY_RSA)
	{
		// 1024bit
		x->Is1024bit = true;
	}

	return x;
}

// 指定された証明書が別の証明書によって署名されているかどうか検証する
bool SeIsCertSignedByCert(SE_CERT *target_cert, SE_CERT *issuer_cert)
{
	SE_KEY *k;
	bool ret;
	// 引数チェック
	if (target_cert == NULL || issuer_cert == NULL)
	{
		return false;
	}

	if (SeCompareCert(target_cert, issuer_cert))
	{
		// 2 つの証明書が同一の場合
		return true;
	}

	k = SeGetKeyFromCert(issuer_cert);
	if (k == NULL)
	{
		return false;
	}

	ret = SeCheckSignature(target_cert, k);

	SeFreeKey(k);

	return ret;
}

// 証明書 X の署名を公開鍵 K で確認する
bool SeCheckSignature(SE_CERT *x, SE_KEY *k)
{
	// 引数チェック
	if (x == NULL || k == NULL)
	{
		return false;
	}

	if (X509_verify(x->x509, k->pkey) == 0)
	{
		return false;
	}

	return true;
}

// 証明書から公開鍵を取得する
SE_KEY *SeGetKeyFromCert(SE_CERT *x)
{
	EVP_PKEY *pkey;
	SE_KEY *k;
	// 引数チェック
	if (x == NULL)
	{
		return NULL;
	}

	pkey = X509_get_pubkey(x->x509);

	if (pkey == NULL)
	{
		return NULL;
	}

	k = SeZeroMalloc(sizeof(EVP_PKEY));
	k->pkey = pkey;

	return k;
}

// 証明書 x1 と x2 が等しいかどうかチェックする
bool SeCompareCert(SE_CERT *x1, SE_CERT *x2)
{
	// 引数チェック
	if (x1 == NULL || x2 == NULL)
	{
		return false;
	}

	if (X509_cmp(x1->x509, x2->x509) == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

// KEY が CERT の秘密鍵かどうかチェックする
bool SeCheckCertAndKey(SE_CERT *x, SE_KEY *k)
{
	// 引数チェック
	if (x == NULL || k == NULL)
	{
		return false;
	}

	if (X509_check_private_key(x->x509, k->pkey) != 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

// BUF を KEY に変換
SE_KEY *SeBufToKey(SE_BUF *b, bool private_key, bool text, char *password)
{
	BIO *bio;
	SE_KEY *k;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	bio = SeBufToBio(b);
	k = SeBioToKey(bio, private_key, text, password);
	SeFreeBio(bio);

	return k;
}

// KEY をファイルに保存する
bool SeSaveKey(SE_KEY *k, char *filename, bool text, char *password)
{
	SE_BUF *b;
	bool ret;
	// 引数チェック
	if (k == NULL || filename == NULL)
	{
		return false;
	}

	b = SeKeyToBuf(k, text, password);
	if (b == NULL)
	{
		return false;
	}

	ret = SeDumpBuf(b, filename);
	SeFreeBuf(b);

	return ret;
}

// ファイルから KEY を読み込む
SE_KEY *SeLoadKey(char *filename, bool private_key, char *password)
{
	bool text;
	SE_BUF *b;
	SE_KEY *k;
	// 引数チェック
	if (filename == NULL)
	{
		return NULL;
	}

	b = SeReadDump(filename);
	if (b == NULL)
	{
		return NULL;
	}

	text = SeIsBase64(b);
	if (text == false)
	{
		k = SeBufToKey(b, private_key, false, NULL);
	}
	else
	{
		k = SeBufToKey(b, private_key, true, NULL);
		if (k == NULL)
		{
			k = SeBufToKey(b, private_key, true, password);
		}
	}

	SeFreeBuf(b);

	return k;
}

// BUF が Base64 エンコードされているかどうか調べる
bool SeIsBase64(SE_BUF *b)
{
	UINT i;
	// 引数チェック
	if (b == NULL)
	{
		return false;
	}

	for (i = 0;i < b->Size;i++)
	{
		char c = ((char *)b->Buf)[i];
		bool b = false;
		if ('a' <= c && c <= 'z')
		{
			b = true;
		}
		else if ('A' <= c && c <= 'Z')
		{
			b = true;
		}
		else if ('0' <= c && c <= '9')
		{
			b = true;
		}
		else if (c == ':' || c == '.' || c == ';' || c == ',')
		{
			b = true;
		}
		else if (c == '!' || c == '&' || c == '#' || c == '(' || c == ')')
		{
			b = true;
		}
		else if (c == '-' || c == ' ')
		{
			b = true;
		}
		else if (c == 13 || c == 10 || c == -1)
		{
			b = true;
		}
		else if (c == '\t' || c == '=' || c == '+' || c == '/')
		{
			b = true;
		}
		if (b == false)
		{
			return false;
		}
	}
	return true;
}

// CERT をファイルに書き出す
bool SeSaveCert(SE_CERT *x, char *filename, bool text)
{
	SE_BUF *b;
	bool ret;
	// 引数チェック
	if (x == NULL || filename == NULL)
	{
		return false;
	}

	b = SeCertToBuf(x, text);
	if (b == NULL)
	{
		return false;
	}

	ret = SeDumpBuf(b, filename);
	SeFreeBuf(b);

	return ret;
}

// ファイルから CERT を読み込む
SE_CERT *SeLoadCert(char *filename)
{
	bool text;
	SE_BUF *b;
	SE_CERT *x;
	// 引数チェック
	if (filename == NULL)
	{
		return NULL;
	}

	b = SeReadDump(filename);
	text = SeIsBase64(b);

	x = SeBufToCert(b, text);
	SeFreeBuf(b);

	return x;
}

// KEY を BUF に変換する
SE_BUF *SeKeyToBuf(SE_KEY *k, bool text, char *password)
{
	SE_BUF *buf;
	BIO *bio;
	// 引数チェック
	if (k == NULL)
	{
		return NULL;
	}

	bio = SeKeyToBio(k, text, password);
	if (bio == NULL)
	{
		return NULL;
	}

	buf = SeBioToBuf(bio);
	SeFreeBio(bio);

	SeSeekBuf(buf, 0, 0);

	return buf;
}

// KEY を BIO に変換する
BIO *SeKeyToBio(SE_KEY *k, bool text, char *password)
{
	BIO *bio;
	// 引数チェック
	if (k == NULL)
	{
		return NULL;
	}

	bio = SeNewBio();

	if (k->IsPrivateKey)
	{
		// 秘密鍵
		if (text == false)
		{
			// バイナリ形式
			i2d_PrivateKey_bio(bio, k->pkey);
		}
		else
		{
			// テキスト形式
			if (password == 0 || SeStrLen(password) == 0)
			{
				// 暗号化無し
				PEM_write_bio_PrivateKey(bio, k->pkey, NULL, NULL, 0, NULL, NULL);
			}
			else
			{
				// 暗号化する
				CB_PARAM cb;
				cb.password = password;
				PEM_write_bio_PrivateKey(bio, k->pkey, EVP_des_ede3_cbc(),
					NULL, 0, (pem_password_cb *)SePKeyPasswordCallbackFunction, &cb);
			}
		}
	}
	else
	{
		// 公開鍵
		if (text == false)
		{
			// バイナリ形式
			i2d_PUBKEY_bio(bio, k->pkey);
		}
		else
		{
			// テキスト形式
			PEM_write_bio_PUBKEY(bio, k->pkey);
		}
	}

	return bio;
}

// KEY を解放
void SeFreeKey(SE_KEY *k)
{
	// 引数チェック
	if (k == NULL)
	{
		return;
	}

	SeFreePKey(k->pkey);
	SeFree(k);
}

// 秘密鍵を解放
void SeFreePKey(EVP_PKEY *pkey)
{
	// 引数チェック
	if (pkey == NULL)
	{
		return;
	}

	EVP_PKEY_free(pkey);
}

// パスワードコールバック関数
int SePKeyPasswordCallbackFunction(char *buf, int bufsize, int verify, void *param)
{
	CB_PARAM *cb;
	// 引数チェック
	if (buf == NULL || param == NULL || bufsize == 0)
	{
		return 0;
	}

	cb = (CB_PARAM *)param;
	if (cb->password == NULL)
	{
		return 0;
	}

	return SeStrCpy(buf, bufsize, cb->password);
}

// BIO を KEY に変換する
SE_KEY *SeBioToKey(BIO *bio, bool private_key, bool text, char *password)
{
	EVP_PKEY *pkey;
	SE_KEY *k;
	// 引数チェック
	if (bio == NULL)
	{
		return NULL;
	}

	if (password != NULL && SeStrLen(password) == 0)
	{
		password = NULL;
	}

	if (private_key == false)
	{
		// 公開鍵
		if (text == false)
		{
			// バイナリ形式
			pkey = d2i_PUBKEY_bio(bio, NULL);
			if (pkey == NULL)
			{
				return NULL;
			}
		}
		else
		{
			// テキスト形式
			CB_PARAM cb;
			cb.password = password;

			pkey = PEM_read_bio_PUBKEY(bio, NULL, (pem_password_cb *)SePKeyPasswordCallbackFunction, &cb);

			if (pkey == NULL)
			{
				return NULL;
			}
		}
	}
	else
	{
		if (text == false)
		{
			// バイナリ形式
			pkey = d2i_PrivateKey_bio(bio, NULL);

			if (pkey == NULL)
			{
				return NULL;
			}
		}
		else
		{
			// テキスト形式
			CB_PARAM cb;
			cb.password = password;

			pkey = PEM_read_bio_PrivateKey(bio, NULL, (pem_password_cb *)SePKeyPasswordCallbackFunction, &cb);

			if (pkey == NULL)
			{
				return NULL;
			}
		}
	}

	k = SeZeroMalloc(sizeof(SE_KEY));
	k->pkey = pkey;
	k->IsPrivateKey = private_key;

	return k;
}

// CERT を BUF に変換する
SE_BUF *SeCertToBuf(SE_CERT *x, bool text)
{
	BIO *bio;
	SE_BUF *b;
	// 引数チェック
	if (x == NULL)
	{
		return NULL;
	}

	bio = SeCertToBio(x, text);
	if (bio == NULL)
	{
		return NULL;
	}

	b = SeBioToBuf(bio);
	SeFreeBio(bio);

	SeSeekBuf(b, 0, 0);

	return b;
}

// CERT を BIO に変換する
BIO *SeCertToBio(SE_CERT *x, bool text)
{
	BIO *bio;
	// 引数チェック
	if (x == NULL)
	{
		return NULL;
	}

	bio = SeNewBio();

	if (text == false)
	{
		// バイナリ形式
		i2d_X509_bio(bio, x->x509);
	}
	else
	{
		// テキスト形式
		PEM_write_bio_X509(bio, x->x509);
	}

	return bio;
}

// CERT の IssuerName の取得
SE_BUF *SeGetCertIssuerName(SE_CERT *x)
{
	X509 *x509;
	UINT size;
	UCHAR *tmp;
	UCHAR *out;
	SE_BUF *ret;
	// 引数チェック
	if (x == NULL)
	{
		return NULL;
	}

	x509 = x->x509;

	size = (UINT)i2d_X509_NAME(x509->cert_info->issuer, NULL);
	if (size == 0)
	{
		return SeNewBuf();
	}

	tmp = SeZeroMalloc(size);
	out = NULL;
	size = (UINT)i2d_X509_NAME(x509->cert_info->issuer, &out);

	SeCopy(tmp, out, size);

	ret = SeMemToBuf(tmp, size);

	SeFree(tmp);

	return ret;
}

// CERT の SubjectName の取得
SE_BUF *SeGetCertSubjectName(SE_CERT *x)
{
	X509 *x509;
	UINT size;
	UCHAR *tmp;
	UCHAR *out;
	SE_BUF *ret;
	// 引数チェック
	if (x == NULL)
	{
		return NULL;
	}

	x509 = x->x509;

	size = (UINT)i2d_X509_NAME(x509->cert_info->subject, NULL);
	if (size == 0)
	{
		return SeNewBuf();
	}

	tmp = SeZeroMalloc(size);
	out = NULL;
	size = (UINT)i2d_X509_NAME(x509->cert_info->subject, &out);

	SeCopy(tmp, out, size);

	ret = SeMemToBuf(tmp, size);

	SeFree(tmp);

	return ret;
}

// CERT の解放
void SeFreeCert(SE_CERT *x)
{
	// 引数チェック
	if (x == NULL)
	{
		return;
	}

	if (x->DontFree == false)
	{
		SeFreeX509(x->x509);
	}
	SeFree(x);
}

// X509 の解放
void SeFreeX509(X509 *x509)
{
	// 引数チェック
	if (x509 == NULL)
	{
		return;
	}

	X509_free(x509);
}

// BIO を CERT に変換する
SE_CERT *SeBioToCert(BIO *bio, bool text)
{
	SE_CERT *x;
	X509 *x509;
	// 引数チェック
	if (bio == NULL)
	{
		return NULL;
	}

	// x509 の読み込み
	if (text == false)
	{
		// バイナリモード
		x509 = d2i_X509_bio(bio, NULL);
	}
	else
	{
		// テキストモード
		x509 = PEM_read_bio_X509(bio, NULL, NULL, NULL);
	}

	if (x509 == NULL)
	{
		return NULL;
	}

	x = SeX509ToCert(x509);

	if (x == NULL)
	{
		return NULL;
	}

	return x;
}

// BIO を解放する
void SeFreeBio(BIO *bio)
{
	// 引数チェック
	if (bio == NULL)
	{
		return;
	}

	BIO_free(bio);
}

// BIO を作成する
BIO *SeNewBio()
{
	return BIO_new(BIO_s_mem());
}

// BIO を BUF に変換
SE_BUF *SeBioToBuf(BIO *bio)
{
	SE_BUF *b;
	UINT size;
	void *tmp;
	// 引数チェック
	if (bio == NULL)
	{
		return NULL;
	}

	BIO_seek(bio, 0);
	size = bio->num_write;
	tmp = SeZeroMalloc(size);
	BIO_read(bio, tmp, size);

	b = SeNewBuf();
	SeWriteBuf(b, tmp, size);
	SeFree(tmp);

	SeSeekBuf(b, 0, 0);

	return b;
}

// BUF を BIO に変換
BIO *SeBufToBio(SE_BUF *b)
{
	BIO *bio;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	bio = BIO_new(BIO_s_mem());
	if (bio == NULL)
	{
		return NULL;
	}
	BIO_write(bio, b->Buf, b->Size);
	BIO_seek(bio, 0);

	return bio;
}

// SHA-1 ハッシュ
void SeSha1(void *dst, void *src, UINT size)
{
	// 引数チェック
	if (dst == NULL || src == NULL)
	{
		return;
	}

	SHA1(src, size, dst);
}

// MD5 ハッシュ
void SeMd5(void *dst, void *src, UINT size)
{
	// 引数チェック
	if (dst == NULL || src == NULL)
	{
		return;
	}

	MD5(src, size, dst);
}

// 3DES 暗号化
void SeDes3Encrypt(void *dest, void *src, UINT size, SE_DES_KEY *key, void *ivec)
{
	UCHAR ivec_copy[SE_DES_IV_SIZE];
	// 引数チェック
	if (dest == NULL || src == NULL || size == 0 || key == NULL || ivec == NULL)
	{
		return;
	}

	SeCopy(ivec_copy, ivec, SE_DES_IV_SIZE);

	DES_ede3_cbc_encrypt(src, dest, size,
		key->k1->KeySchedule,
		key->k2->KeySchedule,
		key->k3->KeySchedule,
		(DES_cblock *)ivec_copy,
		1);
}

// 3DES 解読
void SeDes3Decrypt(void *dest, void *src, UINT size, SE_DES_KEY *key, void *ivec)
{
	UCHAR ivec_copy[SE_DES_IV_SIZE];
	// 引数チェック
	if (dest == NULL || src == NULL || size == 0 || key == NULL || ivec == NULL)
	{
		return;
	}

	SeCopy(ivec_copy, ivec, SE_DES_IV_SIZE);

	DES_ede3_cbc_encrypt(src, dest, size,
		key->k1->KeySchedule,
		key->k2->KeySchedule,
		key->k3->KeySchedule,
		(DES_cblock *)ivec_copy,
		0);
}

// ランダムな 3DES 鍵の生成
SE_DES_KEY *SeDes3RandKey()
{
	SE_DES_KEY *k = SeZeroMalloc(sizeof(SE_DES_KEY));

	k->k1 = SeDesRandKeyValue();
	k->k2 = SeDesRandKeyValue();
	k->k3 = SeDesRandKeyValue();

	return k;
}

// ランダムな DES 鍵の生成
SE_DES_KEY *SeDesRandKey()
{
	SE_DES_KEY *k = SeZeroMalloc(sizeof(SE_DES_KEY));

	k->k1 = SeDesRandKeyValue();
	k->k2 = SeDesNewKeyValue(k->k1->KeyValue);
	k->k3 = SeDesNewKeyValue(k->k1->KeyValue);

	return k;
}

// 3DES 鍵の解放
void SeDes3FreeKey(SE_DES_KEY *k)
{
	// 引数チェック
	if (k == NULL)
	{
		return;
	}

	SeDesFreeKeyValue(k->k1);
	SeDesFreeKeyValue(k->k2);
	SeDesFreeKeyValue(k->k3);

	SeFree(k);
}

// DES 鍵の解放
void SeDesFreeKey(SE_DES_KEY *k)
{
	SeDes3FreeKey(k);
}

// 3DES 鍵の作成
SE_DES_KEY *SeDes3NewKey(void *k1, void *k2, void *k3)
{
	SE_DES_KEY *k;
	// 引数チェック
	if (k1 == NULL || k2 == NULL || k3 == NULL)
	{
		return NULL;
	}

	k = SeZeroMalloc(sizeof(SE_DES_KEY));

	k->k1 = SeDesNewKeyValue(k1);
	k->k2 = SeDesNewKeyValue(k2);
	k->k3 = SeDesNewKeyValue(k3);

	return k;
}

// DES 鍵の作成
SE_DES_KEY *SeDesNewKey(void *k1)
{
	return SeDes3NewKey(k1, k1, k1);
}

// 新しい DES 鍵要素の作成
SE_DES_KEY_VALUE *SeDesNewKeyValue(void *value)
{
	SE_DES_KEY_VALUE *v;
	// 引数チェック
	if (value == NULL)
	{
		return NULL;
	}

	v = SeZeroMalloc(sizeof(SE_DES_KEY_VALUE));

	SeCopy(v->KeyValue, value, SE_DES_KEY_SIZE);

	v->KeySchedule = SeZeroMalloc(sizeof(DES_key_schedule));

	DES_set_key_unchecked(value, v->KeySchedule);

	return v;
}

// 新しい DES 鍵要素のランダム生成
SE_DES_KEY_VALUE *SeDesRandKeyValue()
{
	UCHAR key_value[SE_DES_KEY_SIZE];

	DES_random_key((DES_cblock *)key_value);

	return SeDesNewKeyValue(key_value);
}

// DES 鍵要素の解放
void SeDesFreeKeyValue(SE_DES_KEY_VALUE *v)
{
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	SeFree(v->KeySchedule);
	SeFree(v);
}

// 乱数発生
void SeRand(void *buf, UINT size)
{
	// 引数チェック
	if (buf == NULL || size == 0)
	{
		return;
	}

	RAND_bytes(buf, size);
}
UINT64 SeRand64()
{
	UINT64 ret;

	SeRand(&ret, sizeof(ret));

	return ret;
}
UINT SeRand32()
{
	UINT ret;

	SeRand(&ret, sizeof(ret));

	return ret;
}
USHORT SeRand16()
{
	USHORT ret;

	SeRand(&ret, sizeof(ret));

	return ret;
}
UCHAR SeRand8()
{
	UCHAR ret;

	SeRand(&ret, sizeof(ret));

	return ret;
}
bool SeRand1()
{
	return ((SeRand8() & 1) == 0 ? false : true);
}

// 初期化
void SeInitCrypto(bool init_openssl)
{
	if (init_openssl)
	{
		char tmp[16];

		OpenSSL_add_all_ciphers();
		SSLeay_add_all_digests();
		ERR_load_crypto_strings();
		RAND_poll();

		SeRand(tmp, sizeof(tmp));

		rt->OpenSslInited = true;
	}
}

// 解放
void SeFreeCrypto()
{
	if (rt->OpenSslInited)
	{
	}

	rt->OpenSslInited = false;
}

