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

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SALTLEN 8		/* salt size */
#define ITER 1024		/* iteration */
#define KEYLEN 32		/* 256 bit */
#define IVLEN 16		/* AES block size */

void
endfunc (void)
{
	system ("stty echo");
}

int
readpw (char *msg, char *pw, int len)
{
	char *r;

	fprintf (stderr, "%s", msg);
	system ("stty -echo");
	r = fgets (pw, len, stdin);
	endfunc ();
	fprintf (stderr, "\n");
	if (r == NULL) {
		perror ("fgets");
		return 0;
	}
	len = strlen (pw);
	if (len > 0 && pw[len - 1] =='\n')
		pw[--len] = '\0';
	return len;
}

int
main (int argc, char **argv)
{
	FILE *fp;
	long size;
	AES_KEY key;
	int pw1len, pw2len;
	static char pw1[4096], pw2[4096];
	unsigned char *readbuf, *writebuf;
	unsigned char salt[SALTLEN], keyiv[KEYLEN + IVLEN];

	/* check args */
	if (argc <= 1) {
		fprintf (stderr, "makedata config-data-file > output\n");
		exit (EXIT_FAILURE);
	}
	/* read the file */
	fp = fopen (argv[1], "r");
	if (fp == NULL) {
		perror ("fopen");
		exit (EXIT_FAILURE);
	}
	if (fseek (fp, 0L, SEEK_END) < 0) {
		perror ("fseek");
		fclose (fp);
		exit (EXIT_FAILURE);
	}
	size = ftell (fp);
	if (size < 0) {
		perror ("ftell");
		fclose (fp);
		exit (EXIT_FAILURE);
	}
	rewind (fp);
	readbuf = malloc ((size + 16) * 2);
	if (readbuf == NULL) {
		perror ("malloc");
		fclose (fp);
		exit (EXIT_FAILURE);
	}
	writebuf = readbuf + size + 16;
	if (fread (readbuf, size, 1, fp) != 1) {
		perror ("fread");
		fclose (fp);
		exit (EXIT_FAILURE);
	}
	fclose (fp);
	memset (readbuf + size, 0, 16);
	size = (size + 15) / 16 * 16;
	/* read a password */
	atexit (endfunc);
	switch (0) do {
		fprintf (stderr, "Passwords do not match\n");
	case 0:
		pw1len = readpw ("Enter password: ", pw1, sizeof pw1);
		pw2len = readpw ("Retype password: ", pw2, sizeof pw2);
	} while (pw1len != pw2len || memcmp (pw1, pw2, pw1len));
	if (pw1len == 0)
		fprintf (stderr, "No password supplied\n");
	/* make a salt */
	RAND_pseudo_bytes (salt, SALTLEN);
	/* make a key */
	PKCS5_PBKDF2_HMAC_SHA1 (pw1, pw1len, salt, SALTLEN, ITER,
				KEYLEN + IVLEN, keyiv);
	/* do encryption */
	AES_set_encrypt_key (keyiv, KEYLEN * 8, &key);
	AES_cbc_encrypt (readbuf, writebuf, size, &key, keyiv + KEYLEN, 1);
	/* write the salt and encrypted data */
	if (fwrite (salt, SALTLEN, 1, stdout) != 1) {
		perror ("fwrite");
		free (readbuf);
		exit (EXIT_FAILURE);
	}
	if (fwrite (writebuf, (size + 15) / 16 * 16, 1, stdout) != 1) {
		perror ("fwrite");
		free (readbuf);
		exit (EXIT_FAILURE);
	}
	free (readbuf);
	exit (EXIT_SUCCESS);
}
