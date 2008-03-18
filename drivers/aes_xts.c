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

//#include <stdio.h>
//#include <assert.h>
//#include <vl.h>
#include <core.h>
#include "aes.h" //1st-proto

#define GF_128_FDBK     0x87
#define AES_BLK_BYTES   16
#define uint8_t		u8
#define uint64_t	u64

/*
* XTS-AES-128 Encryption Function
* @author University of Tsukuba
* @param k2 key used for tweaking
* @param k1 key used for "ECB" encryption
* @param S sector number (64 bits)
* @param N sector size, in bytes
* @param pt plaintext sector output data
* @param ciphertext sector output data
* @since 2007.09
* @version 1.0
*/
void AES_xts_encrypt(const AES_KEY *k2, const AES_KEY *k1, uint64_t S, uint N, const uint8_t *pt, uint8_t *ct)
{
	uint	i, j;
	uint8_t	I[AES_BLK_BYTES];			// tweak value (1)
	uint8_t	T[AES_BLK_BYTES];			// tweak value (2)
	uint8_t	pp[AES_BLK_BYTES];
	uint8_t	cc[AES_BLK_BYTES];
	uint8_t	Cin, Cout;					// "carry" bits for LFSR shifting

	//assert(N % AES_BLK_BYTES == 0);		// data unit is multiple of 16 bytes

	for (j=0;j<AES_BLK_BYTES;j++)	// convert sector number to tweak plaintext
	{
		I[j] = (uint8_t) (S & 0xFF);
		S = S >> 8;						// shift S right
	}

	AES_encrypt(I, T, k2);			// encrypt the tweak

   // now encrypt the data unit, AES_BLK_BYTES at a time
	for (i=0;i+AES_BLK_BYTES<=N;i+=AES_BLK_BYTES)		
	{
		// merge the tweak into the input block
		for (j=0;j<AES_BLK_BYTES;j++)
		{
			pp[j] = pt[i+j] ^ T[j];	// bit-wise exclusive-OR
		}

		// encrypt one block
		AES_encrypt(pp, cc, k1);

		// merge the tweak into the output block
		for (j=0;j<AES_BLK_BYTES;j++)
		{
			ct[i+j] = cc[j] ^ T[j];
		}
		//hexdump(stdout, "ct", ct+i, AES_BLK_BYTES);

		// multiply T by alpha
		Cin = 0;
		for (j=0;j<AES_BLK_BYTES;j++)
		{
			Cout = (T[j] >> 7) & 1;
			T[j] = ((T[j] << 1) + Cin) & 0xFF;
			Cin = Cout;
		}
		if (Cout)
		{
			T[0] ^= GF_128_FDBK;
		}

	}
}


/*
* XTS-AES-128 Decryption Function
* @author University of Tsukuba
* @param k2 key used for tweaking
* @param k1 key used for "ECB" encryption
* @param S sector number (64 bits)
* @param N sector size, in bytes
* @param pt plaintext sector output data
* @param ct ciphertext sector output data
* @since 2007.09
* @version 1.0
*/
void AES_xts_decrypt(const AES_KEY *k2, const AES_KEY *k1, uint64_t S, uint N, const uint8_t *ct, uint8_t *pt)
{
	uint	i, j;
	uint8_t	I[AES_BLK_BYTES];			// tweak value (1)
	uint8_t	T[AES_BLK_BYTES];			// tweak value (2)
	uint8_t	pp[AES_BLK_BYTES];
	uint8_t	cc[AES_BLK_BYTES];
	uint8_t	Cin, Cout;					// "carry" bits for LFSR shifting

	//assert(N % AES_BLK_BYTES == 0);		// data unit is multiple of 16 bytes

	for (j=0;j<AES_BLK_BYTES;j++)	// convert sector number to tweak plaintext
	{
		I[j] = (uint8_t) (S & 0xFF);
		S = S >> 8;						// shift S right
	}

	AES_encrypt(I, T, k2);			// encrypt the tweak

   // now encrypt the data unit, AES_BLK_BYTES at a time
	for (i=0;i+AES_BLK_BYTES<=N;i+=AES_BLK_BYTES)		
	{
		// merge the tweak into the input block
		for (j=0;j<AES_BLK_BYTES;j++)
		{
			cc[j] = ct[i+j] ^ T[j];	// bit-wise exclusive-OR
		}

		// encrypt one block
		AES_decrypt(cc, pp, k1);

		// merge the tweak into the output block
		for (j=0;j<AES_BLK_BYTES;j++)
		{
			pt[i+j] = pp[j] ^ T[j];
		}
		//hexdump(stdout, "pt", pt+i, AES_BLK_BYTES);

		// multiply T by alpha
		Cin = 0;
		for (j=0;j<AES_BLK_BYTES;j++)
		{
			Cout = (T[j] >> 7) & 1;
			T[j] = ((T[j] << 1) + Cin) & 0xFF;
			Cin = Cout;
		}
		if (Cout)
		{
			T[0] ^= GF_128_FDBK;
		}

	}
}

